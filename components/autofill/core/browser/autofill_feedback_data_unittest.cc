// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/autofill/core/browser/autofill_feedback_data.h"

#include "base/json/json_reader.h"
#include "base/test/scoped_feature_list.h"
#include "base/test/task_environment.h"
#include "base/values.h"
#include "components/autofill/core/browser/autofill_test_utils.h"
#include "components/autofill/core/browser/test_autofill_client.h"
#include "components/autofill/core/browser/test_autofill_clock.h"
#include "components/autofill/core/browser/test_autofill_driver.h"
#include "components/autofill/core/browser/test_browser_autofill_manager.h"
#include "components/autofill/core/common/autofill_features.h"
#include "components/autofill/core/common/form_data.h"
#include "testing/gmock/include/gmock/gmock.h"

namespace autofill {
namespace {

const char kExpectedFeedbackDataJSON[] = R"({
   "formStructures": [ {
      "formSignature": "4232380759432074174",
      "hostFrame": "00000000000000000000000000000000",
      "idAttribute": "",
      "mainFrameUrl": "https://myform_root.com",
      "nameAttribute": "",
      "rendererId": "11",
      "sourceUrl": "https://myform.com",
      "fields": [ {
         "autocompleteAttribute": "cc-given-name",
         "fieldSignature": "3879476562",
         "fieldType": "HTML_TYPE_CREDIT_CARD_NAME_FIRST",
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
         "fieldType": "HTML_TYPE_CREDIT_CARD_NAME_LAST",
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
         "autocompleteAttribute": "cc-family-name",
         "fieldSignature": "1029417091",
         "fieldType": "HTML_TYPE_CREDIT_CARD_NAME_LAST",
         "heuristicType": "EMAIL_ADDRESS",
         "hostFormSignature": "0",
         "htmlType": "HTML_TYPE_CREDIT_CARD_NAME_LAST",
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
         "section": "firstnameoncard_0_11",
         "serverType": "NO_SERVER_DATA",
         "serverTypeIsOverride": false
      } ]
   } ]
})";

void CreateFeedbackTestFormData(FormData* form) {
  form->unique_renderer_id = test::MakeFormRendererId();
  form->name = u"MyForm";
  form->url = GURL("https://myform.com/form.html");
  form->action = GURL("https://myform.com/submit.html");
  form->main_frame_origin =
      url::Origin::Create(GURL("https://myform_root.com/form.html"));

  FormFieldData field;
  test::CreateTestFormField("First Name on Card", "firstnameoncard", "", "text",
                            "cc-given-name", &field);
  form->fields.push_back(field);
  test::CreateTestFormField("Last Name on Card", "lastnameoncard", "", "text",
                            "cc-family-name", &field);
  form->fields.push_back(field);
  test::CreateTestFormField("Email", "email", "", "email", &field);
  form->fields.push_back(field);
}
}  // namespace

class AutofillFeedbackDataUnitTest : public testing::Test {
 protected:
  AutofillFeedbackDataUnitTest() {
    feature_.InitWithFeatures(
        /*enabled_features=*/{features::kAutofillFeedback},
        /*disabled_features=*/{});
  }
  void SetUp() override {
    autofill_driver_ = std::make_unique<TestAutofillDriver>();
    browser_autofill_manager_ = std::make_unique<TestBrowserAutofillManager>(
        autofill_driver_.get(), &autofill_client_);
  }

  base::test::TaskEnvironment task_environment_;
  test::AutofillUnitTestEnvironment autofill_test_environment_;
  TestAutofillClient autofill_client_;
  std::unique_ptr<TestAutofillDriver> autofill_driver_;
  std::unique_ptr<TestBrowserAutofillManager> browser_autofill_manager_;
  base::test::ScopedFeatureList feature_;
};

TEST_F(AutofillFeedbackDataUnitTest, CreatesCompleteReport) {
  FormData form;
  CreateFeedbackTestFormData(&form);
  browser_autofill_manager_->OnFormsSeen(
      /*updated_forms=*/{form},
      /*removed_forms=*/{});

  base::Value::Dict autofill_feedback_data =
      data_logs::FetchAutofillFeedbackData(browser_autofill_manager_.get());

  auto expected_data = base::JSONReader::ReadAndReturnValueWithError(
      kExpectedFeedbackDataJSON,
      base::JSONParserOptions::JSON_ALLOW_TRAILING_COMMAS);

  ASSERT_TRUE(expected_data.has_value()) << expected_data.error().message;
  ASSERT_TRUE(expected_data->is_dict());
  EXPECT_EQ(autofill_feedback_data, expected_data->GetDict());
}

