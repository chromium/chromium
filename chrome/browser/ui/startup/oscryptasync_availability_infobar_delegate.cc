// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/startup/oscryptasync_availability_infobar_delegate.h"

#include <memory>

#include "chrome/browser/infobars/confirm_infobar_creator.h"
#include "chrome/browser/lifetime/application_lifetime.h"
#include "chrome/grit/generated_resources.h"
#include "components/infobars/content/content_infobar_manager.h"
#include "components/infobars/core/infobar.h"
#include "components/vector_icons/vector_icons.h"
#include "ui/base/l10n/l10n_util.h"
#include "ui/base/ui_base_features.h"

// static
void OSCryptAsyncAvailabilityInfoBarDelegate::Create(
    infobars::ContentInfoBarManager* infobar_manager) {
  infobar_manager->AddInfoBar(
      CreateConfirmInfoBar(std::unique_ptr<ConfirmInfoBarDelegate>(
          new OSCryptAsyncAvailabilityInfoBarDelegate())));
}

infobars::InfoBarDelegate::InfoBarIdentifier
OSCryptAsyncAvailabilityInfoBarDelegate::GetIdentifier() const {
  return OSCRYPTASYNC_AVAILABILITY_INFOBAR_DELEGATE;
}

infobars::InfoBarDelegate::InfobarPriority
OSCryptAsyncAvailabilityInfoBarDelegate::GetPriority() const {
  return infobars::InfoBarDelegate::InfobarPriority::kCriticalSecurity;
}

const gfx::VectorIcon& OSCryptAsyncAvailabilityInfoBarDelegate::GetVectorIcon()
    const {
  return features::IsRoundedIconsEnabled() ? vector_icons::kErrorFilledIcon
                                           : vector_icons::kErrorOldIcon;
}

std::u16string OSCryptAsyncAvailabilityInfoBarDelegate::GetMessageText() const {
  return l10n_util::GetStringUTF16(
      IDS_OSCRYPTASYNC_AVAILABILITY_INFOBAR_MESSAGE);
}

int OSCryptAsyncAvailabilityInfoBarDelegate::GetButtons() const {
  return BUTTON_OK;
}

std::u16string OSCryptAsyncAvailabilityInfoBarDelegate::GetButtonLabel(
    InfoBarButton button) const {
  CHECK_EQ(button, BUTTON_OK);
  return l10n_util::GetStringUTF16(
      IDS_OSCRYPTASYNC_AVAILABILITY_INFOBAR_BUTTON);
}

bool OSCryptAsyncAvailabilityInfoBarDelegate::Accept() {
  chrome::AttemptRelaunch();
  return false;
}

bool OSCryptAsyncAvailabilityInfoBarDelegate::ShouldExpire(
    const NavigationDetails& details) const {
  return false;
}

bool OSCryptAsyncAvailabilityInfoBarDelegate::IsCloseable() const {
  return false;
}
