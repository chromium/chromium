// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/download/internal/background_service/scheduler/device_status_listener.h"

#include <memory>

#include "base/run_loop.h"
#include "base/test/power_monitor_test_base.h"
#include "base/test/task_environment.h"
#include "components/download/internal/background_service/scheduler/battery_status_listener_impl.h"
#include "components/download/network/network_status_listener_impl.h"
#include "services/network/test/test_network_connection_tracker.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

using testing::_;
using testing::InSequence;
using ConnectionType = network::mojom::ConnectionType;

namespace download {
namespace {

MATCHER_P(NetworkStatusEqual, value, "") {
  return arg.network_status == value;
}

MATCHER_P(BatteryStatusEqual, value, "") {
  return arg.battery_status == value;
}

class MockObserver : public DeviceStatusListener::Observer {
 public:
  MOCK_METHOD1(OnDeviceStatusChanged, void(const DeviceStatus&));
};

class TestBatteryStatusListener : public BatteryStatusListenerImpl {
 public:
  TestBatteryStatusListener() : BatteryStatusListenerImpl(base::TimeDelta()) {}
  ~TestBatteryStatusListener() override = default;

  void set_battery_percentage(int battery_percentage) {
    battery_percentage_ = battery_percentage;
  }

  // BatteryStatusListener implementation.
  int GetBatteryPercentageInternal() override { return battery_percentage_; }

 private:
  int battery_percentage_ = 0;
  DISALLOW_COPY_AND_ASSIGN(TestBatteryStatusListener);
};

// Test target that only loads default implementation of NetworkStatusListener.
class TestDeviceStatusListener : public DeviceStatusListener {
 public:
  explicit TestDeviceStatusListener(
      std::unique_ptr<TestBatteryStatusListener> battery_listener,
      std::unique_ptr<NetworkStatusListener> network_listener)
      : DeviceStatusListener(base::TimeDelta(),
                             base::TimeDelta(),
                             std::move(battery_listener),
                             std::move(network_listener)) {}

  // DeviceStatusListener implementation.
  void Start(const base::TimeDelta& start_delay) override {
    // Cache the start delay for verification.
    start_delay_ = start_delay;
    DeviceStatusListener::Start(start_delay);
  }

  base::TimeDelta start_delay() const { return start_delay_; }

 private:
  friend class DeviceStatusListenerTest;
  base::TimeDelta start_delay_;

  DISALLOW_COPY_AND_ASSIGN(TestDeviceStatusListener);
};

class DeviceStatusListenerTest : public testing::Test {
 public:
  DeviceStatusListenerTest() {}

  void SetUp() override {
    auto power_source = std::make_unique<base::PowerMonitorTestSource>();
    power_source_ = power_source.get();
    base::PowerMonitor::Initialize(std::move(power_source));

    auto battery_listener = std::make_unique<TestBatteryStatusListener>();
    test_battery_listener_ = battery_listener.get();

    auto network_listener = std::make_unique<NetworkStatusListenerImpl>(
        network::TestNetworkConnectionTracker::GetInstance());

    listener_ = std::make_unique<TestDeviceStatusListener>(
        std::move(battery_listener), std::move(network_listener));
    listener_->SetObserver(&mock_observer_);
  }

  void TearDown() override {
    listener_.reset();
    base::PowerMonitor::ShutdownForTesting();
  }

 protected:
  // Start the listener with certain network and battery state.
  void StartListener(ConnectionType type, bool on_battery_power) {
    ChangeNetworkType(type);
    SimulateBatteryChange(on_battery_power);
    base::RunLoop().RunUntilIdle();

    EXPECT_CALL(mock_observer_, OnDeviceStatusChanged(_))
        .Times(1)
        .RetiresOnSaturation();
    listener_->Start(base::TimeDelta());
    base::RunLoop().RunUntilIdle();
  }

  // Simulates a network change call, the event will be broadcasted
  // asynchronously.
  void ChangeNetworkType(ConnectionType type) {
    network::TestNetworkConnectionTracker::GetInstance()->SetConnectionType(
        type);
  }

  // Simulates a network change call, the event will be sent to client
  // immediately.
  void ChangeNetworkTypeImmediately(ConnectionType type) {
    DCHECK(listener_);
    listener_->OnNetworkChanged(type);
  }

  // Simulates a battery change call.
  void SimulateBatteryChange(bool on_battery_power) {
    power_source_->GeneratePowerStateEvent(on_battery_power);
  }

  void ChangeBatteryPercentage(int percentage) {
    DCHECK(test_battery_listener_);
    test_battery_listener_->set_battery_percentage(percentage);
  }

  std::unique_ptr<TestDeviceStatusListener> listener_;
  MockObserver mock_observer_;

