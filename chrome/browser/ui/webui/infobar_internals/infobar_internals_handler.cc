// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/webui/infobar_internals/infobar_internals_handler.h"

#include <memory>
#include <ranges>
#include <string>
#include <utility>
#include <vector>

#include "base/functional/bind.h"
#include "base/functional/callback_helpers.h"
#include "base/no_destructor.h"
#include "base/strings/stringprintf.h"
#include "base/strings/utf_string_conversions.h"
#include "build/branding_buildflags.h"
#include "build/buildflag.h"
#include "chrome/browser/browser_process.h"
#include "chrome/browser/buildflags.h"
#include "chrome/browser/devtools/devtools_infobar_delegate.h"
#include "chrome/browser/devtools/devtools_window.h"
#include "chrome/browser/devtools/global_confirm_info_bar.h"
#include "chrome/browser/devtools/process_sharing_infobar.h"
#include "chrome/browser/devtools/process_sharing_infobar_delegate.h"
#include "chrome/browser/global_features.h"
#include "chrome/browser/infobars/browser_infobar_manager.h"
#include "chrome/browser/infobars/confirm_infobar_creator.h"
#include "chrome/browser/infobars/infobar_features.h"
#include "chrome/browser/infobars/simple_alert_infobar_creator.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/ssl/known_interception_disclosure_infobar_delegate.h"
#include "chrome/browser/ui/browser_window/public/browser_window_interface.h"
#include "chrome/browser/ui/browser_window/public/browser_window_interface_iterator.h"
#include "chrome/browser/ui/browser_window/public/global_browser_collection.h"
#include "chrome/browser/ui/collected_cookies_infobar_delegate.h"
#include "chrome/browser/ui/omnibox/alternate_nav_infobar_delegate.h"
#include "chrome/browser/ui/page_info/page_info_infobar_delegate.h"
#include "chrome/browser/ui/startup/google_api_keys_infobar_delegate.h"
#include "chrome/browser/ui/startup/obsolete_system_infobar_delegate.h"
#include "chrome/browser/ui/tabs/tab_strip_model.h"
#include "chrome/browser/ui/ui_features.h"
#include "chrome/browser/ui/views/site_data/page_specific_site_data_dialog_controller.h"
#include "chrome/common/pref_names.h"
#include "chrome/grit/generated_resources.h"
#include "components/infobars/content/content_infobar_manager.h"
#include "components/infobars/core/infobar.h"
#include "components/infobars/core/infobar_manager.h"
#include "components/infobars/core/simple_alert_infobar_delegate.h"
#include "components/omnibox/browser/autocomplete_match.h"
#include "components/prefs/pref_service.h"
#include "components/strings/grit/components_strings.h"
#include "components/tabs/public/tab_interface.h"
#include "content/public/browser/navigation_controller.h"
#include "content/public/browser/web_ui.h"
#include "content/public/common/buildflags.h"
#include "extensions/buildflags/buildflags.h"
#include "mojo/public/cpp/bindings/receiver.h"
#include "ui/base/l10n/l10n_util.h"
#include "url/gurl.h"

#if BUILDFLAG(CHROME_FOR_TESTING)
#include "chrome/browser/ui/startup/chrome_for_testing_infobar_delegate.h"
#endif

#if BUILDFLAG(ENABLE_PLUGINS)
#include "chrome/browser/plugins/reload_plugin_infobar_delegate.h"
#endif

#if BUILDFLAG(ENABLE_EXTENSIONS)
#include "chrome/browser/extensions/api/debugger/extension_dev_tools_infobar_delegate.h"
#include "chrome/browser/extensions/api/messaging/incognito_connectability.h"
#include "chrome/browser/extensions/api/messaging/incognito_connectability_infobar_delegate.h"
#include "chrome/browser/extensions/theme_installed_infobar_delegate.h"
#include "chrome/browser/themes/theme_service.h"
#include "chrome/browser/themes/theme_service_factory.h"
#include "chrome/browser/ui/extensions/installation_error_infobar_delegate.h"
#include "extensions/browser/extension_registry.h"
#include "extensions/browser/install/crx_install_error.h"
#include "extensions/common/extension.h"
#include "extensions/strings/grit/extensions_strings.h"
#endif

