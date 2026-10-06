// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/dbus/xdg/portal.h"

#include <map>
#include <optional>
#include <string>
#include <string_view>
#include <utility>
#include <vector>

#include "base/functional/bind.h"
#include "base/memory/scoped_refptr.h"
#include "base/scoped_environment_variable_override.h"
#include "base/test/run_until.h"
#include "base/test/task_environment.h"
#include "components/dbus/xdg/portal_constants.h"
#include "components/dbus/xdg/systemd.h"
#include "components/dbus/xdg/systemd_constants.h"
#include "dbus/mock_bus.h"
#include "dbus/mock_object_proxy.h"
#include "dbus/object_path.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

using ::testing::_;
using ::testing::ElementsAre;
using ::testing::Return;

namespace dbus_xdg {

namespace {

constexpr char kFakeUnitPath[] = "/fake/unit/path";

class RequestXdgDesktopPortalTest : public testing::Test {
 public:
  void SetUp() override {
    SetPortalStateForTesting(PortalRegistrarState::kIdle);
    bus_ = base::MakeRefCounted<dbus::MockBus>(dbus::Bus::Options());

    EXPECT_CALL(*bus_, GetOriginTaskRunner())
        .WillRepeatedly(
            Return(task_environment_.GetMainThreadTaskRunner().get()));
  }

  void TearDown() override {
    SetPortalStateForTesting(PortalRegistrarState::kIdle);
  }

