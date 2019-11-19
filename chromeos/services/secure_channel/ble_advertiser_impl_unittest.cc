// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chromeos/services/secure_channel/ble_advertiser_impl.h"

#include <memory>
#include <sstream>
#include <vector>

#include "base/bind.h"
#include "base/test/gtest_util.h"
#include "base/test/task_environment.h"
#include "base/test/test_simple_task_runner.h"
#include "chromeos/services/secure_channel/error_tolerant_ble_advertisement_impl.h"
#include "chromeos/services/secure_channel/fake_ble_advertiser.h"
#include "chromeos/services/secure_channel/fake_ble_service_data_helper.h"
#include "chromeos/services/secure_channel/fake_ble_synchronizer.h"
#include "chromeos/services/secure_channel/fake_error_tolerant_ble_advertisement.h"
#include "chromeos/services/secure_channel/fake_one_shot_timer.h"
#include "chromeos/services/secure_channel/fake_timer_factory.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace chromeos {

namespace secure_channel {

namespace {

class FakeErrorTolerantBleAdvertisementFactory
    : public ErrorTolerantBleAdvertisementImpl::Factory {
 public:
  FakeErrorTolerantBleAdvertisementFactory(
      BleServiceDataHelper* ble_service_data_helper,
      BleSynchronizerBase* ble_synchronizer_base)
      : ble_service_data_helper_(ble_service_data_helper),
        ble_synchronizer_base_(ble_synchronizer_base) {}
  ~FakeErrorTolerantBleAdvertisementFactory() override = default;

  const base::Optional<DeviceIdPair>& last_created_device_id_pair() const {
    return last_created_device_id_pair_;
  }

  base::flat_map<DeviceIdPair, FakeErrorTolerantBleAdvertisement*>&
  device_id_pair_to_active_advertisement_map() {
    return device_id_pair_to_active_advertisement_map_;
  }

  size_t num_instances_created() const { return num_instances_created_; }

 private:
  // ErrorTolerantBleAdvertisementImpl::Factory:
  std::unique_ptr<ErrorTolerantBleAdvertisement> BuildInstance(
      const DeviceIdPair& device_id_pair,
      std::unique_ptr<DataWithTimestamp> advertisement_data,
      BleSynchronizerBase* ble_synchronizer) override {
    EXPECT_EQ(*ble_service_data_helper_->GenerateForegroundAdvertisement(
                  device_id_pair),
              *advertisement_data);
    EXPECT_EQ(ble_synchronizer_base_, ble_synchronizer);

    ++num_instances_created_;

    auto fake_advertisement =
        std::make_unique<FakeErrorTolerantBleAdvertisement>(
            device_id_pair,
            base::BindOnce(
                &FakeErrorTolerantBleAdvertisementFactory::OnInstanceDeleted,
                base::Unretained(this)));

    device_id_pair_to_active_advertisement_map_[fake_advertisement
                                                    ->device_id_pair()] =
        fake_advertisement.get();
    last_created_device_id_pair_ = fake_advertisement->device_id_pair();

    return fake_advertisement;
  }

  void OnInstanceDeleted(const DeviceIdPair& device_id_pair) {
    size_t num_deleted =
        device_id_pair_to_active_advertisement_map_.erase(device_id_pair);
    EXPECT_EQ(1u, num_deleted);
  }

  BleServiceDataHelper* ble_service_data_helper_;
  BleSynchronizerBase* ble_synchronizer_base_;

  base::Optional<DeviceIdPair> last_created_device_id_pair_;
  base::flat_map<DeviceIdPair, FakeErrorTolerantBleAdvertisement*>
      device_id_pair_to_active_advertisement_map_;
  size_t num_instances_created_ = 0u;

  DISALLOW_COPY_AND_ASSIGN(FakeErrorTolerantBleAdvertisementFactory);
};

const int64_t kDefaultStartTimestamp = 1337;
const int64_t kDefaultEndTimestamp = 13337;

}  // namespace

class SecureChannelBleAdvertiserImplTest : public testing::Test {
 protected:
  SecureChannelBleAdvertiserImplTest() = default;
  ~SecureChannelBleAdvertiserImplTest() override = default;

