// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/autofill/core/browser/webdata/autofill_profile_syncable_service.h"

#include <stddef.h>

#include <memory>
#include <utility>

#include "base/location.h"
#include "base/memory/ptr_util.h"
#include "base/message_loop/message_loop.h"
#include "base/strings/utf_string_conversions.h"
#include "components/autofill/core/browser/autofill_profile.h"
#include "components/autofill/core/browser/country_names.h"
#include "components/autofill/core/browser/webdata/autofill_change.h"
#include "components/autofill/core/common/autofill_constants.h"
#include "components/sync/model/sync_change_processor.h"
#include "components/sync/model/sync_error_factory.h"
#include "components/sync/model/sync_error_factory_mock.h"
#include "components/sync/protocol/sync.pb.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace autofill {

using ::testing::_;
using ::testing::DoAll;
using ::testing::Eq;
using ::testing::Return;
using ::testing::Property;
using base::ASCIIToUTF16;

namespace {

// Some guids for testing.
const char kGuid1[] = "EDC609ED-7EEE-4F27-B00C-423242A9C44B";
const char kGuid2[] = "EDC609ED-7EEE-4F27-B00C-423242A9C44C";
const char kGuid3[] = "EDC609ED-7EEE-4F27-B00C-423242A9C44D";
const char kGuid4[] = "EDC609ED-7EEE-4F27-B00C-423242A9C44E";
const char kEmptyOrigin[] = "";
const int kValidityStateBitfield = 1984;

class MockAutofillProfileSyncableService
    : public AutofillProfileSyncableService {
 public:
  MockAutofillProfileSyncableService() {}
  ~MockAutofillProfileSyncableService() override {}

  using AutofillProfileSyncableService::DataBundle;
  using AutofillProfileSyncableService::set_sync_processor;
  using AutofillProfileSyncableService::CreateData;

  MOCK_METHOD1(LoadAutofillData,
               bool(std::vector<std::unique_ptr<AutofillProfile>>*));
  MOCK_METHOD1(SaveChangesToWebData,
               bool(const AutofillProfileSyncableService::DataBundle&));
};

ACTION_P(LoadAutofillProfiles, datafunc) {
  std::vector<std::unique_ptr<AutofillProfile>> profiles =
      std::move(datafunc());
  arg0->swap(profiles);
}

MATCHER_P(CheckSyncChanges, n_sync_changes_list, "") {
  if (arg.size() != n_sync_changes_list.size())
    return false;
  syncer::SyncChangeList::const_iterator passed, expected;
  for (passed = arg.begin(), expected = n_sync_changes_list.begin();
       passed != arg.end() && expected != n_sync_changes_list.end();
       ++passed, ++expected) {
    DCHECK(passed->IsValid());
    if (passed->change_type() != expected->change_type())
      return false;
    if (passed->sync_data().GetSpecifics().SerializeAsString() !=
            expected->sync_data().GetSpecifics().SerializeAsString()) {
      return false;
    }
  }
  return true;
}

MATCHER_P(DataBundleCheck, n_bundle, "") {
  if ((arg.profiles_to_delete.size() != n_bundle.profiles_to_delete.size()) ||
      (arg.profiles_to_update.size() != n_bundle.profiles_to_update.size()) ||
      (arg.profiles_to_add.size() != n_bundle.profiles_to_add.size()))
    return false;
  for (size_t i = 0; i < arg.profiles_to_delete.size(); ++i) {
    if (arg.profiles_to_delete[i] != n_bundle.profiles_to_delete[i])
      return false;
  }
  for (size_t i = 0; i < arg.profiles_to_update.size(); ++i) {
    if (*arg.profiles_to_update[i] != *n_bundle.profiles_to_update[i])
      return false;
  }
  for (size_t i = 0; i < arg.profiles_to_add.size(); ++i) {
    if (*arg.profiles_to_add[i] != *n_bundle.profiles_to_add[i])
      return false;
  }
  return true;
}

class MockSyncChangeProcessor : public syncer::SyncChangeProcessor {
 public:
  MockSyncChangeProcessor() {}
  ~MockSyncChangeProcessor() override {}

  MOCK_METHOD2(ProcessSyncChanges,
               syncer::SyncError(const base::Location&,
                                 const syncer::SyncChangeList&));
  syncer::SyncDataList GetAllSyncData(syncer::ModelType type) const override {
    return syncer::SyncDataList();
  }
};

class TestSyncChangeProcessor : public syncer::SyncChangeProcessor {
 public:
  TestSyncChangeProcessor() {}
  ~TestSyncChangeProcessor() override {}

  syncer::SyncError ProcessSyncChanges(
      const base::Location& location,
      const syncer::SyncChangeList& changes) override {
    changes_ = changes;
    return syncer::SyncError();
  }

  syncer::SyncDataList GetAllSyncData(syncer::ModelType type) const override {
    return syncer::SyncDataList();
  }

  const syncer::SyncChangeList& changes() { return changes_; }

 private:
  syncer::SyncChangeList changes_;
};

// Returns a profile with all fields set.  Contains identical data to the data
// returned from ConstructCompleteSyncData().
std::unique_ptr<AutofillProfile> ConstructCompleteProfile() {
  std::unique_ptr<AutofillProfile> profile(
      new AutofillProfile(kGuid1, kSettingsOrigin));

  profile->set_use_count(7);
  profile->set_use_date(base::Time::FromTimeT(1423182152));

  profile->SetRawInfo(NAME_FULL, ASCIIToUTF16("John K. Doe, Jr."));
  profile->SetRawInfo(NAME_FIRST, ASCIIToUTF16("John"));
  profile->SetRawInfo(NAME_MIDDLE, ASCIIToUTF16("K."));
  profile->SetRawInfo(NAME_LAST, ASCIIToUTF16("Doe"));

  profile->SetRawInfo(EMAIL_ADDRESS, ASCIIToUTF16("user@example.com"));
  profile->SetRawInfo(PHONE_HOME_WHOLE_NUMBER, ASCIIToUTF16("1.800.555.1234"));

  profile->SetRawInfo(ADDRESS_HOME_STREET_ADDRESS,
                      ASCIIToUTF16("123 Fake St.\n"
                                   "Apt. 42"));
  EXPECT_EQ(ASCIIToUTF16("123 Fake St."),
            profile->GetRawInfo(ADDRESS_HOME_LINE1));
  EXPECT_EQ(ASCIIToUTF16("Apt. 42"), profile->GetRawInfo(ADDRESS_HOME_LINE2));

  profile->SetRawInfo(COMPANY_NAME, ASCIIToUTF16("Google, Inc."));
  profile->SetRawInfo(ADDRESS_HOME_CITY, ASCIIToUTF16("Mountain View"));
  profile->SetRawInfo(ADDRESS_HOME_STATE, ASCIIToUTF16("California"));
  profile->SetRawInfo(ADDRESS_HOME_ZIP, ASCIIToUTF16("94043"));
  profile->SetRawInfo(ADDRESS_HOME_COUNTRY, ASCIIToUTF16("US"));
  profile->SetRawInfo(ADDRESS_HOME_SORTING_CODE, ASCIIToUTF16("CEDEX"));
  profile->SetRawInfo(ADDRESS_HOME_DEPENDENT_LOCALITY,
                      ASCIIToUTF16("Santa Clara"));
  profile->set_language_code("en");
  profile->SetClientValidityFromBitfieldValue(kValidityStateBitfield);
  profile->set_is_client_validity_states_updated(true);
  return profile;
}

// Returns SyncData with all Autofill profile fields set.  Contains identical
// data to the data returned from ConstructCompleteProfile().
syncer::SyncData ConstructCompleteSyncData() {
  sync_pb::EntitySpecifics entity_specifics;
  sync_pb::AutofillProfileSpecifics* specifics =
      entity_specifics.mutable_autofill_profile();

  specifics->set_guid(kGuid1);
  specifics->set_origin(kSettingsOrigin);
  specifics->set_use_count(7);
  specifics->set_use_date(1423182152);

  specifics->add_name_first("John");
  specifics->add_name_middle("K.");
  specifics->add_name_last("Doe");
  specifics->add_name_full("John K. Doe, Jr.");

  specifics->add_email_address("user@example.com");

  specifics->add_phone_home_whole_number("1.800.555.1234");

  specifics->set_address_home_line1("123 Fake St.");
  specifics->set_address_home_line2("Apt. 42");
  specifics->set_address_home_street_address("123 Fake St.\n"
                                             "Apt. 42");

  specifics->set_company_name("Google, Inc.");
  specifics->set_address_home_city("Mountain View");
  specifics->set_address_home_state("California");
  specifics->set_address_home_zip("94043");
  specifics->set_address_home_country("US");
  specifics->set_address_home_sorting_code("CEDEX");
  specifics->set_address_home_dependent_locality("Santa Clara");
  specifics->set_address_home_language_code("en");
  specifics->set_validity_state_bitfield(kValidityStateBitfield);
  specifics->set_is_client_validity_states_updated(true);

  return syncer::SyncData::CreateLocalData(kGuid1, kGuid1, entity_specifics);
}

}  // namespace

class AutofillProfileSyncableServiceTest : public testing::Test {
 public:
  AutofillProfileSyncableServiceTest() {
    CountryNames::SetLocaleString("en-US");
  }

  void SetUp() override { sync_processor_.reset(new MockSyncChangeProcessor); }

  // Wrapper around AutofillProfileSyncableService::MergeDataAndStartSyncing()
  // that also verifies expectations.
  void MergeDataAndStartSyncing(
      std::vector<std::unique_ptr<AutofillProfile>> profiles_from_web_db,
      const syncer::SyncDataList& data_list,
      const MockAutofillProfileSyncableService::DataBundle& expected_bundle,
      const syncer::SyncChangeList& expected_change_list) {
    auto profile_returner = [&profiles_from_web_db]() {
      return std::move(profiles_from_web_db);
    };
    EXPECT_CALL(autofill_syncable_service_, LoadAutofillData(_))
        .Times(1)
        .WillOnce(DoAll(LoadAutofillProfiles(profile_returner), Return(true)));
    EXPECT_CALL(autofill_syncable_service_,
                SaveChangesToWebData(DataBundleCheck(expected_bundle)))
        .Times(1)
        .WillOnce(Return(true));
    if (expected_change_list.empty()) {
      EXPECT_CALL(*sync_processor_, ProcessSyncChanges(_, _)).Times(0);
    } else {
      ON_CALL(*sync_processor_, ProcessSyncChanges(_, _))
          .WillByDefault(Return(syncer::SyncError()));
      EXPECT_CALL(*sync_processor_,
                  ProcessSyncChanges(_, CheckSyncChanges(expected_change_list)))
          .Times(1)
          .WillOnce(Return(syncer::SyncError()));
    }

    // Takes ownership of sync_processor_.
    autofill_syncable_service_.MergeDataAndStartSyncing(
        syncer::AUTOFILL_PROFILE, data_list, std::move(sync_processor_),
        std::unique_ptr<syncer::SyncErrorFactory>(
            new syncer::SyncErrorFactoryMock()));
  }

