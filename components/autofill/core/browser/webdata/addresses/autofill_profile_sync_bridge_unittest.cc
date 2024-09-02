// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/autofill/core/browser/webdata/addresses/autofill_profile_sync_bridge.h"

#include <stddef.h>

#include <memory>
#include <utility>

#include "base/feature_list.h"
#include "base/files/scoped_temp_dir.h"
#include "base/functional/bind.h"
#include "base/functional/callback_helpers.h"
#include "base/location.h"
#include "base/memory/ptr_util.h"
#include "base/run_loop.h"
#include "base/strings/utf_string_conversions.h"
#include "base/test/bind.h"
#include "base/test/scoped_feature_list.h"
#include "base/test/task_environment.h"
#include "base/time/time.h"
#include "base/uuid.h"
#include "components/autofill/core/browser/data_model/autofill_profile.h"
#include "components/autofill/core/browser/data_model/autofill_profile_test_api.h"
#include "components/autofill/core/browser/test_autofill_clock.h"
#include "components/autofill/core/browser/webdata/addresses/address_autofill_table.h"
#include "components/autofill/core/browser/webdata/addresses/autofill_profile_sync_util.h"
#include "components/autofill/core/browser/webdata/autofill_change.h"
#include "components/autofill/core/browser/webdata/autofill_sync_metadata_table.h"
#include "components/autofill/core/browser/webdata/mock_autofill_webdata_backend.h"
#include "components/autofill/core/common/autofill_constants.h"
#include "components/autofill/core/common/autofill_features.h"
#include "components/sync/base/client_tag_hash.h"
#include "components/sync/engine/data_type_activation_response.h"
#include "components/sync/model/client_tag_based_data_type_processor.h"
#include "components/sync/model/data_batch.h"
#include "components/sync/model/data_type_activation_request.h"
#include "components/sync/model/sync_data.h"
#include "components/sync/protocol/autofill_specifics.pb.h"
#include "components/sync/protocol/data_type_state.pb.h"
#include "components/sync/protocol/entity_data.h"
#include "components/sync/protocol/entity_specifics.pb.h"
#include "components/sync/test/mock_commit_queue.h"
#include "components/sync/test/mock_data_type_local_change_processor.h"
#include "components/webdata/common/web_database.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace autofill {
namespace {

using base::ScopedTempDir;
using base::UTF16ToUTF8;
using base::UTF8ToUTF16;
using sync_pb::AutofillProfileSpecifics;
using syncer::DataBatch;
using syncer::DataType;
using syncer::EntityChange;
using syncer::EntityChangeList;
using syncer::EntityData;
using syncer::KeyAndData;
using syncer::MockDataTypeLocalChangeProcessor;
using testing::_;
using testing::DoAll;
using testing::ElementsAre;
using testing::Eq;
using testing::Property;
using testing::Return;
using testing::UnorderedElementsAre;

// Some guids for testing.
const char kGuidA[] = "EDC609ED-7EEE-4F27-B00C-423242A9C44A";
const char kGuidB[] = "EDC609ED-7EEE-4F27-B00C-423242A9C44B";
const char kGuidC[] = "EDC609ED-7EEE-4F27-B00C-423242A9C44C";
const char kGuidD[] = "EDC609ED-7EEE-4F27-B00C-423242A9C44D";
const char kGuidInvalid[] = "EDC609ED-7EEE-4F27-B00C";
const int kValidityStateBitfield = 1984;
const char kLocaleString[] = "en-US";
const base::Time kJune2017 = base::Time::FromSecondsSinceUnixEpoch(1497552271);

AutofillProfile CreateAutofillProfile(
    const AutofillProfileSpecifics& specifics) {
  // As more copying does not hurt in tests, we prefer to use AutofillProfile
  // instead of std::unique_ptr<AutofillProfile> because of code brevity.
  return *CreateAutofillProfileFromSpecifics(specifics);
}

AutofillProfileSpecifics CreateAutofillProfileSpecifics(
    const AutofillProfile& entry) {
  // Reuse production code. We do not need EntityData, just take out the
  // specifics.
  std::unique_ptr<EntityData> entity_data =
      CreateEntityDataFromAutofillProfile(entry);
  return entity_data->specifics.autofill_profile();
}

AutofillProfileSpecifics CreateAutofillProfileSpecifics(
    const std::string& guid) {
  AutofillProfileSpecifics specifics;
  specifics.set_guid(guid);
  // Make it consistent with the constructor of AutofillProfile constructor (the
  // clock value is overrided by TestAutofillClock in the test fixture).
  specifics.set_use_count(1);
  specifics.set_use_date(kJune2017.ToTimeT());
  return specifics;
}

MATCHER_P(HasSpecifics, expected, "") {
  AutofillProfile arg_profile =
      CreateAutofillProfile(arg->specifics.autofill_profile());
  AutofillProfile expected_profile = CreateAutofillProfile(expected);
  if (!test_api(arg_profile).EqualsIncludingUsageStats(expected_profile)) {
    *result_listener << "entry\n[" << arg_profile << "]\n"
                     << "did not match expected\n[" << expected_profile << "]";
    return false;
  }
  return true;
}

MATCHER_P(WithUsageStats, expected, "") {
  AutofillProfile arg_profile = arg;
  if (!test_api(arg_profile).EqualsIncludingUsageStats(expected)) {
    *result_listener << "entry\n[" << arg << "]\n"
                     << "did not match expected\n[" << expected << "]";
    return false;
  }
  return true;
}

void ExtractAutofillProfilesFromDataBatch(
    std::unique_ptr<DataBatch> batch,
    std::vector<AutofillProfile>* output) {
  while (batch->HasNext()) {
    const KeyAndData& data_pair = batch->Next();
    output->push_back(
        CreateAutofillProfile(data_pair.second->specifics.autofill_profile()));
  }
}

// Returns a profile with all fields set.  Contains identical data to the data
// returned from ConstructCompleteSpecifics().
AutofillProfile ConstructCompleteProfile() {
  AutofillProfile profile(kGuidA, AutofillProfile::RecordType::kLocalOrSyncable,
                          AddressCountryCode("ES"));

  profile.set_use_count(7);
  profile.set_use_date(base::Time::FromTimeT(1423182152));

  profile.SetRawInfo(NAME_FULL, u"John K. Doe, Jr.");
  profile.SetRawInfo(NAME_FIRST, u"John");
  profile.SetRawInfo(NAME_MIDDLE, u"K.");
  profile.SetRawInfo(NAME_LAST, u"Doe");
  profile.SetRawInfo(NAME_LAST_FIRST, u"D");
  profile.SetRawInfo(NAME_LAST_CONJUNCTION, u"o");
  profile.SetRawInfo(NAME_LAST_SECOND, u"e");

  profile.SetRawInfo(EMAIL_ADDRESS, u"user@example.com");
  profile.SetRawInfo(PHONE_HOME_WHOLE_NUMBER, u"1.800.555.1234");

  profile.SetRawInfo(ADDRESS_HOME_STREET_ADDRESS, u"123 Fake St.\nApt. 42");
  EXPECT_EQ(u"123 Fake St.", profile.GetRawInfo(ADDRESS_HOME_LINE1));
  EXPECT_EQ(u"Apt. 42", profile.GetRawInfo(ADDRESS_HOME_LINE2));

  profile.SetRawInfo(COMPANY_NAME, u"Google, Inc.");
  profile.SetRawInfo(ADDRESS_HOME_CITY, u"Mountain View");
  profile.SetRawInfo(ADDRESS_HOME_STATE, u"California");
  profile.SetRawInfo(ADDRESS_HOME_ZIP, u"94043");
  profile.SetRawInfo(ADDRESS_HOME_SORTING_CODE, u"CEDEX");
  profile.SetRawInfo(ADDRESS_HOME_DEPENDENT_LOCALITY, u"Santa Clara");
  profile.SetRawInfo(ADDRESS_HOME_STREET_NAME, u"Street Name");
  profile.SetRawInfo(ADDRESS_HOME_HOUSE_NUMBER, u"House Number");
  profile.SetRawInfo(ADDRESS_HOME_SUBPREMISE, u"Subpremise");
  profile.set_language_code("en");
  profile.FinalizeAfterImport();
  return profile;
}

// Returns AutofillProfileSpecifics with all Autofill profile fields set.
// Contains identical data to the data returned from ConstructCompleteProfile().
AutofillProfileSpecifics ConstructCompleteSpecifics() {
  AutofillProfileSpecifics specifics;

  specifics.set_guid(kGuidA);
  specifics.set_use_count(7);
  specifics.set_use_date(1423182152);

  specifics.add_name_first("John");
  specifics.add_name_middle("K.");
  specifics.add_name_last("Doe");
  specifics.add_name_full("John K. Doe, Jr.");
  specifics.add_name_last_first("D");
  specifics.add_name_last_conjunction("o");
  specifics.add_name_last_second("e");

  specifics.add_name_first_status(
      sync_pb::
          AutofillProfileSpecifics_VerificationStatus_VERIFICATION_STATUS_UNSPECIFIED);
  specifics.add_name_middle_status(
      sync_pb::
          AutofillProfileSpecifics_VerificationStatus_VERIFICATION_STATUS_UNSPECIFIED);
  specifics.add_name_last_status(
      sync_pb::
          AutofillProfileSpecifics_VerificationStatus_VERIFICATION_STATUS_UNSPECIFIED);
  specifics.add_name_full_status(
      sync_pb::
          AutofillProfileSpecifics_VerificationStatus_VERIFICATION_STATUS_UNSPECIFIED);
  specifics.add_name_last_first_status(
      sync_pb::
          AutofillProfileSpecifics_VerificationStatus_VERIFICATION_STATUS_UNSPECIFIED);
  specifics.add_name_last_conjunction_status(
      sync_pb::
          AutofillProfileSpecifics_VerificationStatus_VERIFICATION_STATUS_UNSPECIFIED);
  specifics.add_name_last_second_status(
      sync_pb::
          AutofillProfileSpecifics_VerificationStatus_VERIFICATION_STATUS_UNSPECIFIED);

  specifics.add_email_address("user@example.com");

  specifics.add_phone_home_whole_number("1.800.555.1234");

  specifics.set_address_home_line1("123 Fake St.");
  specifics.set_address_home_line2("Apt. 42");
  specifics.set_address_home_street_address(
      "123 Fake St.\n"
      "Apt. 42");

  specifics.set_company_name("Google, Inc.");
  specifics.set_address_home_city("Mountain View");
  specifics.set_address_home_state("California");
  specifics.set_address_home_zip("94043");
  specifics.set_address_home_country("ES");
  specifics.set_address_home_sorting_code("CEDEX");
  specifics.set_address_home_dependent_locality("Santa Clara");
  specifics.set_address_home_language_code("en");

  specifics.set_address_home_thoroughfare_name("Street Name");
  specifics.set_address_home_thoroughfare_number("House Number");
  specifics.set_address_home_subpremise_name("Subpremise");

  specifics.set_validity_state_bitfield(kValidityStateBitfield);
  return specifics;
}

class AutofillProfileSyncBridgeTest : public testing::Test {
 public:
  AutofillProfileSyncBridgeTest() = default;

