// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chromeos/ash/services/bluetooth_config/adapter_state_controller_impl.h"

#include <memory>
#include <optional>
#include <vector>

#include "base/containers/queue.h"
#include "base/functional/callback.h"
#include "base/test/metrics/histogram_tester.h"
#include "base/test/task_environment.h"
#include "device/bluetooth/test/mock_bluetooth_adapter.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace ash::bluetooth_config {

namespace {

class FakeObserver : public AdapterStateController::Observer {
 public:
  FakeObserver() = default;
  ~FakeObserver() override = default;

  size_t num_calls() const { return num_calls_; }

 private:
  // AdapterStateController::Observer:
  void OnAdapterStateChanged() override { ++num_calls_; }

  size_t num_calls_ = 0u;
};

}  // namespace

class AdapterStateControllerImplTest
    : public testing::Test,
      public testing::WithParamInterface<bool> {
 protected:
  AdapterStateControllerImplTest() = default;
  AdapterStateControllerImplTest(const AdapterStateControllerImplTest&) =
      delete;
  AdapterStateControllerImplTest& operator=(
      const AdapterStateControllerImplTest&) = delete;
  ~AdapterStateControllerImplTest() override = default;

  // testing::Test:
  void SetUp() override {
    mock_adapter_ =
        base::MakeRefCounted<testing::NiceMock<device::MockBluetoothAdapter>>();
    ON_CALL(*mock_adapter_, IsPresent())
        .WillByDefault(
            testing::Invoke([this]() { return is_adapter_present_; }));
    ON_CALL(*mock_adapter_, IsPowered())
        .WillByDefault(
            testing::Invoke([this]() { return is_adapter_powered_; }));
    ON_CALL(*mock_adapter_, SetPowered(testing::_, testing::_, testing::_))
        .WillByDefault(testing::Invoke(
            [this](bool powered, base::OnceClosure success_callback,
                   base::OnceClosure error_callback) {
              EXPECT_FALSE(pending_power_state_.has_value());
              pending_power_state_ = powered;
              set_powered_success_callbacks_.emplace(
                  std::move(success_callback));
              set_powered_error_callbacks_.emplace(std::move(error_callback));
            }));

    adapter_state_controller_ =
        std::make_unique<AdapterStateControllerImpl>(mock_adapter_);
    adapter_state_controller_->AddObserver(&fake_observer_);
  }

  void TearDown() override {
    adapter_state_controller_->RemoveObserver(&fake_observer_);
  }

  void SetAdapterPresentState(bool present) {
    if (is_adapter_present_ == present)
      return;

    is_adapter_present_ = present;

    AdapterStateControllerImpl* impl = static_cast<AdapterStateControllerImpl*>(
        adapter_state_controller_.get());
    impl->AdapterPresentChanged(mock_adapter_.get(), present);
  }

  void SetAdapterPoweredState(bool powered) {
    if (is_adapter_powered_ == powered)
      return;

    is_adapter_powered_ = powered;

    AdapterStateControllerImpl* impl = static_cast<AdapterStateControllerImpl*>(
        adapter_state_controller_.get());
    impl->AdapterPoweredChanged(mock_adapter_.get(), powered);
  }

  mojom::BluetoothSystemState GetAdapterState() const {
    return adapter_state_controller_->GetAdapterState();
  }

  void SetBluetoothEnabledState(bool enabled) {
    adapter_state_controller_->SetBluetoothEnabledState(enabled);
  }

  void InvokeSetPoweredCallback(bool expected_pending_state, bool success) {
    EXPECT_EQ(expected_pending_state, *pending_power_state_);
    pending_power_state_.reset();

    base::OnceClosure success_callback =
        std::move(set_powered_success_callbacks_.front());
    set_powered_success_callbacks_.pop();
    base::OnceClosure error_callback =
        std::move(set_powered_error_callbacks_.front());
    set_powered_error_callbacks_.pop();

    if (success) {
      // Depending on if BlueZ or Floss is being used, SetAdapterPoweredState()
      // gets called before or after the success callback. Add coverage to test
      // both cases (b/274973520).
      bool is_floss_enabled = GetParam();
      if (!is_floss_enabled) {
        SetAdapterPoweredState(expected_pending_state);
      }

      std::move(success_callback).Run();
      error_callback.Reset();

      if (is_floss_enabled) {
        SetAdapterPoweredState(expected_pending_state);
      }
      return;
    }

    std::move(error_callback).Run();
    success_callback.Reset();
  }

  size_t GetNumObserverEvents() const { return fake_observer_.num_calls(); }

  base::HistogramTester histogram_tester;

 private:
  base::test::TaskEnvironment task_environment_;

  bool is_adapter_present_ = true;
  bool is_adapter_powered_ = true;

  std::optional<bool> pending_power_state_;
  base::queue<base::OnceClosure> set_powered_success_callbacks_;
  base::queue<base::OnceClosure> set_powered_error_callbacks_;

  scoped_refptr<testing::NiceMock<device::MockBluetoothAdapter>> mock_adapter_;
  FakeObserver fake_observer_;

  std::unique_ptr<AdapterStateController> adapter_state_controller_;
};

