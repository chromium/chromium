// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_SIGNIN_SIGNIN_QRCODE_INFOBAR_H_
#define CHROME_BROWSER_UI_SIGNIN_SIGNIN_QRCODE_INFOBAR_H_

#include "chrome/browser/ui/views/infobars/infobar_view.h"
#include "ui/base/metadata/metadata_header_macros.h"

namespace views {
class WebView;
}

class Profile;
class SigninQRCodeInfoBarDelegate;

class SigninQRCodeInfoBar : public InfoBarView {
  METADATA_HEADER(SigninQRCodeInfoBar, InfoBarView)

 public:
  SigninQRCodeInfoBar(Profile* profile,
                      std::unique_ptr<SigninQRCodeInfoBarDelegate> delegate);
  SigninQRCodeInfoBar(const SigninQRCodeInfoBar&) = delete;
  SigninQRCodeInfoBar& operator=(const SigninQRCodeInfoBar&) = delete;
  ~SigninQRCodeInfoBar() override;

  // InfoBarView:
  void PlatformSpecificShow(bool animate) override;

 private:
  raw_ptr<views::WebView> web_view_ = nullptr;
};

#endif  // CHROME_BROWSER_UI_SIGNIN_SIGNIN_QRCODE_INFOBAR_H_
