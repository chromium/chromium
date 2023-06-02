// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/autofill/core/browser/webdata/contact_info_sync_bridge.h"

#include <memory>

#include "base/files/scoped_temp_dir.h"
#include "base/run_loop.h"
#include "base/strings/string_piece.h"
#include "base/test/bind.h"
#include "base/test/scoped_feature_list.h"
#include "base/test/task_environment.h"
#include "components/autofill/core/browser/contact_info_sync_util.h"
#include "components/autofill/core/browser/data_model/autofill_profile.h"
#include "components/autofill/core/browser/webdata/autofill_table.h"
#include "components/autofill/core/browser/webdata/mock_autofill_webdata_backend.h"
#include "components/sync/base/features.h"
#include "components/sync/model/data_batch.h"
#include "components/sync/test/mock_model_type_change_processor.h"
#include "components/webdata/common/web_database.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace autofill {

namespace {

using testing::_;
using testing::ElementsAre;
using testing::UnorderedElementsAre;

constexpr char kGUID1[] = "00000000-0000-0000-0000-000000000001";
constexpr char kGUID2[] = "00000000-0000-0000-0000-000000000002";
constexpr char kInvalidGUID[] = "1234";

// Matches `syncer::EntityData*` and expects that the specifics of it match
// the `expected_profile`.
MATCHER_P(ContactInfoSpecificsEqualsProfile, expected_profile, "") {
  AutofillProfile arg_profile = *CreateAutofillProfileFromContactInfoSpecifics(
      arg->specifics.contact_info());
  if (!arg_profile.EqualsIncludingUsageStatsForTesting(expected_profile)) {
    *result_listener << "entry\n[" << arg_profile << "]\n"
                     << "did not match expected\n[" << expected_profile << "]";
    return false;
  }
  return true;
}

// Extracts all `ContactInfoSpecifics` from `batch`, converts them into
// `AutofillProfile`s and returns the result.
// Note that for consistency with the `AUTOFILL_PROFILE` sync type,
// `CONTACT_INFO` uses the same local `AutofillProfile` model.
std::vector<AutofillProfile> ExtractAutofillProfilesFromDataBatch(
    std::unique_ptr<syncer::DataBatch> batch) {
  std::vector<AutofillProfile> profiles;
  while (batch->HasNext()) {
    const syncer::KeyAndData& data_pair = batch->Next();
    profiles.push_back(*CreateAutofillProfileFromContactInfoSpecifics(
        data_pair.second->specifics.contact_info()));
  }
  return profiles;
}

AutofillProfile TestProfile(base::StringPiece guid) {
  return AutofillProfile(std::string(guid), AutofillProfile::Source::kAccount);
}

}  // namespace

class ContactInfoSyncBridgeTest : public testing::Test {
 public:
  // Creates the `bridge()` and mocks its `AutofillTable`.
  void SetUp() override {
    ASSERT_TRUE(temp_dir_.CreateUniqueTempDir());
    db_.AddTable(&table_);
    db_.Init(temp_dir_.GetPath().AppendASCII("SyncTestWebDatabase"));
    ON_CALL(backend_, GetDatabase()).WillByDefault(testing::Return(&db_));

    bridge_ = std::make_unique<ContactInfoSyncBridge>(
        mock_processor_.CreateForwardingProcessor(), &backend_);
    ON_CALL(mock_processor(), IsTrackingMetadata)
        .WillByDefault(testing::Return(true));

    ON_CALL(mock_processor_, GetPossiblyTrimmedRemoteSpecifics)
        .WillByDefault(
            testing::ReturnRef(sync_pb::EntitySpecifics::default_instance()));
  }

  // Tells the processor to starts syncing with pre-existing `remote_profiles`.
  // Triggers the `bridge()`'s `MergeFullSyncData()`.
  // Returns true if syncing started successfully.
  bool StartSyncing(const std::vector<AutofillProfile>& remote_profiles) {
    syncer::EntityChangeList entity_data;
    for (const AutofillProfile& profile : remote_profiles) {
      entity_data.push_back(syncer::EntityChange::CreateAdd(
          profile.guid(), ProfileToEntity(profile)));
    }
    // `MergeFullSyncData()` returns an error if it fails.
    return !bridge().MergeFullSyncData(bridge().CreateMetadataChangeList(),
                                       std::move(entity_data));
  }

