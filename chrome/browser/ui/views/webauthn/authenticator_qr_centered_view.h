// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_VIEWS_WEBAUTHN_AUTHENTICATOR_QR_CENTERED_VIEW_H_
#define CHROME_BROWSER_UI_VIEWS_WEBAUTHN_AUTHENTICATOR_QR_CENTERED_VIEW_H_

#include <string>

#include "base/memory/raw_ptr.h"
#include "ui/base/metadata/metadata_header_macros.h"
#include "ui/gfx/geometry/size.h"
#include "ui/views/controls/image_view.h"
#include "ui/views/view.h"

class AuthenticatorQrCenteredView : public views::View {
  METADATA_HEADER(AuthenticatorQrCenteredView, views::View)

 public:
  static constexpr int kDefaultQrCodeImageSize = 240;
  static constexpr int kDefaultQrCodeMargin = 40;

  explicit AuthenticatorQrCenteredView(const std::string& qr_string,
                                       int qr_size = kDefaultQrCodeImageSize,
                                       int qr_margin = kDefaultQrCodeMargin);
  ~AuthenticatorQrCenteredView() override;

  AuthenticatorQrCenteredView(const AuthenticatorQrCenteredView&) = delete;
  AuthenticatorQrCenteredView& operator=(const AuthenticatorQrCenteredView&) =
      delete;

  void OnThemeChanged() override;

 private:
  raw_ptr<views::ImageView> qr_code_image_;
};

#endif  // CHROME_BROWSER_UI_VIEWS_WEBAUTHN_AUTHENTICATOR_QR_CENTERED_VIEW_H_
