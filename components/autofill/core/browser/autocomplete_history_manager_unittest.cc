// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <vector>

#include "base/macros.h"
#include "base/memory/ref_counted.h"
#include "base/run_loop.h"
#include "base/strings/string16.h"
#include "base/strings/utf_string_conversions.h"
#include "base/synchronization/waitable_event.h"
#include "base/test/metrics/histogram_tester.h"
#include "base/test/scoped_feature_list.h"
#include "base/test/scoped_task_environment.h"
#include "base/threading/thread_task_runner_handle.h"
#include "components/autofill/core/browser/autocomplete_history_manager.h"
#include "components/autofill/core/browser/autofill_external_delegate.h"
#include "components/autofill/core/browser/autofill_manager.h"
#include "components/autofill/core/browser/autofill_test_utils.h"
#include "components/autofill/core/browser/test_autofill_client.h"
#include "components/autofill/core/browser/test_autofill_driver.h"
#include "components/autofill/core/browser/webdata/autofill_webdata_service.h"
#include "components/autofill/core/common/autofill_features.h"
#include "components/autofill/core/common/form_data.h"
#include "components/prefs/pref_service.h"
#include "components/webdata_services/web_data_service_test_util.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "ui/gfx/geometry/rect.h"

using base::ASCIIToUTF16;
using testing::_;

namespace autofill {

namespace {

class MockWebDataService : public AutofillWebDataService {
 public:
  MockWebDataService()
      : AutofillWebDataService(base::ThreadTaskRunnerHandle::Get(),
                               base::ThreadTaskRunnerHandle::Get()) {}

  MOCK_METHOD1(AddFormFields, void(const std::vector<FormFieldData>&));

  WebDataServiceBase::Handle GetFormValuesForElementName(
      const base::string16& name,
      const base::string16& prefix,
      int limit,
      WebDataServiceConsumer* consumer) override {
    return mock_handle_;
  }

  void set_mock_handle(WebDataServiceBase::Handle handle) {
    mock_handle_ = handle;
  }

 protected:
  ~MockWebDataService() override {}

 private:
  WebDataServiceBase::Handle mock_handle_ = 0;
};

class MockAutofillClient : public TestAutofillClient {
 public:
  MockAutofillClient(scoped_refptr<MockWebDataService> web_data_service)
      : web_data_service_(web_data_service),
        prefs_(test::PrefServiceForTesting()) {}
  ~MockAutofillClient() override {}
  scoped_refptr<AutofillWebDataService> GetDatabase() override {
    return web_data_service_;
  }
  PrefService* GetPrefs() override { return prefs_.get(); }

 private:
  scoped_refptr<MockWebDataService> web_data_service_;
  std::unique_ptr<PrefService> prefs_;

  DISALLOW_COPY_AND_ASSIGN(MockAutofillClient);
};

}  // namespace

class AutocompleteHistoryManagerTest : public testing::Test {
 protected:
  AutocompleteHistoryManagerTest() {}

  void SetUp() override {
    web_data_service_ = base::MakeRefCounted<MockWebDataService>();
    autofill_client_ = std::make_unique<MockAutofillClient>(web_data_service_);
    autofill_driver_ = std::make_unique<TestAutofillDriver>();
    autocomplete_manager_ = std::make_unique<AutocompleteHistoryManager>(
        autofill_driver_.get(), autofill_client_.get());
  }

  void TearDown() override { autocomplete_manager_.reset(); }

