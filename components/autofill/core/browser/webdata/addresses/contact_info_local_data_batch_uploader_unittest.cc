// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/autofill/core/browser/webdata/addresses/contact_info_local_data_batch_uploader.h"

#include "base/test/bind.h"
#include "base/test/test_future.h"
#include "components/autofill/core/browser/autofill_test_utils.h"
#include "components/autofill/core/browser/data_model/autofill_profile.h"
#include "components/autofill/core/browser/data_model/autofill_profile_test_api.h"
#include "components/autofill/core/browser/profile_requirement_utils.h"
#include "components/autofill/core/browser/test_address_data_manager.h"
#include "components/sync/service/local_data_description.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace autofill {

namespace {

using ::testing::ElementsAre;
using ::testing::IsEmpty;
using ::testing::Pointee;
using ::testing::UnorderedElementsAre;

// Compares syncer::LocalDataItemModel id with the input
// AutofillProfile::guid().
MATCHER_P(MatchesModelId, guid, "") {
  return std::get<std::string>(arg.id) == guid;
}

// Compares the profile content, ignoring additional information like guid.
MATCHER_P(MatchesProfileContent, expected, "") {
  return arg->Compare(expected) == 0;
}

class ContactInfoLocalDataBatchUploaderTest : public testing::Test {
 public:
  ContactInfoLocalDataBatchUploaderTest() {
    address_data_manager().SetIsEligibleForAddressAccountStorage(true);
  }

  ContactInfoLocalDataBatchUploader& uploader() { return uploader_; }

  TestAddressDataManager& address_data_manager() {
    return address_data_manager_;
  }

  // TODO(crbug.com/373568992): Mobile information are not yet filled for
  // autofill. When combining the data, these tests should make sure that the
  // information is aligned between the platforms.
  bool HasDataForMobileSet(syncer::LocalDataDescription description) const {
    // Those info are only used on Mobile.
    return description.item_count != 0u || description.domain_count != 0u ||
           !description.domains.empty();
  }

