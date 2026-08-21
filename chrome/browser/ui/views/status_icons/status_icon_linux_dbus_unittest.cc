// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/views/status_icons/status_icon_linux_dbus.h"

#include <map>
#include <memory>
#include <string>
#include <utility>

#include "base/functional/callback_helpers.h"
#include "base/memory/ref_counted.h"
#include "base/memory/scoped_refptr.h"
#include "base/task/single_thread_task_runner.h"
#include "base/test/bind.h"
#include "base/test/test_future.h"
#include "content/public/test/browser_task_environment.h"
#include "dbus/bus.h"
#include "dbus/dbus-shared.h"
#include "dbus/message.h"
#include "dbus/mock_bus.h"
#include "dbus/mock_exported_object.h"
#include "dbus/mock_object_proxy.h"
#include "dbus/object_path.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "ui/gfx/image/image_skia.h"
#include "ui/linux/status_icon_linux.h"

using testing::_;

namespace {

class TestDelegate : public ui::StatusIconLinux::Delegate {
 public:
  void OnClick() override {}
  bool HasClickAction() override { return false; }
  const gfx::ImageSkia& GetImage() const override { return image_; }
  const gfx::VectorIcon* GetIcon() const override { return nullptr; }
  const std::u16string& GetToolTip() const override { return tool_tip_; }
  ui::MenuModel* GetMenuModel() const override { return nullptr; }
  void OnImplInitializationFailed() override {}

 private:
  gfx::ImageSkia image_;
  std::u16string tool_tip_;
};

void OnBusProxyCall(dbus::MethodCall* method_call,
                    int timeout_ms,
                    dbus::ObjectProxy::ResponseOrErrorCallback callback) {
  if (method_call->GetSerial() == 0) {
    method_call->SetSerial(1);
  }
  auto response = dbus::Response::FromMethodCall(method_call);
  dbus::MessageWriter writer(response.get());
  if (method_call->GetMember() == "NameHasOwner") {
    writer.AppendBool(true);
  }
  base::SingleThreadTaskRunner::GetCurrentDefault()->PostTask(
      FROM_HERE, base::BindOnce(
                     [](dbus::ObjectProxy::ResponseOrErrorCallback callback,
                        std::unique_ptr<dbus::Response> response) {
                       std::move(callback).Run(response.get(), nullptr);
                     },
                     std::move(callback), std::move(response)));
}

void OnWatcherProxyCall(std::string* registered_service_out,
                        dbus::MethodCall* method_call,
                        int timeout_ms,
                        dbus::ObjectProxy::ResponseOrErrorCallback callback) {
  if (method_call->GetSerial() == 0) {
    method_call->SetSerial(1);
  }
  auto response = dbus::Response::FromMethodCall(method_call);
  dbus::MessageWriter writer(response.get());
  if (method_call->GetMember() == "Get") {
    dbus::MessageWriter variant_writer(nullptr);
    writer.OpenVariant("b", &variant_writer);
    variant_writer.AppendBool(true);
    writer.CloseContainer(&variant_writer);
  } else if (method_call->GetMember() == "RegisterStatusNotifierItem") {
    if (registered_service_out) {
      dbus::MessageReader reader(method_call);
      reader.PopString(registered_service_out);
    }
  }
  base::SingleThreadTaskRunner::GetCurrentDefault()->PostTask(
      FROM_HERE, base::BindOnce(
                     [](dbus::ObjectProxy::ResponseOrErrorCallback callback,
                        std::unique_ptr<dbus::Response> response) {
                       std::move(callback).Run(response.get(), nullptr);
                     },
                     std::move(callback), std::move(response)));
}

}  // namespace

class StatusIconLinuxDbusTest : public testing::Test {
 public:
  StatusIconLinuxDbusTest() = default;
  ~StatusIconLinuxDbusTest() override = default;