  base::test::ScopedTaskEnvironment scoped_task_environment_;
  scoped_refptr<MockWebDataService> web_data_service_;
  std::unique_ptr<AutocompleteHistoryManager> autocomplete_manager_;
  std::unique_ptr<AutofillDriver> autofill_driver_;
  std::unique_ptr<MockAutofillClient> autofill_client_;
};

// Tests that credit card numbers are not sent to the WebDatabase to be saved.
TEST_F(AutocompleteHistoryManagerTest, CreditCardNumberValue) {
  FormData form;
  form.name = ASCIIToUTF16("MyForm");
  form.origin = GURL("http://myform.com/form.html");
  form.action = GURL("http://myform.com/submit.html");

  // Valid Visa credit card number pulled from the paypal help site.
  FormFieldData valid_cc;
  valid_cc.label = ASCIIToUTF16("Credit Card");
  valid_cc.name = ASCIIToUTF16("ccnum");
  valid_cc.value = ASCIIToUTF16("4012888888881881");
  valid_cc.form_control_type = "text";
  form.fields.push_back(valid_cc);

  EXPECT_CALL(*web_data_service_, AddFormFields(_)).Times(0);
  autocomplete_manager_->OnWillSubmitForm(form);
}

// Contrary test to AutocompleteHistoryManagerTest.CreditCardNumberValue.  The
// value being submitted is not a valid credit card number, so it will be sent
// to the WebDatabase to be saved.
TEST_F(AutocompleteHistoryManagerTest, NonCreditCardNumberValue) {
  FormData form;
  form.name = ASCIIToUTF16("MyForm");
  form.origin = GURL("http://myform.com/form.html");
  form.action = GURL("http://myform.com/submit.html");

  // Invalid credit card number.
  FormFieldData invalid_cc;
  invalid_cc.label = ASCIIToUTF16("Credit Card");
  invalid_cc.name = ASCIIToUTF16("ccnum");
  invalid_cc.value = ASCIIToUTF16("4580123456789012");
  invalid_cc.form_control_type = "text";
  form.fields.push_back(invalid_cc);

  EXPECT_CALL(*(web_data_service_.get()), AddFormFields(_)).Times(1);
  autocomplete_manager_->OnWillSubmitForm(form);
}

// Tests that SSNs are not sent to the WebDatabase to be saved.
TEST_F(AutocompleteHistoryManagerTest, SSNValue) {
  FormData form;
  form.name = ASCIIToUTF16("MyForm");
  form.origin = GURL("http://myform.com/form.html");
  form.action = GURL("http://myform.com/submit.html");

  FormFieldData ssn;
  ssn.label = ASCIIToUTF16("Social Security Number");
  ssn.name = ASCIIToUTF16("ssn");
  ssn.value = ASCIIToUTF16("078-05-1120");
  ssn.form_control_type = "text";
  form.fields.push_back(ssn);

  EXPECT_CALL(*web_data_service_, AddFormFields(_)).Times(0);
  autocomplete_manager_->OnWillSubmitForm(form);
}

// Verify that autocomplete text is saved for search fields.
TEST_F(AutocompleteHistoryManagerTest, SearchField) {
  FormData form;
  form.name = ASCIIToUTF16("MyForm");
  form.origin = GURL("http://myform.com/form.html");
  form.action = GURL("http://myform.com/submit.html");

  // Search field.
  FormFieldData search_field;
  search_field.label = ASCIIToUTF16("Search");
  search_field.name = ASCIIToUTF16("search");
  search_field.value = ASCIIToUTF16("my favorite query");
  search_field.form_control_type = "search";
  form.fields.push_back(search_field);

  EXPECT_CALL(*(web_data_service_.get()), AddFormFields(_)).Times(1);
  autocomplete_manager_->OnWillSubmitForm(form);
}

// Tests that text entered into fields specifying autocomplete="off" is not sent
// to the WebDatabase to be saved. Note this is also important as the mechanism
// for preventing CVCs from being saved.
// See AutofillManagerTest.DontSaveCvcInAutocompleteHistory
TEST_F(AutocompleteHistoryManagerTest, FieldWithAutocompleteOff) {
  FormData form;
  form.name = ASCIIToUTF16("MyForm");
  form.origin = GURL("http://myform.com/form.html");
  form.action = GURL("http://myform.com/submit.html");

  // Field specifying autocomplete="off".
  FormFieldData field;
  field.label = ASCIIToUTF16("Something esoteric");
  field.name = ASCIIToUTF16("esoterica");
  field.value = ASCIIToUTF16("a truly esoteric value, I assure you");
  field.form_control_type = "text";
  field.should_autocomplete = false;
  form.fields.push_back(field);

  EXPECT_CALL(*web_data_service_, AddFormFields(_)).Times(0);
  autocomplete_manager_->OnWillSubmitForm(form);
}

// Tests that text entered into fields that are not focusable is not sent to the
// WebDatabase to be saved.
TEST_F(AutocompleteHistoryManagerTest, NonFocusableField) {
  FormData form;
  form.name = ASCIIToUTF16("MyForm");
  form.origin = GURL("http://myform.com/form.html");
  form.action = GURL("http://myform.com/submit.html");

  // Unfocusable field.
  FormFieldData field;
  field.label = ASCIIToUTF16("Something esoteric");
  field.name = ASCIIToUTF16("esoterica");
  field.value = ASCIIToUTF16("a truly esoteric value, I assure you");
  field.form_control_type = "text";
  field.is_focusable = false;
  form.fields.push_back(field);

  EXPECT_CALL(*web_data_service_, AddFormFields(_)).Times(0);
  autocomplete_manager_->OnWillSubmitForm(form);
}

// Tests that text entered into presentation fields is not sent to the
// WebDatabase to be saved.
TEST_F(AutocompleteHistoryManagerTest, PresentationField) {
  FormData form;
  form.name = ASCIIToUTF16("MyForm");
  form.origin = GURL("http://myform.com/form.html");
  form.action = GURL("http://myform.com/submit.html");

  // Presentation field.
  FormFieldData field;
  field.label = ASCIIToUTF16("Something esoteric");
  field.name = ASCIIToUTF16("esoterica");
  field.value = ASCIIToUTF16("a truly esoteric value, I assure you");
  field.form_control_type = "text";
  field.role = FormFieldData::ROLE_ATTRIBUTE_PRESENTATION;
  form.fields.push_back(field);

  EXPECT_CALL(*web_data_service_, AddFormFields(_)).Times(0);
  autocomplete_manager_->OnWillSubmitForm(form);
}

namespace {

class MockAutofillExternalDelegate : public AutofillExternalDelegate {
 public:
  MockAutofillExternalDelegate(AutofillManager* autofill_manager,
                               AutofillDriver* autofill_driver)
      : AutofillExternalDelegate(autofill_manager, autofill_driver) {}
  ~MockAutofillExternalDelegate() override {}