// Boolean parameter indicating whether to simulate Floss
// (floss::features::IsFlossEnabled()) adapter behavior or not.
INSTANTIATE_TEST_SUITE_P(All, AdapterStateControllerImplTest, testing::Bool());

TEST_P(AdapterStateControllerImplTest, StateChangesFromOutsideClass) {
  EXPECT_EQ(mojom::BluetoothSystemState::kEnabled, GetAdapterState());

  SetAdapterPoweredState(/*powered=*/false);
  EXPECT_EQ(mojom::BluetoothSystemState::kDisabled, GetAdapterState());
  EXPECT_EQ(1u, GetNumObserverEvents());

  SetAdapterPresentState(/*present=*/false);
  EXPECT_EQ(mojom::BluetoothSystemState::kUnavailable, GetAdapterState());
  EXPECT_EQ(2u, GetNumObserverEvents());
}

TEST_P(AdapterStateControllerImplTest, SetBluetoothEnabledState) {
  EXPECT_EQ(mojom::BluetoothSystemState::kEnabled, GetAdapterState());

  histogram_tester.ExpectBucketCount("Bluetooth.ChromeOS.PoweredState", false,
                                     0);
  histogram_tester.ExpectBucketCount("Bluetooth.ChromeOS.PoweredState", true,
                                     0);

  SetBluetoothEnabledState(/*enabled=*/false);
  EXPECT_EQ(mojom::BluetoothSystemState::kDisabling, GetAdapterState());
  EXPECT_EQ(1u, GetNumObserverEvents());

  histogram_tester.ExpectBucketCount("Bluetooth.ChromeOS.PoweredState", false,
                                     1);
  histogram_tester.ExpectBucketCount("Bluetooth.ChromeOS.PoweredState", true,
                                     0);

  InvokeSetPoweredCallback(/*expected_pending_state=*/false,
                           /*success=*/true);
  EXPECT_EQ(mojom::BluetoothSystemState::kDisabled, GetAdapterState());
  EXPECT_EQ(2u, GetNumObserverEvents());
  histogram_tester.ExpectBucketCount(
      "Bluetooth.ChromeOS.PoweredState.Disable.Result", true, 1);
  histogram_tester.ExpectBucketCount(
      "Bluetooth.ChromeOS.PoweredState.Enable.Result", true, 0);

  SetBluetoothEnabledState(/*enabled=*/true);
  EXPECT_EQ(mojom::BluetoothSystemState::kEnabling, GetAdapterState());
  EXPECT_EQ(3u, GetNumObserverEvents());

  histogram_tester.ExpectBucketCount("Bluetooth.ChromeOS.PoweredState", false,
                                     1);
  histogram_tester.ExpectBucketCount("Bluetooth.ChromeOS.PoweredState", true,
                                     1);

  InvokeSetPoweredCallback(/*expected_pending_state=*/true,
                           /*success=*/true);
  EXPECT_EQ(mojom::BluetoothSystemState::kEnabled, GetAdapterState());
  EXPECT_EQ(4u, GetNumObserverEvents());
  histogram_tester.ExpectBucketCount(
      "Bluetooth.ChromeOS.PoweredState.Disable.Result", true, 1);
  histogram_tester.ExpectBucketCount(
      "Bluetooth.ChromeOS.PoweredState.Enable.Result", true, 1);
}