 protected:
  base::MessageLoop message_loop_;
  MockAutofillProfileSyncableService autofill_syncable_service_;
  std::unique_ptr<MockSyncChangeProcessor> sync_processor_;
};

TEST_F(AutofillProfileSyncableServiceTest, MergeDataAndStartSyncing) {
  std::vector<std::unique_ptr<AutofillProfile>> profiles_from_web_db;
  std::string guid_present1 = kGuid1;
  std::string guid_present2 = kGuid2;
  std::string guid_synced1 = kGuid3;
  std::string guid_synced2 = kGuid4;
  std::string origin_present1 = kEmptyOrigin;
  std::string origin_present2 = kEmptyOrigin;
  std::string origin_synced1 = kEmptyOrigin;
  std::string origin_synced2 = kSettingsOrigin;

  profiles_from_web_db.push_back(
      std::make_unique<AutofillProfile>(guid_present1, origin_present1));
  profiles_from_web_db.back()->SetRawInfo(NAME_FIRST, ASCIIToUTF16("John"));
  profiles_from_web_db.back()->SetRawInfo(ADDRESS_HOME_LINE1,
                                          ASCIIToUTF16("1 1st st"));
  profiles_from_web_db.push_back(
      std::make_unique<AutofillProfile>(guid_present2, origin_present2));
  profiles_from_web_db.back()->SetRawInfo(NAME_FIRST, ASCIIToUTF16("Tom"));
  profiles_from_web_db.back()->SetRawInfo(ADDRESS_HOME_LINE1,
                                          ASCIIToUTF16("2 2nd st"));

  syncer::SyncDataList data_list;
  AutofillProfile profile1(guid_synced1, origin_synced1);
  profile1.SetRawInfo(NAME_FIRST, ASCIIToUTF16("Jane"));
  data_list.push_back(autofill_syncable_service_.CreateData(profile1));
  AutofillProfile profile2(guid_synced2, origin_synced2);
  profile2.SetRawInfo(NAME_FIRST, ASCIIToUTF16("Harry"));
  data_list.push_back(autofill_syncable_service_.CreateData(profile2));
  // This one will have the name and origin updated.
  AutofillProfile profile3(guid_present2, origin_synced2);
  profile3.SetRawInfo(NAME_FIRST, ASCIIToUTF16("Tom Doe"));
  data_list.push_back(autofill_syncable_service_.CreateData(profile3));

  syncer::SyncChangeList expected_change_list;
  expected_change_list.push_back(
      syncer::SyncChange(FROM_HERE,
                         syncer::SyncChange::ACTION_ADD,
                         MockAutofillProfileSyncableService::CreateData(
                             *profiles_from_web_db.front())));

  MockAutofillProfileSyncableService::DataBundle expected_bundle;
  expected_bundle.profiles_to_add.push_back(&profile1);
  expected_bundle.profiles_to_add.push_back(&profile2);
  expected_bundle.profiles_to_update.push_back(&profile3);

  MergeDataAndStartSyncing(std::move(profiles_from_web_db), data_list,
                           expected_bundle, expected_change_list);
  autofill_syncable_service_.StopSyncing(syncer::AUTOFILL_PROFILE);
}

TEST_F(AutofillProfileSyncableServiceTest, MergeIdenticalProfiles) {
  std::vector<std::unique_ptr<AutofillProfile>> profiles_from_web_db;
  std::string guid_present1 = kGuid1;
  std::string guid_present2 = kGuid2;
  std::string guid_synced1 = kGuid3;
  std::string guid_synced2 = kGuid4;
  std::string origin_present1 = kEmptyOrigin;
  std::string origin_present2 = kSettingsOrigin;
  std::string origin_synced1 = kEmptyOrigin;
  std::string origin_synced2 = kEmptyOrigin;

  profiles_from_web_db.push_back(
      std::make_unique<AutofillProfile>(guid_present1, origin_present1));
  profiles_from_web_db.back()->SetRawInfo(NAME_FIRST, ASCIIToUTF16("John"));
  profiles_from_web_db.back()->SetRawInfo(ADDRESS_HOME_LINE1,
                                          ASCIIToUTF16("1 1st st"));
  profiles_from_web_db.push_back(
      std::make_unique<AutofillProfile>(guid_present2, origin_present2));
  profiles_from_web_db.back()->SetRawInfo(NAME_FIRST, ASCIIToUTF16("Tom"));
  profiles_from_web_db.back()->SetRawInfo(ADDRESS_HOME_LINE1,
                                          ASCIIToUTF16("2 2nd st"));

  // The synced profiles are identical to the local ones, except that the guids
  // are different.
  syncer::SyncDataList data_list;
  AutofillProfile profile1(guid_synced1, origin_synced1);
  profile1.SetRawInfo(NAME_FIRST, ASCIIToUTF16("John"));
  profile1.SetRawInfo(ADDRESS_HOME_LINE1, ASCIIToUTF16("1 1st st"));
  data_list.push_back(autofill_syncable_service_.CreateData(profile1));
  AutofillProfile profile2(guid_synced2, origin_synced2);
  profile2.SetRawInfo(NAME_FIRST, ASCIIToUTF16("Tom"));
  profile2.SetRawInfo(ADDRESS_HOME_LINE1, ASCIIToUTF16("2 2nd st"));
  data_list.push_back(autofill_syncable_service_.CreateData(profile2));

  AutofillProfile expected_profile(profile2);
  expected_profile.set_origin(kSettingsOrigin);
  syncer::SyncChangeList expected_change_list;
  expected_change_list.push_back(
      syncer::SyncChange(FROM_HERE,
                         syncer::SyncChange::ACTION_UPDATE,
                         MockAutofillProfileSyncableService::CreateData(
                             expected_profile)));

  MockAutofillProfileSyncableService::DataBundle expected_bundle;
  expected_bundle.profiles_to_delete.push_back(guid_present1);
  expected_bundle.profiles_to_delete.push_back(guid_present2);
  expected_bundle.profiles_to_add.push_back(&profile1);
  expected_bundle.profiles_to_add.push_back(&expected_profile);

  MergeDataAndStartSyncing(std::move(profiles_from_web_db), data_list,
                           expected_bundle, expected_change_list);
  autofill_syncable_service_.StopSyncing(syncer::AUTOFILL_PROFILE);
}

TEST_F(AutofillProfileSyncableServiceTest, MergeSimilarProfiles) {
  std::vector<std::unique_ptr<AutofillProfile>> profiles_from_web_db;
  std::string guid_present1 = kGuid1;
  std::string guid_present2 = kGuid2;
  std::string guid_synced1 = kGuid3;
  std::string guid_synced2 = kGuid4;
  std::string origin_present1 = kEmptyOrigin;
  std::string origin_present2 = kSettingsOrigin;
  std::string origin_synced1 = kEmptyOrigin;
  std::string origin_synced2 = kEmptyOrigin;

  profiles_from_web_db.push_back(
      std::make_unique<AutofillProfile>(guid_present1, origin_present1));
  profiles_from_web_db.back()->SetRawInfo(NAME_FIRST, ASCIIToUTF16("John"));
  profiles_from_web_db.back()->SetRawInfo(ADDRESS_HOME_LINE1,
                                          ASCIIToUTF16("1 1st st"));
  profiles_from_web_db.back()->set_use_count(27);
  profiles_from_web_db.push_back(
      std::make_unique<AutofillProfile>(guid_present2, origin_present2));
  profiles_from_web_db.back()->SetRawInfo(NAME_FIRST, ASCIIToUTF16("Tom"));
  profiles_from_web_db.back()->SetRawInfo(ADDRESS_HOME_LINE1,
                                          ASCIIToUTF16("2 2nd st"));

  // The synced profiles are identical to the local ones, except that the guids
  // and use_count values are different.
  syncer::SyncDataList data_list;
  AutofillProfile profile1(guid_synced1, origin_synced1);
  profile1.SetRawInfo(NAME_FIRST, ASCIIToUTF16("John"));
  profile1.SetRawInfo(ADDRESS_HOME_LINE1, ASCIIToUTF16("1 1st st"));
  profile1.SetRawInfo(COMPANY_NAME, ASCIIToUTF16("Frobbers, Inc."));
  profile1.set_use_count(13);
  data_list.push_back(autofill_syncable_service_.CreateData(profile1));
  AutofillProfile profile2(guid_synced2, origin_synced2);
  profile2.SetRawInfo(NAME_FIRST, ASCIIToUTF16("Tom"));
  profile2.SetRawInfo(ADDRESS_HOME_LINE1, ASCIIToUTF16("2 2nd st"));
  profile2.SetRawInfo(COMPANY_NAME, ASCIIToUTF16("Fizzbang, LLC."));
  profile1.set_use_count(4);
  data_list.push_back(autofill_syncable_service_.CreateData(profile2));

  // The first profile should have its origin updated.
  // The second profile should remain as-is, because an unverified profile
  // should never overwrite a verified one.
  AutofillProfile expected_profile(profile1);
  expected_profile.set_origin(origin_present1);
  expected_profile.SetRawInfo(NAME_FULL, ASCIIToUTF16("John"));
  // Merging two profile takes their max use count.
  expected_profile.set_use_count(27);
  syncer::SyncChangeList expected_change_list;
  expected_change_list.push_back(
      syncer::SyncChange(FROM_HERE,
                         syncer::SyncChange::ACTION_ADD,
                         MockAutofillProfileSyncableService::CreateData(
                             *profiles_from_web_db.back())));
  expected_change_list.push_back(
      syncer::SyncChange(FROM_HERE,
                         syncer::SyncChange::ACTION_UPDATE,
                         MockAutofillProfileSyncableService::CreateData(
                             expected_profile)));

  MockAutofillProfileSyncableService::DataBundle expected_bundle;
  expected_bundle.profiles_to_delete.push_back(guid_present1);
  expected_bundle.profiles_to_add.push_back(&expected_profile);
  expected_bundle.profiles_to_add.push_back(&profile2);

  MergeDataAndStartSyncing(std::move(profiles_from_web_db), data_list,
                           expected_bundle, expected_change_list);
  autofill_syncable_service_.StopSyncing(syncer::AUTOFILL_PROFILE);
}

