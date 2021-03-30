// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_COLLECTED_COOKIES_INFOBAR_DELEGATE_H_
#define CHROME_BROWSER_UI_COLLECTED_COOKIES_INFOBAR_DELEGATE_H_

#include "base/macros.h"
#include "components/infobars/core/confirm_infobar_delegate.h"

class InfoBarService;

// This class configures an infobar shown when the collected cookies dialog
// is closed and the settings for one or more cookies have been changed.  The
// user is shown a message indicating that a reload of the page is
// required for the changes to take effect, and presented a button to perform
// the reload right from the infobar.
class CollectedCookiesInfoBarDelegate : public ConfirmInfoBarDelegate {
 public:
  // Creates a collected cookies infobar and delegate and adds the infobar to
  // |infobar_service|.
  static void Create(InfoBarService* infobar_service);

 private:
  CollectedCookiesInfoBarDelegate();
  ~CollectedCookiesInfoBarDelegate() override;

  // ConfirmInfoBarDelegate:
  infobars::InfoBarDelegate::InfoBarIdentifier GetIdentifier() const override;
  const gfx::VectorIcon& GetVectorIcon() const override;
  std::u16string GetMessageText() const override;
  int GetButtons() const override;
  std::u16string GetButtonLabel(InfoBarButton button) const override;
  bool Accept() override;

  DISALLOW_COPY_AND_ASSIGN(CollectedCookiesInfoBarDelegate);
};

#endif  // CHROME_BROWSER_UI_COLLECTED_COOKIES_INFOBAR_DELEGATE_H_