  void SetUp() override {
    bus_ = base::MakeRefCounted<dbus::MockBus>(dbus::Bus::Options());
    bus_proxy_ = base::MakeRefCounted<dbus::MockObjectProxy>(
        bus_.get(), DBUS_SERVICE_DBUS, dbus::ObjectPath(DBUS_PATH_DBUS));
    watcher_proxy_ = base::MakeRefCounted<dbus::MockObjectProxy>(
        bus_.get(), "org.kde.StatusNotifierWatcher",
        dbus::ObjectPath("/StatusNotifierWatcher"));
    exported_item_object_ = base::MakeRefCounted<dbus::MockExportedObject>(
        bus_.get(), dbus::ObjectPath("/StatusNotifierItem"));
    exported_menu_object_ = base::MakeRefCounted<dbus::MockExportedObject>(
        bus_.get(), dbus::ObjectPath("/org/chromium/DbusMenu"));

    EXPECT_CALL(*bus_, GetDBusTaskRunner())
        .WillRepeatedly(testing::Return(
            base::SingleThreadTaskRunner::GetCurrentDefault().get()));

    EXPECT_CALL(*bus_, ShutdownAndBlock()).WillRepeatedly(testing::Return());

    EXPECT_CALL(*bus_, GetObjectProxy(_, _))
        .WillRepeatedly([this](std::string_view service_name,
                               const dbus::ObjectPath& object_path) {
          if (service_name == "org.kde.StatusNotifierWatcher") {
            return watcher_proxy_.get();
          }
          return bus_proxy_.get();
        });

    EXPECT_CALL(*bus_, GetExportedObject(_))
        .WillRepeatedly([this](const dbus::ObjectPath& object_path) {
          if (object_path.value().starts_with("/org/chromium/DbusMenu")) {
            return exported_menu_object_.get();
          }
          return exported_item_object_.get();
        });

    EXPECT_CALL(*exported_menu_object_, ExportMethod(_, _, _, _))
        .WillRepeatedly(
            [](const std::string& interface_name,
               const std::string& method_name,
               dbus::ExportedObject::MethodCallCallback callback,
               dbus::ExportedObject::OnExportedCallback on_exported_callback) {
              std::move(on_exported_callback)
                  .Run(interface_name, method_name, true);
            });

    EXPECT_CALL(*bus_proxy_, CallMethodWithErrorResponse(_, _, _))
        .WillRepeatedly(&OnBusProxyCall);

    EXPECT_CALL(*watcher_proxy_, CallMethodWithErrorResponse(_, _, _))
        .WillRepeatedly(
            [](dbus::MethodCall* method_call, int timeout_ms,
               dbus::ObjectProxy::ResponseOrErrorCallback callback) {
              OnWatcherProxyCall(nullptr, method_call, timeout_ms,
                                 std::move(callback));
            });

    EXPECT_CALL(*bus_, RequestOwnership(_, _, _))
        .WillRepeatedly([](const std::string& service_name,
                           dbus::Bus::ServiceOwnershipOptions options,
                           dbus::Bus::OnOwnershipCallback callback) {
          base::SingleThreadTaskRunner::GetCurrentDefault()->PostTask(
              FROM_HERE,
              base::BindOnce(std::move(callback), service_name, true));
        });
  }

 protected:
  content::BrowserTaskEnvironment task_environment_;
  scoped_refptr<dbus::MockBus> bus_;
  scoped_refptr<dbus::MockObjectProxy> bus_proxy_;
  scoped_refptr<dbus::MockObjectProxy> watcher_proxy_;
  scoped_refptr<dbus::MockExportedObject> exported_item_object_;
  scoped_refptr<dbus::MockExportedObject> exported_menu_object_;
};

