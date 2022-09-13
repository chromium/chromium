// Copyright 2016 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_SUBRESOURCE_FILTER_CONTENT_BROWSER_ADS_BLOCKED_INFOBAR_DELEGATE_H_
#define COMPONENTS_SUBRESOURCE_FILTER_CONTENT_BROWSER_ADS_BLOCKED_INFOBAR_DELEGATE_H_

#include "components/infobars/android/infobar_android.h"
#include "components/infobars/core/confirm_infobar_delegate.h"

namespace infobars {
class ContentInfoBarManager;
}

namespace subresource_filter {

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

  AdsBlockedInfobarDelegate(const AdsBlockedInfobarDelegate&) = delete;
  AdsBlockedInfobarDelegate& operator=(const AdsBlockedInfobarDelegate&) =
      delete;

  ~AdsBlockedInfobarDelegate() override;

  std::u16string GetExplanationText() const;
  std::u16string GetToggleText() const;

  // ConfirmInfoBarDelegate:
  InfoBarIdentifier GetIdentifier() const override;
  int GetIconId() const override;
  GURL GetLinkURL() const override;
  bool LinkClicked(WindowOpenDisposition disposition) override;
  std::u16string GetMessageText() const override;
  int GetButtons() const override;
  std::u16string GetButtonLabel(InfoBarButton button) const override;
  bool Cancel() override;

 private:
  AdsBlockedInfobarDelegate();

  // True when the infobar is in the expanded state.
  bool infobar_expanded_ = false;
};

}  // namespace subresource_filter

#endif  // COMPONENTS_SUBRESOURCE_FILTER_CONTENT_BROWSER_ADS_BLOCKED_INFOBAR_DELEGATE_H_
