// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/autofill/payments/webauthn_offer_dialog_model.h"

#include "chrome/app/vector_icons/vector_icons.h"
#include "chrome/browser/ui/autofill/payments/webauthn_offer_dialog_model_observer.h"
#include "components/strings/grit/components_strings.h"
#include "ui/base/l10n/l10n_util.h"
#include "ui/gfx/paint_vector_icon.h"

namespace autofill {

WebauthnOfferDialogModel::WebauthnOfferDialogModel() {
  state_ = kOffer;
}

WebauthnOfferDialogModel::~WebauthnOfferDialogModel() = default;

void WebauthnOfferDialogModel::SetDialogState(DialogState state) {
  state_ = state;
  for (WebauthnOfferDialogModelObserver& observer : observers_)
    observer.OnDialogStateChanged();
}

void WebauthnOfferDialogModel::AddObserver(
    WebauthnOfferDialogModelObserver* observer) {
  observers_.AddObserver(observer);
}

void WebauthnOfferDialogModel::RemoveObserver(
    WebauthnOfferDialogModelObserver* observer) {
  observers_.RemoveObserver(observer);
}

bool WebauthnOfferDialogModel::IsActivityIndicatorVisible() const {
  return state_ == DialogState::kPending;
}

bool WebauthnOfferDialogModel::IsBackButtonVisible() const {
  return false;
}

bool WebauthnOfferDialogModel::IsCancelButtonVisible() const {
  return true;
}

base::string16 WebauthnOfferDialogModel::GetCancelButtonLabel() const {
  switch (state_) {
    case DialogState::kOffer:
    case DialogState::kPending:
      return l10n_util::GetStringUTF16(
          IDS_AUTOFILL_WEBAUTHN_OPT_IN_DIALOG_CANCEL_BUTTON_LABEL);
    case DialogState::kError:
      return l10n_util::GetStringUTF16(
          IDS_AUTOFILL_WEBAUTHN_OPT_IN_DIALOG_CANCEL_BUTTON_LABEL_ERROR);
    case DialogState::kInactive:
    case DialogState::kUnknown:
      break;
  }
  return base::string16();
}

bool WebauthnOfferDialogModel::IsAcceptButtonVisible() const {
  return state_ == DialogState::kOffer || state_ == DialogState::kPending;
}

bool WebauthnOfferDialogModel::IsAcceptButtonEnabled() const {
  return state_ != DialogState::kPending;
}

base::string16 WebauthnOfferDialogModel::GetAcceptButtonLabel() const {
  return l10n_util::GetStringUTF16(
      IDS_AUTOFILL_WEBAUTHN_OPT_IN_DIALOG_OK_BUTTON_LABEL);
}

const gfx::VectorIcon& WebauthnOfferDialogModel::GetStepIllustration(
    ImageColorScheme color_scheme) const {
  switch (state_) {
    case DialogState::kOffer:
    case DialogState::kPending:
      return color_scheme == ImageColorScheme::kDark
                 ? kWebauthnOfferDialogHeaderDarkIcon
                 : kWebauthnOfferDialogHeaderIcon;
    case DialogState::kError:
      return color_scheme == ImageColorScheme::kDark ? kWebauthnErrorDarkIcon
                                                     : kWebauthnErrorIcon;
    case DialogState::kInactive:
    case DialogState::kUnknown:
      break;
  }
  NOTREACHED();
  return gfx::kNoneIcon;
}

base::string16 WebauthnOfferDialogModel::GetStepTitle() const {
  switch (state_) {
    case DialogState::kOffer:
    case DialogState::kPending:
      return l10n_util::GetStringUTF16(
          IDS_AUTOFILL_WEBAUTHN_OPT_IN_DIALOG_TITLE);
    case DialogState::kError:
      return l10n_util::GetStringUTF16(
          IDS_AUTOFILL_WEBAUTHN_OPT_IN_DIALOG_TITLE_ERROR);
    case DialogState::kInactive:
    case DialogState::kUnknown:
      break;
  }
  NOTREACHED();
  return base::string16();
}

base::string16 WebauthnOfferDialogModel::GetStepDescription() const {
  switch (state_) {
    case DialogState::kOffer:
    case DialogState::kPending:
      return l10n_util::GetStringUTF16(
          IDS_AUTOFILL_WEBAUTHN_OPT_IN_DIALOG_INSTRUCTION);
    case DialogState::kError:
      return l10n_util::GetStringUTF16(
          IDS_AUTOFILL_WEBAUTHN_OPT_IN_DIALOG_INSTRUCTION_ERROR);
    case DialogState::kInactive:
    case DialogState::kUnknown:
      break;
  }
  NOTREACHED();
  return base::string16();
}

base::Optional<base::string16>
WebauthnOfferDialogModel::GetAdditionalDescription() const {
  return base::nullopt;
}

ui::MenuModel* WebauthnOfferDialogModel::GetOtherTransportsMenuModel() {
  return nullptr;
}

}  // namespace autofill
