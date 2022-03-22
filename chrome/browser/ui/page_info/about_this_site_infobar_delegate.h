// Copyright 2022 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_PAGE_INFO_ABOUT_THIS_SITE_INFOBAR_DELEGATE_H_
#define CHROME_BROWSER_UI_PAGE_INFO_ABOUT_THIS_SITE_INFOBAR_DELEGATE_H_

#include "base/callback.h"
#include "components/infobars/core/confirm_infobar_delegate.h"
#include "components/page_info/core/proto/about_this_site_metadata.pb.h"

namespace infobars {
class ContentInfoBarManager;
}  // namespace infobars

// This class configures an infobar that is shown when there is information to
// display from AboutThisSiteBannerController.
class AboutThisSiteInfoBarDelegate : public ConfirmInfoBarDelegate {
 public:
  AboutThisSiteInfoBarDelegate(const AboutThisSiteInfoBarDelegate&) = delete;
  AboutThisSiteInfoBarDelegate& operator=(const AboutThisSiteInfoBarDelegate&) =
      delete;
  ~AboutThisSiteInfoBarDelegate() override;

  // Creates an AboutThisSite infobar and delegate and adds the infobar to
  // |infobar_manager|.
  static void Create(infobars::ContentInfoBarManager* infobar_manager,
                     page_info::proto::BannerInfo banner_info,
                     base::OnceClosure on_dismissed,
                     base::OnceClosure on_url_opened);

 private:
  explicit AboutThisSiteInfoBarDelegate(
      page_info::proto::BannerInfo banner_info,
      base::OnceClosure on_dismissed,
      base::OnceClosure on_url_opened);

  // ConfirmInfoBarDelegate:
  infobars::InfoBarDelegate::InfoBarIdentifier GetIdentifier() const override;
  const gfx::VectorIcon& GetVectorIcon() const override;
  std::u16string GetTitleText() const override;
  std::u16string GetMessageText() const override;
  int GetButtons() const override;
  std::u16string GetLinkText() const override;
  GURL GetLinkURL() const override;
  bool LinkClicked(WindowOpenDisposition disposition) override;
  void InfoBarDismissed() override;

  page_info::proto::BannerInfo banner_info_;
  base::OnceClosure on_dismissed_;
  base::OnceClosure on_url_opened_;
};

#endif  // CHROME_BROWSER_UI_PAGE_INFO_ABOUT_THIS_SITE_INFOBAR_DELEGATE_H_
