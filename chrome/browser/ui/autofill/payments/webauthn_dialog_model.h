// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_AUTOFILL_PAYMENTS_WEBAUTHN_DIALOG_MODEL_H_
#define CHROME_BROWSER_UI_AUTOFILL_PAYMENTS_WEBAUTHN_DIALOG_MODEL_H_

#include "base/observer_list.h"
#include "chrome/browser/ui/webauthn/authenticator_request_sheet_model.h"

namespace autofill {

class WebauthnDialogModelObserver;
enum class WebauthnDialogState;

// The model for WebauthnDialogView determining what content is shown.
// Owned by the AuthenticatorRequestSheetView.
class WebauthnDialogModel : public AuthenticatorRequestSheetModel {
 public:
  explicit WebauthnDialogModel(WebauthnDialogState dialog_state);
  WebauthnDialogModel(const WebauthnDialogModel&) = delete;
  WebauthnDialogModel& operator=(const WebauthnDialogModel&) = delete;
  ~WebauthnDialogModel() override;

  // Update the current state the dialog should be. When the state is changed,
  // the view's contents should be re-initialized. This should not be used
  // before the view is created.
  void SetDialogState(WebauthnDialogState state);
  WebauthnDialogState dialog_state() { return state_; }

  void AddObserver(WebauthnDialogModelObserver* observer);
  void RemoveObserver(WebauthnDialogModelObserver* observer);

  // AuthenticatorRequestSheetModel:
  bool IsActivityIndicatorVisible() const override;
  bool IsCancelButtonVisible() const override;
  std::u16string GetCancelButtonLabel() const override;
  bool IsAcceptButtonVisible() const override;
  bool IsAcceptButtonEnabled() const override;
  std::u16string GetAcceptButtonLabel() const override;
  std::u16string GetStepTitle() const override;
  std::u16string GetStepDescription() const override;
  // Event handling is handed over to the controller.
  void OnBack() override {}
  void OnAccept() override {}
  void OnCancel() override {}

 private:
  void SetIllustrationsFromState();

  WebauthnDialogState state_;

  base::ObserverList<WebauthnDialogModelObserver> observers_;
};

}  // namespace autofill

#endif  // CHROME_BROWSER_UI_AUTOFILL_PAYMENTS_WEBAUTHN_DIALOG_MODEL_H_
