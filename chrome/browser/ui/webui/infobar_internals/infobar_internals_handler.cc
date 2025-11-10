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
#include "components/prefs/pref_service.h"
#include "content/public/browser/web_ui.h"
#include "mojo/public/cpp/bindings/receiver.h"

#if BUILDFLAG(IS_WIN) && BUILDFLAG(GOOGLE_CHROME_BRANDING)
#include "chrome/browser/win/installer_downloader/installer_downloader_controller.h"
#include "chrome/browser/win/installer_downloader/installer_downloader_pref_names.h"
#endif

#if BUILDFLAG(IS_WIN) || BUILDFLAG(IS_MAC) || BUILDFLAG(IS_LINUX)
#include "chrome/browser/ui/views/session_restore_infobar/session_restore_infobar_delegate.h"
#include "chrome/browser/ui/views/session_restore_infobar/session_restore_infobar_manager.h"
#endif

using InfoBarType = infobar_internals::mojom::InfoBarType;
using InfoBarEntry = infobar_internals::mojom::InfoBarEntry;

InfoBarInternalsHandler::InfoBarInternalsHandler(
    mojo::PendingReceiver<infobar_internals::mojom::PageHandler> receiver)
    : receiver_(this, std::move(receiver)) {}

InfoBarInternalsHandler::~InfoBarInternalsHandler() = default;

void InfoBarInternalsHandler::TriggerInfoBar(InfoBarType type,
                                             TriggerInfoBarCallback callback) {
  std::move(callback).Run(TriggerInfoBarInternal(type));
}

void InfoBarInternalsHandler::GetInfoBars(GetInfoBarsCallback callback) {
  static const base::NoDestructor<std::array<InfoBarEntry, 2>> kInfobars(
      {InfoBarEntry{
           /*type=*/InfoBarType::kInstallerDownloader, "Installer Downloader",
           "The Installer Downloader can only be triggered on Windows. The "
           "manual trigger consist to reset any browser state that can prevent "
           "it to shown and then trigger a show request."},
       InfoBarEntry{
           /*type=*/InfoBarType::kSessionRestore, "Session Restore",
           "Triggers the session restore infobar. This infobar can only be "
           "triggered on Mac, Windows and Linux."}});

  std::vector<infobar_internals::mojom::InfoBarEntryPtr> infobar_list;
  for (const auto& infobar : *kInfobars) {
    infobar_list.emplace_back(
        InfoBarEntry::New(infobar.type, infobar.name, infobar.description));
  }

  std::move(callback).Run(std::move(infobar_list));
}

bool InfoBarInternalsHandler::TriggerInfoBarInternal(InfoBarType type) {
  switch (type) {
    case InfoBarType::kInstallerDownloader: {
#if BUILDFLAG(IS_WIN) && BUILDFLAG(GOOGLE_CHROME_BRANDING)
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
#endif
      return false;
    }
    case InfoBarType::kSessionRestore: {
#if BUILDFLAG(IS_WIN) || BUILDFLAG(IS_MAC) || BUILDFLAG(IS_LINUX)
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
#endif
    }
  }

  return false;
}
