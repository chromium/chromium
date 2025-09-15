// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/autofill/content/browser/integrators/one_time_tokens/content_otp_field_detector.h"

#include "components/autofill/content/browser/content_autofill_client.h"

namespace autofill {

namespace {

// Returns if `form` in `manager` contains at least one `ONE_TIME_CODE` field.
[[nodiscard]] bool IsOtpForm(AutofillManager& manager, FormGlobalId form) {
  const FormStructure* form_structure = manager.FindCachedFormById(form);
  if (!form_structure) {
    return false;
  }
  return std::ranges::any_of(
      form_structure->fields(), [](const std::unique_ptr<AutofillField>& f) {
        return f->Type().GetTypes().contains(ONE_TIME_CODE) &&
               f->is_focusable();
      });
}

}  // namespace

ContentOtpFieldDetector::ContentOtpFieldDetector(
    ContentAutofillClient* client) {
  autofill_manager_observation_.Observe(client);
}

ContentOtpFieldDetector::~ContentOtpFieldDetector() = default;

void ContentOtpFieldDetector::OnFieldTypesDetermined(AutofillManager& manager,
                                                     FormGlobalId form,
                                                     FieldTypeSource source) {
  if (IsOtpForm(manager, form)) {
    AddFormAndNotifyIfNecessary(form);
  } else {
    RemoveFormAndNotifyIfNecessary(form);
  }
}

void ContentOtpFieldDetector::OnAfterFormsSeen(
    AutofillManager& manager,
    base::span<const FormGlobalId> updated_forms,
    base::span<const FormGlobalId> removed_forms) {
  for (const FormGlobalId form : removed_forms) {
    RemoveFormAndNotifyIfNecessary(form);
  }
}

void ContentOtpFieldDetector::OnAutofillManagerStateChanged(
    AutofillManager& manager,
    AutofillDriver::LifecycleState previous,
    AutofillDriver::LifecycleState current) {
  if (current != AutofillDriver::LifecycleState::kActive) {
    for (const auto& [form_id, form_structure] : manager.form_structures()) {
      RemoveFormAndNotifyIfNecessary(form_id);
    }
  } else {
    for (const auto& [form_id, form_structure] : manager.form_structures()) {
      if (IsOtpForm(manager, form_id)) {
        AddFormAndNotifyIfNecessary(form_id);
      }
    }
  }
}

void ContentOtpFieldDetector::OnAfterFormSubmitted(AutofillManager& manager,
                                                   const FormData& form) {
  RemoveFormAndNotifyIfNecessary(form.global_id());
}

}  // namespace autofill
