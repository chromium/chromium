// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_PAGE_INFO_PAGE_INFO_INFOBAR_DELEGATE_H_
#define CHROME_BROWSER_UI_PAGE_INFO_PAGE_INFO_INFOBAR_DELEGATE_H_

#include "components/infobars/core/confirm_infobar_delegate.h"

namespace infobars {
class ContentInfoBarManager;
}

// This class configures an infobar that is shown when the page info UI
// is closed and the settings for one or more site permissions have been
// changed. The user is shown a message indicating that a reload of the page is
// required for the changes to take effect, and presented a button to perform
// the reload right from the infobar.
class PageInfoInfoBarDelegate : public ConfirmInfoBarDelegate {
 public:
  PageInfoInfoBarDelegate(const PageInfoInfoBarDelegate&) = delete;
  PageInfoInfoBarDelegate& operator=(const PageInfoInfoBarDelegate&) = delete;

  // Creates a page info infobar and delegate and adds the infobar to
  // |infobar_manager|.
  static void Create(infobars::ContentInfoBarManager* infobar_manager);

 private:
  PageInfoInfoBarDelegate();
  ~PageInfoInfoBarDelegate() override;

  // ConfirmInfoBarDelegate:
  infobars::InfoBarDelegate::InfoBarIdentifier GetIdentifier() const override;
  const gfx::VectorIcon& GetVectorIcon() const override;
  std::u16string GetMessageText() const override;
  int GetButtons() const override;
  std::u16string GetButtonLabel(InfoBarButton button) const override;
  bool Accept() override;
};

#endif  // CHROME_BROWSER_UI_PAGE_INFO_PAGE_INFO_INFOBAR_DELEGATE_H_