  // Needed for network change notifier and power monitor.
  base::test::SingleThreadTaskEnvironment task_environment_;
  base::PowerMonitorTestSource* power_source_;
  TestBatteryStatusListener* test_battery_listener_;
};

// Verifies the initial state that the observer should be notified.
TEST_F(DeviceStatusListenerTest, InitialNoOptState) {
  ChangeNetworkType(ConnectionType::CONNECTION_NONE);
  SimulateBatteryChange(true); /* Not charging. */
  EXPECT_EQ(DeviceStatus(), listener_->CurrentDeviceStatus());

  const int kInitialBatteryPercentage = 45;
  listener_->Start(base::TimeDelta());

  ChangeBatteryPercentage(kInitialBatteryPercentage);

  // We are in no opt state, notify the observer.
  EXPECT_CALL(mock_observer_, OnDeviceStatusChanged(_)).Times(1);
  base::RunLoop().RunUntilIdle();
  const DeviceStatus& status = listener_->CurrentDeviceStatus();
  EXPECT_EQ(BatteryStatus::NOT_CHARGING, status.battery_status);
  EXPECT_EQ(kInitialBatteryPercentage, status.battery_percentage);
  EXPECT_EQ(NetworkStatus::DISCONNECTED, status.network_status);
}

// Verifies two Start() call will only do initialization for once, and the start
// delay should be refreshed based on a later Start() call.
TEST_F(DeviceStatusListenerTest, DuplicateStart) {
  ChangeNetworkType(ConnectionType::CONNECTION_NONE);
  SimulateBatteryChange(true); /* Not charging. */
  EXPECT_EQ(DeviceStatus(), listener_->CurrentDeviceStatus());
  const auto acutual_delay = base::TimeDelta::FromSeconds(0);
  listener_->Start(base::TimeDelta::FromSeconds(1));
  listener_->Start(acutual_delay);
  EXPECT_CALL(mock_observer_, OnDeviceStatusChanged(_)).Times(1);
  base::RunLoop().RunUntilIdle();
  EXPECT_EQ(listener_->start_delay(), acutual_delay);
}

TEST_F(DeviceStatusListenerTest, TestValidStateChecks) {
  ChangeNetworkType(ConnectionType::CONNECTION_NONE);
  SimulateBatteryChange(true);
  EXPECT_EQ(DeviceStatus(), listener_->CurrentDeviceStatus());

  {
    EXPECT_CALL(mock_observer_, OnDeviceStatusChanged(_)).Times(0);
    listener_->Start(base::TimeDelta());
    EXPECT_FALSE(listener_->is_valid_state());
  }

  {
    EXPECT_CALL(mock_observer_, OnDeviceStatusChanged(_)).Times(1);
    base::RunLoop().RunUntilIdle();
    EXPECT_TRUE(listener_->is_valid_state());
  }

  // Simulate a connection change directly on the DeviceStatusListener itself to
  // allow validating the state correctly here.
  ChangeNetworkTypeImmediately(ConnectionType::CONNECTION_4G);
  EXPECT_FALSE(listener_->is_valid_state());

  {
    EXPECT_CALL(mock_observer_, OnDeviceStatusChanged(_)).Times(1);
    base::RunLoop().RunUntilIdle();
    EXPECT_TRUE(listener_->is_valid_state());
  }

  {
    EXPECT_CALL(mock_observer_, OnDeviceStatusChanged(_)).Times(1);
    ChangeNetworkTypeImmediately(ConnectionType::CONNECTION_NONE);
    EXPECT_TRUE(listener_->is_valid_state());
    base::RunLoop().RunUntilIdle();
    EXPECT_TRUE(listener_->is_valid_state());
  }
}

// Ensures the observer is notified when network condition changes.
TEST_F(DeviceStatusListenerTest, NotifyObserverNetworkChange) {
  listener_->Start(base::TimeDelta());

  // Initial states check.
  EXPECT_EQ(NetworkStatus::DISCONNECTED,
            listener_->CurrentDeviceStatus().network_status);

  // Network switch between mobile networks, the observer should be notified
  // only once.
  ChangeNetworkType(ConnectionType::CONNECTION_4G);
  ChangeNetworkType(ConnectionType::CONNECTION_3G);
  ChangeNetworkType(ConnectionType::CONNECTION_2G);

  // Verifies the online signal is sent in a post task after a delay.
  EXPECT_EQ(NetworkStatus::DISCONNECTED,
            listener_->CurrentDeviceStatus().network_status);
  EXPECT_CALL(mock_observer_,
              OnDeviceStatusChanged(NetworkStatusEqual(NetworkStatus::METERED)))
      .Times(1)
      .RetiresOnSaturation();
  base::RunLoop().RunUntilIdle();
  EXPECT_EQ(NetworkStatus::METERED,
            listener_->CurrentDeviceStatus().network_status);

  // Network is switched between wifi and ethernet, the observer should be
  // notified only once.
  ChangeNetworkType(ConnectionType::CONNECTION_WIFI);
  ChangeNetworkType(ConnectionType::CONNECTION_ETHERNET);

  EXPECT_CALL(mock_observer_, OnDeviceStatusChanged(
                                  NetworkStatusEqual(NetworkStatus::UNMETERED)))
      .Times(1)
      .RetiresOnSaturation();
  base::RunLoop().RunUntilIdle();
  EXPECT_EQ(NetworkStatus::UNMETERED,
            listener_->CurrentDeviceStatus().network_status);
}

// Ensures the observer is notified when battery condition changes.
TEST_F(DeviceStatusListenerTest, NotifyObserverBatteryChange) {
  InSequence s;
  ChangeNetworkType(ConnectionType::CONNECTION_4G);
  SimulateBatteryChange(false); /* Charging. */
  EXPECT_EQ(DeviceStatus(), listener_->CurrentDeviceStatus());

  listener_->Start(base::TimeDelta());

  EXPECT_CALL(mock_observer_, OnDeviceStatusChanged(
                                  BatteryStatusEqual(BatteryStatus::CHARGING)))
      .Times(1)
      .RetiresOnSaturation();
  base::RunLoop().RunUntilIdle();
  EXPECT_EQ(BatteryStatus::CHARGING,
            listener_->CurrentDeviceStatus().battery_status);

  EXPECT_CALL(
      mock_observer_,
      OnDeviceStatusChanged(BatteryStatusEqual(BatteryStatus::NOT_CHARGING)))
      .Times(1)
      .RetiresOnSaturation();
  SimulateBatteryChange(true); /* Not charging. */
  const int kBatteryPercentage = 70;
  ChangeBatteryPercentage(kBatteryPercentage);
  base::RunLoop().RunUntilIdle();
  EXPECT_EQ(BatteryStatus::NOT_CHARGING,
            listener_->CurrentDeviceStatus().battery_status);
  EXPECT_EQ(kBatteryPercentage,
            listener_->CurrentDeviceStatus().battery_percentage);

  listener_->Stop();
}

// Verify a sequence of offline->online->offline network state changes.
TEST_F(DeviceStatusListenerTest, OfflineOnlineOffline) {
  StartListener(ConnectionType::CONNECTION_NONE, true);

  // Initial state is offline.
  EXPECT_EQ(NetworkStatus::DISCONNECTED,
            listener_->CurrentDeviceStatus().network_status);
  EXPECT_TRUE(listener_->is_valid_state());
  EXPECT_CALL(mock_observer_, OnDeviceStatusChanged(_)).Times(0);

  // Change to online.
  ChangeNetworkTypeImmediately(ConnectionType::CONNECTION_4G);
  EXPECT_FALSE(listener_->is_valid_state());
  EXPECT_EQ(NetworkStatus::DISCONNECTED,
            listener_->CurrentDeviceStatus().network_status);

  // Change to offline immediately.
  ChangeNetworkTypeImmediately(ConnectionType::CONNECTION_NONE);

  // Since the state changed back to offline before delayed online signal is
  // reported. The state becomes valid again immediately.
  EXPECT_TRUE(listener_->is_valid_state());
  EXPECT_EQ(NetworkStatus::DISCONNECTED,
            listener_->CurrentDeviceStatus().network_status);

  // No more online signal since we are already offline.
  base::RunLoop().RunUntilIdle();
  EXPECT_EQ(NetworkStatus::DISCONNECTED,
            listener_->CurrentDeviceStatus().network_status);
}

// Verify a sequence of online->offline->online network state changes.
TEST_F(DeviceStatusListenerTest, OnlineOfflineOnline) {
  StartListener(ConnectionType::CONNECTION_3G, true);

  // Initial states is online.
  EXPECT_EQ(NetworkStatus::METERED,
            listener_->CurrentDeviceStatus().network_status);
  EXPECT_TRUE(listener_->is_valid_state());

  // Change to offline. Signal is broadcasted immediately.
  EXPECT_CALL(
      mock_observer_,
      OnDeviceStatusChanged(NetworkStatusEqual(NetworkStatus::DISCONNECTED)))
      .Times(1)
      .RetiresOnSaturation();
  ChangeNetworkTypeImmediately(ConnectionType::CONNECTION_NONE);
  EXPECT_TRUE(listener_->is_valid_state());
  EXPECT_EQ(NetworkStatus::DISCONNECTED,
            listener_->CurrentDeviceStatus().network_status);

  // Change to online. Signal will be broadcasted after a delay.
  ChangeNetworkTypeImmediately(ConnectionType::CONNECTION_WIFI);
  EXPECT_FALSE(listener_->is_valid_state());
  EXPECT_EQ(NetworkStatus::DISCONNECTED,
            listener_->CurrentDeviceStatus().network_status);

  EXPECT_CALL(mock_observer_, OnDeviceStatusChanged(
                                  NetworkStatusEqual(NetworkStatus::UNMETERED)))
      .Times(1)
      .RetiresOnSaturation();
  base::RunLoop().RunUntilIdle();
  EXPECT_TRUE(listener_->is_valid_state());
  EXPECT_EQ(NetworkStatus::UNMETERED,
            listener_->CurrentDeviceStatus().network_status);
}

}  // namespace
}  // namespace download
