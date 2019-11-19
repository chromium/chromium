// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/autofill/core/browser/webdata/autofill_profile_sync_bridge.h"

#include <stddef.h>

#include <memory>
#include <utility>

#include "base/bind.h"
#include "base/bind_helpers.h"
#include "base/files/scoped_temp_dir.h"
#include "base/guid.h"
#include "base/location.h"
#include "base/memory/ptr_util.h"
#include "base/run_loop.h"
#include "base/strings/utf_string_conversions.h"
#include "base/test/bind_test_util.h"
#include "base/test/task_environment.h"
#include "base/time/time.h"
#include "components/autofill/core/browser/autofill_profile_sync_util.h"
#include "components/autofill/core/browser/data_model/autofill_profile.h"
#include "components/autofill/core/browser/geo/country_names.h"
#include "components/autofill/core/browser/test_autofill_clock.h"
#include "components/autofill/core/browser/webdata/autofill_change.h"
#include "components/autofill/core/browser/webdata/autofill_table.h"
#include "components/autofill/core/browser/webdata/mock_autofill_webdata_backend.h"
#include "components/autofill/core/common/autofill_constants.h"
#include "components/sync/base/client_tag_hash.h"
#include "components/sync/model/data_batch.h"
#include "components/sync/model/data_type_activation_request.h"
#include "components/sync/model/entity_data.h"
#include "components/sync/model/mock_model_type_change_processor.h"
#include "components/sync/model/sync_data.h"
#include "components/sync/model/sync_error_factory.h"
#include "components/sync/model/sync_error_factory_mock.h"
#include "components/sync/model_impl/client_tag_based_model_type_processor.h"
#include "components/sync/protocol/sync.pb.h"
#include "components/webdata/common/web_database.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace autofill {

using base::ASCIIToUTF16;
using base::ScopedTempDir;
using base::UTF16ToUTF8;
using base::UTF8ToUTF16;
using sync_pb::AutofillProfileSpecifics;
using syncer::DataBatch;
using syncer::EntityChange;
using syncer::EntityChangeList;
using syncer::EntityData;
using syncer::KeyAndData;
using syncer::MockModelTypeChangeProcessor;
using syncer::ModelType;
using testing::_;
using testing::DoAll;
using testing::ElementsAre;
using testing::Eq;
using testing::Property;
using testing::Return;
using testing::UnorderedElementsAre;

namespace {

// Some guids for testing.
const char kGuidA[] = "EDC609ED-7EEE-4F27-B00C-423242A9C44A";
const char kGuidB[] = "EDC609ED-7EEE-4F27-B00C-423242A9C44B";
const char kGuidC[] = "EDC609ED-7EEE-4F27-B00C-423242A9C44C";
const char kGuidD[] = "EDC609ED-7EEE-4F27-B00C-423242A9C44D";
const char kGuidInvalid[] = "EDC609ED-7EEE-4F27-B00C";
const char kHttpOrigin[] = "http://www.example.com/";
const char kHttpsOrigin[] = "https://www.example.com/";
const int kValidityStateBitfield = 1984;
const char kLocaleString[] = "en-US";
const base::Time kJune2017 = base::Time::FromDoubleT(1497552271);

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
    const std::string& guid,
    const std::string& origin) {
  AutofillProfileSpecifics specifics;
  specifics.set_guid(guid);
  specifics.set_origin(origin);
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
  if (!arg_profile.EqualsIncludingUsageStatsForTesting(expected_profile)) {
    *result_listener << "entry\n[" << arg_profile << "]\n"
                     << "did not match expected\n[" << expected_profile << "]";
    return false;
  }
  return true;
}

MATCHER_P(WithUsageStats, expected, "") {
  if (!arg.EqualsIncludingUsageStatsForTesting(expected)) {
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
  AutofillProfile profile(kGuidA, kHttpsOrigin);

  profile.set_use_count(7);
  profile.set_use_date(base::Time::FromTimeT(1423182152));

  profile.SetRawInfo(NAME_FULL, ASCIIToUTF16("John K. Doe, Jr."));
  profile.SetRawInfo(NAME_FIRST, ASCIIToUTF16("John"));
  profile.SetRawInfo(NAME_MIDDLE, ASCIIToUTF16("K."));
  profile.SetRawInfo(NAME_LAST, ASCIIToUTF16("Doe"));

  profile.SetRawInfo(EMAIL_ADDRESS, ASCIIToUTF16("user@example.com"));
  profile.SetRawInfo(PHONE_HOME_WHOLE_NUMBER, ASCIIToUTF16("1.800.555.1234"));

  profile.SetRawInfo(ADDRESS_HOME_STREET_ADDRESS, ASCIIToUTF16("123 Fake St.\n"
                                                               "Apt. 42"));
  EXPECT_EQ(ASCIIToUTF16("123 Fake St."),
            profile.GetRawInfo(ADDRESS_HOME_LINE1));
  EXPECT_EQ(ASCIIToUTF16("Apt. 42"), profile.GetRawInfo(ADDRESS_HOME_LINE2));

  profile.SetRawInfo(COMPANY_NAME, ASCIIToUTF16("Google, Inc."));
  profile.SetRawInfo(ADDRESS_HOME_CITY, ASCIIToUTF16("Mountain View"));
  profile.SetRawInfo(ADDRESS_HOME_STATE, ASCIIToUTF16("California"));
  profile.SetRawInfo(ADDRESS_HOME_ZIP, ASCIIToUTF16("94043"));
  profile.SetRawInfo(ADDRESS_HOME_COUNTRY, ASCIIToUTF16("US"));
  profile.SetRawInfo(ADDRESS_HOME_SORTING_CODE, ASCIIToUTF16("CEDEX"));
  profile.SetRawInfo(ADDRESS_HOME_DEPENDENT_LOCALITY,
                     ASCIIToUTF16("Santa Clara"));
  profile.set_language_code("en");
  profile.SetClientValidityFromBitfieldValue(kValidityStateBitfield);
  return profile;
}

// Returns AutofillProfileSpecifics with all Autofill profile fields set.
// Contains identical data to the data returned from ConstructCompleteProfile().
AutofillProfileSpecifics ConstructCompleteSpecifics() {
  AutofillProfileSpecifics specifics;

  specifics.set_guid(kGuidA);
  specifics.set_origin(kHttpsOrigin);
  specifics.set_use_count(7);
  specifics.set_use_date(1423182152);

  specifics.add_name_first("John");
  specifics.add_name_middle("K.");
  specifics.add_name_last("Doe");
  specifics.add_name_full("John K. Doe, Jr.");

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
  specifics.set_address_home_country("US");
  specifics.set_address_home_sorting_code("CEDEX");
  specifics.set_address_home_dependent_locality("Santa Clara");
  specifics.set_address_home_language_code("en");
  specifics.set_validity_state_bitfield(kValidityStateBitfield);

  return specifics;
}

}  // namespace

