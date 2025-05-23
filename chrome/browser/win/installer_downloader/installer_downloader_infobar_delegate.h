// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_WIN_INSTALLER_DOWNLOADER_INSTALLER_DOWNLOADER_INFOBAR_DELEGATE_H_
#define CHROME_BROWSER_WIN_INSTALLER_DOWNLOADER_INSTALLER_DOWNLOADER_INFOBAR_DELEGATE_H_

#include <memory>
#include <string>

#include "base/functional/callback_forward.h"
#include "components/infobars/core/confirm_infobar_delegate.h"
#include "components/infobars/core/infobar_delegate.h"
#include "content/public/browser/web_contents.h"
#include "ui/base/models/image_model.h"
#include "ui/base/window_open_disposition.h"

namespace infobars {
class InfoBar;
class ContentInfoBarManager;
}  // namespace infobars

namespace installer_downloader {

class InstallerDownloaderInfoBarDelegate : public ConfirmInfoBarDelegate {
 public:
  // Creates a installer downloader infobar and adds it to the provided
  // `infobar_manager`. The `infobar_manager` will own the returned infobar.
  // `accept_cb` is called when the user accepts the infobar.
  static infobars::InfoBar* Show(infobars::ContentInfoBarManager* contents,
                                 base::OnceClosure accept_cb,
                                 base::OnceClosure close_cb);

  InstallerDownloaderInfoBarDelegate& operator=(
      const InstallerDownloaderInfoBarDelegate&) = delete;

  explicit InstallerDownloaderInfoBarDelegate(base::OnceClosure accept_cb,
                                              base::OnceClosure close_cb);
  ~InstallerDownloaderInfoBarDelegate() override;

  // ConfirmInfoBarDelegate:
  infobars::InfoBarDelegate::InfoBarIdentifier GetIdentifier() const override;
  const gfx::VectorIcon& GetVectorIcon() const override;
  bool ShouldExpire(const NavigationDetails& details) const override;
  bool Accept() override;
  void InfoBarDismissed() override;
  std::u16string GetMessageText() const override;
  std::u16string GetLinkText() const override;
  int GetButtons() const override;
  std::u16string GetButtonLabel(InfoBarButton button) const override;
  GURL GetLinkURL() const override;

 private:
  base::OnceClosure accept_cb_;
  base::OnceClosure close_cb_;
};

}  // namespace installer_downloader

#endif  // CHROME_BROWSER_WIN_INSTALLER_DOWNLOADER_INSTALLER_DOWNLOADER_INFOBAR_DELEGATE_H_
