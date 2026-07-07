// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/signin/signin_qrcode_infobar.h"

#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/ui/signin/signin_qrcode_infobar_delegate.h"
#include "ui/base/metadata/metadata_impl_macros.h"

SigninQRCodeInfoBar::SigninQRCodeInfoBar(
    Profile* profile,
    std::unique_ptr<SigninQRCodeInfoBarDelegate> delegate)
    : InfoBarView(std::move(delegate)) {
  // Stub implementation for CL 2.5 (WebUI removed, Views not yet added).
}

SigninQRCodeInfoBar::~SigninQRCodeInfoBar() = default;

void SigninQRCodeInfoBar::PlatformSpecificShow(bool animate) {
  InfoBarView::PlatformSpecificShow(animate);
  if (parent()) {
    views::View* shadow = parent()->children().back().get();
    if (shadow) {
      shadow->SetVisible(false);
    }
  }
}

BEGIN_METADATA(SigninQRCodeInfoBar)
END_METADATA