TEST_P(AdapterStateControllerImplTest, SetBluetoothEnabledState_Error) {
  EXPECT_EQ(mojom::BluetoothSystemState::kEnabled, GetAdapterState());

  histogram_tester.ExpectBucketCount("Bluetooth.ChromeOS.PoweredState", false,
                                     0);
  histogram_tester.ExpectBucketCount("Bluetooth.ChromeOS.PoweredState", true,
                                     0);

  SetBluetoothEnabledState(/*enabled=*/false);
  EXPECT_EQ(mojom::BluetoothSystemState::kDisabling, GetAdapterState());
  EXPECT_EQ(1u, GetNumObserverEvents());

  histogram_tester.ExpectBucketCount("Bluetooth.ChromeOS.PoweredState", false,
                                     1);
  histogram_tester.ExpectBucketCount("Bluetooth.ChromeOS.PoweredState", true,
                                     0);

  InvokeSetPoweredCallback(/*expected_pending_state=*/false,
                           /*success=*/false);
  EXPECT_EQ(mojom::BluetoothSystemState::kEnabled, GetAdapterState());
  EXPECT_EQ(2u, GetNumObserverEvents());
  histogram_tester.ExpectBucketCount(
      "Bluetooth.ChromeOS.PoweredState.Disable.Result", false, 1);
  histogram_tester.ExpectBucketCount(
      "Bluetooth.ChromeOS.PoweredState.Enable.Result", true, 0);
}

TEST_P(AdapterStateControllerImplTest, MultiplePowerChanges_SameChange) {
  EXPECT_EQ(mojom::BluetoothSystemState::kEnabled, GetAdapterState());

  histogram_tester.ExpectBucketCount("Bluetooth.ChromeOS.PoweredState", false,
                                     0);
  histogram_tester.ExpectBucketCount("Bluetooth.ChromeOS.PoweredState", true,
                                     0);

  // Start disabling once.
  SetBluetoothEnabledState(/*enabled=*/false);
  EXPECT_EQ(mojom::BluetoothSystemState::kDisabling, GetAdapterState());
  EXPECT_EQ(1u, GetNumObserverEvents());

  histogram_tester.ExpectBucketCount("Bluetooth.ChromeOS.PoweredState", false,
                                     1);
  histogram_tester.ExpectBucketCount("Bluetooth.ChromeOS.PoweredState", true,
                                     0);

  // Try disabling again, even though this is already in progress.
  SetBluetoothEnabledState(/*enabled=*/false);
  EXPECT_EQ(mojom::BluetoothSystemState::kDisabling, GetAdapterState());
  EXPECT_EQ(1u, GetNumObserverEvents());

  histogram_tester.ExpectBucketCount("Bluetooth.ChromeOS.PoweredState", false,
                                     1);
  histogram_tester.ExpectBucketCount("Bluetooth.ChromeOS.PoweredState", true,
                                     0);

  InvokeSetPoweredCallback(/*expected_pending_state=*/false,
                           /*success=*/true);
  EXPECT_EQ(mojom::BluetoothSystemState::kDisabled, GetAdapterState());
  EXPECT_EQ(2u, GetNumObserverEvents());
  histogram_tester.ExpectBucketCount(
      "Bluetooth.ChromeOS.PoweredState.Disable.Result", true, 1);
  histogram_tester.ExpectBucketCount(
      "Bluetooth.ChromeOS.PoweredState.Enable.Result", true, 0);

  // Simulate adapter being enabled by system. There should be no leftover
  // queued disable requests (Regression test for b/269082911).
  SetAdapterPoweredState(/*powered=*/true);
  EXPECT_EQ(mojom::BluetoothSystemState::kEnabled, GetAdapterState());
  EXPECT_EQ(3u, GetNumObserverEvents());
  histogram_tester.ExpectBucketCount(
      "Bluetooth.ChromeOS.PoweredState.Disable.Result", true, 1);
  histogram_tester.ExpectBucketCount(
      "Bluetooth.ChromeOS.PoweredState.Enable.Result", true, 0);
}

