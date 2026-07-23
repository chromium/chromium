// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_SIGNIN_SIGNIN_QRCODE_VIEW_H_
#define CHROME_BROWSER_UI_SIGNIN_SIGNIN_QRCODE_VIEW_H_

#include <string_view>

#include "base/memory/raw_ptr.h"
#include "ui/base/metadata/metadata_header_macros.h"
#include "ui/views/layout/box_layout_view.h"

// A common view that displays the passkey QR code and text. Used by the
// SigninQRCodeInfoBar.
class SigninQRCodeView : public views::BoxLayoutView {
  METADATA_HEADER(SigninQRCodeView, views::BoxLayoutView)

 public:
  SigninQRCodeView();
  SigninQRCodeView(const SigninQRCodeView&) = delete;
  SigninQRCodeView& operator=(const SigninQRCodeView&) = delete;
  ~SigninQRCodeView() override;

  // Updates the QR code container to display the given QR code string,
  // replacing the loading spinner.
  void UpdateQrCode(std::string_view qr_string);

  // Returns true if the QR code image is currently shown.
  bool IsShowingQrCodeForTesting() const;

 private:
  raw_ptr<views::BoxLayoutView> qr_container_ = nullptr;
};

#endif  // CHROME_BROWSER_UI_SIGNIN_SIGNIN_QRCODE_VIEW_H_
