// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/webui/infobar_internals/infobar_internals_handler.h"

#include <memory>
#include <string>
#include <utility>
#include <vector>

#include "base/functional/bind.h"
#include "base/functional/callback_helpers.h"
#include "base/no_destructor.h"
#include "base/strings/stringprintf.h"
#include "base/strings/utf_string_conversions.h"
#include "build/branding_buildflags.h"
#include "chrome/browser/browser_process.h"
#include "chrome/browser/devtools/devtools_infobar_delegate.h"
#include "chrome/browser/devtools/devtools_window.h"
#include "chrome/browser/global_features.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/ui/browser_window/public/browser_window_interface.h"
#include "chrome/browser/ui/browser_window/public/browser_window_interface_iterator.h"
#include "chrome/common/pref_names.h"
#include "chrome/grit/generated_resources.h"
#include "components/infobars/content/content_infobar_manager.h"
#include "components/prefs/pref_service.h"
#include "components/tabs/public/tab_interface.h"
#include "content/public/browser/web_ui.h"
#include "extensions/buildflags/buildflags.h"
#include "mojo/public/cpp/bindings/receiver.h"
#include "ui/base/l10n/l10n_util.h"
#include "url/gurl.h"

#if BUILDFLAG(ENABLE_EXTENSIONS)
#include "chrome/browser/extensions/api/debugger/extension_dev_tools_infobar_delegate.h"
#include "chrome/browser/extensions/api/messaging/incognito_connectability.h"
#include "chrome/browser/extensions/api/messaging/incognito_connectability_infobar_delegate.h"
#include "chrome/browser/extensions/theme_installed_infobar_delegate.h"
#include "chrome/browser/themes/theme_service.h"
#include "chrome/browser/themes/theme_service_factory.h"
#include "extensions/browser/extension_registry.h"
#include "extensions/common/extension.h"
#endif

#if BUILDFLAG(IS_WIN) && BUILDFLAG(GOOGLE_CHROME_BRANDING)
#include "chrome/browser/win/installer_downloader/installer_downloader_controller.h"
#include "chrome/browser/win/installer_downloader/installer_downloader_pref_names.h"
#endif

#if BUILDFLAG(IS_WIN) || BUILDFLAG(IS_MAC) || BUILDFLAG(IS_LINUX)
#include "chrome/browser/ui/startup/default_browser_prompt/default_browser_prompt_manager.h"  // nogncheck
#include "chrome/browser/ui/startup/default_browser_prompt/default_browser_prompt_prefs.h"  // nogncheck
#include "chrome/browser/ui/views/session_restore_infobar/session_restore_infobar_delegate.h"
#include "chrome/browser/ui/views/session_restore_infobar/session_restore_infobar_manager.h"
#endif

#if BUILDFLAG(IS_WIN)
#include "chrome/browser/startup/startup_launch_manager.h"  // nogncheck
#include "chrome/browser/ui/startup/startup_launch_infobar_manager_impl.h"
#endif

using InfoBarType = infobar_internals::mojom::InfoBarType;
using InfoBarEntry = infobar_internals::mojom::InfoBarEntry;
using InfoBarEntryPtr = infobar_internals::mojom::InfoBarEntryPtr;

InfoBarInternalsHandler::InfoBarInternalsHandler(
    mojo::PendingReceiver<infobar_internals::mojom::PageHandler> receiver)
    : receiver_(this, std::move(receiver)) {}

InfoBarInternalsHandler::~InfoBarInternalsHandler() = default;

void InfoBarInternalsHandler::TriggerInfoBar(InfoBarType type,
                                             TriggerInfoBarCallback callback) {
  std::move(callback).Run(TriggerInfoBarInternal(type));
}

