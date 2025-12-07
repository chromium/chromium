// Copyright 2013 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/autofill/core/browser/single_field_fillers/autocomplete/autocomplete_history_manager.h"

#include <string>
#include <vector>

#include "base/functional/callback_helpers.h"
#include "base/memory/ref_counted.h"
#include "base/memory/weak_ptr.h"
#include "base/run_loop.h"
#include "base/strings/utf_string_conversions.h"
#include "base/synchronization/waitable_event.h"
#include "base/test/gmock_callback_support.h"
#include "base/test/metrics/histogram_tester.h"
#include "base/test/mock_callback.h"
#include "base/test/scoped_feature_list.h"
#include "base/test/task_environment.h"
#include "base/time/time.h"
#include "base/version_info/version_info.h"
#include "build/build_config.h"
#include "components/autofill/core/browser/foundations/test_autofill_client.h"
#include "components/autofill/core/browser/test_utils/autofill_test_utils.h"
#include "components/autofill/core/browser/webdata/autocomplete/autocomplete_entry.h"
#include "components/autofill/core/browser/webdata/autofill_webdata_service.h"
#include "components/autofill/core/browser/webdata/mock_autofill_webdata_service.h"
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
constexpr int kTestDbQuryId = 100;

using OnSuggestionsReturnedCallback =
    SingleFieldFillRouter::OnSuggestionsReturnedCallback;
using ::autofill::test::CreateTestFormField;
using ::testing::_;
using ::testing::AllOf;
using ::testing::Eq;
using ::testing::Field;
using ::testing::IsEmpty;
using ::testing::IsTrue;
using ::testing::Return;
using ::testing::UnorderedElementsAre;

using MockSuggestionsReturnedCallback =
    base::MockCallback<OnSuggestionsReturnedCallback>;
using DbCallback = base::OnceCallback<void(WebDataServiceBase::Handle,
                                           std::unique_ptr<WDTypedResult>)>;

auto HasSingleSuggestionWithMainText(std::u16string text) {
  return ElementsAre(
      Field(&Suggestion::main_text,
            AllOf(Field(&Suggestion::Text::value, text),
                  Field(&Suggestion::Text::is_primary, IsTrue()))));
}

class MockAutofillClient : public TestAutofillClient {
 public:
  MOCK_METHOD(AutocompleteHistoryManager*,
              GetAutocompleteHistoryManager,
              (),
              (override));
};

}  // namespace
// The anonymous namespace needs to end here because of `friend`ships between
// the tests and the production code.

class AutocompleteHistoryManagerTest : public testing::Test {
 protected:
  AutocompleteHistoryManagerTest() = default;

  void SetUp() override {
    prefs_ = test::PrefServiceForTesting();

    // Mock such that we don't trigger the cleanup.
    prefs_->SetInteger(prefs::kAutocompleteLastVersionRetentionPolicy,
                       version_info::GetMajorVersionNumberAsInt());

    // Set time to some arbitrary date.
    task_environment_.AdvanceClock(
        base::Time::FromSecondsSinceUnixEpoch(1546889367) - base::Time::Now());
    web_data_service_ = base::MakeRefCounted<MockAutofillWebDataService>();
    autocomplete_manager_ = std::make_unique<AutocompleteHistoryManager>();
    autocomplete_manager_->Init(web_data_service_, prefs_.get(), false);
    ON_CALL(autofill_client_, GetAutocompleteHistoryManager())
        .WillByDefault(Return(autocomplete_manager_.get()));
    test_field_ =
        CreateTestFormField(/*label=*/"", "Some Field Name", "SomePrefix",
                            FormControlType::kInputText);
    test_form_data_.set_url(GURL("https://www.foo.com"));
    test_form_data_.set_fields({test_field_});
  }

  void TearDown() override {
    // Ensure there are no left-over entries in the map (leak check).
    EXPECT_TRUE(PendingQueryEmpty());

    autocomplete_manager_.reset();
  }

  bool PendingQueryEmpty() {
    return !autocomplete_manager_ ||
           !autocomplete_manager_->suggestion_generator_ ||
           !autocomplete_manager_->suggestion_generator_->HasPendingQuery();
  }

  std::unique_ptr<WDTypedResult> GetMockedDbResults(
      std::vector<AutocompleteEntry> values) {
    return std::make_unique<WDResult<std::vector<AutocompleteEntry>>>(
        AUTOFILL_VALUE_RESULT, values);
  }

  AutocompleteEntry GetAutocompleteEntry(
      const std::u16string& name,
      const std::u16string& value,
      base::Time date_created = base::Time::Now(),
      base::Time date_last_used = base::Time::Now()) {
    return AutocompleteEntry(AutocompleteKey(name, value), date_created,
                             date_last_used);
  }

