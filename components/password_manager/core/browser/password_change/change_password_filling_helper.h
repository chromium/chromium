// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_PASSWORD_MANAGER_CORE_BROWSER_PASSWORD_CHANGE_CHANGE_PASSWORD_FILLING_HELPER_H_
#define COMPONENTS_PASSWORD_MANAGER_CORE_BROWSER_PASSWORD_CHANGE_CHANGE_PASSWORD_FILLING_HELPER_H_

#include <optional>
#include <string>

#include "base/functional/callback_forward.h"
#include "base/memory/weak_ptr.h"
#include "base/types/expected.h"
#include "components/actor/public/mojom/actor_types.mojom-shared.h"
#include "components/autofill/core/common/unique_ids.h"

namespace password_manager {

class PasswordManagerDriver;

// Outcome of validating or executing an actor change-password action.
struct ChangePasswordActionResult {
  static ChangePasswordActionResult Ok();
  static ChangePasswordActionResult Ok(bool requires_page_stabilization,
                                       std::string message);
  static ChangePasswordActionResult Error(actor::mojom::ActionResultCode code,
                                          std::string message);

  bool IsOk() const { return code == actor::mojom::ActionResultCode::kOk; }

  actor::mojom::ActionResultCode code = actor::mojom::ActionResultCode::kOk;
  bool requires_page_stabilization = false;
  std::string message;
};

// Single-use stateful helper encapsulating cross-platform field validation and
// form filling logic for the actor change-password tool.
class ChangePasswordFillingHelper {
 public:
  ChangePasswordFillingHelper();
  ~ChangePasswordFillingHelper();

  ChangePasswordFillingHelper(const ChangePasswordFillingHelper&) = delete;
  ChangePasswordFillingHelper& operator=(const ChangePasswordFillingHelper&) =
      delete;

  // Validates that at least one target password field was supplied. Intended to
  // be called during early argument validation before DOM/frame resolution.
  template <typename T>
  static ChangePasswordActionResult ValidateProvidedFields(
      const std::optional<T>& current_password_field,
      const std::optional<T>& new_password_field,
      const std::optional<T>& confirm_new_password_field) {
    return ValidateProvidedFields(current_password_field.has_value(),
                                  new_password_field.has_value(),
                                  confirm_new_password_field.has_value());
  }
  static ChangePasswordActionResult ValidateProvidedFields(
      bool has_current_password,
      bool has_new_password,
      bool has_confirm_new_password);

  // Validates that at least one resolved `FieldGlobalId` is valid, all valid
  // IDs belong to the same frame, and all valid IDs are distinct. Unset targets
  // should be passed as default-constructed `FieldGlobalId{}`. On success,
  // stores the `FieldRendererId`s and returns the shared `LocalFrameToken`.
  base::expected<autofill::LocalFrameToken, ChangePasswordActionResult>
  ValidateAndSetFieldIds(
      const autofill::FieldGlobalId& current_password_field_id,
      const autofill::FieldGlobalId& new_password_field_id,
      const autofill::FieldGlobalId& confirm_new_password_field_id);

  // Validates that every stored target field is managed by `driver`'s
  // `PasswordFormCache` as a password input element. On success, retains
  // `driver.AsWeakPtr()`. Requires `ValidateAndSetFieldIds()` to have
  // succeeded.
  ChangePasswordActionResult ValidateDriverAndTargetFields(
      PasswordManagerDriver& driver);

  // Fills the stored target password fields via the validated driver, evaluates
  // the returned `FormData`, and asynchronously invokes `callback` with the
  // resulting status and message. Requires `ValidateDriverAndTargetFields()` to
  // have succeeded.
  void FillChangePasswordForm(
      base::OnceCallback<void(ChangePasswordActionResult)> callback);

 private:
  base::WeakPtr<PasswordManagerDriver> driver_;
  autofill::FieldRendererId current_password_field_id_;
  autofill::FieldRendererId new_password_field_id_;
  autofill::FieldRendererId confirm_new_password_field_id_;
};

}  // namespace password_manager

#endif  // COMPONENTS_PASSWORD_MANAGER_CORE_BROWSER_PASSWORD_CHANGE_CHANGE_PASSWORD_FILLING_HELPER_H_
