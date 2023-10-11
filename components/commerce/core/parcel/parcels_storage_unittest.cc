// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <string>

#include "base/check.h"
#include "base/functional/callback.h"
#include "base/run_loop.h"
#include "base/strings/stringprintf.h"
#include "base/test/scoped_feature_list.h"
#include "base/test/simple_test_clock.h"
#include "base/test/task_environment.h"
#include "components/commerce/core/parcel/parcels_storage.h"
#include "components/commerce/core/proto/parcel.pb.h"
#include "components/commerce/core/proto/parcel_tracking_db_content.pb.h"
#include "components/session_proto_db/session_proto_storage.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

using testing::_;
using testing::InSequence;

namespace {

const commerce::ParcelIdentifier::Carrier kCarrier1 =
    commerce::ParcelIdentifier::UPS;
const std::string kTrackingId1 = "abc";
const std::string kTrackingId2 = "xyz";
const commerce::ParcelStatus::ParcelState kDefaultState =
    commerce::ParcelStatus::NEW;

std::string GetStorageKey(commerce::ParcelIdentifier::Carrier carrier,
                          const std::string& tracking_id) {
  return base::StringPrintf("%d_%s", carrier, tracking_id.c_str());
}

commerce::ParcelStatus CreateParcelStatus(
    commerce::ParcelIdentifier::Carrier carrier,
    const std::string& tracking_id,
    commerce::ParcelStatus::ParcelState state) {
  commerce::ParcelStatus status;
  auto* identifier = status.mutable_parcel_identifier();
  identifier->set_tracking_id(tracking_id);
  identifier->set_carrier(carrier);
  status.set_parcel_state(state);
  return status;
}

parcel_tracking_db::ParcelTrackingContent CreateParcelTrackingContent(
    commerce::ParcelIdentifier::Carrier carrier,
    const std::string& tracking_id,
    commerce::ParcelStatus::ParcelState state) {
  parcel_tracking_db::ParcelTrackingContent content;
  content.set_key(GetStorageKey(carrier, tracking_id));
  auto* parcel_status = content.mutable_parcel_status();
  *parcel_status = CreateParcelStatus(carrier, tracking_id, state);
  return content;
}

std::vector<
    SessionProtoStorage<parcel_tracking_db::ParcelTrackingContent>::KeyAndValue>
MockDbLoadResponse() {
  parcel_tracking_db::ParcelTrackingContent content =
      CreateParcelTrackingContent(kCarrier1, kTrackingId1, kDefaultState);

  return std::vector<SessionProtoStorage<
      parcel_tracking_db::ParcelTrackingContent>::KeyAndValue>{
      {GetStorageKey(kCarrier1, kTrackingId1), content}};
}

void DoNothing(bool success) {}

class MockProtoStorage
    : public SessionProtoStorage<parcel_tracking_db::ParcelTrackingContent> {
 public:
  MockProtoStorage() = default;
  ~MockProtoStorage() override = default;

  MOCK_METHOD(void,
              LoadContentWithPrefix,
              (const std::string& key_prefix,
               SessionProtoStorage<parcel_tracking_db::ParcelTrackingContent>::
                   LoadCallback callback),
              (override));
  MOCK_METHOD(void,
              InsertContent,
              (const std::string& key,
               const parcel_tracking_db::ParcelTrackingContent& value,
               SessionProtoStorage<parcel_tracking_db::ParcelTrackingContent>::
                   OperationCallback callback),
              (override));
  MOCK_METHOD(void,
              DeleteOneEntry,
              (const std::string& key,
               SessionProtoStorage<parcel_tracking_db::ParcelTrackingContent>::
                   OperationCallback callback),
              (override));
  MOCK_METHOD(
      void,
      UpdateEntries,
      ((std::unique_ptr<std::vector<
            std::pair<std::string, parcel_tracking_db::ParcelTrackingContent>>>
            entries_to_update),
       std::unique_ptr<std::vector<std::string>> keys_to_remove,
       SessionProtoStorage<parcel_tracking_db::ParcelTrackingContent>::
           OperationCallback callback),
      (override));
  MOCK_METHOD(void,
              DeleteAllContent,
              (SessionProtoStorage<parcel_tracking_db::ParcelTrackingContent>::
                   OperationCallback callback),
              (override));
  MOCK_METHOD(
      void,
      LoadAllEntries,
      (SessionProtoStorage<
          parcel_tracking_db::ParcelTrackingContent>::LoadCallback callback),
      (override));
  MOCK_METHOD(void,
              LoadOneEntry,
              (const std::string& key,
               SessionProtoStorage<parcel_tracking_db::ParcelTrackingContent>::
                   LoadCallback callback),
              (override));
  MOCK_METHOD(void,
              PerformMaintenance,
              (const std::vector<std::string>& keys_to_keep,
               const std::string& key_substring_to_match,
               SessionProtoStorage<parcel_tracking_db::ParcelTrackingContent>::
                   OperationCallback callback),
              (override));
  MOCK_METHOD(void,
              DeleteContentWithPrefix,
              (const std::string& key_prefix,
               SessionProtoStorage<parcel_tracking_db::ParcelTrackingContent>::
                   OperationCallback callback),
              (override));
  MOCK_METHOD(void, Destroy, (), (const, override));

  void MockLoadAllResponse() {
    ON_CALL(*this, LoadAllEntries)
        .WillByDefault(
            [](SessionProtoStorage<parcel_tracking_db::ParcelTrackingContent>::
                   LoadCallback callback) {
              std::move(callback).Run(true, MockDbLoadResponse());
            });
  }

  void MockOperationResult(bool succeeded) {
    ON_CALL(*this, InsertContent)
        .WillByDefault(
            [succeeded](
                const std::string& key,
                const parcel_tracking_db::ParcelTrackingContent& value,
                SessionProtoStorage<parcel_tracking_db::ParcelTrackingContent>::
                    OperationCallback callback) {
              std::move(callback).Run(succeeded);
            });
    ON_CALL(*this, DeleteOneEntry)
        .WillByDefault(
            [succeeded](
                const std::string& key,
                SessionProtoStorage<parcel_tracking_db::ParcelTrackingContent>::
                    OperationCallback callback) {
              std::move(callback).Run(succeeded);
            });
    ON_CALL(*this, DeleteAllContent)
        .WillByDefault(
            [succeeded](
                SessionProtoStorage<parcel_tracking_db::ParcelTrackingContent>::
                    OperationCallback callback) {
              std::move(callback).Run(succeeded);
            });
  }
};

}  // namespace