  // testing::Test:
  void SetUp() override {
    fake_delegate_ = std::make_unique<FakeBleAdvertiserDelegate>();
    fake_ble_service_data_helper_ =
        std::make_unique<FakeBleServiceDataHelper>();
    fake_ble_synchronizer_ = std::make_unique<FakeBleSynchronizer>();
    fake_timer_factory_ = std::make_unique<FakeTimerFactory>();

    fake_advertisement_factory_ =
        std::make_unique<FakeErrorTolerantBleAdvertisementFactory>(
            fake_ble_service_data_helper_.get(), fake_ble_synchronizer_.get());
    ErrorTolerantBleAdvertisementImpl::Factory::SetFactoryForTesting(
        fake_advertisement_factory_.get());

    test_runner_ = base::MakeRefCounted<base::TestSimpleTaskRunner>();

    advertiser_ = BleAdvertiserImpl::Factory::Get()->BuildInstance(
        fake_delegate_.get(), fake_ble_service_data_helper_.get(),
        fake_ble_synchronizer_.get(), fake_timer_factory_.get(), test_runner_);
  }

  void TearDown() override {
    ErrorTolerantBleAdvertisementImpl::Factory::SetFactoryForTesting(nullptr);

    // Ensure that all slot ended delegate callbacks were verified.
    if (highest_slot_ended_delegate_index_verified_) {
      EXPECT_EQ(*highest_slot_ended_delegate_index_verified_ + 1u,
                fake_delegate_->slot_ended_events().size());
    } else {
      EXPECT_TRUE(fake_delegate_->slot_ended_events().empty());
    }

    // Ensure that all failed advertisement delegate callbacks were verified.
    if (highest_failed_advertisement_delegate_index_verified_) {
      EXPECT_EQ(*highest_failed_advertisement_delegate_index_verified_ + 1u,
                fake_delegate_->advertisement_generation_failures().size());
    } else {
      EXPECT_TRUE(fake_delegate_->advertisement_generation_failures().empty());
    }

    // Ensure that there are no more active timers/advertisements.
    EXPECT_TRUE(fake_timer_factory_->id_to_active_one_shot_timer_map().empty());
    EXPECT_TRUE(fake_advertisement_factory_
                    ->device_id_pair_to_active_advertisement_map()
                    .empty());
  }

  FakeErrorTolerantBleAdvertisement* GetLastCreatedAdvertisement(
      const DeviceIdPair& expected_device_id_pair) {
    EXPECT_EQ(expected_device_id_pair,
              *fake_advertisement_factory_->last_created_device_id_pair());
    auto* fake_advertisement =
        fake_advertisement_factory_
            ->device_id_pair_to_active_advertisement_map()
                [expected_device_id_pair];

    EXPECT_NE(fake_advertisement->id(), last_fetched_advertisement_id_);
    last_fetched_advertisement_id_ = fake_advertisement->id();

    // The advertisement should not yet be stopped.
    EXPECT_FALSE(fake_advertisement->HasBeenStopped());

    return fake_advertisement;
  }

  FakeOneShotTimer* GetLastCreatedTimer() {
    EXPECT_FALSE(
        fake_timer_factory_->id_for_last_created_one_shot_timer().is_empty());
    auto* fake_timer =
        fake_timer_factory_->id_to_active_one_shot_timer_map()
            [fake_timer_factory_->id_for_last_created_one_shot_timer()];

    EXPECT_NE(fake_timer->id(), last_fetched_timer_id_);
    last_fetched_timer_id_ = fake_timer->id();

    // The timer should have been started.
    EXPECT_TRUE(fake_timer->IsRunning());
    EXPECT_EQ(base::TimeDelta::FromSeconds(
                  BleAdvertiserImpl::kNumSecondsPerAdvertisementTimeslot),
              fake_timer->GetCurrentDelay());

    return fake_timer;
  }

  void AddAdvertisementRequest(const DeviceIdPair& request,
                               ConnectionPriority connection_priority,
                               bool should_advertisement_succeed = true) {
    if (should_advertisement_succeed) {
      // Generate fake service data using the two device IDs.
      std::stringstream ss;
      ss << request.remote_device_id() << "+" << request.local_device_id();
      fake_ble_service_data_helper_->SetAdvertisement(
          request,
          DataWithTimestamp(ss.str() /* data */, kDefaultStartTimestamp,
                            kDefaultEndTimestamp));
    }

    advertiser_->AddAdvertisementRequest(request, connection_priority);
  }

