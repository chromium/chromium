// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/infobars/core/infobar_delegate.h"

#include "base/logging.h"
#include "base/no_destructor.h"
#include "build/build_config.h"
#include "components/infobars/core/infobar.h"
#include "components/infobars/core/infobar_manager.h"
#include "ui/base/resource/resource_bundle.h"
#include "ui/gfx/vector_icon_types.h"

#if !defined(OS_IOS) && !defined(OS_ANDROID)
#include "ui/gfx/color_palette.h"
#include "ui/gfx/paint_vector_icon.h"
#endif

namespace infobars {

const int InfoBarDelegate::kNoIconID = 0;

InfoBarDelegate::~InfoBarDelegate() {
}

InfoBarDelegate::InfoBarAutomationType
    InfoBarDelegate::GetInfoBarAutomationType() const {
  return UNKNOWN_INFOBAR;
}

int InfoBarDelegate::GetIconId() const {
  return kNoIconID;
}

const gfx::VectorIcon& InfoBarDelegate::GetVectorIcon() const {
  static base::NoDestructor<gfx::VectorIcon> empty_icon;
  return *empty_icon;
}

gfx::Image InfoBarDelegate::GetIcon() const {
#if !defined(OS_IOS) && !defined(OS_ANDROID)
  const gfx::VectorIcon& vector_icon = GetVectorIcon();
  if (!vector_icon.is_empty()) {
    return gfx::Image(
        gfx::CreateVectorIcon(vector_icon, 20, gfx::kGoogleBlue500));
  }
#endif

  int icon_id = GetIconId();
  return icon_id == kNoIconID
             ? gfx::Image()
             : ui::ResourceBundle::GetSharedInstance().GetNativeImageNamed(
                   icon_id);
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

HungRendererInfoBarDelegate* InfoBarDelegate::AsHungRendererInfoBarDelegate() {
  return nullptr;
}

PopupBlockedInfoBarDelegate* InfoBarDelegate::AsPopupBlockedInfoBarDelegate() {
  return nullptr;
}

ThemeInstalledInfoBarDelegate*
    InfoBarDelegate::AsThemePreviewInfobarDelegate() {
  return nullptr;
}

translate::TranslateInfoBarDelegate*
    InfoBarDelegate::AsTranslateInfoBarDelegate() {
  return nullptr;
}

#if defined(OS_ANDROID)
offline_pages::OfflinePageInfoBarDelegate*
InfoBarDelegate::AsOfflinePageInfoBarDelegate() {
  return nullptr;
}
#endif

InfoBarDelegate::InfoBarDelegate() : nav_entry_id_(0) {
}

}  // namespace infobars
