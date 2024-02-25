// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/startup/test_third_party_cookie_phaseout_infobar_delegate.h"

#include <memory>
#include <string>

#include "chrome/grit/generated_resources.h"
#include "components/infobars/content/content_infobar_manager.h"
#include "components/vector_icons/vector_icons.h"
#include "ui/base/l10n/l10n_util.h"
#include "url/gurl.h"

#if BUILDFLAG(IS_ANDROID)
#include "chrome/browser/infobars/confirm_infobar_creator.h"
#include "components/infobars/core/infobar.h"
#else
#include "chrome/browser/devtools/global_confirm_info_bar.h"
#endif

// static
void TestThirdPartyCookiePhaseoutInfoBarDelegate::Create(
    content::WebContents* web_contents) {
#if BUILDFLAG(IS_ANDROID)
  infobars::ContentInfoBarManager::FromWebContents(web_contents)
      ->AddInfoBar(CreateConfirmInfoBar(
          std::make_unique<TestThirdPartyCookiePhaseoutInfoBarDelegate>()));
#else
  GlobalConfirmInfoBar::Show(
      std::make_unique<TestThirdPartyCookiePhaseoutInfoBarDelegate>());
#endif
}

const gfx::VectorIcon&
TestThirdPartyCookiePhaseoutInfoBarDelegate::GetVectorIcon() const {
  return vector_icons::kErrorOutlineIcon;
}

infobars::InfoBarDelegate::InfoBarIdentifier
TestThirdPartyCookiePhaseoutInfoBarDelegate::GetIdentifier() const {
  return TEST_THIRD_PARTY_COOKIE_PHASEOUT_DELEGATE;
}

std::u16string TestThirdPartyCookiePhaseoutInfoBarDelegate::GetMessageText()
    const {
  return l10n_util::GetStringUTF16(
      IDS_TEST_THIRD_PARTY_COOKIE_BLOCKING_PHASEOUT_INFO);
}

std::u16string TestThirdPartyCookiePhaseoutInfoBarDelegate::GetLinkText()
    const {
  return l10n_util::GetStringUTF16(IDS_DISABLE);
}

GURL TestThirdPartyCookiePhaseoutInfoBarDelegate::GetLinkURL() const {
  return GURL("chrome://flags/#test-third-party-cookie-phaseout");
}

bool TestThirdPartyCookiePhaseoutInfoBarDelegate::ShouldExpire(
    const NavigationDetails& details) const {
  return false;
}

bool TestThirdPartyCookiePhaseoutInfoBarDelegate::ShouldAnimate() const {
  return false;
}

int TestThirdPartyCookiePhaseoutInfoBarDelegate::GetButtons() const {
  return BUTTON_NONE;
}

bool TestThirdPartyCookiePhaseoutInfoBarDelegate::IsCloseable() const {
  return true;
}