// Ensure that no Sync events are generated to fill in missing origins from Sync
// with explicitly present empty ones.  This ensures that the migration to add
// origins to profiles does not generate lots of needless Sync updates.
TEST_F(AutofillProfileSyncableServiceTest, MergeDataEmptyOrigins) {
  std::vector<std::unique_ptr<AutofillProfile>> profiles_from_web_db;

  // Create a profile with an empty origin.
  AutofillProfile profile(kGuid1, std::string());
  profile.SetRawInfo(NAME_FIRST, ASCIIToUTF16("John"));
  profile.SetRawInfo(ADDRESS_HOME_LINE1, ASCIIToUTF16("1 1st st"));

  profiles_from_web_db.push_back(std::make_unique<AutofillProfile>(profile));

  // Create a Sync profile identical to |profile|, except with no origin set.
  sync_pb::EntitySpecifics specifics;
  sync_pb::AutofillProfileSpecifics* autofill_specifics =
      specifics.mutable_autofill_profile();
  autofill_specifics->set_guid(profile.guid());
  autofill_specifics->add_name_first("John");
  autofill_specifics->add_name_middle(std::string());
  autofill_specifics->add_name_last(std::string());
  autofill_specifics->add_name_full(std::string());
  autofill_specifics->add_email_address(std::string());
  autofill_specifics->add_phone_home_whole_number(std::string());
  autofill_specifics->set_address_home_line1("1 1st st");
  autofill_specifics->set_use_count(profile.use_count());
  autofill_specifics->set_use_date(profile.use_date().ToTimeT());
  EXPECT_FALSE(autofill_specifics->has_origin());

  syncer::SyncDataList data_list;
  data_list.push_back(
      syncer::SyncData::CreateLocalData(
          profile.guid(), profile.guid(), specifics));

  MockAutofillProfileSyncableService::DataBundle expected_bundle;
  syncer::SyncChangeList expected_change_list;
  MergeDataAndStartSyncing(std::move(profiles_from_web_db), data_list,
                           expected_bundle, expected_change_list);
  autofill_syncable_service_.StopSyncing(syncer::AUTOFILL_PROFILE);
}

TEST_F(AutofillProfileSyncableServiceTest, GetAllSyncData) {
  std::vector<std::unique_ptr<AutofillProfile>> profiles_from_web_db;
  std::string guid_present1 = kGuid1;
  std::string guid_present2 = kGuid2;

  profiles_from_web_db.push_back(
      std::make_unique<AutofillProfile>(guid_present1, kEmptyOrigin));
  profiles_from_web_db.back()->SetRawInfo(NAME_FIRST, ASCIIToUTF16("John"));
  profiles_from_web_db.push_back(
      std::make_unique<AutofillProfile>(guid_present2, kEmptyOrigin));
  profiles_from_web_db.back()->SetRawInfo(NAME_FIRST, ASCIIToUTF16("Jane"));

  syncer::SyncChangeList expected_change_list;
  expected_change_list.push_back(
      syncer::SyncChange(FROM_HERE,
                         syncer::SyncChange::ACTION_ADD,
                         MockAutofillProfileSyncableService::CreateData(
                             *profiles_from_web_db.front())));
  expected_change_list.push_back(
      syncer::SyncChange(FROM_HERE,
                         syncer::SyncChange::ACTION_ADD,
                         MockAutofillProfileSyncableService::CreateData(
                             *profiles_from_web_db.back())));

  MockAutofillProfileSyncableService::DataBundle expected_bundle;
  syncer::SyncDataList data_list;
  MergeDataAndStartSyncing(std::move(profiles_from_web_db), data_list,
                           expected_bundle, expected_change_list);

  syncer::SyncDataList data =
      autofill_syncable_service_.GetAllSyncData(syncer::AUTOFILL_PROFILE);

  ASSERT_EQ(2U, data.size());
  EXPECT_EQ(guid_present1, data[0].GetSpecifics().autofill_profile().guid());
  EXPECT_EQ(guid_present2, data[1].GetSpecifics().autofill_profile().guid());
  EXPECT_EQ(kEmptyOrigin, data[0].GetSpecifics().autofill_profile().origin());
  EXPECT_EQ(kEmptyOrigin, data[1].GetSpecifics().autofill_profile().origin());

  autofill_syncable_service_.StopSyncing(syncer::AUTOFILL_PROFILE);
}

TEST_F(AutofillProfileSyncableServiceTest, ProcessSyncChanges) {
  std::vector<AutofillProfile *> profiles_from_web_db;
  std::string guid_present = kGuid1;
  std::string guid_synced = kGuid2;

  syncer::SyncChangeList change_list;
  AutofillProfile profile(guid_synced, kEmptyOrigin);
  profile.SetRawInfo(NAME_FIRST, ASCIIToUTF16("Jane"));
  change_list.push_back(
      syncer::SyncChange(
          FROM_HERE,
          syncer::SyncChange::ACTION_ADD,
          MockAutofillProfileSyncableService::CreateData(profile)));
  AutofillProfile empty_profile(guid_present, kEmptyOrigin);
  change_list.push_back(
      syncer::SyncChange(
          FROM_HERE,
          syncer::SyncChange::ACTION_DELETE,
          MockAutofillProfileSyncableService::CreateData(empty_profile)));

  MockAutofillProfileSyncableService::DataBundle expected_bundle;
  expected_bundle.profiles_to_delete.push_back(guid_present);
  expected_bundle.profiles_to_add.push_back(&profile);

  EXPECT_CALL(autofill_syncable_service_, SaveChangesToWebData(
              DataBundleCheck(expected_bundle)))
      .Times(1)
      .WillOnce(Return(true));

  autofill_syncable_service_.set_sync_processor(sync_processor_.release());
  syncer::SyncError error = autofill_syncable_service_.ProcessSyncChanges(
      FROM_HERE, change_list);

  EXPECT_FALSE(error.IsSet());
}

TEST_F(AutofillProfileSyncableServiceTest, AutofillProfileAdded) {
  // Will be owned by the syncable service.  Keep a reference available here for
  // verifying test expectations.
  TestSyncChangeProcessor* sync_change_processor = new TestSyncChangeProcessor;
  autofill_syncable_service_.set_sync_processor(sync_change_processor);

  AutofillProfile profile(kGuid1, kEmptyOrigin);
  profile.SetRawInfo(NAME_FIRST, ASCIIToUTF16("Jane"));
  AutofillProfileChange change(AutofillProfileChange::ADD, kGuid1, &profile);
  autofill_syncable_service_.AutofillProfileChanged(change);

  ASSERT_EQ(1U, sync_change_processor->changes().size());
  syncer::SyncChange result = sync_change_processor->changes()[0];
  EXPECT_EQ(syncer::SyncChange::ACTION_ADD, result.change_type());

  sync_pb::AutofillProfileSpecifics specifics =
      result.sync_data().GetSpecifics().autofill_profile();
  EXPECT_EQ(kGuid1, specifics.guid());
  EXPECT_EQ(kEmptyOrigin, specifics.origin());
  EXPECT_THAT(specifics.name_first(), testing::ElementsAre("Jane"));
}

TEST_F(AutofillProfileSyncableServiceTest, AutofillProfileDeleted) {
  // Will be owned by the syncable service.  Keep a reference available here for
  // verifying test expectations.
  TestSyncChangeProcessor* sync_change_processor = new TestSyncChangeProcessor;
  autofill_syncable_service_.set_sync_processor(sync_change_processor);

  // First add the profile so we have something to delete.
  AutofillProfile profile(kGuid1, kEmptyOrigin);
  profile.SetRawInfo(NAME_FIRST, ASCIIToUTF16("Jane"));
  AutofillProfileChange change1(AutofillProfileChange::ADD, kGuid1, &profile);
  autofill_syncable_service_.AutofillProfileChanged(change1);

  AutofillProfileChange change2(AutofillProfileChange::REMOVE, kGuid1, nullptr);
  autofill_syncable_service_.AutofillProfileChanged(change2);

  ASSERT_EQ(1U, sync_change_processor->changes().size());
  syncer::SyncChange result = sync_change_processor->changes()[0];
  EXPECT_EQ(syncer::SyncChange::ACTION_DELETE, result.change_type());
  sync_pb::AutofillProfileSpecifics specifics =
      result.sync_data().GetSpecifics().autofill_profile();
  EXPECT_EQ(kGuid1, specifics.guid());
}

TEST_F(AutofillProfileSyncableServiceTest,
       AutofillProfileDeletedIgnoresUnknown) {
  // Will be owned by the syncable service.  Keep a reference available here for
  // verifying test expectations.
  TestSyncChangeProcessor* sync_change_processor = new TestSyncChangeProcessor;
  autofill_syncable_service_.set_sync_processor(sync_change_processor);

  AutofillProfileChange change(AutofillProfileChange::REMOVE, kGuid2, nullptr);
  autofill_syncable_service_.AutofillProfileChanged(change);

  ASSERT_EQ(0U, sync_change_processor->changes().size());
}

TEST_F(AutofillProfileSyncableServiceTest, UpdateField) {
  AutofillProfile profile(kGuid1, kSettingsOrigin);
  std::string company1 = "A Company";
  std::string company2 = "Another Company";
  profile.SetRawInfo(COMPANY_NAME, ASCIIToUTF16(company1));
  EXPECT_FALSE(AutofillProfileSyncableService::UpdateField(
      COMPANY_NAME, company1, &profile));
  EXPECT_EQ(profile.GetRawInfo(COMPANY_NAME), ASCIIToUTF16(company1));
  EXPECT_TRUE(AutofillProfileSyncableService::UpdateField(
      COMPANY_NAME, company2, &profile));
  EXPECT_EQ(profile.GetRawInfo(COMPANY_NAME), ASCIIToUTF16(company2));
  EXPECT_FALSE(AutofillProfileSyncableService::UpdateField(
      COMPANY_NAME, company2, &profile));
  EXPECT_EQ(profile.GetRawInfo(COMPANY_NAME), ASCIIToUTF16(company2));
}

