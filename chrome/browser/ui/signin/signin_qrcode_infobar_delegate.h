// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_SIGNIN_SIGNIN_QRCODE_INFOBAR_DELEGATE_H_
#define CHROME_BROWSER_UI_SIGNIN_SIGNIN_QRCODE_INFOBAR_DELEGATE_H_

#include "base/memory/raw_ptr.h"
#include "base/scoped_observation.h"
#include "chrome/browser/signin/dice_tab_helper.h"
#include "components/infobars/core/infobar_delegate.h"

namespace content {
class WebContents;
}

class Profile;

class SigninQRCodeInfoBarDelegate : public infobars::InfoBarDelegate,
                                    public DiceTabHelper::Observer {
 public:
  explicit SigninQRCodeInfoBarDelegate(content::WebContents* web_contents);
  SigninQRCodeInfoBarDelegate(const SigninQRCodeInfoBarDelegate&) = delete;
  SigninQRCodeInfoBarDelegate& operator=(const SigninQRCodeInfoBarDelegate&) =
      delete;
  ~SigninQRCodeInfoBarDelegate() override;

  Profile* profile() const { return profile_; }

  // InfoBarDelegate:
  infobars::InfoBarDelegate::InfoBarIdentifier GetIdentifier() const override;
  bool EqualsDelegate(InfoBarDelegate* delegate) const override;
  bool ShouldExpire(const NavigationDetails& details) const override;
  bool IsCloseable() const override;

  // DiceTabHelper::Observer:
  void OnIsChromeSigninPageChanged(bool is_signin_page) override;

 private:
  raw_ptr<Profile> profile_;
  base::ScopedObservation<DiceTabHelper, DiceTabHelper::Observer>
      scoped_observation_{this};
};

#endif  // CHROME_BROWSER_UI_SIGNIN_SIGNIN_QRCODE_INFOBAR_DELEGATE_H_
