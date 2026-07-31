// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/web_applications/os_integration/dynamic_launcher_portal.h"

#include <map>
#include <memory>
#include <optional>
#include <string>
#include <string_view>
#include <utility>
#include <vector>

#include "base/test/bind.h"
#include "base/test/test_future.h"
#include "components/dbus/xdg/portal_constants.h"
#include "content/public/test/browser_task_environment.h"
#include "dbus/message.h"
#include "dbus/mock_bus.h"
#include "dbus/mock_object_proxy.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "url/gurl.h"

using ::testing::_;
using ::testing::Invoke;
using ::testing::Return;

namespace web_app {

namespace {

// GMock matcher to check the interface and member names of a D-Bus method call.
MATCHER_P2(MethodCallIs, interface_name, member_name, "") {
  return arg && arg->GetInterface() == interface_name &&
         arg->GetMember() == member_name;
}

}  // namespace

class DynamicLauncherPortalTest : public testing::Test {
 protected:
  void SetUp() override {
    mock_bus_ = base::MakeRefCounted<dbus::MockBus>(dbus::Bus::Options());
    mock_proxy_ = base::MakeRefCounted<dbus::MockObjectProxy>(
        mock_bus_.get(), dbus_xdg::kPortalServiceName,
        dbus::ObjectPath(dbus_xdg::kPortalObjectPath));
    mock_dbus_proxy_ = base::MakeRefCounted<dbus::MockObjectProxy>(
        mock_bus_.get(), DBUS_SERVICE_DBUS, dbus::ObjectPath(DBUS_PATH_DBUS));

    EXPECT_CALL(*mock_bus_,
                GetObjectProxy(dbus_xdg::kPortalServiceName,
                               dbus::ObjectPath(dbus_xdg::kPortalObjectPath)))
        .WillRepeatedly(Return(mock_proxy_.get()));

    EXPECT_CALL(*mock_bus_, GetObjectProxy(DBUS_SERVICE_DBUS,
                                           dbus::ObjectPath(DBUS_PATH_DBUS)))
        .WillRepeatedly(Return(mock_dbus_proxy_.get()));

    EXPECT_CALL(*mock_proxy_, SetNameOwnerChangedCallback(_))
        .WillRepeatedly(Return());

    // Default mock behavior for NameHasOwner:
    EXPECT_CALL(*mock_dbus_proxy_, CallMethod(_, _, _))
        .WillRepeatedly([this](dbus::MethodCall* method_call, int timeout_ms,
                               dbus::ObjectProxy::ResponseCallback callback) {
          if (method_call->GetMember() == "NameHasOwner") {
            dbus::MessageReader reader(method_call);
            std::string service_name;
            reader.PopString(&service_name);
            bool exists = service_owners_[service_name];
            auto response = dbus::Response::CreateEmpty();
            dbus::MessageWriter writer(response.get());
            writer.AppendBool(exists);
            std::move(callback).Run(response.get());
            return;
          }
          std::move(callback).Run(nullptr);
        });

    // Default mock behavior for CallMethodWithErrorResponse (Properties.Get):
    EXPECT_CALL(*mock_proxy_, CallMethodWithErrorResponse(_, _, _))
        .WillRepeatedly(
            [this](dbus::MethodCall* method_call, int timeout_ms,
                   dbus::ObjectProxy::ResponseOrErrorCallback callback) {
              if (method_call->GetInterface() == DBUS_INTERFACE_PROPERTIES &&
                  method_call->GetMember() == "Get") {
                dbus::MessageReader reader(method_call);
                std::string iface;
                std::string prop;
                if (reader.PopString(&iface) && reader.PopString(&prop)) {
                  auto key = std::make_pair(iface, prop);
                  auto it = uint32_properties_.find(key);
                  if (it != uint32_properties_.end() &&
                      it->second.has_value()) {
                    auto response = dbus::Response::CreateEmpty();
                    dbus::MessageWriter writer(response.get());
                    writer.AppendVariantOfUint32(*it->second);
                    std::move(callback).Run(response.get(), nullptr);
                    return;
                  }
                }
              }
              auto response = dbus::Response::CreateEmpty();
              std::move(callback).Run(response.get(), nullptr);
            });

    // Default mock behavior for CallMethod (e.g. FileChooser version check):
    EXPECT_CALL(*mock_proxy_, CallMethod(_, _, _))
        .WillRepeatedly([this](dbus::MethodCall* method_call, int timeout_ms,
                               dbus::ObjectProxy::ResponseCallback callback) {
          if (method_call->GetInterface() == DBUS_INTERFACE_PROPERTIES &&
              method_call->GetMember() == "Get") {
            dbus::MessageReader reader(method_call);
            std::string iface;
            std::string prop;
            if (reader.PopString(&iface) && reader.PopString(&prop)) {
              auto key = std::make_pair(iface, prop);
              auto it = uint32_properties_.find(key);
              if (it != uint32_properties_.end() && it->second.has_value()) {
                auto response = dbus::Response::CreateEmpty();
                dbus::MessageWriter writer(response.get());
                writer.AppendVariantOfUint32(*it->second);
                std::move(callback).Run(response.get());
                return;
              }
            }
          }
          std::move(callback).Run(nullptr);
        });
  }

  void TearDown() override {
    // D-Bus mock objects are reference-counted and retained by async callbacks
    // past test teardown, requiring AllowLeak to suppress GMock exit warnings.
    testing::Mock::AllowLeak(mock_proxy_.get());
    testing::Mock::AllowLeak(mock_dbus_proxy_.get());
    testing::Mock::AllowLeak(mock_bus_.get());
  }

