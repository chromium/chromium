// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/webui/infobar_internals/infobar_internals_handler.h"

#include <utility>
#include <vector>

#include "base/functional/bind.h"
#include "base/no_destructor.h"
#include "build/branding_buildflags.h"
#include "chrome/browser/browser_process.h"
#include "chrome/browser/global_features.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/ui/browser_window/public/browser_window_interface.h"
#include "chrome/browser/ui/browser_window/public/browser_window_interface_iterator.h"
#include "chrome/common/pref_names.h"
#include "components/prefs/pref_service.h"
#include "content/public/browser/web_ui.h"
#include "mojo/public/cpp/bindings/receiver.h"

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

#if BUILDFLAG(IS_WIN) && BUILDFLAG(GOOGLE_CHROME_BRANDING)
  infobar_list.emplace_back(InfoBarEntry::New(
      /*type=*/InfoBarType::kInstallerDownloader,
      /*name=*/"Installer Downloader",
      /*description=*/
      "The Installer Downloader can only be triggered on Windows. The "
      "manual trigger consist to reset any browser state that can "
      "prevent it to shown and then trigger a show request."));
#endif

  std::move(callback).Run(std::move(infobar_list));
}

bool InfoBarInternalsHandler::TriggerInfoBarInternal(InfoBarType type) {
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
  }

  return false;
}