// Tests that MergeSimilarProfiles adds the additional information of
// |from_profile| into |into_profile| but not the other way around.
TEST_F(AutofillProfileSyncableServiceTest,
       MergeSimilarProfiles_AdditionalInfoInBothProfiles) {
  AutofillProfile into_profile(kGuid1, kEmptyOrigin);
  into_profile.SetRawInfo(ADDRESS_HOME_LINE1, ASCIIToUTF16("111 First St."));

  AutofillProfile from_profile(kGuid2, kEmptyOrigin);
  from_profile.SetRawInfo(ADDRESS_HOME_LINE1, ASCIIToUTF16("111 First St."));

  from_profile.set_use_count(0);
  into_profile.set_use_count(0);

  into_profile.set_use_date(base::Time::FromTimeT(1234));
  from_profile.set_use_date(base::Time::FromTimeT(1234));

  into_profile.SetRawInfo(NAME_FIRST, ASCIIToUTF16("John"));
  from_profile.SetRawInfo(NAME_FIRST, ASCIIToUTF16("John"));

  into_profile.SetRawInfo(NAME_LAST, ASCIIToUTF16("Doe"));
  from_profile.SetRawInfo(NAME_LAST, ASCIIToUTF16("Doe"));

  from_profile.SetRawInfo(PHONE_HOME_WHOLE_NUMBER, ASCIIToUTF16("650234567"));

  into_profile.set_language_code("en");

  // Expect true because the phone number and origin of |from_profile| were
  // saved in |into_profile|.
  EXPECT_TRUE(AutofillProfileSyncableService::MergeSimilarProfiles(
      from_profile, &into_profile, "en-US"));
  EXPECT_EQ(ASCIIToUTF16("650234567"),
            into_profile.GetRawInfo(PHONE_HOME_WHOLE_NUMBER));
  EXPECT_EQ(kEmptyOrigin, into_profile.origin());

  // Make sure that the language code of |into_profile| was not added to
  // |from_profile|.
  EXPECT_EQ("", from_profile.language_code());
}

// Tests that MergeSimilarProfiles keeps the most recent use date of the two
// profiles being merged.
TEST_F(AutofillProfileSyncableServiceTest,
       MergeSimilarProfiles_DifferentUseDates) {
  // Different guids, same origin.
  AutofillProfile into_profile(kGuid1, kEmptyOrigin);
  AutofillProfile from_profile(kGuid2, kEmptyOrigin);

  from_profile.set_use_count(0);
  into_profile.set_use_count(0);

  // |from_profile| has a more recent use date.
  from_profile.set_use_date(base::Time::FromTimeT(30));
  into_profile.set_use_date(base::Time::FromTimeT(25));

  // Expect true because the use date of |from_profile| replaced the use date of
  // |into_profile|.
  EXPECT_TRUE(AutofillProfileSyncableService::MergeSimilarProfiles(
      from_profile, &into_profile, "en-US"));
  EXPECT_EQ(base::Time::FromTimeT(30), into_profile.use_date());

  // |into_profile| has a more recent use date.
  into_profile.set_use_date(base::Time::FromTimeT(35));

  // Expect false because |from_profile| was not updated in any way by
  // |into_profile|.
  EXPECT_FALSE(AutofillProfileSyncableService::MergeSimilarProfiles(
      from_profile, &into_profile, "en-US"));
  EXPECT_EQ(base::Time::FromTimeT(35), into_profile.use_date());
}

// Tests that MergeSimilarProfiles saves the max of the use counts of the two
// profiles in |into_profile|.
TEST_F(AutofillProfileSyncableServiceTest,
       MergeSimilarProfiles_NonZeroUseCounts) {
  // Different guids, same origin, same use date.
  AutofillProfile into_profile(kGuid1, kEmptyOrigin);
  AutofillProfile from_profile(kGuid2, kEmptyOrigin);
  from_profile.set_use_date(base::Time::FromTimeT(1234));
  into_profile.set_use_date(base::Time::FromTimeT(1234));

  from_profile.set_use_count(12);
  into_profile.set_use_count(5);

  // Expect true because the use count of |from_profile| was added to the use
  // count of |into_profile|.
  EXPECT_TRUE(AutofillProfileSyncableService::MergeSimilarProfiles(
      from_profile, &into_profile, "en-US"));
  EXPECT_EQ(12U, into_profile.use_count());
}

// Ensure that all profile fields are able to be synced up from the client to
// the server.
TEST_F(AutofillProfileSyncableServiceTest, SyncAllFieldsToServer) {
  std::vector<std::unique_ptr<AutofillProfile>> profiles_from_web_db;

  // Create a profile with all fields set.
  profiles_from_web_db.push_back(ConstructCompleteProfile());

  // Set up expectations: No changes to the WebDB, and all fields correctly
  // copied to Sync.
  MockAutofillProfileSyncableService::DataBundle expected_bundle;
  syncer::SyncChangeList expected_change_list;
  expected_change_list.push_back(
      syncer::SyncChange(FROM_HERE,
                         syncer::SyncChange::ACTION_ADD,
                         ConstructCompleteSyncData()));

  // Verify the expectations.
  syncer::SyncDataList data_list;
  MergeDataAndStartSyncing(std::move(profiles_from_web_db), data_list,
                           expected_bundle, expected_change_list);
  autofill_syncable_service_.StopSyncing(syncer::AUTOFILL_PROFILE);
}

// Ensure that all profile fields are able to be synced down from the server to
// the client.
TEST_F(AutofillProfileSyncableServiceTest, SyncAllFieldsToClient) {
  // Create a profile with all fields set.
  syncer::SyncDataList data_list;
  data_list.push_back(ConstructCompleteSyncData());

  // Set up expectations: All fields correctly copied to the WebDB, and no
  // changes propagated to Sync.
  syncer::SyncChangeList expected_change_list;
  std::unique_ptr<AutofillProfile> expected_profile =
      ConstructCompleteProfile();
  MockAutofillProfileSyncableService::DataBundle expected_bundle;
  expected_bundle.profiles_to_add.push_back(expected_profile.get());

  // Verify the expectations.
  std::vector<std::unique_ptr<AutofillProfile>> profiles_from_web_db;
  MergeDataAndStartSyncing(std::move(profiles_from_web_db), data_list,
                           expected_bundle, expected_change_list);
  autofill_syncable_service_.StopSyncing(syncer::AUTOFILL_PROFILE);
}

// Ensure that the street address field takes precedence over the address line 1
// and line 2 fields, even though these are expected to always be in sync in
// practice.
TEST_F(AutofillProfileSyncableServiceTest,
       StreetAddressTakesPrecedenceOverAddressLines) {
  // Create a Sync profile with conflicting address data in the street address
  // field vs. the address line 1 and address line 2 fields.
  sync_pb::EntitySpecifics specifics;
  sync_pb::AutofillProfileSpecifics* autofill_specifics =
      specifics.mutable_autofill_profile();
  autofill_specifics->set_guid(kGuid1);
  autofill_specifics->set_origin(kEmptyOrigin);
  autofill_specifics->add_name_first(std::string());
  autofill_specifics->add_name_middle(std::string());
  autofill_specifics->add_name_last(std::string());
  autofill_specifics->add_email_address(std::string());
  autofill_specifics->add_phone_home_whole_number(std::string());
  autofill_specifics->set_address_home_line1("123 Example St.");
  autofill_specifics->set_address_home_line2("Apt. 42");
  autofill_specifics->set_address_home_street_address("456 El Camino Real\n"
                                                      "Suite #1337");

  syncer::SyncDataList data_list;
  data_list.push_back(
      syncer::SyncData::CreateLocalData(kGuid1, kGuid1, specifics));

  // Set up expectations: Full street address takes precedence over address
  // lines.
  syncer::SyncChangeList expected_change_list;
  AutofillProfile expected_profile(kGuid1, kEmptyOrigin);
  expected_profile.SetRawInfo(ADDRESS_HOME_STREET_ADDRESS,
                              ASCIIToUTF16("456 El Camino Real\n"
                                           "Suite #1337"));
  EXPECT_EQ(ASCIIToUTF16("456 El Camino Real"),
            expected_profile.GetRawInfo(ADDRESS_HOME_LINE1));
  EXPECT_EQ(ASCIIToUTF16("Suite #1337"),
            expected_profile.GetRawInfo(ADDRESS_HOME_LINE2));
  MockAutofillProfileSyncableService::DataBundle expected_bundle;
  expected_bundle.profiles_to_add.push_back(&expected_profile);

  // Verify the expectations.
  std::vector<std::unique_ptr<AutofillProfile>> profiles_from_web_db;
  MergeDataAndStartSyncing(std::move(profiles_from_web_db), data_list,
                           expected_bundle, expected_change_list);
  autofill_syncable_service_.StopSyncing(syncer::AUTOFILL_PROFILE);
}

// Ensure that no Sync events are generated to fill in missing street address
// fields from Sync with explicitly present ones identical to the data stored in
// the line1 and line2 fields.  This ensures that the migration to add the
// street address field to profiles does not generate lots of needless Sync
// updates.
TEST_F(AutofillProfileSyncableServiceTest, MergeDataEmptyStreetAddress) {
  std::vector<std::unique_ptr<AutofillProfile>> profiles_from_web_db;

  // Create a profile with the street address set.
  AutofillProfile profile(kGuid1, kEmptyOrigin);
  profile.SetRawInfo(ADDRESS_HOME_STREET_ADDRESS,
                     ASCIIToUTF16("123 Example St.\n"
                                  "Apt. 42"));
  EXPECT_EQ(ASCIIToUTF16("123 Example St."),
            profile.GetRawInfo(ADDRESS_HOME_LINE1));
  EXPECT_EQ(ASCIIToUTF16("Apt. 42"), profile.GetRawInfo(ADDRESS_HOME_LINE2));

  profiles_from_web_db.push_back(std::make_unique<AutofillProfile>(profile));

  // Create a Sync profile identical to |profile|, except without street address
  // explicitly set.
  sync_pb::EntitySpecifics specifics;
  sync_pb::AutofillProfileSpecifics* autofill_specifics =
      specifics.mutable_autofill_profile();
  autofill_specifics->set_guid(profile.guid());
  autofill_specifics->set_origin(profile.origin());
  autofill_specifics->add_name_first(std::string());
  autofill_specifics->add_name_middle(std::string());
  autofill_specifics->add_name_last(std::string());
  autofill_specifics->add_name_full(std::string());
  autofill_specifics->add_email_address(std::string());
  autofill_specifics->add_phone_home_whole_number(std::string());
  autofill_specifics->set_address_home_line1("123 Example St.");
  autofill_specifics->set_address_home_line2("Apt. 42");
  autofill_specifics->set_use_count(profile.use_count());
  autofill_specifics->set_use_date(profile.use_date().ToTimeT());
  EXPECT_FALSE(autofill_specifics->has_address_home_street_address());

  syncer::SyncDataList data_list;
  data_list.push_back(
      syncer::SyncData::CreateLocalData(
          profile.guid(), profile.guid(), specifics));

  MockAutofillProfileSyncableService::DataBundle expected_bundle;
  syncer::SyncChangeList expected_change_list;
  MergeDataAndStartSyncing(std::move(profiles_from_web_db), data_list,
                           expected_bundle, expected_change_list);
  autofill_syncable_service_.StopSyncing(syncer::AUTOFILL_PROFILE);
}

