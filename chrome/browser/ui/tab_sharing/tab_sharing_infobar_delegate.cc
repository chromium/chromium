// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/tab_sharing/tab_sharing_infobar_delegate.h"

#include <utility>

#include "chrome/browser/infobars/infobar_service.h"
#include "chrome/browser/ui/tab_sharing/tab_sharing_ui.h"
#include "chrome/grit/generated_resources.h"
#include "components/infobars/core/infobar.h"
#include "components/vector_icons/vector_icons.h"
#include "ui/base/l10n/l10n_util.h"

// static
infobars::InfoBar* TabSharingInfoBarDelegate::Create(
    InfoBarService* infobar_service,
    const base::string16& shared_tab_name,
    const base::string16& app_name,
    bool is_sharing_allowed,
    TabSharingUI* ui) {
  DCHECK(infobar_service);
  return infobar_service->AddInfoBar(infobar_service->CreateConfirmInfoBar(
      base::WrapUnique(new TabSharingInfoBarDelegate(shared_tab_name, app_name,
                                                     is_sharing_allowed, ui))));
}

TabSharingInfoBarDelegate::TabSharingInfoBarDelegate(
    base::string16 shared_tab_name,
    base::string16 app_name,
    bool is_sharing_allowed,
    TabSharingUI* ui)
    : shared_tab_name_(std::move(shared_tab_name)),
      app_name_(std::move(app_name)),
      is_sharing_allowed_(is_sharing_allowed),
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

base::string16 TabSharingInfoBarDelegate::GetMessageText() const {
  if (shared_tab_name_.empty()) {
    return l10n_util::GetStringFUTF16(
        IDS_TAB_SHARING_INFOBAR_SHARING_CURRENT_TAB_LABEL, app_name_);
  }
  return l10n_util::GetStringFUTF16(
      IDS_TAB_SHARING_INFOBAR_SHARING_ANOTHER_TAB_LABEL, shared_tab_name_,
      app_name_);
}

base::string16 TabSharingInfoBarDelegate::GetButtonLabel(
    InfoBarButton button) const {
  return l10n_util::GetStringUTF16((button == BUTTON_OK)
                                       ? IDS_TAB_SHARING_INFOBAR_STOP_BUTTON
                                       : IDS_TAB_SHARING_INFOBAR_SHARE_BUTTON);
}

int TabSharingInfoBarDelegate::GetButtons() const {
  return shared_tab_name_.empty() || !is_sharing_allowed_
             ? BUTTON_OK
             : BUTTON_OK | BUTTON_CANCEL;
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