  base::test::SingleThreadTaskEnvironment task_environment_{
      base::test::TaskEnvironment::TimeSource::MOCK_TIME};
  test::AutofillUnitTestEnvironment autofill_test_environment_;
  MockAutofillClient autofill_client_;
  scoped_refptr<MockAutofillWebDataService> web_data_service_;
  std::unique_ptr<AutocompleteHistoryManager> autocomplete_manager_;
  std::unique_ptr<PrefService> prefs_;
  FormFieldData test_field_;
  FormData test_form_data_;
};

// Tests that credit card numbers are not sent to the WebDatabase to be saved.
TEST_F(AutocompleteHistoryManagerTest, CreditCardNumberValue) {
  FormData form;
  form.set_name(u"MyForm");
  form.set_url(GURL("http://myform.com/form.html"));
  form.set_action(GURL("http://myform.com/submit.html"));

  // Valid Visa credit card number pulled from the paypal help site.
  FormFieldData valid_cc;
  valid_cc.set_label(u"Credit Card");
  valid_cc.set_name(u"ccnum");
  valid_cc.set_value(u"4012888888881881");
  valid_cc.set_properties_mask(valid_cc.properties_mask() | kUserTyped);
  valid_cc.set_form_control_type(FormControlType::kInputText);
  form.set_fields({valid_cc});

  EXPECT_CALL(*(web_data_service_.get()), AddFormFields(_)).Times(0);
  autocomplete_manager_->OnWillSubmitFormWithFields(
      form.fields(),
      /*is_autocomplete_enabled=*/true);
}

// Contrary test to AutocompleteHistoryManagerTest.CreditCardNumberValue.  The
// value being submitted is not a valid credit card number, so it will be sent
// to the WebDatabase to be saved.
TEST_F(AutocompleteHistoryManagerTest, NonCreditCardNumberValue) {
  FormData form;
  form.set_name(u"MyForm");
  form.set_url(GURL("http://myform.com/form.html"));
  form.set_action(GURL("http://myform.com/submit.html"));

  // Invalid credit card number.
  FormFieldData invalid_cc;
  invalid_cc.set_label(u"Credit Card");
  invalid_cc.set_name(u"ccnum");
  invalid_cc.set_value(u"4580123456789012");
  invalid_cc.set_properties_mask(invalid_cc.properties_mask() | kUserTyped);
  invalid_cc.set_form_control_type(FormControlType::kInputText);
  form.set_fields({invalid_cc});

  EXPECT_CALL(*(web_data_service_.get()), AddFormFields(_));
  autocomplete_manager_->OnWillSubmitFormWithFields(
      form.fields(),
      /*is_autocomplete_enabled=*/true);
}

// Tests that SSNs are not sent to the WebDatabase to be saved.
TEST_F(AutocompleteHistoryManagerTest, SSNValue) {
  FormData form;
  form.set_name(u"MyForm");
  form.set_url(GURL("http://myform.com/form.html"));
  form.set_action(GURL("http://myform.com/submit.html"));

  FormFieldData ssn;
  ssn.set_label(u"Social Security Number");
  ssn.set_name(u"ssn");
  ssn.set_value(u"078-05-1120");
  ssn.set_properties_mask(ssn.properties_mask() | kUserTyped);
  ssn.set_form_control_type(FormControlType::kInputText);
  form.set_fields({ssn});

  EXPECT_CALL(*web_data_service_, AddFormFields(_)).Times(0);
  autocomplete_manager_->OnWillSubmitFormWithFields(
      form.fields(),
      /*is_autocomplete_enabled=*/true);
}

// Verify that autocomplete text is saved for search fields.
TEST_F(AutocompleteHistoryManagerTest, SearchField) {
  FormData form;
  form.set_name(u"MyForm");
  form.set_url(GURL("http://myform.com/form.html"));
  form.set_action(GURL("http://myform.com/submit.html"));

  // Search field.
  FormFieldData search_field;
  search_field.set_label(u"Search");
  search_field.set_name(u"search");
  search_field.set_value(u"my favorite query");
  search_field.set_properties_mask(search_field.properties_mask() | kUserTyped);
  search_field.set_form_control_type(FormControlType::kInputSearch);
  form.set_fields({search_field});

  EXPECT_CALL(*(web_data_service_.get()), AddFormFields(_));
  autocomplete_manager_->OnWillSubmitFormWithFields(
      form.fields(),
      /*is_autocomplete_enabled=*/true);
}

TEST_F(AutocompleteHistoryManagerTest, AutocompleteFeatureOff) {
  FormData form;
  form.set_name(u"MyForm");
  form.set_url(GURL("http://myform.com/form.html"));
  form.set_action(GURL("http://myform.com/submit.html"));

  // Search field.
  FormFieldData search_field;
  search_field.set_label(u"Search");
  search_field.set_name(u"search");
  search_field.set_value(u"my favorite query");
  search_field.set_properties_mask(search_field.properties_mask() | kUserTyped);
  search_field.set_form_control_type(FormControlType::kInputSearch);
  form.set_fields({search_field});

  EXPECT_CALL(*(web_data_service_.get()), AddFormFields(_)).Times(0);
  autocomplete_manager_->OnWillSubmitFormWithFields(
      form.fields(),
      /*is_autocomplete_enabled=*/false);
}