#if BUILDFLAG(IS_WIN) && BUILDFLAG(GOOGLE_CHROME_BRANDING)
#include "chrome/browser/win/installer_downloader/installer_downloader_controller.h"
#include "chrome/browser/win/installer_downloader/installer_downloader_pref_names.h"
#endif

#if BUILDFLAG(IS_MAC) && BUILDFLAG(ENABLE_UPDATER)
#include "chrome/browser/ui/cocoa/keystone_infobar_delegate.h"
#endif

#if BUILDFLAG(IS_WIN) || BUILDFLAG(IS_MAC) || BUILDFLAG(IS_LINUX)
#include "chrome/browser/ui/startup/default_browser_prompt/default_browser_prompt_manager.h"  // nogncheck
#include "chrome/browser/ui/startup/default_browser_prompt/default_browser_prompt_prefs.h"  // nogncheck
#include "chrome/browser/ui/views/session_restore_infobar/session_restore_infobar_manager.h"
#include "chrome/browser/web_applications/web_app_provider.h"
#include "chrome/browser/web_applications/web_app_registrar.h"
#endif

#if BUILDFLAG(IS_WIN) || BUILDFLAG(IS_MAC)
#include "chrome/browser/ui/pdf/infobar/pdf_infobar_controller.h"
#endif

#if BUILDFLAG(IS_WIN)
#include "chrome/browser/startup/startup_launch_manager.h"  // nogncheck
#include "chrome/browser/ui/startup/startup_launch_infobar_manager_impl.h"
#endif

using InfoBarType = infobar_internals::mojom::InfoBarType;
using InfoBarEntry = infobar_internals::mojom::InfoBarEntry;
using InfoBarEntryPtr = infobar_internals::mojom::InfoBarEntryPtr;

namespace {

// What a trigger needs before its case runs. Deliberately exhaustive: a new
// InfoBarType does not compile until it declares its preconditions here.
struct TriggerRequirements {
  bool profile = false;
  bool web_contents = false;
};

TriggerRequirements RequirementsFor(InfoBarType type) {
  switch (type) {
    case InfoBarType::kAlternateNav:
    case InfoBarType::kCollectedCookies:
    case InfoBarType::kDevTools:
    case InfoBarType::kDevToolsSharedProcess:
    case InfoBarType::kGoogleApiKeys:
    case InfoBarType::kKnownInterception:
    case InfoBarType::kObsoleteSystem:
    case InfoBarType::kPageInfo:
#if BUILDFLAG(ENABLE_PLUGINS)
    case InfoBarType::kReloadPlugin:
#endif
#if BUILDFLAG(IS_WIN) || BUILDFLAG(IS_MAC)
    case InfoBarType::kPdf:
#endif
      return {.web_contents = true};
#if BUILDFLAG(ENABLE_EXTENSIONS)
    case InfoBarType::kIncognitoConnectability:
    case InfoBarType::kInstallationError:
      return {.profile = true, .web_contents = true};
#endif
    case InfoBarType::kExtensionDevTools:
#if BUILDFLAG(IS_WIN) || BUILDFLAG(IS_MAC) || BUILDFLAG(IS_LINUX)
    case InfoBarType::kDefaultBrowser:
    case InfoBarType::kEnableLinkCapturing:
    case InfoBarType::kSessionRestore:
#endif
#if BUILDFLAG(IS_MAC)
    case InfoBarType::kKeystone:
#endif
#if BUILDFLAG(ENABLE_EXTENSIONS)
    case InfoBarType::kThemeInstalled:
#endif
      return {.profile = true};
#if BUILDFLAG(CHROME_FOR_TESTING)
    case InfoBarType::kChromeForTesting:
#endif
    case InfoBarType::kLocalTestPoliciesApplied:
#if BUILDFLAG(IS_WIN) && BUILDFLAG(GOOGLE_CHROME_BRANDING)
    case InfoBarType::kInstallerDownloader:
#endif
#if BUILDFLAG(IS_WIN)
    case InfoBarType::kStartupLaunch:
#endif
      return {};
  }
}

}  // namespace

InfoBarInternalsHandler::InfoBarInternalsHandler(
    mojo::PendingReceiver<infobar_internals::mojom::PageHandler> receiver)
    : receiver_(this, std::move(receiver)) {}

