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
#include "chrome/app/vector_icons/vector_icons.h"
#include "chrome/browser/browser_process.h"
#include "chrome/browser/infobars/browser_infobar_manager.h"
#include "chrome/browser/infobars/infobar_features.h"
#include "chrome/browser/infobars/infobar_spec.h"
#include "chrome/browser/lifetime/application_lifetime.h"
#include "chrome/browser/obsolete_system/obsolete_system.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/ssl/known_interception_disclosure_infobar_delegate.h"
#include "chrome/browser/ui/browser_window/public/browser_window_interface.h"
#include "chrome/browser/ui/page_info/chrome_page_info_delegate.h"
#include "chrome/grit/branded_strings.h"
#include "chrome/grit/generated_resources.h"
#include "components/infobars/core/infobar_delegate.h"
#include "components/strings/grit/components_strings.h"
#include "components/vector_icons/vector_icons.h"
#include "components/version_info/version_info.h"
#include "content/public/browser/navigation_controller.h"
#include "content/public/browser/web_contents.h"
#include "extensions/buildflags/buildflags.h"
#include "google_apis/google_api_keys.h"
#include "ui/base/l10n/l10n_util.h"
#include "ui/base/ui_base_features.h"
#include "ui/strings/grit/ui_strings.h"
#include "url/gurl.h"

#if BUILDFLAG(ENABLE_EXTENSIONS)
#include "chrome/browser/extensions/api/debugger/debugger_api.h"
#endif

#include "components/omnibox/browser/vector_icons.h"

#if BUILDFLAG(IS_WIN) || BUILDFLAG(IS_MAC)
#include "chrome/browser/ui/startup/default_browser_prompt/pin_infobar/pin_infobar_controller.h"
#endif

#if BUILDFLAG(IS_WIN) || BUILDFLAG(IS_MAC) || BUILDFLAG(IS_LINUX)
#include "chrome/browser/ui/views/session_restore_infobar/session_restore_infobar_manager.h"
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

#if !BUILDFLAG(IS_CHROMEOS)
  if (IsInfoBarMigrated(
          InfoBarDelegate::ENABLE_LINK_CAPTURING_INFOBAR_DELEGATE)) {
    auto spec = InfoBarSpec::Builder(
                    InfoBarDelegate::ENABLE_LINK_CAPTURING_INFOBAR_DELEGATE)
                    .SetIcon(features::IsRoundedIconsEnabled()
                                 ? vector_icons::kSettingsFilledIcon
                                 : vector_icons::kSettingsOldIcon)
                    .SetScope(InfoBarScope::kTab)
                    .AddOkButton(
                        l10n_util::GetStringUTF16(
                            IDR_INTENT_PICKER_SUPPORTED_LINKS_INFOBAR_OK_LABEL),
                        base::DoNothing())
                    .AddCancelButton(l10n_util::GetStringUTF16(IDS_NO_THANKS),
                                     base::DoNothing())
                    .Build();
    browser_infobar_manager->Register(std::move(spec));
  }
#endif

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
                    // Only offer to pin in normal, non-incognito,
                    // non-guest browsers.
                    .SetBrowserFilter(base::BindRepeating(
                        [](BrowserWindowInterface* browser) {
                          const Profile* profile = browser->GetProfile();
                          return browser->GetType() ==
                                     BrowserWindowInterface::TYPE_NORMAL &&
                                 !profile->IsIncognitoProfile() &&
                                 !profile->IsGuestSession();
                        }))
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

  if (IsInfoBarMigrated(InfoBarDelegate::LOCAL_TEST_POLICIES_APPLIED_INFOBAR)) {
    auto spec = InfoBarSpec::Builder(
                    InfoBarDelegate::LOCAL_TEST_POLICIES_APPLIED_INFOBAR)
                    .SetMessageText(l10n_util::GetStringUTF16(
                        IDS_LOCAL_TEST_POLICIES_ENABLED))
                    .SetScope(InfoBarScope::kGlobal)
                    .SetExpireOnNavigation(false)
                    .SetShouldAnimate(false)
                    .SetIsCloseable(false)
                    .SetPriority(InfoBarDelegate::InfobarPriority::kLow)
                    .Build();
    browser_infobar_manager->Register(std::move(spec));
  }

  if (IsInfoBarMigrated(InfoBarDelegate::THEME_INSTALLED_INFOBAR_DELEGATE)) {
    auto spec =
        InfoBarSpec::Builder(InfoBarDelegate::THEME_INSTALLED_INFOBAR_DELEGATE)
            .SetIcon(features::IsRoundedIconsEnabled() ? kBrushFilledIcon
                                                       : kPaintbrushOldIcon)
            .SetScope(InfoBarScope::kTab)
            .AddCancelButton(l10n_util::GetStringUTF16(
                                 IDS_THEME_INSTALL_INFOBAR_UNDO_BUTTON),
                             base::DoNothing())
            .Build();
    browser_infobar_manager->Register(std::move(spec));
  }

