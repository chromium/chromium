// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/extensions/installation_error_infobar_delegate.h"

#include "chrome/browser/infobars/confirm_infobar_creator.h"
#include "components/infobars/content/content_infobar_manager.h"
#include "components/infobars/core/infobar.h"
#include "components/strings/grit/components_strings.h"
#include "ui/base/l10n/l10n_util.h"

// static
void InstallationErrorInfoBarDelegate::Create(
    infobars::ContentInfoBarManager* infobar_manager,
    const extensions::CrxInstallError& error) {
  infobar_manager->AddInfoBar(
      CreateConfirmInfoBar(std::unique_ptr<ConfirmInfoBarDelegate>(
          new InstallationErrorInfoBarDelegate(error))));
}

InstallationErrorInfoBarDelegate::InstallationErrorInfoBarDelegate(
    const extensions::CrxInstallError& error)
    : ConfirmInfoBarDelegate(), error_(error) {}

InstallationErrorInfoBarDelegate::~InstallationErrorInfoBarDelegate() {}

infobars::InfoBarDelegate::InfoBarIdentifier
InstallationErrorInfoBarDelegate::GetIdentifier() const {
  return INSTALLATION_ERROR_INFOBAR_DELEGATE;
}

std::u16string InstallationErrorInfoBarDelegate::GetLinkText() const {
  return error_.type() == extensions::CrxInstallErrorType::OTHER &&
                 error_.detail() == extensions::CrxInstallErrorDetail::
                                        OFFSTORE_INSTALL_DISALLOWED
             ? l10n_util::GetStringUTF16(IDS_LEARN_MORE)
             : std::u16string();
}

GURL InstallationErrorInfoBarDelegate::GetLinkURL() const {
  return GURL("https://support.google.com/chrome_webstore/?p=crx_warning");
}

std::u16string InstallationErrorInfoBarDelegate::GetMessageText() const {
  return error_.message();
}

int InstallationErrorInfoBarDelegate::GetButtons() const {
  return BUTTON_OK;
}