// Verify that we don't save invalid values in Autocomplete.
TEST_F(AutocompleteHistoryManagerTest, InvalidValues) {
  FormData form;
  form.set_name(u"MyForm");
  form.set_url(GURL("http://myform.com/form.html"));
  form.set_action(GURL("http://myform.com/submit.html"));

  auto make_field = [](std::u16string label, std::u16string name,
                       std::u16string value) {
    FormFieldData f;
    f.set_label(label);
    f.set_name(name);
    f.set_value(value);
    f.set_properties_mask(kUserTyped);
    f.set_form_control_type(FormControlType::kInputSearch);
    return f;
  };

  form.set_fields({make_field(u"Search", u"search", u""),
                   make_field(u"Search2", u"other search", u" "),
                   make_field(u"Search3", u"other search", u"      ")});

  EXPECT_CALL(*(web_data_service_.get()), AddFormFields(_)).Times(0);
  autocomplete_manager_->OnWillSubmitFormWithFields(
      form.fields(),
      /*is_autocomplete_enabled=*/true);
}

// Tests that text entered into fields specifying autocomplete="off" is not sent
// to the WebDatabase to be saved. Note this is also important as the mechanism
// for preventing CVCs from being saved.
// See BrowserAutofillManagerTest.DontSaveCvcInAutocompleteHistory
TEST_F(AutocompleteHistoryManagerTest, FieldWithAutocompleteOff) {
  FormData form;
  form.set_name(u"MyForm");
  form.set_url(GURL("http://myform.com/form.html"));
  form.set_action(GURL("http://myform.com/submit.html"));

  // Field specifying autocomplete="off".
  FormFieldData field;
  field.set_label(u"Something esoteric");
  field.set_name(u"esoterica");
  field.set_value(u"a truly esoteric value, I assure you");
  field.set_properties_mask(field.properties_mask() | kUserTyped);
  field.set_form_control_type(FormControlType::kInputText);
  field.set_should_autocomplete(false);
  form.set_fields({field});

  EXPECT_CALL(*web_data_service_, AddFormFields(_)).Times(0);
  autocomplete_manager_->OnWillSubmitFormWithFields(
      form.fields(),
      /*is_autocomplete_enabled=*/true);
}

// Shouldn't save entries when in Incognito mode.
TEST_F(AutocompleteHistoryManagerTest, Incognito) {
  autocomplete_manager_->Init(web_data_service_, prefs_.get(),
                              /*is_off_the_record_=*/true);
  FormData form;
  form.set_name(u"MyForm");
  form.set_url(GURL("http://myform.com/form.html"));
  form.set_action(GURL("http://myform.com/submit.html"));

  // Search field.
  FormFieldData search_field;
  search_field.set_label(u"Search");
  search_field.set_name(u"search");
  search_field.set_value(u"my favorite query");
  search_field.set_properties_mask(search_field.properties_mask() | kUserTyped);
  search_field.set_form_control_type(FormControlType::kInputSearch);
  form.set_fields({search_field});

  EXPECT_CALL(*web_data_service_, AddFormFields(_)).Times(0);
  autocomplete_manager_->OnWillSubmitFormWithFields(
      form.fields(),
      /*is_autocomplete_enabled=*/true);
}

#if !BUILDFLAG(IS_IOS)
// Tests that fields that are no longer focusable but still have user typed
// input are sent to the WebDatabase to be saved. Will not work for iOS
// because |properties_mask| is not set on iOS.
TEST_F(AutocompleteHistoryManagerTest, UserInputNotFocusable) {
  FormData form;
  form.set_name(u"MyForm");
  form.set_url(GURL("http://myform.com/form.html"));
  form.set_action(GURL("http://myform.com/submit.html"));

  // Search field.
  FormFieldData search_field;
  search_field.set_label(u"Search");
  search_field.set_name(u"search");
  search_field.set_value(u"my favorite query");
  search_field.set_form_control_type(FormControlType::kInputSearch);
  search_field.set_properties_mask(search_field.properties_mask() | kUserTyped);
  search_field.set_is_focusable(false);
  form.set_fields({search_field});

  EXPECT_CALL(*(web_data_service_.get()), AddFormFields(_));
  autocomplete_manager_->OnWillSubmitFormWithFields(
      form.fields(),
      /*is_autocomplete_enabled=*/true);
}
#endif

