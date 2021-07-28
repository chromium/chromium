// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chromeos/services/bluetooth_config/system_properties_provider_impl.h"

#include <memory>

#include "base/run_loop.h"
#include "base/test/task_environment.h"
#include "chromeos/services/bluetooth_config/fake_adapter_state_controller.h"
#include "chromeos/services/bluetooth_config/fake_system_properties_observer.h"
#include "mojo/public/cpp/bindings/pending_remote.h"
#include "mojo/public/cpp/bindings/receiver.h"
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
    provider_ = std::make_unique<SystemPropertiesProviderImpl>(
        &fake_adapter_state_controller_);
  }

  void SetSystemState(mojom::BluetoothSystemState system_state) {
    fake_adapter_state_controller_.SetSystemState(system_state);
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
  FakeAdapterStateController fake_adapter_state_controller_;

  std::unique_ptr<SystemPropertiesProvider> provider_;
};

TEST_F(SystemPropertiesProviderImplTest, SystemStateChanges) {
  std::unique_ptr<FakeSystemPropertiesObserver> observer = Observe();

  // Once Observe() is called, the observer should immediately receive one
  // update with the current state.
  ASSERT_EQ(1u, observer->received_properties_list().size());
  EXPECT_EQ(mojom::BluetoothSystemState::kEnabled,
            observer->received_properties_list()[0]->system_state);

  // Change the state to kDisabled and verify that the observer was notified.
  SetSystemState(mojom::BluetoothSystemState::kDisabled);
  ASSERT_EQ(2u, observer->received_properties_list().size());
  EXPECT_EQ(mojom::BluetoothSystemState::kDisabled,
            observer->received_properties_list()[1]->system_state);

  // Change the state to kUnavailable and verify that the observer was notified.
  SetSystemState(mojom::BluetoothSystemState::kUnavailable);
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

  // Change the state to kDisabled; the observer should not be notified since it
  // is no longer connected.
  SetSystemState(mojom::BluetoothSystemState::kDisabled);
  EXPECT_EQ(1u, observer->received_properties_list().size());
}

}  // namespace bluetooth_config
}  // namespace chromeos
