// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/autofill/core/browser/integrators/one_time_tokens/form_cache_query_util.h"

#include <algorithm>
#include <optional>

#include "components/autofill/core/browser/autofill_field.h"
#include "components/autofill/core/browser/form_structure.h"
#include "components/autofill/core/browser/foundations/autofill_driver.h"
#include "components/autofill/core/browser/foundations/autofill_manager.h"
#include "components/autofill/core/browser/integrators/one_time_tokens/otp_field_detector.h"
#include "components/autofill/core/browser/integrators/one_time_tokens/otp_types.h"

namespace autofill {

AutofillManager::FormAndField FindOtpField(const AutofillManager& manager,
                                           std::optional<FormGlobalId> form_id,
                                           FieldGlobalId field_id) {
  const FormStructure* form = nullptr;
  const AutofillField* field = nullptr;
  if (form_id) {
    AutofillManager::FormAndField form_and_field =
        manager.FindFormAndField(*form_id, field_id);
    form = form_and_field.form_structure;
    field = form_and_field.autofill_field;
  }
  if (!field) {
    form = manager.FindCachedFormById(field_id);
    if (form) {
      field = form->GetFieldById(field_id);
    }
  }
  if (form && field && OtpFieldDetector::IsOtpForm(*form) &&
      OtpFieldDetector::IsOtpField(*form, *field)) {
    return {form, field};
  }
  return {nullptr, nullptr};
}

AutofillManager::FormAndField FirstEligibleOtpField(
    const AutofillManager& manager) {
  const FormStructure* target_form = nullptr;
  const AutofillField* target_field = nullptr;
  manager.ForEachCachedForm([&target_form,
                             &target_field](const FormStructure& form) {
    if (target_field || !OtpFieldDetector::IsOtpForm(form)) {
      return;
    }
    auto it = std::ranges::find_if(form.fields(), [&form](const auto& field) {
      return OtpFieldDetector::IsOtpField(form, *field);
    });
    if (it != form.fields().end()) {
      target_form = &form;
      target_field = it->get();
    }
  });
  return {target_form, target_field};
}

bool AnyOtpFieldContainsTypedInput(const AutofillManager& manager) {
  bool has_typed_input = false;
  manager.ForEachCachedForm([&has_typed_input](const FormStructure& form) {
    if (has_typed_input) {
      return;
    }
    has_typed_input =
        std::ranges::any_of(form.fields(), [&form](const auto& field) {
          return OtpFieldDetector::IsOtpField(form, *field) &&
                 field->all_modifiers().contains(FieldModifier::kUser);
        });
  });
  return has_typed_input;
}

bool HasOtpForm(const AutofillManager& manager) {
  bool has_otp_form = false;
  manager.ForEachCachedForm([&has_otp_form](const FormStructure& form) {
    if (has_otp_form) {
      return;
    }
    if (OtpFieldDetector::IsOtpForm(form)) {
      has_otp_form = true;
    }
  });
  return has_otp_form;
}

OtpTarget BuildOtpTarget(AutofillManager& manager,
                         const FormStructure& form,
                         const AutofillField& field) {
  return OtpTarget(field.origin(), field.host_frame(), form.global_id(),
                   field.global_id(), manager.driver().GetFrameToken());
}

}  // namespace autofill
