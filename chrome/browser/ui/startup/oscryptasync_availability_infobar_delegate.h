// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_STARTUP_OSCRYPTASYNC_AVAILABILITY_INFOBAR_DELEGATE_H_
#define CHROME_BROWSER_UI_STARTUP_OSCRYPTASYNC_AVAILABILITY_INFOBAR_DELEGATE_H_

#include <string>

#include "components/infobars/core/confirm_infobar_delegate.h"

class BrowserWindowInterface;

namespace infobars {
class ContentInfoBarManager;
}  // namespace infobars

// An infobar that displays a message saying that OSCryptAsync isn't available
// and that various parts of profile data may not be possible to read. The
// suggested remedy is a browser relaunch to authorize keychain access.
class OSCryptAsyncAvailabilityInfoBarDelegate : public ConfirmInfoBarDelegate {
 public:
  // Creates the info-bar only if OSCryptAsync isn't available and the platform
  // is one where the issue can be resolved by a browser relaunch.
  static void MaybeCreate(BrowserWindowInterface* browser);

  // Forces an infobar to appear, bypasses other checks.
  static void CreateForTest(infobars::ContentInfoBarManager* infobar_manager);

 private:
  infobars::InfoBarDelegate::InfoBarIdentifier GetIdentifier() const override;
  infobars::InfoBarDelegate::InfobarPriority GetPriority() const override;
  const gfx::VectorIcon& GetVectorIcon() const override;
  std::u16string GetMessageText() const override;
  int GetButtons() const override;
  std::u16string GetButtonLabel(InfoBarButton button) const override;
  bool Accept() override;
  bool ShouldExpire(const NavigationDetails& details) const override;
  bool IsCloseable() const override;
};

#endif  // CHROME_BROWSER_UI_STARTUP_OSCRYPTASYNC_AVAILABILITY_INFOBAR_DELEGATE_H_
