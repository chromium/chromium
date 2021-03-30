// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_RENDERER_AUTOFILL_FAKE_MOJO_PASSWORD_MANAGER_DRIVER_H_
#define CHROME_RENDERER_AUTOFILL_FAKE_MOJO_PASSWORD_MANAGER_DRIVER_H_

#include <string>
#include <vector>

#include "base/optional.h"
#include "components/autofill/content/common/mojom/autofill_driver.mojom.h"
#include "components/autofill/core/common/unique_ids.h"
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
  MOCK_METHOD1(PasswordFormCleared, void(const autofill::FormData&));

  MOCK_METHOD0(ShowTouchToFill, void());

  MOCK_METHOD4(ShowPasswordSuggestions,
               void(base::i18n::TextDirection,
                    const std::u16string&,
                    int,
                    const gfx::RectF&));

  bool called_show_not_secure_warning() const {
    return called_show_not_secure_warning_;
  }

  bool called_password_form_submitted() const {
    return called_password_form_submitted_ && form_data_submitted_;
  }

  const base::Optional<autofill::FormData>& form_data_submitted() const {
    return form_data_submitted_;
  }

  bool called_dynamic_form_submission() const {
    return called_dynamic_form_submission_;
  }

  const base::Optional<autofill::FormData>& form_data_maybe_submitted() const {
    return form_data_maybe_submitted_;
  }

  bool called_password_forms_parsed() const {
    return called_password_forms_parsed_;
  }

  const base::Optional<std::vector<autofill::FormData>>& form_data_parsed()
      const {
    return form_data_parsed_;
  }

  bool called_password_forms_rendered() const {
    return called_password_forms_rendered_;
  }

  const base::Optional<std::vector<autofill::FormData>>& form_data_rendered()
      const {
    return form_data_rendered_;
  }

  void reset_password_forms_calls() {
    called_password_forms_parsed_ = false;
    form_data_parsed_ = base::nullopt;
    called_password_forms_rendered_ = false;
    form_data_rendered_ = base::nullopt;
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

  const base::Optional<std::u16string>& save_generation_field() const {
    return save_generation_field_;
  }

  void reset_save_generation_field() {
    called_save_generation_field_ = false;
    save_generation_field_ = base::nullopt;
  }

  int called_check_safe_browsing_reputation_cnt() const {
    return called_check_safe_browsing_reputation_cnt_;
  }

  int called_inform_about_user_input_count() const {
    return called_inform_about_user_input_count_;
  }

  autofill::FieldRendererId last_focused_field_id() const {
    return last_focused_field_id_;
  }

  autofill::mojom::FocusedFieldType last_focused_field_type() const {
    return last_focused_field_type_;
  }

 private:
  // mojom::PasswordManagerDriver:
  void PasswordFormsParsed(
      const std::vector<autofill::FormData>& forms_data) override;

  void PasswordFormsRendered(
      const std::vector<autofill::FormData>& visible_forms_data,
      bool did_stop_loading) override;

  void PasswordFormSubmitted(const autofill::FormData& form_data) override;

  void DynamicFormSubmission(autofill::mojom::SubmissionIndicatorEvent
                                 submission_indication_event) override;

  void RecordSavePasswordProgress(const std::string& log) override;

  void UserModifiedPasswordField() override;

  void UserModifiedNonPasswordField(autofill::FieldRendererId renderer_id,
                                    const std::u16string& value) override;

  void CheckSafeBrowsingReputation(const GURL& form_action,
                                   const GURL& frame_url) override;

  void InformAboutUserInput(const autofill::FormData& form_data) override;

  void FocusedInputChanged(
      autofill::FieldRendererId focused_field_id,
      autofill::mojom::FocusedFieldType focused_field_type) override;
  void LogFirstFillingResult(autofill::FormRendererId form_renderer_id,
                             int32_t result) override {}

  // Records whether ShowNotSecureWarning() gets called.
  bool called_show_not_secure_warning_ = false;
  // Records whether PasswordFormSubmitted() gets called.
  bool called_password_form_submitted_ = false;
  // Records data received via PasswordFormSubmitted() call.
  base::Optional<autofill::FormData> form_data_submitted_;
  // Records data received via ShowManualFallbackForSaving() call.
  base::Optional<autofill::FormData> form_data_maybe_submitted_;
  // Records whether DynamicFormSubmission() gets called.
  bool called_dynamic_form_submission_ = false;
  // Records whether PasswordFormsParsed() gets called.
  bool called_password_forms_parsed_ = false;
  // Records if the list received via PasswordFormsParsed() call was empty.
  base::Optional<std::vector<autofill::FormData>> form_data_parsed_;
  // Records whether PasswordFormsRendered() gets called.
  bool called_password_forms_rendered_ = false;
  // Records data received via PasswordFormsRendered() call.
  base::Optional<std::vector<autofill::FormData>> form_data_rendered_;
  // Records whether RecordSavePasswordProgress() gets called.
  bool called_record_save_progress_ = false;
  // Records whether UserModifiedPasswordField() gets called.
  bool called_user_modified_password_field_ = false;
  // Records whether SaveGenerationFieldDetectedByClassifier() gets called.
  bool called_save_generation_field_ = false;
  // Records data received via SaveGenerationFieldDetectedByClassifier() call.
  base::Optional<std::u16string> save_generation_field_;

  // Records number of times CheckSafeBrowsingReputation() gets called.
  int called_check_safe_browsing_reputation_cnt_ = 0;

  // Records the number of request to inform about user input.
  int called_inform_about_user_input_count_ = 0;

  // Records the last focused field ID and type that FocusedInputChanged() was
  // called with.
  autofill::FieldRendererId last_focused_field_id_;
  autofill::mojom::FocusedFieldType last_focused_field_type_ =
      autofill::mojom::FocusedFieldType::kUnknown;

  mojo::AssociatedReceiver<autofill::mojom::PasswordManagerDriver> receiver_{
      this};
};

#endif  // CHROME_RENDERER_AUTOFILL_FAKE_MOJO_PASSWORD_MANAGER_DRIVER_H_
