// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/autofill/core/browser/test_browser_autofill_manager.h"

#include "autofill_test_utils.h"
#include "base/strings/stringprintf.h"
#include "base/strings/utf_string_conversions.h"
#include "components/autofill/core/browser/autofill_suggestion_generator.h"
#include "components/autofill/core/browser/browser_autofill_manager_test_api.h"
#include "components/autofill/core/browser/field_types.h"
#include "components/autofill/core/browser/form_structure.h"
#include "components/autofill/core/browser/form_structure_test_api.h"
#include "components/autofill/core/browser/mock_single_field_form_fill_router.h"
#include "components/autofill/core/browser/test_autofill_client.h"
#include "components/autofill/core/browser/test_autofill_driver.h"
#include "components/autofill/core/browser/test_autofill_manager_waiter.h"
#include "components/autofill/core/browser/test_personal_data_manager.h"
#include "components/autofill/core/browser/ui/suggestion.h"
#include "components/autofill/core/common/autofill_features.h"
#include "services/network/public/cpp/shared_url_loader_factory.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace autofill {

TestBrowserAutofillManager::TestBrowserAutofillManager(AutofillDriver* driver,
                                                       AutofillClient* client)
    : BrowserAutofillManager(driver, client, "en-US") {}

TestBrowserAutofillManager::~TestBrowserAutofillManager() = default;

void TestBrowserAutofillManager::OnLanguageDetermined(
    const translate::LanguageDetectionDetails& details) {
  TestAutofillManagerWaiter waiter(*this,
                                   {AutofillManagerEvent::kLanguageDetermined});
  AutofillManager::OnLanguageDetermined(details);
  ASSERT_TRUE(waiter.Wait());
}

void TestBrowserAutofillManager::OnFormsSeen(
    const std::vector<FormData>& updated_forms,
    const std::vector<FormGlobalId>& removed_forms) {
  TestAutofillManagerWaiter waiter(*this, {AutofillManagerEvent::kFormsSeen});
  AutofillManager::OnFormsSeen(updated_forms, removed_forms);
  ASSERT_TRUE(waiter.Wait());
}

void TestBrowserAutofillManager::OnTextFieldDidChange(
    const FormData& form,
    const FormFieldData& field,
    const gfx::RectF& bounding_box,
    const base::TimeTicks timestamp) {
  TestAutofillManagerWaiter waiter(*this,
                                   {AutofillManagerEvent::kTextFieldDidChange});
  AutofillManager::OnTextFieldDidChange(form, field, bounding_box, timestamp);
  ASSERT_TRUE(waiter.Wait());
}

void TestBrowserAutofillManager::OnDidFillAutofillFormData(
    const FormData& form,
    const base::TimeTicks timestamp) {
  TestAutofillManagerWaiter waiter(
      *this, {AutofillManagerEvent::kDidFillAutofillFormData});
  AutofillManager::OnDidFillAutofillFormData(form, timestamp);
  ASSERT_TRUE(waiter.Wait());
}

void TestBrowserAutofillManager::OnAskForValuesToFill(
    const FormData& form,
    const FormFieldData& field,
    const gfx::RectF& bounding_box,
    AutofillSuggestionTriggerSource trigger_source) {
  TestAutofillManagerWaiter waiter(*this,
                                   {AutofillManagerEvent::kAskForValuesToFill});
  AutofillManager::OnAskForValuesToFill(form, field, bounding_box,
                                        trigger_source);
  ASSERT_TRUE(waiter.Wait());
}

void TestBrowserAutofillManager::OnJavaScriptChangedAutofilledValue(
    const FormData& form,
    const FormFieldData& field,
    const std::u16string& old_value) {
  TestAutofillManagerWaiter waiter(
      *this, {AutofillManagerEvent::kJavaScriptChangedAutofilledValue});
  AutofillManager::OnJavaScriptChangedAutofilledValue(form, field, old_value);
  ASSERT_TRUE(waiter.Wait());
}

void TestBrowserAutofillManager::OnFormSubmitted(
    const FormData& form,
    const bool known_success,
    const mojom::SubmissionSource source) {
  TestAutofillManagerWaiter waiter(*this, {AutofillManagerEvent::kFormsSeen});
  AutofillManager::OnFormSubmitted(form, known_success, source);
  ASSERT_TRUE(waiter.Wait());
}

