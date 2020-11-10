// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/infobars/core/confirm_infobar_delegate.h"

#include "build/build_config.h"
#include "ui/base/l10n/l10n_util.h"
#include "ui/strings/grit/ui_strings.h"

ConfirmInfoBarDelegate::~ConfirmInfoBarDelegate() = default;

bool ConfirmInfoBarDelegate::EqualsDelegate(
    infobars::InfoBarDelegate* delegate) const {
  ConfirmInfoBarDelegate* confirm_delegate =
      delegate->AsConfirmInfoBarDelegate();
  return confirm_delegate &&
         (confirm_delegate->GetMessageText() == GetMessageText());
}

ConfirmInfoBarDelegate* ConfirmInfoBarDelegate::AsConfirmInfoBarDelegate() {
  return this;
}

infobars::InfoBarDelegate::InfoBarAutomationType
ConfirmInfoBarDelegate::GetInfoBarAutomationType() const {
  return CONFIRM_INFOBAR;
}

base::string16 ConfirmInfoBarDelegate::GetTitleText() const {
  return base::string16();
}

gfx::ElideBehavior ConfirmInfoBarDelegate::GetMessageElideBehavior() const {
  return gfx::ELIDE_TAIL;
}

int ConfirmInfoBarDelegate::GetButtons() const {
  return BUTTON_OK | BUTTON_CANCEL;
}

base::string16 ConfirmInfoBarDelegate::GetButtonLabel(
    InfoBarButton button) const {
  return l10n_util::GetStringUTF16((button == BUTTON_OK) ? IDS_APP_OK
                                                         : IDS_APP_CANCEL);
}

bool ConfirmInfoBarDelegate::OKButtonTriggersUACPrompt() const {
  return false;
}

#if defined(OS_IOS)
bool ConfirmInfoBarDelegate::UseIconBackgroundTint() const {
  return true;
}
#endif

bool ConfirmInfoBarDelegate::Accept() {
  return true;
}

bool ConfirmInfoBarDelegate::Cancel() {
  return true;
}

ConfirmInfoBarDelegate::ConfirmInfoBarDelegate() = default;
