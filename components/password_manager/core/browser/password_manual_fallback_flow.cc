// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/password_manager/core/browser/password_manual_fallback_flow.h"

#include "components/password_manager/core/browser/manage_passwords_referrer.h"
#include "components/password_manager/core/browser/password_form_cache.h"
#include "components/password_manager/core/browser/password_manager_client.h"
#include "components/password_manager/core/browser/password_manager_metrics_util.h"
#include "components/password_manager/core/browser/password_manager_util.h"
#include "components/password_manager/core/browser/password_ui_utils.h"

namespace password_manager {

namespace {
using autofill::Suggestion;

// If `label` was made for an empty username, then return the empty string,
// otherwise return `label`.
std::u16string GetUsernameFromLabel(const std::u16string& label) {
  return label == l10n_util::GetStringUTF16(IDS_PASSWORD_MANAGER_EMPTY_LOGIN)
             ? std::u16string()
             : label;
}
}  // namespace

PasswordManualFallbackFlow::PasswordManualFallbackFlow(
    PasswordManagerDriver* password_manager_driver,
    autofill::AutofillClient* autofill_client,
    PasswordManagerClient* password_client,
    const PasswordFormCache* password_form_cache,
    std::unique_ptr<SavedPasswordsPresenter> passwords_presenter)
    : suggestion_generator_(password_manager_driver, password_client),
      password_manager_driver_(password_manager_driver),
      autofill_client_(autofill_client),
      password_client_(password_client),
      password_form_cache_(password_form_cache),
      passwords_presenter_(std::move(passwords_presenter)) {
  passwords_presenter_observation_.Observe(passwords_presenter_.get());
  passwords_presenter_->Init();
}

PasswordManualFallbackFlow::~PasswordManualFallbackFlow() {
  CancelBiometricReauthIfOngoing();
}

// static
bool PasswordManualFallbackFlow::SupportsSuggestionType(
    autofill::PopupItemId popup_item_id) {
  switch (popup_item_id) {
    case autofill::PopupItemId::kPasswordEntry:
    case autofill::PopupItemId::kPasswordFieldByFieldFilling:
    case autofill::PopupItemId::kFillPassword:
    case autofill::PopupItemId::kViewPasswordDetails:
    case autofill::PopupItemId::kAllSavedPasswordsEntry:
      return true;
    default:
      return false;
  }
}

void PasswordManualFallbackFlow::OnSavedPasswordsChanged(
    const PasswordStoreChangeList& changes) {
  FlowState old_state =
      std::exchange(flow_state_, FlowState::kPasswordsRetrived);
  if (old_state == FlowState::kInvokedWithoutPasswords) {
    CHECK(saved_bounds_);
    CHECK(saved_text_direction_);
    RunFlowImpl(saved_bounds_.value(), saved_text_direction_.value());
    saved_bounds_.reset();
    saved_text_direction_.reset();
  }
}

void PasswordManualFallbackFlow::RunFlow(
    autofill::FieldRendererId field_id,
    const gfx::RectF& bounds,
    base::i18n::TextDirection text_direction) {
  saved_field_id_ = field_id;
  if (flow_state_ != FlowState::kPasswordsRetrived) {
    flow_state_ = FlowState::kInvokedWithoutPasswords;
    saved_bounds_ = bounds;
    saved_text_direction_ = text_direction;
    return;
  }
  RunFlowImpl(bounds, text_direction);
}

absl::variant<autofill::AutofillDriver*, PasswordManagerDriver*>
PasswordManualFallbackFlow::GetDriver() {
  return password_manager_driver_.get();
}

void PasswordManualFallbackFlow::OnPopupShown() {}

void PasswordManualFallbackFlow::OnPopupHidden() {}

void PasswordManualFallbackFlow::DidSelectSuggestion(
    const Suggestion& suggestion) {
  CHECK(SupportsSuggestionType(suggestion.popup_item_id));
  if (!suggestion.is_acceptable) {
    return;
  }
  switch (suggestion.popup_item_id) {
    case autofill::PopupItemId::kPasswordEntry:
      password_manager_driver_->PreviewSuggestion(
          GetUsernameFromLabel(suggestion.additional_label),
          suggestion.GetPayload<Suggestion::ValueToFill>().value());
      break;
    case autofill::PopupItemId::kPasswordFieldByFieldFilling:
      password_manager_driver_->PreviewField(saved_field_id_,
                                             suggestion.main_text.value);
      break;
    case autofill::PopupItemId::kFillPassword:
    case autofill::PopupItemId::kViewPasswordDetails:
    case autofill::PopupItemId::kAllSavedPasswordsEntry:
      // No preview for these suggestions.
      break;
    default:
      // Other suggestion types are not supported.
      NOTREACHED_NORETURN();
  }
}

void PasswordManualFallbackFlow::DidAcceptSuggestion(
    const Suggestion& suggestion,
    const SuggestionPosition& position) {
  CHECK(SupportsSuggestionType(suggestion.popup_item_id));
  if (!suggestion.is_acceptable) {
    return;
  }
  switch (suggestion.popup_item_id) {
    case autofill::PopupItemId::kPasswordEntry:
      MaybeAuthenticateBeforeFilling(base::BindOnce(
          &PasswordManagerDriver::FillSuggestion,
          base::Unretained(password_manager_driver_),
          GetUsernameFromLabel(suggestion.additional_label),
          suggestion.GetPayload<Suggestion::ValueToFill>().value()));
      break;
    case autofill::PopupItemId::kPasswordFieldByFieldFilling:
      password_manager_driver_->FillField(saved_field_id_,
                                          suggestion.main_text.value);
      break;
    case autofill::PopupItemId::kFillPassword:
      MaybeAuthenticateBeforeFilling(base::BindOnce(
          &PasswordManagerDriver::FillField,
          base::Unretained(password_manager_driver_), saved_field_id_,
          suggestion.GetPayload<Suggestion::ValueToFill>().value()));
      break;
    case autofill::PopupItemId::kViewPasswordDetails:
      // TODO(b/324242001): Trigger password details dialog.
      break;
    case autofill::PopupItemId::kAllSavedPasswordsEntry:
      password_client_->NavigateToManagePasswordsPage(
          ManagePasswordsReferrer::kPasswordDropdown);
      metrics_util::LogPasswordDropdownItemSelected(
          metrics_util::PasswordDropdownSelectedOption::kShowAll,
          password_client_->IsOffTheRecord());
      break;
    default:
      // Other suggestion types are not supported.
      NOTREACHED_NORETURN();
  }
  autofill_client_->HideAutofillPopup(
      autofill::PopupHidingReason::kAcceptSuggestion);
}

void PasswordManualFallbackFlow::DidPerformButtonActionForSuggestion(
    const Suggestion& suggestion) {
  // Button actions do currently not exist for password entries.
  NOTREACHED_NORETURN();
}

bool PasswordManualFallbackFlow::RemoveSuggestion(
    const Suggestion& suggestion) {
  // Password suggestions cannot be deleted this way.
  // See http://crbug.com/329038#c15
  return false;
}

void PasswordManualFallbackFlow::ClearPreviewedForm() {
  password_manager_driver_->ClearPreviewedForm();
}

autofill::FillingProduct PasswordManualFallbackFlow::GetMainFillingProduct()
    const {
  return autofill::FillingProduct::kPassword;
}

void PasswordManualFallbackFlow::RunFlowImpl(
    const gfx::RectF& bounds,
    base::i18n::TextDirection text_direction) {
  IsTriggeredOnPasswordForm on_password_form(
      password_form_cache_->HasPasswordForm(password_manager_driver_,
                                            saved_field_id_));
  std::vector<Suggestion> suggestions =
      suggestion_generator_.GetManualFallbackSuggestions(
          passwords_presenter_->GetSavedPasswords(), on_password_form);
  // TODO(crbug.com/991253): Set the right `form_control_ax_id`.
  autofill::AutofillClient::PopupOpenArgs open_args(
      bounds, text_direction, std::move(suggestions),
      autofill::AutofillSuggestionTriggerSource::kManualFallbackPasswords,
      /*form_control_ax_id=*/0);
  autofill_client_->ShowAutofillPopup(open_args,
                                      weak_ptr_factory_.GetWeakPtr());
}

void PasswordManualFallbackFlow::MaybeAuthenticateBeforeFilling(
    base::OnceClosure fill_fields) {
  // TODO(b/324241248): Conditionally trigger consent dialog and fill
  // password.
  CancelBiometricReauthIfOngoing();
  std::unique_ptr<device_reauth::DeviceAuthenticator> authenticator =
      password_client_->GetDeviceAuthenticator();
  // Note: this is currently only implemented on Android, Mac and Windows.
  // For other platforms, the `authenticator` will be null.
  if (!password_client_->CanUseBiometricAuthForFilling(authenticator.get())) {
    std::move(fill_fields).Run();
  } else {
    authenticator_ = std::move(authenticator);

    std::u16string message;
    auto on_reath_complete =
        base::BindOnce(&PasswordManualFallbackFlow::OnBiometricReauthCompleted,
                       weak_ptr_factory_.GetWeakPtr(), std::move(fill_fields));

#if BUILDFLAG(IS_MAC) || BUILDFLAG(IS_WIN)
    const std::u16string origin = base::UTF8ToUTF16(GetShownOrigin(
        url::Origin::Create(password_manager_driver_->GetLastCommittedURL())));
    message =
        l10n_util::GetStringFUTF16(IDS_PASSWORD_MANAGER_FILLING_REAUTH, origin);
#endif
    authenticator_->AuthenticateWithMessage(
        message, metrics_util::TimeCallback(
                     std::move(on_reath_complete),
                     "PasswordManager.PasswordFilling.AuthenticationTime"));
  }
}

void PasswordManualFallbackFlow::OnBiometricReauthCompleted(
    base::OnceClosure fill_fields,
    bool auth_succeeded) {
  authenticator_.reset();
  base::UmaHistogramBoolean(
      "PasswordManager.PasswordFilling.AuthenticationResult", auth_succeeded);
  if (!auth_succeeded) {
    return;
  }
  std::move(fill_fields).Run();
}

void PasswordManualFallbackFlow::CancelBiometricReauthIfOngoing() {
  if (!authenticator_) {
    return;
  }
  authenticator_->Cancel();
  authenticator_.reset();
}

}  // namespace password_manager