 private:
  TestAddressDataManager address_data_manager_;
  ContactInfoLocalDataBatchUploader uploader_{base::BindLambdaForTesting(
      [this]() -> AddressDataManager* { return &address_data_manager_; })};
};

TEST_F(ContactInfoLocalDataBatchUploaderTest,
       EmptyReturnsLocalDataDescription) {
  base::test::TestFuture<syncer::LocalDataDescription> description;
  uploader().GetLocalDataDescription(description.GetCallback());

  EXPECT_FALSE(HasDataForMobileSet(description.Get()));
  EXPECT_THAT(description.Get().local_data_models, IsEmpty());
}

TEST_F(ContactInfoLocalDataBatchUploaderTest,
       LocalProfilesWithOrderByFrequency) {
  // These profiles are local profiles by default.
  AutofillProfile first_profile = test::GetFullProfile();
  AutofillProfile second_profile = test::GetFullProfile2();
  // Set the `first_profile` use_count greater than `second_profile` use_count.
  first_profile.set_use_count(20);
  second_profile.set_use_count(10);
  address_data_manager().AddProfile(first_profile);
  address_data_manager().AddProfile(second_profile);

  base::test::TestFuture<syncer::LocalDataDescription> description;
  uploader().GetLocalDataDescription(description.GetCallback());

  EXPECT_FALSE(HasDataForMobileSet(description.Get()));
  // Order should match the use count, so `first_profile` first.
  EXPECT_THAT(description.Get().local_data_models,
              ElementsAre(MatchesModelId(first_profile.guid()),
                          MatchesModelId(second_profile.guid())));
}

TEST_F(ContactInfoLocalDataBatchUploaderTest,
       LocalProfilesNotRetrievedIfUserNotEligible) {
  // These profiles are local profiles by default.
  address_data_manager().AddProfile(test::GetFullProfile());
  address_data_manager().AddProfile(test::GetFullProfile2());

  address_data_manager().SetIsEligibleForAddressAccountStorage(false);

  base::test::TestFuture<syncer::LocalDataDescription> description;
  uploader().GetLocalDataDescription(description.GetCallback());

  EXPECT_FALSE(HasDataForMobileSet(description.Get()));
  EXPECT_THAT(description.Get().local_data_models, IsEmpty());
}

TEST_F(ContactInfoLocalDataBatchUploaderTest, LocalProfilesOnly) {
  // These profiles are local profiles by default.
  AutofillProfile first_profile = test::GetFullProfile();
  AutofillProfile second_profile = test::GetFullProfile2();
  // Make `first_profile` an account profile.
  test_api(first_profile)
      .set_record_type(AutofillProfile::RecordType::kAccount);
  address_data_manager().AddProfile(first_profile);
  address_data_manager().AddProfile(second_profile);

  base::test::TestFuture<syncer::LocalDataDescription> description;
  uploader().GetLocalDataDescription(description.GetCallback());

  EXPECT_FALSE(HasDataForMobileSet(description.Get()));
  // Only `second_profile` is local and should be retrieved.
  EXPECT_THAT(description.Get().local_data_models,
              ElementsAre(MatchesModelId(second_profile.guid())));
}

TEST_F(ContactInfoLocalDataBatchUploaderTest, LocalCompleteProfilesOnly) {
  // These profiles are local profiles by default.
  AutofillProfile complete_profile = test::GetFullProfile();
  // This profile does not meet the minimum requirement to be retrieved.
  AutofillProfile incomplete_profile = test::GetIncompleteProfile2();
  ASSERT_FALSE(IsMinimumAddress(incomplete_profile));
  address_data_manager().AddProfile(complete_profile);
  address_data_manager().AddProfile(incomplete_profile);

  base::test::TestFuture<syncer::LocalDataDescription> description;
  uploader().GetLocalDataDescription(description.GetCallback());

  EXPECT_FALSE(HasDataForMobileSet(description.Get()));
  // Only `complete_profile` should be retrieved.
  EXPECT_THAT(description.Get().local_data_models,
              ElementsAre(MatchesModelId(complete_profile.guid())));
}

TEST_F(ContactInfoLocalDataBatchUploaderTest,
       LocalWithEligibleCountryProfilesOnly) {
  // These profiles are local profiles by default.
  AutofillProfile eligible_profile = test::GetFullProfile();
  AddressCountryCode ineligible_country_code("IR");
  ASSERT_FALSE(address_data_manager().IsCountryEligibleForAccountStorage(
      ineligible_country_code.value()));
  // This profile does not meet the minimum requirement to be retrieved.
  AutofillProfile ineligible_profile =
      test::GetFullProfile2(ineligible_country_code);
  address_data_manager().AddProfile(eligible_profile);
  address_data_manager().AddProfile(ineligible_profile);

  base::test::TestFuture<syncer::LocalDataDescription> description;
  uploader().GetLocalDataDescription(description.GetCallback());

  EXPECT_FALSE(HasDataForMobileSet(description.Get()));
  // Only `eligible_profile` should be retrieved.
  EXPECT_THAT(description.Get().local_data_models,
              ElementsAre(MatchesModelId(eligible_profile.guid())));
}

TEST_F(ContactInfoLocalDataBatchUploaderTest, MigrateAllLocalProfiles) {
  // These profiles are local profiles by default.
  AutofillProfile first_profile = test::GetFullProfile();
  AutofillProfile second_profile = test::GetFullProfile2();
  address_data_manager().AddProfile(first_profile);
  address_data_manager().AddProfile(second_profile);

  ASSERT_THAT(
      address_data_manager().GetProfilesByRecordType(
          AutofillProfile::RecordType::kLocalOrSyncable),
      UnorderedElementsAre(Pointee(second_profile), Pointee(first_profile)));
  ASSERT_THAT(address_data_manager().GetProfilesByRecordType(
                  AutofillProfile::RecordType::kAccount),
              IsEmpty());

  uploader().TriggerLocalDataMigration();

  // All profiles migrated.
  EXPECT_THAT(address_data_manager().GetProfilesByRecordType(
                  AutofillProfile::RecordType::kLocalOrSyncable),
              IsEmpty());
  EXPECT_THAT(address_data_manager().GetProfilesByRecordType(
                  AutofillProfile::RecordType::kAccount),
              UnorderedElementsAre(MatchesProfileContent(second_profile),
                                   MatchesProfileContent(first_profile)));
}

TEST_F(ContactInfoLocalDataBatchUploaderTest, MigrateWithProfilesIds) {
  // These profiles are local profiles by default.
  AutofillProfile first_profile = test::GetFullProfile();
  AutofillProfile second_profile = test::GetFullProfile2();
  AutofillProfile third_profile = test::GetFullCanadianProfile();
  address_data_manager().AddProfile(first_profile);
  address_data_manager().AddProfile(second_profile);
  address_data_manager().AddProfile(third_profile);

  ASSERT_THAT(
      address_data_manager().GetProfilesByRecordType(
          AutofillProfile::RecordType::kLocalOrSyncable),
      UnorderedElementsAre(Pointee(second_profile), Pointee(first_profile),
                           Pointee(third_profile)));
  ASSERT_THAT(address_data_manager().GetProfilesByRecordType(
                  AutofillProfile::RecordType::kAccount),
              IsEmpty());

  // Request migration for 2 of the 3 profiles.
  uploader().TriggerLocalDataMigration(
      {first_profile.guid(), second_profile.guid()});

  // Third profile remains as a local profile.
  EXPECT_THAT(address_data_manager().GetProfilesByRecordType(
                  AutofillProfile::RecordType::kLocalOrSyncable),
              ElementsAre(MatchesProfileContent(third_profile)));
  EXPECT_THAT(address_data_manager().GetProfilesByRecordType(
                  AutofillProfile::RecordType::kAccount),
              UnorderedElementsAre(MatchesProfileContent(second_profile),
                                   MatchesProfileContent(first_profile)));
}

TEST_F(ContactInfoLocalDataBatchUploaderTest,
       MigrateWithProfilesIdsWithIncompleteProfile) {
  // These profiles are local profiles by default.
  AutofillProfile complete_profile = test::GetFullProfile();
  AutofillProfile incomplete_profile = test::GetIncompleteProfile2();
  ASSERT_FALSE(IsMinimumAddress(incomplete_profile));
  address_data_manager().AddProfile(complete_profile);
  address_data_manager().AddProfile(incomplete_profile);

  ASSERT_THAT(address_data_manager().GetProfilesByRecordType(
                  AutofillProfile::RecordType::kLocalOrSyncable),
              UnorderedElementsAre(Pointee(incomplete_profile),
                                   Pointee(complete_profile)));
  ASSERT_THAT(address_data_manager().GetProfilesByRecordType(
                  AutofillProfile::RecordType::kAccount),
              IsEmpty());

  uploader().TriggerLocalDataMigration(
      {complete_profile.guid(), incomplete_profile.guid()});

  // Only `complete_profile` migrated, `incomplete_profile` remains.
  EXPECT_THAT(address_data_manager().GetProfilesByRecordType(
                  AutofillProfile::RecordType::kLocalOrSyncable),
              ElementsAre(MatchesProfileContent(incomplete_profile)));
  EXPECT_THAT(address_data_manager().GetProfilesByRecordType(
                  AutofillProfile::RecordType::kAccount),
              ElementsAre(MatchesProfileContent(complete_profile)));
}

TEST_F(ContactInfoLocalDataBatchUploaderTest,
       MigrateWithProfilesIdsWithIneligibleCountryProfile) {
  // These profiles are local profiles by default.
  AutofillProfile eligible_profile = test::GetFullProfile();
  AddressCountryCode ineligible_country_code("IR");
  ASSERT_FALSE(address_data_manager().IsCountryEligibleForAccountStorage(
      ineligible_country_code.value()));
  // This profile does not meet the minimum requirement to be retrieved.
  AutofillProfile ineligible_profile =
      test::GetFullProfile2(ineligible_country_code);
  address_data_manager().AddProfile(eligible_profile);
  address_data_manager().AddProfile(ineligible_profile);

  ASSERT_THAT(address_data_manager().GetProfilesByRecordType(
                  AutofillProfile::RecordType::kLocalOrSyncable),
              UnorderedElementsAre(Pointee(ineligible_profile),
                                   Pointee(eligible_profile)));
  ASSERT_THAT(address_data_manager().GetProfilesByRecordType(
                  AutofillProfile::RecordType::kAccount),
              IsEmpty());

  uploader().TriggerLocalDataMigration(
      {eligible_profile.guid(), ineligible_profile.guid()});

  // Only `eligible_profile` migrated, `ineligible_profile` remains.
  EXPECT_THAT(address_data_manager().GetProfilesByRecordType(
                  AutofillProfile::RecordType::kLocalOrSyncable),
              ElementsAre(MatchesProfileContent(ineligible_profile)));
  EXPECT_THAT(address_data_manager().GetProfilesByRecordType(
                  AutofillProfile::RecordType::kAccount),
              ElementsAre(MatchesProfileContent(eligible_profile)));
}

TEST_F(ContactInfoLocalDataBatchUploaderTest,
       MigrateWithProfilesIdsWithNonExistingIds) {
  // These profiles are local profiles by default.
  AutofillProfile first_profile = test::GetFullProfile();
  AutofillProfile second_profile = test::GetFullProfile2();
  address_data_manager().AddProfile(first_profile);
  address_data_manager().AddProfile(second_profile);

  ASSERT_THAT(
      address_data_manager().GetProfilesByRecordType(
          AutofillProfile::RecordType::kLocalOrSyncable),
      UnorderedElementsAre(Pointee(second_profile), Pointee(first_profile)));
  ASSERT_THAT(address_data_manager().GetProfilesByRecordType(
                  AutofillProfile::RecordType::kAccount),
              IsEmpty());

  // Request migration for `first_profile` and some fake_id.
  uploader().TriggerLocalDataMigration(
      {first_profile.guid(), std::string("fake_id")});

  // Only `first_profile` migrated.
  EXPECT_THAT(address_data_manager().GetProfilesByRecordType(
                  AutofillProfile::RecordType::kLocalOrSyncable),
              ElementsAre(MatchesProfileContent(second_profile)));
  EXPECT_THAT(address_data_manager().GetProfilesByRecordType(
                  AutofillProfile::RecordType::kAccount),
              ElementsAre(MatchesProfileContent(first_profile)));
}

}  // namespace

}  // namespace autofill
