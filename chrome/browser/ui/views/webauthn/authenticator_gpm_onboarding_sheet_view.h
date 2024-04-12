// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_VIEWS_WEBAUTHN_AUTHENTICATOR_GPM_ONBOARDING_SHEET_VIEW_H_
#define CHROME_BROWSER_UI_VIEWS_WEBAUTHN_AUTHENTICATOR_GPM_ONBOARDING_SHEET_VIEW_H_

#include "chrome/browser/ui/views/webauthn/authenticator_request_sheet_view.h"
#include "chrome/browser/ui/webauthn/sheet_models.h"

// Represents a sheet in the Web Authentication request dialog presented during
// the Google Password Manager onboarding flow.
class AuthenticatorGpmOnboardingSheetView
    : public AuthenticatorRequestSheetView {
 public:
  explicit AuthenticatorGpmOnboardingSheetView(
      std::unique_ptr<AuthenticatorGpmOnboardingSheetModel> sheet_model);

  AuthenticatorGpmOnboardingSheetView(
      const AuthenticatorGpmOnboardingSheetView&) = delete;
  AuthenticatorGpmOnboardingSheetView& operator=(
      const AuthenticatorGpmOnboardingSheetView&) = delete;

  ~AuthenticatorGpmOnboardingSheetView() override;

 private:
  // AuthenticatorRequestSheetView:
  std::pair<std::unique_ptr<views::View>, AutoFocus> BuildStepSpecificContent()
      override;
};

#endif  // CHROME_BROWSER_UI_VIEWS_WEBAUTHN_AUTHENTICATOR_GPM_ONBOARDING_SHEET_VIEW_H_
