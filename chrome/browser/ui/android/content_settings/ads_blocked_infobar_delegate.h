// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_ANDROID_CONTENT_SETTINGS_ADS_BLOCKED_INFOBAR_DELEGATE_H_
#define CHROME_BROWSER_UI_ANDROID_CONTENT_SETTINGS_ADS_BLOCKED_INFOBAR_DELEGATE_H_

#include "base/macros.h"
#include "components/infobars/core/confirm_infobar_delegate.h"

namespace infobars {
class ContentInfoBarManager;
}

// This infobar appears when the user proceeds through Safe Browsing warning
// interstitials to a site with deceptive embedded content. It tells the user
// ads have been blocked and provides a button to reload the page with the
// content unblocked.
//
// The infobar also appears when the site is known to show intrusive ads.
class AdsBlockedInfobarDelegate : public ConfirmInfoBarDelegate {
 public:
  // Creates a subresource filter infobar and delegate and adds the infobar to
  // |infobar_manager|.
  static void Create(infobars::ContentInfoBarManager* infobar_manager);

  ~AdsBlockedInfobarDelegate() override;

  base::string16 GetExplanationText() const;
  base::string16 GetToggleText() const;

  // ConfirmInfoBarDelegate:
  InfoBarIdentifier GetIdentifier() const override;
  int GetIconId() const override;
  GURL GetLinkURL() const override;
  bool LinkClicked(WindowOpenDisposition disposition) override;
  base::string16 GetMessageText() const override;
  int GetButtons() const override;
  base::string16 GetButtonLabel(InfoBarButton button) const override;
  bool Cancel() override;

 private:
  AdsBlockedInfobarDelegate();

  // True when the infobar is in the expanded state.
  bool infobar_expanded_ = false;

  DISALLOW_COPY_AND_ASSIGN(AdsBlockedInfobarDelegate);
};

#endif  // CHROME_BROWSER_UI_ANDROID_CONTENT_SETTINGS_ADS_BLOCKED_INFOBAR_DELEGATE_H_
