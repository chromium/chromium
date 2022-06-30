// Copyright 2022 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chromeos/ash/components/oobe_quick_start/connectivity/target_device_connection_broker_impl.h"

#include "base/bind.h"
#include "base/test/task_environment.h"
#include "chromeos/ash/components/oobe_quick_start/connectivity/fast_pair_advertiser.h"
#include "chromeos/ash/components/oobe_quick_start/connectivity/target_device_connection_broker_factory.h"
#include "device/bluetooth/bluetooth_adapter_factory.h"
#include "device/bluetooth/test/mock_bluetooth_adapter.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace {

using testing::NiceMock;

using TargetDeviceConnectionBroker =
    ash::quick_start::TargetDeviceConnectionBroker;
using TargetDeviceConnectionBrokerImpl =
    ash::quick_start::TargetDeviceConnectionBrokerImpl;

class FakeFastPairAdvertiser : public FastPairAdvertiser {
 public:
  explicit FakeFastPairAdvertiser(
      scoped_refptr<device::BluetoothAdapter> adapter,
      bool should_succeed_on_start,
      base::OnceCallback<void()> on_stop_advertising_callback,
      base::OnceCallback<void()> on_destroy_callback)
      : FastPairAdvertiser(adapter),
        should_succeed_on_start_(should_succeed_on_start),
        on_stop_advertising_callback_(std::move(on_stop_advertising_callback)),
        on_destroy_callback_(std::move(on_destroy_callback)) {}

  ~FakeFastPairAdvertiser() override {
    StopAdvertising(base::DoNothing());
    std::move(on_destroy_callback_).Run();
  }

  void StartAdvertising(base::OnceCallback<void()> callback,
                        base::OnceCallback<void()> error_callback) override {
    ++start_advertising_call_count_;
    if (should_succeed_on_start_)
      std::move(callback).Run();
    else
      std::move(error_callback).Run();
  }

  void StopAdvertising(base::OnceCallback<void()> callback) override {
    if (!has_called_on_stop_advertising_callback_) {
      std::move(on_stop_advertising_callback_).Run();
      has_called_on_stop_advertising_callback_ = true;
    }

    std::move(callback).Run();
  }

  size_t start_advertising_call_count() {
    return start_advertising_call_count_;
  }

 private:
  bool should_succeed_on_start_;
  bool has_called_on_stop_advertising_callback_ = false;
  size_t start_advertising_call_count_ = 0u;
  base::OnceCallback<void()> on_stop_advertising_callback_;
  base::OnceCallback<void()> on_destroy_callback_;
};

class FakeFastPairAdvertiserFactory : public FastPairAdvertiser::Factory {
 public:
  explicit FakeFastPairAdvertiserFactory(bool should_succeed_on_start)
      : should_succeed_on_start_(should_succeed_on_start) {}

  std::unique_ptr<FastPairAdvertiser> CreateInstance(
      scoped_refptr<device::BluetoothAdapter> adapter) override {
    auto fake_fast_pair_advertiser = std::make_unique<FakeFastPairAdvertiser>(
        adapter, should_succeed_on_start_,
        base::BindOnce(&FakeFastPairAdvertiserFactory::OnStopAdvertising,
                       weak_ptr_factory_.GetWeakPtr()),
        base::BindOnce(
            &FakeFastPairAdvertiserFactory::OnFastPairAdvertiserDestroyed,
            weak_ptr_factory_.GetWeakPtr()));
    last_fake_fast_pair_advertiser_ = fake_fast_pair_advertiser.get();
    return std::move(fake_fast_pair_advertiser);
  }

  void OnFastPairAdvertiserDestroyed() {
    fast_pair_advertiser_destroyed_ = true;
    last_fake_fast_pair_advertiser_ = nullptr;
  }

  void OnStopAdvertising() { stop_advertising_called_ = true; }

  size_t StartAdvertisingCount() {
    return last_fake_fast_pair_advertiser_
               ? last_fake_fast_pair_advertiser_->start_advertising_call_count()
               : 0;
  }

  bool AdvertiserDestroyed() { return fast_pair_advertiser_destroyed_; }

