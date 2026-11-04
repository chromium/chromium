// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/autofill/core/browser/integrators/one_time_tokens/otp_target_field_identifier.h"

#include <algorithm>
#include <memory>

#include "components/autofill/core/browser/autofill_field.h"
#include "components/autofill/core/browser/form_structure.h"
#include "components/autofill/core/browser/foundations/autofill_driver.h"
#include "components/autofill/core/browser/foundations/browser_autofill_manager.h"
#include "components/autofill/core/browser/integrators/one_time_tokens/form_cache_query_util.h"
#include "components/autofill/core/browser/integrators/one_time_tokens/otp_field_detector.h"

namespace autofill {

OtpTargetFieldIdentifier::OtpTargetFieldIdentifier() = default;
OtpTargetFieldIdentifier::~OtpTargetFieldIdentifier() = default;

void OtpTargetFieldIdentifier::RegisterManager(
    BrowserAutofillManager& manager) {
  if (!std::ranges::contains(managers_, &manager)) {
    managers_.push_back(&manager);
  }
}

void OtpTargetFieldIdentifier::UnregisterManager(
    BrowserAutofillManager& manager) {
  OnFrameBecameInactive(manager.driver().GetFrameToken());
  std::erase(managers_, &manager);
}

void OtpTargetFieldIdentifier::OnFrameBecameInactive(LocalFrameToken frame) {
  if (focused_field_ && (focused_field_->frame == frame ||
                         focused_field_->field_id.frame_token == frame)) {
    focused_field_.reset();
  }
}

void OtpTargetFieldIdentifier::OnFocusChanged(
    LocalFrameToken frame,
    std::optional<FormGlobalId> form_id,
    std::optional<FieldGlobalId> field_id) {
  if (field_id.has_value()) {
    focused_field_ =
        FocusedField{.frame = frame, .form_id = form_id, .field_id = *field_id};
    return;
  }

  // Only clear tab-wide focus if the non-form focus event came from the frame
  // that reported focus or the child frame hosting the focused field, so a
  // broadcast `FocusOnNonFormField()` to other frames does not wipe out focus
  // in the active frame.
  if (focused_field_ && (focused_field_->frame == frame ||
                         focused_field_->field_id.frame_token == frame)) {
    focused_field_.reset();
  }
}

void OtpTargetFieldIdentifier::OnOtpFieldsDetected() {
  has_detected_otp_field_ = true;
}

void OtpTargetFieldIdentifier::OnOtpFieldsCleared() {
  has_detected_otp_field_ = false;
  typed_in_any_field_after_otp_detection_ = false;
}

void OtpTargetFieldIdentifier::OnTextFieldValueChanged() {
  if (has_detected_otp_field_) {
    typed_in_any_field_after_otp_detection_ = true;
  }
}

std::optional<OtpTarget> OtpTargetFieldIdentifier::GetTarget() const {
  if (focused_field_.has_value()) {
    auto it = std::ranges::find_if(
        managers_, [this](BrowserAutofillManager* manager) {
          return manager->driver().GetFrameToken() == focused_field_->frame;
        });
    if (it != managers_.end()) {
      if (auto [form_structure, autofill_field] = FindOtpField(
              **it, focused_field_->form_id, focused_field_->field_id);
          form_structure && autofill_field) {
        return BuildOtpTarget(**it, *form_structure, *autofill_field);
      }
    }
    // A non-OTP field has focus; do not fall back to another field.
    return std::nullopt;
  }

  std::vector<OtpTarget> eligible_targets = GetEligibleTargets();
  if (!eligible_targets.empty()) {
    return eligible_targets.front();
  }
  return std::nullopt;
}

std::vector<OtpTarget> OtpTargetFieldIdentifier::GetEligibleTargets() const {
  std::vector<OtpTarget> targets;
  for (BrowserAutofillManager* manager : managers_) {
    if (!IsActiveNonEmbeddedDriver(manager->driver())) {
      continue;
    }
    manager->ForEachCachedForm([&](const FormStructure& form) {
      if (!OtpFieldDetector::IsOtpForm(form)) {
        return;
      }
      for (const std::unique_ptr<AutofillField>& field : form.fields()) {
        if (OtpFieldDetector::IsOtpField(form, *field) &&
            IsFrameActive(field->host_frame())) {
          targets.push_back(BuildOtpTarget(*manager, form, *field));
        }
      }
    });
  }
  return targets;
}

bool OtpTargetFieldIdentifier::HasNonOtpFocusAnywhere() const {
  return focused_field_.has_value() && !GetTarget().has_value();
}

bool OtpTargetFieldIdentifier::AnyOtpFieldContainsTypedInput() const {
  return std::ranges::any_of(managers_, [](BrowserAutofillManager* manager) {
    return autofill::AnyOtpFieldContainsTypedInput(*manager);
  });
}

bool OtpTargetFieldIdentifier::TypedInAnyFieldAfterOtpDetection() const {
  return typed_in_any_field_after_otp_detection_;
}

bool OtpTargetFieldIdentifier::IsFieldFocused(FieldGlobalId field_id) const {
  return focused_field_ && focused_field_->field_id == field_id;
}

bool OtpTargetFieldIdentifier::IsFrameActive(LocalFrameToken frame) const {
  return std::ranges::any_of(
      managers_, [frame](BrowserAutofillManager* manager) {
        return manager->driver().GetFrameToken() == frame &&
               IsActiveNonEmbeddedDriver(manager->driver());
      });
}

}  // namespace autofill