// Tests that text entered into presentation fields is not sent to the
// WebDatabase to be saved.
TEST_F(AutocompleteHistoryManagerTest, PresentationField) {
  FormData form;
  form.set_name(u"MyForm");
  form.set_url(GURL("http://myform.com/form.html"));
  form.set_action(GURL("http://myform.com/submit.html"));

  // Presentation field.
  FormFieldData field;
  field.set_label(u"Something esoteric");
  field.set_name(u"esoterica");
  field.set_value(u"a truly esoteric value, I assure you");
  field.set_properties_mask(field.properties_mask() | kUserTyped);
  field.set_form_control_type(FormControlType::kInputText);
  field.set_role(FormFieldData::RoleAttribute::kPresentation);
  form.set_fields({field});

  EXPECT_CALL(*web_data_service_, AddFormFields(_)).Times(0);
  autocomplete_manager_->OnWillSubmitFormWithFields(
      form.fields(),
      /*is_autocomplete_enabled=*/true);
}

// Tests that the Init function will trigger the Autocomplete Retention Policy
// cleanup if the flag is enabled, we're not in OTR and it hadn't run in the
// current major version.
TEST_F(AutocompleteHistoryManagerTest, Init_TriggersCleanup) {
  // Set the retention policy cleanup to a past major version.
  prefs_->SetInteger(prefs::kAutocompleteLastVersionRetentionPolicy,
                     version_info::GetMajorVersionNumberAsInt() - 1);

  EXPECT_CALL(*web_data_service_, RemoveExpiredAutocompleteEntries).Times(1);
  autocomplete_manager_->Init(web_data_service_, prefs_.get(),
                              /*is_off_the_record=*/false);
}

// Tests that the Init function will not trigger the Autocomplete Retention
// Policy when running in OTR.
TEST_F(AutocompleteHistoryManagerTest, Init_OTR_Not_TriggersCleanup) {
  // Set the retention policy cleanup to a past major version.
  prefs_->SetInteger(prefs::kAutocompleteLastVersionRetentionPolicy,
                     version_info::GetMajorVersionNumberAsInt() - 1);

  EXPECT_CALL(*web_data_service_, RemoveExpiredAutocompleteEntries).Times(0);
  autocomplete_manager_->Init(web_data_service_, prefs_.get(),
                              /*is_off_the_record=*/true);
}

// Tests that the Init function will not crash even if we don't have a DB.
TEST_F(AutocompleteHistoryManagerTest, Init_NullDB_NoCrash) {
  // Set the retention policy cleanup to a past major version.
  prefs_->SetInteger(prefs::kAutocompleteLastVersionRetentionPolicy,
                     version_info::GetMajorVersionNumberAsInt() - 1);

  EXPECT_CALL(*web_data_service_, RemoveExpiredAutocompleteEntries).Times(0);
  autocomplete_manager_->Init(nullptr, prefs_.get(),
                              /*is_off_the_record=*/false);
}

// Tests that the Init function will not trigger the Autocomplete Retention
// Policy when running in a major version that was already cleaned.
TEST_F(AutocompleteHistoryManagerTest,
       Init_SameMajorVersion_Not_TriggersCleanup) {
  // Set the retention policy cleanup to the current major version.
  prefs_->SetInteger(prefs::kAutocompleteLastVersionRetentionPolicy,
                     version_info::GetMajorVersionNumberAsInt());

  EXPECT_CALL(*web_data_service_, RemoveExpiredAutocompleteEntries).Times(0);
  autocomplete_manager_->Init(web_data_service_, prefs_.get(),
                              /*is_off_the_record=*/false);
}

// Make sure suggestions are not returned if the field should not autocomplete.
TEST_F(AutocompleteHistoryManagerTest,
       OnGetSingleFieldSuggestions_FieldShouldNotAutocomplete) {
  test_field_.set_should_autocomplete(false);
  EXPECT_CALL(*web_data_service_,
              GetFormValuesForElementName(test_field_.name(),
                                          test_field_.value(), _, _))
      .Times(0);

  // Setting up mock to verify that call to the handler's OnSuggestionsReturned
  // is triggered with no suggestions.
  base::RunLoop run_loop;
  MockSuggestionsReturnedCallback mock_callback;
  EXPECT_CALL(mock_callback, Run(test_field_.global_id(), IsEmpty()))
      .WillOnce(base::test::RunClosure(run_loop.QuitClosure()));

  // Simulate request for suggestions.
  autocomplete_manager_->OnGetSingleFieldSuggestions(
      test_form_data_, /*form_structure=*/nullptr, test_field_,
      /*trigger_autofill_field=*/nullptr, autofill_client_,
      mock_callback.Get());
  run_loop.Run();
}