  bool StopAdvertisingCalled() { return stop_advertising_called_; }

 private:
  FakeFastPairAdvertiser* last_fake_fast_pair_advertiser_ = nullptr;
  bool should_succeed_on_start_ = false;
  bool stop_advertising_called_ = false;
  bool fast_pair_advertiser_destroyed_ = false;
  base::WeakPtrFactory<FakeFastPairAdvertiserFactory> weak_ptr_factory_{this};
};

}  // namespace

class TargetDeviceConnectionBrokerImplTest : public testing::Test {
 public:
  TargetDeviceConnectionBrokerImplTest() = default;
  TargetDeviceConnectionBrokerImplTest(TargetDeviceConnectionBrokerImplTest&) =
      delete;
  TargetDeviceConnectionBrokerImplTest& operator=(
      TargetDeviceConnectionBrokerImplTest&) = delete;
  ~TargetDeviceConnectionBrokerImplTest() override = default;

  void SetUp() override {
    mock_bluetooth_adapter_ =
        base::MakeRefCounted<NiceMock<device::MockBluetoothAdapter>>();
    ON_CALL(*mock_bluetooth_adapter_, IsPresent())
        .WillByDefault(Invoke(
            this, &TargetDeviceConnectionBrokerImplTest::IsBluetoothPresent));
    ON_CALL(*mock_bluetooth_adapter_, IsPowered())
        .WillByDefault(Invoke(
            this, &TargetDeviceConnectionBrokerImplTest::IsBluetoothPowered));
    device::BluetoothAdapterFactory::SetAdapterForTesting(
        mock_bluetooth_adapter_);

    CreateConnectionBroker();
    SetFakeFastPairAdvertiserFactory(/*should_succeed_on_start=*/true);
    // Allow the Bluetooth adapter to be fetched.
    base::RunLoop().RunUntilIdle();
  }

  void CreateConnectionBroker() {
    connection_broker_ =
        ash::quick_start::TargetDeviceConnectionBrokerFactory::Create();
  }

  bool IsBluetoothPowered() { return is_bluetooth_powered_; }

  bool IsBluetoothPresent() { return is_bluetooth_present_; }

  void SetBluetoothIsPowered(bool powered) { is_bluetooth_powered_ = powered; }

  void SetBluetoothIsPresent(bool present) { is_bluetooth_present_ = present; }

  void SetFakeFastPairAdvertiserFactory(bool should_succeed_on_start) {
    fast_pair_advertiser_factory_ =
        std::make_unique<FakeFastPairAdvertiserFactory>(
            should_succeed_on_start);
    FastPairAdvertiser::Factory::SetFactoryForTesting(
        fast_pair_advertiser_factory_.get());
  }

  void StartAdvertisingResultCallback(bool success) {
    start_advertising_callback_called_ = true;
    if (success) {
      start_advertising_callback_success_ = true;
      return;
    }
    start_advertising_callback_success_ = false;
  }

  void StopAdvertisingCallback() { stop_advertising_callback_called_ = true; }

 protected:
  bool is_bluetooth_powered_ = true;
  bool is_bluetooth_present_ = true;
  bool start_advertising_callback_called_ = false;
  bool start_advertising_callback_success_ = false;
  bool stop_advertising_callback_called_ = false;
  scoped_refptr<NiceMock<device::MockBluetoothAdapter>> mock_bluetooth_adapter_;
  std::unique_ptr<TargetDeviceConnectionBroker> connection_broker_;
  std::unique_ptr<FakeFastPairAdvertiserFactory> fast_pair_advertiser_factory_;
  base::test::SingleThreadTaskEnvironment task_environment_;
  base::WeakPtrFactory<TargetDeviceConnectionBrokerImplTest> weak_ptr_factory_{
      this};
};

TEST_F(TargetDeviceConnectionBrokerImplTest, GetFeatureSupportStatus) {
  SetBluetoothIsPresent(false);
  EXPECT_EQ(
      TargetDeviceConnectionBrokerImpl::FeatureSupportStatus::kNotSupported,
      connection_broker_->GetFeatureSupportStatus());

  SetBluetoothIsPresent(true);
  EXPECT_EQ(TargetDeviceConnectionBrokerImpl::FeatureSupportStatus::kSupported,
            connection_broker_->GetFeatureSupportStatus());
}

