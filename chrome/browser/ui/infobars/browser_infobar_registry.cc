// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/infobars/browser_infobar_registry.h"

#include <utility>

#include "base/check.h"
#include "base/functional/bind.h"
#include "base/strings/utf_string_conversions.h"
#include "build/branding_buildflags.h"
#include "build/buildflag.h"
#include "chrome/browser/browser_process.h"
#include "chrome/browser/infobars/browser_infobar_manager.h"
#include "chrome/browser/infobars/infobar_features.h"
#include "chrome/browser/infobars/infobar_spec.h"
#include "chrome/browser/obsolete_system/obsolete_system.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/ssl/known_interception_disclosure_infobar_delegate.h"
#include "chrome/browser/ui/page_info/chrome_page_info_delegate.h"
#include "chrome/grit/branded_strings.h"
#include "chrome/grit/generated_resources.h"
#include "components/infobars/core/infobar_delegate.h"
#include "components/strings/grit/components_strings.h"
#include "components/vector_icons/vector_icons.h"
#include "components/version_info/version_info.h"
#include "content/public/browser/navigation_controller.h"
#include "content/public/browser/web_contents.h"
#include "google_apis/google_api_keys.h"
#include "ui/base/l10n/l10n_util.h"
#include "ui/base/ui_base_features.h"
#include "url/gurl.h"

#if BUILDFLAG(IS_WIN) || BUILDFLAG(IS_MAC)
#include "chrome/browser/ui/startup/default_browser_prompt/pin_infobar/pin_infobar_controller.h"
#include "components/omnibox/browser/vector_icons.h"
#endif