TEST_F(StatusIconLinuxDbusTest, InterfaceSupport) {
  std::map<std::pair<std::string, std::string>,
           dbus::ExportedObject::MethodCallCallback>
      callbacks;

  EXPECT_CALL(*exported_item_object_, ExportMethod(_, _, _, _))
      .WillRepeatedly(
          [&](const std::string& interface_name, const std::string& method_name,
              dbus::ExportedObject::MethodCallCallback callback,
              dbus::ExportedObject::OnExportedCallback on_exported_callback) {
            callbacks[{interface_name, method_name}] = callback;
            std::move(on_exported_callback)
                .Run(interface_name, method_name, true);
          });

  TestDelegate delegate;
  auto status_icon = base::MakeRefCounted<StatusIconLinuxDbus>(bus_);
  status_icon->SetDelegate(&delegate);

  task_environment_.RunUntilIdle();

  // Check that methods are exported for both org.kde and org.freedesktop.
  EXPECT_TRUE(callbacks.contains({"org.kde.StatusNotifierItem", "Activate"}));
  EXPECT_TRUE(
      callbacks.contains({"org.freedesktop.StatusNotifierItem", "Activate"}));
  EXPECT_TRUE(
      callbacks.contains({"org.kde.StatusNotifierItem", "ContextMenu"}));
  EXPECT_TRUE(callbacks.contains(
      {"org.freedesktop.StatusNotifierItem", "ContextMenu"}));
  EXPECT_TRUE(callbacks.contains({"org.kde.StatusNotifierItem", "Scroll"}));
  EXPECT_TRUE(
      callbacks.contains({"org.freedesktop.StatusNotifierItem", "Scroll"}));
  EXPECT_TRUE(
      callbacks.contains({"org.kde.StatusNotifierItem", "SecondaryActivate"}));
  EXPECT_TRUE(callbacks.contains(
      {"org.freedesktop.StatusNotifierItem", "SecondaryActivate"}));
  EXPECT_TRUE(callbacks.contains({"org.freedesktop.DBus.Properties", "Get"}));
  EXPECT_TRUE(
      callbacks.contains({"org.freedesktop.DBus.Properties", "GetAll"}));
}

TEST_F(StatusIconLinuxDbusTest, PropertiesGetAll) {
  std::map<std::pair<std::string, std::string>,
           dbus::ExportedObject::MethodCallCallback>
      callbacks;

  EXPECT_CALL(*exported_item_object_, ExportMethod(_, _, _, _))
      .WillRepeatedly(
          [&](const std::string& interface_name, const std::string& method_name,
              dbus::ExportedObject::MethodCallCallback callback,
              dbus::ExportedObject::OnExportedCallback on_exported_callback) {
            callbacks[{interface_name, method_name}] = callback;
            std::move(on_exported_callback)
                .Run(interface_name, method_name, true);
          });

  TestDelegate delegate;
  auto status_icon = base::MakeRefCounted<StatusIconLinuxDbus>(bus_);
  status_icon->SetDelegate(&delegate);

  task_environment_.RunUntilIdle();

  auto get_all_callback =
      callbacks[{"org.freedesktop.DBus.Properties", "GetAll"}];
  ASSERT_TRUE(get_all_callback);

  // Test GetAll with org.kde.StatusNotifierItem
  {
    dbus::MethodCall method_call("org.freedesktop.DBus.Properties", "GetAll");
    method_call.SetSerial(1);
    dbus::MessageWriter writer(&method_call);
    writer.AppendString("org.kde.StatusNotifierItem");

    base::test::TestFuture<std::unique_ptr<dbus::Response>> future;
    get_all_callback.Run(&method_call, future.GetCallback());
    EXPECT_NE(future.Take(), nullptr);
  }

  // Test GetAll with org.freedesktop.StatusNotifierItem
  {
    dbus::MethodCall method_call("org.freedesktop.DBus.Properties", "GetAll");
    method_call.SetSerial(2);
    dbus::MessageWriter writer(&method_call);
    writer.AppendString("org.freedesktop.StatusNotifierItem");

    base::test::TestFuture<std::unique_ptr<dbus::Response>> future;
    get_all_callback.Run(&method_call, future.GetCallback());
    EXPECT_NE(future.Take(), nullptr);
  }

  // Test GetAll with empty interface string
  {
    dbus::MethodCall method_call("org.freedesktop.DBus.Properties", "GetAll");
    method_call.SetSerial(3);
    dbus::MessageWriter writer(&method_call);
    writer.AppendString("");

    base::test::TestFuture<std::unique_ptr<dbus::Response>> future;
    get_all_callback.Run(&method_call, future.GetCallback());
    EXPECT_NE(future.Take(), nullptr);
  }

  // Test GetAll with invalid interface
  {
    dbus::MethodCall method_call("org.freedesktop.DBus.Properties", "GetAll");
    method_call.SetSerial(4);
    dbus::MessageWriter writer(&method_call);
    writer.AppendString("org.invalid.Interface");

    base::test::TestFuture<std::unique_ptr<dbus::Response>> future;
    get_all_callback.Run(&method_call, future.GetCallback());
    EXPECT_EQ(future.Take(), nullptr);
  }
}