InfoBarInternalsHandler::~InfoBarInternalsHandler() = default;

void InfoBarInternalsHandler::TriggerInfoBar(InfoBarType type,
                                             TriggerInfoBarCallback callback) {
  std::move(callback).Run(TriggerInfoBarInternal(type));
}

void InfoBarInternalsHandler::GetInfoBars(GetInfoBarsCallback callback) {
  // Please keep the entries in alphabetical order, based on the type.
  std::vector<InfoBarEntryPtr> infobar_list;
  auto add_entry = [&infobar_list](InfoBarType type, const std::string& name,
                                   const std::string& description) {
    infobar_list.emplace_back(InfoBarEntry::New(type, name, description));
  };
  if (base::FeatureList::IsEnabled(features::kInfoBarInlineLinks)) {
    add_entry(InfoBarType::kAlternateNav, "Alternate Nav",
              "The Alternate Nav infobar is shown when a user searches for a "
              "term they may have meant to navigate to.");
  }
#if BUILDFLAG(CHROME_FOR_TESTING)
  add_entry(InfoBarType::kChromeForTesting, "Chrome for Testing",
            "The Chrome for Testing infobar warns users that this version is "
            "only for automated testing.");
#endif
  add_entry(InfoBarType::kCollectedCookies, "Collected Cookies",
            "The Collected Cookies infobar is shown after the user has changed "
            "the allowed/blocked state of a cookie, reminding them to reload "
            "the page in order for the new cookies to take effect.");
#if BUILDFLAG(IS_WIN) || BUILDFLAG(IS_MAC) || BUILDFLAG(IS_LINUX)
  add_entry(InfoBarType::kDefaultBrowser, "Default Browser",
            "The Default Browser infobar asks the user if they want to set "
            "Chrome as their default browser. This trigger resets any browser "
            "state that prevents the infobar from showing, then shows it. This "
            "can only be triggered on non-ChromeOS Desktop platforms.");
#endif

  add_entry(InfoBarType::kDevTools, "DevTools",
            "The DevTools infobar is used to confirm that the user wants to "
            "allow DevTools to be used. This trigger shows the infobar.");

  add_entry(InfoBarType::kDevToolsSharedProcess, "DevTools Shared Process",
            "The DevTools shared process infobar warns that the inspected tab "
            "shares a renderer process and offers a restart with "
            "process-per-site disabled. This trigger shows the infobar on the "
            "active tab.");

#if BUILDFLAG(IS_WIN) || BUILDFLAG(IS_MAC) || BUILDFLAG(IS_LINUX)
  add_entry(InfoBarType::kEnableLinkCapturing, "Enable Link Capturing",
            "The Enable Link Capturing infobar asks the user if they want to "
            "open supported links in an installed web app. This trigger "
            "shows the infobar.");
#endif

  add_entry(InfoBarType::kExtensionDevTools, "Extension DevTools",
            "The Extension DevTools infobar is used to globally warn users "
            "that an extension is debugging the browser. This trigger shows "
            "the infobar.");

  add_entry(InfoBarType::kGoogleApiKeys, "Google API Keys",
            "The Google API Keys infobar warns users when Google API keys are "
            "missing. This trigger shows the infobar.");

#if BUILDFLAG(ENABLE_EXTENSIONS)
  add_entry(InfoBarType::kIncognitoConnectability, "Incognito Connectability",
            "The Incognito Connectability infobar is used to ask the user if "
            "they want to allow an extension to communicate with a website in "
            "incognito mode. This trigger shows the infobar.");
  add_entry(InfoBarType::kInstallationError, "Installation Error",
            "The Installation Error infobar is shown when an extension "
            "installation fails.");
#endif
#if BUILDFLAG(IS_WIN) && BUILDFLAG(GOOGLE_CHROME_BRANDING)
  add_entry(InfoBarType::kInstallerDownloader, "Installer Downloader",
            "The Installer Downloader can only be triggered on Windows. This "
            "trigger resets any browser state that prevents it from showing, "
            "then requests a show.");
#endif

#if BUILDFLAG(IS_MAC) && BUILDFLAG(ENABLE_UPDATER)
  add_entry(InfoBarType::kKeystone, "Keystone",
            "The Keystone infobar asks the user to promote the updater to "
            "system scope. This trigger resets any browser state that prevents "
            "the infobar from being shown, then shows the infobar. This can "
            "only be triggered on Mac.");
#endif

  add_entry(InfoBarType::kKnownInterception, "Known Interception Disclosure",
            "The Known Interception Disclosure infobar alerts users when "
            "network interception or monitoring is detected. This trigger "
            "shows the infobar.");

  add_entry(InfoBarType::kLocalTestPoliciesApplied,
            "Local Test Policies Applied",
            "The Local Test Policies Applied infobar warns the user that local "
            "test policies are active.");

  add_entry(InfoBarType::kObsoleteSystem, "Obsolete System",
            "The Obsolete System infobar warns users when their operating "
            "system is no longer supported. This trigger shows the infobar.");

  add_entry(InfoBarType::kPageInfo, "Page Info",
            "The Page Info infobar is shown when a user changes permissions, "
            "asking them to reload the page to apply settings.");

#if BUILDFLAG(IS_WIN) || BUILDFLAG(IS_MAC)
  add_entry(InfoBarType::kPdf, "PDF",
            "The PDF infobar offers to set Chrome as the default PDF viewer if "
            "it's not already. This trigger resets any browser state that "
            "prevents the infobar from being shown, then shows the infobar. "
            "This can only be triggered on Windows or Mac.");
#endif

#if BUILDFLAG(ENABLE_PLUGINS)
  add_entry(InfoBarType::kReloadPlugin, "Reload Plugin",
            "The Reload Plugin infobar is used to ask the user to reload a "
            "page when a plugin has crashed or disconnected. This trigger "
            "shows the infobar.");
#endif

#if BUILDFLAG(IS_WIN) || BUILDFLAG(IS_MAC) || BUILDFLAG(IS_LINUX)
  add_entry(InfoBarType::kSessionRestore, "Session Restore",
            "Triggers the session restore infobar. This infobar can only be "
            "triggered on Mac, Windows and Linux.");
#endif

#if BUILDFLAG(IS_WIN)
  add_entry(InfoBarType::kStartupLaunch, "Startup Launch",
            "Triggers the startup launch infobar. This infobar can only be "
            "triggered on Windows, and only when LaunchOnStartup feature flag "
            "is enabled.");
#endif

#if BUILDFLAG(ENABLE_EXTENSIONS)
  add_entry(InfoBarType::kThemeInstalled, "Theme Installed",
            "The Theme Installed infobar is shown when a user installs a "
            "theme. This trigger shows the infobar for the current theme, "
            "allowing you to 'undo' to the state before this trigger.");
#endif

  std::move(callback).Run(std::move(infobar_list));
}

