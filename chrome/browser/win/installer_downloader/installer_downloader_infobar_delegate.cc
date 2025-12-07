// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/win/installer_downloader/installer_downloader_infobar_delegate.h"

#include <memory>
#include <string>
#include <utility>

#include "base/check.h"
#include "base/functional/callback.h"
#include "base/memory/ptr_util.h"
#include "chrome/browser/ui/views/infobars/confirm_infobar.h"
#include "chrome/browser/win/installer_downloader/installer_downloader_feature.h"
#include "chrome/grit/branded_strings.h"
#include "chrome/grit/generated_resources.h"
#include "components/infobars/content/content_infobar_manager.h"
#include "components/infobars/core/infobar.h"
#include "components/infobars/core/infobar_manager.h"
#include "components/omnibox/browser/vector_icons.h"
#include "components/strings/grit/components_strings.h"
#include "components/vector_icons/vector_icons.h"
#include "ui/base/l10n/l10n_util.h"
#include "ui/gfx/vector_icon_types.h"
#include "url/gurl.h"

namespace installer_downloader {

// static
infobars::InfoBar* InstallerDownloaderInfoBarDelegate::Show(
    infobars::ContentInfoBarManager* infobar_manager,
    base::OnceClosure accept_cb,
    base::OnceClosure close_cb) {
  std::unique_ptr<InstallerDownloaderInfoBarDelegate> delegate =
      std::make_unique<InstallerDownloaderInfoBarDelegate>(std::move(accept_cb),
                                                           std::move(close_cb));
  return infobar_manager->AddInfoBar(
      std::make_unique<ConfirmInfoBar>(std::move(delegate)));
}

InstallerDownloaderInfoBarDelegate::InstallerDownloaderInfoBarDelegate(
    base::OnceClosure accept_cb,
    base::OnceClosure close_cb)
    : accept_cb_(std::move(accept_cb)), close_cb_(std::move(close_cb)) {}

InstallerDownloaderInfoBarDelegate::~InstallerDownloaderInfoBarDelegate() =
    default;

infobars::InfoBarDelegate::InfoBarIdentifier
InstallerDownloaderInfoBarDelegate::GetIdentifier() const {
  return infobars::InfoBarDelegate::InfoBarIdentifier::
      INSTALLER_DOWNLOADER_INFOBAR_DELEGATE;
}

const gfx::VectorIcon& InstallerDownloaderInfoBarDelegate::GetVectorIcon()
    const {
  return dark_mode() ? omnibox::kProductChromeRefreshIcon
                     : vector_icons::kProductRefreshIcon;
}

bool InstallerDownloaderInfoBarDelegate::ShouldExpire(
    const NavigationDetails& details) const {
  // Returns false if the infobar should not be dismissed on navigation.
  return false;
}

bool InstallerDownloaderInfoBarDelegate::Accept() {
  std::move(accept_cb_).Run();
  return true;
}

void InstallerDownloaderInfoBarDelegate::InfoBarDismissed() {
  std::move(close_cb_).Run();
}

std::u16string InstallerDownloaderInfoBarDelegate::GetMessageText() const {
  return l10n_util::GetStringUTF16(IDS_INSTALLER_DOWNLOADER_DISCLAIMER);
}

std::u16string InstallerDownloaderInfoBarDelegate::GetLinkText() const {
  return l10n_util::GetStringUTF16(IDS_INSTALLER_DOWNLOADER_LINK);
}

int InstallerDownloaderInfoBarDelegate::GetButtons() const {
  return BUTTON_OK;
}

std::u16string InstallerDownloaderInfoBarDelegate::GetButtonLabel(
    InfoBarButton button) const {
  return l10n_util::GetStringUTF16(IDS_INSTALLER_DOWNLOADER_BUTTON_LABEL);
}

GURL InstallerDownloaderInfoBarDelegate::GetLinkURL() const {
  const std::string learn_more_url_str = kLearnMoreUrl.Get();
  GURL learn_more_url(learn_more_url_str);
  CHECK(learn_more_url.is_valid());

  return learn_more_url;
}

}  // namespace installer_downloader
