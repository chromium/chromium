// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/autofill/payments/webauthn_dialog_model.h"

#include "base/observer_list.h"
#include "chrome/app/vector_icons/vector_icons.h"
#include "chrome/browser/ui/autofill/payments/webauthn_dialog_model_observer.h"
#include "chrome/browser/ui/autofill/payments/webauthn_dialog_state.h"
#include "components/strings/grit/components_strings.h"
#include "ui/base/l10n/l10n_util.h"
#include "ui/gfx/paint_vector_icon.h"

namespace autofill {

WebauthnDialogModel::WebauthnDialogModel(WebauthnDialogState dialog_state)
    : state_(dialog_state) {
  SetIllustrationsFromState();
}

WebauthnDialogModel::~WebauthnDialogModel() = default;

void WebauthnDialogModel::SetDialogState(WebauthnDialogState state) {
  state_ = state;
  SetIllustrationsFromState();
  for (WebauthnDialogModelObserver& observer : observers_)
    observer.OnDialogStateChanged();
}

void WebauthnDialogModel::AddObserver(WebauthnDialogModelObserver* observer) {
  observers_.AddObserver(observer);
}

void WebauthnDialogModel::RemoveObserver(
    WebauthnDialogModelObserver* observer) {
  observers_.RemoveObserver(observer);
}

bool WebauthnDialogModel::IsActivityIndicatorVisible() const {
  return state_ == WebauthnDialogState::kOfferPending ||
         state_ == WebauthnDialogState::kVerifyPending;
}

bool WebauthnDialogModel::IsCancelButtonVisible() const {
  return true;
}

std::u16string WebauthnDialogModel::GetCancelButtonLabel() const {
  switch (state_) {
    case WebauthnDialogState::kOffer:
    case WebauthnDialogState::kOfferPending:
      return l10n_util::GetStringUTF16(
          IDS_AUTOFILL_WEBAUTHN_OPT_IN_DIALOG_CANCEL_BUTTON_LABEL);
    case WebauthnDialogState::kOfferError:
      return l10n_util::GetStringUTF16(
          IDS_AUTOFILL_WEBAUTHN_OPT_IN_DIALOG_CANCEL_BUTTON_LABEL_ERROR);
    case WebauthnDialogState::kVerifyPending:
      return l10n_util::GetStringUTF16(
          IDS_AUTOFILL_WEBAUTHN_VERIFY_PENDING_DIALOG_CANCEL_BUTTON_LABEL);
    case WebauthnDialogState::kInactive:
    case WebauthnDialogState::kUnknown:
      break;
  }
  return std::u16string();
}

bool WebauthnDialogModel::IsAcceptButtonVisible() const {
  return state_ == WebauthnDialogState::kOffer ||
         state_ == WebauthnDialogState::kOfferPending;
}

bool WebauthnDialogModel::IsAcceptButtonEnabled() const {
  return state_ != WebauthnDialogState::kOfferPending;
}

std::u16string WebauthnDialogModel::GetAcceptButtonLabel() const {
  return l10n_util::GetStringUTF16(
      IDS_AUTOFILL_WEBAUTHN_OPT_IN_DIALOG_OK_BUTTON_LABEL);
}

std::u16string WebauthnDialogModel::GetStepTitle() const {
  switch (state_) {
    case WebauthnDialogState::kOffer:
    case WebauthnDialogState::kOfferPending:
      return l10n_util::GetStringUTF16(
          IDS_AUTOFILL_WEBAUTHN_OPT_IN_DIALOG_TITLE);
    case WebauthnDialogState::kOfferError:
      return l10n_util::GetStringUTF16(
          IDS_AUTOFILL_WEBAUTHN_OPT_IN_DIALOG_TITLE_ERROR);
    case WebauthnDialogState::kVerifyPending:
      return l10n_util::GetStringUTF16(
          IDS_AUTOFILL_WEBAUTHN_VERIFY_PENDING_DIALOG_TITLE);
    case WebauthnDialogState::kInactive:
    case WebauthnDialogState::kUnknown:
      break;
  }
  NOTREACHED_IN_MIGRATION();
  return std::u16string();
}

std::u16string WebauthnDialogModel::GetStepDescription() const {
  switch (state_) {
    case WebauthnDialogState::kOffer:
    case WebauthnDialogState::kOfferPending:
      return l10n_util::GetStringUTF16(
          IDS_AUTOFILL_WEBAUTHN_OPT_IN_DIALOG_INSTRUCTION);
    case WebauthnDialogState::kOfferError:
      return l10n_util::GetStringUTF16(
          IDS_AUTOFILL_WEBAUTHN_OPT_IN_DIALOG_INSTRUCTION_ERROR);
    case WebauthnDialogState::kVerifyPending:
      return std::u16string();
    case WebauthnDialogState::kInactive:
    case WebauthnDialogState::kUnknown:
      break;
  }
  NOTREACHED_IN_MIGRATION();
  return std::u16string();
}

void WebauthnDialogModel::SetIllustrationsFromState() {
  switch (state_) {
    case WebauthnDialogState::kOffer:
    case WebauthnDialogState::kOfferPending:
    case WebauthnDialogState::kVerifyPending:
      vector_illustrations_.emplace(kWebauthnDialogHeaderIcon,
                                    kWebauthnDialogHeaderDarkIcon);
      break;
    case WebauthnDialogState::kOfferError:
      vector_illustrations_.emplace(kWebauthnErrorIcon, kWebauthnErrorDarkIcon);
      break;
    case WebauthnDialogState::kInactive:
    case WebauthnDialogState::kUnknown:
      vector_illustrations_.reset();
      break;
  }
}

}  // namespace autofill