#if BUILDFLAG(ENABLE_EXTENSIONS)
  if (IsInfoBarMigrated(
          InfoBarDelegate::EXTENSION_DEV_TOOLS_INFOBAR_DELEGATE)) {
    auto spec =
        InfoBarSpec::Builder(
            InfoBarDelegate::EXTENSION_DEV_TOOLS_INFOBAR_DELEGATE)
            .SetMessageTextTemplate(
                l10n_util::GetStringUTF16(IDS_DEV_TOOLS_INFOBAR_LABEL))
            .SetSubstitutionsCallback(base::BindRepeating(
                [](content::WebContents*) {
                  return extensions::ExtensionDevToolsInfoBarController::
                      GetMessageSubstitutions();
                }))
            .SetScope(InfoBarScope::kGlobal)
            .SetExpireOnNavigation(false)
            .AddCancelButton(
                l10n_util::GetStringUTF16(IDS_APP_CANCEL),
                base::BindRepeating([](content::WebContents*) {
                  extensions::ExtensionDevToolsInfoBarController::
                      OnInfoBarAction();
                }))
            .SetDismissAction(base::BindRepeating([](content::WebContents*) {
              extensions::ExtensionDevToolsInfoBarController::
                  OnInfoBarAction();
            }))
            .Build();
    browser_infobar_manager->Register(std::move(spec));
  }

  if (IsInfoBarMigrated(InfoBarDelegate::INSTALLATION_ERROR_INFOBAR_DELEGATE)) {
    auto spec =
        InfoBarSpec::Builder(
            InfoBarDelegate::INSTALLATION_ERROR_INFOBAR_DELEGATE)
            .SetScope(InfoBarScope::kTab)
            .SetLinkNavigationUrl(GURL(
                "https://support.google.com/chrome_webstore/?p=crx_warning"))
            .AddOkButton(std::u16string(), base::DoNothing())
            .Build();
    browser_infobar_manager->Register(std::move(spec));
  }
#endif

#if BUILDFLAG(IS_WIN) || BUILDFLAG(IS_MAC) || BUILDFLAG(IS_LINUX)
  if (IsInfoBarMigrated(InfoBarDelegate::DEFAULT_BROWSER_INFOBAR_DELEGATE)) {
    auto spec =
        InfoBarSpec::Builder(InfoBarDelegate::DEFAULT_BROWSER_INFOBAR_DELEGATE)
            // The pin-to-taskbar variant of the text and the result callback
            // come in per show via InfoBarShowParams.
            .SetMessageText(
                l10n_util::GetStringUTF16(IDS_DEFAULT_BROWSER_INFOBAR_TEXT))
            .SetIcon(vector_icons::kProductRefreshIcon)
            .SetDarkModeIcon(features::IsRoundedIconsEnabled()
                                 ? omnibox::kChromeProductIcon
                                 : omnibox::kProductChromeRefreshOldIcon)
            .SetScope(InfoBarScope::kGlobal)
            .SetExpireOnNavigation(false)
            .SetShouldHideInFullscreen(true)
            .SetShouldAnimate(false)
            .SetBrowserFilter(
                base::BindRepeating([](BrowserWindowInterface* browser) {
                  const Profile* profile = browser->GetProfile();
                  return browser->GetType() ==
                             BrowserWindowInterface::TYPE_NORMAL &&
                         !profile->IsIncognitoProfile() &&
                         !profile->IsGuestSession();
                }))
            .AddOkButton(l10n_util::GetStringUTF16(
                             IDS_DEFAULT_BROWSER_INFOBAR_OK_BUTTON_LABEL),
                         base::DoNothing())
            .Build();
    browser_infobar_manager->Register(std::move(spec));
  }

  if (IsInfoBarMigrated(InfoBarDelegate::SESSION_RESTORE_INFOBAR_DELEGATE)) {
    auto spec =
        InfoBarSpec::Builder(InfoBarDelegate::SESSION_RESTORE_INFOBAR_DELEGATE)
            .SetMessageTextTemplate(u"$1")
            .SetSubstitutionsCallback(base::BindRepeating(
                [](content::WebContents*) {
                  return session_restore_infobar::
                      SessionRestoreInfoBarManager::GetInstance()
                          ->GetMessageSubstitutions();
                }))
            .SetLinkText(l10n_util::GetStringUTF16(IDS_SESSION_RESTORE_LINK))
            .SetLinkNavigationUrl(GURL("chrome://settings/onStartup"))
            .SetIcon(vector_icons::kProductRefreshIcon)
            .SetDarkModeIcon(features::IsRoundedIconsEnabled()
                                 ? omnibox::kChromeProductIcon
                                 : omnibox::kProductChromeRefreshOldIcon)
            .SetScope(InfoBarScope::kGlobal)
            .SetExpireOnNavigation(false)
            .SetBrowserFilter(base::BindRepeating(
                [](BrowserWindowInterface* browser) {
                  return session_restore_infobar::
                      SessionRestoreInfoBarManager::GetInstance()
                          ->ShouldTrackBrowser(browser);
                }))
            .SetResultCallback(base::BindRepeating(
                [](content::WebContents*, InfoBarResult result) {
                  session_restore_infobar::
                      SessionRestoreInfoBarManager::GetInstance()
                          ->OnInfoBarResult(result);
                }))
            .Build();
    browser_infobar_manager->Register(std::move(spec));
  }
