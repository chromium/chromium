// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/data_sharing/internal/personal_collaboration_data/personal_collaboration_data_service_impl.h"

#include <memory>

#include "base/test/scoped_feature_list.h"
#include "base/test/task_environment.h"
#include "components/sync/base/features.h"
#include "components/sync/protocol/entity_specifics.pb.h"
#include "components/sync/test/data_type_store_test_util.h"
#include "components/sync/test/mock_data_type_local_change_processor.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace data_sharing::personal_collaboration_data {

namespace {

using testing::_;
using testing::Return;
using testing::ReturnRef;

const char kStorageKey[] = "storage_key";

class MockObserver : public PersonalCollaborationDataService::Observer {
 public:
  MOCK_METHOD(void, OnInitialized, (), (override));
  MOCK_METHOD(void,
              OnSpecificsUpdated,
              (PersonalCollaborationDataService::SpecificsType specifics_type,
               const std::string& storage_key,
               const sync_pb::SharedTabGroupAccountDataSpecifics& specifics),
              (override));
};

}  // namespace

class PersonalCollaborationDataServiceImplTest : public testing::Test {
 public:
  PersonalCollaborationDataServiceImplTest()
      : data_type_store_(
            syncer::DataTypeStoreTestUtil::CreateInMemoryStoreForTest()) {
    feature_list_.InitAndEnableFeature(syncer::kSyncSharedTabGroupAccountData);
  }

  void SetUp() override {
    ON_CALL(mock_processor_, IsTrackingMetadata()).WillByDefault(Return(true));
    ON_CALL(mock_processor_, GetPossiblyTrimmedRemoteSpecifics(_))
        .WillByDefault(ReturnRef(sync_pb::EntitySpecifics::default_instance()));

    base::RunLoop run_loop;
    base::RepeatingClosure quit_closure = run_loop.QuitClosure();
    EXPECT_CALL(mock_processor_, ModelReadyToSync).WillOnce([&]() {
      quit_closure.Run();
    });
    EXPECT_CALL(mock_observer_, OnInitialized());

    service_ = std::make_unique<PersonalCollaborationDataServiceImpl>(
        mock_processor_.CreateForwardingProcessor(),
        syncer::DataTypeStoreTestUtil::FactoryForForwardingStore(
            data_type_store_.get()));
    service_->AddObserver(&mock_observer_);

    run_loop.Run();
    ASSERT_TRUE(service_->IsInitialized());
  }

 protected:
  base::test::SingleThreadTaskEnvironment task_environment_;
  std::unique_ptr<syncer::DataTypeStore> data_type_store_;
  testing::NiceMock<syncer::MockDataTypeLocalChangeProcessor> mock_processor_;
  std::unique_ptr<PersonalCollaborationDataServiceImpl> service_;
  testing::StrictMock<MockObserver> mock_observer_;
  base::test::ScopedFeatureList feature_list_;
};