TEST_F(TargetDeviceConnectionBrokerImplTest, StartFastPairAdvertising) {
  EXPECT_EQ(0u, fast_pair_advertiser_factory_->StartAdvertisingCount());

  connection_broker_->StartAdvertising(
      nullptr,
      base::BindOnce(
          &TargetDeviceConnectionBrokerImplTest::StartAdvertisingResultCallback,
          weak_ptr_factory_.GetWeakPtr()));
  EXPECT_EQ(1u, fast_pair_advertiser_factory_->StartAdvertisingCount());
  EXPECT_TRUE(start_advertising_callback_called_);
  EXPECT_TRUE(start_advertising_callback_success_);
}

TEST_F(TargetDeviceConnectionBrokerImplTest,
       StartFastPairAdvertisingError_BluetoothNotPowered) {
  SetBluetoothIsPowered(false);
  EXPECT_EQ(0u, fast_pair_advertiser_factory_->StartAdvertisingCount());

  connection_broker_->StartAdvertising(
      nullptr,
      base::BindOnce(
          &TargetDeviceConnectionBrokerImplTest::StartAdvertisingResultCallback,
          weak_ptr_factory_.GetWeakPtr()));
  EXPECT_EQ(0u, fast_pair_advertiser_factory_->StartAdvertisingCount());
  EXPECT_TRUE(start_advertising_callback_called_);
  EXPECT_FALSE(start_advertising_callback_success_);
}

TEST_F(TargetDeviceConnectionBrokerImplTest,
       StartFastPairAdvertisingError_Unsuccessful) {
  SetFakeFastPairAdvertiserFactory(/*should_succeed_on_start=*/false);
  EXPECT_EQ(0u, fast_pair_advertiser_factory_->StartAdvertisingCount());

  connection_broker_->StartAdvertising(
      nullptr,
      base::BindOnce(
          &TargetDeviceConnectionBrokerImplTest::StartAdvertisingResultCallback,
          weak_ptr_factory_.GetWeakPtr()));
  EXPECT_TRUE(start_advertising_callback_called_);
  EXPECT_FALSE(start_advertising_callback_success_);
  EXPECT_TRUE(fast_pair_advertiser_factory_->AdvertiserDestroyed());
}

TEST_F(TargetDeviceConnectionBrokerImplTest,
       StopFastPairAdvertising_NeverStarted) {
  // If StartAdvertising is never called, StopAdvertising should not propagate
  // to the fast pair advertiser.
  connection_broker_->StopAdvertising(base::BindOnce(
      &TargetDeviceConnectionBrokerImplTest::StopAdvertisingCallback,
      weak_ptr_factory_.GetWeakPtr()));

  EXPECT_TRUE(stop_advertising_callback_called_);
  EXPECT_FALSE(fast_pair_advertiser_factory_->StopAdvertisingCalled());
}

TEST_F(TargetDeviceConnectionBrokerImplTest, StopFastPairAdvertising) {
  connection_broker_->StartAdvertising(
      nullptr,
      base::BindOnce(
          &TargetDeviceConnectionBrokerImplTest::StartAdvertisingResultCallback,
          weak_ptr_factory_.GetWeakPtr()));

  EXPECT_EQ(1u, fast_pair_advertiser_factory_->StartAdvertisingCount());
  EXPECT_TRUE(start_advertising_callback_called_);
  EXPECT_TRUE(start_advertising_callback_success_);
  EXPECT_FALSE(fast_pair_advertiser_factory_->StopAdvertisingCalled());

  connection_broker_->StopAdvertising(base::BindOnce(
      &TargetDeviceConnectionBrokerImplTest::StopAdvertisingCallback,
      weak_ptr_factory_.GetWeakPtr()));

  EXPECT_TRUE(fast_pair_advertiser_factory_->StopAdvertisingCalled());
  EXPECT_TRUE(fast_pair_advertiser_factory_->AdvertiserDestroyed());
  EXPECT_TRUE(stop_advertising_callback_called_);
}