  MOCK_METHOD4(OnSuggestionsReturned,
               void(int query_id,
                    const std::vector<Suggestion>& suggestions,
                    bool autoselect_first_suggestion,
                    bool is_all_server_suggestions));

 private:
  DISALLOW_COPY_AND_ASSIGN(MockAutofillExternalDelegate);
};

class TestAutocompleteHistoryManager : public AutocompleteHistoryManager {
 public:
  TestAutocompleteHistoryManager(AutofillDriver* driver, AutofillClient* client)
      : AutocompleteHistoryManager(driver, client) {}

  using AutocompleteHistoryManager::SendSuggestions;
};

// Predicate for GMock.
bool IsEmptySuggestionVector(const std::vector<Suggestion>& suggestions) {
  return suggestions.empty();
}

bool NonEmptySuggestionVector(const std::vector<Suggestion>& suggestions) {
  return !suggestions.empty();
}

}  // namespace

// Make sure our external delegate is called at the right time.
TEST_F(AutocompleteHistoryManagerTest, ExternalDelegate) {
  TestAutocompleteHistoryManager autocomplete_history_manager(
      autofill_driver_.get(), autofill_client_.get());

  auto autofill_manager = std::make_unique<AutofillManager>(
      autofill_driver_.get(), autofill_client_.get(), "en-US",
      AutofillManager::ENABLE_AUTOFILL_DOWNLOAD_MANAGER);

  MockAutofillExternalDelegate external_delegate(autofill_manager.get(),
                                                 autofill_driver_.get());
  autocomplete_history_manager.SetExternalDelegate(&external_delegate);

  // Should trigger a call to OnSuggestionsReturned, verified by the mock.
  EXPECT_CALL(external_delegate, OnSuggestionsReturned(_, _, _, _));
  autocomplete_history_manager.SendSuggestions(nullptr);
}

// Verify that no autocomplete suggestion is returned for textarea.
TEST_F(AutocompleteHistoryManagerTest, NoAutocompleteSuggestionsForTextarea) {
  TestAutocompleteHistoryManager autocomplete_history_manager(
      autofill_driver_.get(), autofill_client_.get());

  auto autofill_manager = std::make_unique<AutofillManager>(
      autofill_driver_.get(), autofill_client_.get(), "en-US",
      AutofillManager::ENABLE_AUTOFILL_DOWNLOAD_MANAGER);

  MockAutofillExternalDelegate external_delegate(autofill_manager.get(),
                                                 autofill_driver_.get());
  autocomplete_history_manager.SetExternalDelegate(&external_delegate);

  FormData form;
  form.name = ASCIIToUTF16("MyForm");
  form.origin = GURL("http://myform.com/form.html");
  form.action = GURL("http://myform.com/submit.html");

  FormFieldData field;
  test::CreateTestFormField("Address", "address", "", "textarea", &field);

  EXPECT_CALL(external_delegate,
              OnSuggestionsReturned(0, testing::Truly(IsEmptySuggestionVector),
                                    false, _));
  autocomplete_history_manager.OnGetAutocompleteSuggestions(
      0,
      field.name,
      field.value,
      field.form_control_type);
}

// Verify that no autocomplete suggestion is returned for textarea and UMA is
// logged correctly.
TEST_F(AutocompleteHistoryManagerTest, AutocompleteUMAQueryNotCreated) {
  TestAutocompleteHistoryManager autocomplete_history_manager(
      autofill_driver_.get(), autofill_client_.get());
  auto autofill_manager = std::make_unique<AutofillManager>(
      autofill_driver_.get(), autofill_client_.get(), "en-US",
      AutofillManager::ENABLE_AUTOFILL_DOWNLOAD_MANAGER);

  MockAutofillExternalDelegate external_delegate(autofill_manager.get(),
                                                 autofill_driver_.get());
  autocomplete_history_manager.SetExternalDelegate(&external_delegate);

  FormFieldData field;
  test::CreateTestFormField("Address", "address", "", "textarea", &field);

  base::HistogramTester histogram_tester;
  EXPECT_CALL(external_delegate,
              OnSuggestionsReturned(0, testing::Truly(IsEmptySuggestionVector),
                                    false, _));
  autocomplete_history_manager.OnGetAutocompleteSuggestions(
      0, field.name, field.value, field.form_control_type);
  histogram_tester.ExpectBucketCount("Autofill.AutocompleteQuery", 0, 1);
  histogram_tester.ExpectBucketCount("Autofill.AutocompleteQuery", 1, 0);
}

// Verify that autocomplete suggestion is returned and suggestions is logged
// correctly.
TEST_F(AutocompleteHistoryManagerTest, AutocompleteUMAQueryCreated) {
  TestAutocompleteHistoryManager autocomplete_history_manager(
      autofill_driver_.get(), autofill_client_.get());
  auto autofill_manager = std::make_unique<AutofillManager>(
      autofill_driver_.get(), autofill_client_.get(), "en-US",
      AutofillManager::ENABLE_AUTOFILL_DOWNLOAD_MANAGER);

  MockAutofillExternalDelegate external_delegate(autofill_manager.get(),
                                                 autofill_driver_.get());
  autocomplete_history_manager.SetExternalDelegate(&external_delegate);

  FormFieldData field;
  test::CreateTestFormField("Address", "address", "", "text", &field);

  // Mock returned handle to match it in OnWebDataServiceRequestDone().
  scoped_refptr<AutofillWebDataService> service =
      autofill_client_->GetDatabase();
  MockWebDataService* data_service =
      static_cast<MockWebDataService*>(service.get());
  WebDataServiceBase::Handle mock_handle = 1;
  data_service->set_mock_handle(mock_handle);

  // Verify that the query has been created.
  base::HistogramTester histogram_tester;
  EXPECT_CALL(external_delegate,
              OnSuggestionsReturned(0, testing::Truly(IsEmptySuggestionVector),
                                    false, _));
  autocomplete_history_manager.OnGetAutocompleteSuggestions(
      0, field.name, field.value, field.form_control_type);
  histogram_tester.ExpectBucketCount("Autofill.AutocompleteQuery", 1, 1);
  histogram_tester.ExpectBucketCount("Autofill.AutocompleteQuery", 0, 0);

  // Mock no suggestion returned and verify that the suggestion UMA is correct.
  std::unique_ptr<WDTypedResult> result =
      std::make_unique<WDResult<std::vector<base::string16>>>(
          AUTOFILL_VALUE_RESULT, std::vector<base::string16>());
  autocomplete_history_manager.OnWebDataServiceRequestDone(mock_handle,
                                                           std::move(result));

  histogram_tester.ExpectBucketCount("Autofill.AutocompleteSuggestions", 0, 1);
  histogram_tester.ExpectBucketCount("Autofill.AutocompleteSuggestions", 1, 0);

  // Changed the returned handle
  mock_handle = 2;
  data_service->set_mock_handle(mock_handle);
  // Changed field's name to trigger UMA again.
  test::CreateTestFormField("Address", "address1", "", "text", &field);
  EXPECT_CALL(external_delegate,
              OnSuggestionsReturned(0, testing::Truly(NonEmptySuggestionVector),
                                    false, _));
  autocomplete_history_manager.OnGetAutocompleteSuggestions(
      0, field.name, field.value, field.form_control_type);
  histogram_tester.ExpectBucketCount("Autofill.AutocompleteQuery", 1, 2);
  histogram_tester.ExpectBucketCount("Autofill.AutocompleteQuery", 0, 0);

  // Mock one suggestion returned and verify that the suggestion UMA is correct.
  std::vector<base::string16> values;
  values.push_back(ASCIIToUTF16("value"));
  result = std::make_unique<WDResult<std::vector<base::string16>>>(
      AUTOFILL_VALUE_RESULT, values);
  autocomplete_history_manager.OnWebDataServiceRequestDone(mock_handle,
                                                           std::move(result));

  histogram_tester.ExpectBucketCount("Autofill.AutocompleteSuggestions", 0, 1);
  histogram_tester.ExpectBucketCount("Autofill.AutocompleteSuggestions", 1, 1);
}

}  // namespace autofill
