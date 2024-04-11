// Copyright 2013 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/autofill/core/browser/personal_data_manager.h"

#include <memory>
#include <string>
#include <utility>

#include "base/strings/utf_string_conversions.h"
#include "base/test/mock_callback.h"
#include "components/autofill/core/browser/autofill_test_utils.h"
#include "components/autofill/core/browser/data_model/autofill_profile.h"
#include "components/autofill/core/browser/personal_data_manager_test_base.h"
#include "components/autofill/core/common/autofill_prefs.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace autofill {

class PersonalDataManagerTest : public PersonalDataManagerTestBase,
                                public testing::Test {
 protected:
  PersonalDataManagerTest() = default;

  ~PersonalDataManagerTest() override {
    if (personal_data_)
      personal_data_->Shutdown();
    personal_data_.reset();
  }

  void SetUp() override {
    SetUpTest();
    ResetPersonalDataManager();
  }
  void TearDown() override { TearDownTest(); }

  void ResetPersonalDataManager(bool use_sync_transport_mode = false) {
    if (personal_data_)
      personal_data_->Shutdown();
    personal_data_ = std::make_unique<PersonalDataManager>("EN", "US");
    PersonalDataManagerTestBase::ResetPersonalDataManager(
        use_sync_transport_mode, personal_data_.get());
  }

  void AddProfileToPersonalDataManager(const AutofillProfile& profile) {
    PersonalDataChangedWaiter waiter(*personal_data_);
    personal_data_->AddProfile(profile);
    std::move(waiter).Wait();
  }

  std::unique_ptr<PersonalDataManager> personal_data_;
};

TEST_F(PersonalDataManagerTest, DefaultCountryCodeIsCached) {
  // The return value should always be some country code, no matter what.
  std::string default_country =
      personal_data_->GetDefaultCountryCodeForNewAddress();
  EXPECT_EQ(2U, default_country.size());

  AutofillProfile profile = test::GetFullProfile();
  AddProfileToPersonalDataManager(profile);

  // The value is cached and doesn't change even after adding an address.
  EXPECT_EQ(default_country,
            personal_data_->GetDefaultCountryCodeForNewAddress());

  // Disabling Autofill blows away this cache and shouldn't account for Autofill
  // profiles.
  prefs::SetAutofillProfileEnabled(prefs_.get(), false);
  prefs::SetAutofillPaymentMethodsEnabled(prefs_.get(), false);
  PersonalDataChangedWaiter(*personal_data_).Wait();
  EXPECT_EQ(default_country,
            personal_data_->GetDefaultCountryCodeForNewAddress());

  // Enabling Autofill blows away the cached value and should reflect the new
  // value (accounting for profiles).
  prefs::SetAutofillProfileEnabled(prefs_.get(), true);
  EXPECT_EQ(base::UTF16ToUTF8(profile.GetRawInfo(ADDRESS_HOME_COUNTRY)),
            personal_data_->GetDefaultCountryCodeForNewAddress());
}

TEST_F(PersonalDataManagerTest, DefaultCountryCodeComesFromProfiles) {
  AutofillProfile canadian_profile = test::GetFullCanadianProfile();
  ASSERT_EQ(canadian_profile.GetRawInfo(ADDRESS_HOME_COUNTRY), u"CA");
  AddProfileToPersonalDataManager(canadian_profile);
  ResetPersonalDataManager();
  EXPECT_EQ("CA", personal_data_->GetDefaultCountryCodeForNewAddress());

  // Multiple profiles cast votes.
  AutofillProfile us_profile1 = test::GetFullProfile();
  AutofillProfile us_profile2 = test::GetFullProfile2();
  ASSERT_EQ(us_profile1.GetRawInfo(ADDRESS_HOME_COUNTRY), u"US");
  ASSERT_EQ(us_profile2.GetRawInfo(ADDRESS_HOME_COUNTRY), u"US");
  AddProfileToPersonalDataManager(us_profile1);
  AddProfileToPersonalDataManager(us_profile2);
  ResetPersonalDataManager();
  EXPECT_EQ("US", personal_data_->GetDefaultCountryCodeForNewAddress());
}

TEST_F(PersonalDataManagerTest, DefaultCountryCodeComesFromVariations) {
  const std::string expected_country_code = "DE";
  const std::string unexpected_country_code = "FR";

  // Normally, the variation country code is passed to the constructor.
  personal_data_->set_variations_country_code_for_testing(
      expected_country_code);

  // Since there are no profiles set, the country code supplied by variations
  // should be used get get a default country code.
  ASSERT_EQ(0u, personal_data_->GetProfiles().size());
  std::string actual_country_code =
      personal_data_->GetDefaultCountryCodeForNewAddress();
  EXPECT_EQ(expected_country_code, actual_country_code);

  // Set a new country code.
  // The default country code retrieved before should have been cached.
  personal_data_->set_variations_country_code_for_testing(
      unexpected_country_code);
  actual_country_code = personal_data_->GetDefaultCountryCodeForNewAddress();
  EXPECT_EQ(expected_country_code, actual_country_code);
}

TEST_F(PersonalDataManagerTest, OnAccountsCookieDeletedByUserAction) {
  // Set up some sync transport opt-ins in the prefs.
  ::autofill::prefs::SetUserOptedInWalletSyncTransport(
      prefs_.get(), CoreAccountId::FromGaiaId("account1"), true);
  EXPECT_FALSE(prefs_->GetDict(prefs::kAutofillSyncTransportOptIn).empty());

  // Simulate that the cookies get cleared by the user.
  personal_data_->OnAccountsCookieDeletedByUserAction();

  // Make sure the pref is now empty.
  EXPECT_TRUE(prefs_->GetDict(prefs::kAutofillSyncTransportOptIn).empty());
}

TEST_F(PersonalDataManagerTest, IsCountryEligibleForAccountStorage) {
  EXPECT_TRUE(personal_data_->IsCountryEligibleForAccountStorage("AT"));
  EXPECT_FALSE(personal_data_->IsCountryEligibleForAccountStorage("IR"));
}

TEST_F(PersonalDataManagerTest, ChangeCallbackIsTriggeredOnAddedProfile) {
  ::testing::StrictMock<base::MockOnceClosure> callback;
  EXPECT_CALL(callback, Run);

  personal_data_->AddChangeCallback(callback.Get());
  AddProfileToPersonalDataManager(test::GetFullProfile());
}

}  // namespace autofill
