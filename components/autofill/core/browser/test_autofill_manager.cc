// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/autofill/core/browser/test_autofill_manager.h"

#include "base/strings/stringprintf.h"
#include "base/strings/utf_string_conversions.h"
#include "components/autofill/core/browser/field_types.h"
#include "components/autofill/core/browser/mock_autocomplete_history_manager.h"
#include "components/autofill/core/browser/test_form_structure.h"
#include "components/autofill/core/browser/test_personal_data_manager.h"
#include "services/network/public/cpp/shared_url_loader_factory.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace autofill {

TestAutofillManager::TestAutofillManager(
    AutofillDriver* driver,
    AutofillClient* client,
    TestPersonalDataManager* personal_data,
    MockAutocompleteHistoryManager* autocomplete_history_manager)
    : AutofillManager(driver,
                      client,
                      personal_data,
                      autocomplete_history_manager),
      personal_data_(personal_data) {}

TestAutofillManager::~TestAutofillManager() {}

bool TestAutofillManager::IsAutofillProfileEnabled() const {
  return autofill_profile_enabled_;
}

bool TestAutofillManager::IsAutofillCreditCardEnabled() const {
  return autofill_credit_card_enabled_;
}

void TestAutofillManager::UploadFormData(const FormStructure& submitted_form,
                                         bool observed_submission) {
  submitted_form_signature_ = submitted_form.FormSignatureAsStr();

  if (call_parent_upload_form_data_)
    AutofillManager::UploadFormData(submitted_form, observed_submission);
}

bool TestAutofillManager::MaybeStartVoteUploadProcess(
    std::unique_ptr<FormStructure> form_structure,
    bool observed_submission) {
  run_loop_ = std::make_unique<base::RunLoop>();
  if (AutofillManager::MaybeStartVoteUploadProcess(std::move(form_structure),
                                                   observed_submission)) {
    run_loop_->Run();
    return true;
  }
  return false;
}

void TestAutofillManager::UploadFormDataAsyncCallback(
    const FormStructure* submitted_form,
    const base::TimeTicks& interaction_time,
    const base::TimeTicks& submission_time,
    bool observed_submission) {
  run_loop_->Quit();

  if (expected_observed_submission_ != base::nullopt)
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
      for (auto it = expected_submitted_field_types_[i].begin();
           it != expected_submitted_field_types_[i].end(); ++it) {
        EXPECT_TRUE(possible_types.count(*it))
            << "Expected type: " << AutofillType(*it).ToString();
      }
    }
  }

  AutofillManager::UploadFormDataAsyncCallback(
      submitted_form, interaction_time, submission_time, observed_submission);
}

int TestAutofillManager::GetPackedCreditCardID(int credit_card_id) {
  std::string credit_card_guid =
      base::StringPrintf("00000000-0000-0000-0000-%012d", credit_card_id);

  return MakeFrontendID(credit_card_guid, std::string());
}

void TestAutofillManager::AddSeenForm(
    const FormData& form,
    const std::vector<ServerFieldType>& heuristic_types,
    const std::vector<ServerFieldType>& server_types) {
  FormData empty_form = form;
  for (size_t i = 0; i < empty_form.fields.size(); ++i) {
    empty_form.fields[i].value = std::u16string();
  }

  std::unique_ptr<TestFormStructure> form_structure =
      std::make_unique<TestFormStructure>(empty_form);
  form_structure->SetFieldTypes(heuristic_types, server_types);
  form_structure->identify_sections_for_testing();
  AddSeenFormStructure(std::move(form_structure));

  form_interactions_ukm_logger()->OnFormsParsed(client()->GetUkmSourceId());
}

void TestAutofillManager::AddSeenFormStructure(
    std::unique_ptr<FormStructure> form_structure) {
  const auto id = form_structure->global_id();
  (*mutable_form_structures())[id] = std::move(form_structure);
}

void TestAutofillManager::ClearFormStructures() {
  mutable_form_structures()->clear();
}

const std::string TestAutofillManager::GetSubmittedFormSignature() {
  return submitted_form_signature_;
}

void TestAutofillManager::SetAutofillProfileEnabled(
    bool autofill_profile_enabled) {
  autofill_profile_enabled_ = autofill_profile_enabled;
  if (!autofill_profile_enabled_)
    // Profile data is refreshed when this pref is changed.
    personal_data_->ClearProfiles();
}

void TestAutofillManager::SetAutofillCreditCardEnabled(
    bool autofill_credit_card_enabled) {
  autofill_credit_card_enabled_ = autofill_credit_card_enabled;
  if (!autofill_credit_card_enabled_)
    // Credit card data is refreshed when this pref is changed.
    personal_data_->ClearCreditCards();
}

void TestAutofillManager::SetExpectedSubmittedFieldTypes(
    const std::vector<ServerFieldTypeSet>& expected_types) {
  expected_submitted_field_types_ = expected_types;
}

void TestAutofillManager::SetExpectedObservedSubmission(bool expected) {
  expected_observed_submission_ = expected;
}

void TestAutofillManager::SetCallParentUploadFormData(bool value) {
  call_parent_upload_form_data_ = value;
}

}  // namespace autofill
