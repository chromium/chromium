// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/dbus/xdg/systemd.h"

#include <unistd.h>

#include <optional>
#include <string>
#include <vector>

#include "base/environment.h"
#include "base/files/file_path.h"
#include "base/files/file_util.h"
#include "base/files/scoped_temp_dir.h"
#include "base/functional/callback_helpers.h"
#include "base/memory/scoped_refptr.h"
#include "base/nix/xdg_util.h"
#include "base/strings/string_number_conversions.h"
#include "base/strings/string_split.h"
#include "base/strings/string_util.h"
#include "base/test/bind.h"
#include "base/test/nix/scoped_chrome_version_extra_override.h"
#include "base/test/task_environment.h"
#include "base/test/test_future.h"
#include "base/version_info/channel.h"
#include "base/version_info/nix/version_extra_utils.h"
#include "components/dbus/xdg/request.h"
#include "components/dbus/xdg/systemd_constants.h"
#include "dbus/message.h"
#include "dbus/mock_bus.h"
#include "dbus/mock_object_proxy.h"
#include "dbus/object_path.h"
#include "dbus/object_proxy.h"
#include "dbus/property.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

using ::testing::_;
using ::testing::ElementsAre;
using ::testing::Return;

namespace dbus_xdg {

namespace {

using internal::SystemdUnitStatus;

constexpr char kFakeUnitPath[] = "/fake/unit/path";

std::unique_ptr<dbus::Response> CreateActiveStateGetAllResponse(
    const std::string& state) {
  auto response = dbus::Response::CreateEmpty();
  dbus::MessageWriter writer(response.get());
  dbus::MessageWriter array_writer(nullptr);
  dbus::MessageWriter dict_entry_writer(nullptr);
  writer.OpenArray("{sv}", &array_writer);
  array_writer.OpenDictEntry(&dict_entry_writer);
  dict_entry_writer.AppendString(kSystemdActiveStateProp);
  dict_entry_writer.AppendVariantOfString(state);
  array_writer.CloseContainer(&dict_entry_writer);
  writer.CloseContainer(&array_writer);
  return response;
}

constexpr char kOldCgroup[] = "0::/user.slice/app.slice/vte-spawn-1.scope\n";
constexpr char kNewCgroup[] = "0::/user.slice/app.slice/app-chromium-1.scope\n";

constexpr pid_t kInitPid = 1;
constexpr pid_t kZygotePid = 100001;
constexpr pid_t kRendererPid = 100002;
constexpr pid_t kGpuPid = 100003;

// Writes a fake /proc/<pid> entry with the given parent and cgroup.
void WriteFakeProcess(const base::FilePath& proc_dir,
                      pid_t pid,
                      pid_t ppid,
                      std::string_view cgroup) {
  base::FilePath dir = proc_dir.Append(base::NumberToString(pid));
  ASSERT_TRUE(base::CreateDirectory(dir));
  // The comm field may contain spaces and parentheses.
  ASSERT_TRUE(base::WriteFile(dir.Append("stat"),
                              base::NumberToString(pid) + " (a (b) c) S " +
                                  base::NumberToString(ppid) + " 1 1 0 -1\n"));
  ASSERT_TRUE(base::WriteFile(dir.Append("cmdline"), "chrome"));
  ASSERT_TRUE(base::WriteFile(dir.Append("cgroup"), cgroup));
}

class SetSystemdScopeUnitNameForXdgPortalTest : public ::testing::Test {
 public:
  void SetUp() override {
    ASSERT_TRUE(temp_dir_.CreateUniqueTempDir());
    // By default, the process has no descendants to migrate.
    WriteFakeProcess(proc_dir(), getpid(), kInitPid, kNewCgroup);
  }

 protected:
  const base::FilePath& proc_dir() const { return temp_dir_.GetPath(); }

