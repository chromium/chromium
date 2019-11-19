// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_STARTUP_AUTOMATION_INFOBAR_DELEGATE_H_
#define CHROME_BROWSER_UI_STARTUP_AUTOMATION_INFOBAR_DELEGATE_H_

#include "base/compiler_specific.h"
#include "base/macros.h"
#include "base/strings/string16.h"
#include "components/infobars/core/confirm_infobar_delegate.h"
#include "url/gurl.h"

// An infobar to inform users if their browser is being controlled by an
// automated test.
class AutomationInfoBarDelegate : public ConfirmInfoBarDelegate {
 public:
  static void Create();

 private:
  AutomationInfoBarDelegate();
  ~AutomationInfoBarDelegate() override;

  infobars::InfoBarDelegate::InfoBarIdentifier GetIdentifier() const override;
  bool ShouldExpire(const NavigationDetails& details) const override;
  bool ShouldAnimate() const override;
  base::string16 GetMessageText() const override;
  int GetButtons() const override;

  DISALLOW_COPY_AND_ASSIGN(AutomationInfoBarDelegate);
};

#endif  // CHROME_BROWSER_UI_STARTUP_AUTOMATION_INFOBAR_DELEGATE_H_
