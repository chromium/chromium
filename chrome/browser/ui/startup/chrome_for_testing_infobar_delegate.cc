// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/startup/chrome_for_testing_infobar_delegate.h"

#include <memory>
#include <string>

#include "base/strings/strcat.h"
#include "base/strings/utf_string_conversions.h"
#include "chrome/browser/devtools/global_confirm_info_bar.h"
#include "chrome/browser/infobars/confirm_infobar_creator.h"
#include "chrome/grit/generated_resources.h"
#include "components/infobars/core/infobar.h"
#include "components/version_info/version_info.h"
#include "ui/base/l10n/l10n_util.h"
#include "url/gurl.h"

// static
void ChromeForTestingInfoBarDelegate::Create() {
  GlobalConfirmInfoBar::Show(
      std::make_unique<ChromeForTestingInfoBarDelegate>());
}

infobars::InfoBarDelegate::InfoBarIdentifier
ChromeForTestingInfoBarDelegate::GetIdentifier() const {
  return CHROME_FOR_TESTING_INFOBAR_DELEGATE;
}

std::u16string ChromeForTestingInfoBarDelegate::GetMessageText() const {
  return l10n_util::GetStringFUTF16(
      IDS_CHROME_FOR_TESTING_DISCLAIMER,
      base::UTF8ToUTF16(version_info::GetVersionNumber()));
}

std::u16string ChromeForTestingInfoBarDelegate::GetLinkText() const {
  return l10n_util::GetStringUTF16(IDS_DOWNLOAD_CHROME);
}

GURL ChromeForTestingInfoBarDelegate::GetLinkURL() const {
  return GURL("https://www.google.com/chrome/");
}

bool ChromeForTestingInfoBarDelegate::ShouldExpire(
    const NavigationDetails& details) const {
  return false;
}

bool ChromeForTestingInfoBarDelegate::ShouldAnimate() const {
  // Animating the infobar also animates the content area size which can trigger
  // a flood of page layout, compositing, texture reallocations, etc.  Since
  // this infobar is primarily used for automated testing do not animate the
  // infobar to reduce noise in tests.
  return false;
}

int ChromeForTestingInfoBarDelegate::GetButtons() const {
  return BUTTON_NONE;
}

bool ChromeForTestingInfoBarDelegate::IsCloseable() const {
  return false;
}
