// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_STARTUP_AUTOMATION_INFOBAR_DELEGATE_H_
#define CHROME_BROWSER_UI_STARTUP_AUTOMATION_INFOBAR_DELEGATE_H_

#include <string>

#include "components/infobars/content/content_infobar_manager.h"
#include "components/infobars/core/confirm_infobar_delegate.h"

// An infobar to inform users if their browser is being controlled by an
// automated test.
class AutomationInfoBarDelegate : public ConfirmInfoBarDelegate {
 public:
  static void Create();
  static infobars::InfoBar* Create(
      infobars::ContentInfoBarManager* infobar_manager);

  AutomationInfoBarDelegate(const AutomationInfoBarDelegate&) = delete;
  AutomationInfoBarDelegate& operator=(const AutomationInfoBarDelegate&) =
      delete;

 private:
  AutomationInfoBarDelegate() = default;
  ~AutomationInfoBarDelegate() override = default;

  infobars::InfoBarDelegate::InfoBarIdentifier GetIdentifier() const override;
  bool ShouldExpire(const NavigationDetails& details) const override;
  bool ShouldAnimate() const override;
  std::u16string GetMessageText() const override;
  int GetButtons() const override;
};

#endif  // CHROME_BROWSER_UI_STARTUP_AUTOMATION_INFOBAR_DELEGATE_H_
