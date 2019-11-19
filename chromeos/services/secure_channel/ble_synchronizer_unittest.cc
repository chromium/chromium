// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chromeos/services/secure_channel/ble_synchronizer.h"

#include <memory>

#include "base/bind.h"
#include "base/callback_forward.h"
#include "base/callback_helpers.h"
#include "base/memory/ptr_util.h"
#include "base/memory/ref_counted.h"
#include "base/memory/weak_ptr.h"
#include "base/test/metrics/histogram_tester.h"
#include "base/test/simple_test_clock.h"
#include "base/test/task_environment.h"
#include "base/test/test_simple_task_runner.h"
#include "base/timer/mock_timer.h"
#include "chromeos/services/secure_channel/ble_constants.h"
#include "chromeos/services/secure_channel/data_with_timestamp.h"
#include "device/bluetooth/bluetooth_advertisement.h"
#include "device/bluetooth/test/mock_bluetooth_adapter.h"
#include "device/bluetooth/test/mock_bluetooth_advertisement.h"
#include "testing/gtest/include/gtest/gtest.h"

using testing::Invoke;
using testing::NiceMock;
using testing::_;

namespace chromeos {

namespace secure_channel {

namespace {

const char kId1[] = "id1";
const char kId2[] = "id2";
const char kId3[] = "id3";

int64_t kTimeBetweenEachCommandMs = 200;

struct RegisterAdvertisementArgs {
  RegisterAdvertisementArgs(
      const device::BluetoothAdvertisement::UUIDList& service_uuids,
      const device::BluetoothAdapter::CreateAdvertisementCallback& callback,
      const device::BluetoothAdapter::AdvertisementErrorCallback&
          error_callback)
      : service_uuids(service_uuids),
        callback(callback),
        error_callback(error_callback) {}

  device::BluetoothAdvertisement::UUIDList service_uuids;
  const device::BluetoothAdapter::CreateAdvertisementCallback callback;
  const device::BluetoothAdapter::AdvertisementErrorCallback error_callback;
};

struct UnregisterAdvertisementArgs {
  UnregisterAdvertisementArgs(
      const device::BluetoothAdvertisement::SuccessCallback& callback,
      const device::BluetoothAdvertisement::ErrorCallback& error_callback)
      : callback(callback), error_callback(error_callback) {}

  const device::BluetoothAdvertisement::SuccessCallback callback;
  const device::BluetoothAdvertisement::ErrorCallback error_callback;
};

struct StartDiscoverySessionArgs {
  StartDiscoverySessionArgs(
      const base::RepeatingClosure callback,
      const device::BluetoothAdapter::ErrorCallback& error_callback)
      : callback(callback), error_callback(error_callback) {}

  const base::RepeatingClosure callback;
  const device::BluetoothAdapter::ErrorCallback error_callback;
};

struct StopDiscoverySessionArgs {
  StopDiscoverySessionArgs(
      const base::Closure& callback,
      const device::BluetoothDiscoverySession::ErrorCallback& error_callback)
      : callback(callback), error_callback(error_callback) {}

  const base::Closure callback;
  const device::BluetoothDiscoverySession::ErrorCallback error_callback;
};

class MockBluetoothAdapterWithAdvertisements
    : public device::MockBluetoothAdapter {
 public:
  MockBluetoothAdapterWithAdvertisements() : MockBluetoothAdapter() {}

  MOCK_METHOD1(RegisterAdvertisementWithArgsStruct,
               void(RegisterAdvertisementArgs*));

  void RegisterAdvertisement(
      std::unique_ptr<device::BluetoothAdvertisement::Data> advertisement_data,
      const device::BluetoothAdapter::CreateAdvertisementCallback& callback,
      const device::BluetoothAdapter::AdvertisementErrorCallback&
          error_callback) override {
    RegisterAdvertisementWithArgsStruct(new RegisterAdvertisementArgs(
        *advertisement_data->service_uuids(), callback, error_callback));
  }

 protected:
  ~MockBluetoothAdapterWithAdvertisements() override = default;
};

class FakeBluetoothAdvertisement : public device::BluetoothAdvertisement {
 public:
  // |unregister_callback| should be called with the callbacks passed to
  // Unregister() whenever an Unregister() call occurs.
  FakeBluetoothAdvertisement(
      const base::Callback<
          void(const device::BluetoothAdvertisement::SuccessCallback&,
               const device::BluetoothAdvertisement::ErrorCallback&)>&
          unregister_callback)
      : unregister_callback_(unregister_callback) {}

