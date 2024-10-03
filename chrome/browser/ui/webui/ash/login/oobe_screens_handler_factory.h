// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_WEBUI_ASH_LOGIN_OOBE_SCREENS_HANDLER_FACTORY_H_
#define CHROME_BROWSER_UI_WEBUI_ASH_LOGIN_OOBE_SCREENS_HANDLER_FACTORY_H_

#include "chrome/browser/ash/login/oobe_screen.h"
#include "chrome/browser/ui/webui/ash/login/mojom/screens_common.mojom.h"
#include "chrome/browser/ui/webui/ash/login/mojom/screens_factory.mojom.h"
#include "chrome/browser/ui/webui/ash/login/mojom/screens_oobe.mojom.h"
#include "chrome/browser/ui/webui/ash/login/mojom/screens_osauth.mojom.h"
#include "mojo/public/cpp/bindings/pending_receiver.h"
#include "mojo/public/cpp/bindings/pending_remote.h"
#include "mojo/public/cpp/bindings/receiver.h"

namespace ash {

class OobeScreensHandlerFactory
    : public screens_factory::mojom::ScreensFactory {
 public:
  OobeScreensHandlerFactory(
      mojo::PendingReceiver<screens_factory::mojom::ScreensFactory>
          pending_receiver);

  OobeScreensHandlerFactory(const OobeScreensHandlerFactory&) = delete;
  OobeScreensHandlerFactory& operator=(const OobeScreensHandlerFactory&) =
      delete;

  ~OobeScreensHandlerFactory() override;

  void BindScreensHandlerFactory();
  void UnbindScreensHandlerFactory();

 private:
  // screens_factory::mojom::ScreensFactory:
  void EstablishAiIntroScreenPipe(
      mojo::PendingReceiver<screens_common::mojom::AiIntroPageHandler> receiver,
      EstablishAiIntroScreenPipeCallback callback) override;

  void EstablishAppDownloadingScreenPipe(
      mojo::PendingReceiver<screens_common::mojom::AppDownloadingPageHandler>
          receiver) override;

  void EstablishDrivePinningScreenPipe(
      mojo::PendingReceiver<screens_common::mojom::DrivePinningPageHandler>
          receiver,
      EstablishDrivePinningScreenPipeCallback callback) override;

  void EstablishGaiaInfoScreenPipe(
      mojo::PendingReceiver<screens_common::mojom::GaiaInfoPageHandler>
          receiver,
      EstablishGaiaInfoScreenPipeCallback callback) override;

  void EstablishGeminiIntroScreenPipe(
      mojo::PendingReceiver<screens_common::mojom::GeminiIntroPageHandler>
          receiver) override;

  void EstablishGestureNavigationScreenPipe(
      mojo::PendingReceiver<screens_common::mojom::GestureNavigationPageHandler>
          receiver) override;

  void EstablishConsumerUpdateScreenPipe(
      mojo::PendingReceiver<screens_oobe::mojom::ConsumerUpdatePageHandler>
          handler,
      EstablishConsumerUpdateScreenPipeCallback callback) override;

  void EstablishPackagedLicenseScreenPipe(
      mojo::PendingReceiver<screens_oobe::mojom::PackagedLicensePageHandler>
          receiver) override;

  void EstablishArcVmDataMigrationScreenPipe(
      mojo::PendingReceiver<screens_login::mojom::ArcVmDataMigrationPageHandler>
          receiver,
      EstablishArcVmDataMigrationScreenPipeCallback callback) override;

  void EstablishEncryptionMigrationScreenPipe(
      mojo::PendingReceiver<
          screens_login::mojom::EncryptionMigrationPageHandler> receiver,
      EstablishEncryptionMigrationScreenPipeCallback callback) override;

  void EstablishLocalDataLossWarningScreenPipe(
      mojo::PendingReceiver<
          screens_osauth::mojom::LocalDataLossWarningPageHandler> receiver)
      override;

  mojo::Receiver<screens_factory::mojom::ScreensFactory> page_factory_receiver_{
      this};
  mojo::PendingReceiver<screens_factory::mojom::ScreensFactory>
      peding_receiver_;
};

}  // namespace ash

#endif  // CHROME_BROWSER_UI_WEBUI_ASH_LOGIN_OOBE_SCREENS_HANDLER_FACTORY_H_