  void VerifyDelegateNotifiedOnAdvertisingSlotEnded(
      const DeviceIdPair& expected_request,
      bool expected_replaced_by_higher_priority_advertisement,
      size_t expected_index) {
    const std::vector<FakeBleAdvertiserDelegate::SlotEndedEvent>&
        slot_ended_events = fake_delegate_->slot_ended_events();

    EXPECT_EQ(expected_index + 1, slot_ended_events.size());
    EXPECT_EQ(expected_request, slot_ended_events.back().first);
    EXPECT_EQ(expected_replaced_by_higher_priority_advertisement,
              slot_ended_events.back().second);

    highest_slot_ended_delegate_index_verified_ = expected_index;
  }

  void VerifyDelegateNotifiedOnFailureToGenerateAdvertisement(
      const DeviceIdPair& expected_request,
      size_t expected_index) {
    const std::vector<DeviceIdPair>& advertisement_generation_failures =
        fake_delegate_->advertisement_generation_failures();

    EXPECT_EQ(expected_index + 1, advertisement_generation_failures.size());
    EXPECT_EQ(expected_request, advertisement_generation_failures.back());

    highest_failed_advertisement_delegate_index_verified_ = expected_index;
  }

  size_t GetNumAdvertisementsCreated() {
    return fake_advertisement_factory_->num_instances_created();
  }

  size_t GetNumTimersCreated() {
    return fake_timer_factory_->num_instances_created();
  }

  size_t GetNumSlotEndedDelegateCallbacks() {
    return fake_delegate_->slot_ended_events().size();
  }

  size_t GetNumFailedAdvertisementDelegateCallbacks() {
    return fake_delegate_->advertisement_generation_failures().size();
  }

  void VerifyAdvertisementStopped(
      FakeErrorTolerantBleAdvertisement* advertisement,
      bool should_finish_stopping) {
    EXPECT_TRUE(advertisement->HasBeenStopped());

    if (should_finish_stopping)
      advertisement->InvokeStopCallback();
  }

  base::TestSimpleTaskRunner* test_runner() { return test_runner_.get(); }
  BleAdvertiser* advertiser() { return advertiser_.get(); }
  FakeBleServiceDataHelper* fake_ble_service_data_helper() {
    return fake_ble_service_data_helper_.get();
  }

 private:
  base::test::TaskEnvironment task_environment_;

  std::unique_ptr<FakeBleAdvertiserDelegate> fake_delegate_;
  std::unique_ptr<FakeBleServiceDataHelper> fake_ble_service_data_helper_;
  std::unique_ptr<FakeBleSynchronizer> fake_ble_synchronizer_;
  std::unique_ptr<FakeTimerFactory> fake_timer_factory_;

  std::unique_ptr<FakeErrorTolerantBleAdvertisementFactory>
      fake_advertisement_factory_;

  base::UnguessableToken last_fetched_advertisement_id_;
  base::UnguessableToken last_fetched_timer_id_;
  base::Optional<size_t> highest_slot_ended_delegate_index_verified_;
  base::Optional<size_t> highest_failed_advertisement_delegate_index_verified_;

  scoped_refptr<base::TestSimpleTaskRunner> test_runner_;

  std::unique_ptr<BleAdvertiser> advertiser_;