TEST_F(StatusIconLinuxDbusTest, RegisterStatusNotifierItem) {
  std::string registered_service;

  EXPECT_CALL(*exported_item_object_, ExportMethod(_, _, _, _))
      .WillRepeatedly(
          [](const std::string& interface_name, const std::string& method_name,
             dbus::ExportedObject::MethodCallCallback callback,
             dbus::ExportedObject::OnExportedCallback on_exported_callback) {
            std::move(on_exported_callback)
                .Run(interface_name, method_name, true);
          });

  EXPECT_CALL(*watcher_proxy_, CallMethodWithErrorResponse(_, _, _))
      .WillRepeatedly([&](dbus::MethodCall* method_call, int timeout_ms,
                          dbus::ObjectProxy::ResponseOrErrorCallback callback) {
        OnWatcherProxyCall(&registered_service, method_call, timeout_ms,
                           std::move(callback));
      });

  TestDelegate delegate;
  auto status_icon = base::MakeRefCounted<StatusIconLinuxDbus>(bus_);
  status_icon->SetDelegate(&delegate);

  task_environment_.RunUntilIdle();

  // The service string passed to RegisterStatusNotifierItem should be
  // the well-known service name without an object path.
  EXPECT_THAT(registered_service,
              testing::StartsWith("org.freedesktop.StatusNotifierItem-"));
  EXPECT_THAT(registered_service, testing::Not(testing::HasSubstr("/")));
}

