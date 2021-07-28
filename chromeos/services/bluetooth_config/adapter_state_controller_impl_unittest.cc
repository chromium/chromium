// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chromeos/services/bluetooth_config/adapter_state_controller_impl.h"

#include <memory>
#include <vector>

#include "base/callback.h"
#include "base/test/task_environment.h"
#include "device/bluetooth/test/mock_bluetooth_adapter.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/abseil-cpp/absl/types/optional.h"

namespace chromeos {
namespace bluetooth_config {
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

class AdapterStateControllerImplTest : public testing::Test {
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
              set_powered_success_callback_ = std::move(success_callback);
              set_powered_error_callback_ = std::move(error_callback);
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

    if (success) {
      SetAdapterPoweredState(expected_pending_state);
      std::move(set_powered_success_callback_).Run();
      set_powered_error_callback_.Reset();
      return;
    }

    std::move(set_powered_error_callback_).Run();
    set_powered_success_callback_.Reset();
  }

  size_t GetNumObserverEvents() const { return fake_observer_.num_calls(); }

 private:
  base::test::TaskEnvironment task_environment_;

  bool is_adapter_present_ = true;
  bool is_adapter_powered_ = true;

  absl::optional<bool> pending_power_state_;
  base::OnceClosure set_powered_success_callback_;
  base::OnceClosure set_powered_error_callback_;

  scoped_refptr<testing::NiceMock<device::MockBluetoothAdapter>> mock_adapter_;
  FakeObserver fake_observer_;

  std::unique_ptr<AdapterStateController> adapter_state_controller_;
};

TEST_F(AdapterStateControllerImplTest, StateChangesFromOutsideClass) {
  EXPECT_EQ(mojom::BluetoothSystemState::kEnabled, GetAdapterState());

  SetAdapterPoweredState(/*powered=*/false);
  EXPECT_EQ(mojom::BluetoothSystemState::kDisabled, GetAdapterState());
  EXPECT_EQ(1u, GetNumObserverEvents());

  SetAdapterPresentState(/*present=*/false);
  EXPECT_EQ(mojom::BluetoothSystemState::kUnavailable, GetAdapterState());
  EXPECT_EQ(2u, GetNumObserverEvents());
}

TEST_F(AdapterStateControllerImplTest, SetBluetoothEnabledState) {
  EXPECT_EQ(mojom::BluetoothSystemState::kEnabled, GetAdapterState());

  SetBluetoothEnabledState(/*enabled=*/false);
  EXPECT_EQ(mojom::BluetoothSystemState::kDisabling, GetAdapterState());
  EXPECT_EQ(1u, GetNumObserverEvents());

  InvokeSetPoweredCallback(/*expected_pending_state=*/false,
                           /*success=*/true);
  EXPECT_EQ(mojom::BluetoothSystemState::kDisabled, GetAdapterState());
  EXPECT_EQ(2u, GetNumObserverEvents());

  SetBluetoothEnabledState(/*enabled=*/true);
  EXPECT_EQ(mojom::BluetoothSystemState::kEnabling, GetAdapterState());
  EXPECT_EQ(3u, GetNumObserverEvents());

  InvokeSetPoweredCallback(/*expected_pending_state=*/true,
                           /*success=*/true);
  EXPECT_EQ(mojom::BluetoothSystemState::kEnabled, GetAdapterState());
  EXPECT_EQ(4u, GetNumObserverEvents());
}

TEST_F(AdapterStateControllerImplTest, SetBluetoothEnabledState_Error) {
  EXPECT_EQ(mojom::BluetoothSystemState::kEnabled, GetAdapterState());

  SetBluetoothEnabledState(/*enabled=*/false);
  EXPECT_EQ(mojom::BluetoothSystemState::kDisabling, GetAdapterState());
  EXPECT_EQ(1u, GetNumObserverEvents());

  InvokeSetPoweredCallback(/*expected_pending_state=*/false,
                           /*success=*/false);
  EXPECT_EQ(mojom::BluetoothSystemState::kEnabled, GetAdapterState());
  EXPECT_EQ(2u, GetNumObserverEvents());
}

TEST_F(AdapterStateControllerImplTest, MultiplePowerChanges_SameChange) {
  EXPECT_EQ(mojom::BluetoothSystemState::kEnabled, GetAdapterState());

  // Start disabling once.
  SetBluetoothEnabledState(/*enabled=*/false);
  EXPECT_EQ(mojom::BluetoothSystemState::kDisabling, GetAdapterState());
  EXPECT_EQ(1u, GetNumObserverEvents());

  // Try disabling again, even though this is already in progress.
  SetBluetoothEnabledState(/*enabled=*/false);
  EXPECT_EQ(mojom::BluetoothSystemState::kDisabling, GetAdapterState());
  EXPECT_EQ(1u, GetNumObserverEvents());

  InvokeSetPoweredCallback(/*expected_pending_state=*/false,
                           /*success=*/true);
  EXPECT_EQ(mojom::BluetoothSystemState::kDisabled, GetAdapterState());
  EXPECT_EQ(2u, GetNumObserverEvents());
}

TEST_F(AdapterStateControllerImplTest, MultiplePowerChanges_DifferentChange) {
  EXPECT_EQ(mojom::BluetoothSystemState::kEnabled, GetAdapterState());

  // Start disabling.
  SetBluetoothEnabledState(/*enabled=*/false);
  EXPECT_EQ(mojom::BluetoothSystemState::kDisabling, GetAdapterState());
  EXPECT_EQ(1u, GetNumObserverEvents());

  // Before the disable request finishes, start enabling; this simulates a user
  // very quickly clicking the on/off toggle.
  SetBluetoothEnabledState(/*enabled=*/true);
  EXPECT_EQ(mojom::BluetoothSystemState::kDisabling, GetAdapterState());
  EXPECT_EQ(1u, GetNumObserverEvents());

  // Invoke the first callback; because there was a queued request, we should
  // now be enabling.
  InvokeSetPoweredCallback(/*expected_pending_state=*/false,
                           /*success=*/true);
  EXPECT_EQ(mojom::BluetoothSystemState::kEnabling, GetAdapterState());
  EXPECT_EQ(3u, GetNumObserverEvents());

  // Invoke the second request; we should now be enabled.
  InvokeSetPoweredCallback(/*expected_pending_state=*/true,
                           /*success=*/true);
  EXPECT_EQ(mojom::BluetoothSystemState::kEnabled, GetAdapterState());
  EXPECT_EQ(4u, GetNumObserverEvents());
}

}  // namespace bluetooth_config
}  // namespace chromeos
