// Copyright 2014 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/infobars/core/infobar_delegate.h"

#include "build/build_config.h"
#include "components/infobars/core/infobar.h"
#include "components/infobars/core/infobar_manager.h"
#include "ui/base/models/image_model.h"
#include "ui/base/resource/resource_bundle.h"
#include "ui/gfx/vector_icon_types.h"

#if !BUILDFLAG(IS_IOS) && !BUILDFLAG(IS_ANDROID)
#include "ui/gfx/color_palette.h"
#include "ui/gfx/paint_vector_icon.h"
#endif

namespace infobars {

const int InfoBarDelegate::kNoIconID = 0;

InfoBarDelegate::~InfoBarDelegate() {
}

int InfoBarDelegate::GetIconId() const {
  return kNoIconID;
}

const gfx::VectorIcon& InfoBarDelegate::GetVectorIcon() const {
  static gfx::VectorIcon empty_icon;
  return empty_icon;
}

ui::ImageModel InfoBarDelegate::GetIcon() const {
#if !BUILDFLAG(IS_IOS) && !BUILDFLAG(IS_ANDROID)
  const gfx::VectorIcon& vector_icon = GetVectorIcon();
  if (!vector_icon.is_empty())
    return ui::ImageModel::FromVectorIcon(vector_icon, ui::kColorInfoBarIcon,
                                          20);
#endif

  int icon_id = GetIconId();
  return icon_id == kNoIconID
             ? ui::ImageModel()
             : ui::ImageModel::FromImage(
                   ui::ResourceBundle::GetSharedInstance().GetNativeImageNamed(
                       icon_id));
}

std::u16string InfoBarDelegate::GetLinkText() const {
  return std::u16string();
}

GURL InfoBarDelegate::GetLinkURL() const {
  return GURL();
}

bool InfoBarDelegate::EqualsDelegate(InfoBarDelegate* delegate) const {
  return false;
}

bool InfoBarDelegate::ShouldExpire(const NavigationDetails& details) const {
  return details.is_navigation_to_different_page &&
      !details.did_replace_entry &&
      // This next condition ensures a navigation that passes the above
      // conditions doesn't dismiss infobars added while that navigation was
      // already in process.  We carve out an exception for reloads since we
      // want reloads to dismiss infobars, but they will have unchanged entry
      // IDs.
      ((nav_entry_id_ != details.entry_id) || details.is_reload);
}

bool InfoBarDelegate::LinkClicked(WindowOpenDisposition disposition) {
  infobar()->owner()->OpenURL(GetLinkURL(), disposition);
  return false;
}

void InfoBarDelegate::InfoBarDismissed() {
}

bool InfoBarDelegate::IsCloseable() const {
  return true;
}

bool InfoBarDelegate::ShouldAnimate() const {
  return true;
}

ConfirmInfoBarDelegate* InfoBarDelegate::AsConfirmInfoBarDelegate() {
  return nullptr;
}

blocked_content::PopupBlockedInfoBarDelegate*
InfoBarDelegate::AsPopupBlockedInfoBarDelegate() {
  return nullptr;
}

ThemeInstalledInfoBarDelegate*
    InfoBarDelegate::AsThemePreviewInfobarDelegate() {
  return nullptr;
}

#if BUILDFLAG(IS_IOS)
translate::TranslateInfoBarDelegate*
    InfoBarDelegate::AsTranslateInfoBarDelegate() {
  return nullptr;
}
#endif

#if BUILDFLAG(IS_ANDROID)
offline_pages::OfflinePageInfoBarDelegate*
InfoBarDelegate::AsOfflinePageInfoBarDelegate() {
  return nullptr;
}
#endif

InfoBarDelegate::InfoBarDelegate() = default;

}  // namespace infobars