TEST_F(StatusIconLinuxDbusTest, MultipleStatusIcons) {
  auto bus2 = base::MakeRefCounted<dbus::MockBus>(dbus::Bus::Options());
  auto bus_proxy2 = base::MakeRefCounted<dbus::MockObjectProxy>(
      bus2.get(), DBUS_SERVICE_DBUS, dbus::ObjectPath(DBUS_PATH_DBUS));
  auto watcher_proxy2 = base::MakeRefCounted<dbus::MockObjectProxy>(
      bus2.get(), "org.kde.StatusNotifierWatcher",
      dbus::ObjectPath("/StatusNotifierWatcher"));
  auto exported_item_object2 = base::MakeRefCounted<dbus::MockExportedObject>(
      bus2.get(), dbus::ObjectPath("/StatusNotifierItem"));
  auto exported_menu_object2 = base::MakeRefCounted<dbus::MockExportedObject>(
      bus2.get(), dbus::ObjectPath("/org/chromium/DbusMenu"));

  EXPECT_CALL(*bus2, GetDBusTaskRunner())
      .WillRepeatedly(testing::Return(
          base::SingleThreadTaskRunner::GetCurrentDefault().get()));
  EXPECT_CALL(*bus2, ShutdownAndBlock()).WillRepeatedly(testing::Return());
  EXPECT_CALL(*bus2, GetObjectProxy(_, _))
      .WillRepeatedly([&](std::string_view service_name,
                          const dbus::ObjectPath& object_path) {
        if (service_name == "org.kde.StatusNotifierWatcher") {
          return watcher_proxy2.get();
        }
        return bus_proxy2.get();
      });
  EXPECT_CALL(*bus2, GetExportedObject(_))
      .WillRepeatedly([&](const dbus::ObjectPath& object_path) {
        if (object_path.value().starts_with("/org/chromium/DbusMenu")) {
          return exported_menu_object2.get();
        }
        return exported_item_object2.get();
      });
  EXPECT_CALL(*exported_menu_object2, ExportMethod(_, _, _, _))
      .WillRepeatedly(
          [](const std::string& interface_name, const std::string& method_name,
             dbus::ExportedObject::MethodCallCallback callback,
             dbus::ExportedObject::OnExportedCallback on_exported_callback) {
            std::move(on_exported_callback)
                .Run(interface_name, method_name, true);
          });
  EXPECT_CALL(*bus_proxy2, CallMethodWithErrorResponse(_, _, _))
      .WillRepeatedly(&OnBusProxyCall);
  EXPECT_CALL(*bus2, RequestOwnership(_, _, _))
      .WillRepeatedly([](const std::string& service_name,
                         dbus::Bus::ServiceOwnershipOptions options,
                         dbus::Bus::OnOwnershipCallback callback) {
        base::SingleThreadTaskRunner::GetCurrentDefault()->PostTask(
            FROM_HERE, base::BindOnce(std::move(callback), service_name, true));
      });

  std::string registered_service1;
  std::string registered_service2;

  EXPECT_CALL(*exported_item_object_, ExportMethod(_, _, _, _))
      .WillRepeatedly(
          [](const std::string& interface_name, const std::string& method_name,
             dbus::ExportedObject::MethodCallCallback callback,
             dbus::ExportedObject::OnExportedCallback on_exported_callback) {
            std::move(on_exported_callback)
                .Run(interface_name, method_name, true);
          });
  EXPECT_CALL(*exported_item_object2, ExportMethod(_, _, _, _))
      .WillRepeatedly(
          [](const std::string& interface_name, const std::string& method_name,
             dbus::ExportedObject::MethodCallCallback callback,
             dbus::ExportedObject::OnExportedCallback on_exported_callback) {
            std::move(on_exported_callback)
                .Run(interface_name, method_name, true);
          });

  EXPECT_CALL(*watcher_proxy_, CallMethodWithErrorResponse(_, _, _))
      .WillRepeatedly([&](dbus::MethodCall* method_call, int timeout_ms,
                          dbus::ObjectProxy::ResponseOrErrorCallback callback) {
        OnWatcherProxyCall(&registered_service1, method_call, timeout_ms,
                           std::move(callback));
      });
  EXPECT_CALL(*watcher_proxy2, CallMethodWithErrorResponse(_, _, _))
      .WillRepeatedly([&](dbus::MethodCall* method_call, int timeout_ms,
                          dbus::ObjectProxy::ResponseOrErrorCallback callback) {
        OnWatcherProxyCall(&registered_service2, method_call, timeout_ms,
                           std::move(callback));
      });

  TestDelegate delegate1;
  auto status_icon1 = base::MakeRefCounted<StatusIconLinuxDbus>(bus_);
  status_icon1->SetDelegate(&delegate1);

  TestDelegate delegate2;
  auto status_icon2 = base::MakeRefCounted<StatusIconLinuxDbus>(bus2);
  status_icon2->SetDelegate(&delegate2);

  task_environment_.RunUntilIdle();

  EXPECT_THAT(registered_service1,
              testing::StartsWith("org.freedesktop.StatusNotifierItem-"));
  EXPECT_THAT(registered_service2,
              testing::StartsWith("org.freedesktop.StatusNotifierItem-"));
  EXPECT_NE(registered_service1, registered_service2);
}
