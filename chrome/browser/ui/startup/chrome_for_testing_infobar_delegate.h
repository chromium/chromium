// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_STARTUP_CHROME_FOR_TESTING_INFOBAR_DELEGATE_H_
#define CHROME_BROWSER_UI_STARTUP_CHROME_FOR_TESTING_INFOBAR_DELEGATE_H_

#include <string>

#include "components/infobars/core/confirm_infobar_delegate.h"
#include "url/gurl.h"

// An infobar for Chrome for Testing, which displays a message saying that this
// flavor of chrome is unsupported and does not auto-update.
class ChromeForTestingInfoBarDelegate : public ConfirmInfoBarDelegate {
 public:
  static void Create();

  ChromeForTestingInfoBarDelegate(const ChromeForTestingInfoBarDelegate&) =
      delete;
  ChromeForTestingInfoBarDelegate& operator=(
      const ChromeForTestingInfoBarDelegate&) = delete;
  ChromeForTestingInfoBarDelegate() = default;
  ~ChromeForTestingInfoBarDelegate() override = default;

 private:
  infobars::InfoBarDelegate::InfoBarIdentifier GetIdentifier() const override;
  std::u16string GetMessageText() const override;
  std::u16string GetLinkText() const override;
  GURL GetLinkURL() const override;
  bool ShouldExpire(const NavigationDetails& details) const override;
  bool ShouldAnimate() const override;
  int GetButtons() const override;
  bool IsCloseable() const override;
};

#endif  // CHROME_BROWSER_UI_STARTUP_CHROME_FOR_TESTING_INFOBAR_DELEGATE_H_
