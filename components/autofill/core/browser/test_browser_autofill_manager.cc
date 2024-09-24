// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/autofill/core/browser/test_browser_autofill_manager.h"

#include "base/check_deref.h"
#include "base/strings/stringprintf.h"
#include "base/strings/utf_string_conversions.h"
#include "components/autofill/core/browser/autofill_test_utils.h"
#include "components/autofill/core/browser/browser_autofill_manager_test_api.h"
#include "components/autofill/core/browser/field_types.h"
#include "components/autofill/core/browser/form_structure.h"
#include "components/autofill/core/browser/form_structure_test_api.h"
#include "components/autofill/core/browser/mock_single_field_form_fill_router.h"
#include "components/autofill/core/browser/test_autofill_client.h"
#include "components/autofill/core/browser/test_autofill_driver.h"
#include "components/autofill/core/browser/test_autofill_manager_waiter.h"
#include "components/autofill/core/browser/test_form_filler.h"
#include "components/autofill/core/browser/test_personal_data_manager.h"
#include "components/autofill/core/browser/ui/suggestion.h"
#include "components/autofill/core/common/autofill_features.h"
#include "components/autofill/core/common/autofill_prefs.h"
#include "services/network/public/cpp/shared_url_loader_factory.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace autofill {

TestBrowserAutofillManager::TestBrowserAutofillManager(AutofillDriver* driver)
    : BrowserAutofillManager(driver, "en-US") {
  test_api(*this).set_form_filler(
      std::make_unique<TestFormFiller>(*this, log_manager(), "en-US"));
}

TestBrowserAutofillManager::~TestBrowserAutofillManager() = default;

void TestBrowserAutofillManager::OnLanguageDetermined(
    const translate::LanguageDetectionDetails& details) {
  AutofillManager::OnLanguageDetermined(details);
  ASSERT_TRUE(waiter_.Wait(0));
}

void TestBrowserAutofillManager::OnFormsSeen(
    const std::vector<FormData>& updated_forms,
    const std::vector<FormGlobalId>& removed_forms) {
  AutofillManager::OnFormsSeen(updated_forms, removed_forms);
  ASSERT_TRUE(waiter_.Wait(0));
}

void TestBrowserAutofillManager::OnCaretMovedInFormField(
    const FormData& form,
    const FieldGlobalId& field_id,
    const gfx::Rect& caret_bounds) {
  AutofillManager::OnCaretMovedInFormField(form, field_id, caret_bounds);
  ASSERT_TRUE(waiter_.Wait(0));
}

void TestBrowserAutofillManager::OnTextFieldDidChange(
    const FormData& form,
    const FieldGlobalId& field_id,
    const base::TimeTicks timestamp) {
  AutofillManager::OnTextFieldDidChange(form, field_id, timestamp);
  ASSERT_TRUE(waiter_.Wait(0));
}

void TestBrowserAutofillManager::OnTextFieldDidScroll(
    const FormData& form,
    const FieldGlobalId& field_id) {
  AutofillManager::OnTextFieldDidScroll(form, field_id);
  ASSERT_TRUE(waiter_.Wait(0));
}

void TestBrowserAutofillManager::OnSelectControlDidChange(
    const FormData& form,
    const FieldGlobalId& field_id) {
  AutofillManager::OnSelectControlDidChange(form, field_id);
  ASSERT_TRUE(waiter_.Wait(0));
}

void TestBrowserAutofillManager::OnAskForValuesToFill(
    const FormData& form,
    const FieldGlobalId& field_id,
    const gfx::Rect& caret_bounds,
    AutofillSuggestionTriggerSource trigger_source) {
  AutofillManager::OnAskForValuesToFill(form, field_id, caret_bounds,
                                        trigger_source);
  ASSERT_TRUE(waiter_.Wait(0));
}

void TestBrowserAutofillManager::OnFocusOnFormField(
    const FormData& form,
    const FieldGlobalId& field_id) {
  AutofillManager::OnFocusOnFormField(form, field_id);
  ASSERT_TRUE(waiter_.Wait(0));
}

void TestBrowserAutofillManager::OnDidFillAutofillFormData(
    const FormData& form,
    const base::TimeTicks timestamp) {
  AutofillManager::OnDidFillAutofillFormData(form, timestamp);
  ASSERT_TRUE(waiter_.Wait(0));
}

void TestBrowserAutofillManager::OnJavaScriptChangedAutofilledValue(
    const FormData& form,
    const FieldGlobalId& field_id,
    const std::u16string& old_value,
    bool formatting_only) {
  AutofillManager::OnJavaScriptChangedAutofilledValue(form, field_id, old_value,
                                                      formatting_only);
  ASSERT_TRUE(waiter_.Wait(0));
}

void TestBrowserAutofillManager::OnFormSubmitted(
    const FormData& form,
    const bool known_success,
    const mojom::SubmissionSource source) {
  AutofillManager::OnFormSubmitted(form, known_success, source);
  ASSERT_TRUE(waiter_.Wait(0));
}

bool TestBrowserAutofillManager::IsAutofillProfileEnabled() const {
  return autofill_profile_enabled_;
}

