// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/signin/signin_qrcode_infobar_delegate.h"

#include "base/logging.h"
#include "chrome/browser/profiles/profile.h"
#include "components/infobars/core/infobar.h"
#include "content/public/browser/web_contents.h"

SigninQRCodeInfoBarDelegate::SigninQRCodeInfoBarDelegate(
    content::WebContents* web_contents)
    : profile_(Profile::FromBrowserContext(web_contents->GetBrowserContext())) {
  auto* dice_tab_helper = DiceTabHelper::FromWebContents(web_contents);
  CHECK(dice_tab_helper);
  scoped_observation_.Observe(dice_tab_helper);
}

SigninQRCodeInfoBarDelegate::~SigninQRCodeInfoBarDelegate() = default;

infobars::InfoBarDelegate::InfoBarIdentifier
SigninQRCodeInfoBarDelegate::GetIdentifier() const {
  return infobars::InfoBarDelegate::TEST_INFOBAR;
}

bool SigninQRCodeInfoBarDelegate::EqualsDelegate(
    infobars::InfoBarDelegate* delegate) const {
  return delegate && delegate->GetIdentifier() == GetIdentifier();
}

bool SigninQRCodeInfoBarDelegate::ShouldExpire(
    const NavigationDetails& details) const {
  // Dismissal is handled by DiceTabHelper::Observer.
  return false;
}

bool SigninQRCodeInfoBarDelegate::IsCloseable() const {
  return false;
}

void SigninQRCodeInfoBarDelegate::OnIsChromeSigninPageChanged(
    bool is_signin_page) {
  if (!is_signin_page) {
    if (infobar()) {
      infobar()->RemoveSelf();
    }
  }
}
