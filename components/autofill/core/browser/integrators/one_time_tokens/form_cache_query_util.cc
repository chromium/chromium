// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/autofill/core/browser/integrators/one_time_tokens/form_cache_query_util.h"

#include <algorithm>
#include <optional>

#include "components/autofill/core/browser/autofill_field.h"
#include "components/autofill/core/browser/form_structure.h"
#include "components/autofill/core/browser/foundations/autofill_client.h"
#include "components/autofill/core/browser/foundations/autofill_driver.h"
#include "components/autofill/core/browser/foundations/autofill_driver_factory.h"
#include "components/autofill/core/browser/foundations/autofill_manager.h"
#include "components/autofill/core/browser/integrators/one_time_tokens/otp_field_detector.h"
#include "components/autofill/core/browser/integrators/one_time_tokens/otp_types.h"

namespace autofill {
namespace {

bool IsHostFrameActive(AutofillManager& manager, LocalFrameToken host_frame) {
  if (!IsActiveNonEmbeddedDriver(manager.driver())) {
    return false;
  }
  if (manager.driver().GetFrameToken() == host_frame) {
    return true;
  }
  return std::ranges::any_of(
      manager.client().GetAutofillDriverFactory().GetExistingDrivers(),
      [host_frame](const AutofillDriver* driver) {
        return driver && driver->GetFrameToken() == host_frame &&
               IsActiveNonEmbeddedDriver(*driver);
      });
}

}  // namespace

bool IsActiveNonEmbeddedDriver(const AutofillDriver& driver) {
  return !driver.IsEmbedded() && driver.IsActive() &&
         driver.GetLifecycleState() !=
             AutofillDriver::LifecycleState::kPendingDeletion;
}

AutofillManager::FormAndField FindOtpField(AutofillManager& manager,
                                           std::optional<FormGlobalId> form_id,
                                           FieldGlobalId field_id) {
  if (!IsActiveNonEmbeddedDriver(manager.driver())) {
    return {nullptr, nullptr};
  }
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
      OtpFieldDetector::IsOtpField(*form, *field) &&
      IsHostFrameActive(manager, field->host_frame())) {
    return {form, field};
  }
  return {nullptr, nullptr};
}

AutofillManager::FormAndField FirstEligibleOtpField(AutofillManager& manager) {
  if (!IsActiveNonEmbeddedDriver(manager.driver())) {
    return {nullptr, nullptr};
  }
  const FormStructure* target_form = nullptr;
  const AutofillField* target_field = nullptr;
  manager.ForEachCachedForm(
      [&manager, &target_form, &target_field](const FormStructure& form) {
        if (target_field || !OtpFieldDetector::IsOtpForm(form)) {
          return;
        }
        auto it = std::ranges::find_if(
            form.fields(), [&manager, &form](const auto& field) {
              return OtpFieldDetector::IsOtpField(form, *field) &&
                     IsHostFrameActive(manager, field->host_frame());
            });
        if (it != form.fields().end()) {
          target_form = &form;
          target_field = it->get();
        }
      });
  return {target_form, target_field};
}

bool AnyOtpFieldContainsTypedInput(AutofillManager& manager) {
  if (!IsActiveNonEmbeddedDriver(manager.driver())) {
    return false;
  }
  bool has_typed_input = false;
  manager.ForEachCachedForm(
      [&manager, &has_typed_input](const FormStructure& form) {
        if (has_typed_input || !OtpFieldDetector::IsOtpForm(form)) {
          return;
        }
        has_typed_input = std::ranges::any_of(
            form.fields(), [&manager, &form](const auto& field) {
              return field->all_modifiers().contains(FieldModifier::kUser) &&
                     OtpFieldDetector::IsOtpField(form, *field) &&
                     IsHostFrameActive(manager, field->host_frame());
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