  // BluetoothAdvertisement:
  void Unregister(
      const device::BluetoothAdvertisement::SuccessCallback& success_callback,
      const device::BluetoothAdvertisement::ErrorCallback& error_callback)
      override {
    unregister_callback_.Run(success_callback, error_callback);
  }

 private:
  ~FakeBluetoothAdvertisement() override = default;

  base::Callback<void(const device::BluetoothAdvertisement::SuccessCallback&,
                      const device::BluetoothAdvertisement::ErrorCallback&)>
      unregister_callback_;

  DISALLOW_COPY_AND_ASSIGN(FakeBluetoothAdvertisement);
};

// Creates a UUIDList with one element of value |id|.
std::unique_ptr<device::BluetoothAdvertisement::UUIDList> CreateUUIDList(
    const std::string& id) {
  return std::make_unique<device::BluetoothAdvertisement::UUIDList>(1u, id);
}

// Creates advertisement data with a UUID list with one element of value |id|.
std::unique_ptr<device::BluetoothAdvertisement::Data> GenerateAdvertisementData(
    const std::string& id) {
  auto data = std::make_unique<device::BluetoothAdvertisement::Data>(
      device::BluetoothAdvertisement::AdvertisementType::
          ADVERTISEMENT_TYPE_PERIPHERAL);
  data->set_service_uuids(CreateUUIDList(id));
  return data;
}

}  // namespace

class SecureChannelBleSynchronizerTest : public testing::Test {
 protected:
  SecureChannelBleSynchronizerTest()
      : fake_advertisement_(base::MakeRefCounted<FakeBluetoothAdvertisement>(
            base::Bind(&SecureChannelBleSynchronizerTest::OnUnregisterCalled,
                       base::Unretained(this)))) {}

  void SetUp() override {
    num_register_success_ = 0;
    num_register_error_ = 0;
    num_unregister_success_ = 0;
    num_unregister_error_ = 0;
    num_start_success_ = 0;
    num_start_error_ = 0;
    num_stop_success_ = 0;
    num_stop_error_ = 0;
    register_args_list_.clear();

    mock_adapter_ = base::MakeRefCounted<
        NiceMock<MockBluetoothAdapterWithAdvertisements>>();
    ON_CALL(*mock_adapter_, RegisterAdvertisementWithArgsStruct(_))
        .WillByDefault(Invoke(
            this,
            &SecureChannelBleSynchronizerTest::OnAdapterRegisterAdvertisement));
    ON_CALL(*mock_adapter_, StartScanWithFilter_(_, _))
        .WillByDefault(Invoke(
            this, &SecureChannelBleSynchronizerTest::OnAdapterStartScan));
    ON_CALL(*mock_adapter_, StopScan(_))
        .WillByDefault(
            Invoke(this, &SecureChannelBleSynchronizerTest::OnStopScan));

    mock_timer_ = new base::MockOneShotTimer();

    test_clock_.Advance(TimeDeltaMillis(kTimeBetweenEachCommandMs));
    test_task_runner_ = base::MakeRefCounted<base::TestSimpleTaskRunner>();

    synchronizer_ =
        BleSynchronizer::Factory::Get()->BuildInstance(mock_adapter_);

    BleSynchronizer* derived_type =
        static_cast<BleSynchronizer*>(synchronizer_.get());
    derived_type->SetTestDoubles(base::WrapUnique(mock_timer_), &test_clock_,
                                 test_task_runner_);
  }

  base::TimeDelta TimeDeltaMillis(int64_t num_millis) {
    return base::TimeDelta::FromMilliseconds(num_millis);
  }

  void OnAdapterRegisterAdvertisement(RegisterAdvertisementArgs* args) {
    register_args_list_.emplace_back(base::WrapUnique(args));
  }

