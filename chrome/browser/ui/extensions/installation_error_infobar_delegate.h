// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_EXTENSIONS_INSTALLATION_ERROR_INFOBAR_DELEGATE_H_
#define CHROME_BROWSER_UI_EXTENSIONS_INSTALLATION_ERROR_INFOBAR_DELEGATE_H_

#include "components/infobars/core/confirm_infobar_delegate.h"
#include "extensions/browser/install/crx_install_error.h"

namespace infobars {
class ContentInfoBarManager;
}

// Helper class to put up an infobar when installation fails.
class InstallationErrorInfoBarDelegate : public ConfirmInfoBarDelegate {
 public:
  InstallationErrorInfoBarDelegate(const InstallationErrorInfoBarDelegate&) =
      delete;
  InstallationErrorInfoBarDelegate& operator=(
      const InstallationErrorInfoBarDelegate&) = delete;

  // Creates an error infobar and delegate and adds the infobar to
  // |infobar_manager|.
  static void Create(infobars::ContentInfoBarManager* infobar_manager,
                     const extensions::CrxInstallError& error);

 private:
  explicit InstallationErrorInfoBarDelegate(
      const extensions::CrxInstallError& error);
  ~InstallationErrorInfoBarDelegate() override;

  // ConfirmInfoBarDelegate:
  infobars::InfoBarDelegate::InfoBarIdentifier GetIdentifier() const override;
  std::u16string GetLinkText() const override;
  GURL GetLinkURL() const override;
  std::u16string GetMessageText() const override;
  int GetButtons() const override;

  extensions::CrxInstallError error_;
};

#endif  // CHROME_BROWSER_UI_EXTENSIONS_INSTALLATION_ERROR_INFOBAR_DELEGATE_H_
