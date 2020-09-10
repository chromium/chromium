// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/renderer/autofill/fake_mojo_password_manager_driver.h"

#include <utility>

#include "testing/gtest/include/gtest/gtest.h"

FakeMojoPasswordManagerDriver::FakeMojoPasswordManagerDriver() = default;

FakeMojoPasswordManagerDriver::~FakeMojoPasswordManagerDriver() = default;

void FakeMojoPasswordManagerDriver::BindReceiver(
    mojo::PendingAssociatedReceiver<autofill::mojom::PasswordManagerDriver>
        receiver) {
  receiver_.Bind(std::move(receiver));
}

void FakeMojoPasswordManagerDriver::Flush() {
  receiver_.FlushForTesting();
}

// mojom::PasswordManagerDriver:
void FakeMojoPasswordManagerDriver::PasswordFormsParsed(
    const std::vector<autofill::FormData>& forms_data) {
  called_password_forms_parsed_ = true;
  form_data_parsed_ = forms_data;
}

void FakeMojoPasswordManagerDriver::PasswordFormsRendered(
    const std::vector<autofill::FormData>& visible_forms_data,
    bool did_stop_loading) {
  called_password_forms_rendered_ = true;
  form_data_rendered_ = visible_forms_data;
}

void FakeMojoPasswordManagerDriver::PasswordFormSubmitted(
    const autofill::FormData& form_data) {
  called_password_form_submitted_ = true;
  form_data_submitted_ = form_data;
}

void FakeMojoPasswordManagerDriver::SameDocumentNavigation(
    autofill::mojom::SubmissionIndicatorEvent submission_indication_event) {
  called_same_document_navigation_ = true;
  form_data_maybe_submitted_->submission_event = submission_indication_event;
}

void FakeMojoPasswordManagerDriver::RecordSavePasswordProgress(
    const std::string& log) {
  called_record_save_progress_ = true;
}

void FakeMojoPasswordManagerDriver::UserModifiedPasswordField() {
  called_user_modified_password_field_ = true;
}

void FakeMojoPasswordManagerDriver::UserModifiedNonPasswordField(
    autofill::FieldRendererId renderer_id,
    const base::string16& value) {}

void FakeMojoPasswordManagerDriver::CheckSafeBrowsingReputation(
    const GURL& form_action,
    const GURL& frame_url) {
  called_check_safe_browsing_reputation_cnt_++;
}

void FakeMojoPasswordManagerDriver::InformAboutUserInput(
    const autofill::FormData& form_data) {
  called_inform_about_user_input_count_++;
  form_data_maybe_submitted_ = form_data;
}

void FakeMojoPasswordManagerDriver::FocusedInputChanged(
    autofill::mojom::FocusedFieldType focused_field_type) {
  last_focused_field_type_ = focused_field_type;
}