  // Adds multiple `profiles` the `bridge()`'s AutofillTable.
  void AddAutofillProfilesToTable(
      const std::vector<AutofillProfile>& profiles) {
    for (const auto& profile : profiles) {
      table_.AddAutofillProfile(profile);
    }
  }

  // Reads all kAccount profiles from `table_`. We should not rely on the
  // `bridge()` (and `GetAllDataForDebugging()`) here, since we want to simulate
  // how the PersonalDataManager will access the profiles.
  std::vector<AutofillProfile> GetAllDataFromTable() {
    std::vector<std::unique_ptr<AutofillProfile>> profile_ptrs;
    EXPECT_TRUE(table_.GetAutofillProfiles(AutofillProfile::Source::kAccount,
                                           &profile_ptrs));
    // In tests, it's more convenient to work without `std::unique_ptr`.
    std::vector<AutofillProfile> profiles;
    for (const std::unique_ptr<AutofillProfile>& profile_ptr : profile_ptrs) {
      profiles.push_back(std::move(*profile_ptr));
    }
    return profiles;
  }

  syncer::EntityData ProfileToEntity(const AutofillProfile& profile) {
    return std::move(*CreateContactInfoEntityDataFromAutofillProfile(
        profile, /*base_contact_info_specifics=*/{}));
  }

  MockAutofillWebDataBackend& backend() { return backend_; }

  syncer::MockModelTypeChangeProcessor& mock_processor() {
    return mock_processor_;
  }

  ContactInfoSyncBridge& bridge() { return *bridge_; }

 private:
  base::ScopedTempDir temp_dir_;
  base::test::SingleThreadTaskEnvironment task_environment_;
  testing::NiceMock<MockAutofillWebDataBackend> backend_;
  AutofillTable table_;
  WebDatabase db_;
  testing::NiceMock<syncer::MockModelTypeChangeProcessor> mock_processor_;
  std::unique_ptr<ContactInfoSyncBridge> bridge_;
};

// Tests that a failure in the database initialization reports an error and
// doesn't cause a crash.
// Regression test for crbug.com/1421663.
TEST_F(ContactInfoSyncBridgeTest, InitializationFailure) {
  // The database will be null if it failed to initialize.
  ON_CALL(backend(), GetDatabase()).WillByDefault(testing::Return(nullptr));
  EXPECT_CALL(mock_processor(), ReportError);
  // The `bridge()` was already initialized during `SetUp()`. Recreate it.
  ContactInfoSyncBridge(mock_processor().CreateForwardingProcessor(),
                        &backend());
}

TEST_F(ContactInfoSyncBridgeTest, IsEntityDataValid) {
  // Valid case.
  std::unique_ptr<syncer::EntityData> entity =
      CreateContactInfoEntityDataFromAutofillProfile(
          TestProfile(kGUID1), /*base_contact_info_specifics=*/{});
  EXPECT_TRUE(bridge().IsEntityDataValid(*entity));
  // Invalid case.
  entity->specifics.mutable_contact_info()->set_guid(kInvalidGUID);
  EXPECT_FALSE(bridge().IsEntityDataValid(*entity));
}

TEST_F(ContactInfoSyncBridgeTest, GetStorageKey) {
  std::unique_ptr<syncer::EntityData> entity =
      CreateContactInfoEntityDataFromAutofillProfile(
          TestProfile(kGUID1), /*base_contact_info_specifics=*/{});
  ASSERT_TRUE(bridge().IsEntityDataValid(*entity));
  EXPECT_EQ(kGUID1, bridge().GetStorageKey(*entity));
}

// Tests that during the initial sync, `MergeFullSyncData()` incorporates remote
// profiles.
TEST_F(ContactInfoSyncBridgeTest, MergeFullSyncData) {
  const AutofillProfile remote1 = TestProfile(kGUID1);
  const AutofillProfile remote2 = TestProfile(kGUID2);

  EXPECT_CALL(mock_processor(), Put).Times(0);
  EXPECT_CALL(mock_processor(), Delete).Times(0);
  EXPECT_CALL(backend(), CommitChanges);
  EXPECT_CALL(backend(), NotifyOfMultipleAutofillChanges);
  EXPECT_CALL(backend(), NotifyThatSyncHasStarted(syncer::CONTACT_INFO));

  EXPECT_TRUE(StartSyncing({remote1, remote2}));

  EXPECT_THAT(GetAllDataFromTable(), UnorderedElementsAre(remote1, remote2));
}