TEST_P(AdapterStateControllerImplTest, MultiplePowerChanges_DifferentChange) {
  EXPECT_EQ(mojom::BluetoothSystemState::kEnabled, GetAdapterState());

  histogram_tester.ExpectBucketCount("Bluetooth.ChromeOS.PoweredState", false,
                                     0);
  histogram_tester.ExpectBucketCount("Bluetooth.ChromeOS.PoweredState", true,
                                     0);

  // Start disabling.
  SetBluetoothEnabledState(/*enabled=*/false);
  EXPECT_EQ(mojom::BluetoothSystemState::kDisabling, GetAdapterState());
  EXPECT_EQ(1u, GetNumObserverEvents());

  histogram_tester.ExpectBucketCount("Bluetooth.ChromeOS.PoweredState", false,
                                     1);
  histogram_tester.ExpectBucketCount("Bluetooth.ChromeOS.PoweredState", true,
                                     0);

  // Before the disable request finishes, start enabling; this simulates a user
  // very quickly clicking the on/off toggle.
  SetBluetoothEnabledState(/*enabled=*/true);
  EXPECT_EQ(mojom::BluetoothSystemState::kDisabling, GetAdapterState());
  EXPECT_EQ(1u, GetNumObserverEvents());
  histogram_tester.ExpectBucketCount("Bluetooth.ChromeOS.PoweredState", false,
                                     1);
  histogram_tester.ExpectBucketCount("Bluetooth.ChromeOS.PoweredState", true,
                                     0);

  // Invoke the first callback; because there was a queued request, we should
  // now be enabling.
  InvokeSetPoweredCallback(/*expected_pending_state=*/false,
                           /*success=*/true);
  EXPECT_EQ(mojom::BluetoothSystemState::kEnabling, GetAdapterState());
  EXPECT_EQ(3u, GetNumObserverEvents());
  histogram_tester.ExpectBucketCount(
      "Bluetooth.ChromeOS.PoweredState.Disable.Result", true, 1);
  histogram_tester.ExpectBucketCount(
      "Bluetooth.ChromeOS.PoweredState.Enable.Result", true, 0);

  // Invoke the second request; we should now be enabled.
  InvokeSetPoweredCallback(/*expected_pending_state=*/true,
                           /*success=*/true);
  EXPECT_EQ(mojom::BluetoothSystemState::kEnabled, GetAdapterState());
  EXPECT_EQ(4u, GetNumObserverEvents());
  histogram_tester.ExpectBucketCount(
      "Bluetooth.ChromeOS.PoweredState.Disable.Result", true, 1);
  histogram_tester.ExpectBucketCount(
      "Bluetooth.ChromeOS.PoweredState.Enable.Result", true, 1);

  histogram_tester.ExpectBucketCount("Bluetooth.ChromeOS.PoweredState", false,
                                     1);
  histogram_tester.ExpectBucketCount("Bluetooth.ChromeOS.PoweredState", true,
                                     1);
}

