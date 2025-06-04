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
#include "components/prefs/pref_service.h"
#include "content/public/browser/web_ui.h"
#include "mojo/public/cpp/bindings/receiver.h"

#if BUILDFLAG(IS_WIN) && BUILDFLAG(GOOGLE_CHROME_BRANDING)
#include "chrome/browser/win/installer_downloader/installer_downloader_controller.h"
#include "chrome/browser/win/installer_downloader/installer_downloader_pref_names.h"
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
  static const base::NoDestructor<std::array<InfoBarEntry, 1>> kInfobars(
      {InfoBarEntry{InfoBarType::kInstallerDownloader,
                    "Installer Downloader"}});

  std::vector<infobar_internals::mojom::InfoBarEntryPtr> infobar_list;
  for (const auto& infobar : *kInfobars) {
    infobar_list.emplace_back(InfoBarEntry::New(infobar.type, infobar.name));
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
  }

  return false;
}