// Tests that when sync changes are applied, `ApplyIncrementalSyncChanges()`
// merges remotes changes into the local store. New local changes are not
// applied to sync.
TEST_F(ContactInfoSyncBridgeTest, ApplyIncrementalSyncChanges) {
  AddAutofillProfilesToTable({TestProfile(kGUID1)});
  ASSERT_TRUE(StartSyncing(/*remote_profiles=*/{}));

  AutofillProfile remote = TestProfile(kGUID2);

  // Delete the existing local profile and add + update `remote`.
  syncer::EntityChangeList entity_change_list;
  entity_change_list.push_back(syncer::EntityChange::CreateDelete(kGUID1));
  entity_change_list.push_back(
      syncer::EntityChange::CreateAdd(kGUID2, ProfileToEntity(remote)));
  remote.SetRawInfo(EMAIL_ADDRESS, u"test@example.com");
  entity_change_list.push_back(
      syncer::EntityChange::CreateUpdate(kGUID2, ProfileToEntity(remote)));

  // Expect no changes to the remote profiles.
  EXPECT_CALL(mock_processor(), Delete).Times(0);
  EXPECT_CALL(mock_processor(), Put).Times(0);
  EXPECT_CALL(backend(), CommitChanges());
  EXPECT_CALL(backend(), NotifyOfMultipleAutofillChanges);

  // `ApplyIncrementalSyncChanges()` returns an error if it fails.
  EXPECT_FALSE(bridge().ApplyIncrementalSyncChanges(
      bridge().CreateMetadataChangeList(), std::move(entity_change_list)));

  // Expect that the local profiles have changed.
  EXPECT_THAT(GetAllDataFromTable(), ElementsAre(remote));
}

// Tests that `GetData()` returns all local profiles of matching GUID.
TEST_F(ContactInfoSyncBridgeTest, GetData) {
  const AutofillProfile profile1 = TestProfile(kGUID1);
  const AutofillProfile profile2 = TestProfile(kGUID2);
  AddAutofillProfilesToTable({profile1, profile2});

  // Synchronously get data the data of `kGUID1`.
  std::vector<AutofillProfile> profiles;
  base::RunLoop loop;
  bridge().GetData(
      {kGUID1},
      base::BindLambdaForTesting([&](std::unique_ptr<syncer::DataBatch> batch) {
        profiles = ExtractAutofillProfilesFromDataBatch(std::move(batch));
        loop.Quit();
      }));
  loop.Run();
  EXPECT_THAT(profiles, ElementsAre(profile1));
}

// Tests that `GetAllDataForDebugging()` returns all local data.
TEST_F(ContactInfoSyncBridgeTest, GetAllDataForDebugging) {
  const AutofillProfile profile1 = TestProfile(kGUID1);
  const AutofillProfile profile2 = TestProfile(kGUID2);
  AddAutofillProfilesToTable({profile1, profile2});

  // Synchronously gets all data from the `bridge()`.
  std::vector<AutofillProfile> profiles;
  base::RunLoop loop;
  bridge().GetAllDataForDebugging(
      base::BindLambdaForTesting([&](std::unique_ptr<syncer::DataBatch> batch) {
        profiles = ExtractAutofillProfilesFromDataBatch(std::move(batch));
        loop.Quit();
      }));
  loop.Run();
  EXPECT_THAT(profiles, UnorderedElementsAre(profile1, profile2));
}

// Tests that new local profiles are pushed to Sync.
TEST_F(ContactInfoSyncBridgeTest, AutofillProfileChange_Add) {
  ASSERT_TRUE(StartSyncing(/*remote_profiles=*/{}));

  const AutofillProfile profile = TestProfile(kGUID1);
  const AutofillProfileChange change(AutofillProfileChange::ADD, kGUID1,
                                     &profile);
  EXPECT_CALL(mock_processor(),
              Put(kGUID1, ContactInfoSpecificsEqualsProfile(profile), _));
  // The bridge does not need to commit when reacting to a notification about a
  // local change.
  EXPECT_CALL(backend(), CommitChanges()).Times(0);

  bridge().AutofillProfileChanged(change);
}