namespace infobars {

void RegisterInfoBars() {
  auto* browser_infobar_manager =
      BrowserInfoBarManager::From(g_browser_process);
  if (!browser_infobar_manager) {
    return;
  }

  if (IsInfoBarMigrated(InfoBarDelegate::COLLECTED_COOKIES_INFOBAR_DELEGATE)) {
    auto spec =
        InfoBarSpec::Builder(
            InfoBarDelegate::COLLECTED_COOKIES_INFOBAR_DELEGATE)
            .SetMessageText(l10n_util::GetStringUTF16(
                IDS_COLLECTED_COOKIES_INFOBAR_MESSAGE))
            .SetIcon(features::IsRoundedIconsEnabled()
                         ? vector_icons::kSettingsIcon
                         : vector_icons::kSettingsChromeRefreshOldIcon)
            .SetScope(InfoBarScope::kTab)
            .AddOkButton(
                l10n_util::GetStringUTF16(IDS_COLLECTED_COOKIES_INFOBAR_BUTTON),
                base::BindRepeating([](content::WebContents* web_contents) {
                  CHECK(web_contents);
                  web_contents->GetController().Reload(
                      content::ReloadType::NORMAL, true);
                }))
            .Build();
    browser_infobar_manager->Register(std::move(spec));
  }

  if (IsInfoBarMigrated(InfoBarDelegate::GOOGLE_API_KEYS_INFOBAR_DELEGATE)) {
    auto spec =
        InfoBarSpec::Builder(InfoBarDelegate::GOOGLE_API_KEYS_INFOBAR_DELEGATE)
            .SetMessageText(
                l10n_util::GetStringUTF16(IDS_MISSING_GOOGLE_API_KEYS))
            .SetLinkText(l10n_util::GetStringUTF16(IDS_LEARN_MORE))
            .SetLinkNavigationUrl(GURL(google_apis::kAPIKeysDevelopersHowToURL))
            .SetScope(InfoBarScope::kTab)
            .Build();
    browser_infobar_manager->Register(std::move(spec));
  }

  if (IsInfoBarMigrated(InfoBarDelegate::PAGE_INFO_INFOBAR_DELEGATE)) {
      ChromePageInfoDelegate::RegisterPageInfoInfoBar(browser_infobar_manager);
  }

  if (IsInfoBarMigrated(InfoBarDelegate::OBSOLETE_SYSTEM_INFOBAR_DELEGATE)) {
    auto spec =
        InfoBarSpec::Builder(InfoBarDelegate::OBSOLETE_SYSTEM_INFOBAR_DELEGATE)
            .SetMessageText(ObsoleteSystem::LocalizedObsoleteString())
            .SetLinkText(l10n_util::GetStringUTF16(IDS_LEARN_MORE))
            .SetLinkNavigationUrl(GURL(ObsoleteSystem::GetLinkURL()))
            .SetScope(InfoBarScope::kTab)
            .SetExpireOnNavigation(false)
            .Build();
    browser_infobar_manager->Register(std::move(spec));
  }

  if (IsInfoBarMigrated(
          InfoBarDelegate::KNOWN_INTERCEPTION_DISCLOSURE_INFOBAR_DELEGATE)) {
    auto spec =
        InfoBarSpec::Builder(
            InfoBarDelegate::KNOWN_INTERCEPTION_DISCLOSURE_INFOBAR_DELEGATE)
            .SetMessageText(
                l10n_util::GetStringUTF16(IDS_KNOWN_INTERCEPTION_HEADER))
            .SetLinkText(l10n_util::GetStringUTF16(IDS_LEARN_MORE))
            .SetLinkNavigationUrl(
                GURL("chrome://connection-monitoring-detected/"))
            .SetScope(InfoBarScope::kTab)
            .SetPriority(InfoBarDelegate::InfobarPriority::kCriticalSecurity)
            .SetExpireOnNavigation(false)
            .SetDismissAction(
                base::BindRepeating([](content::WebContents* web_contents) {
                  CHECK(web_contents);
                  Profile* profile = Profile::FromBrowserContext(
                      web_contents->GetBrowserContext());
                  KnownInterceptionDisclosureCooldown::GetInstance()->Activate(
                      profile);
                }))
            .Build();
    browser_infobar_manager->Register(std::move(spec));
  }

#if BUILDFLAG(IS_WIN) || BUILDFLAG(IS_MAC)
  if (infobars::IsInfoBarMigrated(
          infobars::InfoBarDelegate::PIN_INFOBAR_DELEGATE)) {
    CHECK(browser_infobar_manager);
    auto spec = infobars::InfoBarSpec::Builder(
                    infobars::InfoBarDelegate::PIN_INFOBAR_DELEGATE)
                    .SetMessageText(
                        default_browser::PinInfoBarController::GetMessageText())
                    .SetIcon(features::IsRoundedIconsEnabled()
                                 ? omnibox::kChromeProductIcon
                                 : vector_icons::kProductRefreshIcon)
                    .SetScope(infobars::InfoBarScope::kGlobal)
                    .AddOkButton(
                        default_browser::PinInfoBarController::GetButtonLabel(),
                        base::BindRepeating(
                            &default_browser::PinInfoBarController::OnAccept))
                    .SetDismissAction(base::BindRepeating(
                        &default_browser::PinInfoBarController::OnDismiss))
                    .Build();
    browser_infobar_manager->Register(std::move(spec));
  }
#endif
}

#if BUILDFLAG(CHROME_FOR_TESTING)
void RegisterChromeForTestingInfoBar() {
  if (IsInfoBarMigrated(InfoBarDelegate::CHROME_FOR_TESTING_INFOBAR_DELEGATE)) {
    auto* browser_infobar_manager =
        BrowserInfoBarManager::From(g_browser_process);
    CHECK(browser_infobar_manager);
    auto spec =
        InfoBarSpec::Builder(
            InfoBarDelegate::CHROME_FOR_TESTING_INFOBAR_DELEGATE)
            .SetMessageText(l10n_util::GetStringFUTF16(
                IDS_CHROME_FOR_TESTING_DISCLAIMER,
                base::UTF8ToUTF16(version_info::GetVersionNumber())))
            .SetLinkText(l10n_util::GetStringUTF16(IDS_DOWNLOAD_CHROME))
            .SetLinkNavigationUrl(GURL("https://www.google.com/chrome/"))
            .SetScope(InfoBarScope::kGlobal)
            .SetExpireOnNavigation(false)
            .SetShouldAnimate(false)
            .SetIsCloseable(false)
            .Build();
    browser_infobar_manager->Register(std::move(spec));
  }
}
#endif

}  // namespace infobars
