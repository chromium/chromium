// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_TAB_SHARING_TAB_SHARING_INFOBAR_DELEGATE_H_
#define CHROME_BROWSER_UI_TAB_SHARING_TAB_SHARING_INFOBAR_DELEGATE_H_

#include "components/infobars/core/confirm_infobar_delegate.h"

namespace infobars {
class InfoBar;
}

class InfoBarService;
class TabSharingUI;

// Creates an infobar for sharing a tab using desktopCapture() API; one delegate
// per tab.
// Layout for currently shared tab:
// "Sharing this tab to |app_name_|  [Stop]"
// Layout for all other tabs:
// "Sharing |shared_tab_name_| to |app_name_| [Stop] [Share this tab instead]"
// or if |shared_tab_name_| is empty:
// "Sharing a tab to |app_name_| [Stop] [Share this tab instead]"
class TabSharingInfoBarDelegate : public ConfirmInfoBarDelegate {
 public:
  // Creates a tab sharing infobar. If |shared_tab| is true, it creates an
  // infobar with "currently shared tab" layout (see class comment). If
  // |can_share| is false, [Share this tab] button is not displayed.
  static infobars::InfoBar* Create(InfoBarService* infobar_service,
                                   const base::string16& shared_tab_name,
                                   const base::string16& app_name,
                                   bool shared_tab,
                                   bool can_share,
                                   TabSharingUI* ui);
  ~TabSharingInfoBarDelegate() override = default;

 private:
  TabSharingInfoBarDelegate(base::string16 shared_tab_name,
                            base::string16 app_name,
                            bool shared_tab,
                            bool can_share,
                            TabSharingUI* ui);

  // ConfirmInfoBarDelegate:
  bool EqualsDelegate(InfoBarDelegate* delegate) const override;
  bool ShouldExpire(const NavigationDetails& details) const override;
  infobars::InfoBarDelegate::InfoBarIdentifier GetIdentifier() const override;
  base::string16 GetMessageText() const override;
  base::string16 GetButtonLabel(InfoBarButton button) const override;
  int GetButtons() const override;
  bool Accept() override;
  bool Cancel() override;
  bool IsCloseable() const override;
  const gfx::VectorIcon& GetVectorIcon() const override;

  const base::string16 shared_tab_name_;
  const base::string16 app_name_;
  bool shared_tab_;
  bool can_share_;

  // Creates and removes delegate's infobar; outlives delegate.
  TabSharingUI* ui_;
};

#endif  // CHROME_BROWSER_UI_TAB_SHARING_TAB_SHARING_INFOBAR_DELEGATE_H_