// Make sure our handler is called at the right time.
TEST_F(AutocompleteHistoryManagerTest,
       SuggestionsReturned_InvokeHandler_Empty) {
  std::vector<AutocompleteEntry> expected_values;

  std::unique_ptr<WDTypedResult> mocked_results =
      GetMockedDbResults(expected_values);

  DbCallback db_callback;
  EXPECT_CALL(*web_data_service_,
              GetFormValuesForElementName(test_field_.name(),
                                          test_field_.value(), _, _))
      .WillOnce([&](auto, auto, int, DbCallback callback) {
        db_callback = std::move(callback);
        return kTestDbQuryId;
      });

  MockSuggestionsReturnedCallback mock_callback;
  base::RunLoop run_loop;
  EXPECT_CALL(mock_callback, Run(test_field_.global_id(), IsEmpty()))
      .WillOnce(base::test::RunClosure(run_loop.QuitClosure()));

  autocomplete_manager_->OnGetSingleFieldSuggestions(
      test_form_data_, /*form_structure=*/nullptr, test_field_,
      /*trigger_autofill_field=*/nullptr, autofill_client_,
      mock_callback.Get());

  ASSERT_FALSE(db_callback.is_null());
  std::move(db_callback).Run(kTestDbQuryId, std::move(mocked_results));
  run_loop.Run();
}

// Tests that no suggestions are queried if the field name is filtered because
// it has a meaningless sub string that is allowed for sub string matches.
TEST_F(AutocompleteHistoryManagerTest,
       DoQuerySuggestionsForMeaninglessFieldNames_FilterSubStringName) {
  test_field_ = CreateTestFormField(/*label=*/"", "payment_cvv_info",
                                    /*value=*/"", FormControlType::kInputText);

  // Only expect a call when the name is not filtered out.
  EXPECT_CALL(*web_data_service_,
              GetFormValuesForElementName(test_field_.name(),
                                          test_field_.value(), _, _))
      .Times(0);

  MockSuggestionsReturnedCallback mock_callback;
  base::RunLoop run_loop;
  EXPECT_CALL(mock_callback, Run(test_field_.global_id(), IsEmpty()))
      .WillOnce(base::test::RunClosure(run_loop.QuitClosure()));
  // Simulate request for suggestions.
  autocomplete_manager_->OnGetSingleFieldSuggestions(
      test_form_data_, /*form_structure=*/nullptr, test_field_,
      /*trigger_autofill_field=*/nullptr, autofill_client_,
      mock_callback.Get());
  run_loop.Run();
}

// Tests that no suggestions are queried if the field name is filtered because
// it has a meaningless name.
TEST_F(AutocompleteHistoryManagerTest,
       DoQuerySuggestionsForMeaninglessFieldNames_FilterName) {
  test_field_ = CreateTestFormField(/*label=*/"", "input_123", /*value=*/"",
                                    FormControlType::kInputText);

  // Only expect a call when the name is not filtered out.
  EXPECT_CALL(*web_data_service_,
              GetFormValuesForElementName(test_field_.name(),
                                          test_field_.value(), _, _))
      .Times(0);

  MockSuggestionsReturnedCallback mock_callback;
  base::RunLoop run_loop;
  EXPECT_CALL(mock_callback, Run(test_field_.global_id(), IsEmpty()))
        .WillOnce(base::test::RunClosure(run_loop.QuitClosure()));
  // Simulate request for suggestions.
  autocomplete_manager_->OnGetSingleFieldSuggestions(
      test_form_data_, /*form_structure=*/nullptr, test_field_,
      /*trigger_autofill_field=*/nullptr, autofill_client_,
      mock_callback.Get());
  run_loop.Run();
}

// Tests that the suggestions are queried if the field has meaningless substring
// which is not allowed for substring matches.
TEST_F(AutocompleteHistoryManagerTest,
       DoQuerySuggestionsForMeaninglessFieldNames_PassNameWithSubstring) {
  test_field_ = CreateTestFormField(/*label=*/"", "foOTPace", /*value=*/"",
                                    FormControlType::kInputText);

  std::vector<AutocompleteEntry> expected_values = {
      GetAutocompleteEntry(test_field_.name(), u"SomePrefixOne")};
  std::unique_ptr<WDTypedResult> mocked_results =
      GetMockedDbResults(expected_values);

  // Expect a call because the name is not filtered.
  DbCallback db_callback;
  EXPECT_CALL(*web_data_service_,
              GetFormValuesForElementName(test_field_.name(),
                                          test_field_.value(), _, _))
      .WillOnce([&](auto, auto, int, DbCallback callback) {
        db_callback = std::move(callback);
        return kTestDbQuryId;
      });

  base::RunLoop run_loop;
  MockSuggestionsReturnedCallback mock_callback;
  EXPECT_CALL(mock_callback,
              Run(test_field_.global_id(),
                  HasSingleSuggestionWithMainText(u"SomePrefixOne")))
      .WillOnce(base::test::RunClosure(run_loop.QuitClosure()));

  // Simulate request for suggestions.
  autocomplete_manager_->OnGetSingleFieldSuggestions(
      test_form_data_, /*form_structure=*/nullptr, test_field_,
      /*trigger_autofill_field=*/nullptr, autofill_client_,
      mock_callback.Get());

  ASSERT_FALSE(db_callback.is_null());
  std::move(db_callback).Run(kTestDbQuryId, std::move(mocked_results));
  run_loop.Run();
}