bool InfoBarInternalsHandler::TriggerInfoBarInternal(InfoBarType type) {
  BrowserWindowInterface* const bwi =
      GetLastActiveBrowserWindowInterfaceWithAnyProfile();
  Profile* const profile = bwi ? bwi->GetProfile() : nullptr;
  tabs::TabInterface* const active_tab =
      bwi ? bwi->GetActiveTabInterface() : nullptr;
  content::WebContents* const web_contents =
      active_tab ? active_tab->GetContents() : nullptr;
  const TriggerRequirements needs = RequirementsFor(type);
  if ((needs.profile && !profile) || (needs.web_contents && !web_contents)) {
    return false;
  }
  auto* const browser_infobar_manager =
      infobars::BrowserInfoBarManager::From(g_browser_process);

  // Please keep the entries in alphabetical order, based on the type.
  switch (type) {
    case InfoBarType::kAlternateNav: {
      AutocompleteMatch match;
      match.destination_url = GURL("https://google.com/");

      AlternateNavInfoBarDelegate::CreateForOmniboxNavigation(
          web_contents, u"test", match, GURL("https://youtube.com/"));
      return true;
    }
#if BUILDFLAG(CHROME_FOR_TESTING)
    case InfoBarType::kChromeForTesting: {
      if (infobars::IsInfoBarMigrated(
              infobars::InfoBarDelegate::CHROME_FOR_TESTING_INFOBAR_DELEGATE)) {
        if (!browser_infobar_manager) {
          return false;
        }
        browser_infobar_manager->ShowGlobally(
            infobars::InfoBarDelegate::CHROME_FOR_TESTING_INFOBAR_DELEGATE);
      } else {
        ChromeForTestingInfoBarDelegate::Create();
      }
      return true;
    }
#endif
    case InfoBarType::kCollectedCookies: {
      if (infobars::IsInfoBarMigrated(
              infobars::InfoBarDelegate::COLLECTED_COOKIES_INFOBAR_DELEGATE)) {
        PageSpecificSiteDataDialogController::ShowCollectedCookiesInfoBar(
            web_contents);
      } else {
        infobars::ContentInfoBarManager* infobar_manager =
            infobars::ContentInfoBarManager::FromWebContents(web_contents);
        if (!infobar_manager) {
          return false;
        }
        CollectedCookiesInfoBarDelegate::Create(infobar_manager);
      }
      return true;
    }
#if BUILDFLAG(IS_WIN) || BUILDFLAG(IS_MAC) || BUILDFLAG(IS_LINUX)
    case InfoBarType::kDefaultBrowser: {
      chrome::startup::default_prompt::ResetPromptPrefs(profile);
      DefaultBrowserPromptManager::GetInstance()->MaybeShowPrompt();
      return true;
    }
#endif
    case InfoBarType::kDevTools: {
      DevToolsInfoBarDelegate::Create(
          l10n_util::GetStringFUTF16(IDS_DEV_TOOLS_INFOBAR_LABEL,
                                     u"Infobar Internals"),
          base::BindOnce(
              [](content::WebContents* web_contents, bool accepted) {
                if (accepted) {
                  DevToolsWindow::OpenDevToolsWindow(
                      web_contents, DevToolsOpenedByAction::kUnknown);
                }
              },
              web_contents));
      return true;
    }
    case InfoBarType::kDevToolsSharedProcess: {
      auto* infobar_manager =
          infobars::ContentInfoBarManager::FromWebContents(web_contents);
      if (!infobar_manager) {
        return false;
      }
      return infobar_manager->AddInfoBar(CreateConfirmInfoBar(
                 std::make_unique<ProcessSharingInfobarDelegate>(
                     web_contents))) != nullptr;
    }
#if BUILDFLAG(IS_WIN) || BUILDFLAG(IS_MAC) || BUILDFLAG(IS_LINUX)
    case InfoBarType::kEnableLinkCapturing: {
      if (!infobars::IsInfoBarMigrated(
              infobars::InfoBarDelegate::
                  ENABLE_LINK_CAPTURING_INFOBAR_DELEGATE) ||
          !browser_infobar_manager) {
        return false;
      }
      std::u16string app_name = u"Example App";
      if (auto* provider = web_app::WebAppProvider::GetForWebApps(profile)) {
        const auto& app_ids = provider->registrar_unsafe().GetAppIds();
        if (!app_ids.empty()) {
          app_name = base::UTF8ToUTF16(
              provider->registrar_unsafe().GetAppShortName(app_ids[0]));
        }
      }

      infobars::InfoBarShowParams params;
      params.message_text = l10n_util::GetStringFUTF16(
          IDR_INTENT_PICKER_SUPPORTED_LINKS_INFOBAR_MESSAGE, app_name);
      params.ok_button_callback = base::DoNothing();
      params.cancel_button_callback = base::DoNothing();
      return browser_infobar_manager->Show(
                 active_tab,
                 infobars::InfoBarDelegate::
                     ENABLE_LINK_CAPTURING_INFOBAR_DELEGATE,
                 std::move(params)) != nullptr;
    }
#endif
    case InfoBarType::kExtensionDevTools: {
#if BUILDFLAG(ENABLE_EXTENSIONS)
      if (infobars::IsInfoBarMigrated(
              infobars::InfoBarDelegate::
                  EXTENSION_DEV_TOOLS_INFOBAR_DELEGATE)) {
        if (!browser_infobar_manager) {
          return false;
        }
        return browser_infobar_manager->ShowGlobally(
            infobars::InfoBarDelegate::EXTENSION_DEV_TOOLS_INFOBAR_DELEGATE);
      }

      if (!profile) {
        return false;
      }

      extensions::ExtensionRegistry* registry =
          extensions::ExtensionRegistry::Get(profile);
      const extensions::ExtensionSet& extensions =
          registry->enabled_extensions();

      std::string extension_id = "dummy_extension_id";
      std::string extension_name = "Dummy Extension";

      if (!extensions.empty()) {
        const extensions::Extension* extension = extensions.begin()->get();
        extension_id = extension->id();
        extension_name = extension->name();
      }

      subscriptions_.push_back(
          extensions::ExtensionDevToolsInfoBarDelegate::Create(
              extension_id, extension_name, /*callback=*/base::DoNothing()));
      return true;
#else
      return false;
#endif
    }
    case InfoBarType::kGoogleApiKeys: {
      if (infobars::IsInfoBarMigrated(
              infobars::InfoBarDelegate::GOOGLE_API_KEYS_INFOBAR_DELEGATE)) {
        if (!browser_infobar_manager) {
          return false;
        }
        browser_infobar_manager->Show(
            bwi->GetActiveTabInterface(),
            infobars::InfoBarDelegate::GOOGLE_API_KEYS_INFOBAR_DELEGATE);
      } else {
        infobars::ContentInfoBarManager* infobar_manager =
            infobars::ContentInfoBarManager::FromWebContents(web_contents);
        if (!infobar_manager) {
          return false;
        }
        GoogleApiKeysInfoBarDelegate::Create(infobar_manager);
      }
      return true;
    }
    case InfoBarType::kIncognitoConnectability: {
#if BUILDFLAG(ENABLE_EXTENSIONS)
      extensions::ExtensionRegistry* registry =
          extensions::ExtensionRegistry::Get(profile);
      const extensions::ExtensionSet& extensions =
          registry->enabled_extensions();

      const extensions::Extension* extension = nullptr;
      if (!extensions.empty()) {
        extension = extensions.begin()->get();
      }

      if (profile->IsOffTheRecord() && extension) {
        extensions::IncognitoConnectability::Get(profile)->Query(
            extension, web_contents,
            GURL("https://infobar-internals.google.com"), base::DoNothing());
        return true;
      }

      // Fallback: If not in incognito or no extension, show a visually
      // accurate infobar using the delegate.
      infobars::ContentInfoBarManager* infobar_manager =
          infobars::ContentInfoBarManager::FromWebContents(web_contents);
      std::u16string extension_name =
          extension ? base::UTF8ToUTF16(extension->name()) : u"Dummy Extension";
      std::u16string message = l10n_util::GetStringFUTF16(
          IDS_EXTENSION_PROMPT_EXTENSION_CONNECT_FROM_INCOGNITO,
          u"Infobar Internals", extension_name);

      extensions::IncognitoConnectabilityInfoBarDelegate::Create(
          infobar_manager, message, base::DoNothing());
      return true;
#else
      return false;
#endif
    }
#if BUILDFLAG(ENABLE_EXTENSIONS)
    case InfoBarType::kInstallationError: {
      const std::u16string msg =
          l10n_util::GetStringUTF16(IDS_EXTENSION_INSTALL_DISALLOWED_ON_SITE);
      if (infobars::IsInfoBarMigrated(
              infobars::InfoBarDelegate::INSTALLATION_ERROR_INFOBAR_DELEGATE)) {
        infobars::InfoBarShowParams params;
        params.message_text = msg;
        params.link_text = l10n_util::GetStringUTF16(IDS_LEARN_MORE);
        if (!browser_infobar_manager) {
          return false;
        }
        browser_infobar_manager->Show(
            active_tab,
            infobars::InfoBarDelegate::INSTALLATION_ERROR_INFOBAR_DELEGATE,
            std::move(params));
      } else {
        infobars::ContentInfoBarManager* infobar_manager =
            infobars::ContentInfoBarManager::FromWebContents(web_contents);
        if (!infobar_manager) {
          return false;
        }
        InstallationErrorInfoBarDelegate::Create(
            infobar_manager,
            extensions::CrxInstallError(
                extensions::CrxInstallErrorType::OTHER,
                extensions::CrxInstallErrorDetail::OFFSTORE_INSTALL_DISALLOWED,
                msg));
      }
      return true;
    }
#endif
#if BUILDFLAG(IS_WIN) && BUILDFLAG(GOOGLE_CHROME_BRANDING)
    case InfoBarType::kInstallerDownloader: {
      if (auto* controller = g_browser_process->GetFeatures()
                                 ->installer_downloader_controller()) {
        PrefService* prefs = g_browser_process->local_state();

        // This manual triggering from the debug page will reset the state of
        // the installer downloader.
        prefs->SetInteger(
            installer_downloader::prefs::kInstallerDownloaderInfobarShowCount,
            0);

        // Reset the prevent future display flag.
        prefs->SetBoolean(installer_downloader::prefs::
                              kInstallerDownloaderPreventFutureDisplay,
                          false);

        // Set bypass flag to instruct to the controller to skip/ignore
        // eligibility check result since it may failed.
        prefs->SetBoolean(installer_downloader::prefs::
                              kInstallerDownloaderBypassEligibilityCheck,
                          true);

        controller->MaybeShowInfoBar();

        return true;
      }
      return false;
    }
#endif
#if BUILDFLAG(IS_MAC)
    case InfoBarType::kKeystone: {
#if BUILDFLAG(ENABLE_UPDATER)
      profile->GetPrefs()->SetBoolean(prefs::kShowUpdatePromotionInfoBar, true);
      ShowUpdaterPromotionInfoBar();
      return true;
#else
      return false;
#endif
    }
#endif
    case InfoBarType::kKnownInterception: {
      if (infobars::IsInfoBarMigrated(
              infobars::InfoBarDelegate::
                  KNOWN_INTERCEPTION_DISCLOSURE_INFOBAR_DELEGATE)) {
        if (!browser_infobar_manager) {
          return false;
        }
        browser_infobar_manager->Show(
            active_tab, infobars::InfoBarDelegate::
                            KNOWN_INTERCEPTION_DISCLOSURE_INFOBAR_DELEGATE);
      } else {
        infobars::ContentInfoBarManager* infobar_manager =
            infobars::ContentInfoBarManager::FromWebContents(web_contents);
        if (!infobar_manager) {
          return false;
        }
        auto delegate =
            std::make_unique<KnownInterceptionDisclosureInfoBarDelegate>(
                profile);
        infobar_manager->AddInfoBar(CreateConfirmInfoBar(std::move(delegate)));
      }
      return true;
    }
    case InfoBarType::kLocalTestPoliciesApplied: {
      if (infobars::IsInfoBarMigrated(
              infobars::InfoBarDelegate::LOCAL_TEST_POLICIES_APPLIED_INFOBAR)) {
        if (!browser_infobar_manager) {
          return false;
        }
        browser_infobar_manager->ShowGlobally(
            infobars::InfoBarDelegate::LOCAL_TEST_POLICIES_APPLIED_INFOBAR);
      } else {
        GlobalConfirmInfoBar::Show(std::make_unique<SimpleAlertInfoBarDelegate>(
            infobars::InfoBarDelegate::LOCAL_TEST_POLICIES_APPLIED_INFOBAR,
            /*vector_icon=*/nullptr,
            l10n_util::GetStringUTF16(IDS_LOCAL_TEST_POLICIES_ENABLED),
            /*auto_expire=*/false, /*should_animate=*/false,
            /*closeable=*/false,
            infobars::InfoBarDelegate::InfobarPriority::kLow));
      }
      return true;
    }
    case InfoBarType::kObsoleteSystem: {
      if (infobars::IsInfoBarMigrated(
              infobars::InfoBarDelegate::OBSOLETE_SYSTEM_INFOBAR_DELEGATE)) {
        if (!browser_infobar_manager) {
          return false;
        }
        browser_infobar_manager->Show(
            active_tab,
            infobars::InfoBarDelegate::OBSOLETE_SYSTEM_INFOBAR_DELEGATE);
      } else {
        infobars::ContentInfoBarManager* infobar_manager =
            infobars::ContentInfoBarManager::FromWebContents(web_contents);
        if (!infobar_manager) {
          return false;
        }
        ObsoleteSystemInfoBarDelegate::Create(infobar_manager);
      }
      return true;
    }
    case InfoBarType::kPageInfo: {
      if (infobars::IsInfoBarMigrated(
              infobars::InfoBarDelegate::PAGE_INFO_INFOBAR_DELEGATE)) {
        if (!browser_infobar_manager) {
          return false;
        }
        browser_infobar_manager->Show(
            active_tab, infobars::InfoBarDelegate::PAGE_INFO_INFOBAR_DELEGATE);
      } else {
        infobars::ContentInfoBarManager* infobar_manager =
            infobars::ContentInfoBarManager::FromWebContents(web_contents);
        if (!infobar_manager) {
          return false;
        }
        PageInfoInfoBarDelegate::Create(infobar_manager);
      }
      return true;
    }
#if BUILDFLAG(IS_WIN) || BUILDFLAG(IS_MAC)
    case InfoBarType::kPdf: {
      auto* controller = pdf::infobar::PdfInfoBarController::From(bwi);
      if (!controller) {
        return false;
      }
      // Reset rate-limiting preferences to ensure repeated triggers succeed.
      PrefService* local_state = g_browser_process->local_state();
      local_state->ClearPref(prefs::kPdfInfoBarTimesShown);
      local_state->ClearPref(prefs::kPdfInfoBarLastShown);
      pdf::infobar::PdfInfoBarController::
          SetHigherPriorityInfoBarShownForTesting(false);

      controller->MaybeShowInfoBarCallback(
          shell_integration::DefaultWebClientState::NOT_DEFAULT);
      return true;
    }
#endif
#if BUILDFLAG(ENABLE_PLUGINS)
    case InfoBarType::kReloadPlugin: {
      ReloadPluginInfoBarDelegate::Create(
          infobars::ContentInfoBarManager::FromWebContents(web_contents),
          &web_contents->GetController(),
          l10n_util::GetStringFUTF16(IDS_PLUGIN_CRASHED_PROMPT,
                                     u"Infobar Internals"));
      return true;
    }
#endif
#if BUILDFLAG(IS_WIN) || BUILDFLAG(IS_MAC) || BUILDFLAG(IS_LINUX)
    case InfoBarType::kSessionRestore: {
      session_restore_infobar::SessionRestoreInfoBarManager::GetInstance()
          ->ShowInfoBar(*profile,
                        session_restore_infobar::InfobarMessageType::
                            kTurnOffFromRestart);
      return true;
    }
#endif
#if BUILDFLAG(IS_WIN)
    case InfoBarType::kStartupLaunch: {
      PrefService* local_state = g_browser_process->local_state();
      local_state->ClearPref(prefs::kForegroundLaunchOnLogin);
      local_state->ClearPref(prefs::kStartupLaunchInfobarAccepted);
      local_state->ClearPref(prefs::kStartupLaunchInfobarDeclinedCount);
      local_state->ClearPref(prefs::kStartupLaunchInfobarLastDeclinedTime);

      if (auto* startup_launch_manager =
              StartupLaunchManager::From(g_browser_process)) {
        startup_launch_manager->SetInfoBarManager(
            std::make_unique<StartupLaunchInfoBarManagerImpl>());
        startup_launch_manager->MaybeShowInfoBars();
        return true;
      }
      return false;
    }
#endif
#if BUILDFLAG(ENABLE_EXTENSIONS)
    case InfoBarType::kThemeInstalled: {
      ThemeService* theme_service = ThemeServiceFactory::GetForProfile(profile);
      extensions::ExtensionRegistry* registry =
          extensions::ExtensionRegistry::Get(profile);

      std::string theme_name = "Default";
      std::string theme_id = "";

      if (theme_service->UsingExtensionTheme()) {
        theme_id = theme_service->GetThemeID();
        const extensions::Extension* extension = registry->GetExtensionById(
            theme_id, extensions::ExtensionRegistry::EVERYTHING);
        if (extension) {
          theme_name = extension->name();
        }
      }

      if (infobars::IsInfoBarMigrated(
              infobars::InfoBarDelegate::THEME_INSTALLED_INFOBAR_DELEGATE)) {
        ThemeService::ShowThemeInstalledInfoBar(
            profile, theme_name, theme_id,
            theme_service->BuildReinstallerForCurrentTheme());
      } else {
        ThemeInstalledInfoBarDelegate::CreateForLastActiveTab(
            profile, theme_name, theme_id,
            theme_service->BuildReinstallerForCurrentTheme());
      }
      return true;
    }
#endif
  }

  return false;
}