  void SetServiceOwnerExists(std::string_view service_name, bool exists) {
    service_owners_[std::string(service_name)] = exists;
  }

  void SetPropertyUint32(std::string_view interface_name,
                         std::string_view property_name,
                         uint32_t value) {
    uint32_properties_[std::make_pair(std::string(interface_name),
                                      std::string(property_name))] = value;
  }

  void SetPropertyError(std::string_view interface_name,
                        std::string_view property_name) {
    uint32_properties_[std::make_pair(std::string(interface_name),
                                      std::string(property_name))] =
        std::nullopt;
  }

  content::BrowserTaskEnvironment task_environment_;
  scoped_refptr<dbus::MockBus> mock_bus_;
  scoped_refptr<dbus::MockObjectProxy> mock_proxy_;
  scoped_refptr<dbus::MockObjectProxy> mock_dbus_proxy_;

  std::map<std::string, bool> service_owners_;
  std::map<std::pair<std::string, std::string>, std::optional<uint32_t>>
      uint32_properties_;
};

TEST_F(DynamicLauncherPortalTest, InstallSuccess) {
  EXPECT_CALL(*mock_proxy_,
              CallMethodWithErrorResponse(
                  MethodCallIs(kDynamicLauncherInterfaceName, "Install"), _, _))
      .WillOnce(Invoke([](dbus::MethodCall* method_call, int timeout_ms,
                          dbus::ObjectProxy::ResponseOrErrorCallback callback) {
        dbus::MessageReader reader(method_call);
        std::string token, desktop_file_id, desktop_entry;
        EXPECT_TRUE(reader.PopString(&token));
        EXPECT_TRUE(reader.PopString(&desktop_file_id));
        EXPECT_TRUE(reader.PopString(&desktop_entry));
        EXPECT_EQ(token, "token_123");
        EXPECT_EQ(desktop_file_id, "app.desktop");
        EXPECT_EQ(desktop_entry, "[Desktop Entry]\nExec=app");

        dbus::MessageReader options_reader(nullptr);
        EXPECT_TRUE(reader.PopArray(&options_reader));
        EXPECT_FALSE(options_reader.HasMoreData());

        auto response = dbus::Response::CreateEmpty();
        std::move(callback).Run(response.get(), nullptr);
      }));

  base::test::TestFuture<bool> future;
  DynamicLauncherPortal portal(mock_bus_);
  portal.Install("token_123", "app.desktop", "[Desktop Entry]\nExec=app",
                 future.GetCallback());
  EXPECT_TRUE(future.Get());
}

TEST_F(DynamicLauncherPortalTest, UninstallSuccess) {
  EXPECT_CALL(
      *mock_proxy_,
      CallMethodWithErrorResponse(
          MethodCallIs(kDynamicLauncherInterfaceName, "Uninstall"), _, _))
      .WillOnce(Invoke([](dbus::MethodCall* method_call, int timeout_ms,
                          dbus::ObjectProxy::ResponseOrErrorCallback callback) {
        dbus::MessageReader reader(method_call);
        std::string desktop_file_id;
        EXPECT_TRUE(reader.PopString(&desktop_file_id));
        EXPECT_EQ(desktop_file_id, "app.desktop");

        auto response = dbus::Response::CreateEmpty();
        std::move(callback).Run(response.get(), nullptr);
      }));

  base::test::TestFuture<bool> future;
  DynamicLauncherPortal portal(mock_bus_);
  portal.Uninstall("app.desktop", future.GetCallback());
  EXPECT_TRUE(future.Get());
}

TEST_F(DynamicLauncherPortalTest, PrepareInstallEmptyIcon) {
  base::test::TestFuture<std::optional<std::string>> future;
  DynamicLauncherPortal portal(mock_bus_);
  portal.PrepareInstall("App Name", {}, GURL("https://example.com"),
                        future.GetCallback());
  EXPECT_FALSE(future.Get().has_value());
}

TEST_F(DynamicLauncherPortalTest, IsAvailableWebappSupported) {
  SetServiceOwnerExists(dbus_xdg::kPortalServiceName, true);
  SetPropertyUint32(kDynamicLauncherInterfaceName, "version", 1);
  SetPropertyUint32(kDynamicLauncherInterfaceName, "SupportedLauncherTypes", 2);
  SetPropertyUint32(dbus_xdg::kFileChooserInterfaceName, "version", 1);

  base::test::TestFuture<bool> future;
  DynamicLauncherPortal portal(mock_bus_);
  portal.IsAvailable(future.GetCallback());
  EXPECT_TRUE(future.Get());
}

TEST_F(DynamicLauncherPortalTest, IsAvailableNoWebappSupport) {
  SetServiceOwnerExists(dbus_xdg::kPortalServiceName, true);
  SetPropertyUint32(kDynamicLauncherInterfaceName, "version", 1);
  SetPropertyUint32(kDynamicLauncherInterfaceName, "SupportedLauncherTypes", 1);
  SetPropertyUint32(dbus_xdg::kFileChooserInterfaceName, "version", 1);

  base::test::TestFuture<bool> future;
  DynamicLauncherPortal portal(mock_bus_);
  portal.IsAvailable(future.GetCallback());
  EXPECT_FALSE(future.Get());
}

}  // namespace web_app
