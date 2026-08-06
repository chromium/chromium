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
#include "base/test/bind.h"
#include "base/test/task_environment.h"
#include "base/test/test_future.h"
#include "dbus/bus.h"
#include "dbus/message.h"
#include "dbus/mock_bus.h"
#include "dbus/mock_exported_object.h"
#include "dbus/object_path.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/abseil-cpp/absl/cleanup/cleanup.h"

using testing::_;

class StatusIconLinuxDbusTest : public testing::Test {
 public:
  StatusIconLinuxDbusTest() = default;
  ~StatusIconLinuxDbusTest() override = default;

  void SetUp() override {
    bus_ = base::MakeRefCounted<dbus::MockBus>(dbus::Bus::Options());
    exported_object_ = base::MakeRefCounted<dbus::MockExportedObject>(
        bus_.get(), dbus::ObjectPath("/StatusNotifierItem"));
    EXPECT_CALL(*bus_,
                GetExportedObject(dbus::ObjectPath("/StatusNotifierItem")))
        .WillRepeatedly(testing::Return(exported_object_.get()));
  }

 protected:
  base::test::SingleThreadTaskEnvironment task_environment_{
      base::test::TaskEnvironment::MainThreadType::UI};
  scoped_refptr<dbus::MockBus> bus_;
  scoped_refptr<dbus::MockExportedObject> exported_object_;
};

TEST_F(StatusIconLinuxDbusTest, InterfaceSupport) {
  std::map<std::pair<std::string, std::string>,
           dbus::ExportedObject::MethodCallCallback>
      callbacks;

  EXPECT_CALL(*exported_object_, ExportMethod(_, _, _, _))
      .WillRepeatedly(
          [&](const std::string& interface_name, const std::string& method_name,
              dbus::ExportedObject::MethodCallCallback callback,
              dbus::ExportedObject::OnExportedCallback on_exported_callback) {
            callbacks[{interface_name, method_name}] = callback;
            std::move(on_exported_callback)
                .Run(interface_name, method_name, true);
          });

  StatusIconLinuxDbus::ExportMultiplexerMethodsForTesting(bus_.get());
  absl::Cleanup cleanup = [this] {
    StatusIconLinuxDbus::UnexportMultiplexerMethodsForTesting(bus_.get());
  };

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

TEST_F(StatusIconLinuxDbusTest, PropertiesGetAllInterfaces) {
  std::map<std::pair<std::string, std::string>,
           dbus::ExportedObject::MethodCallCallback>
      callbacks;

  EXPECT_CALL(*exported_object_, ExportMethod(_, _, _, _))
      .WillRepeatedly(
          [&](const std::string& interface_name, const std::string& method_name,
              dbus::ExportedObject::MethodCallCallback callback,
              dbus::ExportedObject::OnExportedCallback on_exported_callback) {
            callbacks[{interface_name, method_name}] = callback;
            std::move(on_exported_callback)
                .Run(interface_name, method_name, true);
          });

  StatusIconLinuxDbus::ExportMultiplexerMethodsForTesting(bus_.get());
  absl::Cleanup cleanup = [this] {
    StatusIconLinuxDbus::UnexportMultiplexerMethodsForTesting(bus_.get());
  };

  auto get_all_callback =
      callbacks[{"org.freedesktop.DBus.Properties", "GetAll"}];
  ASSERT_TRUE(get_all_callback);

  // Test GetAll with org.kde.StatusNotifierItem
  {
    dbus::MethodCall method_call("org.freedesktop.DBus.Properties", "GetAll");
    method_call.SetDestination("org.freedesktop.StatusNotifierItem-123-1");
    method_call.SetSerial(1);
    dbus::MessageWriter writer(&method_call);
    writer.AppendString("org.kde.StatusNotifierItem");

    base::test::TestFuture<std::unique_ptr<dbus::Response>> future;
    get_all_callback.Run(&method_call, future.GetCallback());
    // Unregistered destination returns nullptr.
    EXPECT_EQ(future.Take(), nullptr);
  }

  // Test GetAll with org.freedesktop.StatusNotifierItem
  {
    dbus::MethodCall method_call("org.freedesktop.DBus.Properties", "GetAll");
    method_call.SetDestination("org.freedesktop.StatusNotifierItem-123-1");
    method_call.SetSerial(2);
    dbus::MessageWriter writer(&method_call);
    writer.AppendString("org.freedesktop.StatusNotifierItem");

    base::test::TestFuture<std::unique_ptr<dbus::Response>> future;
    get_all_callback.Run(&method_call, future.GetCallback());
    // Unregistered destination returns nullptr.
    EXPECT_EQ(future.Take(), nullptr);
  }

  // Test GetAll with invalid interface
  {
    dbus::MethodCall method_call("org.freedesktop.DBus.Properties", "GetAll");
    method_call.SetDestination("org.freedesktop.StatusNotifierItem-123-1");
    method_call.SetSerial(3);
    dbus::MessageWriter writer(&method_call);
    writer.AppendString("org.invalid.Interface");

    base::test::TestFuture<std::unique_ptr<dbus::Response>> future;
    get_all_callback.Run(&method_call, future.GetCallback());
    EXPECT_EQ(future.Take(), nullptr);
  }
}
