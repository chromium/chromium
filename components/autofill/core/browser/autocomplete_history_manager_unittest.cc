// Copyright 2013 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <string>
#include <vector>

#include "base/memory/ref_counted.h"
#include "base/memory/weak_ptr.h"
#include "base/run_loop.h"
#include "base/strings/utf_string_conversions.h"
#include "base/synchronization/waitable_event.h"
#include "base/test/metrics/histogram_tester.h"
#include "base/test/scoped_feature_list.h"
#include "base/test/task_environment.h"
#include "base/time/time.h"
#include "build/build_config.h"
#include "components/autofill/core/browser/autocomplete_history_manager.h"
#include "components/autofill/core/browser/autofill_test_utils.h"
#include "components/autofill/core/browser/suggestions_context.h"
#include "components/autofill/core/browser/test_autofill_client.h"
#include "components/autofill/core/browser/test_autofill_clock.h"
#include "components/autofill/core/browser/webdata/autofill_entry.h"
#include "components/autofill/core/browser/webdata/autofill_webdata_service.h"
#include "components/autofill/core/browser/webdata/mock_autofill_webdata_service.h"
#include "components/autofill/core/common/autofill_clock.h"
#include "components/autofill/core/common/autofill_features.h"
#include "components/autofill/core/common/autofill_prefs.h"
#include "components/autofill/core/common/autofill_test_utils.h"
#include "components/autofill/core/common/form_data.h"
#include "components/prefs/testing_pref_service.h"
#include "components/version_info/version_info.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "ui/gfx/geometry/rect.h"

namespace autofill {

namespace {

using test::CreateTestFormField;
using ::testing::_;
using ::testing::Eq;
using ::testing::Field;
using ::testing::Return;
using ::testing::UnorderedElementsAre;

class MockSuggestionsHandler
    : public AutocompleteHistoryManager::SuggestionsHandler {
 public:
  MockSuggestionsHandler() {}

  MockSuggestionsHandler(const MockSuggestionsHandler&) = delete;
  MockSuggestionsHandler& operator=(const MockSuggestionsHandler&) = delete;

  MOCK_METHOD(void,
              OnSuggestionsReturned,
              (FieldGlobalId field_id,
               AutofillSuggestionTriggerSource trigger_source,
               const std::vector<Suggestion>& suggestions),
              (override));

  base::WeakPtr<MockSuggestionsHandler> GetWeakPtr() {
    return weak_ptr_factory_.GetWeakPtr();
  }

 private:
  base::WeakPtrFactory<MockSuggestionsHandler> weak_ptr_factory_{this};
};
}  // namespace

class AutocompleteHistoryManagerTest : public testing::Test {
 protected:
  AutocompleteHistoryManagerTest() = default;

  void SetUp() override {
    prefs_ = test::PrefServiceForTesting();

    // Mock such that we don't trigger the cleanup.
    prefs_->SetInteger(prefs::kAutocompleteLastVersionRetentionPolicy,
                       CHROME_VERSION_MAJOR);

    // Set time to some arbitrary date.
    test_clock.SetNow(base::Time::FromDoubleT(1546889367));
    web_data_service_ = base::MakeRefCounted<MockAutofillWebDataService>();
    autocomplete_manager_ = std::make_unique<AutocompleteHistoryManager>();
    autocomplete_manager_->Init(web_data_service_, prefs_.get(), false);
    test_field_ = CreateTestFormField(/*label=*/"", "Some Field Name",
                                      "SomePrefix", "Some Type");
    second_test_field_ = CreateTestFormField(/*label=*/"", "Another Field Name",
                                             "AnotherPrefix", "Another Type");
  }

  void TearDown() override {
    // Ensure there are no left-over entries in the map (leak check).
    EXPECT_TRUE(PendingQueriesEmpty());

    autocomplete_manager_.reset();
  }

  bool PendingQueriesEmpty() {
    return !autocomplete_manager_ ||
           autocomplete_manager_->pending_queries_.empty();
  }

  static bool IsEmptySuggestionVector(
      const std::vector<Suggestion>& suggestions) {
    return suggestions.empty();
  }

  static bool NonEmptySuggestionVector(
      const std::vector<Suggestion>& suggestions) {
    return !suggestions.empty();
  }

  std::unique_ptr<WDTypedResult> GetMockedDbResults(
      std::vector<AutofillEntry> values) {
    return std::make_unique<WDResult<std::vector<AutofillEntry>>>(
        AUTOFILL_VALUE_RESULT, values);
  }

  AutofillEntry GetAutofillEntry(
      const std::u16string& name,
      const std::u16string& value,
      const base::Time& date_created = AutofillClock::Now(),
      const base::Time& date_last_used = AutofillClock::Now()) {
    return AutofillEntry(AutofillKey(name, value), date_created,
                         date_last_used);
  }