// Sync data without origin should not overwrite existing origin in local
// autofill profile.
TEST_F(AutofillProfileSyncableServiceTest, EmptySyncPreservesOrigin) {
  std::vector<std::unique_ptr<AutofillProfile>> profiles_from_web_db;

  // Local autofill profile has an origin.
  AutofillProfile profile(kGuid1, kEmptyOrigin);
  profiles_from_web_db.push_back(std::make_unique<AutofillProfile>(profile));

  // Remote data does not have an origin value.
  sync_pb::EntitySpecifics specifics;
  sync_pb::AutofillProfileSpecifics* autofill_specifics =
      specifics.mutable_autofill_profile();
  autofill_specifics->set_guid(profile.guid());
  autofill_specifics->add_name_first("John");
  autofill_specifics->add_name_middle(std::string());
  autofill_specifics->add_name_last(std::string());
  autofill_specifics->add_name_full(std::string());
  autofill_specifics->add_email_address(std::string());
  autofill_specifics->add_phone_home_whole_number(std::string());
  EXPECT_FALSE(autofill_specifics->has_origin());

  syncer::SyncDataList data_list;
  data_list.push_back(
      syncer::SyncData::CreateLocalData(
          profile.guid(), profile.guid(), specifics));

  // Expect the local autofill profile to still have an origin after sync.
  MockAutofillProfileSyncableService::DataBundle expected_bundle;
  AutofillProfile expected_profile(profile.guid(), profile.origin());
  expected_profile.SetRawInfo(NAME_FIRST, ASCIIToUTF16("John"));
  expected_bundle.profiles_to_update.push_back(&expected_profile);

  // Expect no sync events to add origin to the remote data.
  syncer::SyncChangeList expected_empty_change_list;

  MergeDataAndStartSyncing(std::move(profiles_from_web_db), data_list,
                           expected_bundle, expected_empty_change_list);
  autofill_syncable_service_.StopSyncing(syncer::AUTOFILL_PROFILE);
}

// Sync data without origin should not overwrite existing origin in local
// autofill profile.
TEST_F(AutofillProfileSyncableServiceTest,
       NonSettingsOriginFromSyncIsIgnored_Merge) {
  std::vector<std::unique_ptr<AutofillProfile>> profiles_from_web_db;

  // Remote data has no origin value.
  AutofillProfile profile(kGuid1, std::string());
  profiles_from_web_db.push_back(std::make_unique<AutofillProfile>(profile));

  // Remote data has a non-settings origin value.
  sync_pb::EntitySpecifics specifics;
  sync_pb::AutofillProfileSpecifics* autofill_specifics =
      specifics.mutable_autofill_profile();
  autofill_specifics->set_guid(profile.guid());
  autofill_specifics->set_origin("www.example.com");
  autofill_specifics->add_name_first("John");
  autofill_specifics->add_name_middle(std::string());
  autofill_specifics->add_name_last(std::string());
  autofill_specifics->add_name_full(std::string());
  autofill_specifics->add_email_address(std::string());
  autofill_specifics->add_phone_home_whole_number(std::string());

  syncer::SyncDataList data_list;
  data_list.push_back(syncer::SyncData::CreateLocalData(
      profile.guid(), profile.guid(), specifics));

  // Expect the local autofill profile to still have an origin after sync.
  MockAutofillProfileSyncableService::DataBundle expected_bundle;
  AutofillProfile expected_profile(profile.guid(), profile.origin());
  expected_profile.SetRawInfo(NAME_FIRST, ASCIIToUTF16("John"));
  expected_bundle.profiles_to_update.push_back(&expected_profile);

  // Expect no sync events to add origin to the remote data.
  syncer::SyncChangeList expected_empty_change_list;

  MergeDataAndStartSyncing(std::move(profiles_from_web_db), data_list,
                           expected_bundle, expected_empty_change_list);
  autofill_syncable_service_.StopSyncing(syncer::AUTOFILL_PROFILE);
}

// Missing language code field should not generate sync events.
TEST_F(AutofillProfileSyncableServiceTest, NoLanguageCodeNoSync) {
  std::vector<std::unique_ptr<AutofillProfile>> profiles_from_web_db;

  // Local autofill profile has an empty language code.
  AutofillProfile profile(kGuid1, kEmptyOrigin);
  EXPECT_TRUE(profile.language_code().empty());
  profiles_from_web_db.push_back(std::make_unique<AutofillProfile>(profile));

  // Remote data does not have a language code value.
  sync_pb::EntitySpecifics specifics;
  sync_pb::AutofillProfileSpecifics* autofill_specifics =
      specifics.mutable_autofill_profile();
  autofill_specifics->set_guid(profile.guid());
  autofill_specifics->set_origin(profile.origin());
  autofill_specifics->add_name_first(std::string());
  autofill_specifics->add_name_middle(std::string());
  autofill_specifics->add_name_last(std::string());
  autofill_specifics->add_name_full(std::string());
  autofill_specifics->add_email_address(std::string());
  autofill_specifics->add_phone_home_whole_number(std::string());
  autofill_specifics->set_use_count(profile.use_count());
  autofill_specifics->set_use_date(profile.use_date().ToTimeT());
  EXPECT_FALSE(autofill_specifics->has_address_home_language_code());

  syncer::SyncDataList data_list;
  data_list.push_back(
      syncer::SyncData::CreateLocalData(
          profile.guid(), profile.guid(), specifics));

  // Expect no changes to local and remote data.
  MockAutofillProfileSyncableService::DataBundle expected_empty_bundle;
  syncer::SyncChangeList expected_empty_change_list;

  MergeDataAndStartSyncing(std::move(profiles_from_web_db), data_list,
                           expected_empty_bundle, expected_empty_change_list);
  autofill_syncable_service_.StopSyncing(syncer::AUTOFILL_PROFILE);
}

// Empty language code should be overwritten by sync.
TEST_F(AutofillProfileSyncableServiceTest, SyncUpdatesEmptyLanguageCode) {
  std::vector<std::unique_ptr<AutofillProfile>> profiles_from_web_db;

  // Local autofill profile has an empty language code.
  AutofillProfile profile(kGuid1, kEmptyOrigin);
  EXPECT_TRUE(profile.language_code().empty());
  profiles_from_web_db.push_back(std::make_unique<AutofillProfile>(profile));

  // Remote data has "en" language code.
  sync_pb::EntitySpecifics specifics;
  sync_pb::AutofillProfileSpecifics* autofill_specifics =
      specifics.mutable_autofill_profile();
  autofill_specifics->set_guid(profile.guid());
  autofill_specifics->set_origin(profile.origin());
  autofill_specifics->add_name_first(std::string());
  autofill_specifics->add_name_middle(std::string());
  autofill_specifics->add_name_last(std::string());
  autofill_specifics->add_name_full(std::string());
  autofill_specifics->add_email_address(std::string());
  autofill_specifics->add_phone_home_whole_number(std::string());
  autofill_specifics->set_address_home_language_code("en");
  EXPECT_TRUE(autofill_specifics->has_address_home_language_code());

  syncer::SyncDataList data_list;
  data_list.push_back(
      syncer::SyncData::CreateLocalData(
          profile.guid(), profile.guid(), specifics));

  // Expect the local autofill profile to have "en" language code after sync.
  MockAutofillProfileSyncableService::DataBundle expected_bundle;
  AutofillProfile expected_profile(kGuid1, kEmptyOrigin);
  expected_profile.set_language_code("en");
  expected_bundle.profiles_to_update.push_back(&expected_profile);

  // Expect no changes to remote data.
  syncer::SyncChangeList expected_empty_change_list;

  MergeDataAndStartSyncing(std::move(profiles_from_web_db), data_list,
                           expected_bundle, expected_empty_change_list);
  autofill_syncable_service_.StopSyncing(syncer::AUTOFILL_PROFILE);
}

// Incorrect language code should be overwritten by sync.
TEST_F(AutofillProfileSyncableServiceTest, SyncUpdatesIncorrectLanguageCode) {
  std::vector<std::unique_ptr<AutofillProfile>> profiles_from_web_db;

  // Local autofill profile has "de" language code.
  AutofillProfile profile(kGuid1, kEmptyOrigin);
  profile.set_language_code("de");
  profiles_from_web_db.push_back(std::make_unique<AutofillProfile>(profile));

  // Remote data has "en" language code.
  sync_pb::EntitySpecifics specifics;
  sync_pb::AutofillProfileSpecifics* autofill_specifics =
      specifics.mutable_autofill_profile();
  autofill_specifics->set_guid(profile.guid());
  autofill_specifics->set_origin(profile.origin());
  autofill_specifics->add_name_first(std::string());
  autofill_specifics->add_name_middle(std::string());
  autofill_specifics->add_name_last(std::string());
  autofill_specifics->add_name_full(std::string());
  autofill_specifics->add_email_address(std::string());
  autofill_specifics->add_phone_home_whole_number(std::string());
  autofill_specifics->set_address_home_language_code("en");
  EXPECT_TRUE(autofill_specifics->has_address_home_language_code());

  syncer::SyncDataList data_list;
  data_list.push_back(
      syncer::SyncData::CreateLocalData(
          profile.guid(), profile.guid(), specifics));

  // Expect the local autofill profile to have "en" language code after sync.
  MockAutofillProfileSyncableService::DataBundle expected_bundle;
  AutofillProfile expected_profile(kGuid1, kEmptyOrigin);
  expected_profile.set_language_code("en");
  expected_bundle.profiles_to_update.push_back(&expected_profile);

  // Expect no changes to remote data.
  syncer::SyncChangeList expected_empty_change_list;

  MergeDataAndStartSyncing(std::move(profiles_from_web_db), data_list,
                           expected_bundle, expected_empty_change_list);
  autofill_syncable_service_.StopSyncing(syncer::AUTOFILL_PROFILE);
}