// Tests that the suggestions are queried if the field name is not filtered
// because the field's name is meaningful.
TEST_F(AutocompleteHistoryManagerTest,
       DoQuerySuggestionsForMeaninglessFieldNames_PassName) {
  test_field_ = CreateTestFormField(/*label=*/"", "addressline_1", /*value=*/"",
                                    FormControlType::kInputText);

  std::vector<AutocompleteEntry> expected_values = {
      GetAutocompleteEntry(test_field_.name(), u"SomePrefixOne")};
  std::unique_ptr<WDTypedResult> mocked_results =
      GetMockedDbResults(expected_values);

  // Expect a call because the name is not filtered.
  DbCallback db_callback;
  EXPECT_CALL(*web_data_service_,
              GetFormValuesForElementName(test_field_.name(),
                                          test_field_.value(), _, _))
      .WillOnce([&](auto, auto, int, DbCallback callback) {
        db_callback = std::move(callback);
        return kTestDbQuryId;
      });

  base::RunLoop run_loop;
  MockSuggestionsReturnedCallback mock_callback;
  EXPECT_CALL(mock_callback,
              Run(test_field_.global_id(),
                  HasSingleSuggestionWithMainText(u"SomePrefixOne")))
      .WillOnce(base::test::RunClosure(run_loop.QuitClosure()));

  // Simulate request for suggestions.
  autocomplete_manager_->OnGetSingleFieldSuggestions(
      test_form_data_, /*form_structure=*/nullptr, test_field_,
      /*trigger_autofill_field=*/nullptr, autofill_client_,
      mock_callback.Get());

  ASSERT_FALSE(db_callback.is_null());
  std::move(db_callback).Run(kTestDbQuryId, std::move(mocked_results));
  run_loop.Run();
}

TEST_F(AutocompleteHistoryManagerTest,
       SuggestionsReturned_InvokeHandler_SingleValue) {
  std::vector<AutocompleteEntry> expected_values = {
      GetAutocompleteEntry(test_field_.name(), u"SomePrefixOne")};
  std::unique_ptr<WDTypedResult> mocked_results =
      GetMockedDbResults(expected_values);

  DbCallback db_callback;
  EXPECT_CALL(*web_data_service_,
              GetFormValuesForElementName(test_field_.name(),
                                          test_field_.value(), _, _))
      .WillOnce([&](auto, auto, int, DbCallback callback) {
        db_callback = std::move(callback);
        return kTestDbQuryId;
      });

  base::RunLoop run_loop;
  MockSuggestionsReturnedCallback mock_callback;
  EXPECT_CALL(mock_callback,
              Run(test_field_.global_id(),
                  HasSingleSuggestionWithMainText(u"SomePrefixOne")))
      .WillOnce(base::test::RunClosure(run_loop.QuitClosure()));

  autocomplete_manager_->OnGetSingleFieldSuggestions(
      test_form_data_, /*form_structure=*/nullptr, test_field_,
      /*trigger_autofill_field=*/nullptr, autofill_client_,
      mock_callback.Get());

  ASSERT_FALSE(db_callback.is_null());
  std::move(db_callback).Run(kTestDbQuryId, std::move(mocked_results));
  run_loop.Run();
}

// Tests that we don't return any suggestion if we only have one suggestion that
// is case-sensitive equal to the given prefix.
TEST_F(AutocompleteHistoryManagerTest,
       SuggestionsReturned_InvokeHandler_SingleValue_EqualsPrefix) {
  std::vector<AutocompleteEntry> expected_values = {
      GetAutocompleteEntry(test_field_.name(), test_field_.value())};
  std::unique_ptr<WDTypedResult> mocked_results =
      GetMockedDbResults(expected_values);

  DbCallback db_callback;
  EXPECT_CALL(*web_data_service_,
              GetFormValuesForElementName(test_field_.name(),
                                          test_field_.value(), _, _))
      .WillOnce([&](auto, auto, int, DbCallback callback) {
        db_callback = std::move(callback);
        return kTestDbQuryId;
      });

  base::RunLoop run_loop;
  MockSuggestionsReturnedCallback mock_callback;
  EXPECT_CALL(mock_callback, Run(test_field_.global_id(), IsEmpty()))
      .WillOnce(base::test::RunClosure(run_loop.QuitClosure()));
  autocomplete_manager_->OnGetSingleFieldSuggestions(
      test_form_data_, /*form_structure=*/nullptr, test_field_,
      /*trigger_autofill_field=*/nullptr, autofill_client_,
      mock_callback.Get());

  ASSERT_FALSE(db_callback.is_null());
  std::move(db_callback).Run(kTestDbQuryId, std::move(mocked_results));
  run_loop.Run();
}