  void OnAdapterStartScan(
      const device::BluetoothDiscoveryFilter* discovery_filter,
      device::BluetoothAdapter::DiscoverySessionResultCallback& callback) {
    EXPECT_EQ(device::BluetoothTransport::BLUETOOTH_TRANSPORT_LE,
              discovery_filter->GetTransport());
    auto copyable_callback =
        base::AdaptCallbackForRepeating(std::move(callback));
    start_discovery_args_list_.emplace_back(
        base::WrapUnique(new StartDiscoverySessionArgs(
            base::BindRepeating(
                copyable_callback, /*is_error=*/false,
                device::UMABluetoothDiscoverySessionOutcome::SUCCESS),
            base::BindRepeating(
                copyable_callback, /*is_error=*/true,
                device::UMABluetoothDiscoverySessionOutcome::UNKNOWN))));
  }

  void RegisterAdvertisement(const std::string& id) {
    synchronizer_->RegisterAdvertisement(
        GenerateAdvertisementData(id),
        base::Bind(&SecureChannelBleSynchronizerTest::OnAdvertisementRegistered,
                   base::Unretained(this)),
        base::Bind(
            &SecureChannelBleSynchronizerTest::OnErrorRegisteringAdvertisement,
            base::Unretained(this)));
  }

  void InvokeRegisterCallback(bool success,
                              const std::string& expected_id,
                              size_t reg_arg_index,
                              size_t expected_registration_result_count) {
    EXPECT_TRUE(register_args_list_.size() >= reg_arg_index);
    EXPECT_EQ(*CreateUUIDList(expected_id),
              register_args_list_[reg_arg_index]->service_uuids);

    BleSynchronizer::BluetoothAdvertisementResult expected_result;
    if (success) {
      register_args_list_[reg_arg_index]->callback.Run(
          base::MakeRefCounted<device::MockBluetoothAdvertisement>());
      expected_result = BleSynchronizer::BluetoothAdvertisementResult::SUCCESS;
    } else {
      register_args_list_[reg_arg_index]->error_callback.Run(
          device::BluetoothAdvertisement::ErrorCode::
              INVALID_ADVERTISEMENT_ERROR_CODE);
      expected_result = BleSynchronizer::BluetoothAdvertisementResult::
          INVALID_ADVERTISEMENT_ERROR_CODE;
    }

    histogram_tester_.ExpectBucketCount(
        "InstantTethering.BluetoothAdvertisementRegistrationResult",
        expected_result, expected_registration_result_count);

    // Reset to make sure that this callback is never double-invoked.
    register_args_list_[reg_arg_index].reset();
    test_task_runner_->RunUntilIdle();
  }

  void OnAdvertisementRegistered(
      scoped_refptr<device::BluetoothAdvertisement> advertisement) {
    ++num_register_success_;
  }

  void OnErrorRegisteringAdvertisement(
      device::BluetoothAdvertisement::ErrorCode error_code) {
    ++num_register_error_;
  }

  void UnregisterAdvertisement() {
    synchronizer_->UnregisterAdvertisement(
        fake_advertisement_,
        base::Bind(
            &SecureChannelBleSynchronizerTest::OnAdvertisementUnregistered,
            base::Unretained(this)),
        base::Bind(&SecureChannelBleSynchronizerTest::
                       OnErrorUnregisteringAdvertisement,
                   base::Unretained(this)));
  }

  // If |success| is false, the error code defaults to
  // INVALID_ADVERTISEMENT_ERROR_CODE unless otherwise specified. If |success|
  // is true, |error_code| is simply ignored.
  void InvokeUnregisterCallback(
      bool success,
      size_t unreg_arg_index,
      size_t expected_unregistration_result_count,
      const device::BluetoothAdvertisement::ErrorCode& error_code = device::
          BluetoothAdvertisement::ErrorCode::INVALID_ADVERTISEMENT_ERROR_CODE) {
    EXPECT_TRUE(unregister_args_list_.size() >= unreg_arg_index);

    BleSynchronizer::BluetoothAdvertisementResult expected_result;
    if (success) {
      unregister_args_list_[unreg_arg_index]->callback.Run();
      expected_result = BleSynchronizer::BluetoothAdvertisementResult::SUCCESS;
    } else {
      unregister_args_list_[unreg_arg_index]->error_callback.Run(error_code);
      BleSynchronizer* derived_type =
          static_cast<BleSynchronizer*>(synchronizer_.get());
      expected_result =
          derived_type->BluetoothAdvertisementErrorCodeToResult(error_code);
    }

    histogram_tester_.ExpectBucketCount(
        "InstantTethering.BluetoothAdvertisementUnregistrationResult",
        expected_result, expected_unregistration_result_count);

    // Reset to make sure that this callback is never double-invoked.
    unregister_args_list_[unreg_arg_index].reset();
    test_task_runner_->RunUntilIdle();
  }

