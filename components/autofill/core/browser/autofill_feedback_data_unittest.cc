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
   "form_structures": [ {
      "form_signature": "4232380759432074174",
      "host_frame": "00000000000000000000000000000000",
      "id_attribute": "",
      "main_frame_url": "https://myform_root.com",
      "name_attribute": "",
      "renderer_id": "11",
      "source_url": "https://myform.com",
      "fields": [ {
         "autocomplete_attribute": "cc-given-name",
         "field_signature": "3879476562",
         "field_type": "HTML_TYPE_CREDIT_CARD_NAME_FIRST",
         "heuristic_type": "CREDIT_CARD_NAME_FIRST",
         "host_form_signature": "0",
         "html_type": "HTML_TYPE_CREDIT_CARD_NAME_FIRST",
         "id_attribute": "",
         "is_empty": true,
         "is_focusable": true,
         "is_visible": true,
         "label_attribute": "First Name on Card",
         "parseable_name_attribute": "",
         "placeholder_attribute": "",
         "section": "firstnameoncard_0_11",
         "server_type": "NO_SERVER_DATA",
         "server_type_is_override": false
      }, {
         "autocomplete_attribute": "cc-family-name",
         "field_signature": "3213606822",
         "field_type": "HTML_TYPE_CREDIT_CARD_NAME_LAST",
         "heuristic_type": "CREDIT_CARD_NAME_LAST",
         "host_form_signature": "0",
         "html_type": "HTML_TYPE_CREDIT_CARD_NAME_LAST",
         "id_attribute": "",
         "is_empty": true,
         "is_focusable": true,
         "is_visible": true,
         "label_attribute": "Last Name on Card",
         "parseable_name_attribute": "",
         "placeholder_attribute": "",
         "section": "firstnameoncard_0_11",
         "server_type": "NO_SERVER_DATA",
         "server_type_is_override": false
      }, {
         "autocomplete_attribute": "cc-family-name",
         "field_signature": "1029417091",
         "field_type": "HTML_TYPE_CREDIT_CARD_NAME_LAST",
         "heuristic_type": "EMAIL_ADDRESS",
         "host_form_signature": "0",
         "html_type": "HTML_TYPE_CREDIT_CARD_NAME_LAST",
         "id_attribute": "",
         "is_empty": true,
         "is_focusable": true,
         "is_visible": true,
         "label_attribute": "Email",
         "parseable_name_attribute": "",
         "placeholder_attribute": "",
         "section": "firstnameoncard_0_11",
         "server_type": "NO_SERVER_DATA",
         "server_type_is_override": false
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
  test::AutofillEnvironment autofill_environment_;
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
  last_autofill_event.Set("associated_country", "");
  last_autofill_event.Set("type", "SingleFieldFormFillerIban");
  expected_data->GetDict().Set("last_autofill_event",
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

}  // namespace autofill
