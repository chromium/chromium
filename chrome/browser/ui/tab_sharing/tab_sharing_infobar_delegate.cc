// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/tab_sharing/tab_sharing_infobar_delegate.h"

#include <utility>

#include "base/memory/ptr_util.h"
#include "chrome/browser/infobars/infobar_service.h"
#include "chrome/browser/ui/tab_sharing/tab_sharing_ui.h"
#include "chrome/grit/generated_resources.h"
#include "components/infobars/core/infobar.h"
#include "components/vector_icons/vector_icons.h"
#include "ui/base/l10n/l10n_util.h"

// static
infobars::InfoBar* TabSharingInfoBarDelegate::Create(
    InfoBarService* infobar_service,
    const std::u16string& shared_tab_name,
    const std::u16string& app_name,
    bool shared_tab,
    bool can_share,
    TabSharingUI* ui) {
  DCHECK(infobar_service);
  return infobar_service->AddInfoBar(infobar_service->CreateConfirmInfoBar(
      base::WrapUnique(new TabSharingInfoBarDelegate(
          shared_tab_name, app_name, shared_tab, can_share, ui))));
}

TabSharingInfoBarDelegate::TabSharingInfoBarDelegate(
    std::u16string shared_tab_name,
    std::u16string app_name,
    bool shared_tab,
    bool can_share,
    TabSharingUI* ui)
    : shared_tab_name_(std::move(shared_tab_name)),
      app_name_(std::move(app_name)),
      shared_tab_(shared_tab),
      can_share_(can_share),
      ui_(ui) {}

bool TabSharingInfoBarDelegate::EqualsDelegate(
    InfoBarDelegate* delegate) const {
  return false;
}

bool TabSharingInfoBarDelegate::ShouldExpire(
    const NavigationDetails& details) const {
  return false;
}

infobars::InfoBarDelegate::InfoBarIdentifier
TabSharingInfoBarDelegate::GetIdentifier() const {
  return TAB_SHARING_INFOBAR_DELEGATE;
}

std::u16string TabSharingInfoBarDelegate::GetMessageText() const {
  if (shared_tab_) {
    return l10n_util::GetStringFUTF16(
        IDS_TAB_SHARING_INFOBAR_SHARING_CURRENT_TAB_LABEL, app_name_);
  }
  return !shared_tab_name_.empty()
             ? l10n_util::GetStringFUTF16(
                   IDS_TAB_SHARING_INFOBAR_SHARING_ANOTHER_TAB_LABEL,
                   shared_tab_name_, app_name_)
             : l10n_util::GetStringFUTF16(
                   IDS_TAB_SHARING_INFOBAR_SHARING_ANOTHER_UNTITLED_TAB_LABEL,
                   app_name_);
}

std::u16string TabSharingInfoBarDelegate::GetButtonLabel(
    InfoBarButton button) const {
  return l10n_util::GetStringUTF16((button == BUTTON_OK)
                                       ? IDS_TAB_SHARING_INFOBAR_STOP_BUTTON
                                       : IDS_TAB_SHARING_INFOBAR_SHARE_BUTTON);
}

int TabSharingInfoBarDelegate::GetButtons() const {
  return shared_tab_ || !can_share_ ? BUTTON_OK : BUTTON_OK | BUTTON_CANCEL;
}

bool TabSharingInfoBarDelegate::Accept() {
  ui_->StopSharing();
  return false;
}

bool TabSharingInfoBarDelegate::Cancel() {
  ui_->StartSharing(infobar());
  return false;
}

bool TabSharingInfoBarDelegate::IsCloseable() const {
  return false;
}

const gfx::VectorIcon& TabSharingInfoBarDelegate::GetVectorIcon() const {
  return vector_icons::kScreenShareIcon;
}
