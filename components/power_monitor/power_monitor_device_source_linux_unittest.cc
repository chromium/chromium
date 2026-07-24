// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/power_monitor/power_monitor_device_source_linux.h"

#include <memory>
#include <utility>

#include "base/memory/scoped_refptr.h"
#include "base/power_monitor/power_monitor.h"
#include "base/power_monitor/power_monitor_source.h"
#include "base/power_monitor/power_observer.h"
#include "base/test/run_until.h"
#include "base/test/task_environment.h"
#include "components/dbus/utils/variant.h"
#include "components/dbus/xdg/portal.h"
#include "dbus/message.h"
#include "dbus/mock_bus.h"
#include "dbus/mock_object_proxy.h"
#include "dbus/object_path.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

using ::testing::_;
using ::testing::Return;
using ::testing::SaveArg;

namespace {

constexpr char kPortalServiceName[] = "org.freedesktop.portal.Desktop";
constexpr char kPortalObjectPath[] = "/org/freedesktop/portal/desktop";
constexpr char kPortalPowerProfileMonitorInterface[] =
    "org.freedesktop.portal.PowerProfileMonitor";
constexpr char kPowerSaverEnabledProperty[] = "power-saver-enabled";

constexpr char kDBusPropertiesInterface[] = "org.freedesktop.DBus.Properties";

class TestPowerSuspendObserver : public base::PowerSuspendObserver {
 public:
  TestPowerSuspendObserver() {
    base::PowerMonitor::GetInstance()->AddPowerSuspendObserver(this);
  }
  ~TestPowerSuspendObserver() override {
    base::PowerMonitor::GetInstance()->RemovePowerSuspendObserver(this);
  }

  void OnSuspend() override { suspend_count_++; }
  void OnResume() override { resume_count_++; }

  int suspend_count() const { return suspend_count_; }
  int resume_count() const { return resume_count_; }

 private:
  int suspend_count_ = 0;
  int resume_count_ = 0;
};

class PowerMonitorDeviceSourceLinuxTest : public testing::Test {
 public:
  PowerMonitorDeviceSourceLinuxTest()
      : mock_system_bus_(
            base::MakeRefCounted<dbus::MockBus>(dbus::Bus::Options())),
        mock_session_bus_(
            base::MakeRefCounted<dbus::MockBus>(dbus::Bus::Options())),
        mock_login_proxy_(base::MakeRefCounted<dbus::MockObjectProxy>(
            mock_system_bus_.get(),
            "org.freedesktop.login1",
            dbus::ObjectPath("/org/freedesktop/login1"))),
        mock_portal_proxy_(base::MakeRefCounted<dbus::MockObjectProxy>(
            mock_session_bus_.get(),
            kPortalServiceName,
            dbus::ObjectPath(kPortalObjectPath))) {
    dbus_xdg::SetPortalStateForTesting(
        dbus_xdg::PortalRegistrarState::kSuccess);

    EXPECT_CALL(*mock_system_bus_,
                GetObjectProxy("org.freedesktop.login1",
                               dbus::ObjectPath("/org/freedesktop/login1")))
        .WillRepeatedly(Return(mock_login_proxy_.get()));

    EXPECT_CALL(
        *mock_session_bus_,
        GetObjectProxy(kPortalServiceName, dbus::ObjectPath(kPortalObjectPath)))
        .WillRepeatedly(Return(mock_portal_proxy_.get()));

    EXPECT_CALL(*mock_login_proxy_,
                ConnectToSignal("org.freedesktop.login1.Manager",
                                "PrepareForSleep", _, _))
        .WillRepeatedly(SaveArg<2>(&prepare_for_sleep_callback_));

    EXPECT_CALL(*mock_portal_proxy_, ConnectToSignal(kDBusPropertiesInterface,
                                                     "PropertiesChanged", _, _))
        .WillRepeatedly(SaveArg<2>(&properties_changed_callback_));

    EXPECT_CALL(*mock_portal_proxy_, CallMethodWithErrorResponse(_, _, _))
        .WillRepeatedly(
            [this](dbus::MethodCall* call, int timeout_ms,
                   dbus::ObjectProxy::ResponseOrErrorCallback callback) {
              get_property_callback_ = std::move(callback);
            });
  }

  ~PowerMonitorDeviceSourceLinuxTest() override {
    if (base::PowerMonitor::GetInstance()->IsInitialized()) {
      base::PowerMonitor::GetInstance()->ShutdownForTesting();
    }
  }

 protected:
  void SimulateGetPropertyResponse(bool success, bool power_saver_enabled) {
    if (get_property_callback_.is_null()) {
      return;
    }
    if (success) {
      auto response = dbus::Response::CreateEmpty();
      dbus::MessageWriter writer(response.get());
      dbus_utils::Variant::Wrap<"b">(power_saver_enabled).Write(writer);
      std::move(get_property_callback_).Run(response.get(), nullptr);
    } else {
      dbus::MethodCall method_call(kDBusPropertiesInterface, "Get");
      method_call.SetSerial(1);
      auto error_response = dbus::ErrorResponse::FromMethodCall(
          &method_call, "org.freedesktop.DBus.Error.Failed", "Failed");
      std::move(get_property_callback_).Run(nullptr, error_response.get());
    }
  }

