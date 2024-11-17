// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_VIEWS_WEBAUTHN_AUTHENTICATOR_GPM_ARBITRARY_PIN_SHEET_VIEW_H_
#define CHROME_BROWSER_UI_VIEWS_WEBAUTHN_AUTHENTICATOR_GPM_ARBITRARY_PIN_SHEET_VIEW_H_

#include <memory>
#include <string>
#include <utility>

#include "chrome/browser/ui/views/webauthn/authenticator_gpm_arbitrary_pin_view.h"
#include "chrome/browser/ui/views/webauthn/authenticator_request_sheet_view.h"
#include "chrome/browser/ui/webauthn/sheet_models.h"
#include "ui/base/metadata/metadata_header_macros.h"

namespace views {
class View;
}  // namespace views

// Represents a sheet in the Web Authentication request dialog that allows the
// user to enter an arbitrary (alphanumeric) GPM pin code used in passkeys flow.
class AuthenticatorGPMArbitraryPinSheetView
    : public AuthenticatorRequestSheetView,
      public AuthenticatorGPMArbitraryPinView::Delegate {
  METADATA_HEADER(AuthenticatorGPMArbitraryPinSheetView,
                  AuthenticatorRequestSheetView)

 public:
  explicit AuthenticatorGPMArbitraryPinSheetView(
      std::unique_ptr<AuthenticatorGpmArbitraryPinSheetModel> sheet_model);

  AuthenticatorGPMArbitraryPinSheetView(
      const AuthenticatorGPMArbitraryPinSheetView&) = delete;
  AuthenticatorGPMArbitraryPinSheetView& operator=(
      const AuthenticatorGPMArbitraryPinSheetView&) = delete;

  ~AuthenticatorGPMArbitraryPinSheetView() override;

 private:
  AuthenticatorGpmArbitraryPinSheetModel* gpm_arbitrary_pin_sheet_model();

  // AuthenticatorRequestSheetView:
  std::unique_ptr<views::View> BuildStepSpecificHeader() override;
  std::pair<std::unique_ptr<views::View>, AutoFocus> BuildStepSpecificContent()
      override;
  int GetSpacingBetweenTitleAndDescription() override;

  // AuthenticatorGPMArbitraryPinView::Delegate:
  void OnPinChanged(std::u16string pin) override;
};

#endif  // CHROME_BROWSER_UI_VIEWS_WEBAUTHN_AUTHENTICATOR_GPM_ARBITRARY_PIN_SHEET_VIEW_H_
