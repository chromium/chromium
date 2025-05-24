// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/webui/ash/login/oobe_screens_handler_factory.h"

#include "base/check_deref.h"
#include "chrome/browser/ash/login/oobe_screen.h"
#include "chrome/browser/ash/login/screens/ai_intro_screen.h"
#include "chrome/browser/ash/login/screens/app_downloading_screen.h"
#include "chrome/browser/ash/login/screens/arc_vm_data_migration_screen.h"
#include "chrome/browser/ash/login/screens/consumer_update_screen.h"
#include "chrome/browser/ash/login/screens/encryption_migration_screen.h"
#include "chrome/browser/ash/login/screens/gaia_info_screen.h"
#include "chrome/browser/ash/login/screens/gemini_intro_screen.h"
#include "chrome/browser/ash/login/screens/osauth/local_data_loss_warning_screen.h"
#include "chrome/browser/ash/login/wizard_controller.h"
#include "chrome/browser/ui/webui/ash/login/ai_intro_screen_handler.h"
#include "chrome/browser/ui/webui/ash/login/app_downloading_screen_handler.h"
#include "chrome/browser/ui/webui/ash/login/arc_vm_data_migration_screen_handler.h"
#include "chrome/browser/ui/webui/ash/login/consumer_update_screen_handler.h"
#include "chrome/browser/ui/webui/ash/login/drive_pinning_screen_handler.h"
#include "chrome/browser/ui/webui/ash/login/encryption_migration_screen_handler.h"
#include "chrome/browser/ui/webui/ash/login/gaia_info_screen_handler.h"
#include "chrome/browser/ui/webui/ash/login/gemini_intro_screen_handler.h"
#include "chrome/browser/ui/webui/ash/login/gesture_navigation_screen_handler.h"
#include "chrome/browser/ui/webui/ash/login/mojom/screens_common.mojom.h"
#include "chrome/browser/ui/webui/ash/login/mojom/screens_factory.mojom.h"
#include "chrome/browser/ui/webui/ash/login/mojom/screens_oobe.mojom.h"
#include "chrome/browser/ui/webui/ash/login/osauth/local_data_loss_warning_screen_handler.h"
#include "chrome/browser/ui/webui/ash/login/packaged_license_screen_handler.h"

