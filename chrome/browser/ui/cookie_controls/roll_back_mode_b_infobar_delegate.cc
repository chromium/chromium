// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/cookie_controls/roll_back_mode_b_infobar_delegate.h"

#include "base/metrics/histogram_functions.h"
#include "chrome/browser/infobars/confirm_infobar_creator.h"
#include "chrome/common/webui_url_constants.h"
#include "components/infobars/content/content_infobar_manager.h"
#include "components/infobars/core/infobar.h"
#include "components/strings/grit/privacy_sandbox_strings.h"
#include "components/vector_icons/vector_icons.h"
#include "ui/base/l10n/l10n_util.h"
#include "ui/gfx/text_constants.h"

// static
infobars::InfoBar* RollBackModeBInfoBarDelegate::Create(
    infobars::ContentInfoBarManager* infobar_manager) {
  return infobar_manager->AddInfoBar(
      CreateConfirmInfoBar(std::unique_ptr<ConfirmInfoBarDelegate>(
          new RollBackModeBInfoBarDelegate())));
}

RollBackModeBInfoBarDelegate::RollBackModeBInfoBarDelegate() = default;

RollBackModeBInfoBarDelegate::~RollBackModeBInfoBarDelegate() {
  base::UmaHistogramBoolean(
      "Privacy.3PCD.RollbackNotice.AutomaticallyDismissed", !user_action_);
}

infobars::InfoBarDelegate::InfoBarIdentifier
RollBackModeBInfoBarDelegate::GetIdentifier() const {
  return ROLL_BACK_MODE_B_INFOBAR_DELEGATE;
}

const gfx::VectorIcon& RollBackModeBInfoBarDelegate::GetVectorIcon() const {
  return vector_icons::kCookieChromeRefreshIcon;
}

std::u16string RollBackModeBInfoBarDelegate::GetMessageText() const {
  return l10n_util::GetStringUTF16(IDS_MODE_B_ROLLBACK_DESCRIPTION);
}

bool RollBackModeBInfoBarDelegate::Cancel() {
  // The "cancel" button is a link to cookie settings.
  user_action_ = true;
  base::UmaHistogramEnumeration("Privacy.3PCD.RollbackNotice.Action",
                                RollBack3pcdNoticeAction::kSettings);
  infobar()->owner()->OpenURL(GURL(chrome::kChromeUICookieSettingsURL),
                              WindowOpenDisposition::NEW_FOREGROUND_TAB);
  return false;
}

bool RollBackModeBInfoBarDelegate::Accept() {
  user_action_ = true;
  base::UmaHistogramEnumeration("Privacy.3PCD.RollbackNotice.Action",
                                RollBack3pcdNoticeAction::kGotIt);
  return ConfirmInfoBarDelegate::Accept();
}

std::u16string RollBackModeBInfoBarDelegate::GetButtonLabel(
    InfoBarButton button) const {
  DCHECK(button == BUTTON_OK || button == BUTTON_CANCEL);
  return l10n_util::GetStringUTF16((button == BUTTON_OK)
                                       ? IDS_MODE_B_ROLLBACK_GOT_IT
                                       : IDS_MODE_B_ROLLBACK_SETTINGS);
}

bool RollBackModeBInfoBarDelegate::IsCloseable() const {
  return false;
}

bool RollBackModeBInfoBarDelegate::ShouldExpire(
    const NavigationDetails& details) const {
  // Let the infobar persist indefinitely on the current tab; it will be closed
  // when the user switches tabs.
  return false;
}