// Regression test for b/219596007.
TEST_P(AdapterStateControllerImplTest,
       MultiplePowerChanges_AdapterChangesAvailability) {
  EXPECT_EQ(mojom::BluetoothSystemState::kEnabled, GetAdapterState());

  histogram_tester.ExpectBucketCount("Bluetooth.ChromeOS.PoweredState", false,
                                     0);
  histogram_tester.ExpectBucketCount("Bluetooth.ChromeOS.PoweredState", true,
                                     0);

  // Start with the adapter off.
  SetAdapterPoweredState(/*powered=*/false);
  EXPECT_EQ(mojom::BluetoothSystemState::kDisabled, GetAdapterState());
  EXPECT_EQ(1u, GetNumObserverEvents());

  // Initiate an enable state change.
  SetBluetoothEnabledState(/*enabled=*/true);
  EXPECT_EQ(mojom::BluetoothSystemState::kEnabling, GetAdapterState());
  EXPECT_EQ(2u, GetNumObserverEvents());

  histogram_tester.ExpectBucketCount("Bluetooth.ChromeOS.PoweredState", false,
                                     0);
  histogram_tester.ExpectBucketCount("Bluetooth.ChromeOS.PoweredState", true,
                                     1);

  // Queue a disable state change.
  SetBluetoothEnabledState(/*enabled=*/false);
  EXPECT_EQ(mojom::BluetoothSystemState::kEnabling, GetAdapterState());
  EXPECT_EQ(2u, GetNumObserverEvents());

  histogram_tester.ExpectBucketCount("Bluetooth.ChromeOS.PoweredState", false,
                                     0);
  histogram_tester.ExpectBucketCount("Bluetooth.ChromeOS.PoweredState", true,
                                     1);

  // Set the adapter to not present.
  SetAdapterPresentState(/*present=*/false);
  EXPECT_EQ(mojom::BluetoothSystemState::kUnavailable, GetAdapterState());
  EXPECT_EQ(3u, GetNumObserverEvents());

  // Set the adapter back to present. The in-progress enable state change should
  // have been cleared.
  SetAdapterPresentState(/*present=*/true);
  EXPECT_EQ(mojom::BluetoothSystemState::kDisabled, GetAdapterState());
  EXPECT_EQ(4u, GetNumObserverEvents());

  // Finish the first enable state change callback. This should have no effect
  // because the weak pointer has been invalidated.
  InvokeSetPoweredCallback(/*expected_pending_state=*/true,
                           /*success=*/false);
  EXPECT_EQ(mojom::BluetoothSystemState::kDisabled, GetAdapterState());
  EXPECT_EQ(4u, GetNumObserverEvents());

  histogram_tester.ExpectBucketCount(
      "Bluetooth.ChromeOS.PoweredState.Disable.Result", false, 0);
  histogram_tester.ExpectBucketCount(
      "Bluetooth.ChromeOS.PoweredState.Enable.Result", false, 0);

  // Set the adapter to enabled. This will trigger processing any queued state
  // changes, if they exist. There should not be any because the queued disable
  // state change should have been cleared when the adapter became unavailable.
  SetAdapterPoweredState(/*powered=*/true);
  EXPECT_EQ(mojom::BluetoothSystemState::kEnabled, GetAdapterState());
  EXPECT_EQ(5u, GetNumObserverEvents());

  // Initiate a disable state change. This should be the in-progress state
  // change.
  SetBluetoothEnabledState(/*enabled=*/false);
  EXPECT_EQ(mojom::BluetoothSystemState::kDisabling, GetAdapterState());
  EXPECT_EQ(6u, GetNumObserverEvents());

  histogram_tester.ExpectBucketCount("Bluetooth.ChromeOS.PoweredState", false,
                                     1);
  histogram_tester.ExpectBucketCount("Bluetooth.ChromeOS.PoweredState", true,
                                     1);

  // Finish the disable change.
  InvokeSetPoweredCallback(/*expected_pending_state=*/false,
                           /*success=*/true);
  EXPECT_EQ(mojom::BluetoothSystemState::kDisabled, GetAdapterState());
  EXPECT_EQ(7u, GetNumObserverEvents());

  histogram_tester.ExpectBucketCount(
      "Bluetooth.ChromeOS.PoweredState.Disable.Result", false, 0);
  histogram_tester.ExpectBucketCount(
      "Bluetooth.ChromeOS.PoweredState.Disable.Result", true, 1);
  histogram_tester.ExpectBucketCount(
      "Bluetooth.ChromeOS.PoweredState.Enable.Result", false, 0);
  histogram_tester.ExpectBucketCount(
      "Bluetooth.ChromeOS.PoweredState.Enable.Result", true, 0);
}

}  // namespace ash::bluetooth_config
