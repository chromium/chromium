// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chromeos/services/bluetooth_config/system_properties_provider_impl.h"

#include <memory>
#include <vector>

#include "base/run_loop.h"
#include "base/test/task_environment.h"
#include "chromeos/services/bluetooth_config/fake_system_properties_observer.h"
#include "device/bluetooth/test/mock_bluetooth_adapter.h"
#include "mojo/public/cpp/bindings/pending_remote.h"
#include "mojo/public/cpp/bindings/receiver.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace chromeos {
namespace bluetooth_config {

class SystemPropertiesProviderImplTest : public testing::Test {
 protected:
  SystemPropertiesProviderImplTest() = default;
  SystemPropertiesProviderImplTest(const SystemPropertiesProviderImplTest&) =
      delete;
  SystemPropertiesProviderImplTest& operator=(
      const SystemPropertiesProviderImplTest&) = delete;
  ~SystemPropertiesProviderImplTest() override = default;

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

    provider_ = std::make_unique<SystemPropertiesProviderImpl>(mock_adapter_);
  }

  void SetAdapterPresentState(bool present) {
    if (is_adapter_present_ == present)
      return;

    is_adapter_present_ = present;

    SystemPropertiesProviderImpl* impl =
        static_cast<SystemPropertiesProviderImpl*>(provider_.get());
    impl->AdapterPresentChanged(mock_adapter_.get(), present);

    provider_->FlushForTesting();
  }

  void SetAdapterPoweredState(bool powered) {
    if (is_adapter_powered_ == powered)
      return;

    is_adapter_powered_ = powered;

    SystemPropertiesProviderImpl* impl =
        static_cast<SystemPropertiesProviderImpl*>(provider_.get());
    impl->AdapterPoweredChanged(mock_adapter_.get(), powered);

    provider_->FlushForTesting();
  }

  std::unique_ptr<FakeSystemPropertiesObserver> Observe() {
    auto observer = std::make_unique<FakeSystemPropertiesObserver>();
    provider_->Observe(observer->GeneratePendingRemote());
    provider_->FlushForTesting();
    return observer;
  }

 private:
  base::test::TaskEnvironment task_environment_;

  bool is_adapter_present_ = true;
  bool is_adapter_powered_ = true;

  scoped_refptr<testing::NiceMock<device::MockBluetoothAdapter>> mock_adapter_;

  std::unique_ptr<SystemPropertiesProvider> provider_;
};

TEST_F(SystemPropertiesProviderImplTest, SystemStateChanges) {
  std::unique_ptr<FakeSystemPropertiesObserver> observer = Observe();

  // Once Observe() is called, the observer should immediately receive one
  // update with the current state.
  ASSERT_EQ(1u, observer->received_properties_list().size());
  EXPECT_EQ(mojom::BluetoothSystemState::kEnabled,
            observer->received_properties_list()[0]->system_state);

  // Change the power state to false; observer should have been notified that
  // Bluetooth is disabled.
  SetAdapterPoweredState(false);
  ASSERT_EQ(2u, observer->received_properties_list().size());
  EXPECT_EQ(mojom::BluetoothSystemState::kDisabled,
            observer->received_properties_list()[1]->system_state);

  // Change the present state to false; observer should have been notified that
  // Bluetooth is unavailable.
  SetAdapterPresentState(false);
  ASSERT_EQ(3u, observer->received_properties_list().size());
  EXPECT_EQ(mojom::BluetoothSystemState::kUnavailable,
            observer->received_properties_list()[2]->system_state);
}

TEST_F(SystemPropertiesProviderImplTest, DisconnectToStopObserving) {
  // The initial properties list should have been received.
  std::unique_ptr<FakeSystemPropertiesObserver> observer = Observe();
  ASSERT_EQ(1u, observer->received_properties_list().size());

  // Disconnect the Mojo pipe; this should stop observing.
  observer->DisconnectMojoPipe();

  // Change the power state to false; this is a change in system properties, but
  // the observer should not be notified since it is no longer connected.
  SetAdapterPoweredState(false);
  EXPECT_EQ(1u, observer->received_properties_list().size());
}

}  // namespace bluetooth_config
}  // namespace chromeos