#endif

  if (IsInfoBarMigrated(InfoBarDelegate::DEV_TOOLS_INFOBAR_DELEGATE)) {
    auto spec =
        InfoBarSpec::Builder(InfoBarDelegate::DEV_TOOLS_INFOBAR_DELEGATE)
            // The message and the decision callback come in per show via
            // InfoBarShowParams.
            .AddOkButton(
                l10n_util::GetStringUTF16(IDS_DEV_TOOLS_CONFIRM_ALLOW_BUTTON),
                base::DoNothing())
            .AddCancelButton(
                l10n_util::GetStringUTF16(IDS_DEV_TOOLS_CONFIRM_DENY_BUTTON),
                base::DoNothing())
            .SetScope(InfoBarScope::kGlobal)
            .Build();
    browser_infobar_manager->Register(std::move(spec));
  }

  if (IsInfoBarMigrated(
          InfoBarDelegate::OSCRYPTASYNC_AVAILABILITY_INFOBAR_DELEGATE)) {
    auto spec =
        InfoBarSpec::Builder(
            InfoBarDelegate::OSCRYPTASYNC_AVAILABILITY_INFOBAR_DELEGATE)
            .SetMessageText(l10n_util::GetStringUTF16(
                IDS_OSCRYPTASYNC_AVAILABILITY_INFOBAR_MESSAGE))
            .SetIcon(features::IsRoundedIconsEnabled()
                         ? vector_icons::kErrorFilledIcon
                         : vector_icons::kErrorOldIcon)
            .SetScope(InfoBarScope::kTab)
            .SetPriority(InfoBarDelegate::InfobarPriority::kCriticalSecurity)
            .SetExpireOnNavigation(false)
            // The warning holds until the relaunch actually happens: no
            // close button, and the relaunch button leaves the infobar up
            // in case the relaunch gets cancelled.
            .SetIsCloseable(false)
            .SetCloseOnAccept(false)
            .AddOkButton(l10n_util::GetStringUTF16(
                             IDS_OSCRYPTASYNC_AVAILABILITY_INFOBAR_BUTTON),
                         base::BindRepeating([](content::WebContents*) {
                           chrome::AttemptRelaunch();
                         }))
            .Build();
    browser_infobar_manager->Register(std::move(spec));
  }
}

void RegisterPreProfileInitInfoBars() {
  auto* browser_infobar_manager =
      BrowserInfoBarManager::From(g_browser_process);
  if (!browser_infobar_manager) {
    return;
  }

#if BUILDFLAG(CHROME_FOR_TESTING)
  if (IsInfoBarMigrated(InfoBarDelegate::CHROME_FOR_TESTING_INFOBAR_DELEGATE)) {
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
#endif

  if (IsInfoBarMigrated(InfoBarDelegate::AUTOMATION_INFOBAR_DELEGATE)) {
    auto spec =
        InfoBarSpec::Builder(InfoBarDelegate::AUTOMATION_INFOBAR_DELEGATE)
            .SetMessageText(
                l10n_util::GetStringUTF16(IDS_CONTROLLED_BY_AUTOMATION))
            .SetScope(InfoBarScope::kGlobal)
            .SetPriority(InfoBarDelegate::InfobarPriority::kCriticalSecurity)
            .SetExpireOnNavigation(false)
            .SetShouldAnimate(false)
            .Build();
    browser_infobar_manager->Register(std::move(spec));
  }

#if !BUILDFLAG(IS_ANDROID)
  if (IsInfoBarMigrated(InfoBarDelegate::BAD_FLAGS_INFOBAR_DELEGATE)) {
    auto spec =
        InfoBarSpec::Builder(InfoBarDelegate::BAD_FLAGS_INFOBAR_DELEGATE)
            .SetScope(InfoBarScope::kGlobal)
            .SetExpireOnNavigation(false)
            .SetShouldAnimate(false)
            .Build();
    browser_infobar_manager->Register(std::move(spec));
  }
#endif
}

}  // namespace infobars