TEST_F(PersonalCollaborationDataServiceImplTest,
       ShouldQueueActionsBeforeInitialization) {
  // Create a service without waiting for it to be initialized.
  auto data_type_store =
      syncer::DataTypeStoreTestUtil::CreateInMemoryStoreForTest();
  testing::NiceMock<syncer::MockDataTypeLocalChangeProcessor> mock_processor;
  ON_CALL(mock_processor, IsTrackingMetadata()).WillByDefault(Return(false));
  ON_CALL(mock_processor, GetPossiblyTrimmedRemoteSpecifics(_))
      .WillByDefault(ReturnRef(sync_pb::EntitySpecifics::default_instance()));

  auto service = std::make_unique<PersonalCollaborationDataServiceImpl>(
      mock_processor.CreateForwardingProcessor(),
      syncer::DataTypeStoreTestUtil::FactoryForForwardingStore(
          data_type_store.get()));
  MockObserver mock_observer;
  service->AddObserver(&mock_observer);

  ASSERT_FALSE(service->IsInitialized());

  // These actions should be queued.
  EXPECT_CALL(mock_processor, Put(_, _, _)).Times(0);
  EXPECT_CALL(mock_processor, Delete(_, _, _)).Times(0);
  const std::string kStorageKey1 = "storage_key1";
  const std::string kStorageKey2 = "storage_key2";

  service->CreateOrUpdateSpecifics(
      PersonalCollaborationDataService::SpecificsType::kSharedTabSpecifics,
      kStorageKey1,
      base::BindOnce(
          [](sync_pb::SharedTabGroupAccountDataSpecifics* specifics) {
            specifics->mutable_shared_tab_details();
          }));
  service->CreateOrUpdateSpecifics(
      PersonalCollaborationDataService::SpecificsType::kSharedTabSpecifics,
      kStorageKey2,
      base::BindOnce(
          [](sync_pb::SharedTabGroupAccountDataSpecifics* specifics) {
            specifics->mutable_shared_tab_details();
          }));
  service->DeleteSpecifics(
      PersonalCollaborationDataService::SpecificsType::kSharedTabSpecifics,
      kStorageKey2);
  testing::Mock::VerifyAndClearExpectations(&mock_processor);

  // Now, let the service initialize. The queued actions should be executed.
  base::RunLoop run_loop;
  EXPECT_CALL(mock_observer, OnInitialized()).WillOnce([&]() {
    run_loop.Quit();
  });
  ON_CALL(mock_processor, IsTrackingMetadata()).WillByDefault(Return(true));

  // The service queues a Put for key1, a Put for key2, then a Delete for key2.
  EXPECT_CALL(mock_processor, Put(_, _, _)).Times(2);
  EXPECT_CALL(mock_processor, Delete(_, _, _)).Times(1);

  // The store will finish loading and call ModelReadyToSync, which will trigger
  // OnInitialized on the service, which will run the queued tasks.
  run_loop.Run();
  ASSERT_TRUE(service->IsInitialized());
}

TEST_F(PersonalCollaborationDataServiceImplTest,
       CreateOrUpdateSpecificsForTab) {
  sync_pb::EntitySpecifics entity_specifics;
  entity_specifics.mutable_shared_tab_group_account_data();
  EXPECT_CALL(mock_processor_, GetPossiblyTrimmedRemoteSpecifics(_))
      .WillOnce(ReturnRef(entity_specifics));
  service_->CreateOrUpdateSpecifics(
      PersonalCollaborationDataService::SpecificsType::kSharedTabSpecifics,
      kStorageKey,
      base::BindOnce(
          [](sync_pb::SharedTabGroupAccountDataSpecifics* specifics) {
            specifics->mutable_shared_tab_details();
          }));
  std::optional<sync_pb::SharedTabGroupAccountDataSpecifics> specifics =
      service_->GetSpecifics(
          PersonalCollaborationDataService::SpecificsType::kSharedTabSpecifics,
          kStorageKey);
  EXPECT_TRUE(specifics.has_value());
  EXPECT_TRUE(specifics->has_shared_tab_details());
}

TEST_F(PersonalCollaborationDataServiceImplTest,
       OnEntityAddedOrUpdatedFromSync_SharedTabGroup) {
  sync_pb::SharedTabGroupAccountDataSpecifics specifics;
  specifics.mutable_shared_tab_group_details();

  EXPECT_CALL(mock_observer_,
              OnSpecificsUpdated(PersonalCollaborationDataService::
                                     SpecificsType::kSharedTabGroupSpecifics,
                                 kStorageKey, testing::_));

  service_->OnEntityAddedOrUpdatedFromSync(kStorageKey, specifics);
}

TEST_F(PersonalCollaborationDataServiceImplTest,
       OnEntityAddedOrUpdatedFromSync_SharedTab) {
  sync_pb::SharedTabGroupAccountDataSpecifics specifics;
  specifics.mutable_shared_tab_details();

  EXPECT_CALL(
      mock_observer_,
      OnSpecificsUpdated(
          PersonalCollaborationDataService::SpecificsType::kSharedTabSpecifics,
          kStorageKey, testing::_));

  service_->OnEntityAddedOrUpdatedFromSync(kStorageKey, specifics);
}