class AutofillProfileSyncBridgeTest : public testing::Test {
 public:
  AutofillProfileSyncBridgeTest() {}
  ~AutofillProfileSyncBridgeTest() override {}

  void SetUp() override {
    // Fix a time for implicitly constructed use_dates in AutofillProfile.
    test_clock_.SetNow(kJune2017);
    CountryNames::SetLocaleString(kLocaleString);
    ASSERT_TRUE(temp_dir_.CreateUniqueTempDir());
    db_.AddTable(&table_);
    db_.Init(temp_dir_.GetPath().AppendASCII("SyncTestWebDatabase"));
    ON_CALL(*backend(), GetDatabase()).WillByDefault(Return(&db_));
    ResetProcessor();
    ResetBridge();
  }

  void ResetProcessor() {
    real_processor_ =
        std::make_unique<syncer::ClientTagBasedModelTypeProcessor>(
            syncer::AUTOFILL_PROFILE, /*dump_stack=*/base::DoNothing(),
            /*commit_only=*/false);
    mock_processor_.DelegateCallsByDefaultTo(real_processor_.get());
  }

  void ResetBridge() {
    bridge_.reset(new AutofillProfileSyncBridge(
        mock_processor_.CreateForwardingProcessor(), kLocaleString, &backend_));
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

    // Initialize the processor with initial_sync_done.
    sync_pb::ModelTypeState state;
    state.set_initial_sync_done(true);
    syncer::UpdateResponseDataList initial_updates;
    for (const AutofillProfileSpecifics& specifics : remote_data) {
      initial_updates.push_back(SpecificsToUpdateResponse(specifics));
    }
    real_processor_->OnUpdateReceived(state, std::move(initial_updates));
  }