bool TestBrowserAutofillManager::IsAutofillProfileEnabled() const {
  return autofill_profile_enabled_;
}

bool TestBrowserAutofillManager::IsAutofillCreditCardEnabled() const {
  return autofill_credit_card_enabled_;
}

void TestBrowserAutofillManager::UploadVotesAndLogQuality(
    std::unique_ptr<FormStructure> submitted_form,
    base::TimeTicks interaction_time,
    base::TimeTicks submission_time,
    bool observed_submission) {
  submitted_form_signature_ = submitted_form->FormSignatureAsStr();

  if (observed_submission) {
    // In case no submission was observed, the run_loop is quit in
    // StoreUploadVotesAndLogQualityCallback.
    run_loop_->Quit();
  }

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

  BrowserAutofillManager::UploadVotesAndLogQuality(
      std::move(submitted_form), interaction_time, submission_time,
      observed_submission);
}

void TestBrowserAutofillManager::StoreUploadVotesAndLogQualityCallback(
    FormSignature form_signature,
    base::OnceClosure callback) {
  BrowserAutofillManager::StoreUploadVotesAndLogQualityCallback(
      form_signature, std::move(callback));
  run_loop_->Quit();
}

const gfx::Image& TestBrowserAutofillManager::GetCardImage(
    const CreditCard& credit_card) {
  return card_image_;
}

void TestBrowserAutofillManager::ScheduleRefill(
    const FormData& form,
    const AutofillTriggerSource trigger_source) {
  test_api(*this).TriggerRefill(form, trigger_source);
}

bool TestBrowserAutofillManager::MaybeStartVoteUploadProcess(
    std::unique_ptr<FormStructure> form_structure,
    bool observed_submission) {
  // The purpose of this runloop is to ensure that the field type determination
  // finishes. If `observed_submission` is true, it's terminated in
  // LogQualityAndUploadVotes. Otherwise, it is already terminated in
  // StoreUploadVotesAndLogQualityCallback.
  run_loop_ = std::make_unique<base::RunLoop>();
  if (BrowserAutofillManager::MaybeStartVoteUploadProcess(
          std::move(form_structure), observed_submission)) {
    run_loop_->Run();
    return true;
  }
  return false;
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
  test_api(*form_structure).SetFieldTypes(heuristic_types, server_types);
  test_api(*form_structure).IdentifySections(/*ignore_autocomplete=*/false);
  AddSeenFormStructure(std::move(form_structure));
  form_interactions_ukm_logger()->OnFormsParsed(client().GetUkmSourceId());
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
    const gfx::RectF& bounding_box,
    AutofillSuggestionTriggerSource trigger_source) {
  TestAutofillManagerWaiter waiter(*this,
                                   {AutofillManagerEvent::kAskForValuesToFill});
  BrowserAutofillManager::OnAskForValuesToFill(form, field, bounding_box,
                                               trigger_source);
  ASSERT_TRUE(waiter.Wait());
}

void TestBrowserAutofillManager::SetAutofillProfileEnabled(
    TestAutofillClient& client,
    bool autofill_profile_enabled) {
  autofill_profile_enabled_ = autofill_profile_enabled;
  if (!autofill_profile_enabled_) {
    // Profile data is refreshed when this pref is changed.
    client.GetPersonalDataManager()->ClearProfiles();
  }
}

void TestBrowserAutofillManager::SetAutofillCreditCardEnabled(
    TestAutofillClient& client,
    bool autofill_credit_card_enabled) {
  autofill_credit_card_enabled_ = autofill_credit_card_enabled;
  if (!autofill_credit_card_enabled_) {
    // Credit card data is refreshed when this pref is changed.
    client.GetPersonalDataManager()->ClearCreditCards();
  }
}

void TestBrowserAutofillManager::SetExpectedSubmittedFieldTypes(
    const std::vector<ServerFieldTypeSet>& expected_types) {
  expected_submitted_field_types_ = expected_types;
}

void TestBrowserAutofillManager::SetExpectedObservedSubmission(bool expected) {
  expected_observed_submission_ = expected;
}

}  // namespace autofill