  DISALLOW_COPY_AND_ASSIGN(SecureChannelBleAdvertiserImplTest);
};

TEST_F(SecureChannelBleAdvertiserImplTest, OneAdvertisement_TimerFires) {
  DeviceIdPair pair("remoteDeviceId", "localDeviceId");
  AddAdvertisementRequest(pair, ConnectionPriority::kLow);

  FakeErrorTolerantBleAdvertisement* advertisement;
  FakeOneShotTimer* timer;

  // Simulate 5 consecutive timeouts, followed by removing the advertisement.
  const size_t kNumTimerFires = 5;
  for (size_t i = 0; i < kNumTimerFires; ++i) {
    advertisement = GetLastCreatedAdvertisement(pair);
    timer = GetLastCreatedTimer();

    timer->Fire();
    VerifyDelegateNotifiedOnAdvertisingSlotEnded(
        pair, false /* expected_replaced_by_higher_priority_advertisement */,
        i /* expected_index */);
    VerifyAdvertisementStopped(advertisement,
                               true /* should_finish_stopping */);
  }

  advertisement = GetLastCreatedAdvertisement(pair);
  advertiser()->RemoveAdvertisementRequest(pair);
  advertisement->InvokeStopCallback();
}

TEST_F(SecureChannelBleAdvertiserImplTest, OneAdvertisement_UpdatePriority) {
  DeviceIdPair pair("remoteDeviceId", "localDeviceId");
  AddAdvertisementRequest(pair, ConnectionPriority::kLow);
  FakeErrorTolerantBleAdvertisement* advertisement =
      GetLastCreatedAdvertisement(pair);

  // No delegate callbacks yet.
  EXPECT_EQ(0u, GetNumSlotEndedDelegateCallbacks());

  // Updating the priority should not trigger any delegate callbacks.
  advertiser()->UpdateAdvertisementRequestPriority(pair,
                                                   ConnectionPriority::kMedium);
  EXPECT_EQ(0u, GetNumSlotEndedDelegateCallbacks());
  advertiser()->UpdateAdvertisementRequestPriority(pair,
                                                   ConnectionPriority::kHigh);
  EXPECT_EQ(0u, GetNumSlotEndedDelegateCallbacks());

  advertiser()->RemoveAdvertisementRequest(pair);
  advertisement->InvokeStopCallback();
}

TEST_F(SecureChannelBleAdvertiserImplTest,
       OneAdvertisement_AsyncAdvertisement) {
  DeviceIdPair pair("remoteDeviceId", "localDeviceId");
  AddAdvertisementRequest(pair, ConnectionPriority::kLow);

  FakeErrorTolerantBleAdvertisement* advertisement =
      GetLastCreatedAdvertisement(pair);
  FakeOneShotTimer* timer = GetLastCreatedTimer();

  // Fire the timer and verify that the advertisement was stopped, but do not
  // complete the asynchronous stopping flow.
  timer->Fire();
  VerifyDelegateNotifiedOnAdvertisingSlotEnded(
      pair, false /* expected_replaced_by_higher_priority_advertisement */,
      0u /* expected_index */);
  VerifyAdvertisementStopped(advertisement, false /* should_finish_stopping */);

  // A new timer should have been created for the next timeslot, but the
  // original advertisement is still in the process of stopping.
  timer = GetLastCreatedTimer();

  // Simulate another timeout before the first advertisement has stopped.
  timer->Fire();
  VerifyDelegateNotifiedOnAdvertisingSlotEnded(
      pair, false /* expected_replaced_by_higher_priority_advertisement */,
      1u /* expected_index */);
  VerifyAdvertisementStopped(advertisement, false /* should_finish_stopping */);

  advertiser()->RemoveAdvertisementRequest(pair);
  advertisement->InvokeStopCallback();
}

TEST_F(SecureChannelBleAdvertiserImplTest, TwoAdvertisements_TimerFires) {
  DeviceIdPair pair_1("remoteDeviceId1", "localDeviceId1");
  DeviceIdPair pair_2("remoteDeviceId2", "localDeviceId2");

  AddAdvertisementRequest(pair_1, ConnectionPriority::kLow);
  FakeErrorTolerantBleAdvertisement* advertisement_1 =
      GetLastCreatedAdvertisement(pair_1);
  FakeOneShotTimer* timer_1 = GetLastCreatedTimer();

  AddAdvertisementRequest(pair_2, ConnectionPriority::kLow);
  FakeErrorTolerantBleAdvertisement* advertisement_2 =
      GetLastCreatedAdvertisement(pair_2);
  FakeOneShotTimer* timer_2 = GetLastCreatedTimer();

  // Simulate 5 consecutive timeouts, followed by removing the advertisement.
  const size_t kNumTimerFires = 5;
  for (size_t i = 0; i < kNumTimerFires; ++i) {
    timer_1->Fire();
    VerifyDelegateNotifiedOnAdvertisingSlotEnded(
        pair_1, false /* expected_replaced_by_higher_priority_advertisement */,
        2u * i /* expected_index */);
    VerifyAdvertisementStopped(advertisement_1,
                               true /* should_finish_stopping */);
    advertisement_1 = GetLastCreatedAdvertisement(pair_1);
    timer_1 = GetLastCreatedTimer();

    timer_2->Fire();
    VerifyDelegateNotifiedOnAdvertisingSlotEnded(
        pair_2, false /* expected_replaced_by_higher_priority_advertisement */,
        2u * i + 1u /* expected_index */);
    VerifyAdvertisementStopped(advertisement_2,
                               true /* should_finish_stopping */);
    advertisement_2 = GetLastCreatedAdvertisement(pair_2);
    timer_2 = GetLastCreatedTimer();
  }

  advertiser()->RemoveAdvertisementRequest(pair_1);
  advertisement_1->InvokeStopCallback();

  advertiser()->RemoveAdvertisementRequest(pair_2);
  advertisement_2->InvokeStopCallback();
}

TEST_F(SecureChannelBleAdvertiserImplTest, TwoAdvertisements_UpdatePriority) {
  DeviceIdPair pair_1("remoteDeviceId1", "localDeviceId1");
  DeviceIdPair pair_2("remoteDeviceId2", "localDeviceId2");

  AddAdvertisementRequest(pair_1, ConnectionPriority::kLow);
  FakeErrorTolerantBleAdvertisement* advertisement_1 =
      GetLastCreatedAdvertisement(pair_1);

  AddAdvertisementRequest(pair_2, ConnectionPriority::kLow);
  FakeErrorTolerantBleAdvertisement* advertisement_2 =
      GetLastCreatedAdvertisement(pair_2);

  // No delegate callbacks yet.
  EXPECT_EQ(0u, GetNumSlotEndedDelegateCallbacks());

  // Updating the priority should not trigger any delegate callbacks.
  advertiser()->UpdateAdvertisementRequestPriority(pair_1,
                                                   ConnectionPriority::kMedium);
  EXPECT_EQ(0u, GetNumSlotEndedDelegateCallbacks());
  advertiser()->UpdateAdvertisementRequestPriority(pair_1,
                                                   ConnectionPriority::kHigh);
  EXPECT_EQ(0u, GetNumSlotEndedDelegateCallbacks());
  advertiser()->UpdateAdvertisementRequestPriority(pair_2,
                                                   ConnectionPriority::kMedium);
  EXPECT_EQ(0u, GetNumSlotEndedDelegateCallbacks());
  advertiser()->UpdateAdvertisementRequestPriority(pair_2,
                                                   ConnectionPriority::kHigh);
  EXPECT_EQ(0u, GetNumSlotEndedDelegateCallbacks());

  advertiser()->RemoveAdvertisementRequest(pair_1);
  advertisement_1->InvokeStopCallback();

  advertiser()->RemoveAdvertisementRequest(pair_2);
  advertisement_2->InvokeStopCallback();
}

TEST_F(SecureChannelBleAdvertiserImplTest,
       TwoAdvertisements_AsyncAdvertisement) {
  DeviceIdPair pair_1("remoteDeviceId1", "localDeviceId1");
  DeviceIdPair pair_2("remoteDeviceId2", "localDeviceId2");

  AddAdvertisementRequest(pair_1, ConnectionPriority::kLow);
  FakeErrorTolerantBleAdvertisement* advertisement_1 =
      GetLastCreatedAdvertisement(pair_1);
  FakeOneShotTimer* timer_1 = GetLastCreatedTimer();

  AddAdvertisementRequest(pair_2, ConnectionPriority::kLow);
  FakeErrorTolerantBleAdvertisement* advertisement_2 =
      GetLastCreatedAdvertisement(pair_2);
  FakeOneShotTimer* timer_2 = GetLastCreatedTimer();

  // Fire the timer and verify that the advertisement was stopped, but do not
  // complete the asynchronous stopping flow.
  timer_1->Fire();
  VerifyDelegateNotifiedOnAdvertisingSlotEnded(
      pair_1, false /* expected_replaced_by_higher_priority_advertisement */,
      0u /* expected_index */);
  VerifyAdvertisementStopped(advertisement_1,
                             false /* should_finish_stopping */);

  // A new timer should have been created for the next timeslot, but the
  // original advertisement is still in the process of stopping.
  timer_1 = GetLastCreatedTimer();

  // Same thing for pair_2.
  timer_2->Fire();
  VerifyDelegateNotifiedOnAdvertisingSlotEnded(
      pair_2, false /* expected_replaced_by_higher_priority_advertisement */,
      1u /* expected_index */);
  VerifyAdvertisementStopped(advertisement_2,
                             false /* should_finish_stopping */);

  advertiser()->RemoveAdvertisementRequest(pair_1);
  advertisement_1->InvokeStopCallback();

  advertiser()->RemoveAdvertisementRequest(pair_2);
  advertisement_2->InvokeStopCallback();
}

TEST_F(SecureChannelBleAdvertiserImplTest,
       ManyAdvertisements_ComprehensiveTest) {
  DeviceIdPair pair_1("remoteDeviceId1", "localDeviceId1");
  DeviceIdPair pair_2("remoteDeviceId2", "localDeviceId2");
  DeviceIdPair pair_3("remoteDeviceId3", "localDeviceId3");
  DeviceIdPair pair_4("remoteDeviceId4", "localDeviceId4");

  AddAdvertisementRequest(pair_1, ConnectionPriority::kLow);
  FakeErrorTolerantBleAdvertisement* advertisement_1 =
      GetLastCreatedAdvertisement(pair_1);
  FakeOneShotTimer* timer_1 = GetLastCreatedTimer();

  AddAdvertisementRequest(pair_2, ConnectionPriority::kLow);
  FakeErrorTolerantBleAdvertisement* advertisement_2 =
      GetLastCreatedAdvertisement(pair_2);
  FakeOneShotTimer* timer_2 = GetLastCreatedTimer();

  // Status: pair_1 - low (active, slot 1)
  //         pair_2 - low (active, slot 2)

  // Add pair_3 as a low-priority request. Since this priority is not higher
  // than the two which occupy the active advertisements, this should not result
  // in any change.
  AddAdvertisementRequest(pair_3, ConnectionPriority::kLow);
  EXPECT_EQ(2u, GetNumAdvertisementsCreated());
  EXPECT_EQ(2u, GetNumTimersCreated());
  EXPECT_EQ(0u, GetNumSlotEndedDelegateCallbacks());

  // Now, update pair_3's priority to medium. This should trigger pair_3 to take
  // pair_1's spot in the active advertisements list.
  advertiser()->UpdateAdvertisementRequestPriority(pair_3,
                                                   ConnectionPriority::kMedium);

  // A new timer should have been created, but no new advertisement, since
  // stopping is asynchronous.
  EXPECT_EQ(2u, GetNumAdvertisementsCreated());
  EXPECT_EQ(3u, GetNumTimersCreated());
  timer_1 = GetLastCreatedTimer();
  VerifyDelegateNotifiedOnAdvertisingSlotEnded(
      pair_1, true /* expected_replaced_by_higher_priority_advertisement */,
      0u /* expected_index */);

  // Status: pair_1 - low
  //         pair_2 - low (active, slot 2)
  //         pair_3 - medium (active, slot 1)

  // Finish stopping the advertisement which previously belonged to pair_1. This
  // should cause a new advertisement for pair_3 to be created.
  advertisement_1->InvokeStopCallback();
  EXPECT_EQ(3u, GetNumAdvertisementsCreated());
  EXPECT_EQ(3u, GetNumTimersCreated());
  advertisement_1 = GetLastCreatedAdvertisement(pair_3);

  // Simulate pair_2's timeslot ending; since pair_1 is also low-priority, it
  // is expected to take pair_2's place.
  timer_2->Fire();
  VerifyDelegateNotifiedOnAdvertisingSlotEnded(
      pair_2, false /* expected_replaced_by_higher_priority_advertisement */,
      1u /* expected_index */);
  VerifyAdvertisementStopped(advertisement_2,
                             true /* should_finish_stopping */);
  EXPECT_EQ(4u, GetNumAdvertisementsCreated());
  EXPECT_EQ(4u, GetNumTimersCreated());
  advertisement_2 = GetLastCreatedAdvertisement(pair_1);
  timer_2 = GetLastCreatedTimer();

  // Status: pair_1 - low (active, slot 2)
  //         pair_2 - low
  //         pair_3 - medium (active, slot 1)

  // Add pair_4 as a high-priority request. This should trigger pair_4 to take
  // pair_1's spot, since pair_1 is only low-priority (the other active request
  // is pair_3, which has medium priority).
  AddAdvertisementRequest(pair_4, ConnectionPriority::kHigh);
  EXPECT_EQ(4u, GetNumAdvertisementsCreated());
  EXPECT_EQ(5u, GetNumTimersCreated());
  VerifyDelegateNotifiedOnAdvertisingSlotEnded(
      pair_1, true /* expected_replaced_by_higher_priority_advertisement */,
      2u /* expected_index */);
  timer_2 = GetLastCreatedTimer();
  advertisement_2->InvokeStopCallback();
  EXPECT_EQ(5u, GetNumAdvertisementsCreated());
  advertisement_2 = GetLastCreatedAdvertisement(pair_4);

  // Status: pair_1 - low
  //         pair_2 - low
  //         pair_3 - medium (active, slot 1)
  //         pair_4 - high (active, slot 2)

  // Update pair_1's priority to medium. Since the two active requests (pair_3
  // with medium priority and pair_4 with high priority) are both >= pair_1's
  // new priority, this should not cause any changes.
  advertiser()->UpdateAdvertisementRequestPriority(pair_1,
                                                   ConnectionPriority::kMedium);
  EXPECT_EQ(5u, GetNumAdvertisementsCreated());
  EXPECT_EQ(5u, GetNumTimersCreated());
  EXPECT_EQ(3u, GetNumSlotEndedDelegateCallbacks());

  // Status: pair_1 - medium
  //         pair_2 - low
  //         pair_3 - medium (active, slot 1)
  //         pair_4 - high (active, slot 2)

  // Remove pair_4. This should trigger pair_1 to take its place. Note that the
  // delegate is not triggered, since the request was removed by the client.
  advertiser()->RemoveAdvertisementRequest(pair_4);
  EXPECT_EQ(5u, GetNumAdvertisementsCreated());
  EXPECT_EQ(6u, GetNumTimersCreated());
  EXPECT_EQ(3u, GetNumSlotEndedDelegateCallbacks());
  timer_2 = GetLastCreatedTimer();
  advertisement_2->InvokeStopCallback();
  EXPECT_EQ(6u, GetNumAdvertisementsCreated());
  advertisement_2 = GetLastCreatedAdvertisement(pair_1);

  // Status: pair_1 - medium (active, slot 2)
  //         pair_2 - low
  //         pair_3 - medium (active, slot 1)

  // Update pair_3's priority to high; this should not cause any changes.
  advertiser()->UpdateAdvertisementRequestPriority(pair_3,
                                                   ConnectionPriority::kHigh);
  EXPECT_EQ(6u, GetNumAdvertisementsCreated());
  EXPECT_EQ(6u, GetNumTimersCreated());
  EXPECT_EQ(3u, GetNumSlotEndedDelegateCallbacks());

  // Status: pair_1 - medium (active, slot 2)
  //         pair_2 - low
  //         pair_3 - high (active, slot 1)

  // Simulate pair_1's timeslot ending; since there are not pending requets with
  // a medium-or-higher priority, pair_1 should start up again.
  timer_2->Fire();
  VerifyDelegateNotifiedOnAdvertisingSlotEnded(
      pair_1, false /* expected_replaced_by_higher_priority_advertisement */,
      3u /* expected_index */);
  VerifyAdvertisementStopped(advertisement_2,
                             true /* should_finish_stopping */);
  EXPECT_EQ(7u, GetNumAdvertisementsCreated());
  EXPECT_EQ(7u, GetNumTimersCreated());
  advertisement_2 = GetLastCreatedAdvertisement(pair_1);
  timer_2 = GetLastCreatedTimer();

  // Status: pair_1 - medium (active, slot 2)
  //         pair_2 - low
  //         pair_3 - high (active, slot 1)

  // Remove pair_3. This should trigger pair_2 to take its place.
  advertiser()->RemoveAdvertisementRequest(pair_3);
  EXPECT_EQ(7u, GetNumAdvertisementsCreated());
  EXPECT_EQ(8u, GetNumTimersCreated());
  EXPECT_EQ(4u, GetNumSlotEndedDelegateCallbacks());
  timer_1 = GetLastCreatedTimer();
  advertisement_1->InvokeStopCallback();
  EXPECT_EQ(8u, GetNumAdvertisementsCreated());
  advertisement_1 = GetLastCreatedAdvertisement(pair_2);

  // Status: pair_1 - medium (active, slot 2)
  //         pair_2 - low (active, slot 1)

  // Remove pair_2. Since only pair_1 remains (and it already has an associated
  // timer and advertisement), no new timers/advertisements should be created.
  advertiser()->RemoveAdvertisementRequest(pair_2);
  EXPECT_EQ(8u, GetNumAdvertisementsCreated());
  EXPECT_EQ(8u, GetNumTimersCreated());
  EXPECT_EQ(4u, GetNumSlotEndedDelegateCallbacks());
  advertisement_1->InvokeStopCallback();
  EXPECT_EQ(8u, GetNumAdvertisementsCreated());

  // Status: pair_1 - medium (active, slot 2)

  // Remove pair_1. Nothing else remains.
  advertiser()->RemoveAdvertisementRequest(pair_1);
  EXPECT_EQ(8u, GetNumAdvertisementsCreated());
  EXPECT_EQ(8u, GetNumTimersCreated());
  advertisement_2->InvokeStopCallback();
}

TEST_F(SecureChannelBleAdvertiserImplTest, EdgeCases) {
  DeviceIdPair pair("remoteDeviceId", "localDeviceId");

  // Cannot update or remove an advertisement which was not added.
  EXPECT_DCHECK_DEATH(advertiser()->UpdateAdvertisementRequestPriority(
      pair, ConnectionPriority::kMedium));
  EXPECT_DCHECK_DEATH(advertiser()->RemoveAdvertisementRequest(pair));
}

TEST_F(SecureChannelBleAdvertiserImplTest, FailToGenerateAdvertisement_Simple) {
  DeviceIdPair pair("remoteDeviceId", "localDeviceId");

  AddAdvertisementRequest(pair, ConnectionPriority::kLow,
                          false /* should_advertisement_succeed */);

  test_runner()->RunUntilIdle();
  EXPECT_EQ(0u, GetNumAdvertisementsCreated());
  EXPECT_EQ(1u, GetNumTimersCreated());
  EXPECT_EQ(1u, GetNumFailedAdvertisementDelegateCallbacks());
  VerifyDelegateNotifiedOnFailureToGenerateAdvertisement(
      pair, 0u /* expected_index */);
}

TEST_F(SecureChannelBleAdvertiserImplTest,
       FailToGenerateAdvertisement_RerequestBeforeCallbackExecutes) {
  DeviceIdPair pair("remoteDeviceId", "localDeviceId");

  AddAdvertisementRequest(pair, ConnectionPriority::kLow,
                          false /* should_advertisement_succeed */);
  AddAdvertisementRequest(pair, ConnectionPriority::kLow,
                          true /* should_advertisement_succeed */);
  FakeErrorTolerantBleAdvertisement* advertisement =
      GetLastCreatedAdvertisement(pair);

  // Since another (valid) AddAdvertisementRequest() executes before the failure
  // delegate callback could go through, the delegate is never called.
  test_runner()->RunUntilIdle();
  EXPECT_EQ(1u, GetNumAdvertisementsCreated());
  EXPECT_EQ(2u, GetNumTimersCreated());
  EXPECT_EQ(0u, GetNumFailedAdvertisementDelegateCallbacks());

  advertiser()->RemoveAdvertisementRequest(pair);
  advertisement->InvokeStopCallback();
}

TEST_F(SecureChannelBleAdvertiserImplTest,
       FailToGenerateAdvertisement_UpdateBeforeCallbackExecutes) {
  DeviceIdPair pair("remoteDeviceId", "localDeviceId");

  AddAdvertisementRequest(pair, ConnectionPriority::kLow,
                          false /* should_advertisement_succeed */);
  advertiser()->UpdateAdvertisementRequestPriority(pair,
                                                   ConnectionPriority::kMedium);

  // Should not DCHECK since UpdateAdvertisementRequestPriority() should have
  // been a no-op.
  advertiser()->RemoveAdvertisementRequest(pair);

  // Should not notify the delegate since RemoveAdvertisementRequest() was
  // called.
  test_runner()->RunUntilIdle();
  EXPECT_EQ(0u, GetNumAdvertisementsCreated());
  EXPECT_EQ(1u, GetNumTimersCreated());
  EXPECT_EQ(0u, GetNumFailedAdvertisementDelegateCallbacks());
}

TEST_F(SecureChannelBleAdvertiserImplTest,
       FailToGenerateAdvertisement_RemoveAgainBeforeCallbackExecutes) {
  DeviceIdPair pair("remoteDeviceId", "localDeviceId");

  AddAdvertisementRequest(pair, ConnectionPriority::kLow,
                          false /* should_advertisement_succeed */);
  advertiser()->RemoveAdvertisementRequest(pair);

  // Should not notify the delegate since RemoveAdvertisementRequest() was
  // called.
  test_runner()->RunUntilIdle();
  EXPECT_EQ(0u, GetNumAdvertisementsCreated());
  EXPECT_EQ(1u, GetNumTimersCreated());
  EXPECT_EQ(0u, GetNumFailedAdvertisementDelegateCallbacks());
}

}  // namespace secure_channel

}  // namespace chromeos
