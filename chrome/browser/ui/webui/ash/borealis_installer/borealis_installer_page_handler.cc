// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/webui/ash/borealis_installer/borealis_installer_page_handler.h"

#include "ash/webui/settings/public/constants/routes.mojom.h"
#include "base/functional/bind.h"
#include "base/functional/callback_helpers.h"
#include "base/strings/string_number_conversions.h"
#include "base/strings/utf_string_conversions.h"
#include "base/time/time.h"
#include "chrome/browser/ash/borealis/borealis_app_launcher.h"
#include "chrome/browser/ash/borealis/borealis_context_manager.h"
#include "chrome/browser/ash/borealis/borealis_features.h"
#include "chrome/browser/ash/borealis/borealis_installer.h"
#include "chrome/browser/ash/borealis/borealis_metrics.h"
#include "chrome/browser/ash/borealis/borealis_service.h"
#include "chrome/browser/ash/borealis/borealis_service_factory.h"
#include "chrome/browser/ash/borealis/borealis_types.mojom.h"
#include "chrome/browser/ash/borealis/borealis_util.h"
#include "chrome/browser/profiles/profile_manager.h"
#include "chrome/browser/ui/settings_window_manager_chromeos.h"
#include "chrome/browser/ui/views/borealis/borealis_splash_screen_view.h"
#include "chrome/grit/generated_resources.h"
#include "ui/base/l10n/l10n_util.h"
#include "ui/base/l10n/time_format.h"
#include "ui/strings/grit/ui_strings.h"
#include "ui/web_dialogs/web_dialog_ui.h"

namespace {

// Returns the text to be used for a predicted completion time, which is
// something like "starting up" or "3 mins remaining" depending on how
// accurately we can predict.
std::string GetInstallationPredictionText(const base::Time& start_time,
                                          double completion_proportion) {
  base::TimeDelta duration = base::Time::Now() - start_time;
  // We have no confidence in the prediction for the first second or for
  // too-small proportions.
  if (completion_proportion < 0.001 || duration < base::Seconds(1)) {
    return l10n_util::GetStringUTF8(IDS_BOREALIS_INSTALLER_ONGOING_INITIAL);
  }
  // Linear-interpolation to predict remaining time.
  base::TimeDelta remaining = (duration / completion_proportion) - duration;
  return base::UTF16ToUTF8(
      ui::TimeFormat::Simple(ui::TimeFormat::FORMAT_REMAINING,
                             ui::TimeFormat::LENGTH_SHORT, remaining));
}

}  // namespace

namespace ash {

BorealisInstallerPageHandler::BorealisInstallerPageHandler(
    mojo::PendingReceiver<ash::borealis_installer::mojom::PageHandler>
        pending_page_handler,
    mojo::PendingRemote<ash::borealis_installer::mojom::Page> pending_page,
    base::OnceClosure on_page_closed,
    content::WebUI* web_ui)
    : receiver_{this, std::move(pending_page_handler)},
      page_{std::move(pending_page)},
      on_page_closed_{std::move(on_page_closed)},
      profile_(Profile::FromWebUI(web_ui)),
      observation_(this) {}

BorealisInstallerPageHandler::~BorealisInstallerPageHandler() = default;

void BorealisInstallerPageHandler::Install() {
  install_start_time_ = base::Time::Now();
  borealis::BorealisInstaller& installer =
      borealis::BorealisServiceFactory::GetForProfile(profile_)->Installer();
  if (observation_.IsObserving()) {
    observation_.Reset();
  }
  observation_.Observe(&installer);
  installer.Start();
}

void BorealisInstallerPageHandler::ShutDown() {
  borealis::BorealisServiceFactory::GetForProfile(profile_)
      ->ContextManager()
      .ShutDownBorealis(
          base::BindOnce([](borealis::BorealisShutdownResult result) {
            if (result == borealis::BorealisShutdownResult::kSuccess) {
              return;
            }
            LOG(ERROR) << "Failed to shutdown borealis after install: code="
                       << static_cast<int>(result);
          }));
}

void BorealisInstallerPageHandler::CancelInstall() {
  borealis::BorealisServiceFactory::GetForProfile(profile_)
      ->Installer()
      .Cancel();
}

void BorealisInstallerPageHandler::Launch() {
  // Installation starts the borealis VM, so the splash screen would normally
  // not show, therefore we need to show it manually here. Since this is
  // post-install we know borealis wasn't running previously.
  borealis::ShowBorealisSplashScreenView(profile_);
  // Launch button has been clicked.
  borealis::BorealisServiceFactory::GetForProfile(profile_)
      ->AppLauncher()
      .Launch(borealis::kClientAppId,
              borealis::BorealisLaunchSource::kPostInstallLaunch,
              base::BindOnce(
                  [](borealis::BorealisAppLauncher::LaunchResult result) {
                    if (result ==
                        borealis::BorealisAppLauncher::LaunchResult::kSuccess) {
                      return;
                    }
                    LOG(ERROR)
                        << "Failed to launch borealis after install: code="
                        << static_cast<int>(result);
                  }));
}

void BorealisInstallerPageHandler::OnPageClosed() {
  if (on_page_closed_) {
    std::move(on_page_closed_).Run();
  }
}

void BorealisInstallerPageHandler::OnProgressUpdated(double fraction_complete) {
  page_->OnProgressUpdate(
      fraction_complete,
      GetInstallationPredictionText(install_start_time_, fraction_complete));
}

void BorealisInstallerPageHandler::OnInstallationEnded(
    borealis::mojom::InstallResult result,
    const std::string& error_description) {
  page_->OnInstallFinished(result);
}

void BorealisInstallerPageHandler::OpenStoragePage() {
  chrome::SettingsWindowManager::GetInstance()->ShowOSSettings(
      profile_, chromeos::settings::mojom::kStorageSubpagePath);
}

void BorealisInstallerPageHandler::RequestClosePage() {
  page_->RequestClose();
}

}  // namespace ash
