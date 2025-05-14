// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/infobars/core/confirm_infobar_delegate.h"

#include "build/build_config.h"
#include "ui/base/l10n/l10n_util.h"
#include "ui/base/models/image_model.h"
#include "ui/strings/grit/ui_strings.h"

ConfirmInfoBarDelegate::~ConfirmInfoBarDelegate() = default;

bool ConfirmInfoBarDelegate::EqualsDelegate(
    infobars::InfoBarDelegate* delegate) const {
  ConfirmInfoBarDelegate* confirm_delegate =
      delegate->AsConfirmInfoBarDelegate();
  return confirm_delegate &&
         (confirm_delegate->GetMessageText() == GetMessageText());
}

void ConfirmInfoBarDelegate::InfoBarDismissed() {
  for (auto& observer : observers_) {
    observer.OnDismiss();
  }
}

ConfirmInfoBarDelegate* ConfirmInfoBarDelegate::AsConfirmInfoBarDelegate() {
  return this;
}

std::u16string ConfirmInfoBarDelegate::GetTitleText() const {
  return std::u16string();
}

gfx::ElideBehavior ConfirmInfoBarDelegate::GetMessageElideBehavior() const {
  return gfx::ELIDE_TAIL;
}

int ConfirmInfoBarDelegate::GetButtons() const {
  return BUTTON_OK | BUTTON_CANCEL;
}

std::u16string ConfirmInfoBarDelegate::GetButtonLabel(
    InfoBarButton button) const {
  DCHECK(button == BUTTON_OK || button == BUTTON_CANCEL);
  return l10n_util::GetStringUTF16((button == BUTTON_OK) ? IDS_APP_OK
                                                         : IDS_APP_CANCEL);
}

ui::ImageModel ConfirmInfoBarDelegate::GetButtonImage(
    InfoBarButton button) const {
  return ui::ImageModel();
}

bool ConfirmInfoBarDelegate::GetButtonEnabled(InfoBarButton button) const {
  return true;
}

std::u16string ConfirmInfoBarDelegate::GetButtonTooltip(
    InfoBarButton button) const {
  return std::u16string();
}

#if BUILDFLAG(IS_IOS)
bool ConfirmInfoBarDelegate::UseIconBackgroundTint() const {
  return true;
}
#endif

bool ConfirmInfoBarDelegate::Accept() {
  for (auto& observer : observers_) {
    observer.OnAccept();
  }
  return true;
}

bool ConfirmInfoBarDelegate::Cancel() {
  return true;
}

void ConfirmInfoBarDelegate::AddObserver(Observer* observer) {
  observers_.AddObserver(observer);
}

void ConfirmInfoBarDelegate::RemoveObserver(const Observer* observer) {
  observers_.RemoveObserver(observer);
}

ConfirmInfoBarDelegate::ConfirmInfoBarDelegate() = default;