TEST_F(PersonalCollaborationDataServiceImplTest,
       OnEntityAddedOrUpdatedFromSync_SharedTabGroupWithPrefix) {
  const std::string kStorageKeyWithoutPrefix = "test_key";
  const std::string kStorageKeyWithPrefix = "2|test_key";
  sync_pb::SharedTabGroupAccountDataSpecifics specifics;
  specifics.mutable_shared_tab_group_details();

  EXPECT_CALL(mock_observer_,
              OnSpecificsUpdated(PersonalCollaborationDataService::
                                     SpecificsType::kSharedTabGroupSpecifics,
                                 kStorageKeyWithoutPrefix, testing::_));

  service_->OnEntityAddedOrUpdatedFromSync(kStorageKeyWithPrefix, specifics);
}

TEST_F(PersonalCollaborationDataServiceImplTest,
       OnEntityAddedOrUpdatedFromSync_SharedTabWithPrefix) {
  const std::string kStorageKeyWithoutPrefix = "test_key";
  const std::string kStorageKeyWithPrefix = "1|test_key";
  sync_pb::SharedTabGroupAccountDataSpecifics specifics;
  specifics.mutable_shared_tab_details();

  EXPECT_CALL(
      mock_observer_,
      OnSpecificsUpdated(
          PersonalCollaborationDataService::SpecificsType::kSharedTabSpecifics,
          kStorageKeyWithoutPrefix, testing::_));

  service_->OnEntityAddedOrUpdatedFromSync(kStorageKeyWithPrefix, specifics);
}

TEST_F(PersonalCollaborationDataServiceImplTest,
       OnEntityAddedOrUpdatedFromSync_InvalidPrefix) {
  const std::string kStorageKeyWithInvalidPrefix = "abc|test_key";
  sync_pb::SharedTabGroupAccountDataSpecifics specifics;
  specifics.mutable_shared_tab_group_details();

  EXPECT_CALL(mock_observer_,
              OnSpecificsUpdated(PersonalCollaborationDataService::
                                     SpecificsType::kSharedTabGroupSpecifics,
                                 kStorageKeyWithInvalidPrefix, testing::_));

  service_->OnEntityAddedOrUpdatedFromSync(kStorageKeyWithInvalidPrefix,
                                           specifics);
}

TEST_F(PersonalCollaborationDataServiceImplTest,
       OnEntityAddedOrUpdatedFromSync_MismatchedPrefix) {
  const std::string kStorageKeyWithMismatchedPrefix = "0|test_key";
  sync_pb::SharedTabGroupAccountDataSpecifics specifics;
  specifics.mutable_shared_tab_group_details();

  EXPECT_CALL(mock_observer_,
              OnSpecificsUpdated(PersonalCollaborationDataService::
                                     SpecificsType::kSharedTabGroupSpecifics,
                                 kStorageKeyWithMismatchedPrefix, testing::_));

  service_->OnEntityAddedOrUpdatedFromSync(kStorageKeyWithMismatchedPrefix,
                                           specifics);
}

TEST_F(PersonalCollaborationDataServiceImplTest,
       OnEntityAddedOrUpdatedFromSync_MultipleSeparatorsInKey) {
  const std::string kStorageKeyWithMultipleSeparators = "1|test|key";
  const std::string kStorageKeyWithoutPrefix = "test|key";
  sync_pb::SharedTabGroupAccountDataSpecifics specifics;
  specifics.mutable_shared_tab_details();

  EXPECT_CALL(
      mock_observer_,
      OnSpecificsUpdated(
          PersonalCollaborationDataService::SpecificsType::kSharedTabSpecifics,
          kStorageKeyWithoutPrefix, testing::_));

  service_->OnEntityAddedOrUpdatedFromSync(kStorageKeyWithMultipleSeparators,
                                           specifics);
}

}  // namespace data_sharing::personal_collaboration_data
