// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_RENDERER_AUTOFILL_FAKE_MOJO_PASSWORD_MANAGER_DRIVER_H_
#define CHROME_RENDERER_AUTOFILL_FAKE_MOJO_PASSWORD_MANAGER_DRIVER_H_

#include <string>
#include <vector>

#include "base/optional.h"
#include "base/strings/string16.h"
#include "components/autofill/content/common/mojom/autofill_driver.mojom.h"
#include "components/autofill/core/common/password_form.h"
#include "mojo/public/cpp/bindings/associated_receiver.h"
#include "mojo/public/cpp/bindings/pending_associated_receiver.h"
#include "testing/gmock/include/gmock/gmock.h"

class FakeMojoPasswordManagerDriver
    : public autofill::mojom::PasswordManagerDriver {
 public:
  FakeMojoPasswordManagerDriver();

  ~FakeMojoPasswordManagerDriver() override;

  void BindReceiver(
      mojo::PendingAssociatedReceiver<autofill::mojom::PasswordManagerDriver>
          receiver);

  // Flushes all pending messages from the associated binding.
  void Flush();

  // mojom::PasswordManagerDriver:
  // TODO(crbug.com/948062): Migrate the other methods to GMock as well.
  MOCK_METHOD0(ShowTouchToFill, void());

  MOCK_METHOD4(ShowPasswordSuggestions,
               void(base::i18n::TextDirection,
                    const base::string16&,
                    int,
                    const gfx::RectF&));

  bool called_show_not_secure_warning() const {
    return called_show_not_secure_warning_;
  }

  bool called_password_form_submitted() const {
    return called_password_form_submitted_ && password_form_submitted_ &&
           !password_form_submitted_->only_for_fallback;
  }

  bool called_password_form_submitted_only_for_fallback() const {
    return called_password_form_submitted_ && password_form_submitted_ &&
           password_form_submitted_->only_for_fallback;
  }

  const base::Optional<autofill::PasswordForm>& password_form_submitted()
      const {
    return password_form_submitted_;
  }

  bool called_same_document_navigation() const {
    return called_same_document_navigation_;
  }

  const base::Optional<autofill::PasswordForm>& password_form_maybe_submitted()
      const {
    return password_form_maybe_submitted_;
  }

  bool called_password_forms_parsed() const {
    return called_password_forms_parsed_;
  }

  const base::Optional<std::vector<autofill::PasswordForm>>&
  password_forms_parsed() const {
    return password_forms_parsed_;
  }

  bool called_password_forms_rendered() const {
    return called_password_forms_rendered_;
  }

  const base::Optional<std::vector<autofill::PasswordForm>>&
  password_forms_rendered() const {
    return password_forms_rendered_;
  }

  void reset_password_forms_calls() {
    called_password_forms_parsed_ = false;
    password_forms_parsed_ = base::nullopt;
    called_password_forms_rendered_ = false;
    password_forms_rendered_ = base::nullopt;
  }

  bool called_record_save_progress() const {
    return called_record_save_progress_;
  }

  bool called_user_modified_password_field() const {
    return called_user_modified_password_field_;
  }

  bool called_save_generation_field() const {
    return called_save_generation_field_;
  }

  const base::Optional<base::string16>& save_generation_field() const {
    return save_generation_field_;
  }

  void reset_save_generation_field() {
    called_save_generation_field_ = false;
    save_generation_field_ = base::nullopt;
  }

  int called_check_safe_browsing_reputation_cnt() const {
    return called_check_safe_browsing_reputation_cnt_;
  }

  int called_show_manual_fallback_for_saving_count() const {
    return called_show_manual_fallback_for_saving_count_;
  }

  autofill::mojom::FocusedFieldType last_focused_field_type() const {
    return last_focused_field_type_;
  }

 private:
  // mojom::PasswordManagerDriver:
  void PasswordFormsParsed(
      const std::vector<autofill::PasswordForm>& forms) override;

  void PasswordFormsRendered(
      const std::vector<autofill::PasswordForm>& visible_forms,
      bool did_stop_loading) override;

  void PasswordFormSubmitted(
      const autofill::PasswordForm& password_form) override;

  void SameDocumentNavigation(autofill::mojom::SubmissionIndicatorEvent
                                  submission_indication_event) override;

  void RecordSavePasswordProgress(const std::string& log) override;

  void UserModifiedPasswordField() override;

  void UserModifiedNonPasswordField(uint32_t renderer_id,
                                    const base::string16& value) override;

  void CheckSafeBrowsingReputation(const GURL& form_action,
                                   const GURL& frame_url) override;

  void ShowManualFallbackForSaving(
      const autofill::PasswordForm& password_form) override;
  void HideManualFallbackForSaving() override;
  void FocusedInputChanged(
      autofill::mojom::FocusedFieldType focused_field_type) override;
  void LogFirstFillingResult(uint32_t form_renderer_id,
                             int32_t result) override {}

  // Records whether ShowNotSecureWarning() gets called.
  bool called_show_not_secure_warning_ = false;
  // Records whether PasswordFormSubmitted() gets called.
  bool called_password_form_submitted_ = false;
  // Records data received via PasswordFormSubmitted() call.
  base::Optional<autofill::PasswordForm> password_form_submitted_;
  // Records data received via ShowManualFallbackForSaving() call.
  base::Optional<autofill::PasswordForm> password_form_maybe_submitted_;
  // Records whether SameDocumentNavigation() gets called.
  bool called_same_document_navigation_ = false;
  // Records whether PasswordFormsParsed() gets called.
  bool called_password_forms_parsed_ = false;
  // Records if the list received via PasswordFormsParsed() call was empty.
  base::Optional<std::vector<autofill::PasswordForm>> password_forms_parsed_;
  // Records whether PasswordFormsRendered() gets called.
  bool called_password_forms_rendered_ = false;
  // Records data received via PasswordFormsRendered() call.
  base::Optional<std::vector<autofill::PasswordForm>> password_forms_rendered_;
  // Records whether RecordSavePasswordProgress() gets called.
  bool called_record_save_progress_ = false;
  // Records whether UserModifiedPasswordField() gets called.
  bool called_user_modified_password_field_ = false;
  // Records whether SaveGenerationFieldDetectedByClassifier() gets called.
  bool called_save_generation_field_ = false;
  // Records data received via SaveGenerationFieldDetectedByClassifier() call.
  base::Optional<base::string16> save_generation_field_;

  // Records number of times CheckSafeBrowsingReputation() gets called.
  int called_check_safe_browsing_reputation_cnt_ = 0;

  // Records the number of request to show manual fallback for password saving.
  // If it is zero, the fallback is not available.
  int called_show_manual_fallback_for_saving_count_ = 0;

  // Records the last focused field type that FocusedInputChanged() was called
  // with.
  autofill::mojom::FocusedFieldType last_focused_field_type_ =
      autofill::mojom::FocusedFieldType::kUnknown;

  mojo::AssociatedReceiver<autofill::mojom::PasswordManagerDriver> receiver_{
      this};
};

#endif  // CHROME_RENDERER_AUTOFILL_FAKE_MOJO_PASSWORD_MANAGER_DRIVER_H_