namespace commerce {

class ParcelsStorageTest : public testing::Test {
 public:
  ParcelsStorageTest() = default;
  ~ParcelsStorageTest() override = default;

  void SetUp() override {
    base::Time fake_now;
    EXPECT_TRUE(base::Time::FromString("05/18/20 01:00:00 AM", &fake_now));
    clock_.SetNow(fake_now);
    proto_db_ = std::make_unique<MockProtoStorage>();
    proto_db_->MockLoadAllResponse();
    storage_ = std::make_unique<ParcelsStorage>(proto_db_.get(), &clock_);
    EXPECT_CALL(*proto_db_, LoadAllEntries(_));
    storage_->Init(base::BindOnce(&DoNothing));
  }

 protected:
  base::test::TaskEnvironment task_environment_;
  std::unique_ptr<MockProtoStorage> proto_db_;
  std::unique_ptr<ParcelsStorage> storage_;
  base::SimpleTestClock clock_;
};

TEST_F(ParcelsStorageTest, TestGetAllParcelTrackingContents) {
  auto all_parcels = storage_->GetAllParcelTrackingContents();
  ASSERT_EQ(1u, all_parcels->size());
  auto& parcel_1 = (*all_parcels)[0].parcel_status();
  ASSERT_EQ(kTrackingId1, parcel_1.parcel_identifier().tracking_id());
  ASSERT_EQ(kCarrier1, parcel_1.parcel_identifier().carrier());
  ASSERT_EQ(kDefaultState, parcel_1.parcel_state());
}

TEST_F(ParcelsStorageTest, TestDeleteAllParcelStatus) {
  EXPECT_CALL(*proto_db_, DeleteAllContent(_));
  storage_->DeleteAllParcelStatus(base::BindOnce(&DoNothing));
  task_environment_.RunUntilIdle();
  auto all_parcels = storage_->GetAllParcelTrackingContents();
  ASSERT_EQ(0u, all_parcels->size());
}

TEST_F(ParcelsStorageTest, TestDeleteParcelStatus) {
  EXPECT_CALL(*proto_db_, DeleteOneEntry(_, _)).Times(1);

  // Delete an invalid parcel tracking id.
  storage_->DeleteParcelStatus("xyz", base::BindOnce(&DoNothing));
  task_environment_.RunUntilIdle();

  auto all_parcels = storage_->GetAllParcelTrackingContents();
  ASSERT_EQ(1u, all_parcels->size());

  // Delete the tracking id in storage.
  storage_->DeleteParcelStatus(kTrackingId1, base::BindOnce(&DoNothing));
  task_environment_.RunUntilIdle();

  all_parcels = storage_->GetAllParcelTrackingContents();
  ASSERT_EQ(0u, all_parcels->size());
}

TEST_F(ParcelsStorageTest, TestDeleteMultipleParcelsStatus) {
  EXPECT_CALL(*proto_db_, UpdateEntries(_, _, _)).Times(2);

  std::vector<ParcelIdentifier> identifiers;

  ParcelIdentifier id;
  id.set_tracking_id(kTrackingId2);
  id.set_carrier(kCarrier1);
  identifiers.emplace_back(id);

  // Delete an invalid parcel tracking id.
  storage_->DeleteParcelsStatus(identifiers, base::BindOnce(&DoNothing));
  task_environment_.RunUntilIdle();
  auto all_parcels = storage_->GetAllParcelTrackingContents();
  ASSERT_EQ(1u, all_parcels->size());

  id.set_tracking_id(kTrackingId1);
  identifiers.emplace_back(id);
  // Delete the tracking id in storage.
  storage_->DeleteParcelsStatus(identifiers, base::BindOnce(&DoNothing));
  task_environment_.RunUntilIdle();

  all_parcels = storage_->GetAllParcelTrackingContents();
  ASSERT_EQ(0u, all_parcels->size());
}

TEST_F(ParcelsStorageTest, TestUpdateParcelStatus) {
  EXPECT_CALL(*proto_db_, UpdateEntries(_, _, _)).Times(1);

  std::vector<ParcelStatus> status;
  ParcelStatus status1 = CreateParcelStatus(kCarrier1, kTrackingId1,
                                            commerce::ParcelStatus::PICKED_UP);
  ParcelStatus status2 =
      CreateParcelStatus(kCarrier1, kTrackingId2, kDefaultState);
  status.emplace_back(status1);
  status.emplace_back(status2);

  // Delete the parcel identifier in storage.
  storage_->UpdateParcelStatus(status, base::BindOnce(&DoNothing));
  task_environment_.RunUntilIdle();

  auto all_parcels = storage_->GetAllParcelTrackingContents();
  ASSERT_EQ(2u, all_parcels->size());
  std::map<std::string, ParcelStatus> status_map;
  for (int i = 0; i < 2; ++i) {
    ASSERT_EQ(clock_.Now(),
              base::Time::FromDeltaSinceWindowsEpoch(base::Microseconds(
                  (*all_parcels)[i].last_update_time_usec())));
    auto p = (*all_parcels)[i].parcel_status();
    status_map.emplace(p.parcel_identifier().tracking_id(), p);
  }

  ASSERT_EQ(status1.parcel_state(), status_map[kTrackingId1].parcel_state());
  ASSERT_EQ(status2.parcel_state(), status_map[kTrackingId2].parcel_state());
}

TEST_F(ParcelsStorageTest, TestModifyOldDoneParcels) {
  EXPECT_CALL(*proto_db_, UpdateEntries(_, _, _)).Times(2);

  int64_t delivery_time =
      clock_.Now().ToDeltaSinceWindowsEpoch().InMicroseconds();

  std::vector<ParcelStatus> status;
  ParcelStatus status1 = CreateParcelStatus(kCarrier1, kTrackingId1,
                                            commerce::ParcelStatus::FINISHED);
  status1.set_estimated_delivery_time_usec(delivery_time);
  ParcelStatus status2 =
      CreateParcelStatus(kCarrier1, kTrackingId2, kDefaultState);
  status2.set_estimated_delivery_time_usec(delivery_time);
  status.emplace_back(status1);
  status.emplace_back(status2);
  storage_->UpdateParcelStatus(status, base::BindOnce(&DoNothing));
  task_environment_.RunUntilIdle();

  clock_.Advance(base::Days(20));
  storage_->ModifyOldDoneParcels();
  auto all_parcels = storage_->GetAllParcelTrackingContents();
  ASSERT_EQ(2u, all_parcels->size());
  for (int i = 0; i < 2; ++i) {
    ASSERT_EQ(delivery_time,
              (*all_parcels)[i].parcel_status().estimated_delivery_time_usec());
  }
  task_environment_.RunUntilIdle();

  clock_.Advance(base::Days(10));
  storage_->ModifyOldDoneParcels();
  all_parcels = storage_->GetAllParcelTrackingContents();
  ASSERT_EQ(2u, all_parcels->size());
  std::map<std::string, ParcelStatus> status_map;
  for (int i = 0; i < 2; ++i) {
    auto p = (*all_parcels)[i].parcel_status();
    status_map.emplace(p.parcel_identifier().tracking_id(), p);
  }
  ASSERT_EQ(0, status_map[kTrackingId1].estimated_delivery_time_usec());
  ASSERT_EQ(delivery_time,
            status_map[kTrackingId2].estimated_delivery_time_usec());
}

}  // namespace commerce