  base::ScopedTempDir temp_dir_;
  base::test::TaskEnvironment task_environment_{
      base::test::TaskEnvironment::TimeSource::MOCK_TIME};
};

TEST_F(SetSystemdScopeUnitNameForXdgPortalTest, NotNecessaryInFlatpak) {
  scoped_refptr<dbus::MockBus> bus =
      base::MakeRefCounted<dbus::MockBus>(dbus::Bus::Options());

  base::nix::ScopedAppSandboxOverrideForTesting sandbox_override(
      base::nix::AppSandbox::kFlatpak);

  std::optional<SystemdUnitStatus> status;

  internal::SetSystemdScopeUnitNameForXdgPortal(
      bus.get(), proc_dir(),
      base::BindLambdaForTesting(
          [&](SystemdUnitStatus result) { status = result; }));

  EXPECT_EQ(status, SystemdUnitStatus::kUnitNotNecessary);
}

TEST_F(SetSystemdScopeUnitNameForXdgPortalTest, NotNecessaryInSnap) {
  scoped_refptr<dbus::MockBus> bus =
      base::MakeRefCounted<dbus::MockBus>(dbus::Bus::Options());

  base::nix::ScopedAppSandboxOverrideForTesting sandbox_override(
      base::nix::AppSandbox::kSnap);

  std::optional<SystemdUnitStatus> status;

  internal::SetSystemdScopeUnitNameForXdgPortal(
      bus.get(), proc_dir(),
      base::BindLambdaForTesting(
          [&](SystemdUnitStatus result) { status = result; }));

  EXPECT_EQ(status, SystemdUnitStatus::kUnitNotNecessary);
}

TEST_F(SetSystemdScopeUnitNameForXdgPortalTest, NoSystemdService) {
  scoped_refptr<dbus::MockBus> bus =
      base::MakeRefCounted<dbus::MockBus>(dbus::Bus::Options());

  auto mock_dbus_proxy = base::MakeRefCounted<dbus::MockObjectProxy>(
      bus.get(), DBUS_SERVICE_DBUS, dbus::ObjectPath(DBUS_PATH_DBUS));

  EXPECT_CALL(
      *bus, GetObjectProxy(DBUS_SERVICE_DBUS, dbus::ObjectPath(DBUS_PATH_DBUS)))
      .WillRepeatedly(Return(mock_dbus_proxy.get()));

  EXPECT_CALL(*mock_dbus_proxy, CallMethod(_, _, _))
      .WillOnce([](dbus::MethodCall* method_call, int timeout_ms,
                   dbus::ObjectProxy::ResponseCallback callback) {
        auto response = dbus::Response::CreateEmpty();
        dbus::MessageWriter writer(response.get());
        writer.AppendBool(false);
        std::move(callback).Run(response.get());
      });

  std::optional<SystemdUnitStatus> status;

  internal::SetSystemdScopeUnitNameForXdgPortal(
      bus.get(), proc_dir(),
      base::BindLambdaForTesting(
          [&](SystemdUnitStatus result) { status = result; }));

  EXPECT_EQ(status, SystemdUnitStatus::kNoSystemdService);
}

TEST_F(SetSystemdScopeUnitNameForXdgPortalTest, StartTransientUnitSuccess) {
  scoped_refptr<dbus::MockBus> bus =
      base::MakeRefCounted<dbus::MockBus>(dbus::Bus::Options());

  auto mock_dbus_proxy = base::MakeRefCounted<dbus::MockObjectProxy>(
      bus.get(), DBUS_SERVICE_DBUS, dbus::ObjectPath(DBUS_PATH_DBUS));

  EXPECT_CALL(
      *bus, GetObjectProxy(DBUS_SERVICE_DBUS, dbus::ObjectPath(DBUS_PATH_DBUS)))
      .WillRepeatedly(Return(mock_dbus_proxy.get()));

  EXPECT_CALL(*mock_dbus_proxy, CallMethod(_, _, _))
      .WillOnce([](dbus::MethodCall* method_call, int timeout_ms,
                   dbus::ObjectProxy::ResponseCallback callback) {
        auto response = dbus::Response::CreateEmpty();
        dbus::MessageWriter writer(response.get());
        writer.AppendBool(true);
        std::move(callback).Run(response.get());
      });

  auto mock_systemd_proxy = base::MakeRefCounted<dbus::MockObjectProxy>(
      bus.get(), kServiceNameSystemd, dbus::ObjectPath(kObjectPathSystemd));

  EXPECT_CALL(*bus, GetObjectProxy(kServiceNameSystemd,
                                   dbus::ObjectPath(kObjectPathSystemd)))
      .Times(2)
      .WillRepeatedly(Return(mock_systemd_proxy.get()));

  auto mock_dbus_unit_proxy = base::MakeRefCounted<dbus::MockObjectProxy>(
      bus.get(), kServiceNameSystemd, dbus::ObjectPath(kFakeUnitPath));
  EXPECT_CALL(*bus, GetObjectProxy(kServiceNameSystemd,
                                   dbus::ObjectPath(kFakeUnitPath)))
      .WillOnce(Return(mock_dbus_unit_proxy.get()));

  EXPECT_CALL(*mock_systemd_proxy, CallMethod(_, _, _))
      .WillOnce([](dbus::MethodCall* method_call, int timeout_ms,
                   dbus::ObjectProxy::ResponseCallback callback) {
        // Expect kMethodStartTransientUnit first.
        EXPECT_EQ(method_call->GetInterface(), kInterfaceSystemdManager);
        EXPECT_EQ(method_call->GetMember(), kMethodStartTransientUnit);

        // Simulate a successful response
        auto response = dbus::Response::CreateEmpty();
        std::move(callback).Run(response.get());
      })
      .WillOnce([obj_path = kFakeUnitPath](
                    dbus::MethodCall* method_call, int timeout_ms,
                    dbus::ObjectProxy::ResponseCallback callback) {
        // Then expect kMethodGetUnit. A valid path must be provided.
        EXPECT_EQ(method_call->GetInterface(), kInterfaceSystemdManager);
        EXPECT_EQ(method_call->GetMember(), kMethodGetUnit);

        // Simulate a successful response and provide a fake path to the object.
        auto response = dbus::Response::CreateEmpty();
        dbus::MessageWriter writer(response.get());
        writer.AppendObjectPath(dbus::ObjectPath(obj_path));
        std::move(callback).Run(response.get());
      });

  EXPECT_CALL(*mock_dbus_unit_proxy, CallMethod(_, _, _))
      .WillOnce([](dbus::MethodCall* method_call, int timeout_ms,
                   dbus::ObjectProxy::ResponseCallback callback) {
        EXPECT_EQ(method_call->GetInterface(), dbus::kPropertiesInterface);
        EXPECT_EQ(method_call->GetMember(), dbus::kPropertiesGetAll);
        // Simulate a successful response with "active" state.
        auto response = CreateActiveStateGetAllResponse(kSystemdStateActive);
        std::move(callback).Run(response.get());
      });

  std::optional<SystemdUnitStatus> status;

  internal::SetSystemdScopeUnitNameForXdgPortal(
      bus.get(), proc_dir(),
      base::BindLambdaForTesting(
          [&](SystemdUnitStatus result) { status = result; }));

  EXPECT_EQ(status, SystemdUnitStatus::kUnitStarted);
}

TEST_F(SetSystemdScopeUnitNameForXdgPortalTest, StartTransientUnitFailure) {
  scoped_refptr<dbus::MockBus> bus =
      base::MakeRefCounted<dbus::MockBus>(dbus::Bus::Options());

  auto mock_dbus_proxy = base::MakeRefCounted<dbus::MockObjectProxy>(
      bus.get(), DBUS_SERVICE_DBUS, dbus::ObjectPath(DBUS_PATH_DBUS));

  EXPECT_CALL(
      *bus, GetObjectProxy(DBUS_SERVICE_DBUS, dbus::ObjectPath(DBUS_PATH_DBUS)))
      .WillRepeatedly(Return(mock_dbus_proxy.get()));

  EXPECT_CALL(*mock_dbus_proxy, CallMethod(_, _, _))
      .WillOnce([](dbus::MethodCall* method_call, int timeout_ms,
                   dbus::ObjectProxy::ResponseCallback callback) {
        auto response = dbus::Response::CreateEmpty();
        dbus::MessageWriter writer(response.get());
        writer.AppendBool(true);
        std::move(callback).Run(response.get());
      });

  auto mock_systemd_proxy = base::MakeRefCounted<dbus::MockObjectProxy>(
      bus.get(), kServiceNameSystemd, dbus::ObjectPath(kObjectPathSystemd));

  EXPECT_CALL(*bus, GetObjectProxy(kServiceNameSystemd,
                                   dbus::ObjectPath(kObjectPathSystemd)))
      .WillOnce(Return(mock_systemd_proxy.get()));

  EXPECT_CALL(*mock_systemd_proxy, CallMethod(_, _, _))
      .WillOnce([](dbus::MethodCall* method_call, int timeout_ms,
                   dbus::ObjectProxy::ResponseCallback callback) {
        // Simulate a failure by invoking the callback with nullptr
        std::move(callback).Run(nullptr);
      });

  std::optional<SystemdUnitStatus> status;

  internal::SetSystemdScopeUnitNameForXdgPortal(
      bus.get(), proc_dir(),
      base::BindLambdaForTesting(
          [&](SystemdUnitStatus result) { status = result; }));

  EXPECT_EQ(status, SystemdUnitStatus::kFailedToStart);
}

TEST_F(SetSystemdScopeUnitNameForXdgPortalTest,
       StartTransientUnitInvalidUnitPath) {
  scoped_refptr<dbus::MockBus> bus =
      base::MakeRefCounted<dbus::MockBus>(dbus::Bus::Options());

  auto mock_dbus_proxy = base::MakeRefCounted<dbus::MockObjectProxy>(
      bus.get(), DBUS_SERVICE_DBUS, dbus::ObjectPath(DBUS_PATH_DBUS));

  EXPECT_CALL(
      *bus, GetObjectProxy(DBUS_SERVICE_DBUS, dbus::ObjectPath(DBUS_PATH_DBUS)))
      .WillRepeatedly(Return(mock_dbus_proxy.get()));

  EXPECT_CALL(*mock_dbus_proxy, CallMethod(_, _, _))
      .WillOnce([](dbus::MethodCall* method_call, int timeout_ms,
                   dbus::ObjectProxy::ResponseCallback callback) {
        auto response = dbus::Response::CreateEmpty();
        dbus::MessageWriter writer(response.get());
        writer.AppendBool(true);
        std::move(callback).Run(response.get());
      });

  auto mock_systemd_proxy = base::MakeRefCounted<dbus::MockObjectProxy>(
      bus.get(), kServiceNameSystemd, dbus::ObjectPath(kObjectPathSystemd));

  EXPECT_CALL(*bus, GetObjectProxy(kServiceNameSystemd,
                                   dbus::ObjectPath(kObjectPathSystemd)))
      .Times(2)
      .WillRepeatedly(Return(mock_systemd_proxy.get()));

  EXPECT_CALL(*mock_systemd_proxy, CallMethod(_, _, _))
      .WillOnce([](dbus::MethodCall* method_call, int timeout_ms,
                   dbus::ObjectProxy::ResponseCallback callback) {
        EXPECT_EQ(method_call->GetInterface(), kInterfaceSystemdManager);
        EXPECT_EQ(method_call->GetMember(), kMethodStartTransientUnit);

        // Simulate a successful response
        auto response = dbus::Response::CreateEmpty();
        std::move(callback).Run(response.get());
      })
      .WillOnce([](dbus::MethodCall* method_call, int timeout_ms,
                   dbus::ObjectProxy::ResponseCallback callback) {
        EXPECT_EQ(method_call->GetInterface(), kInterfaceSystemdManager);
        EXPECT_EQ(method_call->GetMember(), kMethodGetUnit);

        // Simulate a failure response.
        std::move(callback).Run(nullptr);
      });

  std::optional<SystemdUnitStatus> status;

  internal::SetSystemdScopeUnitNameForXdgPortal(
      bus.get(), proc_dir(),
      base::BindLambdaForTesting(
          [&](SystemdUnitStatus result) { status = result; }));

  EXPECT_EQ(status, SystemdUnitStatus::kFailedToStart);
}

TEST_F(SetSystemdScopeUnitNameForXdgPortalTest,
       StartTransientUnitFailToActivate) {
  scoped_refptr<dbus::MockBus> bus =
      base::MakeRefCounted<dbus::MockBus>(dbus::Bus::Options());

  auto mock_dbus_proxy = base::MakeRefCounted<dbus::MockObjectProxy>(
      bus.get(), DBUS_SERVICE_DBUS, dbus::ObjectPath(DBUS_PATH_DBUS));

  EXPECT_CALL(
      *bus, GetObjectProxy(DBUS_SERVICE_DBUS, dbus::ObjectPath(DBUS_PATH_DBUS)))
      .WillRepeatedly(Return(mock_dbus_proxy.get()));

  EXPECT_CALL(*mock_dbus_proxy, CallMethod(_, _, _))
      .WillOnce([](dbus::MethodCall* method_call, int timeout_ms,
                   dbus::ObjectProxy::ResponseCallback callback) {
        auto response = dbus::Response::CreateEmpty();
        dbus::MessageWriter writer(response.get());
        writer.AppendBool(true);
        std::move(callback).Run(response.get());
      });

  auto mock_systemd_proxy = base::MakeRefCounted<dbus::MockObjectProxy>(
      bus.get(), kServiceNameSystemd, dbus::ObjectPath(kObjectPathSystemd));

  EXPECT_CALL(*bus, GetObjectProxy(kServiceNameSystemd,
                                   dbus::ObjectPath(kObjectPathSystemd)))
      .Times(2)
      .WillRepeatedly(Return(mock_systemd_proxy.get()));

  auto mock_dbus_unit_proxy = base::MakeRefCounted<dbus::MockObjectProxy>(
      bus.get(), kServiceNameSystemd, dbus::ObjectPath(kFakeUnitPath));
  EXPECT_CALL(*bus, GetObjectProxy(kServiceNameSystemd,
                                   dbus::ObjectPath(kFakeUnitPath)))
      .WillOnce(Return(mock_dbus_unit_proxy.get()));

  EXPECT_CALL(*mock_systemd_proxy, CallMethod(_, _, _))
      .WillOnce([](dbus::MethodCall* method_call, int timeout_ms,
                   dbus::ObjectProxy::ResponseCallback callback) {
        EXPECT_EQ(method_call->GetInterface(), kInterfaceSystemdManager);
        EXPECT_EQ(method_call->GetMember(), kMethodStartTransientUnit);

        // Simulate a successful response
        auto response = dbus::Response::CreateEmpty();
        std::move(callback).Run(response.get());
      })
      .WillOnce([obj_path = kFakeUnitPath](
                    dbus::MethodCall* method_call, int timeout_ms,
                    dbus::ObjectProxy::ResponseCallback callback) {
        EXPECT_EQ(method_call->GetInterface(), kInterfaceSystemdManager);
        EXPECT_EQ(method_call->GetMember(), kMethodGetUnit);

        // Simulate a successful response
        auto response = dbus::Response::CreateEmpty();
        dbus::MessageWriter writer(response.get());
        writer.AppendObjectPath(dbus::ObjectPath(obj_path));
        std::move(callback).Run(response.get());
      });

  EXPECT_CALL(*mock_dbus_unit_proxy, CallMethod(_, _, _))
      .WillOnce([](dbus::MethodCall* method_call, int timeout_ms,
                   dbus::ObjectProxy::ResponseCallback callback) {
        // Then expect kMethodGetUnit. A valid path must be provided.
        EXPECT_EQ(method_call->GetInterface(), dbus::kPropertiesInterface);
        EXPECT_EQ(method_call->GetMember(), dbus::kPropertiesGetAll);

        // Simulate a successful response, but with inactive state.
        auto response = CreateActiveStateGetAllResponse(kSystemdStateInactive);
        std::move(callback).Run(response.get());
      });

  std::optional<SystemdUnitStatus> status;

  internal::SetSystemdScopeUnitNameForXdgPortal(
      bus.get(), proc_dir(),
      base::BindLambdaForTesting(
          [&](SystemdUnitStatus result) { status = result; }));

  EXPECT_EQ(status, SystemdUnitStatus::kFailedToStart);
}

TEST_F(SetSystemdScopeUnitNameForXdgPortalTest, UnitNameConstruction) {
  scoped_refptr<dbus::MockBus> bus =
      base::MakeRefCounted<dbus::MockBus>(dbus::Bus::Options());

  auto mock_dbus_proxy = base::MakeRefCounted<dbus::MockObjectProxy>(
      bus.get(), DBUS_SERVICE_DBUS, dbus::ObjectPath(DBUS_PATH_DBUS));

  EXPECT_CALL(
      *bus, GetObjectProxy(DBUS_SERVICE_DBUS, dbus::ObjectPath(DBUS_PATH_DBUS)))
      .WillRepeatedly(Return(mock_dbus_proxy.get()));

  EXPECT_CALL(*mock_dbus_proxy, CallMethod(_, _, _))
      .WillOnce([](dbus::MethodCall* method_call, int timeout_ms,
                   dbus::ObjectProxy::ResponseCallback callback) {
        auto response = dbus::Response::CreateEmpty();
        dbus::MessageWriter writer(response.get());
        writer.AppendBool(true);
        std::move(callback).Run(response.get());
      });

  base::test::ScopedChromeVersionExtraOverride env_override(
      version_info::Channel::BETA);

  auto mock_systemd_proxy = base::MakeRefCounted<dbus::MockObjectProxy>(
      bus.get(), kServiceNameSystemd, dbus::ObjectPath(kObjectPathSystemd));

  constexpr std::string_view kAppPrefix = "app-";
  constexpr std::string_view kScopeSuffix = ".scope";

  EXPECT_CALL(*bus, GetObjectProxy(kServiceNameSystemd,
                                   dbus::ObjectPath(kObjectPathSystemd)))
      .Times(2)
      .WillRepeatedly(Return(mock_systemd_proxy.get()));

  auto mock_dbus_unit_proxy = base::MakeRefCounted<dbus::MockObjectProxy>(
      bus.get(), kServiceNameSystemd, dbus::ObjectPath(kFakeUnitPath));
  EXPECT_CALL(*bus, GetObjectProxy(kServiceNameSystemd,
                                   dbus::ObjectPath(kFakeUnitPath)))
      .WillOnce(Return(mock_dbus_unit_proxy.get()));

  EXPECT_CALL(*mock_systemd_proxy, CallMethod(_, _, _))
      .WillOnce([&](dbus::MethodCall* method_call, int timeout_ms,
                    dbus::ObjectProxy::ResponseCallback callback) {
        dbus::MessageReader reader(method_call);
        std::string unit_name;
        EXPECT_TRUE(reader.PopString(&unit_name));

        // Check that the unit_name matches the expected format
        // Format: app-<ApplicationID>-<RANDOM>.scope
        EXPECT_TRUE(base::StartsWith(unit_name, kAppPrefix));
        EXPECT_TRUE(base::EndsWith(unit_name, kScopeSuffix));

        // Remove "app-" prefix and ".scope" suffix
        std::string name_body = unit_name.substr(
            kAppPrefix.size(),
            unit_name.size() - kAppPrefix.size() - kScopeSuffix.size());

        auto id_random = base::SplitString(
            name_body, "-", base::KEEP_WHITESPACE, base::SPLIT_WANT_ALL);
        ASSERT_EQ(id_random.size(), 2U);

        std::string application_id = id_random[0];
        EXPECT_FALSE(application_id.empty());
        EXPECT_TRUE(
            std::all_of(application_id.begin(), application_id.end(),
                        [](char c) { return std::isalnum(c) || c == '.'; }));

        std::string random_part = id_random[1];
        EXPECT_FALSE(random_part.empty());
        EXPECT_TRUE(std::all_of(random_part.begin(), random_part.end(),
                                [](char c) { return std::isalnum(c); }));

        auto response = dbus::Response::CreateEmpty();
        std::move(callback).Run(response.get());
      })
      .WillOnce([obj_path = kFakeUnitPath](
                    dbus::MethodCall* method_call, int timeout_ms,
                    dbus::ObjectProxy::ResponseCallback callback) {
        EXPECT_EQ(method_call->GetInterface(), kInterfaceSystemdManager);
        EXPECT_EQ(method_call->GetMember(), kMethodGetUnit);

        // Simulate a successful response
        auto response = dbus::Response::CreateEmpty();
        dbus::MessageWriter writer(response.get());
        writer.AppendObjectPath(dbus::ObjectPath(obj_path));
        std::move(callback).Run(response.get());
      });

  EXPECT_CALL(*mock_dbus_unit_proxy, CallMethod(_, _, _))
      .WillOnce([](dbus::MethodCall* method_call, int timeout_ms,
                   dbus::ObjectProxy::ResponseCallback callback) {
        // Then expect kMethodGetUnit. A valid path must be provided.
        EXPECT_EQ(method_call->GetInterface(), dbus::kPropertiesInterface);
        EXPECT_EQ(method_call->GetMember(), dbus::kPropertiesGetAll);

        // Simulate a successful response
        auto response = CreateActiveStateGetAllResponse(kSystemdStateActive);
        std::move(callback).Run(response.get());
      });

  std::optional<SystemdUnitStatus> status;

  internal::SetSystemdScopeUnitNameForXdgPortal(
      bus.get(), proc_dir(),
      base::BindLambdaForTesting(
          [&](SystemdUnitStatus result) { status = result; }));

  EXPECT_EQ(status, SystemdUnitStatus::kUnitStarted);
}

// Sets up `bus` so that NameHasOwner, StartTransientUnit, GetUnit and GetAll
// succeed, and forwards AttachProcessesToUnit calls to `on_attach`, which
// returns whether the call succeeds.
void SetUpSuccessfulUnitStart(
    dbus::MockBus* bus,
    scoped_refptr<dbus::MockObjectProxy>& dbus_proxy,
    scoped_refptr<dbus::MockObjectProxy>& systemd_proxy,
    scoped_refptr<dbus::MockObjectProxy>& unit_proxy,
    base::RepeatingCallback<bool(const std::string&, pid_t)> on_attach) {
  dbus_proxy = base::MakeRefCounted<dbus::MockObjectProxy>(
      bus, DBUS_SERVICE_DBUS, dbus::ObjectPath(DBUS_PATH_DBUS));
  EXPECT_CALL(
      *bus, GetObjectProxy(DBUS_SERVICE_DBUS, dbus::ObjectPath(DBUS_PATH_DBUS)))
      .WillRepeatedly(Return(dbus_proxy.get()));
  EXPECT_CALL(*dbus_proxy, CallMethod(_, _, _))
      .WillOnce([](dbus::MethodCall* method_call, int timeout_ms,
                   dbus::ObjectProxy::ResponseCallback callback) {
        auto response = dbus::Response::CreateEmpty();
        dbus::MessageWriter writer(response.get());
        writer.AppendBool(true);
        std::move(callback).Run(response.get());
      });

  systemd_proxy = base::MakeRefCounted<dbus::MockObjectProxy>(
      bus, kServiceNameSystemd, dbus::ObjectPath(kObjectPathSystemd));
  EXPECT_CALL(*bus, GetObjectProxy(kServiceNameSystemd,
                                   dbus::ObjectPath(kObjectPathSystemd)))
      .WillRepeatedly(Return(systemd_proxy.get()));
  EXPECT_CALL(*systemd_proxy, CallMethod(_, _, _))
      .WillRepeatedly([on_attach](
                          dbus::MethodCall* method_call, int timeout_ms,
                          dbus::ObjectProxy::ResponseCallback callback) {
        EXPECT_EQ(method_call->GetInterface(), kInterfaceSystemdManager);
        const std::string member = method_call->GetMember();
        dbus::MessageReader reader(method_call);
        if (member == kMethodStartTransientUnit) {
          std::string unit_name;
          std::string mode;
          EXPECT_TRUE(reader.PopString(&unit_name));
          EXPECT_TRUE(reader.PopString(&mode));
          // Properties: a(sv). Check that the unit is delegated.
          dbus::MessageReader array_reader(nullptr);
          EXPECT_TRUE(reader.PopArray(&array_reader));
          bool delegate = false;
          while (array_reader.HasMoreData()) {
            dbus::MessageReader struct_reader(nullptr);
            EXPECT_TRUE(array_reader.PopStruct(&struct_reader));
            std::string key;
            EXPECT_TRUE(struct_reader.PopString(&key));
            if (key == "Delegate") {
              EXPECT_TRUE(struct_reader.PopVariantOfBool(&delegate));
            }
          }
          EXPECT_TRUE(delegate);
          auto response = dbus::Response::CreateEmpty();
          std::move(callback).Run(response.get());
        } else if (member == kMethodGetUnit) {
          auto response = dbus::Response::CreateEmpty();
          dbus::MessageWriter writer(response.get());
          writer.AppendObjectPath(dbus::ObjectPath(kFakeUnitPath));
          std::move(callback).Run(response.get());
        } else if (member == kMethodAttachProcessesToUnit) {
          std::string unit_name;
          std::string subcgroup;
          base::span<const uint32_t> pids;
          EXPECT_TRUE(reader.PopString(&unit_name));
          EXPECT_TRUE(reader.PopString(&subcgroup));
          EXPECT_TRUE(reader.PopArrayOfUint32s(&pids));
          EXPECT_EQ(subcgroup, "");
          if (pids.size() != 1u) {
            ADD_FAILURE();
          } else if (on_attach.Run(unit_name, static_cast<pid_t>(pids[0]))) {
            auto response = dbus::Response::CreateEmpty();
            std::move(callback).Run(response.get());
            return;
          }
          std::move(callback).Run(nullptr);
        } else {
          ADD_FAILURE() << "Unexpected method " << member;
          std::move(callback).Run(nullptr);
        }
      });

  unit_proxy = base::MakeRefCounted<dbus::MockObjectProxy>(
      bus, kServiceNameSystemd, dbus::ObjectPath(kFakeUnitPath));
  EXPECT_CALL(*bus, GetObjectProxy(kServiceNameSystemd,
                                   dbus::ObjectPath(kFakeUnitPath)))
      .WillOnce(Return(unit_proxy.get()));
  EXPECT_CALL(*unit_proxy, CallMethod(_, _, _))
      .WillOnce([](dbus::MethodCall* method_call, int timeout_ms,
                   dbus::ObjectProxy::ResponseCallback callback) {
        auto response = CreateActiveStateGetAllResponse(kSystemdStateActive);
        std::move(callback).Run(response.get());
      });
}

// Descendants that were created before the scope (e.g. the zygotes, the GPU
// process and renderers) are moved into the new scope, parents first, so that
// they are not killed when the original scope stops (crbug.com/580525223).
TEST_F(SetSystemdScopeUnitNameForXdgPortalTest, MigratesDescendants) {
  const pid_t browser = getpid();
  // The browser process has already been moved into the new scope.
  WriteFakeProcess(proc_dir(), browser, kInitPid, kNewCgroup);
  // Zygote, and a renderer forked from it, in the original scope.
  WriteFakeProcess(proc_dir(), kZygotePid, browser, kOldCgroup);
  WriteFakeProcess(proc_dir(), kRendererPid, kZygotePid, kOldCgroup);
  // GPU process in the original scope.
  WriteFakeProcess(proc_dir(), kGpuPid, browser, kOldCgroup);
  // A child that is already in the new scope.
  WriteFakeProcess(proc_dir(), 100004, browser, kNewCgroup);
  // An unrelated process.
  WriteFakeProcess(proc_dir(), 100005, kInitPid, kOldCgroup);

  auto bus = base::MakeRefCounted<dbus::MockBus>(dbus::Bus::Options());
  scoped_refptr<dbus::MockObjectProxy> dbus_proxy;
  scoped_refptr<dbus::MockObjectProxy> systemd_proxy;
  scoped_refptr<dbus::MockObjectProxy> unit_proxy;
  std::vector<pid_t> attached;
  std::string attached_unit;
  SetUpSuccessfulUnitStart(
      bus.get(), dbus_proxy, systemd_proxy, unit_proxy,
      base::BindLambdaForTesting([&](const std::string& unit, pid_t pid) {
        attached_unit = unit;
        attached.push_back(pid);
        // Simulate the migration.
        EXPECT_TRUE(base::WriteFile(
            proc_dir().Append(base::NumberToString(pid)).Append("cgroup"),
            kNewCgroup));
        return true;
      }));

  base::test::TestFuture<SystemdUnitStatus> future;
  internal::SetSystemdScopeUnitNameForXdgPortal(bus.get(), proc_dir(),
                                                future.GetCallback());
  EXPECT_EQ(future.Get(), SystemdUnitStatus::kUnitStarted);

  task_environment_.FastForwardUntilNoTasksRemain();

  // Direct children (`kZygotePid`, `kGpuPid`) come before grandchildren
  // (`kRendererPid`).
  EXPECT_THAT(attached, ElementsAre(kZygotePid, kGpuPid, kRendererPid));
  EXPECT_TRUE(attached_unit.starts_with("app-"));
  EXPECT_TRUE(
      attached_unit.ends_with("-" + base::NumberToString(browser) + ".scope"));
}

// A process that exits during the migration does not stop the others from
// being migrated, and children forked in the meantime are picked up by a
// later pass.
TEST_F(SetSystemdScopeUnitNameForXdgPortalTest,
       MigrationToleratesExitedAndNewProcesses) {
  const pid_t browser = getpid();
  WriteFakeProcess(proc_dir(), browser, kInitPid, kNewCgroup);
  WriteFakeProcess(proc_dir(), kZygotePid, browser, kOldCgroup);
  WriteFakeProcess(proc_dir(), kGpuPid, browser, kOldCgroup);

  auto bus = base::MakeRefCounted<dbus::MockBus>(dbus::Bus::Options());
  scoped_refptr<dbus::MockObjectProxy> dbus_proxy;
  scoped_refptr<dbus::MockObjectProxy> systemd_proxy;
  scoped_refptr<dbus::MockObjectProxy> unit_proxy;
  std::vector<pid_t> attached;
  SetUpSuccessfulUnitStart(
      bus.get(), dbus_proxy, systemd_proxy, unit_proxy,
      base::BindLambdaForTesting([&](const std::string& unit, pid_t pid) {
        attached.push_back(pid);
        base::FilePath dir = proc_dir().Append(base::NumberToString(pid));
        if (pid == kGpuPid) {
          // The process exited (ESRCH).
          EXPECT_TRUE(base::DeletePathRecursively(dir));
          return false;
        }
        EXPECT_TRUE(base::WriteFile(dir.Append("cgroup"), kNewCgroup));
        if (pid == kZygotePid) {
          // It forked a new child before being migrated.
          WriteFakeProcess(proc_dir(), kRendererPid, kZygotePid, kOldCgroup);
        }
        return true;
      }));

  internal::SetSystemdScopeUnitNameForXdgPortal(bus.get(), proc_dir(),
                                                base::DoNothing());
  task_environment_.FastForwardUntilNoTasksRemain();

  EXPECT_THAT(attached, ElementsAre(kZygotePid, kGpuPid, kRendererPid));
}

// If AttachProcessesToUnit() does not work at all (e.g. systemd older than
// v238), the migration is not retried.
TEST_F(SetSystemdScopeUnitNameForXdgPortalTest,
       MigrationStopsWhenAttachUnsupported) {
  const pid_t browser = getpid();
  WriteFakeProcess(proc_dir(), browser, kInitPid, kNewCgroup);
  WriteFakeProcess(proc_dir(), kZygotePid, browser, kOldCgroup);
  WriteFakeProcess(proc_dir(), kGpuPid, browser, kOldCgroup);

  auto bus = base::MakeRefCounted<dbus::MockBus>(dbus::Bus::Options());
  scoped_refptr<dbus::MockObjectProxy> dbus_proxy;
  scoped_refptr<dbus::MockObjectProxy> systemd_proxy;
  scoped_refptr<dbus::MockObjectProxy> unit_proxy;
  int attach_calls = 0;
  SetUpSuccessfulUnitStart(
      bus.get(), dbus_proxy, systemd_proxy, unit_proxy,
      base::BindLambdaForTesting([&](const std::string& unit, pid_t pid) {
        ++attach_calls;
        return false;
      }));

  base::test::TestFuture<SystemdUnitStatus> future;
  internal::SetSystemdScopeUnitNameForXdgPortal(bus.get(), proc_dir(),
                                                future.GetCallback());
  EXPECT_EQ(future.Get(), SystemdUnitStatus::kUnitStarted);
  task_environment_.FastForwardUntilNoTasksRemain();

  EXPECT_EQ(attach_calls, 2);
}

TEST(SystemdGetDescendantsTest, GetDescendantsOutsideCgroup) {
  base::ScopedTempDir temp_dir;
  ASSERT_TRUE(temp_dir.CreateUniqueTempDir());
  const base::FilePath& proc = temp_dir.GetPath();
  WriteFakeProcess(proc, 10, kInitPid, kNewCgroup);
  WriteFakeProcess(proc, 11, 10, kOldCgroup);
  WriteFakeProcess(proc, 12, 11, kOldCgroup);
  WriteFakeProcess(proc, 13, 12, kNewCgroup);
  WriteFakeProcess(proc, 14, 13, kOldCgroup);
  WriteFakeProcess(proc, 20, kInitPid, kOldCgroup);
  // Non-PID entries are ignored.
  ASSERT_TRUE(base::CreateDirectory(proc.Append("self")));
  ASSERT_TRUE(base::WriteFile(proc.Append("uptime"), "1 1\n"));

  EXPECT_THAT(internal::GetDescendantsOutsideCgroup(proc, 10),
              ElementsAre(11, 12, 14));
  EXPECT_TRUE(internal::GetDescendantsOutsideCgroup(proc, 20).empty());
  // Missing root.
  EXPECT_TRUE(internal::GetDescendantsOutsideCgroup(proc, 30).empty());
}

TEST(SystemdEscapeTest, EscapeSystemdUnitName) {
  EXPECT_EQ(internal::EscapeSystemdUnitName("foo-bar"), "foo\\x2dbar");
  EXPECT_EQ(internal::EscapeSystemdUnitName("foo/bar"), "foo-bar");
  EXPECT_EQ(internal::EscapeSystemdUnitName(".foo"), "\\x2efoo");
  EXPECT_EQ(internal::EscapeSystemdUnitName("1foo"), "1foo");
  EXPECT_EQ(internal::EscapeSystemdUnitName("foo_bar:baz.qux"),
            "foo_bar:baz.qux");
  EXPECT_EQ(internal::EscapeSystemdUnitName(""), "");
}

}  // namespace

}  // namespace dbus_xdg
