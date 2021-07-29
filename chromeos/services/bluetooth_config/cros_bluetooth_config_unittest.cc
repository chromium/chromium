// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chromeos/services/bluetooth_config/cros_bluetooth_config.h"

#include <memory>

#include "base/test/task_environment.h"
#include "chromeos/services/bluetooth_config/fake_system_properties_observer.h"
#include "chromeos/services/bluetooth_config/initializer_impl.h"
#include "device/bluetooth/test/mock_bluetooth_adapter.h"
#include "mojo/public/cpp/bindings/pending_remote.h"
#include "mojo/public/cpp/bindings/receiver.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace chromeos {
namespace bluetooth_config {

// Tests initialization and bindings in for CrosBluetoothConfig. Note that this
// test is not meant to be exhaustive of the API, since these details are tested
// within the unit tests for the other classes owned by CrosBluetoothConfig.
class CrosBluetoothConfigTest : public testing::Test {
 protected:
  CrosBluetoothConfigTest() = default;
  CrosBluetoothConfigTest(const CrosBluetoothConfigTest&) = delete;
  CrosBluetoothConfigTest& operator=(const CrosBluetoothConfigTest&) = delete;
  ~CrosBluetoothConfigTest() override = default;

  // testing::Test:
  void SetUp() override {
    mock_adapter_ =
        base::MakeRefCounted<testing::NiceMock<device::MockBluetoothAdapter>>();
    InitializerImpl initializer;
    cros_bluetooth_config_ =
        std::make_unique<CrosBluetoothConfig>(initializer, mock_adapter_);
  }

  mojo::Remote<mojom::CrosBluetoothConfig> BindToInterface() {
    mojo::Remote<mojom::CrosBluetoothConfig> remote;
    cros_bluetooth_config_->BindPendingReceiver(
        remote.BindNewPipeAndPassReceiver());
    return remote;
  }

 private:
  base::test::TaskEnvironment task_environment_;
  scoped_refptr<testing::NiceMock<device::MockBluetoothAdapter>> mock_adapter_;

  std::unique_ptr<CrosBluetoothConfig> cros_bluetooth_config_;
};

TEST_F(CrosBluetoothConfigTest, BindMultipleClients) {
  mojo::Remote<mojom::CrosBluetoothConfig> remote1 = BindToInterface();
  mojo::Remote<mojom::CrosBluetoothConfig> remote2 = BindToInterface();
}

TEST_F(CrosBluetoothConfigTest, CallApiFunction) {
  mojo::Remote<mojom::CrosBluetoothConfig> remote = BindToInterface();
  FakeSystemPropertiesObserver fake_observer;
  remote->ObserveSystemProperties(fake_observer.GeneratePendingRemote());
}

}  // namespace bluetooth_config
}  // namespace chromeos