TEST_F(AutofillFeedbackDataUnitTest, IncludesLastAutofillEventLogEntry) {
  FormData form;
  CreateFeedbackTestFormData(&form);
  FormFieldData field = form.fields[0];
  browser_autofill_manager_->OnFormsSeen(
      /*updated_forms=*/{form},
      /*removed_forms=*/{});

  // Simulates an autofill event.
  browser_autofill_manager_->OnSingleFieldSuggestionSelected(
      u"TestValue", POPUP_ITEM_ID_IBAN_ENTRY, form, field);

  auto expected_data = base::JSONReader::ReadAndReturnValueWithError(
      kExpectedFeedbackDataJSON,
      base::JSONParserOptions::JSON_ALLOW_TRAILING_COMMAS);
  ASSERT_TRUE(expected_data.has_value()) << expected_data.error().message;
  ASSERT_TRUE(expected_data->is_dict());

  // Update the expected data with a last_autofill_event entry.
  base::Value::Dict last_autofill_event;
  last_autofill_event.Set("associatedCountry", "");
  last_autofill_event.Set("type", "SingleFieldFormFillerIban");
  expected_data->GetDict().Set("lastAutofillEvent",
                               std::move(last_autofill_event));

  EXPECT_EQ(
      data_logs::FetchAutofillFeedbackData(browser_autofill_manager_.get()),
      expected_data->GetDict());
}

TEST_F(AutofillFeedbackDataUnitTest,
       NotIncludeLastAutofillEventIfExceedTimeLimit) {
  TestAutofillClock clock(AutofillClock::Now());
  FormData form;
  CreateFeedbackTestFormData(&form);
  FormFieldData field = form.fields[0];
  browser_autofill_manager_->OnFormsSeen(
      /*updated_forms=*/{form},
      /*removed_forms=*/{});

  // Simulates an autofill event.
  browser_autofill_manager_->OnSingleFieldSuggestionSelected(
      u"TestValue", POPUP_ITEM_ID_IBAN_ENTRY, form, field);

  // Advance the clock 4 minutes should disregard the last autofill event log.
  clock.Advance(base::Minutes(4));

  // Expected data does not contain the last_autofill_event entry.
  auto expected_data = base::JSONReader::ReadAndReturnValueWithError(
      kExpectedFeedbackDataJSON,
      base::JSONParserOptions::JSON_ALLOW_TRAILING_COMMAS);
  ASSERT_TRUE(expected_data.has_value()) << expected_data.error().message;
  ASSERT_TRUE(expected_data->is_dict());

  EXPECT_EQ(
      data_logs::FetchAutofillFeedbackData(browser_autofill_manager_.get()),
      expected_data->GetDict());
}

TEST_F(AutofillFeedbackDataUnitTest, IncludesExtraLogs) {
  FormData form;
  CreateFeedbackTestFormData(&form);
  browser_autofill_manager_->OnFormsSeen(
      /*updated_forms=*/{form},
      /*removed_forms=*/{});

  base::Value::Dict extra_logs;
  extra_logs.Set("triggerFormSignature", "123");
  extra_logs.Set("triggerFieldSignature", "456");

  base::Value::Dict autofill_feedback_data =
      data_logs::FetchAutofillFeedbackData(browser_autofill_manager_.get(),
                                           extra_logs.Clone());

  auto expected_data = base::JSONReader::ReadAndReturnValueWithError(
      kExpectedFeedbackDataJSON,
      base::JSONParserOptions::JSON_ALLOW_TRAILING_COMMAS);
  ASSERT_TRUE(expected_data.has_value()) << expected_data.error().message;
  ASSERT_TRUE(expected_data->is_dict());
  // Include extra logs in the expected report.
  expected_data->GetDict().Merge(std::move(extra_logs));
  EXPECT_EQ(autofill_feedback_data, expected_data->GetDict());
}

}  // namespace autofill
