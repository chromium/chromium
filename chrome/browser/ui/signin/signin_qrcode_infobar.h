// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_SIGNIN_SIGNIN_QRCODE_INFOBAR_H_
#define CHROME_BROWSER_UI_SIGNIN_SIGNIN_QRCODE_INFOBAR_H_

#include <string_view>

#include "base/memory/raw_ptr.h"
#include "base/scoped_observation.h"
#include "chrome/browser/ui/signin/signin_qrcode_model.h"
#include "chrome/browser/ui/views/infobars/infobar_view.h"
#include "ui/base/metadata/metadata_header_macros.h"

namespace views {
class BoxLayoutView;
}

class SigninQRCodeInfoBarDelegate;

// A native C++ Views InfoBar that displays a dynamic QR code during sign-in,
// allowing users to sign in with a passkey from their mobile device.
// It observes SigninQRCodeModel to dynamically update the QR code image.
class SigninQRCodeInfoBar : public InfoBarView,
                            public SigninQRCodeModel::Observer {
  METADATA_HEADER(SigninQRCodeInfoBar, InfoBarView)

 public:
  SigninQRCodeInfoBar(std::unique_ptr<SigninQRCodeInfoBarDelegate> delegate,
                      SigninQRCodeModel* model);
  SigninQRCodeInfoBar(const SigninQRCodeInfoBar&) = delete;
  SigninQRCodeInfoBar& operator=(const SigninQRCodeInfoBar&) = delete;
  ~SigninQRCodeInfoBar() override;

  // InfoBarView:
  void PlatformSpecificShow(bool animate) override;

  // SigninQRCodeModel::Observer:
  void OnQrCodeChanged(std::string_view qr_code_string) override;
  void OnQrCodeReset() override;
  void OnModelDestroyed(SigninQRCodeModel* model) override;

 private:
  void OnQrCodeReady(std::string_view qr_string);

  raw_ptr<views::BoxLayoutView> qr_container_ = nullptr;

  base::ScopedObservation<SigninQRCodeModel, SigninQRCodeModel::Observer>
      model_observation_{this};
};

#endif  // CHROME_BROWSER_UI_SIGNIN_SIGNIN_QRCODE_INFOBAR_H_
