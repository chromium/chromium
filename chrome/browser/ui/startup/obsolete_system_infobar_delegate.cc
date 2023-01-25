// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/startup/obsolete_system_infobar_delegate.h"

#include <memory>

#include "chrome/browser/infobars/confirm_infobar_creator.h"
#include "chrome/browser/obsolete_system/obsolete_system.h"
#include "components/infobars/content/content_infobar_manager.h"
#include "components/infobars/core/infobar.h"
#include "components/strings/grit/components_strings.h"
#include "ui/base/l10n/l10n_util.h"

// static
void ObsoleteSystemInfoBarDelegate::Create(
    infobars::ContentInfoBarManager* infobar_manager) {
  infobar_manager->AddInfoBar(
      CreateConfirmInfoBar(std::unique_ptr<ConfirmInfoBarDelegate>(
          new ObsoleteSystemInfoBarDelegate())));
}

ObsoleteSystemInfoBarDelegate::ObsoleteSystemInfoBarDelegate() = default;

infobars::InfoBarDelegate::InfoBarIdentifier
ObsoleteSystemInfoBarDelegate::GetIdentifier() const {
  return OBSOLETE_SYSTEM_INFOBAR_DELEGATE;
}

std::u16string ObsoleteSystemInfoBarDelegate::GetLinkText() const {
  return l10n_util::GetStringUTF16(IDS_LEARN_MORE);
}

GURL ObsoleteSystemInfoBarDelegate::GetLinkURL() const {
  return GURL(ObsoleteSystem::GetLinkURL());
}

std::u16string ObsoleteSystemInfoBarDelegate::GetMessageText() const {
  return ObsoleteSystem::LocalizedObsoleteString();
}

int ObsoleteSystemInfoBarDelegate::GetButtons() const {
  return BUTTON_NONE;
}

bool ObsoleteSystemInfoBarDelegate::ShouldExpire(
    const NavigationDetails& details) const {
  // Since the obsolete system infobar communicates critical state ("your system
  // is no longer receiving updates") it should persist until explicitly
  // dismissed.
  return false;
}