  void OnAdvertisementUnregistered() { ++num_unregister_success_; }

  void OnErrorUnregisteringAdvertisement(
      device::BluetoothAdvertisement::ErrorCode error_code) {
    ++num_unregister_error_;
  }

  void StartDiscoverySession() {
    synchronizer_->StartDiscoverySession(
        base::Bind(&SecureChannelBleSynchronizerTest::OnDiscoverySessionStarted,
                   base::Unretained(this)),
        base::Bind(
            &SecureChannelBleSynchronizerTest::OnErrorStartingDiscoverySession,
            base::Unretained(this)));
  }

  void InvokeStartDiscoveryCallback(
      bool success,
      size_t start_arg_index,
      size_t expected_start_discovery_result_count) {
    EXPECT_TRUE(start_discovery_args_list_.size() >= start_arg_index);

    if (success) {
      start_discovery_args_list_[start_arg_index]->callback.Run();
    } else {
      start_discovery_args_list_[start_arg_index]->error_callback.Run();
    }

    histogram_tester_.ExpectUniqueSample(
        "InstantTethering.BluetoothDiscoverySessionStarted", success ? 1 : 0,
        expected_start_discovery_result_count);

    // Reset to make sure that this callback is never double-invoked.
    start_discovery_args_list_[start_arg_index].reset();
    test_task_runner_->RunUntilIdle();
  }

  void OnDiscoverySessionStarted(
      std::unique_ptr<device::BluetoothDiscoverySession> discovery_session) {
    discovery_session_ = std::move(discovery_session);
    discovery_session_weak_ptr_factory_ = std::make_unique<
        base::WeakPtrFactory<device::BluetoothDiscoverySession>>(
        discovery_session_.get());
    ++num_start_success_;
  }

  void OnErrorStartingDiscoverySession() { ++num_start_error_; }

  void StopDiscoverySession(
      base::WeakPtr<device::BluetoothDiscoverySession> discovery_session) {
    synchronizer_->StopDiscoverySession(
        discovery_session,
        base::Bind(&SecureChannelBleSynchronizerTest::OnDiscoverySessionStopped,
                   base::Unretained(this)),
        base::Bind(
            &SecureChannelBleSynchronizerTest::OnErrorStoppingDiscoverySession,
            base::Unretained(this)));
  }

  void InvokeStopDiscoveryCallback(
      size_t stop_arg_index,
      size_t expected_stop_discovery_result_count) {
    EXPECT_TRUE(stop_discovery_args_list_.size() >= stop_arg_index);

    stop_discovery_args_list_[stop_arg_index]->callback.Run();

    histogram_tester_.ExpectUniqueSample(
        "InstantTethering.BluetoothDiscoverySessionStopped", 1,
        expected_stop_discovery_result_count);

    // Reset to make sure that this callback is never double-invoked.
    stop_discovery_args_list_[stop_arg_index].reset();
    test_task_runner_->RunUntilIdle();
  }

  void OnDiscoverySessionStopped() {
    discovery_session_.reset();
    ++num_stop_success_;
  }

  void OnErrorStoppingDiscoverySession() { ++num_stop_error_; }

  void FireTimer() {
    EXPECT_TRUE(mock_timer_->IsRunning());
    mock_timer_->Fire();
  }

  void OnUnregisterCalled(
      const device::BluetoothAdvertisement::SuccessCallback& callback,
      const device::BluetoothAdvertisement::ErrorCallback& error_callback) {
    unregister_args_list_.emplace_back(base::WrapUnique(
        new UnregisterAdvertisementArgs(callback, error_callback)));
  }

  void OnStopScan(
      device::BluetoothAdapter::DiscoverySessionResultCallback callback) {
    auto repeating_callback =
        base::AdaptCallbackForRepeating(std::move(callback));
    stop_discovery_args_list_.emplace_back(
        base::WrapUnique(new StopDiscoverySessionArgs(
            base::BindRepeating(
                repeating_callback, /*is_error=*/false,
                device::UMABluetoothDiscoverySessionOutcome::SUCCESS),
            base::BindRepeating(
                repeating_callback,
                /*is_error=*/true,
                device::UMABluetoothDiscoverySessionOutcome::UNKNOWN))));
  }

