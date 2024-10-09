// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/services/sharing/nearby/platform/bluetooth_adapter.h"

#include <memory>

#include "base/memory/raw_ptr.h"
#include "base/run_loop.h"
#include "base/test/bind.h"
#include "base/test/scoped_feature_list.h"
#include "base/test/task_environment.h"
#include "chrome/services/sharing/nearby/test_support/fake_adapter.h"
#include "components/cross_device/nearby/nearby_features.h"
#include "mojo/public/cpp/bindings/pending_remote.h"
#include "mojo/public/cpp/bindings/self_owned_receiver.h"
#include "mojo/public/cpp/bindings/shared_remote.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace nearby::chrome {

class BluetoothAdapterTest : public testing::Test {
 public:
  BluetoothAdapterTest() = default;
  ~BluetoothAdapterTest() override = default;
  BluetoothAdapterTest(const BluetoothAdapterTest&) = delete;
  BluetoothAdapterTest& operator=(const BluetoothAdapterTest&) = delete;

  void SetUp() override {
    auto fake_adapter = std::make_unique<bluetooth::FakeAdapter>();
    fake_adapter_ = fake_adapter.get();

    mojo::PendingRemote<bluetooth::mojom::Adapter> pending_adapter;

    mojo::MakeSelfOwnedReceiver(
        std::move(fake_adapter),
        pending_adapter.InitWithNewPipeAndPassReceiver());

    remote_adapter_.Bind(std::move(pending_adapter),
                         /*bind_task_runner=*/nullptr);

    bluetooth_adapter_ = std::make_unique<BluetoothAdapter>(remote_adapter_);
  }

 protected:
  raw_ptr<bluetooth::FakeAdapter> fake_adapter_;
  std::unique_ptr<BluetoothAdapter> bluetooth_adapter_;

 private:
  mojo::SharedRemote<bluetooth::mojom::Adapter> remote_adapter_;
  base::test::TaskEnvironment task_environment_;
};

TEST_F(BluetoothAdapterTest, TestIsEnabled) {
  fake_adapter_->present_ = false;
  fake_adapter_->powered_ = false;
  EXPECT_FALSE(bluetooth_adapter_->IsEnabled());

  fake_adapter_->present_ = true;
  fake_adapter_->powered_ = false;
  EXPECT_FALSE(bluetooth_adapter_->IsEnabled());

  fake_adapter_->present_ = false;
  fake_adapter_->powered_ = true;
  EXPECT_FALSE(bluetooth_adapter_->IsEnabled());

  fake_adapter_->present_ = true;
  fake_adapter_->powered_ = true;
  EXPECT_TRUE(bluetooth_adapter_->IsEnabled());
}

TEST_F(BluetoothAdapterTest, TestGetScanMode) {
  fake_adapter_->present_ = false;
  fake_adapter_->powered_ = false;
  fake_adapter_->discoverable_ = false;
  EXPECT_EQ(BluetoothAdapter::ScanMode::kUnknown,
            bluetooth_adapter_->GetScanMode());

  fake_adapter_->present_ = true;
  EXPECT_EQ(BluetoothAdapter::ScanMode::kNone,
            bluetooth_adapter_->GetScanMode());

  fake_adapter_->powered_ = true;
  EXPECT_EQ(BluetoothAdapter::ScanMode::kConnectable,
            bluetooth_adapter_->GetScanMode());

  fake_adapter_->discoverable_ = true;
  EXPECT_EQ(BluetoothAdapter::ScanMode::kConnectableDiscoverable,
            bluetooth_adapter_->GetScanMode());
}

TEST_F(BluetoothAdapterTest, TestSetScanMode) {
  fake_adapter_->discoverable_ = false;

  EXPECT_TRUE(
      bluetooth_adapter_->SetScanMode(BluetoothAdapter::ScanMode::kUnknown));
  EXPECT_FALSE(fake_adapter_->discoverable_);

  EXPECT_TRUE(
      bluetooth_adapter_->SetScanMode(BluetoothAdapter::ScanMode::kNone));
  EXPECT_FALSE(fake_adapter_->discoverable_);

  EXPECT_TRUE(bluetooth_adapter_->SetScanMode(
      BluetoothAdapter::ScanMode::kConnectable));
  EXPECT_FALSE(fake_adapter_->discoverable_);

  EXPECT_TRUE(bluetooth_adapter_->SetScanMode(
      BluetoothAdapter::ScanMode::kConnectableDiscoverable));
  EXPECT_TRUE(fake_adapter_->discoverable_);

  EXPECT_TRUE(bluetooth_adapter_->SetScanMode(
      BluetoothAdapter::ScanMode::kConnectable));
  EXPECT_FALSE(fake_adapter_->discoverable_);
}

TEST_F(BluetoothAdapterTest, TestGetName) {
  EXPECT_EQ(fake_adapter_->name_, bluetooth_adapter_->GetName());
}

TEST_F(BluetoothAdapterTest, TestSetName) {
  std::string name = "NewName";
  EXPECT_NE(name, fake_adapter_->name_);
  EXPECT_TRUE(bluetooth_adapter_->SetName(name));
  EXPECT_EQ(name, fake_adapter_->name_);
}

TEST_F(BluetoothAdapterTest,
       TestSetName_BluetoothClassicAdvertisingFlagDisabled) {
  base::test::ScopedFeatureList feature_list;
  feature_list.InitWithFeatures(
      /*enabled_features=*/{},
      /*disabled_features=*/{
          ::features::kEnableNearbyBluetoothClassicAdvertising});

  // When the flag is false, SetName returns vacuously true, but doesn't set the
  // name when Bluetooth Classic Advertising flag is disabled; this is because
  // the "name" field is a specific "advertising" format used by Nearby
  // Connections. By not setting "name", the device remains non-discoverable
  // over Nearby Connections.
  std::string name = "NewName";
  EXPECT_NE(name, fake_adapter_->name_);
  EXPECT_TRUE(bluetooth_adapter_->SetName(name, /*persist=*/true));
  EXPECT_NE(name, fake_adapter_->name_);
  EXPECT_TRUE(bluetooth_adapter_->SetName(name, /*persist=*/false));
  EXPECT_NE(name, fake_adapter_->name_);
}

TEST_F(BluetoothAdapterTest,
       TestSetName_BluetoothClassicAdvertisingFlagEnabled) {
  base::test::ScopedFeatureList feature_list;
  feature_list.InitWithFeatures(
      /*enabled_features=*/{::features::
                                kEnableNearbyBluetoothClassicAdvertising},
      /*disabled_features=*/{});

  // When flag is enabled, SetName works normally.
  std::string name = "NewName";
  EXPECT_NE(name, fake_adapter_->name_);
  EXPECT_TRUE(bluetooth_adapter_->SetName(name, /*persist=*/true));
  EXPECT_EQ(name, fake_adapter_->name_);

  name = "DifferentName";
  EXPECT_NE(name, fake_adapter_->name_);
  EXPECT_TRUE(bluetooth_adapter_->SetName(name, /*persist=*/false));
  EXPECT_EQ(name, fake_adapter_->name_);
}

TEST_F(BluetoothAdapterTest, TestGetMacAddress) {
  EXPECT_EQ(fake_adapter_->address_, bluetooth_adapter_->GetMacAddress());
}

TEST_F(BluetoothAdapterTest, TestGetAddress) {
  EXPECT_EQ(fake_adapter_->address_, bluetooth_adapter_->GetAddress());
}

}  // namespace nearby::chrome
