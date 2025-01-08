// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/dbus/xdg/systemd.h"

#include <optional>

#include "base/environment.h"
#include "base/memory/scoped_refptr.h"
#include "base/scoped_environment_variable_override.h"
#include "base/strings/string_split.h"
#include "base/strings/string_util.h"
#include "base/test/bind.h"
#include "components/dbus/xdg/request.h"
#include "dbus/message.h"
#include "dbus/mock_bus.h"
#include "dbus/mock_object_proxy.h"
#include "dbus/object_path.h"
#include "dbus/object_proxy.h"
#include "dbus/property.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

using ::testing::_;
using ::testing::Invoke;
using ::testing::Return;

namespace dbus_xdg {

namespace {

constexpr char kServiceNameSystemd[] = "org.freedesktop.systemd1";
constexpr char kObjectPathSystemd[] = "/org/freedesktop/systemd1";
constexpr char kInterfaceSystemdManager[] = "org.freedesktop.systemd1.Manager";
constexpr char kMethodStartTransientUnit[] = "StartTransientUnit";
constexpr char kMethodGetUnit[] = "GetUnit";

constexpr char kFakeUnitPath[] = "/fake/unit/path";
constexpr char kActiveState[] = "ActiveState";
constexpr char kStateActive[] = "active";
constexpr char kStateInactive[] = "inactive";

std::unique_ptr<dbus::Response> CreateActiveStateGetAllResponse(
    const std::string& state) {
  auto response = dbus::Response::CreateEmpty();
  dbus::MessageWriter writer(response.get());
  dbus::MessageWriter array_writer(nullptr);
  dbus::MessageWriter dict_entry_writer(nullptr);
  writer.OpenArray("{sv}", &array_writer);
  array_writer.OpenDictEntry(&dict_entry_writer);
  dict_entry_writer.AppendString(kActiveState);
  dict_entry_writer.AppendVariantOfString(state);
  array_writer.CloseContainer(&dict_entry_writer);
  writer.CloseContainer(&array_writer);
  return response;
}

class SetSystemdScopeUnitNameForXdgPortalTest : public ::testing::Test {
 public:
  void SetUp() override { ResetCachedStateForTesting(); }
  void TearDown() override { ResetCachedStateForTesting(); }
};

TEST_F(SetSystemdScopeUnitNameForXdgPortalTest, NotNecessaryInFlatpak) {
  scoped_refptr<dbus::MockBus> bus =
      base::MakeRefCounted<dbus::MockBus>(dbus::Bus::Options());

  base::ScopedEnvironmentVariableOverride env_override("FLATPAK_SANDBOX_DIR",
                                                       "/");

  std::optional<SystemdUnitStatus> status;

  SetSystemdScopeUnitNameForXdgPortal(
      bus.get(), base::BindLambdaForTesting(
                     [&](SystemdUnitStatus result) { status = result; }));

  EXPECT_EQ(status, SystemdUnitStatus::kUnitNotNecessary);
}

TEST_F(SetSystemdScopeUnitNameForXdgPortalTest, NotNecessaryInSnap) {
  scoped_refptr<dbus::MockBus> bus =
      base::MakeRefCounted<dbus::MockBus>(dbus::Bus::Options());

  base::ScopedEnvironmentVariableOverride env_override("SNAP", "/");

  std::optional<SystemdUnitStatus> status;

  SetSystemdScopeUnitNameForXdgPortal(
      bus.get(), base::BindLambdaForTesting(
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

  EXPECT_CALL(*mock_dbus_proxy, DoCallMethod(_, _, _))
      .WillOnce(Invoke([](dbus::MethodCall* method_call, int timeout_ms,
                          dbus::ObjectProxy::ResponseCallback* callback) {
        auto response = dbus::Response::CreateEmpty();
        dbus::MessageWriter writer(response.get());
        writer.AppendBool(false);
        std::move(*callback).Run(response.get());
      }));

  std::optional<SystemdUnitStatus> status;

  SetSystemdScopeUnitNameForXdgPortal(
      bus.get(), base::BindLambdaForTesting(
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

  EXPECT_CALL(*mock_dbus_proxy, DoCallMethod(_, _, _))
      .WillOnce(Invoke([](dbus::MethodCall* method_call, int timeout_ms,
                          dbus::ObjectProxy::ResponseCallback* callback) {
        auto response = dbus::Response::CreateEmpty();
        dbus::MessageWriter writer(response.get());
        writer.AppendBool(true);
        std::move(*callback).Run(response.get());
      }));

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

  EXPECT_CALL(*mock_systemd_proxy, DoCallMethod(_, _, _))
      .WillOnce(Invoke([](dbus::MethodCall* method_call, int timeout_ms,
                          dbus::ObjectProxy::ResponseCallback* callback) {
        // Expect kMethodStartTransientUnit first.
        EXPECT_EQ(method_call->GetInterface(), kInterfaceSystemdManager);
        EXPECT_EQ(method_call->GetMember(), kMethodStartTransientUnit);

        // Simulate a successful response
        auto response = dbus::Response::CreateEmpty();
        std::move(*callback).Run(response.get());
      }))
      .WillOnce(Invoke([obj_path = kFakeUnitPath](
                           dbus::MethodCall* method_call, int timeout_ms,
                           dbus::ObjectProxy::ResponseCallback* callback) {
        // Then expect kMethodGetUnit. A valid path must be provided.
        EXPECT_EQ(method_call->GetInterface(), kInterfaceSystemdManager);
        EXPECT_EQ(method_call->GetMember(), kMethodGetUnit);

        // Simulate a successful response and provide a fake path to the object.
        auto response = dbus::Response::CreateEmpty();
        dbus::MessageWriter writer(response.get());
        writer.AppendObjectPath(dbus::ObjectPath(obj_path));
        std::move(*callback).Run(response.get());
      }));

  EXPECT_CALL(*mock_dbus_unit_proxy, DoCallMethod(_, _, _))
      .WillOnce(Invoke([](dbus::MethodCall* method_call, int timeout_ms,
                          dbus::ObjectProxy::ResponseCallback* callback) {
        EXPECT_EQ(method_call->GetInterface(), dbus::kPropertiesInterface);
        EXPECT_EQ(method_call->GetMember(), dbus::kPropertiesGetAll);
        // Simulate a successful response with "active" state.
        auto response = CreateActiveStateGetAllResponse(kStateActive);
        std::move(*callback).Run(response.get());
      }));

  std::optional<SystemdUnitStatus> status;

  SetSystemdScopeUnitNameForXdgPortal(
      bus.get(), base::BindLambdaForTesting(
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

  EXPECT_CALL(*mock_dbus_proxy, DoCallMethod(_, _, _))
      .WillOnce(Invoke([](dbus::MethodCall* method_call, int timeout_ms,
                          dbus::ObjectProxy::ResponseCallback* callback) {
        auto response = dbus::Response::CreateEmpty();
        dbus::MessageWriter writer(response.get());
        writer.AppendBool(true);
        std::move(*callback).Run(response.get());
      }));

  auto mock_systemd_proxy = base::MakeRefCounted<dbus::MockObjectProxy>(
      bus.get(), kServiceNameSystemd, dbus::ObjectPath(kObjectPathSystemd));

  EXPECT_CALL(*bus, GetObjectProxy(kServiceNameSystemd,
                                   dbus::ObjectPath(kObjectPathSystemd)))
      .WillOnce(Return(mock_systemd_proxy.get()));

  EXPECT_CALL(*mock_systemd_proxy, DoCallMethod(_, _, _))
      .WillOnce(Invoke([](dbus::MethodCall* method_call, int timeout_ms,
                          dbus::ObjectProxy::ResponseCallback* callback) {
        // Simulate a failure by invoking the callback with nullptr
        std::move(*callback).Run(nullptr);
      }));

  std::optional<SystemdUnitStatus> status;

  SetSystemdScopeUnitNameForXdgPortal(
      bus.get(), base::BindLambdaForTesting(
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

  EXPECT_CALL(*mock_dbus_proxy, DoCallMethod(_, _, _))
      .WillOnce(Invoke([](dbus::MethodCall* method_call, int timeout_ms,
                          dbus::ObjectProxy::ResponseCallback* callback) {
        auto response = dbus::Response::CreateEmpty();
        dbus::MessageWriter writer(response.get());
        writer.AppendBool(true);
        std::move(*callback).Run(response.get());
      }));

  auto mock_systemd_proxy = base::MakeRefCounted<dbus::MockObjectProxy>(
      bus.get(), kServiceNameSystemd, dbus::ObjectPath(kObjectPathSystemd));

  EXPECT_CALL(*bus, GetObjectProxy(kServiceNameSystemd,
                                   dbus::ObjectPath(kObjectPathSystemd)))
      .Times(2)
      .WillRepeatedly(Return(mock_systemd_proxy.get()));

  EXPECT_CALL(*mock_systemd_proxy, DoCallMethod(_, _, _))
      .WillOnce(Invoke([](dbus::MethodCall* method_call, int timeout_ms,
                          dbus::ObjectProxy::ResponseCallback* callback) {
        EXPECT_EQ(method_call->GetInterface(), kInterfaceSystemdManager);
        EXPECT_EQ(method_call->GetMember(), kMethodStartTransientUnit);

        // Simulate a successful response
        auto response = dbus::Response::CreateEmpty();
        std::move(*callback).Run(response.get());
      }))
      .WillOnce(Invoke([](dbus::MethodCall* method_call, int timeout_ms,
                          dbus::ObjectProxy::ResponseCallback* callback) {
        EXPECT_EQ(method_call->GetInterface(), kInterfaceSystemdManager);
        EXPECT_EQ(method_call->GetMember(), kMethodGetUnit);

        // Simulate a failure response.
        std::move(*callback).Run(nullptr);
      }));

  std::optional<SystemdUnitStatus> status;

  SetSystemdScopeUnitNameForXdgPortal(
      bus.get(), base::BindLambdaForTesting(
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

  EXPECT_CALL(*mock_dbus_proxy, DoCallMethod(_, _, _))
      .WillOnce(Invoke([](dbus::MethodCall* method_call, int timeout_ms,
                          dbus::ObjectProxy::ResponseCallback* callback) {
        auto response = dbus::Response::CreateEmpty();
        dbus::MessageWriter writer(response.get());
        writer.AppendBool(true);
        std::move(*callback).Run(response.get());
      }));

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

  EXPECT_CALL(*mock_systemd_proxy, DoCallMethod(_, _, _))
      .WillOnce(Invoke([](dbus::MethodCall* method_call, int timeout_ms,
                          dbus::ObjectProxy::ResponseCallback* callback) {
        EXPECT_EQ(method_call->GetInterface(), kInterfaceSystemdManager);
        EXPECT_EQ(method_call->GetMember(), kMethodStartTransientUnit);

        // Simulate a successful response
        auto response = dbus::Response::CreateEmpty();
        std::move(*callback).Run(response.get());
      }))
      .WillOnce(Invoke([obj_path = kFakeUnitPath](
                           dbus::MethodCall* method_call, int timeout_ms,
                           dbus::ObjectProxy::ResponseCallback* callback) {
        EXPECT_EQ(method_call->GetInterface(), kInterfaceSystemdManager);
        EXPECT_EQ(method_call->GetMember(), kMethodGetUnit);

        // Simulate a successful response
        auto response = dbus::Response::CreateEmpty();
        dbus::MessageWriter writer(response.get());
        writer.AppendObjectPath(dbus::ObjectPath(obj_path));
        std::move(*callback).Run(response.get());
      }));

  EXPECT_CALL(*mock_dbus_unit_proxy, DoCallMethod(_, _, _))
      .WillOnce(Invoke([](dbus::MethodCall* method_call, int timeout_ms,
                          dbus::ObjectProxy::ResponseCallback* callback) {
        // Then expect kMethodGetUnit. A valid path must be provided.
        EXPECT_EQ(method_call->GetInterface(), dbus::kPropertiesInterface);
        EXPECT_EQ(method_call->GetMember(), dbus::kPropertiesGetAll);

        // Simulate a successful response, but with inactive state.
        auto response = CreateActiveStateGetAllResponse(kStateInactive);
        std::move(*callback).Run(response.get());
      }));

  std::optional<SystemdUnitStatus> status;

  SetSystemdScopeUnitNameForXdgPortal(
      bus.get(), base::BindLambdaForTesting(
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

  EXPECT_CALL(*mock_dbus_proxy, DoCallMethod(_, _, _))
      .WillOnce(Invoke([](dbus::MethodCall* method_call, int timeout_ms,
                          dbus::ObjectProxy::ResponseCallback* callback) {
        auto response = dbus::Response::CreateEmpty();
        dbus::MessageWriter writer(response.get());
        writer.AppendBool(true);
        std::move(*callback).Run(response.get());
      }));

  base::ScopedEnvironmentVariableOverride env_override("CHROME_VERSION_EXTRA",
                                                       "beta");

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

  EXPECT_CALL(*mock_systemd_proxy, DoCallMethod(_, _, _))
      .WillOnce(Invoke([&](dbus::MethodCall* method_call, int timeout_ms,
                           dbus::ObjectProxy::ResponseCallback* callback) {
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
        std::move(*callback).Run(response.get());
      }))
      .WillOnce(Invoke([obj_path = kFakeUnitPath](
                           dbus::MethodCall* method_call, int timeout_ms,
                           dbus::ObjectProxy::ResponseCallback* callback) {
        EXPECT_EQ(method_call->GetInterface(), kInterfaceSystemdManager);
        EXPECT_EQ(method_call->GetMember(), kMethodGetUnit);

        // Simulate a successful response
        auto response = dbus::Response::CreateEmpty();
        dbus::MessageWriter writer(response.get());
        writer.AppendObjectPath(dbus::ObjectPath(obj_path));
        std::move(*callback).Run(response.get());
      }));

  EXPECT_CALL(*mock_dbus_unit_proxy, DoCallMethod(_, _, _))
      .WillOnce(Invoke([](dbus::MethodCall* method_call, int timeout_ms,
                          dbus::ObjectProxy::ResponseCallback* callback) {
        // Then expect kMethodGetUnit. A valid path must be provided.
        EXPECT_EQ(method_call->GetInterface(), dbus::kPropertiesInterface);
        EXPECT_EQ(method_call->GetMember(), dbus::kPropertiesGetAll);

        // Simulate a successful response
        auto response = CreateActiveStateGetAllResponse(kStateActive);
        std::move(*callback).Run(response.get());
      }));

  std::optional<SystemdUnitStatus> status;

  SetSystemdScopeUnitNameForXdgPortal(
      bus.get(), base::BindLambdaForTesting(
                     [&](SystemdUnitStatus result) { status = result; }));

  EXPECT_EQ(status, SystemdUnitStatus::kUnitStarted);
}

}  // namespace

}  // namespace dbus_xdg
