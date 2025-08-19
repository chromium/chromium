// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/data_sharing/internal/personal_collaboration_data/personal_collaboration_data_service_impl.h"

#include <memory>

#include "base/test/task_environment.h"
#include "components/sync/test/data_type_store_test_util.h"
#include "components/sync/test/mock_data_type_local_change_processor.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace data_sharing::personal_collaboration_data {

namespace {

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
            syncer::DataTypeStoreTestUtil::CreateInMemoryStoreForTest()) {}

  void SetUp() override {
    service_ = std::make_unique<PersonalCollaborationDataServiceImpl>(
        mock_processor_.CreateForwardingProcessor(),
        syncer::DataTypeStoreTestUtil::FactoryForForwardingStore(
            data_type_store_.get()));
    service_->AddObserver(&mock_observer_);
  }

 protected:
  base::test::SingleThreadTaskEnvironment task_environment_;
  std::unique_ptr<syncer::DataTypeStore> data_type_store_;
  testing::NiceMock<syncer::MockDataTypeLocalChangeProcessor> mock_processor_;
  std::unique_ptr<PersonalCollaborationDataServiceImpl> service_;
  testing::StrictMock<MockObserver> mock_observer_;
};

TEST_F(PersonalCollaborationDataServiceImplTest, TestServiceConstruction) {
  EXPECT_FALSE(service_->IsInitialized());
}

TEST_F(PersonalCollaborationDataServiceImplTest,
       OnEntityAddedOrUpdatedFromSync_SharedTabGroup) {
  const std::string kStorageKey = "test_key";
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
  const std::string kStorageKey = "test_key";
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
