// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/consent_auditor/consent_sync_bridge_impl.h"

#include <map>
#include <set>
#include <utility>

#include "base/functional/bind.h"
#include "base/functional/callback_helpers.h"
#include "base/run_loop.h"
#include "base/test/task_environment.h"
#include "components/sync/model/data_batch.h"
#include "components/sync/protocol/entity_specifics.pb.h"
#include "components/sync/protocol/user_consent_specifics.pb.h"
#include "components/sync/test/data_type_store_test_util.h"
#include "components/sync/test/mock_data_type_local_change_processor.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace consent_auditor {
namespace {

using sync_pb::UserConsentSpecifics;
using syncer::DataBatch;
using syncer::DataTypeStore;
using syncer::DataTypeStoreTestUtil;
using syncer::DataTypeSyncBridge;
using syncer::EntityChange;
using syncer::EntityChangeList;
using syncer::EntityData;
using syncer::MetadataChangeList;
using syncer::MockDataTypeLocalChangeProcessor;
using syncer::OnceDataTypeStoreFactory;
using testing::_;
using testing::ElementsAre;
using testing::Eq;
using testing::InvokeWithoutArgs;
using testing::IsEmpty;
using testing::IsNull;
using testing::NotNull;
using testing::Pair;
using testing::Pointee;
using testing::Property;
using testing::Return;
using testing::SaveArg;
using testing::SizeIs;
using testing::UnorderedElementsAre;
using testing::WithArg;
using WriteBatch = DataTypeStore::WriteBatch;

MATCHER_P(MatchesUserConsent, expected, "") {
  if (!arg.has_user_consent()) {
    *result_listener << "which is not a user consent";
    return false;
  }
  const UserConsentSpecifics& actual = arg.user_consent();
  if (actual.client_consent_time_usec() !=
      expected.client_consent_time_usec()) {
    return false;
  }
  return true;
}

UserConsentSpecifics CreateSpecifics(int64_t client_consent_time_usec) {
  UserConsentSpecifics specifics;
  specifics.set_client_consent_time_usec(client_consent_time_usec);
  specifics.set_account_id("account_id");
  return specifics;
}

std::unique_ptr<UserConsentSpecifics> SpecificsUniquePtr(
    int64_t client_consent_time_usec) {
  return std::make_unique<UserConsentSpecifics>(
      CreateSpecifics(client_consent_time_usec));
}

class ConsentSyncBridgeImplTest : public testing::Test {
 protected:
  ConsentSyncBridgeImplTest() { ResetBridge(); }

  void ResetBridge() {
    OnceDataTypeStoreFactory store_factory;
    if (bridge_) {
      // Carry over the underlying store from previous bridge instances.
      std::unique_ptr<DataTypeStore> store = bridge_->StealStoreForTest();
      bridge_.reset();
      store_factory =
          DataTypeStoreTestUtil::MoveStoreToFactory(std::move(store));
    } else {
      store_factory = DataTypeStoreTestUtil::FactoryForInMemoryStoreForTest();
    }

    bridge_ = std::make_unique<ConsentSyncBridgeImpl>(
        std::move(store_factory), mock_processor_.CreateForwardingProcessor());
  }

  void WaitUntilModelReadyToSync(const std::string& account_id) {
    base::RunLoop loop;
    base::RepeatingClosure quit_closure = loop.QuitClosure();
    // Let the bridge initialize fully, which should run ModelReadyToSync().
    ON_CALL(*processor(), ModelReadyToSync(_))
        .WillByDefault(InvokeWithoutArgs([=]() { quit_closure.Run(); }));
    loop.Run();
    ON_CALL(*processor(), IsTrackingMetadata()).WillByDefault(Return(true));
    ON_CALL(*processor(), TrackedAccountId()).WillByDefault(Return(account_id));
  }

  static std::string GetStorageKey(const UserConsentSpecifics& specifics) {
    return ConsentSyncBridgeImpl::GetStorageKeyFromSpecificsForTest(specifics);
  }

  ConsentSyncBridgeImpl* bridge() { return bridge_.get(); }
  MockDataTypeLocalChangeProcessor* processor() { return &mock_processor_; }

  std::map<std::string, sync_pb::EntitySpecifics> GetAllDataForDebugging() {
    std::unique_ptr<DataBatch> batch = bridge_->GetAllDataForDebugging();
    EXPECT_NE(nullptr, batch);

    std::map<std::string, sync_pb::EntitySpecifics> storage_key_to_specifics;
    if (batch != nullptr) {
      while (batch->HasNext()) {
        const syncer::KeyAndData& pair = batch->Next();
        storage_key_to_specifics[pair.first] = pair.second->specifics;
      }
    }
    return storage_key_to_specifics;
  }

