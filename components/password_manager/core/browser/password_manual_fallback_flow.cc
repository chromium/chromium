// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/password_manager/core/browser/password_manual_fallback_flow.h"

#include "components/password_manager/core/browser/password_manager_client.h"

namespace password_manager {

using autofill::Suggestion;

PasswordManualFallbackFlow::PasswordManualFallbackFlow(
    PasswordManagerDriver* password_manager_driver,
    autofill::AutofillClient* autofill_client,
    PasswordManagerClient* password_client,
    std::unique_ptr<SavedPasswordsPresenter> passwords_presenter)
    : suggestion_generator_(password_manager_driver, password_client),
      password_manager_driver_(password_manager_driver),
      autofill_client_(autofill_client),
      password_client_(password_client),
      passwords_presenter_(std::move(passwords_presenter)) {
  passwords_presenter_observation_.Observe(passwords_presenter_.get());
  passwords_presenter_->Init();
}

PasswordManualFallbackFlow::~PasswordManualFallbackFlow() {
  if (deletion_callback_) {
    std::move(deletion_callback_).Run();
  }
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
    const gfx::RectF& bounds,
    base::i18n::TextDirection text_direction) {
  if (flow_state_ != FlowState::kPasswordsRetrived) {
    flow_state_ = FlowState::kInvokedWithoutPasswords;
    saved_bounds_ = bounds;
    saved_text_direction_ = text_direction;
    return;
  }
  RunFlowImpl(bounds, text_direction);
}

void PasswordManualFallbackFlow::OnPopupShown() {}

void PasswordManualFallbackFlow::OnPopupHidden() {}

void PasswordManualFallbackFlow::DidSelectSuggestion(
    const Suggestion& suggestion) {
  CHECK(SupportsSuggestionType(suggestion.popup_item_id));
  switch (suggestion.popup_item_id) {
    case autofill::PopupItemId::kPasswordEntry:
      // TODO(b/321678448): Implement full form preview for acceptable
      // suggestions.
      break;
    case autofill::PopupItemId::kPasswordFieldByFieldFilling:
      // TODO(b/321678448): Implement username preview.
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
  switch (suggestion.popup_item_id) {
    case autofill::PopupItemId::kPasswordEntry:
      // TODO(b/321678448): Fill password form for acceptable suggestions.
      break;
    case autofill::PopupItemId::kPasswordFieldByFieldFilling:
      // TODO(b/321678448): Fill username.
      break;
    case autofill::PopupItemId::kFillPassword:
      // TODO(b/324241248): Conditionally trigger concent dialog and fill
      // password.
      break;
    case autofill::PopupItemId::kViewPasswordDetails:
      // TODO(b/324242001): Trigger password details dialog.
      break;
    case autofill::PopupItemId::kAllSavedPasswordsEntry:
      // TODO(b/321678448): Open password settings.
      break;
    default:
      // Other suggestion types are not supported.
      NOTREACHED_NORETURN();
  }
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

int32_t PasswordManualFallbackFlow::GetWebContentsPopupControllerAxId() const {
  // TODO: Needs to be implemented when we step up accessibility features in the
  // future.
  // See http://crbug.com/991253
  NOTIMPLEMENTED_LOG_ONCE();
  return 0;
}

void PasswordManualFallbackFlow::RegisterDeletionCallback(
    base::OnceClosure deletion_callback) {
  deletion_callback_ = std::move(deletion_callback);
}

void PasswordManualFallbackFlow::RunFlowImpl(
    const gfx::RectF& bounds,
    base::i18n::TextDirection text_direction) {
  std::vector<Suggestion> suggestions =
      suggestion_generator_.GetManualFallbackSuggestions(
          passwords_presenter_->GetSavedPasswords());
  autofill::AutofillClient::PopupOpenArgs open_args(
      bounds, text_direction, std::move(suggestions),
      autofill::AutofillSuggestionTriggerSource::kManualFallbackPasswords);
  autofill_client_->ShowAutofillPopup(open_args,
                                      weak_ptr_factory_.GetWeakPtr());
}

}  // namespace password_manager
