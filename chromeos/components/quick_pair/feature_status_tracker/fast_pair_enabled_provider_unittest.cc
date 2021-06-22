// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chromeos/components/quick_pair/feature_status_tracker/fast_pair_enabled_provider.h"

#include <memory>

#include "base/callback.h"
#include "base/memory/scoped_refptr.h"
#include "base/test/mock_callback.h"
#include "base/test/scoped_feature_list.h"
#include "chromeos/components/quick_pair/common/quick_pair_features.h"
#include "chromeos/components/quick_pair/feature_status_tracker/bluetooth_enabled_provider.h"
#include "chromeos/components/quick_pair/feature_status_tracker/fake_bluetooth_adapter.h"
#include "chromeos/components/quick_pair/feature_status_tracker/mock_bluetooth_enabled_provider.h"
#include "device/bluetooth/bluetooth_adapter_factory.h"
#include "testing/gtest/include/gtest/gtest-param-test.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace chromeos {
namespace quick_pair {

class FastPairEnabledProviderTest : public testing::Test {
 public:
  void SetUp() override {
    adapter_ = base::MakeRefCounted<FakeBluetoothAdapter>();
    device::BluetoothAdapterFactory::SetAdapterForTesting(adapter_);
  }

 protected:
  scoped_refptr<FakeBluetoothAdapter> adapter_;
};

TEST_F(FastPairEnabledProviderTest, ProviderCallbackIsInvokedOnBTChanges) {
  base::test::ScopedFeatureList feature_list{features::kFastPair};

  base::MockCallback<base::RepeatingCallback<void(bool)>> callback;
  EXPECT_CALL(callback, Run(true));

  auto provider = std::make_unique<FastPairEnabledProvider>(
      std::unique_ptr<BluetoothEnabledProvider>(
          new BluetoothEnabledProvider()));

  provider->SetCallback(callback.Get());

  adapter_->NotifyPoweredChanged(true);
}

using TestParam = std::tuple<bool, bool>;

class FastPairEnabledProviderTestWithParams
    : public FastPairEnabledProviderTest,
      public testing::WithParamInterface<TestParam> {};

TEST_P(FastPairEnabledProviderTestWithParams, IsEnabledWhenExpected) {
  bool is_flag_enabled = std::get<0>(GetParam());
  bool is_bt_enabled = std::get<1>(GetParam());

  base::test::ScopedFeatureList feature_list;
  feature_list.InitWithFeatureState(features::kFastPair, is_flag_enabled);

  auto* bluetooth_enabled_provider = new MockBluetoothEnabledProvider();
  ON_CALL(*bluetooth_enabled_provider, is_enabled)
      .WillByDefault(testing::Return(is_bt_enabled));

  auto provider = std::make_unique<FastPairEnabledProvider>(
      std::unique_ptr<BluetoothEnabledProvider>(bluetooth_enabled_provider));

  bool all_are_enabled = is_flag_enabled && is_bt_enabled;

  EXPECT_EQ(provider->is_enabled(), all_are_enabled);
}

INSTANTIATE_TEST_SUITE_P(FastPairEnabledProviderTestWithParams,
                         FastPairEnabledProviderTestWithParams,
                         testing::Combine(testing::Bool(), testing::Bool()));

}  // namespace quick_pair
}  // namespace chromeos