  base::test::SingleThreadTaskEnvironment task_environment_;
  test::AutofillUnitTestEnvironment autofill_test_environment_;
  TestAutofillClient autofill_client_;
  scoped_refptr<MockAutofillWebDataService> web_data_service_;
  std::unique_ptr<AutocompleteHistoryManager> autocomplete_manager_;
  std::unique_ptr<PrefService> prefs_;
  FormFieldData test_field_;
  FormFieldData second_test_field_;
  TestAutofillClock test_clock;
};

// Tests that credit card numbers are not sent to the WebDatabase to be saved.
TEST_F(AutocompleteHistoryManagerTest, CreditCardNumberValue) {
  FormData form;
  form.name = u"MyForm";
  form.url = GURL("http://myform.com/form.html");
  form.action = GURL("http://myform.com/submit.html");

  // Valid Visa credit card number pulled from the paypal help site.
  FormFieldData valid_cc;
  valid_cc.label = u"Credit Card";
  valid_cc.name = u"ccnum";
  valid_cc.value = u"4012888888881881";
  valid_cc.properties_mask |= kUserTyped;
  valid_cc.form_control_type = "text";
  form.fields.push_back(valid_cc);

  EXPECT_CALL(*(web_data_service_.get()), AddFormFields(_)).Times(0);
  autocomplete_manager_->OnWillSubmitFormWithFields(
      form.fields,
      /*is_autocomplete_enabled=*/true);
}

// Contrary test to AutocompleteHistoryManagerTest.CreditCardNumberValue.  The
// value being submitted is not a valid credit card number, so it will be sent
// to the WebDatabase to be saved.
TEST_F(AutocompleteHistoryManagerTest, NonCreditCardNumberValue) {
  FormData form;
  form.name = u"MyForm";
  form.url = GURL("http://myform.com/form.html");
  form.action = GURL("http://myform.com/submit.html");

  // Invalid credit card number.
  FormFieldData invalid_cc;
  invalid_cc.label = u"Credit Card";
  invalid_cc.name = u"ccnum";
  invalid_cc.value = u"4580123456789012";
  invalid_cc.properties_mask |= kUserTyped;
  invalid_cc.form_control_type = "text";
  form.fields.push_back(invalid_cc);

  EXPECT_CALL(*(web_data_service_.get()), AddFormFields(_));
  autocomplete_manager_->OnWillSubmitFormWithFields(
      form.fields,
      /*is_autocomplete_enabled=*/true);
}

// Tests that SSNs are not sent to the WebDatabase to be saved.
TEST_F(AutocompleteHistoryManagerTest, SSNValue) {
  FormData form;
  form.name = u"MyForm";
  form.url = GURL("http://myform.com/form.html");
  form.action = GURL("http://myform.com/submit.html");

  FormFieldData ssn;
  ssn.label = u"Social Security Number";
  ssn.name = u"ssn";
  ssn.value = u"078-05-1120";
  ssn.properties_mask |= kUserTyped;
  ssn.form_control_type = "text";
  form.fields.push_back(ssn);

  EXPECT_CALL(*web_data_service_, AddFormFields(_)).Times(0);
  autocomplete_manager_->OnWillSubmitFormWithFields(
      form.fields,
      /*is_autocomplete_enabled=*/true);
}

// Verify that autocomplete text is saved for search fields.
TEST_F(AutocompleteHistoryManagerTest, SearchField) {
  FormData form;
  form.name = u"MyForm";
  form.url = GURL("http://myform.com/form.html");
  form.action = GURL("http://myform.com/submit.html");

  // Search field.
  FormFieldData search_field;
  search_field.label = u"Search";
  search_field.name = u"search";
  search_field.value = u"my favorite query";
  search_field.properties_mask |= kUserTyped;
  search_field.form_control_type = "search";
  form.fields.push_back(search_field);

  EXPECT_CALL(*(web_data_service_.get()), AddFormFields(_));
  autocomplete_manager_->OnWillSubmitFormWithFields(
      form.fields,
      /*is_autocomplete_enabled=*/true);
}

TEST_F(AutocompleteHistoryManagerTest, AutocompleteFeatureOff) {
  FormData form;
  form.name = u"MyForm";
  form.url = GURL("http://myform.com/form.html");
  form.action = GURL("http://myform.com/submit.html");

  // Search field.
  FormFieldData search_field;
  search_field.label = u"Search";
  search_field.name = u"search";
  search_field.value = u"my favorite query";
  search_field.properties_mask |= kUserTyped;
  search_field.form_control_type = "search";
  form.fields.push_back(search_field);

  EXPECT_CALL(*(web_data_service_.get()), AddFormFields(_)).Times(0);
  autocomplete_manager_->OnWillSubmitFormWithFields(
      form.fields,
      /*is_autocomplete_enabled=*/false);
}

// Verify that we don't save invalid values in Autocomplete.
TEST_F(AutocompleteHistoryManagerTest, InvalidValues) {
  FormData form;
  form.name = u"MyForm";
  form.url = GURL("http://myform.com/form.html");
  form.action = GURL("http://myform.com/submit.html");

  // Search field.
  FormFieldData search_field;

  // Empty value.
  search_field.label = u"Search";
  search_field.name = u"search";
  search_field.value = u"";
  search_field.properties_mask |= kUserTyped;
  search_field.form_control_type = "search";
  form.fields.push_back(search_field);

  // Single whitespace.
  search_field.label = u"Search2";
  search_field.name = u"other search";
  search_field.value = u" ";
  search_field.properties_mask |= kUserTyped;
  search_field.form_control_type = "search";
  form.fields.push_back(search_field);

  // Multiple whitespaces.
  search_field.label = u"Search3";
  search_field.name = u"other search";
  search_field.value = u"      ";
  search_field.properties_mask |= kUserTyped;
  search_field.form_control_type = "search";
  form.fields.push_back(search_field);

  EXPECT_CALL(*(web_data_service_.get()), AddFormFields(_)).Times(0);
  autocomplete_manager_->OnWillSubmitFormWithFields(
      form.fields,
      /*is_autocomplete_enabled=*/true);
}

// Tests that text entered into fields specifying autocomplete="off" is not sent
// to the WebDatabase to be saved. Note this is also important as the mechanism
// for preventing CVCs from being saved.
// See BrowserAutofillManagerTest.DontSaveCvcInAutocompleteHistory
TEST_F(AutocompleteHistoryManagerTest, FieldWithAutocompleteOff) {
  FormData form;
  form.name = u"MyForm";
  form.url = GURL("http://myform.com/form.html");
  form.action = GURL("http://myform.com/submit.html");

  // Field specifying autocomplete="off".
  FormFieldData field;
  field.label = u"Something esoteric";
  field.name = u"esoterica";
  field.value = u"a truly esoteric value, I assure you";
  field.properties_mask |= kUserTyped;
  field.form_control_type = "text";
  field.should_autocomplete = false;
  form.fields.push_back(field);

  EXPECT_CALL(*web_data_service_, AddFormFields(_)).Times(0);
  autocomplete_manager_->OnWillSubmitFormWithFields(
      form.fields,
      /*is_autocomplete_enabled=*/true);
}

// Shouldn't save entries when in Incognito mode.
TEST_F(AutocompleteHistoryManagerTest, Incognito) {
  autocomplete_manager_->Init(web_data_service_, prefs_.get(),
                              /*is_off_the_record_=*/true);
  FormData form;
  form.name = u"MyForm";
  form.url = GURL("http://myform.com/form.html");
  form.action = GURL("http://myform.com/submit.html");

  // Search field.
  FormFieldData search_field;
  search_field.label = u"Search";
  search_field.name = u"search";
  search_field.value = u"my favorite query";
  search_field.properties_mask |= kUserTyped;
  search_field.form_control_type = "search";
  form.fields.push_back(search_field);

  EXPECT_CALL(*web_data_service_, AddFormFields(_)).Times(0);
  autocomplete_manager_->OnWillSubmitFormWithFields(
      form.fields,
      /*is_autocomplete_enabled=*/true);
}

#if !BUILDFLAG(IS_IOS)
// Tests that fields that are no longer focusable but still have user typed
// input are sent to the WebDatabase to be saved. Will not work for iOS
// because |properties_mask| is not set on iOS.
TEST_F(AutocompleteHistoryManagerTest, UserInputNotFocusable) {
  FormData form;
  form.name = u"MyForm";
  form.url = GURL("http://myform.com/form.html");
  form.action = GURL("http://myform.com/submit.html");

  // Search field.
  FormFieldData search_field;
  search_field.label = u"Search";
  search_field.name = u"search";
  search_field.value = u"my favorite query";
  search_field.form_control_type = "search";
  search_field.properties_mask |= kUserTyped;
  search_field.is_focusable = false;
  form.fields.push_back(search_field);

  EXPECT_CALL(*(web_data_service_.get()), AddFormFields(_));
  autocomplete_manager_->OnWillSubmitFormWithFields(
      form.fields,
      /*is_autocomplete_enabled=*/true);
}
#endif

// Tests that text entered into presentation fields is not sent to the
// WebDatabase to be saved.
TEST_F(AutocompleteHistoryManagerTest, PresentationField) {
  FormData form;
  form.name = u"MyForm";
  form.url = GURL("http://myform.com/form.html");
  form.action = GURL("http://myform.com/submit.html");

  // Presentation field.
  FormFieldData field;
  field.label = u"Something esoteric";
  field.name = u"esoterica";
  field.value = u"a truly esoteric value, I assure you";
  field.properties_mask |= kUserTyped;
  field.form_control_type = "text";
  field.role = FormFieldData::RoleAttribute::kPresentation;
  form.fields.push_back(field);

  EXPECT_CALL(*web_data_service_, AddFormFields(_)).Times(0);
  autocomplete_manager_->OnWillSubmitFormWithFields(
      form.fields,
      /*is_autocomplete_enabled=*/true);
}

// Tests that the Init function will trigger the Autocomplete Retention Policy
// cleanup if the flag is enabled, we're not in OTR and it hadn't run in the
// current major version.
TEST_F(AutocompleteHistoryManagerTest, Init_TriggersCleanup) {
  // Set the retention policy cleanup to a past major version.
  prefs_->SetInteger(prefs::kAutocompleteLastVersionRetentionPolicy,
                     CHROME_VERSION_MAJOR - 1);

  EXPECT_CALL(*web_data_service_,
              RemoveExpiredAutocompleteEntries(autocomplete_manager_.get()))
      .Times(1);
  autocomplete_manager_->Init(web_data_service_, prefs_.get(),
                              /*is_off_the_record=*/false);
}

// Tests that the Init function will not trigger the Autocomplete Retention
// Policy when running in OTR.
TEST_F(AutocompleteHistoryManagerTest, Init_OTR_Not_TriggersCleanup) {
  // Set the retention policy cleanup to a past major version.
  prefs_->SetInteger(prefs::kAutocompleteLastVersionRetentionPolicy,
                     CHROME_VERSION_MAJOR - 1);

  EXPECT_CALL(*web_data_service_,
              RemoveExpiredAutocompleteEntries(autocomplete_manager_.get()))
      .Times(0);
  autocomplete_manager_->Init(web_data_service_, prefs_.get(),
                              /*is_off_the_record=*/true);
}

// Tests that the Init function will not crash even if we don't have a DB.
TEST_F(AutocompleteHistoryManagerTest, Init_NullDB_NoCrash) {
  // Set the retention policy cleanup to a past major version.
  prefs_->SetInteger(prefs::kAutocompleteLastVersionRetentionPolicy,
                     CHROME_VERSION_MAJOR - 1);

  EXPECT_CALL(*web_data_service_,
              RemoveExpiredAutocompleteEntries(autocomplete_manager_.get()))
      .Times(0);
  autocomplete_manager_->Init(nullptr, prefs_.get(),
                              /*is_off_the_record=*/false);
}

// Tests that the Init function will not trigger the Autocomplete Retention
// Policy when running in a major version that was already cleaned.
TEST_F(AutocompleteHistoryManagerTest,
       Init_SameMajorVersion_Not_TriggersCleanup) {
  // Set the retention policy cleanup to the current major version.
  prefs_->SetInteger(prefs::kAutocompleteLastVersionRetentionPolicy,
                     CHROME_VERSION_MAJOR);

  EXPECT_CALL(*web_data_service_,
              RemoveExpiredAutocompleteEntries(autocomplete_manager_.get()))
      .Times(0);
  autocomplete_manager_->Init(web_data_service_, prefs_.get(),
                              /*is_off_the_record=*/false);
}

// Make sure suggestions are not returned if the field should not autocomplete.
TEST_F(AutocompleteHistoryManagerTest,
       OnGetSingleFieldSuggestions_FieldShouldNotAutocomplete) {
  test_field_.should_autocomplete = false;

  auto suggestions_handler = std::make_unique<MockSuggestionsHandler>();

  // Setting up mock to verify that call to the handler's OnSuggestionsReturned
  // is not triggered.
  EXPECT_CALL(*suggestions_handler, OnSuggestionsReturned).Times(0);

  EXPECT_CALL(*web_data_service_, GetFormValuesForElementName).Times(0);

  // Simulate request for suggestions.
  EXPECT_FALSE(autocomplete_manager_->OnGetSingleFieldSuggestions(
      AutofillSuggestionTriggerSource::kFormControlElementClicked, test_field_,
      autofill_client_, suggestions_handler->GetWeakPtr(),
      SuggestionsContext()));
}

// Make sure our handler is called at the right time.
TEST_F(AutocompleteHistoryManagerTest,
       SuggestionsReturned_InvokeHandler_Empty) {
  int mocked_db_query_id = 100;

  auto suggestions_handler = std::make_unique<MockSuggestionsHandler>();

  std::vector<AutofillEntry> expected_values;

  std::unique_ptr<WDTypedResult> mocked_results =
      GetMockedDbResults(expected_values);

  EXPECT_CALL(*web_data_service_,
              GetFormValuesForElementName(test_field_.name, test_field_.value,
                                          _, autocomplete_manager_.get()))
      .WillOnce(Return(mocked_db_query_id));

  // Simulate request for suggestions.
  EXPECT_TRUE(autocomplete_manager_->OnGetSingleFieldSuggestions(
      AutofillSuggestionTriggerSource::kFormControlElementClicked, test_field_,
      autofill_client_, suggestions_handler->GetWeakPtr(),
      SuggestionsContext()));

  // Setting up mock to verify that DB response triggers a call to the handler's
  // OnSuggestionsReturned
  EXPECT_CALL(*suggestions_handler.get(),
              OnSuggestionsReturned(
                  test_field_.global_id(),
                  AutofillSuggestionTriggerSource::kFormControlElementClicked,
                  testing::Truly(IsEmptySuggestionVector)));

  // Simulate response from DB.
  autocomplete_manager_->OnWebDataServiceRequestDone(mocked_db_query_id,
                                                     std::move(mocked_results));
}

// Tests that no suggestions are queried if the field name is filtered because
// it has a meaningless sub string that is allowed for sub string matches.
TEST_F(AutocompleteHistoryManagerTest,
       DoQuerySuggestionsForMeaninglessFieldNames_FilterSubStringName) {
  auto suggestions_handler = std::make_unique<MockSuggestionsHandler>();
  test_field_ = CreateTestFormField(/*label=*/"", "payment_cvv_info",
                                    /*value=*/"", "Some Type");

  // Only expect a call when the name is not filtered out.
  EXPECT_CALL(*web_data_service_,
              GetFormValuesForElementName(test_field_.name, test_field_.value,
                                          _, autocomplete_manager_.get()))
      .Times(0);

  // Simulate request for suggestions.
  EXPECT_TRUE(autocomplete_manager_->OnGetSingleFieldSuggestions(
      AutofillSuggestionTriggerSource::kFormControlElementClicked, test_field_,
      autofill_client_, suggestions_handler->GetWeakPtr(),
      SuggestionsContext()));

  // Setting up mock to verify that DB response does not trigger a call to the
  // handler's OnSuggestionsReturned.
  EXPECT_CALL(
      *suggestions_handler.get(),
      OnSuggestionsReturned(
          test_field_.global_id(),
          AutofillSuggestionTriggerSource::kFormControlElementClicked, _))
      .Times(0);
}

// Tests that no suggestions are queried if the field name is filtered because
// it has a meaningless name.
TEST_F(AutocompleteHistoryManagerTest,
       DoQuerySuggestionsForMeaninglessFieldNames_FilterName) {
  auto suggestions_handler = std::make_unique<MockSuggestionsHandler>();
  test_field_ =
      CreateTestFormField(/*label=*/"", "input_123", /*value=*/"", "Some Type");

  // Only expect a call when the name is not filtered out.
  EXPECT_CALL(*web_data_service_,
              GetFormValuesForElementName(test_field_.name, test_field_.value,
                                          _, autocomplete_manager_.get()))
      .Times(0);

  // Simulate request for suggestions.
  EXPECT_TRUE(autocomplete_manager_->OnGetSingleFieldSuggestions(
      AutofillSuggestionTriggerSource::kFormControlElementClicked, test_field_,
      autofill_client_, suggestions_handler->GetWeakPtr(),
      SuggestionsContext()));

  // Setting up mock to verify that DB response does not trigger a call to the
  // handler's OnSuggestionsReturned.
  EXPECT_CALL(
      *suggestions_handler.get(),
      OnSuggestionsReturned(
          test_field_.global_id(),
          AutofillSuggestionTriggerSource::kFormControlElementClicked, _))
      .Times(0);
}

// Tests that the suggestions are queried if the field has meaningless substring
// which is not allowed for substring matches.
TEST_F(AutocompleteHistoryManagerTest,
       DoQuerySuggestionsForMeaninglessFieldNames_PassNameWithSubstring) {
  auto suggestions_handler = std::make_unique<MockSuggestionsHandler>();
  int mocked_db_query_id = 100;
  test_field_ =
      CreateTestFormField(/*label=*/"", "foOTPace", /*value=*/"", "Some Type");

  std::vector<AutofillEntry> expected_values;

  std::unique_ptr<WDTypedResult> mocked_results =
      GetMockedDbResults(expected_values);

  // Expect a call because the name is not filtered.
  EXPECT_CALL(*web_data_service_,
              GetFormValuesForElementName(test_field_.name, test_field_.value,
                                          _, autocomplete_manager_.get()))
      .WillOnce(Return(mocked_db_query_id));

  // Simulate request for suggestions.
  EXPECT_TRUE(autocomplete_manager_->OnGetSingleFieldSuggestions(
      AutofillSuggestionTriggerSource::kFormControlElementClicked, test_field_,
      autofill_client_, suggestions_handler->GetWeakPtr(),
      SuggestionsContext()));

  // Setting up mock to verify that DB response triggers a call to the handler's
  EXPECT_CALL(
      *suggestions_handler.get(),
      OnSuggestionsReturned(
          test_field_.global_id(),
          AutofillSuggestionTriggerSource::kFormControlElementClicked, _));

  autocomplete_manager_->OnWebDataServiceRequestDone(mocked_db_query_id,
                                                     std::move(mocked_results));
}
// Tests that the suggestions are queried if the field name is not filtered
// because the field's name is meaningful.
TEST_F(AutocompleteHistoryManagerTest,
       DoQuerySuggestionsForMeaninglessFieldNames_PassName) {
  auto suggestions_handler = std::make_unique<MockSuggestionsHandler>();
  int mocked_db_query_id = 100;
  test_field_ = CreateTestFormField(/*label=*/"", "addressline_1", /*value=*/"",
                                    "Some Type");

  std::vector<AutofillEntry> expected_values;

  std::unique_ptr<WDTypedResult> mocked_results =
      GetMockedDbResults(expected_values);

  // Expect a call because the name is not filtered.
  EXPECT_CALL(*web_data_service_,
              GetFormValuesForElementName(test_field_.name, test_field_.value,
                                          _, autocomplete_manager_.get()))
      .WillOnce(Return(mocked_db_query_id));

  // Simulate request for suggestions.
  EXPECT_TRUE(autocomplete_manager_->OnGetSingleFieldSuggestions(
      AutofillSuggestionTriggerSource::kFormControlElementClicked, test_field_,
      autofill_client_, suggestions_handler->GetWeakPtr(),
      SuggestionsContext()));

  // Setting up mock to verify that DB response triggers a call to the handler's
  EXPECT_CALL(
      *suggestions_handler.get(),
      OnSuggestionsReturned(
          test_field_.global_id(),
          AutofillSuggestionTriggerSource::kFormControlElementClicked, _));

  autocomplete_manager_->OnWebDataServiceRequestDone(mocked_db_query_id,
                                                     std::move(mocked_results));
}

TEST_F(AutocompleteHistoryManagerTest,
       SuggestionsReturned_InvokeHandler_SingleValue) {
  int mocked_db_query_id = 100;

  auto suggestions_handler = std::make_unique<MockSuggestionsHandler>();

  std::vector<AutofillEntry> expected_values = {
      GetAutofillEntry(test_field_.name, u"SomePrefixOne")};

  std::unique_ptr<WDTypedResult> mocked_results =
      GetMockedDbResults(expected_values);

  EXPECT_CALL(*web_data_service_,
              GetFormValuesForElementName(test_field_.name, test_field_.value,
                                          _, autocomplete_manager_.get()))
      .WillOnce(Return(mocked_db_query_id));

  // Simulate request for suggestions.
  EXPECT_TRUE(autocomplete_manager_->OnGetSingleFieldSuggestions(
      AutofillSuggestionTriggerSource::kFormControlElementClicked, test_field_,
      autofill_client_, suggestions_handler->GetWeakPtr(),
      SuggestionsContext()));

  // Setting up mock to verify that DB response triggers a call to the handler's
  EXPECT_CALL(*suggestions_handler.get(),
              OnSuggestionsReturned(
                  test_field_.global_id(),
                  AutofillSuggestionTriggerSource::kFormControlElementClicked,
                  UnorderedElementsAre(Field(
                      &Suggestion::main_text,
                      Suggestion::Text(expected_values[0].key().value(),
                                       Suggestion::Text::IsPrimary(true))))));

  // Simulate response from DB.
  autocomplete_manager_->OnWebDataServiceRequestDone(mocked_db_query_id,
                                                     std::move(mocked_results));
}

// Tests that we are correctly forwarding the value of the
// `AutofillSuggestionTriggerSource` back to the handler.
TEST_F(AutocompleteHistoryManagerTest,
       SuggestionsReturned_InvokeHandler_PassesTriggerSource) {
  int mocked_db_query_id = 100;

  auto suggestions_handler = std::make_unique<MockSuggestionsHandler>();

  std::vector<AutofillEntry> expected_values = {
      GetAutofillEntry(test_field_.name, u"SomePrefixOne")};

  std::unique_ptr<WDTypedResult> mocked_results =
      GetMockedDbResults(expected_values);

  EXPECT_CALL(*web_data_service_,
              GetFormValuesForElementName(test_field_.name, test_field_.value,
                                          _, autocomplete_manager_.get()))
      .WillOnce(Return(mocked_db_query_id));

  // Simulate request for suggestions.
  EXPECT_TRUE(autocomplete_manager_->OnGetSingleFieldSuggestions(
      AutofillSuggestionTriggerSource::kFormControlElementClicked, test_field_,
      autofill_client_, suggestions_handler->GetWeakPtr(),
      SuggestionsContext()));

  // Setting up mock to verify that DB response triggers a call to the handler's
  EXPECT_CALL(*suggestions_handler.get(),
              OnSuggestionsReturned(
                  test_field_.global_id(),
                  AutofillSuggestionTriggerSource::kFormControlElementClicked,
                  UnorderedElementsAre(Field(
                      &Suggestion::main_text,
                      Suggestion::Text(expected_values[0].key().value(),
                                       Suggestion::Text::IsPrimary(true))))));

  // Simulate response from DB.
  autocomplete_manager_->OnWebDataServiceRequestDone(mocked_db_query_id,
                                                     std::move(mocked_results));
}

// Tests that we don't return any suggestion if we only have one suggestion that
// is case-sensitive equal to the given prefix.
TEST_F(AutocompleteHistoryManagerTest,
       SuggestionsReturned_InvokeHandler_SingleValue_EqualsPrefix) {
  int mocked_db_query_id = 100;

  auto suggestions_handler = std::make_unique<MockSuggestionsHandler>();

  std::vector<AutofillEntry> expected_values = {
      GetAutofillEntry(test_field_.name, test_field_.value)};

  std::unique_ptr<WDTypedResult> mocked_results =
      GetMockedDbResults(expected_values);

  EXPECT_CALL(*web_data_service_,
              GetFormValuesForElementName(test_field_.name, test_field_.value,
                                          _, autocomplete_manager_.get()))
      .WillOnce(Return(mocked_db_query_id));

  // Simulate request for suggestions.
  EXPECT_TRUE(autocomplete_manager_->OnGetSingleFieldSuggestions(
      AutofillSuggestionTriggerSource::kFormControlElementClicked, test_field_,
      autofill_client_, suggestions_handler->GetWeakPtr(),
      SuggestionsContext()));

  // Setting up mock to verify that DB response triggers a call to the handler's
  EXPECT_CALL(*suggestions_handler.get(),
              OnSuggestionsReturned(
                  test_field_.global_id(),
                  AutofillSuggestionTriggerSource::kFormControlElementClicked,
                  testing::Truly(IsEmptySuggestionVector)));

  // Simulate response from DB.
  autocomplete_manager_->OnWebDataServiceRequestDone(mocked_db_query_id,
                                                     std::move(mocked_results));
}

// Tests the case sensitivity of the unique suggestion equal to the prefix
// filter.
TEST_F(AutocompleteHistoryManagerTest,
       SuggestionsReturned_InvokeHandler_SingleValue_EqualsPrefix_DiffCase) {
  int mocked_db_query_id = 100;

  auto suggestions_handler = std::make_unique<MockSuggestionsHandler>();

  std::vector<AutofillEntry> expected_values = {
      GetAutofillEntry(test_field_.name, u"someprefix")};

  std::unique_ptr<WDTypedResult> mocked_results =
      GetMockedDbResults(expected_values);

  EXPECT_CALL(*web_data_service_,
              GetFormValuesForElementName(test_field_.name, test_field_.value,
                                          _, autocomplete_manager_.get()))
      .WillOnce(Return(mocked_db_query_id));

  // Simulate request for suggestions.
  EXPECT_TRUE(autocomplete_manager_->OnGetSingleFieldSuggestions(
      AutofillSuggestionTriggerSource::kFormControlElementClicked, test_field_,
      autofill_client_, suggestions_handler->GetWeakPtr(),
      SuggestionsContext()));

  // Setting up mock to verify that DB response triggers a call to the handler's
  EXPECT_CALL(*suggestions_handler.get(),
              OnSuggestionsReturned(
                  test_field_.global_id(),
                  AutofillSuggestionTriggerSource::kFormControlElementClicked,
                  UnorderedElementsAre(Field(
                      &Suggestion::main_text,
                      Suggestion::Text(expected_values[0].key().value(),
                                       Suggestion::Text::IsPrimary(true))))));

  // Simulate response from DB.
  autocomplete_manager_->OnWebDataServiceRequestDone(mocked_db_query_id,
                                                     std::move(mocked_results));
}

TEST_F(AutocompleteHistoryManagerTest,
       OnSingleFieldSuggestionSelected_Found_ShouldLogDays) {
  // Setting up by simulating that there was a query for autocomplete
  // suggestions, and that two values were found.
  int mocked_db_query_id = 100;

  auto suggestions_handler = std::make_unique<MockSuggestionsHandler>();
  std::u16string test_value = u"SomePrefixOne";
  std::u16string other_test_value = u"SomePrefixOne";
  int days_since_last_use = 10;

  std::vector<AutofillEntry> expected_values = {
      GetAutofillEntry(test_field_.name, test_value,
                       AutofillClock::Now() - base::Days(30),
                       AutofillClock::Now() - base::Days(days_since_last_use)),
      GetAutofillEntry(test_field_.name, other_test_value,
                       AutofillClock::Now() - base::Days(30),
                       AutofillClock::Now() - base::Days(days_since_last_use))};

  std::unique_ptr<WDTypedResult> mocked_results =
      GetMockedDbResults(expected_values);

  EXPECT_CALL(*web_data_service_,
              GetFormValuesForElementName(test_field_.name, test_field_.value,
                                          _, autocomplete_manager_.get()))
      .WillOnce(Return(mocked_db_query_id));

  EXPECT_CALL(*suggestions_handler.get(), OnSuggestionsReturned);

  // Simulate request for suggestions.
  EXPECT_TRUE(autocomplete_manager_->OnGetSingleFieldSuggestions(
      AutofillSuggestionTriggerSource::kFormControlElementClicked, test_field_,
      autofill_client_, suggestions_handler->GetWeakPtr(),
      SuggestionsContext()));

  // Simulate response from DB.
  autocomplete_manager_->OnWebDataServiceRequestDone(mocked_db_query_id,
                                                     std::move(mocked_results));

  base::HistogramTester histogram_tester;

  // Now simulate one autocomplete entry being selected, and expect a metric
  // being logged for that value alone.
  autocomplete_manager_->OnSingleFieldSuggestionSelected(
      test_value, PopupItemId::kAutocompleteEntry);

  histogram_tester.ExpectBucketCount("Autocomplete.DaysSinceLastUse",
                                     days_since_last_use, 1);
}

TEST_F(AutocompleteHistoryManagerTest,
       SuggestionsReturned_InvokeHandler_TwoRequests_OneHandler_Cancels) {
  int mocked_db_query_id_first = 100;
  int mocked_db_query_id_second = 101;

  auto suggestions_handler = std::make_unique<MockSuggestionsHandler>();

  std::vector<AutofillEntry> expected_values_first = {
      GetAutofillEntry(test_field_.name, u"SomePrefixOne")};

  std::vector<AutofillEntry> expected_values_second = {
      GetAutofillEntry(test_field_.name, u"SomePrefixTwo")};

  std::unique_ptr<WDTypedResult> mocked_results_first =
      GetMockedDbResults(expected_values_first);

  std::unique_ptr<WDTypedResult> mocked_results_second =
      GetMockedDbResults(expected_values_second);

  EXPECT_CALL(*web_data_service_,
              GetFormValuesForElementName(test_field_.name, test_field_.value,
                                          _, autocomplete_manager_.get()))
      .WillOnce(Return(mocked_db_query_id_first))
      .WillOnce(Return(mocked_db_query_id_second));

  // Simulate request for the first suggestions.
  EXPECT_TRUE(autocomplete_manager_->OnGetSingleFieldSuggestions(
      AutofillSuggestionTriggerSource::kFormControlElementClicked, test_field_,
      autofill_client_, suggestions_handler->GetWeakPtr(),
      SuggestionsContext()));

  // Simulate request for the second suggestions (this will cancel the first
  // one).
  EXPECT_CALL(*web_data_service_, CancelRequest(mocked_db_query_id_first))
      .Times(1);
  EXPECT_TRUE(autocomplete_manager_->OnGetSingleFieldSuggestions(
      AutofillSuggestionTriggerSource::kFormControlElementClicked, test_field_,
      autofill_client_, suggestions_handler->GetWeakPtr(),
      SuggestionsContext()));

  // Setting up mock to verify that we can get the second response first.
  EXPECT_CALL(*suggestions_handler.get(),
              OnSuggestionsReturned(
                  test_field_.global_id(),
                  AutofillSuggestionTriggerSource::kFormControlElementClicked,
                  UnorderedElementsAre(Field(
                      &Suggestion::main_text,
                      Suggestion::Text(expected_values_second[0].key().value(),
                                       Suggestion::Text::IsPrimary(true))))));

  // Simulate response from DB, second request comes back before.
  autocomplete_manager_->OnWebDataServiceRequestDone(
      mocked_db_query_id_second, std::move(mocked_results_second));

  // Setting up mock to verify that the handler doesn't get called for the first
  // request, which was cancelled.
  EXPECT_CALL(
      *suggestions_handler.get(),
      OnSuggestionsReturned(
          test_field_.global_id(),
          AutofillSuggestionTriggerSource::kFormControlElementClicked, _))
      .Times(0);

  // Simulate response from DB, first request comes back after.
  autocomplete_manager_->OnWebDataServiceRequestDone(
      mocked_db_query_id_first, std::move(mocked_results_first));
}

TEST_F(AutocompleteHistoryManagerTest,
       SuggestionsReturned_InvokeHandler_TwoRequests_TwoHandlers) {
  int mocked_db_query_id_first = 100;
  int mocked_db_query_id_second = 101;

  auto suggestions_handler_first = std::make_unique<MockSuggestionsHandler>();
  auto suggestions_handler_second = std::make_unique<MockSuggestionsHandler>();

  std::vector<AutofillEntry> expected_values_first = {
      GetAutofillEntry(test_field_.name, u"SomePrefixOne")};

  std::vector<AutofillEntry> expected_values_second = {
      GetAutofillEntry(test_field_.name, u"SomePrefixTwo")};

  std::unique_ptr<WDTypedResult> mocked_results_first =
      GetMockedDbResults(expected_values_first);

  std::unique_ptr<WDTypedResult> mocked_results_second =
      GetMockedDbResults(expected_values_second);

  EXPECT_CALL(*web_data_service_,
              GetFormValuesForElementName(test_field_.name, test_field_.value,
                                          _, autocomplete_manager_.get()))
      .WillOnce(Return(mocked_db_query_id_first))
      .WillOnce(Return(mocked_db_query_id_second));

  // Simulate request for the first suggestions.
  EXPECT_TRUE(autocomplete_manager_->OnGetSingleFieldSuggestions(
      AutofillSuggestionTriggerSource::kFormControlElementClicked, test_field_,
      autofill_client_, suggestions_handler_first->GetWeakPtr(),
      SuggestionsContext()));

  // Simulate request for the second suggestions.
  EXPECT_TRUE(autocomplete_manager_->OnGetSingleFieldSuggestions(
      AutofillSuggestionTriggerSource::kFormControlElementClicked, test_field_,
      autofill_client_, suggestions_handler_second->GetWeakPtr(),
      SuggestionsContext()));

  // Setting up mock to verify that we get the second response first.
  EXPECT_CALL(*suggestions_handler_second.get(),
              OnSuggestionsReturned(
                  test_field_.global_id(),
                  AutofillSuggestionTriggerSource::kFormControlElementClicked,
                  UnorderedElementsAre(Field(
                      &Suggestion::main_text,
                      Suggestion::Text(expected_values_second[0].key().value(),
                                       Suggestion::Text::IsPrimary(true))))));

  // Simulate response from DB, second request comes back before.
  autocomplete_manager_->OnWebDataServiceRequestDone(
      mocked_db_query_id_second, std::move(mocked_results_second));

  // Setting up mock to verify that we get the first response second.
  EXPECT_CALL(*suggestions_handler_first.get(),
              OnSuggestionsReturned(
                  test_field_.global_id(),
                  AutofillSuggestionTriggerSource::kFormControlElementClicked,
                  UnorderedElementsAre(Field(
                      &Suggestion::main_text,
                      Suggestion::Text(expected_values_first[0].key().value(),
                                       Suggestion::Text::IsPrimary(true))))));

  // Simulate response from DB, first request comes back after.
  autocomplete_manager_->OnWebDataServiceRequestDone(
      mocked_db_query_id_first, std::move(mocked_results_first));
}

TEST_F(AutocompleteHistoryManagerTest,
       SuggestionsReturned_CancelOne_ReturnOne) {
  // Initialize variables for the first handler, which is the one that will be
  // cancelled.
  auto suggestions_handler_one = std::make_unique<MockSuggestionsHandler>();
  int mocked_db_query_id_one = 100;
  std::vector<AutofillEntry> expected_values_one = {
      GetAutofillEntry(test_field_.name, u"SomePrefixOne")};
  std::unique_ptr<WDTypedResult> mocked_results_one =
      GetMockedDbResults(expected_values_one);

  // Initialize variables for the second handler, which will be fulfilled.
  auto suggestions_handler_two = std::make_unique<MockSuggestionsHandler>();
  int mocked_db_query_id_two = 101;
  std::vector<AutofillEntry> expected_values_two = {
      GetAutofillEntry(test_field_.name, u"SomePrefixTwo")};
  std::unique_ptr<WDTypedResult> mocked_results_two =
      GetMockedDbResults(expected_values_two);

  // Simulate first handler request for autocomplete suggestions.
  EXPECT_CALL(*web_data_service_,
              GetFormValuesForElementName(test_field_.name, test_field_.value,
                                          _, autocomplete_manager_.get()))
      .WillOnce(Return(mocked_db_query_id_one))
      .WillOnce(Return(mocked_db_query_id_two));

  EXPECT_TRUE(autocomplete_manager_->OnGetSingleFieldSuggestions(
      AutofillSuggestionTriggerSource::kFormControlElementClicked, test_field_,
      autofill_client_, suggestions_handler_one->GetWeakPtr(),
      SuggestionsContext()));

  // Simulate second handler request for autocomplete suggestions.
  EXPECT_TRUE(autocomplete_manager_->OnGetSingleFieldSuggestions(
      AutofillSuggestionTriggerSource::kFormControlElementClicked, test_field_,
      autofill_client_, suggestions_handler_two->GetWeakPtr(),
      SuggestionsContext()));

  // Simulate first handler cancelling its request.
  EXPECT_CALL(*web_data_service_, CancelRequest(mocked_db_query_id_one))
      .Times(1);
  autocomplete_manager_->CancelPendingQueries(suggestions_handler_one.get());

  // Simulate second handler receiving the suggestions.
  EXPECT_CALL(*suggestions_handler_two.get(),
              OnSuggestionsReturned(
                  test_field_.global_id(),
                  AutofillSuggestionTriggerSource::kFormControlElementClicked,
                  UnorderedElementsAre(Field(
                      &Suggestion::main_text,
                      Suggestion::Text(expected_values_two[0].key().value(),
                                       Suggestion::Text::IsPrimary(true))))));
  autocomplete_manager_->OnWebDataServiceRequestDone(
      mocked_db_query_id_two, std::move(mocked_results_two));

  // Make sure first handler is not called when the DB responds.
  EXPECT_CALL(
      *suggestions_handler_one.get(),
      OnSuggestionsReturned(
          test_field_.global_id(),
          AutofillSuggestionTriggerSource::kFormControlElementClicked, _))
      .Times(0);
  autocomplete_manager_->OnWebDataServiceRequestDone(
      mocked_db_query_id_one, std::move(mocked_results_one));
}

// Verify that no autocomplete suggestion is returned for a textarea.
TEST_F(AutocompleteHistoryManagerTest, NoAutocompleteSuggestionsForTextarea) {
  FormData form;
  form.name = u"MyForm";
  form.url = GURL("http://myform.com/form.html");
  form.action = GURL("http://myform.com/submit.html");

  FormFieldData field =
      CreateTestFormField("Address", "address", "", "textarea");

  auto suggestions_handler = std::make_unique<MockSuggestionsHandler>();
  EXPECT_CALL(*suggestions_handler.get(),
              OnSuggestionsReturned(
                  field.global_id(),
                  AutofillSuggestionTriggerSource::kFormControlElementClicked,
                  testing::Truly(IsEmptySuggestionVector)));

  EXPECT_TRUE(autocomplete_manager_->OnGetSingleFieldSuggestions(
      AutofillSuggestionTriggerSource::kFormControlElementClicked, field,
      autofill_client_, suggestions_handler->GetWeakPtr(),
      SuggestionsContext()));
}

TEST_F(AutocompleteHistoryManagerTest, DestructorCancelsRequests) {
  int mocked_db_query_id_first = 100;
  int mocked_db_query_id_second = 101;

  auto suggestions_handler_first = std::make_unique<MockSuggestionsHandler>();
  auto suggestions_handler_second = std::make_unique<MockSuggestionsHandler>();

  EXPECT_CALL(*web_data_service_,
              GetFormValuesForElementName(test_field_.name, test_field_.value,
                                          _, autocomplete_manager_.get()))
      .WillOnce(Return(mocked_db_query_id_first))
      .WillOnce(Return(mocked_db_query_id_second));

  // Simulate request for the first suggestions.
  EXPECT_TRUE(autocomplete_manager_->OnGetSingleFieldSuggestions(
      AutofillSuggestionTriggerSource::kFormControlElementClicked, test_field_,
      autofill_client_, suggestions_handler_first->GetWeakPtr(),
      SuggestionsContext()));

  // Simulate request for the second suggestions.
  EXPECT_TRUE(autocomplete_manager_->OnGetSingleFieldSuggestions(
      AutofillSuggestionTriggerSource::kFormControlElementClicked, test_field_,
      autofill_client_, suggestions_handler_second->GetWeakPtr(),
      SuggestionsContext()));

  // Expect cancel calls for both requests.
  EXPECT_CALL(*web_data_service_, CancelRequest(mocked_db_query_id_first))
      .Times(1);
  EXPECT_CALL(*web_data_service_, CancelRequest(mocked_db_query_id_second))
      .Times(1);

  autocomplete_manager_.reset();

  EXPECT_TRUE(PendingQueriesEmpty());
}

// Tests that a successful Autocomplete Retention Policy cleanup will
// overwrite the last cleaned major version preference.
TEST_F(AutocompleteHistoryManagerTest, EntriesCleanup_Success) {
  // Set Pref major version to some impossible number.
  prefs_->SetInteger(prefs::kAutocompleteLastVersionRetentionPolicy, -1);

  EXPECT_EQ(-1,
            prefs_->GetInteger(prefs::kAutocompleteLastVersionRetentionPolicy));

  size_t cleanup_result = 10;
  base::HistogramTester histogram_tester;

  autocomplete_manager_->OnWebDataServiceRequestDone(
      1, std::make_unique<WDResult<size_t>>(AUTOFILL_CLEANUP_RESULT,
                                            cleanup_result));

  EXPECT_EQ(CHROME_VERSION_MAJOR,
            prefs_->GetInteger(prefs::kAutocompleteLastVersionRetentionPolicy));
}

// Tests that AutocompleteHistoryManager::OnWebDataServiceRequestDone does not
// crash on empty results.
TEST_F(AutocompleteHistoryManagerTest, EmptyResult_DoesNotCrash) {
  auto empty_unique_ptr = std::unique_ptr<WDTypedResult>(nullptr);

  // The expectation in this test is that the following call doesn't crash.
  autocomplete_manager_->OnWebDataServiceRequestDone(
      1, std::move(empty_unique_ptr));
}

}  // namespace autofill