  void SimulatePropertiesChangedSignal(const std::string& interface_name,
                                       bool power_saver_enabled) {
    if (properties_changed_callback_.is_null()) {
      return;
    }
    dbus::Signal signal(kDBusPropertiesInterface, "PropertiesChanged");
    dbus::MessageWriter writer(&signal);
    writer.AppendString(interface_name);

    dbus::MessageWriter dict_writer(nullptr);
    writer.OpenArray("{sv}", &dict_writer);
    dbus::MessageWriter entry_writer(nullptr);
    dict_writer.OpenDictEntry(&entry_writer);
    entry_writer.AppendString(kPowerSaverEnabledProperty);
    dbus_utils::Variant::Wrap<"b">(power_saver_enabled).Write(entry_writer);
    dict_writer.CloseContainer(&entry_writer);
    writer.CloseContainer(&dict_writer);

    dbus::MessageWriter invalidated_writer(nullptr);
    writer.OpenArray("s", &invalidated_writer);
    writer.CloseContainer(&invalidated_writer);

    properties_changed_callback_.Run(&signal);
  }

  void SimulatePrepareForSleepSignal(bool start) {
    if (prepare_for_sleep_callback_.is_null()) {
      return;
    }
    dbus::Signal signal("org.freedesktop.login1.Manager", "PrepareForSleep");
    dbus::MessageWriter writer(&signal);
    writer.AppendBool(start);
    prepare_for_sleep_callback_.Run(&signal);
  }

  base::test::SingleThreadTaskEnvironment task_environment_;
  scoped_refptr<dbus::MockBus> mock_system_bus_;
  scoped_refptr<dbus::MockBus> mock_session_bus_;
  scoped_refptr<dbus::MockObjectProxy> mock_login_proxy_;
  scoped_refptr<dbus::MockObjectProxy> mock_portal_proxy_;

  dbus::ObjectProxy::SignalCallback prepare_for_sleep_callback_;
  dbus::ObjectProxy::SignalCallback properties_changed_callback_;
  dbus::ObjectProxy::ResponseOrErrorCallback get_property_callback_;
};

TEST_F(PowerMonitorDeviceSourceLinuxTest, InitialFetchTrue) {
  PowerMonitorDeviceSourceLinux source(mock_system_bus_, mock_session_bus_);
  SimulateGetPropertyResponse(true, true);
  EXPECT_EQ(source.GetBatteryPowerStatus(),
            base::PowerStateObserver::BatteryPowerStatus::kBatteryPower);
}

TEST_F(PowerMonitorDeviceSourceLinuxTest, InitialFetchFalse) {
  PowerMonitorDeviceSourceLinux source(mock_system_bus_, mock_session_bus_);
  SimulateGetPropertyResponse(true, false);
  EXPECT_EQ(source.GetBatteryPowerStatus(),
            base::PowerStateObserver::BatteryPowerStatus::kUnknown);
}

TEST_F(PowerMonitorDeviceSourceLinuxTest, GetPropertyErrorHandling) {
  PowerMonitorDeviceSourceLinux source(mock_system_bus_, mock_session_bus_);
  SimulateGetPropertyResponse(false, false);
  EXPECT_EQ(source.GetBatteryPowerStatus(),
            base::PowerStateObserver::BatteryPowerStatus::kUnknown);
}

TEST_F(PowerMonitorDeviceSourceLinuxTest, SignalUpdates) {
  PowerMonitorDeviceSourceLinux source(mock_system_bus_, mock_session_bus_);
  SimulateGetPropertyResponse(true, false);
  EXPECT_EQ(source.GetBatteryPowerStatus(),
            base::PowerStateObserver::BatteryPowerStatus::kUnknown);

  SimulatePropertiesChangedSignal(kPortalPowerProfileMonitorInterface, true);
  EXPECT_EQ(source.GetBatteryPowerStatus(),
            base::PowerStateObserver::BatteryPowerStatus::kBatteryPower);

  SimulatePropertiesChangedSignal(kPortalPowerProfileMonitorInterface, false);
  EXPECT_EQ(source.GetBatteryPowerStatus(),
            base::PowerStateObserver::BatteryPowerStatus::kUnknown);
}

TEST_F(PowerMonitorDeviceSourceLinuxTest, SignalOtherInterfaceIgnored) {
  PowerMonitorDeviceSourceLinux source(mock_system_bus_, mock_session_bus_);
  SimulateGetPropertyResponse(true, false);
  EXPECT_EQ(source.GetBatteryPowerStatus(),
            base::PowerStateObserver::BatteryPowerStatus::kUnknown);

  SimulatePropertiesChangedSignal("org.freedesktop.portal.OtherInterface",
                                  true);
  EXPECT_EQ(source.GetBatteryPowerStatus(),
            base::PowerStateObserver::BatteryPowerStatus::kUnknown);
}

TEST_F(PowerMonitorDeviceSourceLinuxTest, SystemdPrepareForSleep) {
  if (base::PowerMonitor::GetInstance()->IsInitialized()) {
    base::PowerMonitor::GetInstance()->ShutdownForTesting();
  }
  base::PowerMonitor::GetInstance()->Initialize(
      std::make_unique<PowerMonitorDeviceSourceLinux>(mock_system_bus_,
                                                      mock_session_bus_));
  SimulateGetPropertyResponse(true, false);

  TestPowerSuspendObserver observer;

  SimulatePrepareForSleepSignal(true);
  EXPECT_TRUE(
      base::test::RunUntil([&]() { return observer.suspend_count() == 1; }));

  SimulatePrepareForSleepSignal(false);
  EXPECT_TRUE(
      base::test::RunUntil([&]() { return observer.resume_count() == 1; }));
}

}  // namespace