  AutofillProfileSyncBridgeTest(const AutofillProfileSyncBridgeTest&) = delete;
  AutofillProfileSyncBridgeTest& operator=(
      const AutofillProfileSyncBridgeTest&) = delete;

  ~AutofillProfileSyncBridgeTest() override = default;

  void SetUp() override {
    // Fix a time for implicitly constructed use_dates in AutofillProfile.
    test_clock_.SetNow(kJune2017);
    ASSERT_TRUE(temp_dir_.CreateUniqueTempDir());
    db_.AddTable(&table_);
    db_.AddTable(&sync_metadata_table_);
    db_.Init(temp_dir_.GetPath().AppendASCII("SyncTestWebDatabase"));
    ON_CALL(*backend(), GetDatabase()).WillByDefault(Return(&db_));
    ResetProcessor();
    ResetBridge();
  }

  void ResetProcessor() {
    real_processor_ = std::make_unique<syncer::ClientTagBasedDataTypeProcessor>(
        syncer::AUTOFILL_PROFILE, /*dump_stack=*/base::DoNothing());
    mock_processor_.DelegateCallsByDefaultTo(real_processor_.get());
  }

  void ResetBridge() {
    bridge_ = std::make_unique<AutofillProfileSyncBridge>(
        mock_processor_.CreateForwardingProcessor(), kLocaleString, &backend_);
  }

  void StartSyncing(
      const std::vector<AutofillProfileSpecifics>& remote_data = {}) {
    base::RunLoop loop;
    syncer::DataTypeActivationRequest request;
    request.error_handler = base::DoNothing();
    real_processor_->OnSyncStarting(
        request,
        base::BindLambdaForTesting(
            [&loop](std::unique_ptr<syncer::DataTypeActivationResponse>) {
              loop.Quit();
            }));
    loop.Run();

    // ClientTagBasedDataTypeProcessor requires connecting before other
    // interactions with the worker happen.
    real_processor_->ConnectSync(
        std::make_unique<testing::NiceMock<syncer::MockCommitQueue>>());

    // Initialize the processor with the initial sync already done.
    sync_pb::DataTypeState state;
    state.set_initial_sync_state(
        sync_pb::DataTypeState_InitialSyncState_INITIAL_SYNC_DONE);
    syncer::UpdateResponseDataList initial_updates;
    for (const AutofillProfileSpecifics& specifics : remote_data) {
      initial_updates.push_back(SpecificsToUpdateResponse(specifics));
    }
    real_processor_->OnUpdateReceived(state, std::move(initial_updates),
                                      /*gc_directive=*/std::nullopt);
  }

  void ApplyIncrementalSyncChanges(EntityChangeList changes) {
    EXPECT_CALL(*backend(),
                NotifyOnAutofillChangedBySync(syncer::AUTOFILL_PROFILE));
    const std::optional<syncer::ModelError> error =
        bridge()->ApplyIncrementalSyncChanges(
            bridge()->CreateMetadataChangeList(), std::move(changes));
    EXPECT_FALSE(error) << error->ToString();
  }

  void AddAutofillProfilesToTable(
      const std::vector<AutofillProfile>& profile_list) {
    for (const auto& profile : profile_list) {
      table_.AddAutofillProfile(profile);
    }
  }

  std::vector<AutofillProfile> GetAllLocalData() {
    std::vector<AutofillProfile> data;
    ExtractAutofillProfilesFromDataBatch(bridge()->GetAllDataForDebugging(),
                                         &data);
    return data;
  }

  EntityData SpecificsToEntity(const AutofillProfileSpecifics& specifics) {
    EntityData data;
    *data.specifics.mutable_autofill_profile() = specifics;
    data.client_tag_hash = syncer::ClientTagHash::FromUnhashed(
        syncer::AUTOFILL_PROFILE, bridge()->GetClientTag(data));
    return data;
  }

  syncer::UpdateResponseData SpecificsToUpdateResponse(
      const AutofillProfileSpecifics& specifics) {
    syncer::UpdateResponseData data;
    data.entity = SpecificsToEntity(specifics);
    return data;
  }

  AutofillProfileSyncBridge* bridge() { return bridge_.get(); }

  syncer::MockDataTypeLocalChangeProcessor& mock_processor() {
    return mock_processor_;
  }

  AddressAutofillTable* table() { return &table_; }

  MockAutofillWebDataBackend* backend() { return &backend_; }