// Tests the case sensitivity of the unique suggestion equal to the prefix
// filter.
TEST_F(AutocompleteHistoryManagerTest,
       SuggestionsReturned_InvokeHandler_SingleValue_EqualsPrefix_DiffCase) {
  std::vector<AutocompleteEntry> expected_values = {
      GetAutocompleteEntry(test_field_.name(), u"someprefix")};

  std::unique_ptr<WDTypedResult> mocked_results =
      GetMockedDbResults(expected_values);

  DbCallback db_callback;
  EXPECT_CALL(*web_data_service_,
              GetFormValuesForElementName(test_field_.name(),
                                          test_field_.value(), _, _))
      .WillOnce([&](auto, auto, int, DbCallback callback) {
        db_callback = std::move(callback);
        return kTestDbQuryId;
      });

  // Simulate request for suggestions.
  base::RunLoop run_loop;
  MockSuggestionsReturnedCallback mock_callback;
  EXPECT_CALL(mock_callback,
              Run(test_field_.global_id(),
                  HasSingleSuggestionWithMainText(u"someprefix")))
      .WillOnce(base::test::RunClosure(run_loop.QuitClosure()));

  autocomplete_manager_->OnGetSingleFieldSuggestions(
      test_form_data_, /*form_structure=*/nullptr, test_field_,
      /*trigger_autofill_field=*/nullptr, autofill_client_,
      mock_callback.Get());

  ASSERT_FALSE(db_callback.is_null());
  std::move(db_callback).Run(kTestDbQuryId, std::move(mocked_results));
  run_loop.Run();
}

// Test that upon accepting an autocomplete suggestion, we correctly log the
// number of days since its last usage.
TEST_F(AutocompleteHistoryManagerTest,
       OnSingleFieldSuggestionSelected_ShouldLogDays) {
  Suggestion suggestion(u"TestValue", SuggestionType::kAutocompleteEntry);
  suggestion.payload = GetAutocompleteEntry(
      test_field_.name(), u"TestValue",
      /*date_created=*/base::Time::Now() - base::Days(20),
      /*date_last_used=*/base::Time::Now() - base::Days(10));

  base::HistogramTester histogram_tester;
  autocomplete_manager_->OnSingleFieldSuggestionSelected(suggestion);
  histogram_tester.ExpectBucketCount("Autocomplete.DaysSinceLastUse", 10, 1);
}

TEST_F(AutocompleteHistoryManagerTest,
       SuggestionsReturned_InvokeHandler_TwoRequests_OneHandler_Cancels) {
  int kTestDbQuryId_first = 100;
  int kTestDbQuryId_second = 101;

  std::vector<AutocompleteEntry> expected_values_first = {
      GetAutocompleteEntry(test_field_.name(), u"SomePrefixOne")};

  std::vector<AutocompleteEntry> expected_values_second = {
      GetAutocompleteEntry(test_field_.name(), u"SomePrefixTwo")};

  std::unique_ptr<WDTypedResult> mocked_results_first =
      GetMockedDbResults(expected_values_first);

  std::unique_ptr<WDTypedResult> mocked_results_second =
      GetMockedDbResults(expected_values_second);

  DbCallback db_callback_first;
  DbCallback db_callback_second;
  EXPECT_CALL(*web_data_service_,
              GetFormValuesForElementName(test_field_.name(),
                                          test_field_.value(), _, _))
      .WillOnce([&](auto, auto, int, DbCallback callback) {
        // Correctly move the move-only callback.
        db_callback_first = std::move(callback);
        // The function must return a handle.
        return kTestDbQuryId_first;
      })
      .WillOnce([&](auto, auto, int, DbCallback callback) {
        // Correctly move the move-only callback.
        db_callback_second = std::move(callback);
        // The function must return a handle.
        return kTestDbQuryId_second;
      });

  base::RunLoop run_loop;
  MockSuggestionsReturnedCallback mock_callback;
  EXPECT_CALL(mock_callback, Run).Times(0);
  autocomplete_manager_->OnGetSingleFieldSuggestions(
      test_form_data_, /*form_structure=*/nullptr, test_field_,
      /*trigger_autofill_field=*/nullptr, autofill_client_,
      mock_callback.Get());

  EXPECT_CALL(*web_data_service_, CancelRequest(kTestDbQuryId_first))
      .Times(1);
  EXPECT_CALL(mock_callback,
              Run(test_field_.global_id(), HasSingleSuggestionWithMainText(
                                               u"SomePrefixTwo")))
      .WillOnce(base::test::RunClosure(run_loop.QuitClosure()));
  autocomplete_manager_->OnGetSingleFieldSuggestions(
      test_form_data_, /*form_structure=*/nullptr, test_field_,
      /*trigger_autofill_field=*/nullptr, autofill_client_,
      mock_callback.Get());

  ASSERT_FALSE(db_callback_second.is_null());
  std::move(db_callback_second)
      .Run(kTestDbQuryId_second, std::move(mocked_results_second));
  run_loop.Run();
}

