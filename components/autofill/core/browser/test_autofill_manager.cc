// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/autofill/core/browser/test_autofill_manager.h"

#include "base/strings/stringprintf.h"
#include "base/strings/utf_string_conversions.h"
#include "components/autofill/core/browser/field_types.h"
#include "components/autofill/core/browser/payments/payments_client.h"
#include "components/autofill/core/browser/payments/test_payments_client.h"
#include "components/autofill/core/browser/test_form_data_importer.h"
#include "components/autofill/core/browser/test_form_structure.h"
#include "components/autofill/core/browser/test_personal_data_manager.h"
#include "services/network/public/cpp/shared_url_loader_factory.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace autofill {

TestAutofillManager::TestAutofillManager(AutofillDriver* driver,
                                         AutofillClient* client,
                                         TestPersonalDataManager* personal_data)
    : AutofillManager(driver, client, personal_data),
      personal_data_(personal_data),
      url_loader_factory_(driver->GetURLLoaderFactory()),
      client_(client) {
  set_payments_client(new payments::PaymentsClient(
      url_loader_factory_, client->GetPrefs(), client->GetIdentityManager(),
      personal_data));
}

TestAutofillManager::TestAutofillManager(
    AutofillDriver* driver,
    AutofillClient* client,
    TestPersonalDataManager* personal_data,
    std::unique_ptr<CreditCardSaveManager> credit_card_save_manager,
    payments::TestPaymentsClient* payments_client,
    std::unique_ptr<LocalCardMigrationManager> local_card_migration_manager)
    : AutofillManager(driver, client, personal_data),
      personal_data_(personal_data),
      test_form_data_importer_(
          new TestFormDataImporter(client,
                                   payments_client,
                                   std::move(credit_card_save_manager),
                                   personal_data,
                                   "en-US",
                                   std::move(local_card_migration_manager))),
      client_(client) {
  set_payments_client(payments_client);
  set_form_data_importer(test_form_data_importer_);
}

TestAutofillManager::~TestAutofillManager() {}

bool TestAutofillManager::IsAutofillEnabled() const {
  return autofill_enabled_;
}

bool TestAutofillManager::IsProfileAutofillEnabled() const {
  return profile_enabled_;
}

bool TestAutofillManager::IsCreditCardAutofillEnabled() const {
  return credit_card_enabled_;
}

void TestAutofillManager::UploadFormData(const FormStructure& submitted_form,
                                         bool observed_submission) {
  submitted_form_signature_ = submitted_form.FormSignatureAsStr();

  if (call_parent_upload_form_data_)
    AutofillManager::UploadFormData(submitted_form, observed_submission);
}

bool TestAutofillManager::MaybeStartVoteUploadProcess(
    std::unique_ptr<FormStructure> form_structure,
    const base::TimeTicks& timestamp,
    bool observed_submission) {
  run_loop_ = std::make_unique<base::RunLoop>();
  if (AutofillManager::MaybeStartVoteUploadProcess(
          std::move(form_structure), timestamp, observed_submission)) {
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
    empty_form.fields[i].value = base::string16();
  }

  std::unique_ptr<TestFormStructure> form_structure =
      std::make_unique<TestFormStructure>(empty_form);
  form_structure->SetFieldTypes(heuristic_types, server_types);
  AddSeenFormStructure(std::move(form_structure));

  form_interactions_ukm_logger()->OnFormsParsed(form.main_frame_origin.GetURL(),
                                                client_->GetUkmSourceId());
}

void TestAutofillManager::AddSeenFormStructure(
    std::unique_ptr<FormStructure> form_structure) {
  const auto signature = form_structure->form_signature();
  (*mutable_form_structures())[signature] = std::move(form_structure);
}

void TestAutofillManager::ClearFormStructures() {
  mutable_form_structures()->clear();
}

const std::string TestAutofillManager::GetSubmittedFormSignature() {
  return submitted_form_signature_;
}

void TestAutofillManager::SetAutofillEnabled(bool autofill_enabled) {
  autofill_enabled_ = autofill_enabled;
}

void TestAutofillManager::SetProfileEnabled(bool profile_enabled) {
  profile_enabled_ = profile_enabled;
  if (!profile_enabled_)
    // Profile data is refreshed when this pref is changed.
    personal_data_->ClearProfiles();
}

void TestAutofillManager::SetCreditCardEnabled(bool credit_card_enabled) {
  credit_card_enabled_ = credit_card_enabled;
  if (!credit_card_enabled_)
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