// Sync data without language code should not overwrite existing language code
// in local autofill profile.
TEST_F(AutofillProfileSyncableServiceTest, EmptySyncPreservesLanguageCode) {
  std::vector<std::unique_ptr<AutofillProfile>> profiles_from_web_db;

  // Local autofill profile has "en" language code.
  AutofillProfile profile(kGuid1, kEmptyOrigin);
  profile.set_language_code("en");
  profiles_from_web_db.push_back(std::make_unique<AutofillProfile>(profile));

  // Remote data does not have a language code value.
  sync_pb::EntitySpecifics specifics;
  sync_pb::AutofillProfileSpecifics* autofill_specifics =
      specifics.mutable_autofill_profile();
  autofill_specifics->set_guid(profile.guid());
  autofill_specifics->set_origin(profile.origin());
  autofill_specifics->add_name_first("John");
  autofill_specifics->add_name_middle(std::string());
  autofill_specifics->add_name_last(std::string());
  autofill_specifics->add_name_full(std::string());
  autofill_specifics->add_email_address(std::string());
  autofill_specifics->add_phone_home_whole_number(std::string());
  EXPECT_FALSE(autofill_specifics->has_address_home_language_code());

  syncer::SyncDataList data_list;
  data_list.push_back(
      syncer::SyncData::CreateLocalData(
          profile.guid(), profile.guid(), specifics));

  // Expect local autofill profile to still have "en" language code after sync.
  MockAutofillProfileSyncableService::DataBundle expected_bundle;
  AutofillProfile expected_profile(profile.guid(), profile.origin());
  expected_profile.set_language_code("en");
  expected_profile.SetRawInfo(NAME_FIRST, ASCIIToUTF16("John"));
  expected_bundle.profiles_to_update.push_back(&expected_profile);

  // Expect no changes to remote data.
  syncer::SyncChangeList expected_empty_change_list;

  MergeDataAndStartSyncing(std::move(profiles_from_web_db), data_list,
                           expected_bundle, expected_empty_change_list);
  autofill_syncable_service_.StopSyncing(syncer::AUTOFILL_PROFILE);
}

// Language code in autofill profiles should be synced to the server.
TEST_F(AutofillProfileSyncableServiceTest, LanguageCodePropagates) {
  TestSyncChangeProcessor* sync_change_processor = new TestSyncChangeProcessor;
  autofill_syncable_service_.set_sync_processor(sync_change_processor);

  AutofillProfile profile(kGuid1, kEmptyOrigin);
  profile.set_language_code("en");
  AutofillProfileChange change(AutofillProfileChange::ADD, kGuid1, &profile);
  autofill_syncable_service_.AutofillProfileChanged(change);

  ASSERT_EQ(1U, sync_change_processor->changes().size());
  syncer::SyncChange result = sync_change_processor->changes()[0];
  EXPECT_EQ(syncer::SyncChange::ACTION_ADD, result.change_type());

  sync_pb::AutofillProfileSpecifics specifics =
      result.sync_data().GetSpecifics().autofill_profile();
  EXPECT_EQ(kGuid1, specifics.guid());
  EXPECT_EQ(kEmptyOrigin, specifics.origin());
  EXPECT_EQ("en", specifics.address_home_language_code());
}

// Missing validity state bitifield should not generate sync events.
TEST_F(AutofillProfileSyncableServiceTest, DefaultValidityStateNoSync) {
  std::vector<std::unique_ptr<AutofillProfile>> profiles_from_web_db;

  // Local autofill profile has a default validity state bitfield.
  AutofillProfile profile(kGuid1, kEmptyOrigin);
  EXPECT_EQ(0, profile.GetClientValidityBitfieldValue());
  profiles_from_web_db.push_back(std::make_unique<AutofillProfile>(profile));

  // Remote data does not have a validity state bitfield value.
  sync_pb::EntitySpecifics specifics;
  sync_pb::AutofillProfileSpecifics* autofill_specifics =
      specifics.mutable_autofill_profile();
  autofill_specifics->set_guid(profile.guid());
  autofill_specifics->set_origin(profile.origin());
  autofill_specifics->add_name_first(std::string());
  autofill_specifics->add_name_middle(std::string());
  autofill_specifics->add_name_last(std::string());
  autofill_specifics->add_name_full(std::string());
  autofill_specifics->add_email_address(std::string());
  autofill_specifics->add_phone_home_whole_number(std::string());
  autofill_specifics->set_use_count(profile.use_count());
  autofill_specifics->set_use_date(profile.use_date().ToTimeT());
  EXPECT_FALSE(autofill_specifics->has_validity_state_bitfield());

  syncer::SyncDataList data_list;
  data_list.push_back(syncer::SyncData::CreateLocalData(
      profile.guid(), profile.guid(), specifics));

  // Expect no changes to local and remote data.
  MockAutofillProfileSyncableService::DataBundle expected_empty_bundle;
  syncer::SyncChangeList expected_empty_change_list;

  MergeDataAndStartSyncing(std::move(profiles_from_web_db), data_list,
                           expected_empty_bundle, expected_empty_change_list);
  autofill_syncable_service_.StopSyncing(syncer::AUTOFILL_PROFILE);
}

// Default validity state bitfield should be overwritten by sync.
TEST_F(AutofillProfileSyncableServiceTest,
       SyncUpdatesDefaultValidityBitfieldAndFlag) {
  std::vector<std::unique_ptr<AutofillProfile>> profiles_from_web_db;

  // Local autofill profile has a default validity state.
  AutofillProfile profile(kGuid1, kEmptyOrigin);
  EXPECT_EQ(0, profile.GetClientValidityBitfieldValue());
  profiles_from_web_db.push_back(std::make_unique<AutofillProfile>(profile));

  // Remote data has a non default validity state bitfield value.
  sync_pb::EntitySpecifics specifics;
  sync_pb::AutofillProfileSpecifics* autofill_specifics =
      specifics.mutable_autofill_profile();
  autofill_specifics->set_guid(profile.guid());
  autofill_specifics->set_origin(profile.origin());
  autofill_specifics->add_name_first(std::string());
  autofill_specifics->add_name_middle(std::string());
  autofill_specifics->add_name_last(std::string());
  autofill_specifics->add_name_full(std::string());
  autofill_specifics->add_email_address(std::string());
  autofill_specifics->add_phone_home_whole_number(std::string());
  autofill_specifics->set_validity_state_bitfield(kValidityStateBitfield);
  autofill_specifics->set_is_client_validity_states_updated(true);
  EXPECT_TRUE(autofill_specifics->has_validity_state_bitfield());

  syncer::SyncDataList data_list;
  data_list.push_back(syncer::SyncData::CreateLocalData(
      profile.guid(), profile.guid(), specifics));

  // Expect the local autofill profile to have the non default validity state
  // bitfield after sync.
  MockAutofillProfileSyncableService::DataBundle expected_bundle;
  AutofillProfile expected_profile(kGuid1, kEmptyOrigin);
  expected_profile.SetClientValidityFromBitfieldValue(kValidityStateBitfield);
  expected_profile.set_is_client_validity_states_updated(true);
  expected_bundle.profiles_to_update.push_back(&expected_profile);

  // Expect no changes to remote data.
  syncer::SyncChangeList expected_empty_change_list;

  MergeDataAndStartSyncing(std::move(profiles_from_web_db), data_list,
                           expected_bundle, expected_empty_change_list);
  autofill_syncable_service_.StopSyncing(syncer::AUTOFILL_PROFILE);
}

// Local validity state bitfield should be overwritten by sync.
TEST_F(AutofillProfileSyncableServiceTest, SyncUpdatesLocalValidityBitfield) {
  std::vector<std::unique_ptr<AutofillProfile>> profiles_from_web_db;

  // Local autofill profile has a non default validity state bitfield value.
  AutofillProfile profile(kGuid1, kEmptyOrigin);
  profile.SetClientValidityFromBitfieldValue(kValidityStateBitfield + 1);
  profiles_from_web_db.push_back(std::make_unique<AutofillProfile>(profile));

  // Remote data has a different non default validity state bitfield value.
  sync_pb::EntitySpecifics specifics;
  sync_pb::AutofillProfileSpecifics* autofill_specifics =
      specifics.mutable_autofill_profile();
  autofill_specifics->set_guid(profile.guid());
  autofill_specifics->set_origin(profile.origin());
  autofill_specifics->add_name_first(std::string());
  autofill_specifics->add_name_middle(std::string());
  autofill_specifics->add_name_last(std::string());
  autofill_specifics->add_name_full(std::string());
  autofill_specifics->add_email_address(std::string());
  autofill_specifics->add_phone_home_whole_number(std::string());
  autofill_specifics->set_validity_state_bitfield(kValidityStateBitfield);
  EXPECT_TRUE(autofill_specifics->has_validity_state_bitfield());

  syncer::SyncDataList data_list;
  data_list.push_back(syncer::SyncData::CreateLocalData(
      profile.guid(), profile.guid(), specifics));

  // Expect the local autofill profile to have the remote validity state
  // bitfield value after sync.
  MockAutofillProfileSyncableService::DataBundle expected_bundle;
  AutofillProfile expected_profile(kGuid1, kEmptyOrigin);
  expected_profile.SetClientValidityFromBitfieldValue(kValidityStateBitfield);
  expected_bundle.profiles_to_update.push_back(&expected_profile);

  // Expect no changes to remote data.
  syncer::SyncChangeList expected_empty_change_list;

  MergeDataAndStartSyncing(std::move(profiles_from_web_db), data_list,
                           expected_bundle, expected_empty_change_list);
  autofill_syncable_service_.StopSyncing(syncer::AUTOFILL_PROFILE);
}

