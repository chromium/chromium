// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/autofill/core/browser/test_browser_autofill_manager.h"

#include "base/strings/stringprintf.h"
#include "base/strings/utf_string_conversions.h"
#include "components/autofill/core/browser/autofill_suggestion_generator.h"
#include "components/autofill/core/browser/field_types.h"
#include "components/autofill/core/browser/mock_single_field_form_fill_router.h"
#include "components/autofill/core/browser/test_autofill_client.h"
#include "components/autofill/core/browser/test_autofill_driver.h"
#include "components/autofill/core/browser/test_form_structure.h"
#include "components/autofill/core/browser/test_personal_data_manager.h"
#include "services/network/public/cpp/shared_url_loader_factory.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace autofill {

TestBrowserAutofillManager::TestBrowserAutofillManager(
    TestAutofillDriver* driver,
    TestAutofillClient* client)
    : BrowserAutofillManager(driver,
                             client,
                             "en-US",
                             EnableDownloadManager(false)),
      client_(client),
      driver_(driver) {}

TestBrowserAutofillManager::~TestBrowserAutofillManager() = default;

bool TestBrowserAutofillManager::IsAutofillProfileEnabled() const {
  return autofill_profile_enabled_;
}

bool TestBrowserAutofillManager::IsAutofillCreditCardEnabled() const {
  return autofill_credit_card_enabled_;
}

void TestBrowserAutofillManager::UploadFormData(
    const FormStructure& submitted_form,
    bool observed_submission) {
  submitted_form_signature_ = submitted_form.FormSignatureAsStr();

  if (call_parent_upload_form_data_)
    BrowserAutofillManager::UploadFormData(submitted_form, observed_submission);
}

void TestBrowserAutofillManager::ScheduleRefill(const FormData& form) {
  TriggerRefillForTest(form);
}

bool TestBrowserAutofillManager::MaybeStartVoteUploadProcess(
    std::unique_ptr<FormStructure> form_structure,
    bool observed_submission) {
  run_loop_ = std::make_unique<base::RunLoop>();
  if (BrowserAutofillManager::MaybeStartVoteUploadProcess(
          std::move(form_structure), observed_submission)) {
    run_loop_->Run();
    return true;
  }
  return false;
}

void TestBrowserAutofillManager::UploadFormDataAsyncCallback(
    const FormStructure* submitted_form,
    const base::TimeTicks& interaction_time,
    const base::TimeTicks& submission_time,
    bool observed_submission) {
  run_loop_->Quit();

  if (expected_observed_submission_ != absl::nullopt)
    EXPECT_EQ(expected_observed_submission_, observed_submission);

  // If we have expected field types set, make sure they match.
  if (!expected_submitted_field_types_.empty()) {
    ASSERT_EQ(expected_submitted_field_types_.size(),
              submitted_form->field_count());
    for (size_t i = 0; i < expected_submitted_field_types_.size(); ++i) {
      SCOPED_TRACE(base::StringPrintf(
          "Field %d with value %s", static_cast<int>(i),
          base::UTF16ToUTF8(submitted_form->field(i)->value).c_str()));
      const ServerFieldTypeSet& possible_types =
          submitted_form->field(i)->possible_types();
      EXPECT_EQ(expected_submitted_field_types_[i].size(),
                possible_types.size());
      for (auto it : expected_submitted_field_types_[i]) {
        EXPECT_TRUE(possible_types.count(it))
            << "Expected type: " << AutofillType(it).ToString();
      }
    }
  }

  BrowserAutofillManager::UploadFormDataAsyncCallback(
      submitted_form, interaction_time, submission_time, observed_submission);
}

int TestBrowserAutofillManager::GetPackedCreditCardID(int credit_card_id) {
  std::string credit_card_guid =
      base::StringPrintf("00000000-0000-0000-0000-%012d", credit_card_id);

  return suggestion_generator()->MakeFrontendId(credit_card_guid,
                                                std::string());
}

void TestBrowserAutofillManager::AddSeenForm(
    const FormData& form,
    const std::vector<ServerFieldType>& heuristic_types,
    const std::vector<ServerFieldType>& server_types,
    bool preserve_values_in_form_structure) {
  std::vector<std::vector<std::pair<PatternSource, ServerFieldType>>>
      all_heuristic_types;

  base::ranges::transform(
      heuristic_types, std::back_inserter(all_heuristic_types),
      [](ServerFieldType type)
          -> std::vector<std::pair<PatternSource, ServerFieldType>> {
        return {{GetActivePatternSource(), type}};
      });

  AddSeenForm(form, all_heuristic_types, server_types,
              preserve_values_in_form_structure);
}

void TestBrowserAutofillManager::AddSeenForm(
    const FormData& form,
    const std::vector<std::vector<std::pair<PatternSource, ServerFieldType>>>&
        heuristic_types,
    const std::vector<ServerFieldType>& server_types,
    bool preserve_values_in_form_structure) {
  FormData form_with_empty_fields = form;
  for (auto& field : form_with_empty_fields.fields) {
    field.value = std::u16string();
  }

  std::unique_ptr<TestFormStructure> form_structure =
      std::make_unique<TestFormStructure>(
          preserve_values_in_form_structure ? form : form_with_empty_fields);
  form_structure->SetFieldTypes(heuristic_types, server_types);
  form_structure->identify_sections_for_testing();
  AddSeenFormStructure(std::move(form_structure));

  form_interactions_ukm_logger()->OnFormsParsed(client()->GetUkmSourceId());
}

void TestBrowserAutofillManager::AddSeenFormStructure(
    std::unique_ptr<FormStructure> form_structure) {
  const auto id = form_structure->global_id();
  (*mutable_form_structures())[id] = std::move(form_structure);
}

void TestBrowserAutofillManager::ClearFormStructures() {
  mutable_form_structures()->clear();
}

const std::string TestBrowserAutofillManager::GetSubmittedFormSignature() {
  return submitted_form_signature_;
}

void TestBrowserAutofillManager::OnAskForValuesToFillTest(
    const FormData& form,
    const FormFieldData& field,
    int query_id,
    const gfx::RectF& bounding_box,
    bool autoselect_first_suggestion,
    TouchToFillEligible touch_to_fill_eligible) {
  BrowserAutofillManager::OnAskForValuesToFill(
      form, field, bounding_box, query_id, autoselect_first_suggestion,
      touch_to_fill_eligible);
}

void TestBrowserAutofillManager::SetAutofillProfileEnabled(
    bool autofill_profile_enabled) {
  autofill_profile_enabled_ = autofill_profile_enabled;
  if (!autofill_profile_enabled_) {
    // Profile data is refreshed when this pref is changed.
    client()->GetPersonalDataManager()->ClearProfiles();
  }
}

void TestBrowserAutofillManager::SetAutofillCreditCardEnabled(
    bool autofill_credit_card_enabled) {
  autofill_credit_card_enabled_ = autofill_credit_card_enabled;
  if (!autofill_credit_card_enabled_) {
    // Credit card data is refreshed when this pref is changed.
    client()->GetPersonalDataManager()->ClearCreditCards();
  }
}

void TestBrowserAutofillManager::SetExpectedSubmittedFieldTypes(
    const std::vector<ServerFieldTypeSet>& expected_types) {
  expected_submitted_field_types_ = expected_types;
}

void TestBrowserAutofillManager::SetExpectedObservedSubmission(bool expected) {
  expected_observed_submission_ = expected;
}

void TestBrowserAutofillManager::SetCallParentUploadFormData(bool value) {
  call_parent_upload_form_data_ = value;
}

}  // namespace autofill