 private:
  autofill::TestAutofillClock test_clock_;
  ScopedTempDir temp_dir_;
  base::test::SingleThreadTaskEnvironment task_environment_;
  testing::NiceMock<MockAutofillWebDataBackend> backend_;
  AddressAutofillTable table_;
  AutofillSyncMetadataTable sync_metadata_table_;
  WebDatabase db_;
  testing::NiceMock<MockDataTypeLocalChangeProcessor> mock_processor_;
  std::unique_ptr<syncer::ClientTagBasedDataTypeProcessor> real_processor_;
  std::unique_ptr<AutofillProfileSyncBridge> bridge_;
};

TEST_F(AutofillProfileSyncBridgeTest, AutofillProfileChanged_Added) {
  StartSyncing({});

  AutofillProfile local(kGuidA, AutofillProfile::RecordType::kLocalOrSyncable,
                        i18n_model_definition::kLegacyHierarchyCountryCode);
  local.SetRawInfo(NAME_FIRST, u"Jane");
  local.FinalizeAfterImport();
  AutofillProfileChange change(AutofillProfileChange::ADD, kGuidA, local);

  EXPECT_CALL(
      mock_processor(),
      Put(kGuidA, HasSpecifics(CreateAutofillProfileSpecifics(local)), _));
  // The bridge does not need to commit when reacting to a notification about a
  // local change.
  EXPECT_CALL(*backend(), CommitChanges()).Times(0);

  bridge()->AutofillProfileChanged(change);
}

// Language code in autofill profiles should be synced to the server.
TEST_F(AutofillProfileSyncBridgeTest,
       AutofillProfileChanged_Added_LanguageCodePropagates) {
  StartSyncing({});

  AutofillProfile local(kGuidA, AutofillProfile::RecordType::kLocalOrSyncable,
                        i18n_model_definition::kLegacyHierarchyCountryCode);
  local.set_language_code("en");
  AutofillProfileChange change(AutofillProfileChange::ADD, kGuidA, local);

  EXPECT_CALL(
      mock_processor(),
      Put(kGuidA, HasSpecifics(CreateAutofillProfileSpecifics(local)), _));
  // The bridge does not need to commit when reacting to a notification about a
  // local change.
  EXPECT_CALL(*backend(), CommitChanges()).Times(0);

  bridge()->AutofillProfileChanged(change);
}

// Validity state bitfield in autofill profiles should be synced to the server.
TEST_F(AutofillProfileSyncBridgeTest,
       AutofillProfileChanged_Added_LocalValidityBitfieldPropagates) {
  StartSyncing({});

  AutofillProfile local(kGuidA, AutofillProfile::RecordType::kLocalOrSyncable,
                        i18n_model_definition::kLegacyHierarchyCountryCode);
  AutofillProfileChange change(AutofillProfileChange::ADD, kGuidA, local);

  EXPECT_CALL(
      mock_processor(),
      Put(kGuidA, HasSpecifics(CreateAutofillProfileSpecifics(local)), _));
  // The bridge does not need to commit when reacting to a notification about a
  // local change.
  EXPECT_CALL(*backend(), CommitChanges()).Times(0);

  bridge()->AutofillProfileChanged(change);
}

// Local updates should be properly propagated to the server.
TEST_F(AutofillProfileSyncBridgeTest, AutofillProfileChanged_Updated) {
  StartSyncing({});

  AutofillProfile local(kGuidA, AutofillProfile::RecordType::kLocalOrSyncable,
                        i18n_model_definition::kLegacyHierarchyCountryCode);
  local.SetRawInfo(NAME_FIRST, u"Jane");
  AutofillProfileChange change(AutofillProfileChange::UPDATE, kGuidA, local);

  EXPECT_CALL(
      mock_processor(),
      Put(kGuidA, HasSpecifics(CreateAutofillProfileSpecifics(local)), _));
  // The bridge does not need to commit when reacting to a notification about a
  // local change.
  EXPECT_CALL(*backend(), CommitChanges()).Times(0);

  bridge()->AutofillProfileChanged(change);
}

// Usage stats should be updated by the client.
TEST_F(AutofillProfileSyncBridgeTest,
       AutofillProfileChanged_Updated_UsageStatsOverwrittenByClient) {
  // Remote data has a profile with usage stats.
  AutofillProfileSpecifics remote = CreateAutofillProfileSpecifics(kGuidA);
  remote.set_address_home_language_code("en");
  remote.set_use_count(9);
  remote.set_use_date(25);

  StartSyncing({remote});
  EXPECT_THAT(GetAllLocalData(),
              ElementsAre(WithUsageStats(CreateAutofillProfile(remote))));

  // Update to the usage stats for that profile.
  AutofillProfile local(kGuidA, AutofillProfile::RecordType::kLocalOrSyncable,
                        i18n_model_definition::kLegacyHierarchyCountryCode);
  local.set_language_code("en");
  local.set_use_count(10U);
  local.set_use_date(base::Time::FromTimeT(30));
  AutofillProfileChange change(AutofillProfileChange::UPDATE, kGuidA, local);

  EXPECT_CALL(
      mock_processor(),
      Put(kGuidA, HasSpecifics(CreateAutofillProfileSpecifics(local)), _));
  // The bridge does not need to commit when reacting to a notification about a
  // local change.
  EXPECT_CALL(*backend(), CommitChanges()).Times(0);

  bridge()->AutofillProfileChanged(change);
}

TEST_F(AutofillProfileSyncBridgeTest, AutofillProfileChanged_Deleted) {
  StartSyncing({});

  AutofillProfile local(kGuidB, AutofillProfile::RecordType::kLocalOrSyncable,
                        i18n_model_definition::kLegacyHierarchyCountryCode);
  local.SetRawInfo(NAME_FIRST, u"Jane");
  AutofillProfileChange change(AutofillProfileChange::REMOVE, kGuidB, local);
  EXPECT_CALL(mock_processor(), Delete(kGuidB, _, _));
  // The bridge does not need to commit when reacting to a notification about a
  // local change.
  EXPECT_CALL(*backend(), CommitChanges()).Times(0);

  bridge()->AutofillProfileChanged(change);
}

TEST_F(AutofillProfileSyncBridgeTest, GetAllDataForDebugging) {
  AutofillProfile local1(kGuidA, AutofillProfile::RecordType::kLocalOrSyncable,
                         i18n_model_definition::kLegacyHierarchyCountryCode);
  local1.SetRawInfo(NAME_FIRST, u"John");
  local1.SetRawInfo(ADDRESS_HOME_STREET_ADDRESS, u"1 1st st");
  local1.FinalizeAfterImport();
  AutofillProfile local2(kGuidB, AutofillProfile::RecordType::kLocalOrSyncable,
                         i18n_model_definition::kLegacyHierarchyCountryCode);
  local2.SetRawInfo(NAME_FIRST, u"Tom");
  local2.SetRawInfo(ADDRESS_HOME_STREET_ADDRESS, u"2 2nd st");
  local2.FinalizeAfterImport();
  AddAutofillProfilesToTable({local1, local2});

  EXPECT_THAT(GetAllLocalData(), UnorderedElementsAre(local1, local2));
}

TEST_F(AutofillProfileSyncBridgeTest, GetDataForCommit) {
  AutofillProfile local1(kGuidA, AutofillProfile::RecordType::kLocalOrSyncable,
                         i18n_model_definition::kLegacyHierarchyCountryCode);
  local1.SetRawInfo(NAME_FIRST, u"John");
  local1.SetRawInfo(ADDRESS_HOME_STREET_ADDRESS, u"1 1st st");
  local1.FinalizeAfterImport();
  AutofillProfile local2(kGuidB, AutofillProfile::RecordType::kLocalOrSyncable,
                         i18n_model_definition::kLegacyHierarchyCountryCode);
  local2.SetRawInfo(NAME_FIRST, u"Tom");
  local2.SetRawInfo(ADDRESS_HOME_STREET_ADDRESS, u"2 2nd st");
  local2.FinalizeAfterImport();
  AddAutofillProfilesToTable({local1, local2});

  std::vector<AutofillProfile> data;
  ExtractAutofillProfilesFromDataBatch(bridge()->GetDataForCommit({kGuidA}),
                                       &data);

  EXPECT_THAT(data, ElementsAre(local1));
}

TEST_F(AutofillProfileSyncBridgeTest, MergeFullSyncData) {
  AutofillProfile local1(kGuidA, AutofillProfile::RecordType::kLocalOrSyncable,
                         i18n_model_definition::kLegacyHierarchyCountryCode);
  local1.SetRawInfo(NAME_FIRST, u"John");
  local1.SetRawInfo(ADDRESS_HOME_STREET_ADDRESS, u"1 1st st");
  local1.FinalizeAfterImport();
  AutofillProfile local2(kGuidB, AutofillProfile::RecordType::kLocalOrSyncable,
                         i18n_model_definition::kLegacyHierarchyCountryCode);
  local2.SetRawInfo(NAME_FIRST, u"Tom");
  local2.SetRawInfo(ADDRESS_HOME_STREET_ADDRESS, u"2 2nd st");
  local2.FinalizeAfterImport();

  AddAutofillProfilesToTable({local1, local2});

  AutofillProfile remote1(kGuidC, AutofillProfile::RecordType::kLocalOrSyncable,
                          i18n_model_definition::kLegacyHierarchyCountryCode);
  remote1.SetRawInfo(NAME_FIRST, u"Jane");
  remote1.FinalizeAfterImport();

  AutofillProfile remote2(kGuidD, AutofillProfile::RecordType::kLocalOrSyncable,
                          i18n_model_definition::kLegacyHierarchyCountryCode);
  remote2.SetRawInfo(NAME_FIRST, u"Harry");
  remote2.FinalizeAfterImport();

  AutofillProfile remote3(kGuidB, AutofillProfile::RecordType::kLocalOrSyncable,
                          i18n_model_definition::kLegacyHierarchyCountryCode);
  remote3.SetRawInfo(NAME_FIRST, u"Tom Doe");
  remote3.FinalizeAfterImport();

  AutofillProfileSpecifics remote1_specifics =
      CreateAutofillProfileSpecifics(remote1);
  AutofillProfileSpecifics remote2_specifics =
      CreateAutofillProfileSpecifics(remote2);
  AutofillProfileSpecifics remote3_specifics =
      CreateAutofillProfileSpecifics(remote3);

  EXPECT_CALL(
      mock_processor(),
      Put(kGuidA, HasSpecifics(CreateAutofillProfileSpecifics(local1)), _));
  EXPECT_CALL(mock_processor(), Delete).Times(0);
  EXPECT_CALL(*backend(), CommitChanges());

  StartSyncing({remote1_specifics, remote2_specifics, remote3_specifics});

  // Since |local2| and |remote3| have the same GUID, data from |remote3| is
  // incorporated into the local profile which is mostly a replace operation.
  EXPECT_THAT(
      GetAllLocalData(),
      UnorderedElementsAre(local1, CreateAutofillProfile(remote1_specifics),
                           CreateAutofillProfile(remote2_specifics),
                           CreateAutofillProfile(remote3_specifics)));
}

// Tests the profile migration that is performed after specifics are converted
// to profiles.
TEST_F(AutofillProfileSyncBridgeTest, ProfileMigration) {
  AutofillProfile remote1(kGuidC, AutofillProfile::RecordType::kLocalOrSyncable,
                          i18n_model_definition::kLegacyHierarchyCountryCode);
  remote1.SetRawInfo(NAME_FIRST, u"Thomas");
  remote1.SetRawInfo(NAME_MIDDLE, u"Neo");
  remote1.SetRawInfo(NAME_LAST, u"Anderson");

  AutofillProfileSpecifics remote1_specifics =
      CreateAutofillProfileSpecifics(remote1);

  EXPECT_CALL(*backend(), CommitChanges());

  StartSyncing({remote1_specifics});

  // Create the expected profile after migration.
  AutofillProfile finalized_profile(
      kGuidC, AutofillProfile::RecordType::kLocalOrSyncable,
      i18n_model_definition::kLegacyHierarchyCountryCode);
  finalized_profile.SetRawInfoWithVerificationStatus(
      NAME_FULL, u"Thomas Neo Anderson", VerificationStatus::kFormatted);
  finalized_profile.SetRawInfoWithVerificationStatus(
      NAME_FIRST, u"Thomas", VerificationStatus::kObserved);
  finalized_profile.SetRawInfoWithVerificationStatus(
      NAME_MIDDLE, u"Neo", VerificationStatus::kObserved);
  finalized_profile.SetRawInfoWithVerificationStatus(
      NAME_LAST, u"Anderson", VerificationStatus::kObserved);
  finalized_profile.SetRawInfoWithVerificationStatus(
      NAME_LAST_SECOND, u"Anderson", VerificationStatus::kParsed);
  finalized_profile.SetRawInfoWithVerificationStatus(
      NAME_LAST_FIRST, u"", VerificationStatus::kParsed);
  finalized_profile.SetRawInfoWithVerificationStatus(
      NAME_LAST_CONJUNCTION, u"", VerificationStatus::kParsed);

  EXPECT_THAT(GetAllLocalData(), UnorderedElementsAre(finalized_profile));
}

// Ensure that all profile fields are able to be synced up from the client to
// the server.
TEST_F(AutofillProfileSyncBridgeTest, MergeFullSyncData_SyncAllFieldsToServer) {
  AutofillProfile local = ConstructCompleteProfile();
  local.FinalizeAfterImport();
  AddAutofillProfilesToTable({local});

  // This complete profile is fully uploaded to sync.
  EXPECT_CALL(mock_processor(),
              Put(_, HasSpecifics(ConstructCompleteSpecifics()), _));
  EXPECT_CALL(*backend(), CommitChanges());

  StartSyncing({});

  // No changes locally.
  EXPECT_THAT(GetAllLocalData(), ElementsAre(WithUsageStats(local)));
}

// Ensure that all profile fields are able to be synced down from the server to
// the client (and nothing gets uploaded back).
TEST_F(AutofillProfileSyncBridgeTest, MergeFullSyncData_SyncAllFieldsToClient) {
  EXPECT_CALL(mock_processor(), Put).Times(0);
  EXPECT_CALL(*backend(), CommitChanges());
  StartSyncing({ConstructCompleteSpecifics()});

  EXPECT_THAT(GetAllLocalData(),
              ElementsAre(WithUsageStats(ConstructCompleteProfile())));
}

TEST_F(AutofillProfileSyncBridgeTest, MergeFullSyncData_IdenticalProfiles) {
  AutofillProfile local1(kGuidA, AutofillProfile::RecordType::kLocalOrSyncable,
                         i18n_model_definition::kLegacyHierarchyCountryCode);
  local1.SetRawInfoWithVerificationStatus(NAME_FIRST, u"John",
                                          VerificationStatus::kObserved);
  local1.SetRawInfoWithVerificationStatus(
      ADDRESS_HOME_STREET_ADDRESS, u"1 1st st", VerificationStatus::kObserved);
  local1.FinalizeAfterImport();

  AutofillProfile local2(kGuidB, AutofillProfile::RecordType::kLocalOrSyncable,
                         i18n_model_definition::kLegacyHierarchyCountryCode);
  local2.SetRawInfoWithVerificationStatus(NAME_FIRST, u"Tom",
                                          VerificationStatus::kObserved);
  local2.SetRawInfoWithVerificationStatus(
      ADDRESS_HOME_STREET_ADDRESS, u"2 2nd st", VerificationStatus::kObserved);
  local2.FinalizeAfterImport();
  AddAutofillProfilesToTable({local1, local2});

  // The synced profiles are identical to the local ones, except that the guids
  // are different.
  AutofillProfile remote1(kGuidC, AutofillProfile::RecordType::kLocalOrSyncable,
                          i18n_model_definition::kLegacyHierarchyCountryCode);
  remote1.SetRawInfoWithVerificationStatus(NAME_FIRST, u"John",
                                           VerificationStatus::kObserved);
  remote1.SetRawInfoWithVerificationStatus(
      ADDRESS_HOME_STREET_ADDRESS, u"1 1st st", VerificationStatus::kObserved);
  remote1.FinalizeAfterImport();

  AutofillProfile remote2(kGuidD, AutofillProfile::RecordType::kLocalOrSyncable,
                          i18n_model_definition::kLegacyHierarchyCountryCode);
  remote2.SetRawInfoWithVerificationStatus(NAME_FIRST, u"Tom",
                                           VerificationStatus::kObserved);
  remote2.SetRawInfoWithVerificationStatus(
      ADDRESS_HOME_STREET_ADDRESS, u"2 2nd st", VerificationStatus::kObserved);
  remote2.FinalizeAfterImport();

  AutofillProfileSpecifics remote1_specifics =
      CreateAutofillProfileSpecifics(remote1);
  AutofillProfileSpecifics remote2_specifics =
      CreateAutofillProfileSpecifics(remote2);

  EXPECT_CALL(*backend(), CommitChanges());

  StartSyncing({remote1_specifics, remote2_specifics});

  EXPECT_THAT(GetAllLocalData(),
              UnorderedElementsAre(CreateAutofillProfile(remote1_specifics),
                                   CreateAutofillProfile(remote2_specifics)));
}

TEST_F(AutofillProfileSyncBridgeTest, MergeFullSyncData_NonSimilarProfiles) {
  AutofillProfile local = ConstructCompleteProfile();
  local.set_guid(kGuidA);
  local.SetRawInfo(NAME_FULL, u"John K. Doe, Jr.");
  local.SetRawInfo(NAME_FIRST, u"John");
  local.SetRawInfo(NAME_MIDDLE, u"K.");
  local.SetRawInfo(NAME_LAST, u"Doe");
  local.FinalizeAfterImport();
  AddAutofillProfilesToTable({local});

  // The remote profile are not similar as the names are different (all other
  // fields except for guids are identical).
  AutofillProfileSpecifics remote = ConstructCompleteSpecifics();
  remote.set_guid(kGuidB);
  remote.set_name_full(0, "Jane T. Roe, Sr.");
  remote.set_name_first(0, "Jane");
  remote.set_name_middle(0, "T.");
  remote.set_name_last(0, "Roe");

  // The profiles are not similar enough and thus do not get merged.
  // Expect the local one being synced up and the remote one being added to the
  // local database.
  EXPECT_CALL(
      mock_processor(),
      Put(kGuidA, HasSpecifics(CreateAutofillProfileSpecifics(local)), _));
  EXPECT_CALL(mock_processor(), Delete).Times(0);
  EXPECT_CALL(*backend(), CommitChanges());

  StartSyncing({remote});

  EXPECT_THAT(GetAllLocalData(),
              UnorderedElementsAre(local, CreateAutofillProfile(remote)));
}

TEST_F(AutofillProfileSyncBridgeTest, MergeFullSyncData_SimilarProfiles) {
  AutofillProfile local(kGuidA, AutofillProfile::RecordType::kLocalOrSyncable,
                        i18n_model_definition::kLegacyHierarchyCountryCode);
  local.SetRawInfo(NAME_FIRST, u"John");
  local.SetRawInfo(ADDRESS_HOME_STREET_ADDRESS, u"1 1st st");
  local.FinalizeAfterImport();
  local.set_use_count(27);
  AddAutofillProfilesToTable({local});

  // The synced profiles are identical to the local ones, except that the guids
  // and use_count values are different. Remote ones have additional company
  // name which makes them not be identical.
  AutofillProfile remote = local;
  remote.set_guid(kGuidB);
  remote.set_use_count(13);
  remote.SetRawInfo(COMPANY_NAME, u"Frobbers, Inc.");
  // Note, this populates the full name for structured profiles.
  remote.FinalizeAfterImport();

  AutofillProfileSpecifics remote_specifics =
      CreateAutofillProfileSpecifics(remote);

  AutofillProfileSpecifics merged(remote_specifics);
  // Merging two profile takes their max use count.
  merged.set_use_count(27);

  // Expect updating the first (merged) profile and adding the second local one.
  EXPECT_CALL(mock_processor(), Put(kGuidB, HasSpecifics(merged), _));
  EXPECT_CALL(mock_processor(), Delete).Times(0);
  EXPECT_CALL(*backend(), CommitChanges());

  StartSyncing({remote_specifics});

  EXPECT_THAT(
      GetAllLocalData(),
      UnorderedElementsAre(WithUsageStats(CreateAutofillProfile(merged))));
}

// Tests that MergeSimilarProfiles keeps the most recent use date of the two
// profiles being merged.
TEST_F(AutofillProfileSyncBridgeTest,
       MergeFullSyncData_SimilarProfiles_OlderUseDate) {
  // Different guids, difference in the phone number.
  AutofillProfile local(kGuidA, AutofillProfile::RecordType::kLocalOrSyncable,
                        i18n_model_definition::kLegacyHierarchyCountryCode);
  local.SetRawInfo(PHONE_HOME_WHOLE_NUMBER, u"650234567");
  local.set_use_date(base::Time::FromTimeT(30));
  AddAutofillProfilesToTable({local});

  AutofillProfileSpecifics remote = CreateAutofillProfileSpecifics(kGuidB);
  // |local| has a more recent use date.
  remote.set_use_date(25);

  // The use date of |local| should replace the use date of |remote|.
  AutofillProfileSpecifics merged(remote);
  merged.set_use_date(30);
  merged.add_phone_home_whole_number("650234567");
  EXPECT_CALL(mock_processor(), Put(kGuidB, HasSpecifics(merged), _));
  EXPECT_CALL(*backend(), CommitChanges());

  StartSyncing({remote});
}

// Tests that MergeSimilarProfiles keeps the most recent use date of the two
// profiles being merged.
TEST_F(AutofillProfileSyncBridgeTest,
       MergeFullSyncData_SimilarProfiles_NewerUseDate) {
  // Different guids, difference in the phone number.
  AutofillProfile local(kGuidA, AutofillProfile::RecordType::kLocalOrSyncable,
                        i18n_model_definition::kLegacyHierarchyCountryCode);
  local.SetRawInfo(PHONE_HOME_WHOLE_NUMBER, u"650234567");
  local.set_use_date(base::Time::FromTimeT(30));
  AddAutofillProfilesToTable({local});

  AutofillProfileSpecifics remote = CreateAutofillProfileSpecifics(kGuidB);
  // |remote| has a more recent use date.
  remote.set_use_date(35);

  // The use date of |local| should _not_ replace the use date of |remote|.
  AutofillProfileSpecifics merged(remote);
  merged.add_phone_home_whole_number("650234567");
  EXPECT_CALL(mock_processor(), Put(kGuidB, HasSpecifics(merged), _));
  EXPECT_CALL(*backend(), CommitChanges());

  StartSyncing({remote});
}

// Tests that MergeSimilarProfiles saves the max of the use counts of the two
// profiles in |remote|.
TEST_F(AutofillProfileSyncBridgeTest,
       MergeFullSyncData_SimilarProfiles_NonZeroUseCounts) {
  // Different guids, difference in the phone number.
  AutofillProfile local(kGuidA, AutofillProfile::RecordType::kLocalOrSyncable,
                        i18n_model_definition::kLegacyHierarchyCountryCode);
  local.SetRawInfo(PHONE_HOME_WHOLE_NUMBER, u"650234567");
  local.set_use_count(12);
  AddAutofillProfilesToTable({local});

  AutofillProfileSpecifics remote = CreateAutofillProfileSpecifics(kGuidB);
  remote.set_use_count(5);

  // The use count of |local| should replace the use count of |remote|.
  AutofillProfileSpecifics merged(remote);
  merged.set_use_count(12);
  merged.add_phone_home_whole_number("650234567");
  EXPECT_CALL(mock_processor(), Put(kGuidB, HasSpecifics(merged), _));
  EXPECT_CALL(*backend(), CommitChanges());

  StartSyncing({remote});
}

TEST_F(AutofillProfileSyncBridgeTest, ApplyIncrementalSyncChanges) {
  AutofillProfile local(kGuidA, AutofillProfile::RecordType::kLocalOrSyncable,
                        i18n_model_definition::kLegacyHierarchyCountryCode);
  local.FinalizeAfterImport();
  AddAutofillProfilesToTable({local});

  StartSyncing({});

  AutofillProfile remote_profile(
      kGuidB, AutofillProfile::RecordType::kLocalOrSyncable,
      i18n_model_definition::kLegacyHierarchyCountryCode);
  remote_profile.SetRawInfo(NAME_FIRST, u"Jane");
  remote_profile.FinalizeAfterImport();
  AutofillProfileSpecifics remote =
      CreateAutofillProfileSpecifics(remote_profile);

  EXPECT_CALL(mock_processor(), Put).Times(0);
  EXPECT_CALL(mock_processor(), Delete).Times(0);
  EXPECT_CALL(*backend(), CommitChanges());

  syncer::EntityChangeList entity_change_list;
  entity_change_list.push_back(EntityChange::CreateDelete(kGuidA));
  entity_change_list.push_back(
      EntityChange::CreateAdd(kGuidB, SpecificsToEntity(remote)));
  ApplyIncrementalSyncChanges(std::move(entity_change_list));

  EXPECT_THAT(GetAllLocalData(), ElementsAre(CreateAutofillProfile(remote)));
}

// Ensure that entries with invalid specifics are ignored.
TEST_F(AutofillProfileSyncBridgeTest,
       ApplyIncrementalSyncChanges_OmitsInvalidSpecifics) {
  StartSyncing({});

  AutofillProfileSpecifics remote_valid =
      CreateAutofillProfileSpecifics(kGuidA);
  AutofillProfileSpecifics remote_invalid =
      CreateAutofillProfileSpecifics(kGuidInvalid);

  EXPECT_CALL(mock_processor(), Put).Times(0);
  EXPECT_CALL(*backend(), CommitChanges());

  syncer::EntityChangeList entity_change_list;
  entity_change_list.push_back(
      EntityChange::CreateAdd(kGuidA, SpecificsToEntity(remote_valid)));
  entity_change_list.push_back(
      EntityChange::CreateAdd(kGuidInvalid, SpecificsToEntity(remote_invalid)));
  ApplyIncrementalSyncChanges(std::move(entity_change_list));

  EXPECT_THAT(GetAllLocalData(),
              ElementsAre(CreateAutofillProfile(remote_valid)));
}

// Verifies that setting the street address field also sets the (deprecated)
// address line 1 and line 2 fields.
TEST_F(AutofillProfileSyncBridgeTest, StreetAddress_SplitAutomatically) {
  AutofillProfile local(i18n_model_definition::kLegacyHierarchyCountryCode);
  local.SetRawInfo(ADDRESS_HOME_STREET_ADDRESS, u"123 Example St.\nApt. 42");
  EXPECT_EQ(u"123 Example St.", local.GetRawInfo(ADDRESS_HOME_LINE1));
  EXPECT_EQ(u"Apt. 42", local.GetRawInfo(ADDRESS_HOME_LINE2));

  // The same does _not_ work for profile specifics.
  AutofillProfileSpecifics remote;
  remote.set_address_home_street_address(
      "123 Example St.\n"
      "Apt. 42");
  EXPECT_FALSE(remote.has_address_home_line1());
  EXPECT_FALSE(remote.has_address_home_line2());
}

// Verifies that setting the (deprecated) address line 1 and line 2 fields also
// sets the street address.
TEST_F(AutofillProfileSyncBridgeTest, StreetAddress_JointAutomatically) {
  AutofillProfile local(i18n_model_definition::kLegacyHierarchyCountryCode);
  local.SetRawInfo(ADDRESS_HOME_LINE1, u"123 Example St.");
  local.SetRawInfo(ADDRESS_HOME_LINE2, u"Apt. 42");
  EXPECT_EQ(u"123 Example St.\nApt. 42",
            local.GetRawInfo(ADDRESS_HOME_STREET_ADDRESS));

  // The same does _not_ work for profile specifics.
  AutofillProfileSpecifics remote;
  remote.set_address_home_line1("123 Example St.");
  remote.set_address_home_line2("Apt. 42");
  EXPECT_FALSE(remote.has_address_home_street_address());
}

// Ensure that the street address field takes precedence over the (deprecated)
// address line 1 and line 2 fields, even though these are expected to always be
// in sync in practice.
TEST_F(AutofillProfileSyncBridgeTest,
       RemoteWithSameGuid_StreetAddress_TakesPrecedenceOverAddressLines) {
  // Create remote entry with conflicting address data in the street address
  // field vs. the address line 1 and address line 2 fields.
  AutofillProfileSpecifics remote = CreateAutofillProfileSpecifics(kGuidA);
  remote.set_address_home_line1("123 Example St.");
  remote.set_address_home_line2("Apt. 42");
  remote.set_address_home_street_address(
      "456 El Camino Real\n"
      "Suite #1337");
  remote.set_address_home_street_address_status(
      sync_pb::AutofillProfileSpecifics_VerificationStatus_OBSERVED);
  EXPECT_CALL(*backend(), CommitChanges());
  StartSyncing({remote});

  // Verify that full street address takes precedence over address lines.
  AutofillProfile local(kGuidA, AutofillProfile::RecordType::kLocalOrSyncable,
                        i18n_model_definition::kLegacyHierarchyCountryCode);
  local.SetRawInfoWithVerificationStatus(ADDRESS_HOME_STREET_ADDRESS,
                                         u"456 El Camino Real\nSuite #1337",
                                         VerificationStatus::kObserved);
  local.SetRawInfoWithVerificationStatus(
      ADDRESS_HOME_LINE1, u"456 El Camino Real", VerificationStatus::kObserved);
  local.SetRawInfoWithVerificationStatus(ADDRESS_HOME_LINE2, u"Suite #1337",
                                         VerificationStatus::kObserved);
  local.FinalizeAfterImport();
  EXPECT_THAT(GetAllLocalData(), ElementsAre(local));
}

// Ensure that no Sync events are generated to fill in missing street address
// fields from Sync with explicitly present ones identical to the data stored in
// the line1 and line2 fields. This ensures that the migration to add the
// street address field to profiles does not generate lots of needless Sync
// updates.
TEST_F(AutofillProfileSyncBridgeTest,
       RemoteWithSameGuid_StreetAddress_NoUpdateToEmptyStreetAddressSyncedUp) {
  AutofillProfile local(kGuidA, AutofillProfile::RecordType::kLocalOrSyncable,
                        i18n_model_definition::kLegacyHierarchyCountryCode);
  local.SetRawInfoWithVerificationStatus(ADDRESS_HOME_STREET_ADDRESS,
                                         u"123 Example St.\nApt. 42",
                                         VerificationStatus::kObserved);
  local.FinalizeAfterImport();
  AddAutofillProfilesToTable({local});

  // Create a Sync profile identical to |profile|, except without street address
  // explicitly set.
  AutofillProfileSpecifics remote = CreateAutofillProfileSpecifics(kGuidA);
  remote.set_address_home_line1("123 Example St.");
  remote.set_address_home_line2("Apt. 42");

  // No update to sync, no change in local data.
  EXPECT_CALL(mock_processor(), Put).Times(0);
  EXPECT_CALL(*backend(), CommitChanges());
  StartSyncing({remote});
  EXPECT_THAT(GetAllLocalData(), ElementsAre(local));
}

// Missing language code field should not generate sync events.
TEST_F(AutofillProfileSyncBridgeTest,
       RemoteWithSameGuid_LanguageCode_MissingCodesNoSync) {
  AutofillProfile local(kGuidA, AutofillProfile::RecordType::kLocalOrSyncable,
                        i18n_model_definition::kLegacyHierarchyCountryCode);
  ASSERT_TRUE(local.language_code().empty());
  AddAutofillProfilesToTable({local});

  // Remote data does not have a language code value.
  AutofillProfileSpecifics remote = CreateAutofillProfileSpecifics(kGuidA);
  ASSERT_FALSE(remote.has_address_home_language_code());

  // No update to sync, no change in local data.
  EXPECT_CALL(mock_processor(), Put).Times(0);
  EXPECT_CALL(*backend(), CommitChanges());
  StartSyncing({remote});
  EXPECT_THAT(GetAllLocalData(), ElementsAre(local));
}

// Empty language code should be overwritten by sync.
TEST_F(AutofillProfileSyncBridgeTest,
       RemoteWithSameGuid_LanguageCode_ExistingRemoteWinsOverMissingLocal) {
  AutofillProfile local(kGuidA, AutofillProfile::RecordType::kLocalOrSyncable,
                        i18n_model_definition::kLegacyHierarchyCountryCode);
  ASSERT_TRUE(local.language_code().empty());
  AddAutofillProfilesToTable({local});

  // Remote data has "en" language code.
  AutofillProfileSpecifics remote = CreateAutofillProfileSpecifics(kGuidA);
  remote.set_address_home_language_code("en");

  // No update to sync, remote language code overwrites the empty local one.
  EXPECT_CALL(mock_processor(), Put).Times(0);
  EXPECT_CALL(*backend(), CommitChanges());
  StartSyncing({remote});
  EXPECT_THAT(GetAllLocalData(), ElementsAre(CreateAutofillProfile(remote)));
}

// Local language code should be overwritten by remote one.
TEST_F(AutofillProfileSyncBridgeTest,
       RemoteWithSameGuid_LanguageCode_ExistingRemoteWinsOverExistingLocal) {
  AutofillProfile local(kGuidA, AutofillProfile::RecordType::kLocalOrSyncable,
                        i18n_model_definition::kLegacyHierarchyCountryCode);
  local.set_language_code("de");
  AddAutofillProfilesToTable({local});

  // Remote data has "en" language code.
  AutofillProfileSpecifics remote = CreateAutofillProfileSpecifics(kGuidA);
  remote.set_address_home_language_code("en");

  // No update to sync, remote language code overwrites the local one.
  EXPECT_CALL(mock_processor(), Put).Times(0);
  EXPECT_CALL(*backend(), CommitChanges());
  StartSyncing({remote});
  EXPECT_THAT(GetAllLocalData(), ElementsAre(CreateAutofillProfile(remote)));
}

// Sync data without language code should not overwrite existing language code
// in local autofill profile.
TEST_F(AutofillProfileSyncBridgeTest,
       RemoteWithSameGuid_LanguageCode_ExistingLocalWinsOverMissingRemote) {
  // Local autofill profile has "en" language code.
  AutofillProfile local(kGuidA, AutofillProfile::RecordType::kLocalOrSyncable,
                        i18n_model_definition::kLegacyHierarchyCountryCode);
  local.set_language_code("en");
  AddAutofillProfilesToTable({local});

  // Remote data does not have a language code value.
  AutofillProfile remote_profile(
      kGuidA, AutofillProfile::RecordType::kLocalOrSyncable,
      i18n_model_definition::kLegacyHierarchyCountryCode);
  remote_profile.SetRawInfo(NAME_FIRST, u"John");
  remote_profile.FinalizeAfterImport();
  AutofillProfileSpecifics remote =
      CreateAutofillProfileSpecifics(remote_profile);
  ASSERT_TRUE(remote.address_home_language_code().empty());

  // Expect local autofill profile to still have "en" language code after
  AutofillProfile merged(local);
  merged.SetRawInfo(NAME_FIRST, u"John");
  merged.FinalizeAfterImport();

  // No update to sync, remote language code overwrites the local one.
  EXPECT_CALL(mock_processor(), Put).Times(0);
  EXPECT_CALL(*backend(), CommitChanges());
  StartSyncing({remote});
  EXPECT_THAT(GetAllLocalData(), ElementsAre(merged));
}

// Missing validity state bitifield should not generate sync events.
TEST_F(AutofillProfileSyncBridgeTest,
       RemoteWithSameGuid_ValidityState_DefaultValueNoSync) {
  AutofillProfile local(kGuidA, AutofillProfile::RecordType::kLocalOrSyncable,
                        i18n_model_definition::kLegacyHierarchyCountryCode);
  AddAutofillProfilesToTable({local});

  // Remote data does not have a validity state bitfield value.
  AutofillProfileSpecifics remote = CreateAutofillProfileSpecifics(kGuidA);
  ASSERT_FALSE(remote.has_validity_state_bitfield());
  ASSERT_FALSE(remote.is_client_validity_states_updated());

  // No update to sync, no change in local data.
  EXPECT_CALL(mock_processor(), Put).Times(0);
  EXPECT_CALL(*backend(), CommitChanges());
  StartSyncing({remote});
  EXPECT_THAT(GetAllLocalData(), ElementsAre(local));
}

// Default validity state bitfield should be overwritten by sync.
TEST_F(AutofillProfileSyncBridgeTest,
       RemoteWithSameGuid_ValidityState_ExistingRemoteWinsOverMissingLocal) {
  AutofillProfile local(kGuidA, AutofillProfile::RecordType::kLocalOrSyncable,
                        i18n_model_definition::kLegacyHierarchyCountryCode);
  AddAutofillProfilesToTable({local});

  // Remote data has a non default validity state bitfield value.
  AutofillProfileSpecifics remote = CreateAutofillProfileSpecifics(kGuidA);
  remote.set_validity_state_bitfield(kValidityStateBitfield);
  ASSERT_TRUE(remote.has_validity_state_bitfield());

  // No update to sync, the validity bitfield should be stored to local.
  EXPECT_CALL(mock_processor(), Put).Times(0);
  EXPECT_CALL(*backend(), CommitChanges());
  StartSyncing({remote});
  EXPECT_THAT(GetAllLocalData(), ElementsAre(CreateAutofillProfile(remote)));
}

// Local validity state bitfield should be overwritten by sync.
TEST_F(AutofillProfileSyncBridgeTest,
       RemoteWithSameGuid_ValidityState_ExistingRemoteWinsOverExistingLocal) {
  AutofillProfile local(kGuidA, AutofillProfile::RecordType::kLocalOrSyncable,
                        i18n_model_definition::kLegacyHierarchyCountryCode);
  AddAutofillProfilesToTable({local});

  // Remote data has a non default validity state bitfield value.
  AutofillProfileSpecifics remote = CreateAutofillProfileSpecifics(kGuidA);
  remote.set_validity_state_bitfield(kValidityStateBitfield);
  ASSERT_TRUE(remote.has_validity_state_bitfield());

  // No update to sync, the remote validity bitfield should overwrite local.
  EXPECT_CALL(mock_processor(), Put).Times(0);
  EXPECT_CALL(*backend(), CommitChanges());
  StartSyncing({remote});
  EXPECT_THAT(GetAllLocalData(), ElementsAre(CreateAutofillProfile(remote)));
}

// Sync data without a default validity state bitfield should not overwrite
// an existing validity state bitfield in local autofill profile.
TEST_F(AutofillProfileSyncBridgeTest,
       RemoteWithSameGuid_ValidityState_ExistingLocalWinsOverMissingRemote) {
  AutofillProfile local(kGuidA, AutofillProfile::RecordType::kLocalOrSyncable,
                        i18n_model_definition::kLegacyHierarchyCountryCode);
  AddAutofillProfilesToTable({local});

  // Remote data has a non default validity state bitfield value.
  AutofillProfileSpecifics remote = CreateAutofillProfileSpecifics(kGuidA);
  remote.add_name_first("John");
  ASSERT_FALSE(remote.has_validity_state_bitfield());

  // Expect local autofill profile to still have the validity state after.
  AutofillProfile merged(local);
  merged.SetRawInfo(NAME_FIRST, u"John");
  merged.FinalizeAfterImport();

  // No update to sync, the local validity bitfield should stay untouched.
  EXPECT_CALL(mock_processor(), Put).Times(0);
  EXPECT_CALL(*backend(), CommitChanges());
  StartSyncing({remote});
  EXPECT_THAT(GetAllLocalData(), ElementsAre(merged));
}

// Missing full name field should not generate sync events.
TEST_F(AutofillProfileSyncBridgeTest,
       RemoteWithSameGuid_FullName_MissingValueNoSync) {
  // Local autofill profile has an empty full name.
  AutofillProfile local(kGuidA, AutofillProfile::RecordType::kLocalOrSyncable,
                        i18n_model_definition::kLegacyHierarchyCountryCode);
  local.SetRawInfo(NAME_FIRST, u"John");
  local.FinalizeAfterImport();
  AddAutofillProfilesToTable({local});

  // Remote data does not have a full name value.
  AutofillProfileSpecifics remote = CreateAutofillProfileSpecifics(kGuidA);
  remote.add_name_first(std::string("John"));

  // No update to sync, no change in local data.
  EXPECT_CALL(mock_processor(), Put).Times(0);
  EXPECT_CALL(*backend(), CommitChanges());
  StartSyncing({remote});
  EXPECT_THAT(GetAllLocalData(), ElementsAre(local));
}

// This test verifies that for the legacy implementation of names, the full name
// is maintained from the local profile.
// However, this is not a valid use case for structured names as name structures
// must be either merged or fully maintained. For structured names, this test
// verifies that the names are merged.
TEST_F(AutofillProfileSyncBridgeTest,
       RemoteWithSameGuid_FullName_ExistingLocalWinsOverMissingRemote) {
  // Local autofill profile has a full name.
  AutofillProfile local(kGuidA, AutofillProfile::RecordType::kLocalOrSyncable,
                        i18n_model_definition::kLegacyHierarchyCountryCode);
  local.SetRawInfoWithVerificationStatus(NAME_FULL, u"John Jacob Smith",
                                         VerificationStatus::kObserved);
  local.FinalizeAfterImport();
  AddAutofillProfilesToTable({local});

  // After finalization, the first, middle and last name should have the
  // status |kParsed|.
  ASSERT_EQ(local.GetVerificationStatus(NAME_FIRST),
            VerificationStatus::kParsed);
  ASSERT_EQ(local.GetVerificationStatus(NAME_MIDDLE),
            VerificationStatus::kParsed);
  ASSERT_EQ(local.GetVerificationStatus(NAME_LAST),
            VerificationStatus::kParsed);

  // Remote data does not have a full name value.
  AutofillProfile remote_profile(
      kGuidA, AutofillProfile::RecordType::kLocalOrSyncable,
      i18n_model_definition::kLegacyHierarchyCountryCode);
  remote_profile.SetRawInfoWithVerificationStatus(
      NAME_FIRST, u"John", VerificationStatus::kObserved);
  remote_profile.SetRawInfoWithVerificationStatus(
      NAME_MIDDLE, u"Jacob", VerificationStatus::kObserved);
  remote_profile.SetRawInfoWithVerificationStatus(
      NAME_LAST, u"Smith", VerificationStatus::kObserved);
  remote_profile.FinalizeAfterImport();
  AutofillProfileSpecifics remote =
      CreateAutofillProfileSpecifics(remote_profile);

  // Expect local autofill profile to still have the same full name after.
  AutofillProfile merged(local);

  // Note, for structured names, the verification status of those tokens is
  // |kParsed| for local and becomes |kObserved| when merged with the remote
  // profile.
  merged.SetRawInfoWithVerificationStatus(NAME_FIRST, u"John",
                                          VerificationStatus::kObserved);
  merged.SetRawInfoWithVerificationStatus(NAME_MIDDLE, u"Jacob",
                                          VerificationStatus::kObserved);
  merged.SetRawInfoWithVerificationStatus(NAME_LAST, u"Smith",
                                          VerificationStatus::kObserved);

  // No update to sync, merged changes in local data.
  EXPECT_CALL(mock_processor(), Put).Times(0);
  EXPECT_CALL(*backend(), CommitChanges());
  StartSyncing({remote});
  EXPECT_THAT(GetAllLocalData(), ElementsAre(merged));
}

// Missing use_count/use_date fields should not generate sync events.
TEST_F(AutofillProfileSyncBridgeTest,
       RemoteWithSameGuid_UsageStats_MissingValueNoSync) {
  // Local autofill profile has 0 for use_count/use_date.
  AutofillProfile local(kGuidA, AutofillProfile::RecordType::kLocalOrSyncable,
                        i18n_model_definition::kLegacyHierarchyCountryCode);
  local.set_language_code("en");
  local.set_use_count(0);
  local.set_use_date(base::Time());
  AddAutofillProfilesToTable({local});

  // Remote data does not have use_count/use_date.
  AutofillProfileSpecifics remote = CreateAutofillProfileSpecifics(kGuidA);
  remote.clear_use_count();
  remote.clear_use_date();
  remote.set_address_home_language_code("en");

  // No update to sync, no change in local data.
  EXPECT_CALL(mock_processor(), Put).Times(0);
  EXPECT_CALL(*backend(), CommitChanges());
  StartSyncing({remote});
  EXPECT_THAT(GetAllLocalData(), ElementsAre(WithUsageStats(local)));
}

struct UpdatesUsageStatsTestCase {
  size_t local_use_count;
  base::Time local_use_date;
  size_t remote_use_count;
  int remote_use_date;
  size_t merged_use_count;
  base::Time merged_use_date;
};

class AutofillProfileSyncBridgeUpdatesUsageStatsTest
    : public AutofillProfileSyncBridgeTest,
      public testing::WithParamInterface<UpdatesUsageStatsTestCase> {
 public:
  AutofillProfileSyncBridgeUpdatesUsageStatsTest() = default;

  AutofillProfileSyncBridgeUpdatesUsageStatsTest(
      const AutofillProfileSyncBridgeUpdatesUsageStatsTest&) = delete;
  AutofillProfileSyncBridgeUpdatesUsageStatsTest& operator=(
      const AutofillProfileSyncBridgeUpdatesUsageStatsTest&) = delete;

  ~AutofillProfileSyncBridgeUpdatesUsageStatsTest() override = default;
};

TEST_P(AutofillProfileSyncBridgeUpdatesUsageStatsTest, UpdatesUsageStats) {
  auto test_case = GetParam();

  // Local data has usage stats.
  AutofillProfile local(kGuidA, AutofillProfile::RecordType::kLocalOrSyncable,
                        i18n_model_definition::kLegacyHierarchyCountryCode);
  local.set_language_code("en");
  local.set_use_count(test_case.local_use_count);
  local.set_use_date(test_case.local_use_date);
  ASSERT_EQ(test_case.local_use_count, local.use_count());
  ASSERT_EQ(test_case.local_use_date, local.use_date());
  AddAutofillProfilesToTable({local});

  // Remote data has usage stats.
  AutofillProfileSpecifics remote = CreateAutofillProfileSpecifics(kGuidA);
  remote.set_address_home_language_code("en");
  remote.set_use_count(test_case.remote_use_count);
  remote.set_use_date(test_case.remote_use_date);
  ASSERT_TRUE(remote.has_use_count());
  ASSERT_TRUE(remote.has_use_date());

  // Expect the local autofill profile to have usage stats after sync.
  AutofillProfile merged(local);
  merged.set_use_count(test_case.merged_use_count);
  merged.set_use_date(test_case.merged_use_date);

  // Expect no changes to remote data.
  EXPECT_CALL(mock_processor(), Put).Times(0);
  EXPECT_CALL(*backend(), CommitChanges());

  StartSyncing({remote});
  EXPECT_THAT(GetAllLocalData(), ElementsAre(WithUsageStats(merged)));
}

INSTANTIATE_TEST_SUITE_P(
    AutofillProfileSyncBridgeTest,
    AutofillProfileSyncBridgeUpdatesUsageStatsTest,
    testing::Values(
        // Local profile with default stats.
        UpdatesUsageStatsTestCase{
            /*local_use_count=*/0U,
            /*local_use_date=*/base::Time(),
            /*remote_use_count=*/9U,
            /*remote_use_date=*/4321,
            /*merged_use_count=*/9U,
            /*merged_use_date=*/base::Time::FromTimeT(4321)},
        // Local profile has older stats than the server.
        UpdatesUsageStatsTestCase{
            /*local_use_count=*/3U,
            /*local_use_date=*/base::Time::FromTimeT(1234),
            /*remote_use_count=*/9U, /*remote_use_date=*/4321,
            /*merged_use_count=*/9U,
            /*merged_use_date=*/base::Time::FromTimeT(4321)},
        // Local profile has newer stats than the server
        UpdatesUsageStatsTestCase{
            /*local_use_count=*/10U,
            /*local_use_date=*/base::Time::FromTimeT(9999),
            /*remote_use_count=*/9U, /*remote_use_date=*/4321,
            /*merged_use_count=*/9U,
            /*merged_use_date=*/base::Time::FromTimeT(4321)}));

}  // namespace
}  // namespace autofill