// Sync data without a default validity state bitfield should not overwrite
// an existing validity state bitfield in local autofill profile.
TEST_F(AutofillProfileSyncableServiceTest,
       DefaultSyncPreservesLocalValidityBitfield) {
  std::vector<std::unique_ptr<AutofillProfile>> profiles_from_web_db;

  // Local autofill profile has a non default validity state bitfield value.
  AutofillProfile profile(kGuid1, kEmptyOrigin);
  profile.SetClientValidityFromBitfieldValue(kValidityStateBitfield);
  profiles_from_web_db.push_back(std::make_unique<AutofillProfile>(profile));

  // Remote data does not has no validity state bitfield value.
  sync_pb::EntitySpecifics specifics;
  sync_pb::AutofillProfileSpecifics* autofill_specifics =
      specifics.mutable_autofill_profile();
  autofill_specifics->set_guid(profile.guid());
  autofill_specifics->set_origin(profile.origin());
  autofill_specifics->add_name_first("John");
  autofill_specifics->add_name_middle(std::string());
  autofill_specifics->add_name_last(std::string());
  autofill_specifics->add_name_full(std::string());
  autofill_specifics->add_email_address(std::string());
  autofill_specifics->add_phone_home_whole_number(std::string());
  EXPECT_FALSE(autofill_specifics->has_validity_state_bitfield());

  syncer::SyncDataList data_list;
  data_list.push_back(syncer::SyncData::CreateLocalData(
      profile.guid(), profile.guid(), specifics));

  // Expect local autofill profile to still have the kValidityStateBitfield
  // language code after sync.
  MockAutofillProfileSyncableService::DataBundle expected_bundle;
  AutofillProfile expected_profile(profile.guid(), profile.origin());
  expected_profile.SetClientValidityFromBitfieldValue(kValidityStateBitfield);
  expected_profile.SetRawInfo(NAME_FIRST, ASCIIToUTF16("John"));
  expected_bundle.profiles_to_update.push_back(&expected_profile);

  // Expect no changes to remote data.
  syncer::SyncChangeList expected_empty_change_list;

  MergeDataAndStartSyncing(std::move(profiles_from_web_db), data_list,
                           expected_bundle, expected_empty_change_list);
  autofill_syncable_service_.StopSyncing(syncer::AUTOFILL_PROFILE);
}

// Validity state bitfield in autofill profiles should be synced to the server.
TEST_F(AutofillProfileSyncableServiceTest, LocalValidityBitfieldPropagates) {
  TestSyncChangeProcessor* sync_change_processor = new TestSyncChangeProcessor;
  autofill_syncable_service_.set_sync_processor(sync_change_processor);

  AutofillProfile profile(kGuid1, kEmptyOrigin);
  profile.SetClientValidityFromBitfieldValue(kValidityStateBitfield);
  AutofillProfileChange change(AutofillProfileChange::ADD, kGuid1, &profile);
  autofill_syncable_service_.AutofillProfileChanged(change);

  ASSERT_EQ(1U, sync_change_processor->changes().size());
  syncer::SyncChange result = sync_change_processor->changes()[0];
  EXPECT_EQ(syncer::SyncChange::ACTION_ADD, result.change_type());

  sync_pb::AutofillProfileSpecifics specifics =
      result.sync_data().GetSpecifics().autofill_profile();
  EXPECT_EQ(kGuid1, specifics.guid());
  EXPECT_EQ(kEmptyOrigin, specifics.origin());
  EXPECT_EQ(kValidityStateBitfield, specifics.validity_state_bitfield());
}

// Missing full name field should not generate sync events.
TEST_F(AutofillProfileSyncableServiceTest, NoFullNameNoSync) {
  std::vector<std::unique_ptr<AutofillProfile>> profiles_from_web_db;

  // Local autofill profile has an empty full name.
  AutofillProfile profile(kGuid1, kEmptyOrigin);
  profile.SetRawInfo(NAME_FIRST, ASCIIToUTF16("John"));
  profiles_from_web_db.push_back(std::make_unique<AutofillProfile>(profile));

  // Remote data does not have a full name.
  sync_pb::EntitySpecifics specifics;
  sync_pb::AutofillProfileSpecifics* autofill_specifics =
      specifics.mutable_autofill_profile();
  autofill_specifics->set_guid(profile.guid());
  autofill_specifics->set_origin(profile.origin());
  autofill_specifics->add_name_first(std::string("John"));
  autofill_specifics->add_name_middle(std::string());
  autofill_specifics->add_name_last(std::string());
  autofill_specifics->add_email_address(std::string());
  autofill_specifics->set_use_count(profile.use_count());
  autofill_specifics->set_use_date(profile.use_date().ToTimeT());
  autofill_specifics->add_phone_home_whole_number(std::string());

  syncer::SyncDataList data_list;
  data_list.push_back(
      syncer::SyncData::CreateLocalData(
          profile.guid(), profile.guid(), specifics));

  // Expect no changes to local and remote data.
  MockAutofillProfileSyncableService::DataBundle expected_empty_bundle;
  syncer::SyncChangeList expected_empty_change_list;

  MergeDataAndStartSyncing(std::move(profiles_from_web_db), data_list,
                           expected_empty_bundle, expected_empty_change_list);
  autofill_syncable_service_.StopSyncing(syncer::AUTOFILL_PROFILE);
}

TEST_F(AutofillProfileSyncableServiceTest, EmptySyncPreservesFullName) {
  std::vector<std::unique_ptr<AutofillProfile>> profiles_from_web_db;

  // Local autofill profile has a full name.
  AutofillProfile profile(kGuid1, kEmptyOrigin);
  profile.SetRawInfo(NAME_FULL, ASCIIToUTF16("John Jacob Smith, Jr"));
  profiles_from_web_db.push_back(std::make_unique<AutofillProfile>(profile));

  // Remote data does not have a full name value.
  sync_pb::EntitySpecifics specifics;
  sync_pb::AutofillProfileSpecifics* autofill_specifics =
      specifics.mutable_autofill_profile();
  autofill_specifics->set_guid(profile.guid());
  autofill_specifics->set_origin(profile.origin());
  autofill_specifics->add_name_first(std::string("John"));
  autofill_specifics->add_name_middle(std::string("Jacob"));
  autofill_specifics->add_name_last(std::string("Smith"));

  syncer::SyncDataList data_list;
  data_list.push_back(
      syncer::SyncData::CreateLocalData(
          profile.guid(), profile.guid(), specifics));

  // Expect local autofill profile to still have the same full name after sync.
  MockAutofillProfileSyncableService::DataBundle expected_bundle;
  AutofillProfile expected_profile(profile.guid(), profile.origin());
  expected_profile.SetInfo(AutofillType(NAME_FULL),
                           ASCIIToUTF16("John Jacob Smith, Jr"),
                           "en-US");
  expected_bundle.profiles_to_update.push_back(&expected_profile);

  // Expect no changes to remote data.
  syncer::SyncChangeList expected_empty_change_list;

  MergeDataAndStartSyncing(std::move(profiles_from_web_db), data_list,
                           expected_bundle, expected_empty_change_list);
  autofill_syncable_service_.StopSyncing(syncer::AUTOFILL_PROFILE);
}

// Missing use_count/use_date fields should not generate sync events.
TEST_F(AutofillProfileSyncableServiceTest, NoUsageStatsNoSync) {
  std::vector<std::unique_ptr<AutofillProfile>> profiles_from_web_db;

  // Local autofill profile has 0 for use_count/use_date.
  AutofillProfile profile(kGuid1, kEmptyOrigin);
  profile.set_language_code("en");
  profile.set_use_count(0);
  profile.set_use_date(base::Time());
  EXPECT_EQ(0U, profile.use_count());
  EXPECT_EQ(base::Time(), profile.use_date());
  profiles_from_web_db.push_back(std::make_unique<AutofillProfile>(profile));

  // Remote data does not have use_count/use_date.
  sync_pb::EntitySpecifics specifics;
  sync_pb::AutofillProfileSpecifics* autofill_specifics =
      specifics.mutable_autofill_profile();
  autofill_specifics->set_guid(profile.guid());
  autofill_specifics->set_origin(profile.origin());
  autofill_specifics->add_name_first(std::string());
  autofill_specifics->add_name_middle(std::string());
  autofill_specifics->add_name_last(std::string());
  autofill_specifics->add_name_full(std::string());
  autofill_specifics->add_email_address(std::string());
  autofill_specifics->add_phone_home_whole_number(std::string());
  autofill_specifics->set_address_home_language_code("en");
  EXPECT_FALSE(autofill_specifics->has_use_count());
  EXPECT_FALSE(autofill_specifics->has_use_date());

  syncer::SyncDataList data_list;
  data_list.push_back(
      syncer::SyncData::CreateLocalData(
          profile.guid(), profile.guid(), specifics));

  // Expect no changes to local and remote data.
  MockAutofillProfileSyncableService::DataBundle expected_empty_bundle;
  syncer::SyncChangeList expected_empty_change_list;

  MergeDataAndStartSyncing(std::move(profiles_from_web_db), data_list,
                           expected_empty_bundle, expected_empty_change_list);
  autofill_syncable_service_.StopSyncing(syncer::AUTOFILL_PROFILE);
}

struct SyncUpdatesUsageStatsTestCase {
  size_t local_use_count;
  base::Time local_use_date;
  size_t remote_use_count;
  int remote_use_date;
  size_t synced_use_count;
  base::Time synced_use_date;
};

class SyncUpdatesUsageStatsTest
    : public testing::TestWithParam<SyncUpdatesUsageStatsTestCase> {
 public:
  SyncUpdatesUsageStatsTest() { CountryNames::SetLocaleString("en-US"); }

  void SetUp() override { sync_processor_.reset(new MockSyncChangeProcessor); }

  // Wrapper around AutofillProfileSyncableService::MergeDataAndStartSyncing()
  // that also verifies expectations.
  void MergeDataAndStartSyncing(
      std::vector<std::unique_ptr<AutofillProfile>> profiles_from_web_db,
      const syncer::SyncDataList& data_list,
      const MockAutofillProfileSyncableService::DataBundle& expected_bundle,
      const syncer::SyncChangeList& expected_change_list) {
    auto profile_returner = [&profiles_from_web_db]() {
      return std::move(profiles_from_web_db);
    };
    EXPECT_CALL(autofill_syncable_service_, LoadAutofillData(_))
        .Times(1)
        .WillOnce(DoAll(LoadAutofillProfiles(profile_returner), Return(true)));
    EXPECT_CALL(autofill_syncable_service_,
                SaveChangesToWebData(DataBundleCheck(expected_bundle)))
        .Times(1)
        .WillOnce(Return(true));
    if (expected_change_list.empty()) {
      EXPECT_CALL(*sync_processor_, ProcessSyncChanges(_, _)).Times(0);
    } else {
      ON_CALL(*sync_processor_, ProcessSyncChanges(_, _))
          .WillByDefault(Return(syncer::SyncError()));
      EXPECT_CALL(*sync_processor_,
                  ProcessSyncChanges(_, CheckSyncChanges(expected_change_list)))
          .Times(1)
          .WillOnce(Return(syncer::SyncError()));
    }

    // Takes ownership of sync_processor_.
    autofill_syncable_service_.MergeDataAndStartSyncing(
        syncer::AUTOFILL_PROFILE, data_list, std::move(sync_processor_),
        std::unique_ptr<syncer::SyncErrorFactory>(
            new syncer::SyncErrorFactoryMock()));
  }

 protected:
  base::MessageLoop message_loop_;
  MockAutofillProfileSyncableService autofill_syncable_service_;
  std::unique_ptr<MockSyncChangeProcessor> sync_processor_;
};