  base::test::TaskEnvironment task_environment_;
  const scoped_refptr<FakeBluetoothAdvertisement> fake_advertisement_;

  scoped_refptr<NiceMock<MockBluetoothAdapterWithAdvertisements>> mock_adapter_;

  base::MockOneShotTimer* mock_timer_;
  base::SimpleTestClock test_clock_;
  scoped_refptr<base::TestSimpleTaskRunner> test_task_runner_;

  std::vector<std::unique_ptr<RegisterAdvertisementArgs>> register_args_list_;
  std::vector<std::unique_ptr<UnregisterAdvertisementArgs>>
      unregister_args_list_;
  std::vector<std::unique_ptr<StartDiscoverySessionArgs>>
      start_discovery_args_list_;
  std::vector<std::unique_ptr<StopDiscoverySessionArgs>>
      stop_discovery_args_list_;

  std::unique_ptr<device::BluetoothDiscoverySession> discovery_session_;
  std::unique_ptr<base::WeakPtrFactory<device::BluetoothDiscoverySession>>
      discovery_session_weak_ptr_factory_;

  int num_register_success_;
  int num_register_error_;
  int num_unregister_success_;
  int num_unregister_error_;
  int num_start_success_;
  int num_start_error_;
  int num_stop_success_;
  int num_stop_error_;

  bool stopped_callback_called_;

  std::unique_ptr<BleSynchronizerBase> synchronizer_;

  base::HistogramTester histogram_tester_;

