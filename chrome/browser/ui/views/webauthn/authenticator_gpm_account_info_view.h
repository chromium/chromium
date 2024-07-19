// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_VIEWS_WEBAUTHN_AUTHENTICATOR_GPM_ACCOUNT_INFO_VIEW_H_
#define CHROME_BROWSER_UI_VIEWS_WEBAUTHN_AUTHENTICATOR_GPM_ACCOUNT_INFO_VIEW_H_

#include <memory>

#include "ui/base/metadata/metadata_header_macros.h"
#include "ui/views/view.h"

namespace views {
class BoxLayoutView;
}  // namespace views

class AuthenticatorGpmPinSheetModelBase;

// Represents the header in the Web Authentication request dialog for GPM
// passkeys. It consists of the profile picture and the email label of the
// account.
class AuthenticatorGpmAccountInfoView : public views::View {
  METADATA_HEADER(AuthenticatorGpmAccountInfoView, views::View)
 public:
  explicit AuthenticatorGpmAccountInfoView(
      AuthenticatorGpmPinSheetModelBase* sheet_model);

  AuthenticatorGpmAccountInfoView(const AuthenticatorGpmAccountInfoView&) =
      delete;
  AuthenticatorGpmAccountInfoView& operator=(
      const AuthenticatorGpmAccountInfoView&) = delete;

  ~AuthenticatorGpmAccountInfoView() override;

 private:
  std::unique_ptr<views::BoxLayoutView> row_;
};

#endif  // CHROME_BROWSER_UI_VIEWS_WEBAUTHN_AUTHENTICATOR_GPM_ACCOUNT_INFO_VIEW_H_
