// Copyright 2022 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/page_info/about_this_site_infobar_delegate.h"

#include "base/memory/ptr_util.h"
#include "base/strings/utf_string_conversions.h"
#include "chrome/browser/infobars/confirm_infobar_creator.h"
#include "components/infobars/content/content_infobar_manager.h"
#include "components/infobars/core/infobar.h"
#include "components/infobars/core/infobar_delegate.h"
#include "components/vector_icons/vector_icons.h"
#include "content/public/browser/web_contents.h"

// static
void AboutThisSiteInfoBarDelegate::Create(
    infobars::ContentInfoBarManager* infobar_manager,
    page_info::proto::BannerInfo banner_info,
    base::OnceClosure on_dismissed,
    base::OnceClosure on_url_opened) {
  infobar_manager->AddInfoBar(
      CreateConfirmInfoBar(base::WrapUnique(new AboutThisSiteInfoBarDelegate(
          std::move(banner_info), std::move(on_dismissed),
          std::move(on_url_opened)))));
}

AboutThisSiteInfoBarDelegate::AboutThisSiteInfoBarDelegate(
    page_info::proto::BannerInfo banner_info,
    base::OnceClosure on_dismissed,
    base::OnceClosure on_url_opened)
    : banner_info_(std::move(banner_info)),
      on_dismissed_(std::move(on_dismissed)),
      on_url_opened_(std::move(on_url_opened)) {
  DCHECK(on_dismissed_);
  DCHECK(on_url_opened_);
}

AboutThisSiteInfoBarDelegate::~AboutThisSiteInfoBarDelegate() = default;

infobars::InfoBarDelegate::InfoBarIdentifier
AboutThisSiteInfoBarDelegate::GetIdentifier() const {
  return ABOUT_THIS_SITE_INFOBAR_DELEGATE;
}

const gfx::VectorIcon& AboutThisSiteInfoBarDelegate::GetVectorIcon() const {
  return vector_icons::kWarningIcon;
}

std::u16string AboutThisSiteInfoBarDelegate::GetTitleText() const {
  return banner_info_.has_title() ? base::UTF8ToUTF16(banner_info_.title())
                                  : u"";
}

std::u16string AboutThisSiteInfoBarDelegate::GetMessageText() const {
  return base::UTF8ToUTF16(banner_info_.label());
}

int AboutThisSiteInfoBarDelegate::GetButtons() const {
  return BUTTON_NONE;
}

std::u16string AboutThisSiteInfoBarDelegate::GetLinkText() const {
  return base::UTF8ToUTF16(banner_info_.url().label());
}

GURL AboutThisSiteInfoBarDelegate::GetLinkURL() const {
  return GURL(banner_info_.url().url());
}

bool AboutThisSiteInfoBarDelegate::LinkClicked(
    WindowOpenDisposition disposition) {
  std::move(on_url_opened_).Run();
  return ConfirmInfoBarDelegate::LinkClicked(disposition);
}

void AboutThisSiteInfoBarDelegate::InfoBarDismissed() {
  std::move(on_dismissed_).Run();
}