  void ApplySyncChanges(EntityChangeList changes) {
    const base::Optional<syncer::ModelError> error = bridge()->ApplySyncChanges(
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
    // Perform an async call synchronously for testing.
    base::RunLoop loop;
    bridge()->GetAllDataForDebugging(base::BindLambdaForTesting(
        [&loop, &data](std::unique_ptr<DataBatch> batch) {
          ExtractAutofillProfilesFromDataBatch(std::move(batch), &data);
          loop.Quit();
        }));
    loop.Run();
    return data;
  }

  std::unique_ptr<EntityData> SpecificsToEntity(
      const AutofillProfileSpecifics& specifics) {
    auto data = std::make_unique<EntityData>();
    *data->specifics.mutable_autofill_profile() = specifics;
    data->client_tag_hash = syncer::ClientTagHash::FromUnhashed(
        syncer::AUTOFILL_PROFILE, bridge()->GetClientTag(*data));
    return data;
  }

  std::unique_ptr<syncer::UpdateResponseData> SpecificsToUpdateResponse(
      const AutofillProfileSpecifics& specifics) {
    auto data = std::make_unique<syncer::UpdateResponseData>();
    data->entity = SpecificsToEntity(specifics);
    return data;
  }

  AutofillProfileSyncBridge* bridge() { return bridge_.get(); }

  syncer::MockModelTypeChangeProcessor& mock_processor() {
    return mock_processor_;
  }

  AutofillTable* table() { return &table_; }

  MockAutofillWebDataBackend* backend() { return &backend_; }

 private:
  autofill::TestAutofillClock test_clock_;
  ScopedTempDir temp_dir_;
  base::test::SingleThreadTaskEnvironment task_environment_;
  testing::NiceMock<MockAutofillWebDataBackend> backend_;
  AutofillTable table_;
  WebDatabase db_;
  testing::NiceMock<MockModelTypeChangeProcessor> mock_processor_;
  std::unique_ptr<syncer::ClientTagBasedModelTypeProcessor> real_processor_;
  std::unique_ptr<AutofillProfileSyncBridge> bridge_;

  DISALLOW_COPY_AND_ASSIGN(AutofillProfileSyncBridgeTest);
};

TEST_F(AutofillProfileSyncBridgeTest, AutofillProfileChanged_Added) {
  StartSyncing({});

  AutofillProfile local(kGuidA, kHttpsOrigin);
  local.SetRawInfo(NAME_FIRST, ASCIIToUTF16("Jane"));
  AutofillProfileChange change(AutofillProfileChange::ADD, kGuidA, &local);

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

  AutofillProfile local(kGuidA, kHttpsOrigin);
  local.set_language_code("en");
  AutofillProfileChange change(AutofillProfileChange::ADD, kGuidA, &local);

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

  AutofillProfile local(kGuidA, kHttpsOrigin);
  local.SetClientValidityFromBitfieldValue(kValidityStateBitfield);
  AutofillProfileChange change(AutofillProfileChange::ADD, kGuidA, &local);

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

  AutofillProfile local(kGuidA, kHttpsOrigin);
  local.SetRawInfo(NAME_FIRST, ASCIIToUTF16("Jane"));
  AutofillProfileChange change(AutofillProfileChange::UPDATE, kGuidA, &local);

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
  AutofillProfileSpecifics remote =
      CreateAutofillProfileSpecifics(kGuidA, kHttpsOrigin);
  remote.set_address_home_language_code("en");
  remote.set_use_count(9);
  remote.set_use_date(25);

  StartSyncing({remote});
  EXPECT_THAT(GetAllLocalData(),
              ElementsAre(WithUsageStats(CreateAutofillProfile(remote))));

  // Update to the usage stats for that profile.
  AutofillProfile local(kGuidA, kHttpsOrigin);
  local.set_language_code("en");
  local.set_use_count(10U);
  local.set_use_date(base::Time::FromTimeT(30));
  AutofillProfileChange change(AutofillProfileChange::UPDATE, kGuidA, &local);

  EXPECT_CALL(
      mock_processor(),
      Put(kGuidA, HasSpecifics(CreateAutofillProfileSpecifics(local)), _));
  // The bridge does not need to commit when reacting to a notification about a
  // local change.
  EXPECT_CALL(*backend(), CommitChanges()).Times(0);

  bridge()->AutofillProfileChanged(change);
}

// Server profile updates should be ignored.
TEST_F(AutofillProfileSyncBridgeTest,
       AutofillProfileChanged_Updated_IgnoreServerProfiles) {
  StartSyncing({});

  AutofillProfile server_profile(AutofillProfile::SERVER_PROFILE, "server-id");
  AutofillProfileChange change(AutofillProfileChange::UPDATE,
                               server_profile.guid(), &server_profile);

  EXPECT_CALL(mock_processor(), Put(_, _, _)).Times(0);
  // Should not crash.
  bridge()->AutofillProfileChanged(change);
}

TEST_F(AutofillProfileSyncBridgeTest, AutofillProfileChanged_Deleted) {
  StartSyncing({});

  AutofillProfile local(kGuidB, kHttpsOrigin);
  local.SetRawInfo(NAME_FIRST, ASCIIToUTF16("Jane"));
  AutofillProfileChange change(AutofillProfileChange::REMOVE, kGuidB, &local);
  EXPECT_CALL(mock_processor(), Delete(kGuidB, _));
  // The bridge does not need to commit when reacting to a notification about a
  // local change.
  EXPECT_CALL(*backend(), CommitChanges()).Times(0);

  bridge()->AutofillProfileChanged(change);
}

// Server profile updates should be ignored.
TEST_F(AutofillProfileSyncBridgeTest,
       AutofillProfileChanged_Deleted_IgnoreServerProfiles) {
  StartSyncing({});

  AutofillProfile server_profile(AutofillProfile::SERVER_PROFILE, "server-id");
  AutofillProfileChange change(AutofillProfileChange::REMOVE,
                               server_profile.guid(), &server_profile);

  EXPECT_CALL(mock_processor(), Put(_, _, _)).Times(0);
  // Should not crash.
  bridge()->AutofillProfileChanged(change);
}

TEST_F(AutofillProfileSyncBridgeTest, GetAllDataForDebugging) {
  AutofillProfile local1 = AutofillProfile(kGuidA, kHttpsOrigin);
  local1.SetRawInfo(NAME_FIRST, ASCIIToUTF16("John"));
  local1.SetRawInfo(ADDRESS_HOME_STREET_ADDRESS, ASCIIToUTF16("1 1st st"));
  AutofillProfile local2 = AutofillProfile(kGuidB, kHttpsOrigin);
  local2.SetRawInfo(NAME_FIRST, ASCIIToUTF16("Tom"));
  local2.SetRawInfo(ADDRESS_HOME_STREET_ADDRESS, ASCIIToUTF16("2 2nd st"));
  AddAutofillProfilesToTable({local1, local2});

  EXPECT_THAT(GetAllLocalData(), UnorderedElementsAre(local1, local2));
}

TEST_F(AutofillProfileSyncBridgeTest, GetData) {
  AutofillProfile local1 = AutofillProfile(kGuidA, kHttpsOrigin);
  local1.SetRawInfo(NAME_FIRST, ASCIIToUTF16("John"));
  local1.SetRawInfo(ADDRESS_HOME_STREET_ADDRESS, ASCIIToUTF16("1 1st st"));
  AutofillProfile local2 = AutofillProfile(kGuidB, kHttpsOrigin);
  local2.SetRawInfo(NAME_FIRST, ASCIIToUTF16("Tom"));
  local2.SetRawInfo(ADDRESS_HOME_STREET_ADDRESS, ASCIIToUTF16("2 2nd st"));
  AddAutofillProfilesToTable({local1, local2});

  std::vector<AutofillProfile> data;
  base::RunLoop loop;
  bridge()->GetData({kGuidA},
                    base::BindLambdaForTesting(
                        [&loop, &data](std::unique_ptr<DataBatch> batch) {
                          ExtractAutofillProfilesFromDataBatch(std::move(batch),
                                                               &data);
                          loop.Quit();
                        }));
  loop.Run();

  EXPECT_THAT(data, ElementsAre(local1));
}

TEST_F(AutofillProfileSyncBridgeTest, MergeSyncData) {
  AutofillProfile local1 = AutofillProfile(kGuidA, kHttpOrigin);
  local1.SetRawInfo(NAME_FIRST, ASCIIToUTF16("John"));
  local1.SetRawInfo(ADDRESS_HOME_STREET_ADDRESS, ASCIIToUTF16("1 1st st"));
  AutofillProfile local2 = AutofillProfile(kGuidB, std::string());
  local2.SetRawInfo(NAME_FIRST, ASCIIToUTF16("Tom"));
  local2.SetRawInfo(ADDRESS_HOME_STREET_ADDRESS, ASCIIToUTF16("2 2nd st"));
  AddAutofillProfilesToTable({local1, local2});

  AutofillProfileSpecifics remote1 =
      CreateAutofillProfileSpecifics(kGuidC, kHttpsOrigin);
  remote1.add_name_first("Jane");
  AutofillProfileSpecifics remote2 =
      CreateAutofillProfileSpecifics(kGuidD, kSettingsOrigin);
  remote2.add_name_first("Harry");
  // This one will have the name and origin updated.
  AutofillProfileSpecifics remote3 =
      CreateAutofillProfileSpecifics(kGuidB, kSettingsOrigin);
  remote3.add_name_first("Tom Doe");

  EXPECT_CALL(
      mock_processor(),
      Put(kGuidA, HasSpecifics(CreateAutofillProfileSpecifics(local1)), _));
  EXPECT_CALL(mock_processor(), Delete(_, _)).Times(0);
  EXPECT_CALL(*backend(), CommitChanges());

  StartSyncing({remote1, remote2, remote3});

  EXPECT_THAT(GetAllLocalData(),
              UnorderedElementsAre(local1, CreateAutofillProfile(remote1),
                                   CreateAutofillProfile(remote2),
                                   CreateAutofillProfile(remote3)));
}

// Ensure that all profile fields are able to be synced up from the client to
// the server.
TEST_F(AutofillProfileSyncBridgeTest, MergeSyncData_SyncAllFieldsToServer) {
  AutofillProfile local = ConstructCompleteProfile();
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
TEST_F(AutofillProfileSyncBridgeTest, MergeSyncData_SyncAllFieldsToClient) {
  EXPECT_CALL(mock_processor(), Put(_, _, _)).Times(0);
  EXPECT_CALL(*backend(), CommitChanges());
  StartSyncing({ConstructCompleteSpecifics()});

  EXPECT_THAT(GetAllLocalData(),
              ElementsAre(WithUsageStats(ConstructCompleteProfile())));
}

TEST_F(AutofillProfileSyncBridgeTest, MergeSyncData_IdenticalProfiles) {
  AutofillProfile local1 = AutofillProfile(kGuidA, kHttpOrigin);
  local1.SetRawInfo(NAME_FIRST, ASCIIToUTF16("John"));
  local1.SetRawInfo(ADDRESS_HOME_STREET_ADDRESS, ASCIIToUTF16("1 1st st"));
  AutofillProfile local2 = AutofillProfile(kGuidB, kSettingsOrigin);
  local2.SetRawInfo(NAME_FIRST, ASCIIToUTF16("Tom"));
  local2.SetRawInfo(ADDRESS_HOME_STREET_ADDRESS, ASCIIToUTF16("2 2nd st"));
  AddAutofillProfilesToTable({local1, local2});

  // The synced profiles are identical to the local ones, except that the guids
  // are different.
  AutofillProfileSpecifics remote1 =
      CreateAutofillProfileSpecifics(kGuidC, kHttpsOrigin);
  remote1.add_name_first("John");
  remote1.set_address_home_street_address("1 1st st");
  AutofillProfileSpecifics remote2 =
      CreateAutofillProfileSpecifics(kGuidD, kHttpsOrigin);
  remote2.add_name_first("Tom");
  remote2.set_address_home_street_address("2 2nd st");

  // Both remote profiles win, only the verified origin is taken over for the
  // second profile.
  AutofillProfileSpecifics merged2(remote2);
  merged2.set_origin(kSettingsOrigin);
  EXPECT_CALL(mock_processor(), Put(kGuidD, HasSpecifics(merged2), _));
  EXPECT_CALL(*backend(), CommitChanges());

  StartSyncing({remote1, remote2});

  EXPECT_THAT(GetAllLocalData(),
              UnorderedElementsAre(CreateAutofillProfile(remote1),
                                   CreateAutofillProfile(merged2)));
}

TEST_F(AutofillProfileSyncBridgeTest, MergeSyncData_NonSimilarProfiles) {
  AutofillProfile local = ConstructCompleteProfile();
  local.set_guid(kGuidA);
  local.SetRawInfo(NAME_FULL, ASCIIToUTF16("John K. Doe, Jr."));
  local.SetRawInfo(NAME_FIRST, ASCIIToUTF16("John"));
  local.SetRawInfo(NAME_MIDDLE, ASCIIToUTF16("K."));
  local.SetRawInfo(NAME_LAST, ASCIIToUTF16("Doe"));
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
  EXPECT_CALL(mock_processor(), Delete(_, _)).Times(0);
  EXPECT_CALL(*backend(), CommitChanges());

  StartSyncing({remote});

  EXPECT_THAT(GetAllLocalData(),
              UnorderedElementsAre(local, CreateAutofillProfile(remote)));
}

TEST_F(AutofillProfileSyncBridgeTest, MergeSyncData_SimilarProfiles) {
  AutofillProfile local1 = AutofillProfile(kGuidA, kHttpOrigin);
  local1.SetRawInfo(NAME_FIRST, ASCIIToUTF16("John"));
  local1.SetRawInfo(ADDRESS_HOME_STREET_ADDRESS, ASCIIToUTF16("1 1st st"));
  local1.set_use_count(27);
  AutofillProfile local2 = AutofillProfile(kGuidB, kSettingsOrigin);
  local2.SetRawInfo(NAME_FIRST, ASCIIToUTF16("Tom"));
  local2.SetRawInfo(ADDRESS_HOME_STREET_ADDRESS, ASCIIToUTF16("2 2nd st"));
  AddAutofillProfilesToTable({local1, local2});

  // The synced profiles are identical to the local ones, except that the guids
  // and use_count values are different. Remote ones have additional company
  // name which makes them not be identical.
  AutofillProfileSpecifics remote1 =
      CreateAutofillProfileSpecifics(kGuidC, kHttpsOrigin);
  remote1.add_name_first("John");
  remote1.set_address_home_street_address("1 1st st");
  remote1.set_company_name("Frobbers, Inc.");
  remote1.set_use_count(13);
  AutofillProfileSpecifics remote2 =
      CreateAutofillProfileSpecifics(kGuidD, kHttpsOrigin);
  remote2.add_name_first("Tom");
  remote2.set_address_home_street_address("2 2nd st");
  remote2.set_company_name("Fizzbang, LLC.");
  remote2.set_use_count(4);

  // The first profile should have its origin updated.
  // The second profile should remain as-is, because an unverified profile
  // should never overwrite a verified one.
  AutofillProfileSpecifics merged1(remote1);
  merged1.set_origin(kHttpOrigin);
  // When merging, full name gets populated.
  merged1.add_name_full("John");
  // Merging two profile takes their max use count.
  merged1.set_use_count(27);

  // Expect updating the first (merged) profile and adding the second local one.
  EXPECT_CALL(mock_processor(), Put(kGuidC, HasSpecifics(merged1), _));
  EXPECT_CALL(
      mock_processor(),
      Put(kGuidB, HasSpecifics(CreateAutofillProfileSpecifics(local2)), _));
  EXPECT_CALL(mock_processor(), Delete(_, _)).Times(0);
  EXPECT_CALL(*backend(), CommitChanges());

  StartSyncing({remote1, remote2});

  EXPECT_THAT(GetAllLocalData(),
              UnorderedElementsAre(
                  WithUsageStats(CreateAutofillProfile(merged1)), local2,
                  WithUsageStats(CreateAutofillProfile(remote2))));
}

// Tests that MergeSimilarProfiles keeps the most recent use date of the two
// profiles being merged.
TEST_F(AutofillProfileSyncBridgeTest,
       MergeSyncData_SimilarProfiles_OlderUseDate) {
  // Different guids, same origin, difference in the phone number.
  AutofillProfile local(kGuidA, kHttpOrigin);
  local.SetRawInfo(PHONE_HOME_WHOLE_NUMBER, ASCIIToUTF16("650234567"));
  local.set_use_date(base::Time::FromTimeT(30));
  AddAutofillProfilesToTable({local});

  AutofillProfileSpecifics remote =
      CreateAutofillProfileSpecifics(kGuidB, kHttpOrigin);
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
       MergeSyncData_SimilarProfiles_NewerUseDate) {
  // Different guids, same origin, difference in the phone number.
  AutofillProfile local(kGuidA, kHttpOrigin);
  local.SetRawInfo(PHONE_HOME_WHOLE_NUMBER, ASCIIToUTF16("650234567"));
  local.set_use_date(base::Time::FromTimeT(30));
  AddAutofillProfilesToTable({local});

  AutofillProfileSpecifics remote =
      CreateAutofillProfileSpecifics(kGuidB, kHttpOrigin);
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
       MergeSyncData_SimilarProfiles_NonZeroUseCounts) {
  // Different guids, same origin, difference in the phone number.
  AutofillProfile local(kGuidA, kHttpOrigin);
  local.SetRawInfo(PHONE_HOME_WHOLE_NUMBER, ASCIIToUTF16("650234567"));
  local.set_use_count(12);
  AddAutofillProfilesToTable({local});

  AutofillProfileSpecifics remote =
      CreateAutofillProfileSpecifics(kGuidB, kHttpOrigin);
  remote.set_use_count(5);

  // The use count of |local| should replace the use count of |remote|.
  AutofillProfileSpecifics merged(remote);
  merged.set_use_count(12);
  merged.add_phone_home_whole_number("650234567");
  EXPECT_CALL(mock_processor(), Put(kGuidB, HasSpecifics(merged), _));
  EXPECT_CALL(*backend(), CommitChanges());

  StartSyncing({remote});
}

// Tests that when merging similar profiles for initial sync, we add the
// additional information of |local| into |remote|.
TEST_F(AutofillProfileSyncBridgeTest,
       MergeSyncData_SimilarProfiles_LocalOriginPreserved) {
  AutofillProfile local(kGuidA, kHttpsOrigin);
  local.SetRawInfo(PHONE_HOME_WHOLE_NUMBER, ASCIIToUTF16("650234567"));
  AddAutofillProfilesToTable({local});

  AutofillProfileSpecifics remote =
      CreateAutofillProfileSpecifics(kGuidB, kHttpOrigin);
  remote.set_address_home_language_code("en");

  // Expect that the resulting merged profile is written back to sync and that
  // it has the phone number and origin from |local|.
  AutofillProfileSpecifics merged(remote);
  merged.set_origin(kHttpsOrigin);
  merged.add_phone_home_whole_number("650234567");
  // TODO(jkrcal): Is this expected that language code gets deleted? Not
  // explicitly covered by previous tests but happens.
  merged.set_address_home_language_code("");
  EXPECT_CALL(mock_processor(), Put(kGuidB, HasSpecifics(merged), _));
  EXPECT_CALL(*backend(), CommitChanges());

  StartSyncing({remote});
}

// Sync data without origin should not overwrite existing origin in local
// autofill profile.
TEST_F(AutofillProfileSyncBridgeTest,
       MergeSyncData_SimilarProfiles_LocalExistingOriginPreserved) {
  AutofillProfile local(kGuidA, kHttpsOrigin);
  AddAutofillProfilesToTable({local});

  // Remote data does not have an origin value.
  AutofillProfileSpecifics remote = CreateAutofillProfileSpecifics(kGuidA, "");
  remote.clear_origin();
  remote.add_name_first("John");
  ASSERT_FALSE(remote.has_origin());

  // Expect no sync events to add origin to the remote data.
  EXPECT_CALL(mock_processor(), Put(_, _, _)).Times(0);
  EXPECT_CALL(*backend(), CommitChanges());
  StartSyncing({remote});

  // Expect the local autofill profile to still have an origin after sync.
  AutofillProfile merged(local);
  merged.SetRawInfo(NAME_FIRST, ASCIIToUTF16("John"));

  EXPECT_THAT(GetAllLocalData(), ElementsAre(merged));
}

// Ensure that no Sync events are generated to fill in missing origins from Sync
// with explicitly present empty ones. This ensures that the migration to add
// origins to profiles does not generate lots of needless Sync updates.
TEST_F(AutofillProfileSyncBridgeTest,
       MergeSyncData_SimilarProfiles_LocalMissingOriginPreserved) {
  AutofillProfile local = AutofillProfile(kGuidA, std::string());
  local.SetRawInfo(NAME_FIRST, ASCIIToUTF16("John"));
  AddAutofillProfilesToTable({local});

  // Create a Sync profile identical to |local|, except with no origin set.
  AutofillProfileSpecifics remote = CreateAutofillProfileSpecifics(kGuidA, "");
  remote.clear_origin();
  remote.add_name_first("John");
  ASSERT_FALSE(remote.has_origin());

  EXPECT_CALL(mock_processor(), Put(_, _, _)).Times(0);
  EXPECT_CALL(*backend(), CommitChanges());
  StartSyncing({remote});
  EXPECT_THAT(GetAllLocalData(), ElementsAre(local));
}

TEST_F(AutofillProfileSyncBridgeTest, ApplySyncChanges) {
  AutofillProfile local = AutofillProfile(kGuidA, kHttpsOrigin);
  AddAutofillProfilesToTable({local});

  StartSyncing({});

  AutofillProfileSpecifics remote =
      CreateAutofillProfileSpecifics(kGuidB, kHttpOrigin);
  remote.add_name_first("Jane");

  EXPECT_CALL(mock_processor(), Put(_, _, _)).Times(0);
  EXPECT_CALL(mock_processor(), Delete(_, _)).Times(0);
  EXPECT_CALL(*backend(), CommitChanges());

  syncer::EntityChangeList entity_change_list;
  entity_change_list.push_back(EntityChange::CreateDelete(kGuidA));
  entity_change_list.push_back(
      EntityChange::CreateAdd(kGuidB, SpecificsToEntity(remote)));
  ApplySyncChanges(std::move(entity_change_list));

  EXPECT_THAT(GetAllLocalData(), ElementsAre(CreateAutofillProfile(remote)));
}

// Ensure that entries with invalid specifics are ignored.
TEST_F(AutofillProfileSyncBridgeTest, ApplySyncChanges_OmitsInvalidSpecifics) {
  StartSyncing({});

  AutofillProfileSpecifics remote_valid =
      CreateAutofillProfileSpecifics(kGuidA, std::string());
  AutofillProfileSpecifics remote_invalid =
      CreateAutofillProfileSpecifics(kGuidInvalid, std::string());

  EXPECT_CALL(mock_processor(), Put(_, _, _)).Times(0);
  EXPECT_CALL(*backend(), CommitChanges());

  syncer::EntityChangeList entity_change_list;
  entity_change_list.push_back(
      EntityChange::CreateAdd(kGuidA, SpecificsToEntity(remote_valid)));
  entity_change_list.push_back(
      EntityChange::CreateAdd(kGuidInvalid, SpecificsToEntity(remote_invalid)));
  ApplySyncChanges(std::move(entity_change_list));

  EXPECT_THAT(GetAllLocalData(),
              ElementsAre(CreateAutofillProfile(remote_valid)));
}

// Verifies that setting the street address field also sets the (deprecated)
// address line 1 and line 2 fields.
TEST_F(AutofillProfileSyncBridgeTest, StreetAddress_SplitAutomatically) {
  AutofillProfile local;
  local.SetRawInfo(ADDRESS_HOME_STREET_ADDRESS, ASCIIToUTF16("123 Example St.\n"
                                                             "Apt. 42"));
  EXPECT_EQ(ASCIIToUTF16("123 Example St."),
            local.GetRawInfo(ADDRESS_HOME_LINE1));
  EXPECT_EQ(ASCIIToUTF16("Apt. 42"), local.GetRawInfo(ADDRESS_HOME_LINE2));

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
  AutofillProfile local;
  local.SetRawInfo(ADDRESS_HOME_LINE1, ASCIIToUTF16("123 Example St."));
  local.SetRawInfo(ADDRESS_HOME_LINE2, ASCIIToUTF16("Apt. 42"));
  EXPECT_EQ(ASCIIToUTF16("123 Example St.\n"
                         "Apt. 42"),
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
  AutofillProfileSpecifics remote =
      CreateAutofillProfileSpecifics(kGuidA, kHttpsOrigin);
  remote.set_address_home_line1("123 Example St.");
  remote.set_address_home_line2("Apt. 42");
  remote.set_address_home_street_address(
      "456 El Camino Real\n"
      "Suite #1337");
  EXPECT_CALL(*backend(), CommitChanges());

  StartSyncing({remote});

  // Verify that full street address takes precedence over address lines.
  AutofillProfile local(kGuidA, kHttpsOrigin);
  local.SetRawInfo(ADDRESS_HOME_STREET_ADDRESS,
                   ASCIIToUTF16("456 El Camino Real\n"
                                "Suite #1337"));
  local.SetRawInfo(ADDRESS_HOME_LINE1, ASCIIToUTF16("456 El Camino Real"));
  local.SetRawInfo(ADDRESS_HOME_LINE2, ASCIIToUTF16("Suite #1337"));
  EXPECT_THAT(GetAllLocalData(), ElementsAre(local));
}

// Ensure that no Sync events are generated to fill in missing street address
// fields from Sync with explicitly present ones identical to the data stored in
// the line1 and line2 fields. This ensures that the migration to add the
// street address field to profiles does not generate lots of needless Sync
// updates.
TEST_F(AutofillProfileSyncBridgeTest,
       RemoteWithSameGuid_StreetAddress_NoUpdateToEmptyStreetAddressSyncedUp) {
  AutofillProfile local(kGuidA, kHttpsOrigin);
  local.SetRawInfo(ADDRESS_HOME_STREET_ADDRESS, ASCIIToUTF16("123 Example St.\n"
                                                             "Apt. 42"));
  AddAutofillProfilesToTable({local});

  // Create a Sync profile identical to |profile|, except without street address
  // explicitly set.
  AutofillProfileSpecifics remote =
      CreateAutofillProfileSpecifics(kGuidA, kHttpsOrigin);
  remote.set_address_home_line1("123 Example St.");
  remote.set_address_home_line2("Apt. 42");

  // No update to sync, no change in local data.
  EXPECT_CALL(mock_processor(), Put(_, _, _)).Times(0);
  EXPECT_CALL(*backend(), CommitChanges());
  StartSyncing({remote});
  EXPECT_THAT(GetAllLocalData(), ElementsAre(local));
}

// Missing language code field should not generate sync events.
TEST_F(AutofillProfileSyncBridgeTest,
       RemoteWithSameGuid_LanguageCode_MissingCodesNoSync) {
  AutofillProfile local(kGuidA, kHttpsOrigin);
  ASSERT_TRUE(local.language_code().empty());
  AddAutofillProfilesToTable({local});

  // Remote data does not have a language code value.
  AutofillProfileSpecifics remote =
      CreateAutofillProfileSpecifics(kGuidA, kHttpsOrigin);
  ASSERT_FALSE(remote.has_address_home_language_code());

  // No update to sync, no change in local data.
  EXPECT_CALL(mock_processor(), Put(_, _, _)).Times(0);
  EXPECT_CALL(*backend(), CommitChanges());
  StartSyncing({remote});
  EXPECT_THAT(GetAllLocalData(), ElementsAre(local));
}

// Empty language code should be overwritten by sync.
TEST_F(AutofillProfileSyncBridgeTest,
       RemoteWithSameGuid_LanguageCode_ExistingRemoteWinsOverMissingLocal) {
  AutofillProfile local(kGuidA, kHttpsOrigin);
  ASSERT_TRUE(local.language_code().empty());
  AddAutofillProfilesToTable({local});

  // Remote data has "en" language code.
  AutofillProfileSpecifics remote =
      CreateAutofillProfileSpecifics(kGuidA, kHttpsOrigin);
  remote.set_address_home_language_code("en");

  // No update to sync, remote language code overwrites the empty local one.
  EXPECT_CALL(mock_processor(), Put(_, _, _)).Times(0);
  EXPECT_CALL(*backend(), CommitChanges());
  StartSyncing({remote});
  EXPECT_THAT(GetAllLocalData(), ElementsAre(CreateAutofillProfile(remote)));
}

// Local language code should be overwritten by remote one.
TEST_F(AutofillProfileSyncBridgeTest,
       RemoteWithSameGuid_LanguageCode_ExistingRemoteWinsOverExistingLocal) {
  AutofillProfile local(kGuidA, kHttpsOrigin);
  local.set_language_code("de");
  AddAutofillProfilesToTable({local});

  // Remote data has "en" language code.
  AutofillProfileSpecifics remote =
      CreateAutofillProfileSpecifics(kGuidA, kHttpsOrigin);
  remote.set_address_home_language_code("en");

  // No update to sync, remote language code overwrites the local one.
  EXPECT_CALL(mock_processor(), Put(_, _, _)).Times(0);
  EXPECT_CALL(*backend(), CommitChanges());
  StartSyncing({remote});
  EXPECT_THAT(GetAllLocalData(), ElementsAre(CreateAutofillProfile(remote)));
}

// Sync data without language code should not overwrite existing language code
// in local autofill profile.
TEST_F(AutofillProfileSyncBridgeTest,
       RemoteWithSameGuid_LanguageCode_ExistingLocalWinsOverMissingRemote) {
  // Local autofill profile has "en" language code.
  AutofillProfile local(kGuidA, kHttpsOrigin);
  local.set_language_code("en");
  AddAutofillProfilesToTable({local});

  // Remote data does not have a language code value.
  AutofillProfileSpecifics remote =
      CreateAutofillProfileSpecifics(kGuidA, kHttpsOrigin);
  remote.add_name_first("John");
  ASSERT_FALSE(remote.has_address_home_language_code());

  // Expect local autofill profile to still have "en" language code after
  AutofillProfile merged(local);
  merged.SetRawInfo(NAME_FIRST, ASCIIToUTF16("John"));

  // No update to sync, remote language code overwrites the local one.
  EXPECT_CALL(mock_processor(), Put(_, _, _)).Times(0);
  EXPECT_CALL(*backend(), CommitChanges());
  StartSyncing({remote});
  EXPECT_THAT(GetAllLocalData(), ElementsAre(merged));
}

// Missing validity state bitifield should not generate sync events.
TEST_F(AutofillProfileSyncBridgeTest,
       RemoteWithSameGuid_ValidityState_DefaultValueNoSync) {
  AutofillProfile local(kGuidA, kHttpsOrigin);
  ASSERT_EQ(0, local.GetClientValidityBitfieldValue());
  ASSERT_FALSE(local.is_client_validity_states_updated());
  AddAutofillProfilesToTable({local});

  // Remote data does not have a validity state bitfield value.
  AutofillProfileSpecifics remote =
      CreateAutofillProfileSpecifics(kGuidA, kHttpsOrigin);
  ASSERT_FALSE(remote.has_validity_state_bitfield());
  ASSERT_FALSE(remote.is_client_validity_states_updated());

  // No update to sync, no change in local data.
  EXPECT_CALL(mock_processor(), Put(_, _, _)).Times(0);
  EXPECT_CALL(*backend(), CommitChanges());
  StartSyncing({remote});
  EXPECT_THAT(GetAllLocalData(), ElementsAre(local));
}

// Default validity state bitfield should be overwritten by sync.
TEST_F(AutofillProfileSyncBridgeTest,
       RemoteWithSameGuid_ValidityState_ExistingRemoteWinsOverMissingLocal) {
  AutofillProfile local(kGuidA, kHttpsOrigin);
  ASSERT_EQ(0, local.GetClientValidityBitfieldValue());
  AddAutofillProfilesToTable({local});

  // Remote data has a non default validity state bitfield value.
  AutofillProfileSpecifics remote =
      CreateAutofillProfileSpecifics(kGuidA, kHttpsOrigin);
  remote.set_validity_state_bitfield(kValidityStateBitfield);
  ASSERT_TRUE(remote.has_validity_state_bitfield());

  // No update to sync, the validity bitfield should be stored to local.
  EXPECT_CALL(mock_processor(), Put(_, _, _)).Times(0);
  EXPECT_CALL(*backend(), CommitChanges());
  StartSyncing({remote});
  EXPECT_THAT(GetAllLocalData(), ElementsAre(CreateAutofillProfile(remote)));
}

// Local validity state bitfield should be overwritten by sync.
TEST_F(AutofillProfileSyncBridgeTest,
       RemoteWithSameGuid_ValidityState_ExistingRemoteWinsOverExistingLocal) {
  AutofillProfile local(kGuidA, kHttpsOrigin);
  local.SetClientValidityFromBitfieldValue(kValidityStateBitfield + 1);
  AddAutofillProfilesToTable({local});

  // Remote data has a non default validity state bitfield value.
  AutofillProfileSpecifics remote =
      CreateAutofillProfileSpecifics(kGuidA, kHttpsOrigin);
  remote.set_validity_state_bitfield(kValidityStateBitfield);
  ASSERT_TRUE(remote.has_validity_state_bitfield());

  // No update to sync, the remote validity bitfield should overwrite local.
  EXPECT_CALL(mock_processor(), Put(_, _, _)).Times(0);
  EXPECT_CALL(*backend(), CommitChanges());
  StartSyncing({remote});
  EXPECT_THAT(GetAllLocalData(), ElementsAre(CreateAutofillProfile(remote)));
}

// Sync data without a default validity state bitfield should not overwrite
// an existing validity state bitfield in local autofill profile.
TEST_F(AutofillProfileSyncBridgeTest,
       RemoteWithSameGuid_ValidityState_ExistingLocalWinsOverMissingRemote) {
  AutofillProfile local(kGuidA, kHttpsOrigin);
  local.SetClientValidityFromBitfieldValue(kValidityStateBitfield);
  AddAutofillProfilesToTable({local});

  // Remote data has a non default validity state bitfield value.
  AutofillProfileSpecifics remote =
      CreateAutofillProfileSpecifics(kGuidA, kHttpsOrigin);
  remote.add_name_first("John");
  ASSERT_FALSE(remote.has_validity_state_bitfield());

  // Expect local autofill profile to still have the validity state after.
  AutofillProfile merged(local);
  merged.SetRawInfo(NAME_FIRST, ASCIIToUTF16("John"));

  // No update to sync, the local validity bitfield should stay untouched.
  EXPECT_CALL(mock_processor(), Put(_, _, _)).Times(0);
  EXPECT_CALL(*backend(), CommitChanges());
  StartSyncing({remote});
  EXPECT_THAT(GetAllLocalData(), ElementsAre(merged));
}

// Missing full name field should not generate sync events.
TEST_F(AutofillProfileSyncBridgeTest,
       RemoteWithSameGuid_FullName_MissingValueNoSync) {
  // Local autofill profile has an empty full name.
  AutofillProfile local(kGuidA, kHttpsOrigin);
  local.SetRawInfo(NAME_FIRST, ASCIIToUTF16("John"));
  AddAutofillProfilesToTable({local});

  // Remote data does not have a full name value.
  AutofillProfileSpecifics remote =
      CreateAutofillProfileSpecifics(kGuidA, kHttpsOrigin);
  remote.add_name_first(std::string("John"));

  // No update to sync, no change in local data.
  EXPECT_CALL(mock_processor(), Put(_, _, _)).Times(0);
  EXPECT_CALL(*backend(), CommitChanges());
  StartSyncing({remote});
  EXPECT_THAT(GetAllLocalData(), ElementsAre(local));
}

TEST_F(AutofillProfileSyncBridgeTest,
       RemoteWithSameGuid_FullName_ExistingLocalWinsOverMissingRemote) {
  // Local autofill profile has a full name.
  AutofillProfile local(kGuidA, kHttpsOrigin);
  local.SetRawInfo(NAME_FULL, ASCIIToUTF16("John Jacob Smith, Jr"));
  AddAutofillProfilesToTable({local});

  // Remote data does not have a full name value.
  AutofillProfileSpecifics remote =
      CreateAutofillProfileSpecifics(kGuidA, kHttpsOrigin);
  remote.add_name_first(std::string("John"));
  remote.add_name_middle(std::string("Jacob"));
  remote.add_name_last(std::string("Smith"));

  // Expect local autofill profile to still have the same full name after.
  AutofillProfile merged(local);
  merged.SetRawInfo(NAME_FIRST, ASCIIToUTF16("John"));
  merged.SetRawInfo(NAME_MIDDLE, ASCIIToUTF16("Jacob"));
  merged.SetRawInfo(NAME_LAST, ASCIIToUTF16("Smith"));

  // No update to sync, no change in local data.
  EXPECT_CALL(mock_processor(), Put(_, _, _)).Times(0);
  EXPECT_CALL(*backend(), CommitChanges());
  StartSyncing({remote});
  EXPECT_THAT(GetAllLocalData(), ElementsAre(merged));
}

// Missing use_count/use_date fields should not generate sync events.
TEST_F(AutofillProfileSyncBridgeTest,
       RemoteWithSameGuid_UsageStats_MissingValueNoSync) {
  // Local autofill profile has 0 for use_count/use_date.
  AutofillProfile local(kGuidA, kHttpsOrigin);
  local.set_language_code("en");
  local.set_use_count(0);
  local.set_use_date(base::Time());
  AddAutofillProfilesToTable({local});

  // Remote data does not have use_count/use_date.
  AutofillProfileSpecifics remote =
      CreateAutofillProfileSpecifics(kGuidA, kHttpsOrigin);
  remote.clear_use_count();
  remote.clear_use_date();
  remote.set_address_home_language_code("en");

  // No update to sync, no change in local data.
  EXPECT_CALL(mock_processor(), Put(_, _, _)).Times(0);
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
  AutofillProfileSyncBridgeUpdatesUsageStatsTest() {}
  ~AutofillProfileSyncBridgeUpdatesUsageStatsTest() override {}

 private:
  DISALLOW_COPY_AND_ASSIGN(AutofillProfileSyncBridgeUpdatesUsageStatsTest);
};

TEST_P(AutofillProfileSyncBridgeUpdatesUsageStatsTest, UpdatesUsageStats) {
  auto test_case = GetParam();

  // Local data has usage stats.
  AutofillProfile local(kGuidA, kHttpsOrigin);
  local.set_language_code("en");
  local.set_use_count(test_case.local_use_count);
  local.set_use_date(test_case.local_use_date);
  ASSERT_EQ(test_case.local_use_count, local.use_count());
  ASSERT_EQ(test_case.local_use_date, local.use_date());
  AddAutofillProfilesToTable({local});

  // Remote data has usage stats.
  AutofillProfileSpecifics remote =
      CreateAutofillProfileSpecifics(kGuidA, kHttpsOrigin);
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
  EXPECT_CALL(mock_processor(), Put(_, _, _)).Times(0);
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

}  // namespace autofill