  std::unique_ptr<sync_pb::EntitySpecifics> GetDataForCommit(
      const std::string& storage_key) {
    std::unique_ptr<DataBatch> batch = bridge_->GetDataForCommit({storage_key});
    EXPECT_NE(nullptr, batch);

    std::unique_ptr<sync_pb::EntitySpecifics> specifics;
    if (batch != nullptr && batch->HasNext()) {
      const syncer::KeyAndData& pair = batch->Next();
      specifics =
          std::make_unique<sync_pb::EntitySpecifics>(pair.second->specifics);
      EXPECT_FALSE(batch->HasNext());
    }
    return specifics;
  }

 private:
  base::test::SingleThreadTaskEnvironment task_environment_;
  testing::NiceMock<MockDataTypeLocalChangeProcessor> mock_processor_;
  std::unique_ptr<ConsentSyncBridgeImpl> bridge_;
};

TEST_F(ConsentSyncBridgeImplTest, ShouldCallModelReadyToSyncOnStartup) {
  EXPECT_CALL(*processor(), ModelReadyToSync(NotNull()));
  WaitUntilModelReadyToSync("account_id");
}

TEST_F(ConsentSyncBridgeImplTest, ShouldGetDataForCommit) {
  WaitUntilModelReadyToSync("account_id");
  const UserConsentSpecifics specifics(
      CreateSpecifics(/*client_consent_time_usec=*/1u));
  std::string storage_key;
  EXPECT_CALL(*processor(), Put(_, _, _))
      .WillOnce(WithArg<0>(SaveArg<0>(&storage_key)));
  bridge()->RecordConsent(std::make_unique<UserConsentSpecifics>(specifics));

  // Existing specifics should be returned.
  EXPECT_THAT(GetDataForCommit(storage_key),
              Pointee(MatchesUserConsent(specifics)));
  // GetDataForCommit() should handle arbitrary storage key.
  EXPECT_THAT(GetDataForCommit("bogus"), IsNull());
}

TEST_F(ConsentSyncBridgeImplTest, ShouldRecordSingleConsent) {
  WaitUntilModelReadyToSync("account_id");
  const UserConsentSpecifics specifics(
      CreateSpecifics(/*client_consent_time_usec=*/1u));
  std::string storage_key;
  EXPECT_CALL(*processor(), Put(_, _, _))
      .WillOnce(WithArg<0>(SaveArg<0>(&storage_key)));
  bridge()->RecordConsent(std::make_unique<UserConsentSpecifics>(specifics));

  EXPECT_THAT(GetDataForCommit(storage_key),
              Pointee(MatchesUserConsent(specifics)));
  EXPECT_THAT(GetAllDataForDebugging(),
              ElementsAre(Pair(storage_key, MatchesUserConsent(specifics))));
}

TEST_F(ConsentSyncBridgeImplTest, ShouldNotDeleteConsentsWhenSyncIsDisabled) {
  WaitUntilModelReadyToSync("account_id");
  UserConsentSpecifics user_consent_specifics(
      CreateSpecifics(/*client_consent_time_usec=*/2u));
  bridge()->RecordConsent(
      std::make_unique<UserConsentSpecifics>(user_consent_specifics));
  ASSERT_THAT(GetAllDataForDebugging(), SizeIs(1));

  bridge()->ApplyDisableSyncChanges(WriteBatch::CreateMetadataChangeList());
  // The bridge may asynchronously query the store to choose what to delete.
  base::RunLoop().RunUntilIdle();

  // User consent specific must be persisted when sync is disabled.
  EXPECT_THAT(GetAllDataForDebugging(),
              ElementsAre(Pair(_, MatchesUserConsent(user_consent_specifics))));
}

TEST_F(ConsentSyncBridgeImplTest,
       ShouldRecordMultipleConsentsAndDeduplicateByTime) {
  WaitUntilModelReadyToSync("account_id");
  std::set<std::string> unique_storage_keys;
  EXPECT_CALL(*processor(), Put(_, _, _))
      .Times(4)
      .WillRepeatedly(
          [&unique_storage_keys](const std::string& storage_key,
                                 std::unique_ptr<EntityData> entity_data,
                                 MetadataChangeList* metadata_change_list) {
            unique_storage_keys.insert(storage_key);
          });

  bridge()->RecordConsent(SpecificsUniquePtr(/*client_consent_time_usec=*/1u));
  bridge()->RecordConsent(SpecificsUniquePtr(/*client_consent_time_usec=*/1u));
  bridge()->RecordConsent(SpecificsUniquePtr(/*client_consent_time_usec=*/1u));
  bridge()->RecordConsent(SpecificsUniquePtr(/*client_consent_time_usec=*/2u));

  EXPECT_EQ(2u, unique_storage_keys.size());
  EXPECT_THAT(GetAllDataForDebugging(), SizeIs(2));
}

TEST_F(ConsentSyncBridgeImplTest,
       ShouldDeleteCommitedConsentsAfterApplyIncrementalSyncChanges) {
  WaitUntilModelReadyToSync("account_id");
  std::string first_storage_key;
  std::string second_storage_key;
  EXPECT_CALL(*processor(), Put(_, _, _))
      .WillOnce(WithArg<0>(SaveArg<0>(&first_storage_key)))
      .WillOnce(WithArg<0>(SaveArg<0>(&second_storage_key)));

  bridge()->RecordConsent(SpecificsUniquePtr(/*client_consent_time_usec=*/1u));
  bridge()->RecordConsent(SpecificsUniquePtr(/*client_consent_time_usec=*/2u));
  ASSERT_THAT(GetAllDataForDebugging(), SizeIs(2));

  syncer::EntityChangeList entity_change_list;
  entity_change_list.push_back(EntityChange::CreateDelete(first_storage_key));
  auto error_on_delete = bridge()->ApplyIncrementalSyncChanges(
      bridge()->CreateMetadataChangeList(), std::move(entity_change_list));
  EXPECT_FALSE(error_on_delete);
  EXPECT_THAT(GetAllDataForDebugging(), SizeIs(1));
  EXPECT_THAT(GetDataForCommit(first_storage_key), IsNull());
  EXPECT_THAT(GetDataForCommit(second_storage_key), NotNull());
}

TEST_F(ConsentSyncBridgeImplTest, ShouldRecordConsentsBeforeSyncEnabled) {
  WaitUntilModelReadyToSync(/*account_id=*/"");
  // The consent must be recorded, but not propagated anywhere while the
  // initialization is in progress and sync is still disabled.
  EXPECT_CALL(*processor(), Put(_, _, _)).Times(0);
  bridge()->RecordConsent(SpecificsUniquePtr(/*client_consent_time_usec=*/1u));
  // When sync is enabled, the consent should be reported to the processor.
  ON_CALL(*processor(), IsTrackingMetadata()).WillByDefault(Return(true));
  ON_CALL(*processor(), TrackedAccountId()).WillByDefault(Return("account_id"));
  EXPECT_CALL(*processor(), Put(_, _, _));
  bridge()->MergeFullSyncData(WriteBatch::CreateMetadataChangeList(),
                              EntityChangeList());
  base::RunLoop().RunUntilIdle();
}

// User consents should be buffered if the store and processor is not fully
// initialized.
TEST_F(ConsentSyncBridgeImplTest,
       ShouldSubmitBufferedConsentsWhenStoreIsInitialized) {
  EXPECT_CALL(*processor(), ModelReadyToSync(_)).Times(0);

  UserConsentSpecifics first_consent =
      CreateSpecifics(/*client_consent_time_usec=*/1u);
  first_consent.set_account_id("account_id");
  UserConsentSpecifics second_consent =
      CreateSpecifics(/*client_consent_time_usec=*/2u);
  second_consent.set_account_id("account_id");

  // Record consent before the store is initialized (ModelReadyToSync() not
  // called yet).
  bridge()->RecordConsent(
      std::make_unique<UserConsentSpecifics>(first_consent));

  // Wait until the store is initialized.
  EXPECT_CALL(*processor(), ModelReadyToSync(NotNull()));
  WaitUntilModelReadyToSync("account_id");

  // Record consent after initializaiton is done.
  bridge()->RecordConsent(
      std::make_unique<UserConsentSpecifics>(second_consent));

  // Both the pre-initialization and post-initialization consents must be
  // handled after initialization as usual.
  EXPECT_THAT(GetAllDataForDebugging(),
              UnorderedElementsAre(Pair(GetStorageKey(first_consent),
                                        MatchesUserConsent(first_consent)),
                                   Pair(GetStorageKey(second_consent),
                                        MatchesUserConsent(second_consent))));
}

TEST_F(ConsentSyncBridgeImplTest,
       ShouldReportPreviouslyPersistedConsentsWhenSyncIsReenabled) {
  WaitUntilModelReadyToSync("account_id");

  UserConsentSpecifics consent =
      CreateSpecifics(/*client_consent_time_usec=*/1u);
  consent.set_account_id("account_id");

  bridge()->RecordConsent(std::make_unique<UserConsentSpecifics>(consent));

  // User disables sync, hovewer, the consent hasn't been submitted yet. It is
  // preserved to be submitted when sync is re-enabled.
  bridge()->ApplyDisableSyncChanges(WriteBatch::CreateMetadataChangeList());
  // The bridge may asynchronously query the store to choose what to delete.
  base::RunLoop().RunUntilIdle();

  ASSERT_THAT(GetAllDataForDebugging(), SizeIs(1));

  // Reenable sync.
  EXPECT_CALL(*processor(), Put(GetStorageKey(consent), _, _));
  ON_CALL(*processor(), TrackedAccountId()).WillByDefault(Return("account_id"));
  bridge()->MergeFullSyncData(WriteBatch::CreateMetadataChangeList(),
                              EntityChangeList());

  // The bridge may asynchronously query the store to choose what to resubmit.
  base::RunLoop().RunUntilIdle();
}

TEST_F(ConsentSyncBridgeImplTest,
       ShouldReportPersistedConsentsOnStartupWithSyncAlreadyEnabled) {
  // Persist a consent while sync is enabled.
  WaitUntilModelReadyToSync("account_id");
  UserConsentSpecifics consent =
      CreateSpecifics(/*client_consent_time_usec=*/1u);
  consent.set_account_id("account_id");
  bridge()->RecordConsent(std::make_unique<UserConsentSpecifics>(consent));
  base::RunLoop().RunUntilIdle();
  ASSERT_THAT(GetAllDataForDebugging(), SizeIs(1));

  // Restart the bridge, mimic-ing a browser restart.
  EXPECT_CALL(*processor(), Put(GetStorageKey(consent), _, _));
  ResetBridge();

  // The bridge may asynchronously query the store to choose what to resubmit.
  base::RunLoop().RunUntilIdle();
}

TEST_F(ConsentSyncBridgeImplTest, ShouldReportPersistedConsentsOnSyncEnabled) {
  // Persist a consent before sync is enabled.
  WaitUntilModelReadyToSync(/*account=id=*/"");
  UserConsentSpecifics consent =
      CreateSpecifics(/*client_consent_time_usec=*/1u);
  consent.set_account_id("account_id");
  bridge()->RecordConsent(std::make_unique<UserConsentSpecifics>(consent));
  base::RunLoop().RunUntilIdle();
  ASSERT_THAT(GetAllDataForDebugging(), SizeIs(1));

  // Restart the bridge, mimic-ing a browser restart. We expect no Put()
  // until sync is enabled.
  EXPECT_CALL(*processor(), Put(_, _, _)).Times(0);
  ResetBridge();
  WaitUntilModelReadyToSync(/*account_id=*/"");

  // Enable sync.
  EXPECT_CALL(*processor(), Put(GetStorageKey(consent), _, _));
  ON_CALL(*processor(), TrackedAccountId()).WillByDefault(Return("account_id"));
  bridge()->MergeFullSyncData(WriteBatch::CreateMetadataChangeList(),
                              EntityChangeList());
  base::RunLoop().RunUntilIdle();
}

TEST_F(ConsentSyncBridgeImplTest,
       ShouldResubmitPersistedConsentOnlyIfSameAccount) {
  WaitUntilModelReadyToSync("first_account");
  UserConsentSpecifics user_consent_specifics(
      CreateSpecifics(/*client_consent_time_usec=*/2u));
  user_consent_specifics.set_account_id("first_account");
  bridge()->RecordConsent(
      std::make_unique<UserConsentSpecifics>(user_consent_specifics));
  ASSERT_THAT(GetAllDataForDebugging(), SizeIs(1));

  bridge()->ApplyDisableSyncChanges(WriteBatch::CreateMetadataChangeList());
  // The bridge may asynchronously query the store to choose what to delete.
  base::RunLoop().RunUntilIdle();

  ASSERT_THAT(GetAllDataForDebugging(),
              ElementsAre(Pair(_, MatchesUserConsent(user_consent_specifics))));

  // A new user signs in and enables sync.
  // The previous account consent should not be resubmited, because the new sync
  // account is different.
  EXPECT_CALL(*processor(), Put(_, _, _)).Times(0);
  ON_CALL(*processor(), TrackedAccountId())
      .WillByDefault(Return("second_account"));
  bridge()->MergeFullSyncData(WriteBatch::CreateMetadataChangeList(),
                              EntityChangeList());
  base::RunLoop().RunUntilIdle();

  bridge()->ApplyDisableSyncChanges(WriteBatch::CreateMetadataChangeList());
  base::RunLoop().RunUntilIdle();

  // This time their consent should be resubmitted, because it is for the same
  // account.
  EXPECT_CALL(*processor(), Put(GetStorageKey(user_consent_specifics), _, _));
  ON_CALL(*processor(), TrackedAccountId())
      .WillByDefault(Return("first_account"));
  bridge()->MergeFullSyncData(WriteBatch::CreateMetadataChangeList(),
                              EntityChangeList());
  // The bridge may asynchronously query the store to choose what to resubmit.
  base::RunLoop().RunUntilIdle();
}

}  // namespace

}  // namespace consent_auditor
