// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_AUTOFILL_CORE_BROWSER_INTEGRATORS_ONE_TIME_TOKENS_OTP_TARGET_FIELD_IDENTIFIER_H_
#define COMPONENTS_AUTOFILL_CORE_BROWSER_INTEGRATORS_ONE_TIME_TOKENS_OTP_TARGET_FIELD_IDENTIFIER_H_

#include <optional>
#include <vector>

#include "base/memory/raw_ptr.h"
#include "components/autofill/core/browser/integrators/one_time_tokens/otp_types.h"
#include "components/autofill/core/common/unique_ids.h"

namespace autofill {

class BrowserAutofillManager;

// Tab-scoped identifier owned by `OtpCoordinator` that tracks the focused frame
// and field across all frames in the tab, picks the target OTP field, reports
// whether a non-OTP field has focus anywhere in the tab, and tracks whether the
// user has typed into any field since an OTP field was detected.
class OtpTargetFieldIdentifier {
 public:
  OtpTargetFieldIdentifier();
  OtpTargetFieldIdentifier(const OtpTargetFieldIdentifier&) = delete;
  OtpTargetFieldIdentifier& operator=(const OtpTargetFieldIdentifier&) = delete;
  ~OtpTargetFieldIdentifier();

  // Registers/unregisters a frame's `BrowserAutofillManager`. Retains a
  // pointer to `manager`; the caller must ensure `manager` outlives the
  // registration and is unregistered via `UnregisterManager()` before
  // destruction.
  void RegisterManager(BrowserAutofillManager& manager);
  void UnregisterManager(BrowserAutofillManager& manager);

  // Clears focus state if `frame` held focus (e.g. when a frame transitions
  // away from active lifecycle state).
  void OnFrameBecameInactive(LocalFrameToken frame);

  // Updates cross-frame focus state when a field or non-form element is
  // focused.
  void OnFocusChanged(LocalFrameToken frame,
                      std::optional<FormGlobalId> form_id,
                      std::optional<FieldGlobalId> field_id);

  // Records that at least one OTP field is currently present in the tab.
  void OnOtpFieldsDetected();

  // Resets OTP detection / typing tracking when all OTP fields disappear or on
  // navigation.
  void OnOtpFieldsCleared();

  // Records that the user typed into a text field in the tab. If an OTP field
  // has been detected, subsequent proactive triggers are suppressed.
  void OnTextFieldValueChanged();

  // Returns the target OTP field across the tab:
  // - If a form field is focused anywhere in the tab, returns its `OtpTarget`
  //   if it is an eligible OTP field, or `std::nullopt` if it is a non-OTP
  //   field.
  // - If no form field is focused, returns the first eligible OTP field across
  //   registered active frames, or `std::nullopt` if none exists.
  std::optional<OtpTarget> GetTarget() const;

  // Returns all eligible OTP fields across registered active frames in
  // registration and form-cache order, regardless of which field currently has
  // focus.
  std::vector<OtpTarget> GetEligibleTargets() const;

  // Returns true if a form field currently has focus in any frame of the tab
  // and that field is not an eligible OTP field.
  bool HasNonOtpFocusAnywhere() const;

  // Returns true if any OTP field across registered active frames contains
  // user-typed input.
  bool AnyOtpFieldContainsTypedInput() const;

  // Returns true if the user has typed into any field in the tab after an OTP
  // field was detected.
  bool TypedInAnyFieldAfterOtpDetection() const;

  // Returns true if `field_id` is currently focused.
  bool IsFieldFocused(FieldGlobalId field_id) const;

 private:
  struct FocusedField {
    LocalFrameToken frame;
    std::optional<FormGlobalId> form_id;
    FieldGlobalId field_id;
  };

  bool IsFrameActive(LocalFrameToken frame) const;

  // Active frame managers in the tab (registration order).
  std::vector<raw_ptr<BrowserAutofillManager>> managers_;

  // Cross-frame focus state.
  std::optional<FocusedField> focused_field_;

  bool has_detected_otp_field_ = false;
  bool typed_in_any_field_after_otp_detection_ = false;
};

}  // namespace autofill

#endif  // COMPONENTS_AUTOFILL_CORE_BROWSER_INTEGRATORS_ONE_TIME_TOKENS_OTP_TARGET_FIELD_IDENTIFIER_H_