bool TestBrowserAutofillManager::IsAutofillPaymentMethodsEnabled() const {
  return autofill_payment_methods_enabled_;
}

void TestBrowserAutofillManager::UploadVotesAndLogQuality(
    std::unique_ptr<FormStructure> submitted_form,
    base::TimeTicks interaction_time,
    base::TimeTicks submission_time,
    bool observed_submission,
    const ukm::SourceId source_id) {
  submitted_form_signature_ = submitted_form->FormSignatureAsStr();

  if (observed_submission) {
    // In case no submission was observed, the run_loop is quit in
    // StoreUploadVotesAndLogQualityCallback.
    run_loop_->Quit();
  }

  if (expected_observed_submission_ != std::nullopt) {
    EXPECT_EQ(expected_observed_submission_, observed_submission);
  }

  // If we have expected field types set, make sure they match.
  if (!expected_submitted_field_types_.empty()) {
    ASSERT_EQ(expected_submitted_field_types_.size(),
              submitted_form->field_count());
    for (size_t i = 0; i < expected_submitted_field_types_.size(); ++i) {
      SCOPED_TRACE(base::StringPrintf(
          "Field %d with value %s", static_cast<int>(i),
          base::UTF16ToUTF8(
              submitted_form->field(i)->value(ValueSemantics::kCurrent))
              .c_str()));
      const FieldTypeSet& possible_types =
          submitted_form->field(i)->possible_types();
      EXPECT_EQ(expected_submitted_field_types_[i].size(),
                possible_types.size());
      for (auto it : expected_submitted_field_types_[i]) {
        EXPECT_TRUE(possible_types.count(it))
            << "Expected type: " << FieldTypeToStringView(it);
      }
    }
  }

  BrowserAutofillManager::UploadVotesAndLogQuality(
      std::move(submitted_form), interaction_time, submission_time,
      observed_submission, source_id);
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
    const std::vector<FieldType>& heuristic_types,
    const std::vector<FieldType>& server_types,
    bool preserve_values_in_form_structure) {
  std::vector<std::vector<std::pair<HeuristicSource, FieldType>>>
      all_heuristic_types;
  for (FieldType type : heuristic_types) {
    all_heuristic_types.push_back({{GetActiveHeuristicSource(), type}});
  }
  AddSeenForm(form, all_heuristic_types, server_types,
              preserve_values_in_form_structure);
}

void TestBrowserAutofillManager::AddSeenForm(
    const FormData& form,
    const std::vector<std::vector<std::pair<HeuristicSource, FieldType>>>&
        heuristic_types,
    const std::vector<FieldType>& server_types,
    bool preserve_values_in_form_structure) {
  auto form_structure = std::make_unique<FormStructure>(
      preserve_values_in_form_structure ? form : test::WithoutValues(form));
  test_api(*form_structure).SetFieldTypes(heuristic_types, server_types);
  test_api(*form_structure).AssignSections();
  AddSeenFormStructure(std::move(form_structure));
  test_api(*this).OnFormsParsed({form});
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
    const FieldGlobalId& field_id,
    AutofillSuggestionTriggerSource trigger_source) {
  gfx::PointF p =
      CHECK_DEREF(form.FindFieldByGlobalId(field_id)).bounds().origin();
  gfx::Rect caret_bounds(gfx::Point(p.x(), p.y()), gfx::Size(0, 10));
  BrowserAutofillManager::OnAskForValuesToFill(form, field_id, caret_bounds,
                                               trigger_source);
  ASSERT_TRUE(waiter_.Wait(0));
}

void TestBrowserAutofillManager::SetAutofillProfileEnabled(
    TestAutofillClient& client,
    bool autofill_profile_enabled) {
  autofill_profile_enabled_ = autofill_profile_enabled;
  if (PrefService* prefs = client.GetPrefs()) {
    prefs->SetBoolean(prefs::kAutofillProfileEnabled, autofill_profile_enabled);
  }
  if (!autofill_profile_enabled_) {
    // Profile data is refreshed when this pref is changed.
    client.GetPersonalDataManager()
        ->test_address_data_manager()
        .ClearProfiles();
  }
}

void TestBrowserAutofillManager::SetAutofillPaymentMethodsEnabled(
    TestAutofillClient& client,
    bool autofill_payment_methods_enabled) {
  autofill_payment_methods_enabled_ = autofill_payment_methods_enabled;
  if (PrefService* prefs = client.GetPrefs()) {
    prefs->SetBoolean(prefs::kAutofillCreditCardEnabled,
                      autofill_payment_methods_enabled);
  }
  if (!autofill_payment_methods_enabled) {
    // Credit card data is refreshed when this pref is changed.
    client.GetPersonalDataManager()
        ->test_payments_data_manager()
        .ClearCreditCards();
  }
}

void TestBrowserAutofillManager::SetExpectedSubmittedFieldTypes(
    const std::vector<FieldTypeSet>& expected_types) {
  expected_submitted_field_types_ = expected_types;
}

void TestBrowserAutofillManager::SetExpectedObservedSubmission(bool expected) {
  expected_observed_submission_ = expected;
}

}  // namespace autofill
