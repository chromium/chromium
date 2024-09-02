// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/autofill/core/browser/autofill_feedback_data.h"

#include "base/json/json_reader.h"
#include "base/test/gmock_expected_support.h"
#include "base/test/task_environment.h"
#include "base/values.h"
#include "components/autofill/core/browser/autofill_test_utils.h"
#include "components/autofill/core/browser/test_autofill_client.h"
#include "components/autofill/core/browser/test_autofill_driver.h"
#include "components/autofill/core/browser/test_browser_autofill_manager.h"
#include "components/autofill/core/common/autofill_features.h"
#include "components/autofill/core/common/autofill_test_utils.h"
#include "components/autofill/core/common/form_data.h"
#include "components/autofill/core/common/form_data_test_api.h"
#include "testing/gmock/include/gmock/gmock.h"

namespace autofill {
namespace {

using test::CreateTestFormField;

constexpr char kExpectedFeedbackDataJSON[] = R"({
   "formStructures": [ {
      "formSignature": "4232380759432074174",
      "hostFrame": "00000000000181CD000000000000A8CA",
      "idAttribute": "",
      "mainFrameUrl": "https://myform_root.com",
      "nameAttribute": "",
      "rendererId": "11",
      "sourceUrl": "https://myform.com",
      "fields": [ {
         "autocompleteAttribute": "cc-given-name",
         "fieldSignature": "3879476562",
         "fieldType": "NAME_FIRST",
         "heuristicType": "CREDIT_CARD_NAME_FIRST",
         "hostFormSignature": "0",
         "htmlType": "HTML_TYPE_CREDIT_CARD_NAME_FIRST",
         "idAttribute": "",
         "isEmpty": true,
         "isFocusable": true,
         "isVisible": true,
         "labelAttribute": "First Name on Card",
         "parseableNameAttribute": "",
         "placeholderAttribute": "",
         "rank": "0",
         "rankInHostForm": "0",
         "rankInHostFormSignatureGroup": "0",
         "rankInSignatureGroup": "0",
         "section": "firstnameoncard_0_11",
         "serverType": "NO_SERVER_DATA",
         "serverTypeIsOverride": false
      }, {
         "autocompleteAttribute": "cc-family-name",
         "fieldSignature": "3213606822",
         "fieldType": "NAME_LAST",
         "heuristicType": "CREDIT_CARD_NAME_LAST",
         "hostFormSignature": "0",
         "htmlType": "HTML_TYPE_CREDIT_CARD_NAME_LAST",
         "idAttribute": "",
         "isEmpty": true,
         "isFocusable": true,
         "isVisible": true,
         "labelAttribute": "Last Name on Card",
         "parseableNameAttribute": "",
         "placeholderAttribute": "",
         "rank": "1",
         "rankInHostForm": "1",
         "rankInHostFormSignatureGroup": "0",
         "rankInSignatureGroup": "0",
         "section": "firstnameoncard_0_11",
         "serverType": "NO_SERVER_DATA",
         "serverTypeIsOverride": false
      }, {
         "autocompleteAttribute": "",
         "fieldSignature": "1029417091",
         "fieldType": "EMAIL_ADDRESS",
         "heuristicType": "EMAIL_ADDRESS",
         "hostFormSignature": "0",
         "htmlType": "HTML_TYPE_UNSPECIFIED",
         "idAttribute": "",
         "isEmpty": true,
         "isFocusable": true,
         "isVisible": true,
         "labelAttribute": "Email",
         "parseableNameAttribute": "",
         "placeholderAttribute": "",
         "rank": "2",
         "rankInHostForm": "2",
         "rankInHostFormSignatureGroup": "0",
         "rankInSignatureGroup": "0",
         "section": "email_0_13",
         "serverType": "NO_SERVER_DATA",
         "serverTypeIsOverride": false
      } ]
   } ]
})";

FormData CreateFeedbackTestFormData() {
  FormData form;
  form.set_host_frame(test::MakeLocalFrameToken(test::RandomizeFrame(false)));
  form.set_renderer_id(test::MakeFormRendererId());
  form.set_name(u"MyForm");
  form.set_url(GURL("https://myform.com/form.html"));
  form.set_action(GURL("https://myform.com/submit.html"));
  form.set_main_frame_origin(
      url::Origin::Create(GURL("https://myform_root.com/form.html")));
  form.set_fields(
      {CreateTestFormField("First Name on Card", "firstnameoncard", "",
                           FormControlType::kInputText, "cc-given-name"),
       CreateTestFormField("Last Name on Card", "lastnameoncard", "",
                           FormControlType::kInputText, "cc-family-name"),
       CreateTestFormField("Email", "email", "",
                           FormControlType::kInputEmail)});
  for (FormFieldData& field : test_api(form).fields()) {
    field.set_host_frame(form.host_frame());
  }
  return form;
}

class AutofillFeedbackDataUnitTest : public testing::Test {
 protected:
  AutofillFeedbackDataUnitTest() = default;
  void SetUp() override {
    autofill_driver_ = std::make_unique<TestAutofillDriver>(&autofill_client_);
    browser_autofill_manager_ =
        std::make_unique<TestBrowserAutofillManager>(autofill_driver_.get());
  }

