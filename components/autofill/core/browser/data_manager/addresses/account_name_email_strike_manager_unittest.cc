// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/autofill/core/browser/data_manager/addresses/account_name_email_strike_manager.h"

#include <memory>

#include "base/functional/callback_helpers.h"
#include "base/metrics/histogram.h"
#include "base/test/metrics/histogram_tester.h"
#include "base/test/task_environment.h"
#include "components/autofill/core/browser/autofill_field.h"
#include "components/autofill/core/browser/autofill_trigger_source.h"
#include "components/autofill/core/browser/country_type.h"
#include "components/autofill/core/browser/data_manager/addresses/account_name_email_strike_manager_test_api.h"
#include "components/autofill/core/browser/data_model/addresses/autofill_profile.h"
#include "components/autofill/core/browser/form_structure.h"
#include "components/autofill/core/browser/foundations/browser_autofill_manager.h"
#include "components/autofill/core/browser/foundations/browser_autofill_manager_test_api.h"
#include "components/autofill/core/browser/foundations/test_autofill_client.h"
#include "components/autofill/core/browser/foundations/test_autofill_driver.h"
#include "components/autofill/core/browser/foundations/test_browser_autofill_manager.h"
#include "components/autofill/core/browser/foundations/with_test_autofill_client_driver_manager.h"
#include "components/autofill/core/browser/suggestions/suggestion.h"
#include "components/autofill/core/browser/suggestions/suggestion_type.h"
#include "components/autofill/core/common/form_data.h"
#include "components/autofill/core/common/unique_ids.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace autofill {
namespace {

using RecordType = AutofillProfile::RecordType;

class AccountNameEmailStrikeManagerTest
    : public testing::Test,
      public WithTestAutofillClientDriverManager<TestAutofillClient,
                                                 TestAutofillDriver,
                                                 TestBrowserAutofillManager> {
 public:
  void SetUp() override {
    InitAutofillClient();
    autofill_client().SetPrefs(test::PrefServiceForTesting());
    CreateAutofillDriver();
  }

  void TearDown() override { DestroyAutofillClient(); }

  AutofillProfile CreateAutofillProfileWithType(RecordType record_type) {
    AutofillProfile test_profile(record_type, AddressCountryCode("XX"));
    autofill_client()
        .GetPersonalDataManager()
        .address_data_manager()
        .AddProfile(test_profile);
    return test_profile;
  }

  Suggestion CreateSuggestionForProfile(const AutofillProfile& profile) const {
    Suggestion suggestion(SuggestionType::kAddressEntry);
    suggestion.payload =
        Suggestion::AutofillProfilePayload(Suggestion::Guid(profile.guid()));
    return suggestion;
  }

  AccountNameEmailStrikeManager* GetAccountNameEmailStrikeManager() {
    return test_api(autofill_manager()).account_name_email_strike_manager();
  }

 protected:
  base::test::TaskEnvironment task_environment_;
  autofill::test::AutofillUnitTestEnvironment autofill_test_environment_;
};

TEST_F(AccountNameEmailStrikeManagerTest,
       DidShowSuggestions_NotNameEmailSuggestion) {
  autofill_manager().DidShowSuggestions(
      {CreateSuggestionForProfile(
          CreateAutofillProfileWithType(RecordType::kAccount))},
      FormData(), FieldGlobalId(), base::DoNothing());
  EXPECT_FALSE(test_api(GetAccountNameEmailStrikeManager())
                   .was_name_email_profile_suggestion_shown());
  EXPECT_FALSE(test_api(GetAccountNameEmailStrikeManager())
                   .was_name_email_profile_filled());

  autofill_manager().Reset();
  EXPECT_EQ(autofill_client().GetPrefs()->GetInteger(
                prefs::kAutofillNameAndEmailProfileNotSelectedCounter),
            0);
  EXPECT_FALSE(autofill_client().GetPrefs()->GetBoolean(
      prefs::kAutofillWasNameAndEmailProfileUsed));
}

TEST_F(AccountNameEmailStrikeManagerTest,
       DidShowSuggestions_NameEmailSuggestion) {
  autofill_manager().DidShowSuggestions(
      {CreateSuggestionForProfile(
          CreateAutofillProfileWithType(RecordType::kAccountNameEmail))},
      FormData(), FieldGlobalId(), base::DoNothing());
  EXPECT_TRUE(test_api(GetAccountNameEmailStrikeManager())
                  .was_name_email_profile_suggestion_shown());
  EXPECT_FALSE(test_api(GetAccountNameEmailStrikeManager())
                   .was_name_email_profile_filled());

  autofill_manager().Reset();
  EXPECT_EQ(autofill_client().GetPrefs()->GetInteger(
                prefs::kAutofillNameAndEmailProfileNotSelectedCounter),
            1);
  EXPECT_FALSE(autofill_client().GetPrefs()->GetBoolean(
      prefs::kAutofillWasNameAndEmailProfileUsed));
}

TEST_F(AccountNameEmailStrikeManagerTest,
       OnDidFillOrPreviewForm_NameEmailSuggestionPreviewed) {
  AutofillProfile profile =
      CreateAutofillProfileWithType(RecordType::kAccountNameEmail);
  autofill_manager().DidShowSuggestions({CreateSuggestionForProfile(profile)},
                                        FormData(), FieldGlobalId(),
                                        base::DoNothing());
  autofill_manager().OnDidFillOrPreviewForm(
      mojom::ActionPersistence::kPreview, FormStructure(FormData()),
      AutofillField(), {}, {}, &profile, AutofillTriggerSource::kPopup,
      std::nullopt);

  EXPECT_TRUE(test_api(GetAccountNameEmailStrikeManager())
                  .was_name_email_profile_suggestion_shown());
  EXPECT_FALSE(test_api(GetAccountNameEmailStrikeManager())
                   .was_name_email_profile_filled());

  base::HistogramTester histogram_tester;
  autofill_manager().Reset();
  EXPECT_EQ(autofill_client().GetPrefs()->GetInteger(
                prefs::kAutofillNameAndEmailProfileNotSelectedCounter),
            1);
  EXPECT_FALSE(autofill_client().GetPrefs()->GetBoolean(
      prefs::kAutofillWasNameAndEmailProfileUsed));
  // The counter pref is too low to trigger the implicit removal.
  histogram_tester.ExpectUniqueSample(
      "Autofill.ProfileDeleted.ImplicitAccountNameEmail", false, 1);
}

TEST_F(AccountNameEmailStrikeManagerTest,
       OnDidFillOrPreviewForm_ImplicitRemovalMetricRecorded) {
  // Set the initial pref value such that the next suggestion will trigger the
  // implicit removal.
  autofill_client().GetPrefs()->SetInteger(
      prefs::kAutofillNameAndEmailProfileNotSelectedCounter,
      features::kAutofillNameAndEmailProfileNotSelectedThreshold.Get());

  AutofillProfile profile =
      CreateAutofillProfileWithType(RecordType::kAccountNameEmail);

  autofill_manager().DidShowSuggestions({CreateSuggestionForProfile(profile)},
                                        FormData(), FieldGlobalId(),
                                        base::DoNothing());
  autofill_manager().OnDidFillOrPreviewForm(
      mojom::ActionPersistence::kPreview, FormStructure(FormData()),
      AutofillField(), {}, {}, &profile, AutofillTriggerSource::kPopup,
      std::nullopt);

  base::HistogramTester histogram_tester;
  autofill_manager().Reset();
  histogram_tester.ExpectUniqueSample(
      "Autofill.ProfileDeleted.ImplicitAccountNameEmail", true, 1);
}

TEST_F(AccountNameEmailStrikeManagerTest,
       OnDidFillOrPreviewForm_NameEmailSuggestionFilled) {
  AutofillProfile profile =
      CreateAutofillProfileWithType(RecordType::kAccountNameEmail);
  autofill_manager().DidShowSuggestions({CreateSuggestionForProfile(profile)},
                                        FormData(), FieldGlobalId(),
                                        base::DoNothing());
  autofill_manager().OnDidFillOrPreviewForm(
      mojom::ActionPersistence::kFill, FormStructure(FormData()),
      AutofillField(), {}, {}, &profile, AutofillTriggerSource::kPopup,
      std::nullopt);

  EXPECT_TRUE(test_api(GetAccountNameEmailStrikeManager())
                  .was_name_email_profile_suggestion_shown());
  EXPECT_TRUE(test_api(GetAccountNameEmailStrikeManager())
                  .was_name_email_profile_filled());

  autofill_manager().Reset();
  EXPECT_EQ(autofill_client().GetPrefs()->GetInteger(
                prefs::kAutofillNameAndEmailProfileNotSelectedCounter),
            0);
  EXPECT_TRUE(autofill_client().GetPrefs()->GetBoolean(
      prefs::kAutofillWasNameAndEmailProfileUsed));
}

TEST_F(AccountNameEmailStrikeManagerTest,
       OnDidFillOrPreviewForm_NotNameEmailSuggestionWasUsed) {
  AutofillProfile account_profile =
      CreateAutofillProfileWithType(RecordType::kAccount);

  autofill_manager().DidShowSuggestions(
      {CreateSuggestionForProfile(
           CreateAutofillProfileWithType(RecordType::kAccountNameEmail)),
       CreateSuggestionForProfile(account_profile)},
      FormData(), FieldGlobalId(), base::DoNothing());

  autofill_manager().OnDidFillOrPreviewForm(
      mojom::ActionPersistence::kFill, FormStructure(FormData()),
      AutofillField(), {}, {}, &account_profile, AutofillTriggerSource::kPopup,
      std::nullopt);

  EXPECT_TRUE(test_api(GetAccountNameEmailStrikeManager())
                  .was_name_email_profile_suggestion_shown());
  EXPECT_FALSE(test_api(GetAccountNameEmailStrikeManager())
                   .was_name_email_profile_filled());

  autofill_manager().Reset();
  EXPECT_EQ(autofill_client().GetPrefs()->GetInteger(
                prefs::kAutofillNameAndEmailProfileNotSelectedCounter),
            1);
  EXPECT_FALSE(autofill_client().GetPrefs()->GetBoolean(
      prefs::kAutofillWasNameAndEmailProfileUsed));
}

}  // namespace
}  // namespace autofill
