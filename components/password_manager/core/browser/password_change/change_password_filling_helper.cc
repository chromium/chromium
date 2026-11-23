// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/password_manager/core/browser/password_change/change_password_filling_helper.h"

#include <algorithm>
#include <memory>
#include <optional>
#include <string>
#include <utility>
#include <vector>

#include "base/check.h"
#include "base/containers/span.h"
#include "base/functional/bind.h"
#include "base/functional/callback.h"
#include "base/location.h"
#include "base/strings/string_util.h"
#include "base/task/bind_post_task.h"
#include "base/task/sequenced_task_runner.h"
#include "base/types/expected.h"
#include "components/autofill/core/common/form_data.h"
#include "components/autofill/core/common/form_field_data.h"
#include "components/password_manager/core/browser/password_form_cache.h"
#include "components/password_manager/core/browser/password_form_manager.h"
#include "components/password_manager/core/browser/password_manager_driver.h"
#include "components/password_manager/core/browser/password_manager_interface.h"
#include "components/password_manager/core/browser/password_string.h"
#include "mojo/public/cpp/bindings/callback_helpers.h"
#include "third_party/abseil-cpp/absl/strings/str_format.h"

namespace password_manager {

namespace {

bool WasFieldFilled(const autofill::FormData& form_data,
                    autofill::FieldRendererId renderer_id) {
  CHECK(renderer_id);
  auto it = std::ranges::find(form_data.fields(), renderer_id,
                              &autofill::FormFieldData::renderer_id);
  return it != form_data.fields().end() &&
         it->is_autofilled_according_to_renderer();
}

// Returns the `PasswordFormManager` in `form_managers` from `driver`'s frame
// that manages `renderer_id` as a password input element, or `nullptr` if none
// matches. The returned pointer is owned by `PasswordFormCache` and must not be
// retained across asynchronous boundaries.
PasswordFormManager* FindPasswordFormManagerForField(
    PasswordManagerDriver& driver,
    base::span<const std::unique_ptr<PasswordFormManager>> form_managers,
    autofill::FieldRendererId renderer_id) {
  if (!renderer_id) {
    return nullptr;
  }
  for (const std::unique_ptr<PasswordFormManager>& form_manager :
       form_managers) {
    if (!form_manager->DoesManage(renderer_id, &driver)) {
      continue;
    }
    const autofill::FormData* observed_form = form_manager->observed_form();
    auto it = std::ranges::find(observed_form->fields(), renderer_id,
                                &autofill::FormFieldData::renderer_id);
    if (it != observed_form->fields().end() && it->IsPasswordInputElement()) {
      return form_manager.get();
    }
  }
  return nullptr;
}

// TODO(crbug.com/580856538): `PasswordManagerDriver::FillChangePasswordForm()`
// currently passes a single `form_data` here. If a page has password inputs
// that do not share the same <form> element (or one is inside a <form> and
// another is unowned), `form_data` only covers one form and fields filled in
// other forms will be reported as unfilled.
ChangePasswordActionResult BuildChangePasswordActionResult(
    autofill::FieldRendererId current_password_field_id,
    autofill::FieldRendererId new_password_field_id,
    autofill::FieldRendererId confirm_new_password_field_id,
    const std::optional<autofill::FormData>& form_data) {
  // TODO(crbug.com/580865855): Tab & frame liveness checks. Handle general
  // form-filling error reporting when a tab/frame is destroyed while
  // `FillChangePasswordForm()` is in flight (returning `kTabWentAway` /
  // `kFrameWentAway` instead of `kFormFillingUnknownAutofillError`).
  if (!form_data.has_value()) {
    return ChangePasswordActionResult::Error(
        actor::mojom::ActionResultCode::kFormFillingUnknownAutofillError,
        "Found no eligible form to fill.");
  }

  std::vector<std::string> filled_parts;
  std::vector<std::string> unfilled_parts;
  for (auto [label, renderer_id] :
       {std::pair{"old password", current_password_field_id},
        std::pair{"new generated password", new_password_field_id},
        std::pair{"confirmation password", confirm_new_password_field_id}}) {
    if (!renderer_id) {
      continue;
    }
    std::string part =
        absl::StrFormat("%s into field with id %v", label, renderer_id.value());
    if (WasFieldFilled(*form_data, renderer_id)) {
      filled_parts.push_back(std::move(part));
    } else {
      unfilled_parts.push_back(std::move(part));
    }
  }

  if (filled_parts.empty()) {
    return ChangePasswordActionResult::Error(
        actor::mojom::ActionResultCode::kFormFillingUnknownAutofillError,
        absl::StrFormat("Failed to fill %s.",
                        base::JoinString(unfilled_parts, " and ")));
  }

  std::string message =
      absl::StrFormat("Filled %s.", base::JoinString(filled_parts, " and "));
  if (!unfilled_parts.empty()) {
    return ChangePasswordActionResult::Error(
        actor::mojom::ActionResultCode::kFormFillingUnknownAutofillError,
        absl::StrFormat("%s Failed to fill %s.", message,
                        base::JoinString(unfilled_parts, " and ")));
  }

  return ChangePasswordActionResult::Ok(
      /*requires_page_stabilization=*/true, std::move(message));
}

}  // namespace

// static
ChangePasswordActionResult ChangePasswordActionResult::Ok() {
  return Ok(/*requires_page_stabilization=*/false, std::string());
}

// static
ChangePasswordActionResult ChangePasswordActionResult::Ok(
    bool requires_page_stabilization,
    std::string message) {
  return {.code = actor::mojom::ActionResultCode::kOk,
          .requires_page_stabilization = requires_page_stabilization,
          .message = std::move(message)};
}

// static
ChangePasswordActionResult ChangePasswordActionResult::Error(
    actor::mojom::ActionResultCode code,
    std::string message) {
  CHECK(code != actor::mojom::ActionResultCode::kOk);
  return {.code = code,
          .requires_page_stabilization = false,
          .message = std::move(message)};
}

ChangePasswordFillingHelper::ChangePasswordFillingHelper() = default;
ChangePasswordFillingHelper::~ChangePasswordFillingHelper() = default;

// static
ChangePasswordActionResult ChangePasswordFillingHelper::ValidateProvidedFields(
    bool has_current_password,
    bool has_new_password,
    bool has_confirm_new_password) {
  if (!has_current_password && !has_new_password && !has_confirm_new_password) {
    return ChangePasswordActionResult::Error(
        actor::mojom::ActionResultCode::kArgumentsInvalid,
        "At least one password field must be provided.");
  }
  return ChangePasswordActionResult::Ok();
}

base::expected<autofill::LocalFrameToken, ChangePasswordActionResult>
ChangePasswordFillingHelper::ValidateAndSetFieldIds(
    const autofill::FieldGlobalId& current_password_field_id,
    const autofill::FieldGlobalId& new_password_field_id,
    const autofill::FieldGlobalId& confirm_new_password_field_id) {
  CHECK(!driver_ && !driver_.WasInvalidated());
  CHECK(!current_password_field_id_ && !new_password_field_id_ &&
        !confirm_new_password_field_id_);

  autofill::LocalFrameToken target_frame_token;
  for (const autofill::FieldGlobalId& field_id :
       {current_password_field_id, new_password_field_id,
        confirm_new_password_field_id}) {
    if (!field_id.frame_token && !field_id.renderer_id) {
      continue;
    }
    if (!field_id.frame_token || !field_id.renderer_id) {
      return base::unexpected(ChangePasswordActionResult::Error(
          actor::mojom::ActionResultCode::kArgumentsInvalid,
          "Invalid password field identifier."));
    }
    // Password form filling is scoped to a single frame, so all targeted
    // password fields must belong to the same frame.
    if (target_frame_token && field_id.frame_token != target_frame_token) {
      return base::unexpected(ChangePasswordActionResult::Error(
          actor::mojom::ActionResultCode::kArgumentsInvalid,
          "All password fields must belong to the same frame."));
    }
    target_frame_token = field_id.frame_token;
  }

  if (!target_frame_token) {
    return base::unexpected(ChangePasswordActionResult::Error(
        actor::mojom::ActionResultCode::kArgumentsInvalid,
        "At least one password field must be provided."));
  }

  if ((current_password_field_id &&
       (current_password_field_id == new_password_field_id ||
        current_password_field_id == confirm_new_password_field_id)) ||
      (new_password_field_id &&
       new_password_field_id == confirm_new_password_field_id)) {
    return base::unexpected(ChangePasswordActionResult::Error(
        actor::mojom::ActionResultCode::kArgumentsInvalid,
        "Password field targets must be distinct."));
  }

  current_password_field_id_ = current_password_field_id.renderer_id;
  new_password_field_id_ = new_password_field_id.renderer_id;
  confirm_new_password_field_id_ = confirm_new_password_field_id.renderer_id;
  return target_frame_token;
}

ChangePasswordActionResult
ChangePasswordFillingHelper::ValidateDriverAndTargetFields(
    PasswordManagerDriver& driver) {
  CHECK(!driver_ && !driver_.WasInvalidated());
  CHECK(current_password_field_id_ || new_password_field_id_ ||
        confirm_new_password_field_id_);

  PasswordManagerInterface* password_manager = driver.GetPasswordManager();
  PasswordFormCache* form_cache =
      password_manager ? password_manager->GetPasswordFormCache() : nullptr;
  base::span<const std::unique_ptr<PasswordFormManager>> form_managers =
      form_cache ? form_cache->GetFormManagers()
                 : base::span<const std::unique_ptr<PasswordFormManager>>{};

  std::vector<std::string> invalid_fields;
  for (auto [label, renderer_id] :
       {std::pair{"current_password_field", current_password_field_id_},
        std::pair{"new_password_field", new_password_field_id_},
        std::pair{"confirm_new_password_field",
                  confirm_new_password_field_id_}}) {
    if (!renderer_id) {
      continue;
    }
    if (!FindPasswordFormManagerForField(driver, form_managers, renderer_id)) {
      invalid_fields.push_back(
          absl::StrFormat("%s (id %v)", label, renderer_id.value()));
    }
  }
  if (!invalid_fields.empty()) {
    // TODO(crbug.com/584683291): Introduce a dedicated ActionResultCode for
    // target fields that are not managed password inputs.
    return ChangePasswordActionResult::Error(
        actor::mojom::ActionResultCode::kFormFillingUnknownAutofillError,
        absl::StrFormat("Target field is not a valid password field: %s.",
                        base::JoinString(invalid_fields, ", ")));
  }

  driver_ = driver.AsWeakPtr();
  return ChangePasswordActionResult::Ok();
}

void ChangePasswordFillingHelper::FillChangePasswordForm(
    base::OnceCallback<void(ChangePasswordActionResult)> callback) {
  CHECK(driver_ || driver_.WasInvalidated());
  if (!driver_) {
    base::SequencedTaskRunner::GetCurrentDefault()->PostTask(
        FROM_HERE,
        base::BindOnce(
            std::move(callback),
            ChangePasswordActionResult::Error(
                actor::mojom::ActionResultCode::kFrameWentAway,
                "Target frame or password manager driver is unavailable.")));
    return;
  }

  // Unset (null) or missing `FieldRendererId`s are ignored by the renderer;
  // `FillChangePasswordForm` fills whichever fields resolve to valid input
  // elements and returns `std::nullopt` only if none of them can be filled.
  // TODO(crbug.com/574401623): check actor_login::IsValidFrameAndOriginToFill.
  driver_->FillChangePasswordForm(
      current_password_field_id_, new_password_field_id_,
      confirm_new_password_field_id_,
      /*old_password=*/PasswordString(u"dummy_current_password"),
      /*new_password=*/PasswordString(u"dummy_new_password"),
      mojo::WrapCallbackWithDefaultInvokeIfNotRun(
          base::BindOnce(&BuildChangePasswordActionResult,
                         current_password_field_id_, new_password_field_id_,
                         confirm_new_password_field_id_)
              .Then(base::BindPostTaskToCurrentDefault(std::move(callback))),
          std::nullopt));
}

}  // namespace password_manager