  base::test::TaskEnvironment task_environment_{
      base::test::TaskEnvironment::TimeSource::MOCK_TIME};
  test::AutofillUnitTestEnvironment autofill_test_environment_;
  TestAutofillClient autofill_client_;
  std::unique_ptr<TestAutofillDriver> autofill_driver_;
  std::unique_ptr<TestBrowserAutofillManager> browser_autofill_manager_;
};

TEST_F(AutofillFeedbackDataUnitTest, CreatesCompleteReport) {
  FormData form = CreateFeedbackTestFormData();
  browser_autofill_manager_->OnFormsSeen(
      /*updated_forms=*/{form},
      /*removed_forms=*/{});

  base::Value::Dict autofill_feedback_data =
      data_logs::FetchAutofillFeedbackData(browser_autofill_manager_.get());

  ASSERT_OK_AND_ASSIGN(
      auto expected_data,
      base::JSONReader::ReadAndReturnValueWithError(
          kExpectedFeedbackDataJSON,
          base::JSONParserOptions::JSON_ALLOW_TRAILING_COMMAS));
  ASSERT_TRUE(expected_data.is_dict());
  EXPECT_EQ(autofill_feedback_data, expected_data.GetDict());
}

TEST_F(AutofillFeedbackDataUnitTest, IncludesLastAutofillEventLogEntry) {
  FormData form = CreateFeedbackTestFormData();
  FormFieldData field = form.fields()[0];
  browser_autofill_manager_->OnFormsSeen(
      /*updated_forms=*/{form},
      /*removed_forms=*/{});

  // Simulates an autofill event.
  Suggestion suggestion(u"TestValue", SuggestionType::kIbanEntry);
  browser_autofill_manager_->OnSingleFieldSuggestionSelected(suggestion, form,
                                                             field);

  ASSERT_OK_AND_ASSIGN(
      auto expected_data,
      base::JSONReader::ReadAndReturnValueWithError(
          kExpectedFeedbackDataJSON,
          base::JSONParserOptions::JSON_ALLOW_TRAILING_COMMAS));
  ASSERT_TRUE(expected_data.is_dict());

  // Update the expected data with a last_autofill_event entry.
  base::Value::Dict last_autofill_event;
  last_autofill_event.Set("associatedCountry", "");
  last_autofill_event.Set("type", "SingleFieldFormFillerIban");
  expected_data.GetDict().Set("lastAutofillEvent",
                              std::move(last_autofill_event));

  EXPECT_EQ(
      data_logs::FetchAutofillFeedbackData(browser_autofill_manager_.get()),
      expected_data.GetDict());
}

TEST_F(AutofillFeedbackDataUnitTest,
       NotIncludeLastAutofillEventIfExceedTimeLimit) {
  FormData form = CreateFeedbackTestFormData();
  const FormFieldData& field = form.fields()[0];
  browser_autofill_manager_->OnFormsSeen(
      /*updated_forms=*/{form},
      /*removed_forms=*/{});

  // Simulates an autofill event.
  Suggestion suggestion(u"TestValue", SuggestionType::kIbanEntry);
  browser_autofill_manager_->OnSingleFieldSuggestionSelected(suggestion, form,
                                                             field);

  // Advance the clock 4 minutes should disregard the last autofill event log.
  task_environment_.FastForwardBy(base::Minutes(4));

  // Expected data does not contain the last_autofill_event entry.
  ASSERT_OK_AND_ASSIGN(
      auto expected_data,
      base::JSONReader::ReadAndReturnValueWithError(
          kExpectedFeedbackDataJSON,
          base::JSONParserOptions::JSON_ALLOW_TRAILING_COMMAS));
  ASSERT_TRUE(expected_data.is_dict());

  EXPECT_EQ(
      data_logs::FetchAutofillFeedbackData(browser_autofill_manager_.get()),
      expected_data.GetDict());
}

TEST_F(AutofillFeedbackDataUnitTest, IncludesExtraLogs) {
  FormData form = CreateFeedbackTestFormData();
  browser_autofill_manager_->OnFormsSeen(
      /*updated_forms=*/{form},
      /*removed_forms=*/{});

  base::Value::Dict extra_logs;
  extra_logs.Set("triggerFormSignature", "123");
  extra_logs.Set("triggerFieldSignature", "456");

  base::Value::Dict autofill_feedback_data =
      data_logs::FetchAutofillFeedbackData(browser_autofill_manager_.get(),
                                           extra_logs.Clone());

  ASSERT_OK_AND_ASSIGN(
      auto expected_data,
      base::JSONReader::ReadAndReturnValueWithError(
          kExpectedFeedbackDataJSON,
          base::JSONParserOptions::JSON_ALLOW_TRAILING_COMMAS));
  ASSERT_TRUE(expected_data.is_dict());
  // Include extra logs in the expected report.
  expected_data.GetDict().Merge(std::move(extra_logs));
  EXPECT_EQ(autofill_feedback_data, expected_data.GetDict());
}

}  // namespace
}  // namespace autofill