TEST_F(AutocompleteHistoryManagerTest, SuggestionsReturned_CancelPendingQuery) {
  std::vector<AutocompleteEntry> expected_values_one = {
      GetAutocompleteEntry(test_field_.name(), u"SomePrefixOne")};
  std::unique_ptr<WDTypedResult> mocked_results_one =
      GetMockedDbResults(expected_values_one);

  // Simulate a request for autocomplete suggestions.
  DbCallback db_callback;
  EXPECT_CALL(*web_data_service_,
              GetFormValuesForElementName(test_field_.name(),
                                          test_field_.value(), _, _))
      .WillOnce([&](auto, auto, int, DbCallback callback) {
        db_callback = std::move(callback);
        return kTestDbQuryId;
      });

  base::RunLoop run_loop;
  MockSuggestionsReturnedCallback mock_callback;
  EXPECT_CALL(mock_callback, Run(test_field_.global_id(), testing::IsEmpty()))
      .WillOnce(base::test::RunClosure(run_loop.QuitClosure()));
  autocomplete_manager_->OnGetSingleFieldSuggestions(
      test_form_data_, /*form_structure=*/nullptr, test_field_,
      /*trigger_autofill_field=*/nullptr, autofill_client_,
      mock_callback.Get());

  // Simulate cancelling the request.
  EXPECT_CALL(*web_data_service_, CancelRequest(kTestDbQuryId));
  autocomplete_manager_->CancelPendingQuery();

  ASSERT_FALSE(db_callback.is_null());
  std::move(db_callback).Run(kTestDbQuryId, std::move(mocked_results_one));
  run_loop.Run();
}

// Verify that no autocomplete suggestion is returned for a textarea.
TEST_F(AutocompleteHistoryManagerTest, NoAutocompleteSuggestionsForTextarea) {
  FormData form;
  form.set_name(u"MyForm");
  form.set_url(GURL("http://myform.com/form.html"));
  form.set_action(GURL("http://myform.com/submit.html"));

  FormFieldData field =
      CreateTestFormField("Address", "address", "", FormControlType::kTextArea);

  std::vector<AutocompleteEntry> expected_values = {
      GetAutocompleteEntry(test_field_.name(), u"SomePrefixOne")};
  std::unique_ptr<WDTypedResult> mocked_results =
      GetMockedDbResults(expected_values);

  DbCallback db_callback;
  EXPECT_CALL(*web_data_service_,
              GetFormValuesForElementName(test_field_.name(),
                                          test_field_.value(), _, _))
      .WillOnce([&](auto, auto, int, DbCallback callback) {
        db_callback = std::move(callback);
        return kTestDbQuryId;
      });

  MockSuggestionsReturnedCallback mock_callback;
  EXPECT_CALL(mock_callback, Run(test_field_.global_id(), testing::SizeIs(1)));

  autocomplete_manager_->OnGetSingleFieldSuggestions(
      test_form_data_, /*form_structure=*/nullptr, test_field_,
      /*trigger_autofill_field=*/nullptr, autofill_client_,
      mock_callback.Get());

  ASSERT_FALSE(db_callback.is_null());
  std::move(db_callback).Run(kTestDbQuryId, std::move(mocked_results));
}

TEST_F(AutocompleteHistoryManagerTest, DestructorCancelsRequests) {
  base::RunLoop run_loop;
  MockSuggestionsReturnedCallback mock_callback;

  EXPECT_CALL(*web_data_service_,
              GetFormValuesForElementName(test_field_.name(),
                                          test_field_.value(), _, _))
      .WillOnce([&run_loop]() {
        run_loop.Quit();
        return kTestDbQuryId;
      });

  // Simulate request for suggestions.
  autocomplete_manager_->OnGetSingleFieldSuggestions(
      test_form_data_, /*form_structure=*/nullptr, test_field_,
      /*trigger_autofill_field=*/nullptr, autofill_client_,
      mock_callback.Get());
  run_loop.Run();

  // Expect a cancel call.
  EXPECT_CALL(*web_data_service_, CancelRequest(kTestDbQuryId));

  autocomplete_manager_.reset();

  EXPECT_TRUE(PendingQueryEmpty());
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
  MockSuggestionsReturnedCallback mock_callback;

  autocomplete_manager_->OnAutofillCleanupReturned(
      1, std::make_unique<WDResult<size_t>>(AUTOFILL_CLEANUP_RESULT,
                                            cleanup_result));

  EXPECT_EQ(version_info::GetMajorVersionNumberAsInt(),
            prefs_->GetInteger(prefs::kAutocompleteLastVersionRetentionPolicy));
}

}  // namespace autofill
