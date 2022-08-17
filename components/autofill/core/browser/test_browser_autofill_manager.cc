// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/autofill/core/browser/test_browser_autofill_manager.h"

#include "autofill_test_utils.h"
#include "base/strings/stringprintf.h"
#include "base/strings/utf_string_conversions.h"
#include "components/autofill/core/browser/autofill_suggestion_generator.h"
#include "components/autofill/core/browser/field_types.h"
#include "components/autofill/core/browser/form_structure.h"
#include "components/autofill/core/browser/mock_single_field_form_fill_router.h"
#include "components/autofill/core/browser/test_autofill_client.h"
#include "components/autofill/core/browser/test_autofill_driver.h"
#include "components/autofill/core/browser/test_autofill_manager_waiter.h"
#include "components/autofill/core/browser/test_personal_data_manager.h"
#include "form_structure_test_api.h"
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

void TestBrowserAutofillManager::OnLanguageDetermined(
    const translate::LanguageDetectionDetails& details) {
  TestAutofillManagerWaiter waiter(
      *this, {&AutofillManager::Observer::OnAfterLanguageDetermined});
  AutofillManager::OnLanguageDetermined(details);
  ASSERT_TRUE(waiter.Wait());
}

void TestBrowserAutofillManager::OnFormsSeen(
    const std::vector<FormData>& updated_forms,
    const std::vector<FormGlobalId>& removed_forms) {
  TestAutofillManagerWaiter waiter(*this, {&Observer::OnAfterFormsSeen});
  AutofillManager::OnFormsSeen(updated_forms, removed_forms);
  ASSERT_TRUE(waiter.Wait());
}

void TestBrowserAutofillManager::OnTextFieldDidChange(
    const FormData& form,
    const FormFieldData& field,
    const gfx::RectF& bounding_box,
    const base::TimeTicks timestamp) {
  TestAutofillManagerWaiter waiter(*this,
                                   {&Observer::OnAfterTextFieldDidChange});
  AutofillManager::OnTextFieldDidChange(form, field, bounding_box, timestamp);
  ASSERT_TRUE(waiter.Wait());
}

void TestBrowserAutofillManager::OnDidFillAutofillFormData(
    const FormData& form,
    const base::TimeTicks timestamp) {
  TestAutofillManagerWaiter waiter(*this,
                                   {&Observer::OnAfterDidFillAutofillFormData});
  AutofillManager::OnDidFillAutofillFormData(form, timestamp);
  ASSERT_TRUE(waiter.Wait());
}

void TestBrowserAutofillManager::OnAskForValuesToFill(
    const FormData& form,
    const FormFieldData& field,
    const gfx::RectF& bounding_box,
    int query_id,
    bool autoselect_first_suggestion,
    TouchToFillEligible touch_to_fill_eligible) {
  TestAutofillManagerWaiter waiter(*this,
                                   {&Observer::OnAfterAskForValuesToFill});
  AutofillManager::OnAskForValuesToFill(form, field, bounding_box, query_id,
                                        autoselect_first_suggestion,
                                        touch_to_fill_eligible);
  ASSERT_TRUE(waiter.Wait());
}

void TestBrowserAutofillManager::OnJavaScriptChangedAutofilledValue(
    const FormData& form,
    const FormFieldData& field,
    const std::u16string& old_value) {
  TestAutofillManagerWaiter waiter(
      *this, {&Observer::OnAfterJavaScriptChangedAutofilledValue});
  AutofillManager::OnJavaScriptChangedAutofilledValue(form, field, old_value);
  ASSERT_TRUE(waiter.Wait());
}

void TestBrowserAutofillManager::OnFormSubmitted(
    const FormData& form,
    const bool known_success,
    const mojom::SubmissionSource source) {
  TestAutofillManagerWaiter waiter(*this, {&Observer::OnAfterFormsSeen});
  AutofillManager::OnFormSubmitted(form, known_success, source);
  ASSERT_TRUE(waiter.Wait());
}

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
  for (ServerFieldType type : heuristic_types)
    all_heuristic_types.push_back({{GetActivePatternSource(), type}});
  AddSeenForm(form, all_heuristic_types, server_types,
              preserve_values_in_form_structure);
}

void TestBrowserAutofillManager::AddSeenForm(
    const FormData& form,
    const std::vector<std::vector<std::pair<PatternSource, ServerFieldType>>>&
        heuristic_types,
    const std::vector<ServerFieldType>& server_types,
    bool preserve_values_in_form_structure) {
  auto form_structure = std::make_unique<FormStructure>(
      preserve_values_in_form_structure ? form : test::WithoutValues(form));
  FormGlobalId form_id = form_structure->global_id();
  AddSeenFormStructure(std::move(form_structure));
  SetSeenFormPredictions(form_id, heuristic_types, server_types);
  form_interactions_ukm_logger()->OnFormsParsed(client()->GetUkmSourceId());
}

void TestBrowserAutofillManager::SetSeenFormPredictions(
    FormGlobalId form_id,
    const std::vector<ServerFieldType>& heuristic_types,
    const std::vector<ServerFieldType>& server_types) {
  std::vector<std::vector<std::pair<PatternSource, ServerFieldType>>>
      all_heuristic_types;
  for (ServerFieldType type : heuristic_types)
    all_heuristic_types.push_back({{GetActivePatternSource(), type}});
  SetSeenFormPredictions(form_id, all_heuristic_types, server_types);
}

void TestBrowserAutofillManager::SetSeenFormPredictions(
    FormGlobalId form_id,
    const std::vector<std::vector<std::pair<PatternSource, ServerFieldType>>>&
        heuristic_types,
    const std::vector<ServerFieldType>& server_types) {
  FormStructure* form_structure = FindCachedFormByRendererId(form_id);
  ASSERT_TRUE(form_structure);
  FormStructureTestApi(form_structure)
      .SetFieldTypes(heuristic_types, server_types);
  form_structure->identify_sections_for_testing();
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
  TestAutofillManagerWaiter waiter(
      *this, {&AutofillManager::Observer::OnAfterAskForValuesToFill});
  BrowserAutofillManager::OnAskForValuesToFill(
      form, field, bounding_box, query_id, autoselect_first_suggestion,
      touch_to_fill_eligible);
  ASSERT_TRUE(waiter.Wait());
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