TEST_P(SyncUpdatesUsageStatsTest, SyncUpdatesUsageStats) {
  auto test_case = GetParam();
  SetUp();
  std::vector<std::unique_ptr<AutofillProfile>> profiles_from_web_db;

  AutofillProfile profile(kGuid1, kEmptyOrigin);
  profile.set_language_code("en");
  profile.set_use_count(test_case.local_use_count);
  profile.set_use_date(test_case.local_use_date);
  EXPECT_EQ(test_case.local_use_count, profile.use_count());
  EXPECT_EQ(test_case.local_use_date, profile.use_date());
  profiles_from_web_db.push_back(std::make_unique<AutofillProfile>(profile));

  // Remote data has usage stats.
  sync_pb::EntitySpecifics specifics;
  sync_pb::AutofillProfileSpecifics* autofill_specifics =
      specifics.mutable_autofill_profile();
  autofill_specifics->set_guid(profile.guid());
  autofill_specifics->set_origin(profile.origin());
  autofill_specifics->add_name_first(std::string());
  autofill_specifics->add_name_middle(std::string());
  autofill_specifics->add_name_last(std::string());
  autofill_specifics->add_name_full(std::string());
  autofill_specifics->add_email_address(std::string());
  autofill_specifics->add_phone_home_whole_number(std::string());
  autofill_specifics->set_address_home_language_code("en");
  autofill_specifics->set_use_count(test_case.remote_use_count);
  autofill_specifics->set_use_date(test_case.remote_use_date);
  EXPECT_TRUE(autofill_specifics->has_use_count());
  EXPECT_TRUE(autofill_specifics->has_use_date());

  syncer::SyncDataList data_list;
  data_list.push_back(syncer::SyncData::CreateLocalData(
      profile.guid(), profile.guid(), specifics));

  // Expect the local autofill profile to have usage stats after sync.
  MockAutofillProfileSyncableService::DataBundle expected_bundle;
  AutofillProfile expected_profile = profile;
  expected_profile.set_use_count(test_case.synced_use_count);
  expected_profile.set_use_date(test_case.synced_use_date);
  expected_bundle.profiles_to_update.push_back(&expected_profile);

  // Expect no changes to remote data.
  syncer::SyncChangeList expected_empty_change_list;

  MergeDataAndStartSyncing(std::move(profiles_from_web_db), data_list,
                           expected_bundle, expected_empty_change_list);
  autofill_syncable_service_.StopSyncing(syncer::AUTOFILL_PROFILE);
}

INSTANTIATE_TEST_CASE_P(
    AutofillProfileSyncableServiceTest,
    SyncUpdatesUsageStatsTest,
    testing::Values(
        // Local profile with default stats.
        SyncUpdatesUsageStatsTestCase{0U, base::Time(), 9U, 4321, 9U,
                                      base::Time::FromTimeT(4321)},
        // Local profile has older stats than the server.
        SyncUpdatesUsageStatsTestCase{3U, base::Time::FromTimeT(1234), 9U, 4321,
                                      9U, base::Time::FromTimeT(4321)},
        // Local profile has newer stats than the server
        SyncUpdatesUsageStatsTestCase{10U, base::Time::FromTimeT(9999), 9U,
                                      4321, 9U, base::Time::FromTimeT(4321)}));

// Usage stats should be updated by the client.
TEST_F(AutofillProfileSyncableServiceTest, ClientOverwritesUsageStats) {
  TestSyncChangeProcessor* sync_change_processor = new TestSyncChangeProcessor;

  // Remote data has a profile with usage stats.
  sync_pb::EntitySpecifics specifics;
  sync_pb::AutofillProfileSpecifics* autofill_specifics =
      specifics.mutable_autofill_profile();
  autofill_specifics->set_guid(kGuid1);
  autofill_specifics->set_origin(kEmptyOrigin);
  autofill_specifics->add_name_first(std::string());
  autofill_specifics->add_name_middle(std::string());
  autofill_specifics->add_name_last(std::string());
  autofill_specifics->add_name_full(std::string());
  autofill_specifics->add_email_address(std::string());
  autofill_specifics->add_phone_home_whole_number(std::string());
  autofill_specifics->set_address_home_language_code("en");
  autofill_specifics->set_use_count(9);
  autofill_specifics->set_use_date(25);

  syncer::SyncDataList data_list;
  data_list.push_back(
      syncer::SyncData::CreateLocalData(kGuid1, kEmptyOrigin, specifics));

  EXPECT_CALL(autofill_syncable_service_, LoadAutofillData(_))
      .Times(1)
      .WillOnce(Return(true));
  EXPECT_CALL(autofill_syncable_service_,
              SaveChangesToWebData(_))
      .Times(1)
      .WillOnce(Return(true));
  autofill_syncable_service_.MergeDataAndStartSyncing(
      syncer::AUTOFILL_PROFILE, data_list,
      base::WrapUnique(sync_change_processor),
      std::unique_ptr<syncer::SyncErrorFactory>(
          new syncer::SyncErrorFactoryMock()));

  // Update to the usage stats for that profile.
  AutofillProfile profile(kGuid1, kEmptyOrigin);
  profile.set_language_code("en");
  profile.set_use_count(10U);
  profile.set_use_date(base::Time::FromTimeT(30));
  AutofillProfileChange change(AutofillProfileChange::UPDATE, kGuid1, &profile);
  autofill_syncable_service_.AutofillProfileChanged(change);
  std::vector<AutofillProfile*> profiles;
  profiles.push_back(&profile);

  ASSERT_EQ(1U, sync_change_processor->changes().size());
  syncer::SyncChange result = sync_change_processor->changes()[0];
  EXPECT_EQ(syncer::SyncChange::ACTION_UPDATE, result.change_type());

  sync_pb::AutofillProfileSpecifics result_specifics =
      result.sync_data().GetSpecifics().autofill_profile();
  EXPECT_EQ(10U, result_specifics.use_count());
  EXPECT_EQ(30, result_specifics.use_date());

  autofill_syncable_service_.StopSyncing(syncer::AUTOFILL_PROFILE);
}

// Server profile updates should be ignored.
TEST_F(AutofillProfileSyncableServiceTest, IgnoreServerProfileUpdate) {
  EXPECT_CALL(autofill_syncable_service_, LoadAutofillData(_))
      .Times(1)
      .WillOnce(Return(true));
  EXPECT_CALL(autofill_syncable_service_, SaveChangesToWebData(_))
      .Times(1)
      .WillOnce(Return(true));
  autofill_syncable_service_.MergeDataAndStartSyncing(
      syncer::AUTOFILL_PROFILE, syncer::SyncDataList(),
      base::WrapUnique(new TestSyncChangeProcessor),
      std::unique_ptr<syncer::SyncErrorFactory>(
          new syncer::SyncErrorFactoryMock()));
  AutofillProfile server_profile(AutofillProfile::SERVER_PROFILE, "server-id");

  // Should not crash:
  autofill_syncable_service_.AutofillProfileChanged(AutofillProfileChange(
      AutofillProfileChange::UPDATE, server_profile.guid(), &server_profile));
}

// Tests that a non-settings origin from the server is never set to the local
// profile.
TEST_F(AutofillProfileSyncableServiceTest,
       OverwriteProfileWithServerData_NonSettingsOrigin) {
  // Create a profile with an empty origin.
  AutofillProfile profile(kGuid1, std::string());
  profile.SetRawInfo(NAME_FIRST, ASCIIToUTF16("John"));
  profile.SetRawInfo(ADDRESS_HOME_LINE1, ASCIIToUTF16("1 1st st"));

  // Create a Sync profile with a non-settings origin.
  sync_pb::EntitySpecifics specifics;
  sync_pb::AutofillProfileSpecifics* autofill_specifics =
      specifics.mutable_autofill_profile();
  autofill_specifics->set_guid(profile.guid());
  autofill_specifics->set_origin("https://www.example.com");
  autofill_specifics->add_name_first("John");
  autofill_specifics->add_name_middle(std::string());
  autofill_specifics->add_name_last(std::string());
  autofill_specifics->add_name_full(std::string());
  autofill_specifics->add_email_address(std::string());
  autofill_specifics->add_phone_home_whole_number(std::string());
  autofill_specifics->set_address_home_line1("1 1st st");
  autofill_specifics->set_use_count(profile.use_count());
  autofill_specifics->set_use_date(profile.use_date().ToTimeT());

  // Expect that the empty origin is not overwritten.
  autofill_syncable_service_.OverwriteProfileWithServerData(*autofill_specifics,
                                                            &profile);
  EXPECT_TRUE(profile.origin().empty());

  // Set the local origin to settings.
  profile.set_origin(kSettingsOrigin);

  // Expect that the settings origin is not overwritten.
  autofill_syncable_service_.OverwriteProfileWithServerData(*autofill_specifics,
                                                            &profile);
  EXPECT_EQ(kSettingsOrigin, profile.origin());
}

// Tests that a non-settings origin from the server is not set to the local
// profile.
TEST_F(AutofillProfileSyncableServiceTest,
       OverwriteProfileWithServerData_SettingsOrigin) {
  // Create a profile with an empty origin.
  AutofillProfile profile(kGuid1, std::string());
  profile.SetRawInfo(NAME_FIRST, ASCIIToUTF16("John"));
  profile.SetRawInfo(ADDRESS_HOME_LINE1, ASCIIToUTF16("1 1st st"));

  // Create a Sync profile with a non-settings origin.
  sync_pb::EntitySpecifics specifics;
  sync_pb::AutofillProfileSpecifics* autofill_specifics =
      specifics.mutable_autofill_profile();
  autofill_specifics->set_guid(profile.guid());
  autofill_specifics->set_origin(kSettingsOrigin);
  autofill_specifics->add_name_first("John");
  autofill_specifics->add_name_middle(std::string());
  autofill_specifics->add_name_last(std::string());
  autofill_specifics->add_name_full(std::string());
  autofill_specifics->add_email_address(std::string());
  autofill_specifics->add_phone_home_whole_number(std::string());
  autofill_specifics->set_address_home_line1("1 1st st");
  autofill_specifics->set_use_count(profile.use_count());
  autofill_specifics->set_use_date(profile.use_date().ToTimeT());

  // Expect that the settings origin replaced the empty origin.
  autofill_syncable_service_.OverwriteProfileWithServerData(*autofill_specifics,
                                                            &profile);
  EXPECT_EQ(kSettingsOrigin, profile.origin());
}

}  // namespace autofill