namespace ash {

OobeScreensHandlerFactory::OobeScreensHandlerFactory(
    mojo::PendingReceiver<screens_factory::mojom::ScreensFactory>
        pending_receiver) {
  // If WizardController is unavailable during frontend element construction,
  // store the pending receiver. Upon WizardController creation, bind the stored
  // receiver.
  // TODO(b/329384403) Implement browser_test for the scenario where
  // WizardController is unavailable during frontend element construction.
  if (WizardController::default_controller()) {
    page_factory_receiver_.Bind(std::move(pending_receiver));
  } else {
    peding_receiver_ = std::move(pending_receiver);
  }
}

OobeScreensHandlerFactory::~OobeScreensHandlerFactory() = default;

void OobeScreensHandlerFactory::BindScreensHandlerFactory() {
  if (peding_receiver_.is_valid() && !page_factory_receiver_.is_bound()) {
    page_factory_receiver_.Bind(std::move(peding_receiver_));
  }
}

void OobeScreensHandlerFactory::UnbindScreensHandlerFactory() {
  page_factory_receiver_.reset();
}

void OobeScreensHandlerFactory::EstablishAiIntroScreenPipe(
    mojo::PendingReceiver<screens_common::mojom::AiIntroPageHandler> receiver,
    EstablishAiIntroScreenPipeCallback callback) {
  AiIntroScreen* ai_intro = CHECK_DEREF(WizardController::default_controller())
                                .GetScreen<AiIntroScreen>();
  ai_intro->BindPageHandlerReceiver(std::move(receiver));
  ai_intro->PassPagePendingReceiverWithCallback(std::move(callback));
}

void OobeScreensHandlerFactory::EstablishAppDownloadingScreenPipe(
    mojo::PendingReceiver<screens_common::mojom::AppDownloadingPageHandler>
        receiver) {
  AppDownloadingScreen* app_downloading =
      CHECK_DEREF(WizardController::default_controller())
          .GetScreen<AppDownloadingScreen>();
  app_downloading->BindPageHandlerReceiver(std::move(receiver));
}

void OobeScreensHandlerFactory::EstablishDrivePinningScreenPipe(
    mojo::PendingReceiver<screens_common::mojom::DrivePinningPageHandler>
        receiver,
    EstablishDrivePinningScreenPipeCallback callback) {
  DrivePinningScreen* drive_pinning =
      CHECK_DEREF(WizardController::default_controller())
          .GetScreen<DrivePinningScreen>();
  drive_pinning->BindPageHandlerReceiver(std::move(receiver));
  drive_pinning->PassPagePendingReceiverWithCallback(std::move(callback));
}

void OobeScreensHandlerFactory::EstablishGaiaInfoScreenPipe(
    mojo::PendingReceiver<screens_common::mojom::GaiaInfoPageHandler> receiver,
    EstablishGaiaInfoScreenPipeCallback callback) {
  GaiaInfoScreen* gaia_info =
      CHECK_DEREF(WizardController::default_controller())
          .GetScreen<GaiaInfoScreen>();
  gaia_info->BindPageHandlerReceiver(std::move(receiver));
  gaia_info->PassPagePendingReceiverWithCallback(std::move(callback));
}

void OobeScreensHandlerFactory::EstablishGeminiIntroScreenPipe(
    mojo::PendingReceiver<screens_common::mojom::GeminiIntroPageHandler>
        receiver) {
  GeminiIntroScreen* gemini_intro =
      CHECK_DEREF(WizardController::default_controller())
          .GetScreen<GeminiIntroScreen>();
  gemini_intro->BindPageHandlerReceiver(std::move(receiver));
}

void OobeScreensHandlerFactory::EstablishGestureNavigationScreenPipe(
    mojo::PendingReceiver<screens_common::mojom::GestureNavigationPageHandler>
        receiver) {
  GestureNavigationScreen* gesture_navigation =
      CHECK_DEREF(WizardController::default_controller())
          .GetScreen<GestureNavigationScreen>();
  gesture_navigation->BindPageHandlerReceiver(std::move(receiver));
}

void OobeScreensHandlerFactory::EstablishConsumerUpdateScreenPipe(
    mojo::PendingReceiver<screens_oobe::mojom::ConsumerUpdatePageHandler>
        handler,
    EstablishConsumerUpdateScreenPipeCallback callback) {
  ConsumerUpdateScreen* consumer_update =
      CHECK_DEREF(WizardController::default_controller())
          .GetScreen<ConsumerUpdateScreen>();
  consumer_update->BindPageHandlerReceiver(std::move(handler));
  consumer_update->PassPagePendingReceiverWithCallback(std::move(callback));
}

void OobeScreensHandlerFactory::EstablishPackagedLicenseScreenPipe(
    mojo::PendingReceiver<screens_oobe::mojom::PackagedLicensePageHandler>
        receiver) {
  PackagedLicenseScreen* packaged_license =
      CHECK_DEREF(WizardController::default_controller())
          .GetScreen<PackagedLicenseScreen>();
  packaged_license->BindPageHandlerReceiver(std::move(receiver));
}

void OobeScreensHandlerFactory::EstablishArcVmDataMigrationScreenPipe(
    mojo::PendingReceiver<screens_login::mojom::ArcVmDataMigrationPageHandler>
        receiver,
    EstablishArcVmDataMigrationScreenPipeCallback callback) {
  ArcVmDataMigrationScreen* arc_vm_data_migration =
      CHECK_DEREF(WizardController::default_controller())
          .GetScreen<ArcVmDataMigrationScreen>();
  arc_vm_data_migration->BindPageHandlerReceiver(std::move(receiver));
  arc_vm_data_migration->PassPagePendingReceiverWithCallback(
      std::move(callback));
}

void OobeScreensHandlerFactory::EstablishEncryptionMigrationScreenPipe(
    mojo::PendingReceiver<screens_login::mojom::EncryptionMigrationPageHandler>
        receiver,
    EstablishEncryptionMigrationScreenPipeCallback callback) {
  CHECK(WizardController::default_controller());
  EncryptionMigrationScreen* encryption_migration =
      WizardController::default_controller()
          ->GetScreen<EncryptionMigrationScreen>();
  encryption_migration->BindPageHandlerReceiver(std::move(receiver));
  encryption_migration->PassPagePendingReceiverWithCallback(
      std::move(callback));
}

void OobeScreensHandlerFactory::EstablishLocalDataLossWarningScreenPipe(
    mojo::PendingReceiver<
        screens_osauth::mojom::LocalDataLossWarningPageHandler> receiver) {
  LocalDataLossWarningScreen* local_data_loss_warning =
      CHECK_DEREF(WizardController::default_controller())
          .GetScreen<LocalDataLossWarningScreen>();
  local_data_loss_warning->BindPageHandlerReceiver(std::move(receiver));
}

}  // namespace ash