// Tests that updates to local profiles are pushed to Sync.
TEST_F(ContactInfoSyncBridgeTest, AutofillProfileChange_Update) {
  ASSERT_TRUE(StartSyncing(/*remote_profiles=*/{}));

  const AutofillProfile profile = TestProfile(kGUID1);
  const AutofillProfileChange change(AutofillProfileChange::UPDATE, kGUID1,
                                     &profile);
  EXPECT_CALL(mock_processor(),
              Put(kGUID1, ContactInfoSpecificsEqualsProfile(profile), _));
  EXPECT_CALL(backend(), CommitChanges()).Times(0);

  bridge().AutofillProfileChanged(change);
}

// Tests that the removal of local profiles is communicated to Sync.
TEST_F(ContactInfoSyncBridgeTest, AutofillProfileChange_Remove) {
  ASSERT_TRUE(StartSyncing(/*remote_profiles=*/{}));

  const AutofillProfile profile = TestProfile(kGUID1);
  const AutofillProfileChange change(AutofillProfileChange::REMOVE, kGUID1,
                                     &profile);
  EXPECT_CALL(mock_processor(), Delete(kGUID1, _));
  EXPECT_CALL(backend(), CommitChanges()).Times(0);

  bridge().AutofillProfileChanged(change);
}

// Tests that `ApplyDisableSyncChanges()` clears all data in AutofillTable when
// the data type gets disabled.
TEST_F(ContactInfoSyncBridgeTest, ApplyDisableSyncChanges) {
  const AutofillProfile remote = TestProfile(kGUID1);
  ASSERT_TRUE(StartSyncing({remote}));
  ASSERT_THAT(GetAllDataFromTable(), ElementsAre(remote));

  EXPECT_CALL(backend(), CommitChanges());
  EXPECT_CALL(backend(), NotifyOfMultipleAutofillChanges);

  bridge().ApplyDisableSyncChanges(bridge().CreateMetadataChangeList());

  EXPECT_TRUE(GetAllDataFromTable().empty());
}

// Tests that trimming `ContactInfoSpecifics` with only supported values set
// results in a zero-length specifics.
TEST_F(ContactInfoSyncBridgeTest,
       TrimAllSupportedFieldsFromRemoteSpecificsPreservesOnlySupportedFields) {
  sync_pb::EntitySpecifics specifics;
  sync_pb::ContactInfoSpecifics* contact_info_specifics =
      specifics.mutable_contact_info();
  contact_info_specifics->mutable_address_city()->set_value("City");

  EXPECT_EQ(bridge()
                .TrimAllSupportedFieldsFromRemoteSpecifics(specifics)
                .ByteSizeLong(),
            0u);
}

// Tests that trimming `ContactInfoSpecifics` with unsupported fields will only
// preserve the unknown fields.
TEST_F(ContactInfoSyncBridgeTest,
       TrimRemoteSpecificsReturnsEmptyProtoWhenAllFieldsAreSupported) {
  sync_pb::EntitySpecifics specifics_with_only_unknown_fields;
  *specifics_with_only_unknown_fields.mutable_contact_info()
       ->mutable_unknown_fields() = "unsupported_fields";

  sync_pb::EntitySpecifics specifics_with_known_and_unknown_fields =
      specifics_with_only_unknown_fields;
  sync_pb::ContactInfoSpecifics* contact_info_specifics =
      specifics_with_known_and_unknown_fields.mutable_contact_info();
  contact_info_specifics->mutable_address_city()->set_value("City");
  contact_info_specifics->mutable_address_country()->set_value("Country");

  EXPECT_EQ(bridge()
                .TrimAllSupportedFieldsFromRemoteSpecifics(
                    specifics_with_known_and_unknown_fields)
                .SerializeAsString(),
            specifics_with_only_unknown_fields.SerializePartialAsString());
}
}  // namespace autofill