void InfoBarInternalsHandler::GetInfoBars(GetInfoBarsCallback callback) {
  // Please keep the entries in alphabetized order base on the type.
  std::vector<InfoBarEntryPtr> infobar_list;
#if BUILDFLAG(IS_WIN) || BUILDFLAG(IS_MAC) || BUILDFLAG(IS_LINUX)
  infobar_list.emplace_back(InfoBarEntry::New(
      /*type=*/InfoBarType::kDefaultBrowser, /*name=*/"Default Browser",
      /*description=*/
      "The Default Browser infobar asks the user if they want to set "
      "Chrome as their default browser. This trigger resets any browser "
      "state can prevents the infobar to shown, then shows the infobar. "
      "This can only be triggered on non-ChromeOS Desktop platforms."));
  infobar_list.emplace_back(InfoBarEntry::New(
      /*type=*/InfoBarType::kSessionRestore, /*name=*/"Session Restore",
      /*description=*/
      "Triggers the session restore infobar. This infobar can only be "
      "triggered on Mac, Windows and Linux."));
#endif

  infobar_list.emplace_back(InfoBarEntry::New(
      /*type=*/InfoBarType::kDevTools, /*name=*/"DevTools",
      /*description=*/
      "The DevTools infobar is used to confirm that the user wants to "
      "allow DevTools to be used. This trigger shows the infobar."));

  infobar_list.emplace_back(InfoBarEntry::New(
      /*type=*/InfoBarType::kExtensionDevTools, /*name=*/"Extension DevTools",
      /*description=*/
      "The Extension DevTools infobar is used to globally warn users "
      "that an extension is debugging the browser. This trigger shows "
      "the infobar."));

#if BUILDFLAG(ENABLE_EXTENSIONS)
  infobar_list.emplace_back(InfoBarEntry::New(
      /*type=*/InfoBarType::kIncognitoConnectability,
      /*name=*/"Incognito Connectability",
      /*description=*/
      "The Incognito Connectability infobar is used to ask the user if they "
      "want to allow an extension to communicate with a website in "
      "incognito mode. This trigger shows the infobar."));
#endif

#if BUILDFLAG(IS_WIN) && BUILDFLAG(GOOGLE_CHROME_BRANDING)
  infobar_list.emplace_back(InfoBarEntry::New(
      /*type=*/InfoBarType::kInstallerDownloader,
      /*name=*/"Installer Downloader",
      /*description=*/
      "The Installer Downloader can only be triggered on Windows. The "
      "manual trigger consist to reset any browser state that can "
      "prevent it to shown and then trigger a show request."));
#endif

#if BUILDFLAG(IS_WIN)
  infobar_list.emplace_back(InfoBarEntry::New(
      /*type=*/InfoBarType::kStartupLaunch, /*name=*/"Startup Launch",
      /*description=*/
      "Triggers the startup launch infobar. This infobar can only be "
      "triggered on Windows, and only when LaunchOnStartup feature flag is "
      "enabled."));
#endif

#if BUILDFLAG(ENABLE_EXTENSIONS)
  infobar_list.emplace_back(InfoBarEntry::New(
      /*type=*/InfoBarType::kThemeInstalled, /*name=*/"Theme Installed",
      /*description=*/
      "The Theme Installed infobar is shown when a user installs a theme. "
      "This trigger shows the infobar for the current theme, allowing you "
      "to 'undo' to the state before this trigger."));
#endif

  std::move(callback).Run(std::move(infobar_list));
}

bool InfoBarInternalsHandler::TriggerInfoBarInternal(InfoBarType type) {
  // Please keep the entries in alphabetized order base on the type.
  switch (type) {
#if BUILDFLAG(IS_WIN) || BUILDFLAG(IS_MAC) || BUILDFLAG(IS_LINUX)
    case InfoBarType::kDefaultBrowser: {
      BrowserWindowInterface* const bwi =
          GetLastActiveBrowserWindowInterfaceWithAnyProfile();
      Profile* profile = bwi->GetProfile();

      if (!profile) {
        return false;
      }

      chrome::startup::default_prompt::ResetPromptPrefs(profile);
      DefaultBrowserPromptManager::GetInstance()->MaybeShowPrompt();
      return true;
    }
    case InfoBarType::kSessionRestore: {
      BrowserWindowInterface* const bwi =
          GetLastActiveBrowserWindowInterfaceWithAnyProfile();
      Profile* profile = bwi->GetProfile();

      if (!profile) {
        return false;
      }
      session_restore_infobar::SessionRestoreInfoBarManager::GetInstance()
          ->ShowInfoBar(*profile,
                        session_restore_infobar::SessionRestoreInfoBarDelegate::
                            InfobarMessageType::kTurnOffFromRestart);
      return true;
    }
#endif
    case InfoBarType::kDevTools: {
      BrowserWindowInterface* const bwi =
          GetLastActiveBrowserWindowInterfaceWithAnyProfile();
      if (!bwi || !bwi->GetActiveTabInterface()) {
        return false;
      }
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
              bwi->GetActiveTabInterface()->GetContents()));
      return true;
    }
    case InfoBarType::kExtensionDevTools: {
#if BUILDFLAG(ENABLE_EXTENSIONS)
      BrowserWindowInterface* const bwi =
          GetLastActiveBrowserWindowInterfaceWithAnyProfile();
      Profile* profile = bwi->GetProfile();
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
    case InfoBarType::kIncognitoConnectability: {
#if BUILDFLAG(ENABLE_EXTENSIONS)
      BrowserWindowInterface* const bwi =
          GetLastActiveBrowserWindowInterfaceWithAnyProfile();
      Profile* profile = bwi->GetProfile();
      if (!profile || !bwi->GetActiveTabInterface()) {
        return false;
      }

      content::WebContents* web_contents =
          bwi->GetActiveTabInterface()->GetContents();

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
      BrowserWindowInterface* const bwi =
          GetLastActiveBrowserWindowInterfaceWithAnyProfile();
      Profile* profile = bwi->GetProfile();
      if (!profile) {
        return false;
      }

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

      ThemeInstalledInfoBarDelegate::CreateForLastActiveTab(
          profile, theme_name, theme_id,
          theme_service->BuildReinstallerForCurrentTheme());
      return true;
    }
#endif
  }

  return false;
}