 private:
  DISALLOW_COPY_AND_ASSIGN(SecureChannelBleSynchronizerTest);
};

TEST_F(SecureChannelBleSynchronizerTest, TestRegisterSuccess) {
  RegisterAdvertisement(kId1);
  InvokeRegisterCallback(true /* success */, kId1, 0u /* reg_arg_index */,
                         1u /* expected_registration_result_count */);
  EXPECT_EQ(1, num_register_success_);
}

TEST_F(SecureChannelBleSynchronizerTest, TestRegisterError) {
  RegisterAdvertisement(kId1);
  InvokeRegisterCallback(false /* success */, kId1, 0u /* reg_arg_index */,
                         1u /* expected_registration_result_count */);
  EXPECT_EQ(1, num_register_error_);
}

TEST_F(SecureChannelBleSynchronizerTest, TestUnregisterSuccess) {
  UnregisterAdvertisement();
  InvokeUnregisterCallback(true /* success */, 0u /* reg_arg_index */,
                           1 /* expected_unregistration_result_count */);
  EXPECT_EQ(1, num_unregister_success_);
}

TEST_F(SecureChannelBleSynchronizerTest, TestUnregisterError) {
  UnregisterAdvertisement();
  InvokeUnregisterCallback(false /* success */, 0u /* reg_arg_index */,
                           1 /* expected_unregistration_result_count */);
  EXPECT_EQ(1, num_unregister_error_);
}

TEST_F(SecureChannelBleSynchronizerTest,
       TestUnregisterError_AdvertisementDoesNotExist) {
  UnregisterAdvertisement();
  InvokeUnregisterCallback(false /* success */, 0u /* reg_arg_index */,
                           1 /* expected_unregistration_result_count */,
                           device::BluetoothAdvertisement::ErrorCode::
                               ERROR_ADVERTISEMENT_DOES_NOT_EXIST);
  EXPECT_EQ(1, num_unregister_success_);
  EXPECT_EQ(0, num_unregister_error_);
}

TEST_F(SecureChannelBleSynchronizerTest, TestStartSuccess) {
  StartDiscoverySession();
  InvokeStartDiscoveryCallback(true /* success */, 0u /* reg_arg_index */,
                               1 /* expected_start_discovery_result_count */);
  EXPECT_EQ(1, num_start_success_);
}

TEST_F(SecureChannelBleSynchronizerTest, TestStartError) {
  StartDiscoverySession();
  InvokeStartDiscoveryCallback(false /* success */, 0u /* reg_arg_index */,
                               1 /* expected_start_discovery_result_count */);
  EXPECT_EQ(1, num_start_error_);
}

TEST_F(SecureChannelBleSynchronizerTest, TestStopSuccess) {
  StartDiscoverySession();
  InvokeStartDiscoveryCallback(true /* success */, 0u /* reg_arg_index */,
                               1 /* expected_start_discovery_result_count */);
  EXPECT_EQ(1, num_start_success_);

  StopDiscoverySession(discovery_session_weak_ptr_factory_->GetWeakPtr());

  // Advance the clock and fire the timer. This should result in the next
  // command being executed.
  test_clock_.Advance(TimeDeltaMillis(kTimeBetweenEachCommandMs));
  mock_timer_->Fire();

  InvokeStopDiscoveryCallback(0u /* unreg_arg_index */,
                              1 /* expected_stop_discovery_result_count */);
  EXPECT_EQ(1, num_stop_success_);
}

TEST_F(SecureChannelBleSynchronizerTest, TestStop_DeletedDiscoverySession) {
  // Simulate an invalidated WeakPtr being processed.
  StopDiscoverySession(base::WeakPtr<device::BluetoothDiscoverySession>());
  RegisterAdvertisement(kId1);

  // Stop() should not have been called.
  EXPECT_TRUE(stop_discovery_args_list_.empty());

  // The RegisterAdvertisement() command should be sent without the need for a
  // delay, since the previous command was not actually sent.
  InvokeRegisterCallback(true /* success */, kId1, 0u /* reg_arg_index */,
                         1u /* expected_registration_result_count */);
  EXPECT_EQ(1, num_register_success_);
}

TEST_F(SecureChannelBleSynchronizerTest, TestThrottling) {
  RegisterAdvertisement(kId1);
  InvokeRegisterCallback(true /* success */, kId1, 0u /* reg_arg_index */,
                         1u /* expected_registration_result_count */);
  EXPECT_EQ(1, num_register_success_);

  // Advance to one millisecond before the limit.
  test_clock_.Advance(TimeDeltaMillis(kTimeBetweenEachCommandMs - 1));
  UnregisterAdvertisement();

  // Should still be empty since it should have been throttled, and the timer
  // should be running.
  EXPECT_TRUE(unregister_args_list_.empty());
  EXPECT_TRUE(mock_timer_->IsRunning());

  // Advance the clock and fire the timer. This should result in the next
  // command being executed.
  test_clock_.Advance(TimeDeltaMillis(kTimeBetweenEachCommandMs));
  mock_timer_->Fire();

  InvokeUnregisterCallback(true /* success */, 0u /* reg_arg_index */,
                           1 /* expected_unregistration_result_count */);
  EXPECT_EQ(1, num_unregister_success_);

  // Now, register 2 advertisements at the same time.
  RegisterAdvertisement(kId2);
  RegisterAdvertisement(kId3);

  // Should still only have the original register command..
  EXPECT_EQ(1u, register_args_list_.size());

  // Advance the clock and fire the timer. This should result in the next
  // command being executed.
  test_clock_.Advance(TimeDeltaMillis(kTimeBetweenEachCommandMs));
  mock_timer_->Fire();

  EXPECT_EQ(2u, register_args_list_.size());
  InvokeRegisterCallback(false /* success */, kId2, 1u /* reg_arg_index */,
                         1u /* expected_registration_result_count */);
  EXPECT_EQ(1, num_register_error_);

  // Advance the clock and fire the timer. This should result in the next
  // command being executed.
  test_clock_.Advance(TimeDeltaMillis(kTimeBetweenEachCommandMs));
  mock_timer_->Fire();

  EXPECT_EQ(3u, register_args_list_.size());
  InvokeRegisterCallback(true /* success */, kId3, 2u /* reg_arg_index */,
                         2u /* expected_registration_result_count */);
  EXPECT_EQ(2, num_register_success_);

  // Advance the clock before doing anything else. The next request should not
  // be throttled.
  EXPECT_FALSE(mock_timer_->IsRunning());
  test_clock_.Advance(TimeDeltaMillis(kTimeBetweenEachCommandMs));

  UnregisterAdvertisement();
  InvokeUnregisterCallback(false /* success */, 1u /* reg_arg_index */,
                           1 /* expected_unregistration_result_count */);
  EXPECT_EQ(1, num_unregister_error_);
}

}  // namespace secure_channel

}  // namespace chromeos