 protected:
  base::test::TaskEnvironment task_environment_;
  scoped_refptr<dbus::MockBus> bus_;
};

TEST_F(RequestXdgDesktopPortalTest, RequestXdgDesktopPortalSuccessNoSystemd) {
  // Mocks for SetSystemdScopeUnitNameForXdgPortal
  auto mock_dbus_proxy = base::MakeRefCounted<dbus::MockObjectProxy>(
      bus_.get(), DBUS_SERVICE_DBUS, dbus::ObjectPath(DBUS_PATH_DBUS));

  EXPECT_CALL(*bus_, GetObjectProxy(DBUS_SERVICE_DBUS,
                                    dbus::ObjectPath(DBUS_PATH_DBUS)))
      .WillRepeatedly(Return(mock_dbus_proxy.get()));

  // Mock NameHasOwner for systemd (return false to skip systemd setup)
  EXPECT_CALL(*mock_dbus_proxy, CallMethod(_, _, _))
      .WillRepeatedly([](dbus::MethodCall* method_call, int timeout_ms,
                         dbus::ObjectProxy::ResponseCallback callback) {
        if (method_call->GetMember() == "NameHasOwner") {
          dbus::MessageReader reader(method_call);
          std::string name;
          reader.PopString(&name);
          bool has_owner = false;
          if (name == kPortalServiceName) {
            has_owner = true;  // Portal exists
          } else if (name == "org.freedesktop.systemd1") {
            has_owner = false;  // Systemd not present
          }

          auto response = dbus::Response::CreateEmpty();
          dbus::MessageWriter writer(response.get());
          writer.AppendBool(has_owner);
          std::move(callback).Run(response.get());
          return;
        }
        std::move(callback).Run(nullptr);
      });

  // Mock Portal Proxy for Register
  auto mock_portal_proxy = base::MakeRefCounted<dbus::MockObjectProxy>(
      bus_.get(), kPortalServiceName, dbus::ObjectPath(kPortalObjectPath));

  EXPECT_CALL(*bus_, GetObjectProxy(kPortalServiceName,
                                    dbus::ObjectPath(kPortalObjectPath)))
      .WillRepeatedly(Return(mock_portal_proxy.get()));

  // Expect a Register call because systemd unit creation failed
  // (kNoSystemdService).
  EXPECT_CALL(*mock_portal_proxy, CallMethodWithErrorResponse(_, _, _))
      .WillOnce([](dbus::MethodCall* method_call, int timeout_ms,
                   dbus::ObjectProxy::ResponseOrErrorCallback callback) {
        EXPECT_EQ(method_call->GetInterface(), kRegistryInterface);
        EXPECT_EQ(method_call->GetMember(), kMethodRegister);
        auto response = dbus::Response::CreateEmpty();
        std::move(callback).Run(response.get(), nullptr);
      });

  // Expect SetNameOwnerChangedCallback
  EXPECT_CALL(*mock_portal_proxy, SetNameOwnerChangedCallback(_));

  // Expect GetVersion call
  EXPECT_CALL(*mock_portal_proxy, CallMethod(_, _, _))
      .WillRepeatedly([](dbus::MethodCall* method_call, int timeout_ms,
                         dbus::ObjectProxy::ResponseCallback callback) {
        if (method_call->GetInterface() == DBUS_INTERFACE_PROPERTIES &&
            method_call->GetMember() == "Get") {
          dbus::MessageReader reader(method_call);
          std::string interface_name;
          std::string property_name;
          reader.PopString(&interface_name);
          reader.PopString(&property_name);

          if (interface_name == kFileChooserInterfaceName &&
              property_name == "version") {
            auto response = dbus::Response::CreateEmpty();
            dbus::MessageWriter writer(response.get());
            writer.AppendVariantOfUint32(3);
            std::move(callback).Run(response.get());
            return;
          }
        }
        std::move(callback).Run(nullptr);
      });

  uint32_t version = 0;
  base::RunLoop run_loop;
  RequestXdgDesktopPortal(
      bus_.get(), kFileChooserInterfaceName,
      base::BindOnce(
          [](uint32_t* out, base::OnceClosure quit_closure, uint32_t res) {
            *out = res;
            std::move(quit_closure).Run();
          },
          &version, run_loop.QuitClosure()));

  run_loop.Run();
  EXPECT_EQ(version, 3u);
}

TEST_F(RequestXdgDesktopPortalTest, RequestXdgDesktopPortalSuccessWithSystemd) {
  // Mocks for SetSystemdScopeUnitNameForXdgPortal
  auto mock_dbus_proxy = base::MakeRefCounted<dbus::MockObjectProxy>(
      bus_.get(), DBUS_SERVICE_DBUS, dbus::ObjectPath(DBUS_PATH_DBUS));

  EXPECT_CALL(*bus_, GetObjectProxy(DBUS_SERVICE_DBUS,
                                    dbus::ObjectPath(DBUS_PATH_DBUS)))
      .WillRepeatedly(Return(mock_dbus_proxy.get()));

  // Mock NameHasOwner for systemd (return true to enable systemd setup)
  EXPECT_CALL(*mock_dbus_proxy, CallMethod(_, _, _))
      .WillRepeatedly([](dbus::MethodCall* method_call, int timeout_ms,
                         dbus::ObjectProxy::ResponseCallback callback) {
        if (method_call->GetMember() == "NameHasOwner") {
          dbus::MessageReader reader(method_call);
          std::string name;
          reader.PopString(&name);
          const bool has_owner = true;  // Both Portal and Systemd exist

          auto response = dbus::Response::CreateEmpty();
          dbus::MessageWriter writer(response.get());
          writer.AppendBool(has_owner);
          std::move(callback).Run(response.get());
          return;
        }
        std::move(callback).Run(nullptr);
      });

  // Mock Systemd Proxy and interactions

  auto mock_systemd_proxy = base::MakeRefCounted<dbus::MockObjectProxy>(
      bus_.get(), kServiceNameSystemd, dbus::ObjectPath(kObjectPathSystemd));

  EXPECT_CALL(*bus_, GetObjectProxy(kServiceNameSystemd,
                                    dbus::ObjectPath(kObjectPathSystemd)))
      .WillRepeatedly(Return(mock_systemd_proxy.get()));

  auto mock_dbus_unit_proxy = base::MakeRefCounted<dbus::MockObjectProxy>(
      bus_.get(), kServiceNameSystemd, dbus::ObjectPath(kFakeUnitPath));
  EXPECT_CALL(*bus_, GetObjectProxy(kServiceNameSystemd,
                                    dbus::ObjectPath(kFakeUnitPath)))
      .WillRepeatedly(Return(mock_dbus_unit_proxy.get()));

  EXPECT_CALL(*mock_systemd_proxy, CallMethod(_, _, _))
      .WillRepeatedly([&](dbus::MethodCall* method_call, int timeout_ms,
                          dbus::ObjectProxy::ResponseCallback callback) {
        if (method_call->GetInterface() == kInterfaceSystemdManager &&
            method_call->GetMember() == kMethodStartTransientUnit) {
          auto response = dbus::Response::CreateEmpty();
          std::move(callback).Run(response.get());
        } else if (method_call->GetInterface() == kInterfaceSystemdManager &&
                   method_call->GetMember() == kMethodGetUnit) {
          auto response = dbus::Response::CreateEmpty();
          dbus::MessageWriter writer(response.get());
          writer.AppendObjectPath(dbus::ObjectPath(kFakeUnitPath));
          std::move(callback).Run(response.get());
        }
      });

  EXPECT_CALL(*mock_dbus_unit_proxy, CallMethod(_, _, _))
      .WillRepeatedly([](dbus::MethodCall* method_call, int timeout_ms,
                         dbus::ObjectProxy::ResponseCallback callback) {
        // Simulate a successful response with "active" state.
        auto response = dbus::Response::CreateEmpty();
        dbus::MessageWriter writer(response.get());
        dbus::MessageWriter array_writer(nullptr);
        dbus::MessageWriter dict_entry_writer(nullptr);
        writer.OpenArray("{sv}", &array_writer);
        array_writer.OpenDictEntry(&dict_entry_writer);
        dict_entry_writer.AppendString(kSystemdActiveStateProp);
        dict_entry_writer.AppendVariantOfString(kSystemdStateActive);
        array_writer.CloseContainer(&dict_entry_writer);
        writer.CloseContainer(&array_writer);
        std::move(callback).Run(response.get());
      });

  // Mock Portal Proxy
  auto mock_portal_proxy = base::MakeRefCounted<dbus::MockObjectProxy>(
      bus_.get(), kPortalServiceName, dbus::ObjectPath(kPortalObjectPath));

  EXPECT_CALL(*bus_, GetObjectProxy(kPortalServiceName,
                                    dbus::ObjectPath(kPortalObjectPath)))
      .WillRepeatedly(Return(mock_portal_proxy.get()));

  // Expect Register even though the systemd unit started (portal >= 1.21
  // needs an explicit app id).
  EXPECT_CALL(*mock_portal_proxy, CallMethodWithErrorResponse(_, _, _))
      .WillOnce([](dbus::MethodCall* method_call, int timeout_ms,
                   dbus::ObjectProxy::ResponseOrErrorCallback callback) {
        EXPECT_EQ(method_call->GetInterface(), kRegistryInterface);
        EXPECT_EQ(method_call->GetMember(), kMethodRegister);
        auto response = dbus::Response::CreateEmpty();
        std::move(callback).Run(response.get(), nullptr);
      });

  // Expect SetNameOwnerChangedCallback
  EXPECT_CALL(*mock_portal_proxy, SetNameOwnerChangedCallback(_));

  // Expect GetVersion call
  EXPECT_CALL(*mock_portal_proxy, CallMethod(_, _, _))
      .WillRepeatedly([](dbus::MethodCall* method_call, int timeout_ms,
                         dbus::ObjectProxy::ResponseCallback callback) {
        if (method_call->GetInterface() == DBUS_INTERFACE_PROPERTIES &&
            method_call->GetMember() == "Get") {
          dbus::MessageReader reader(method_call);
          std::string interface_name;
          std::string property_name;
          reader.PopString(&interface_name);
          reader.PopString(&property_name);

          if (interface_name == kFileChooserInterfaceName &&
              property_name == "version") {
            auto response = dbus::Response::CreateEmpty();
            dbus::MessageWriter writer(response.get());
            writer.AppendVariantOfUint32(3);
            std::move(callback).Run(response.get());
            return;
          }
        }
        std::move(callback).Run(nullptr);
      });

  uint32_t version = 0;
  base::RunLoop run_loop;
  RequestXdgDesktopPortal(
      bus_.get(), kFileChooserInterfaceName,
      base::BindOnce(
          [](uint32_t* out, base::OnceClosure quit_closure, uint32_t res) {
            *out = res;
            std::move(quit_closure).Run();
          },
          &version, run_loop.QuitClosure()));

  run_loop.Run();
  EXPECT_EQ(version, 3u);
}

TEST_F(RequestXdgDesktopPortalTest,
       RequestXdgDesktopPortalSkipsRegisterInSnap) {
  // Under Flatpak or Snap (kUnitNotNecessary), the sandbox provides the app
  // id, so the portal must not be registered with.
  base::ScopedEnvironmentVariableOverride snap_env("SNAP", "/snap/app");

  auto mock_dbus_proxy = base::MakeRefCounted<dbus::MockObjectProxy>(
      bus_.get(), DBUS_SERVICE_DBUS, dbus::ObjectPath(DBUS_PATH_DBUS));

  EXPECT_CALL(*bus_, GetObjectProxy(DBUS_SERVICE_DBUS,
                                    dbus::ObjectPath(DBUS_PATH_DBUS)))
      .WillRepeatedly(Return(mock_dbus_proxy.get()));

  // The portal service exists; systemd is never queried because the sandbox
  // short-circuits unit detection.
  EXPECT_CALL(*mock_dbus_proxy, CallMethod(_, _, _))
      .WillRepeatedly([](dbus::MethodCall* method_call, int timeout_ms,
                         dbus::ObjectProxy::ResponseCallback callback) {
        if (method_call->GetMember() == "NameHasOwner") {
          dbus::MessageReader reader(method_call);
          std::string name;
          reader.PopString(&name);
          auto response = dbus::Response::CreateEmpty();
          dbus::MessageWriter writer(response.get());
          writer.AppendBool(name == kPortalServiceName);
          std::move(callback).Run(response.get());
          return;
        }
        std::move(callback).Run(nullptr);
      });

  auto mock_portal_proxy = base::MakeRefCounted<dbus::MockObjectProxy>(
      bus_.get(), kPortalServiceName, dbus::ObjectPath(kPortalObjectPath));

  EXPECT_CALL(*bus_, GetObjectProxy(kPortalServiceName,
                                    dbus::ObjectPath(kPortalObjectPath)))
      .WillRepeatedly(Return(mock_portal_proxy.get()));

  // No Register and no NameOwnerChanged listener on the skip path.
  EXPECT_CALL(*mock_portal_proxy, CallMethodWithErrorResponse(_, _, _))
      .Times(0);
  EXPECT_CALL(*mock_portal_proxy, SetNameOwnerChangedCallback(_)).Times(0);

  // The version is still fetched.
  EXPECT_CALL(*mock_portal_proxy, CallMethod(_, _, _))
      .WillRepeatedly([](dbus::MethodCall* method_call, int timeout_ms,
                         dbus::ObjectProxy::ResponseCallback callback) {
        if (method_call->GetInterface() == DBUS_INTERFACE_PROPERTIES &&
            method_call->GetMember() == "Get") {
          dbus::MessageReader reader(method_call);
          std::string interface_name;
          std::string property_name;
          reader.PopString(&interface_name);
          reader.PopString(&property_name);

          if (interface_name == kFileChooserInterfaceName &&
              property_name == "version") {
            auto response = dbus::Response::CreateEmpty();
            dbus::MessageWriter writer(response.get());
            writer.AppendVariantOfUint32(3);
            std::move(callback).Run(response.get());
            return;
          }
        }
        std::move(callback).Run(nullptr);
      });

  uint32_t version = 0;
  base::RunLoop run_loop;
  RequestXdgDesktopPortal(
      bus_.get(), kFileChooserInterfaceName,
      base::BindOnce(
          [](uint32_t* out, base::OnceClosure quit_closure, uint32_t res) {
            *out = res;
            std::move(quit_closure).Run();
          },
          &version, run_loop.QuitClosure()));

  run_loop.Run();
  EXPECT_EQ(version, 3u);
}

// Fakes a portal without systemd. Interface version queries are answered when
// ReplyToVersionQueries() is called.
class RequestXdgDesktopPortalInterfaceTest
    : public RequestXdgDesktopPortalTest {
 public:
  void SetUp() override {
    RequestXdgDesktopPortalTest::SetUp();

    dbus_proxy_ = base::MakeRefCounted<dbus::MockObjectProxy>(
        bus_.get(), DBUS_SERVICE_DBUS, dbus::ObjectPath(DBUS_PATH_DBUS));
    ON_CALL(*bus_,
            GetObjectProxy(DBUS_SERVICE_DBUS, dbus::ObjectPath(DBUS_PATH_DBUS)))
        .WillByDefault(Return(dbus_proxy_.get()));
    ON_CALL(*dbus_proxy_, CallMethod(_, _, _))
        .WillByDefault([this](dbus::MethodCall* method_call, int timeout_ms,
                              dbus::ObjectProxy::ResponseCallback callback) {
          if (method_call->GetMember() != "NameHasOwner") {
            std::move(callback).Run(nullptr);
            return;
          }
          dbus::MessageReader reader(method_call);
          std::string name;
          reader.PopString(&name);
          auto response = dbus::Response::CreateEmpty();
          dbus::MessageWriter writer(response.get());
          writer.AppendBool(name == kPortalServiceName && portal_exists_);
          std::move(callback).Run(response.get());
        });

    portal_proxy_ = base::MakeRefCounted<dbus::MockObjectProxy>(
        bus_.get(), kPortalServiceName, dbus::ObjectPath(kPortalObjectPath));
    ON_CALL(*bus_, GetObjectProxy(kPortalServiceName,
                                  dbus::ObjectPath(kPortalObjectPath)))
        .WillByDefault(Return(portal_proxy_.get()));
    // Register.
    ON_CALL(*portal_proxy_, CallMethodWithErrorResponse(_, _, _))
        .WillByDefault([this](dbus::MethodCall* method_call, int timeout_ms,
                              dbus::ObjectProxy::ResponseOrErrorCallback cb) {
          EXPECT_EQ(method_call->GetMember(), kMethodRegister);
          ++register_calls_;
          auto response = dbus::Response::CreateEmpty();
          std::move(cb).Run(response.get(), nullptr);
        });
    ON_CALL(*portal_proxy_, SetNameOwnerChangedCallback(_))
        .WillByDefault(
            [this](dbus::ObjectProxy::NameOwnerChangedCallback callback) {
              name_owner_changed_callback_ = std::move(callback);
            });
    // Properties.Get for interface versions.
    ON_CALL(*portal_proxy_, CallMethod(_, _, _))
        .WillByDefault([this](dbus::MethodCall* method_call, int timeout_ms,
                              dbus::ObjectProxy::ResponseCallback callback) {
          EXPECT_EQ(method_call->GetInterface(), DBUS_INTERFACE_PROPERTIES);
          EXPECT_EQ(method_call->GetMember(), "Get");
          dbus::MessageReader reader(method_call);
          std::string interface_name;
          std::string property_name;
          EXPECT_TRUE(reader.PopString(&interface_name));
          EXPECT_TRUE(reader.PopString(&property_name));
          EXPECT_EQ(property_name, "version");
          version_queries_.push_back(interface_name);
          pending_queries_.emplace_back(interface_name, std::move(callback));
        });
  }

  void TearDown() override {
    // Setup callbacks hold references to the mocks.
    testing::Mock::AllowLeak(bus_.get());
    testing::Mock::AllowLeak(dbus_proxy_.get());
    testing::Mock::AllowLeak(portal_proxy_.get());
    RequestXdgDesktopPortalTest::TearDown();
  }

 protected:
  // Replies to all pending version queries. Interfaces not in `versions_`
  // reply with an error.
  void ReplyToVersionQueries() {
    auto pending = std::move(pending_queries_);
    pending_queries_.clear();
    for (auto& [interface_name, callback] : pending) {
      auto it = versions_.find(interface_name);
      if (it == versions_.end()) {
        std::move(callback).Run(nullptr);
        continue;
      }
      auto response = dbus::Response::CreateEmpty();
      dbus::MessageWriter writer(response.get());
      writer.AppendVariantOfUint32(it->second);
      std::move(callback).Run(response.get());
    }
  }

  // Waits until `count` version queries have been sent.
  [[nodiscard]] bool WaitForVersionQueries(size_t count) {
    return base::test::RunUntil(
        [&] { return pending_queries_.size() >= count; });
  }

  // Requests `interface_name` and stores the version in `result`, which is
  // reset to nullopt until the callback runs.
  void Request(std::string_view interface_name,
               std::optional<uint32_t>* result) {
    *result = std::nullopt;
    RequestXdgDesktopPortal(
        bus_.get(), interface_name,
        base::BindOnce([](std::optional<uint32_t>* out,
                          uint32_t version) { *out = version; },
                       result));
  }

  bool portal_exists_ = true;
  std::map<std::string, uint32_t> versions_;
  std::vector<std::string> version_queries_;
  std::vector<std::pair<std::string, dbus::ObjectProxy::ResponseCallback>>
      pending_queries_;
  int register_calls_ = 0;
  dbus::ObjectProxy::NameOwnerChangedCallback name_owner_changed_callback_;
  scoped_refptr<dbus::MockObjectProxy> dbus_proxy_;
  scoped_refptr<dbus::MockObjectProxy> portal_proxy_;
};

constexpr char kSettingsInterface[] = "org.freedesktop.portal.Settings";
constexpr char kLocationInterface[] = "org.freedesktop.portal.Location";

TEST_F(RequestXdgDesktopPortalInterfaceTest, QueriesRequestedInterface) {
  versions_[kLocationInterface] = 2;

  std::optional<uint32_t> version;
  Request(kLocationInterface, &version);
  ASSERT_TRUE(WaitForVersionQueries(1));
  EXPECT_EQ(register_calls_, 1);
  EXPECT_THAT(version_queries_, ElementsAre(kLocationInterface));

  ReplyToVersionQueries();
  EXPECT_EQ(version, 2u);
}

// A missing interface (e.g. FileChooser without a desktop backend) must not
// make other interfaces unavailable.
TEST_F(RequestXdgDesktopPortalInterfaceTest, MissingInterfaceIsIndependent) {
  versions_[kSettingsInterface] = 2;

  std::optional<uint32_t> file_chooser_version;
  std::optional<uint32_t> settings_version;
  Request(kFileChooserInterfaceName, &file_chooser_version);
  Request(kSettingsInterface, &settings_version);
  ASSERT_TRUE(WaitForVersionQueries(2));
  ReplyToVersionQueries();

  EXPECT_EQ(file_chooser_version, 0u);
  EXPECT_EQ(settings_version, 2u);

  // Later requests for either interface still get their own result.
  Request(kSettingsInterface, &settings_version);
  EXPECT_EQ(settings_version, 2u);
  Request(kFileChooserInterfaceName, &file_chooser_version);
  EXPECT_EQ(file_chooser_version, 0u);
}

TEST_F(RequestXdgDesktopPortalInterfaceTest, ConcurrentRequestsShareQuery) {
  versions_[kSettingsInterface] = 2;

  // Requests made while setup is in progress.
  std::optional<uint32_t> version1;
  std::optional<uint32_t> version2;
  Request(kSettingsInterface, &version1);
  Request(kSettingsInterface, &version2);
  ASSERT_TRUE(WaitForVersionQueries(1));
  EXPECT_EQ(version_queries_.size(), 1u);

  // A request made while the version query is in flight.
  std::optional<uint32_t> version3;
  Request(kSettingsInterface, &version3);
  EXPECT_EQ(version_queries_.size(), 1u);
  EXPECT_FALSE(version3.has_value());

  ReplyToVersionQueries();
  EXPECT_EQ(version1, 2u);
  EXPECT_EQ(version2, 2u);
  EXPECT_EQ(version3, 2u);
}

TEST_F(RequestXdgDesktopPortalInterfaceTest, CachesVersions) {
  versions_[kSettingsInterface] = 2;

  std::optional<uint32_t> version;
  Request(kSettingsInterface, &version);
  ASSERT_TRUE(WaitForVersionQueries(1));
  ReplyToVersionQueries();
  EXPECT_EQ(version, 2u);

  // Cached results are returned synchronously without another query.
  Request(kSettingsInterface, &version);
  EXPECT_EQ(version, 2u);
  EXPECT_EQ(version_queries_.size(), 1u);

  // Requesting a new interface after setup queries only that interface.
  versions_[kLocationInterface] = 1;
  Request(kLocationInterface, &version);
  EXPECT_FALSE(version.has_value());
  ReplyToVersionQueries();
  EXPECT_EQ(version, 1u);
  EXPECT_EQ(register_calls_, 1);
  EXPECT_THAT(version_queries_,
              ElementsAre(kSettingsInterface, kLocationInterface));
}

TEST_F(RequestXdgDesktopPortalInterfaceTest, PortalRestartClearsCache) {
  versions_[kSettingsInterface] = 1;

  std::optional<uint32_t> version;
  Request(kSettingsInterface, &version);
  ASSERT_TRUE(WaitForVersionQueries(1));
  ReplyToVersionQueries();
  EXPECT_EQ(version, 1u);

  // The portal restarts, e.g. after a backend was installed.
  ASSERT_TRUE(name_owner_changed_callback_);
  versions_[kSettingsInterface] = 2;
  name_owner_changed_callback_.Run(":1.1", ":1.2");
  EXPECT_EQ(register_calls_, 2);

  Request(kSettingsInterface, &version);
  ReplyToVersionQueries();
  EXPECT_EQ(version, 2u);
  EXPECT_EQ(version_queries_.size(), 2u);
}

TEST_F(RequestXdgDesktopPortalInterfaceTest, SetupFailure) {
  portal_exists_ = false;

  std::optional<uint32_t> version1;
  std::optional<uint32_t> version2;
  Request(kSettingsInterface, &version1);
  Request(kLocationInterface, &version2);
  ASSERT_TRUE(base::test::RunUntil(
      [&] { return version1.has_value() && version2.has_value(); }));

  EXPECT_EQ(version1, 0u);
  EXPECT_EQ(version2, 0u);
  EXPECT_TRUE(version_queries_.empty());
  EXPECT_EQ(register_calls_, 0);

  // Later requests fail immediately.
  Request(kSettingsInterface, &version1);
  EXPECT_EQ(version1, 0u);
}

TEST_F(RequestXdgDesktopPortalTest, TestingOverrides) {
  SetPortalStateForTesting(PortalRegistrarState::kSuccess);
  SetPortalInterfaceVersionForTesting(kFileChooserInterfaceName, 0);
  SetPortalInterfaceVersionForTesting("org.freedesktop.portal.Print", 4);

  auto get_version = [this](std::string_view interface_name) {
    uint32_t result = 1234;
    RequestXdgDesktopPortal(
        bus_.get(), interface_name,
        base::BindOnce([](uint32_t* out, uint32_t version) { *out = version; },
                       &result));
    return result;
  };
  EXPECT_EQ(get_version(kFileChooserInterfaceName), 0u);
  EXPECT_EQ(get_version("org.freedesktop.portal.Print"), 4u);
  EXPECT_EQ(get_version("org.freedesktop.portal.Settings"),
            kDefaultPortalVersionForTesting);

  SetPortalStateForTesting(PortalRegistrarState::kFailed);
  EXPECT_EQ(get_version("org.freedesktop.portal.Settings"), 0u);
}

}  // namespace

}  // namespace dbus_xdg
