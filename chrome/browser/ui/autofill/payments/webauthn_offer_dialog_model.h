// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_AUTOFILL_PAYMENTS_WEBAUTHN_OFFER_DIALOG_MODEL_H_
#define CHROME_BROWSER_UI_AUTOFILL_PAYMENTS_WEBAUTHN_OFFER_DIALOG_MODEL_H_

#include "base/macros.h"
#include "base/observer_list.h"
#include "chrome/browser/ui/webauthn/authenticator_request_sheet_model.h"
#include "ui/gfx/vector_icon_types.h"

namespace autofill {

class WebauthnOfferDialogModelObserver;

// The model for WebauthnOfferDialogView determining what content is shown.
// Owned by the AuthenticatorRequestSheetView.
class WebauthnOfferDialogModel : public AuthenticatorRequestSheetModel {
 public:
  enum DialogState {
    kUnknown,
    // The dialog is about to be closed automatically. This happens only after
    // authentication challenge is successfully fetched.
    kInactive,
    // The option of using platform authenticator is being offered.
    kOffer,
    // Offer was accepted, fetching authentication challenge.
    kPending,
    // Fetching authentication challenge failed.
    kError,
  };

  WebauthnOfferDialogModel();
  ~WebauthnOfferDialogModel() override;

  void SetDialogState(DialogState state);
  DialogState dialog_state() { return state_; }

  void AddObserver(WebauthnOfferDialogModelObserver* observer);
  void RemoveObserver(WebauthnOfferDialogModelObserver* observer);

  // AuthenticatorRequestSheetModel:
  bool IsActivityIndicatorVisible() const override;
  bool IsBackButtonVisible() const override;
  bool IsCancelButtonVisible() const override;
  base::string16 GetCancelButtonLabel() const override;
  bool IsAcceptButtonVisible() const override;
  bool IsAcceptButtonEnabled() const override;
  base::string16 GetAcceptButtonLabel() const override;
  const gfx::VectorIcon& GetStepIllustration(
      ImageColorScheme color_scheme) const override;
  base::string16 GetStepTitle() const override;
  base::string16 GetStepDescription() const override;
  base::Optional<base::string16> GetAdditionalDescription() const override;
  ui::MenuModel* GetOtherTransportsMenuModel() override;
  // Event handling is handed over to the controller.
  void OnBack() override {}
  void OnAccept() override {}
  void OnCancel() override {}

 private:
  DialogState state_ = DialogState::kUnknown;

  base::ObserverList<WebauthnOfferDialogModelObserver> observers_;

  DISALLOW_COPY_AND_ASSIGN(WebauthnOfferDialogModel);
};

}  // namespace autofill

#endif  // CHROME_BROWSER_UI_AUTOFILL_PAYMENTS_WEBAUTHN_OFFER_DIALOG_MODEL_H_
