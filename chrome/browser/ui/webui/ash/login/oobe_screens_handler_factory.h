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
  void CreateDrivePinningScreenHandler(
      mojo::PendingRemote<screens_common::mojom::DrivePinningPage> page,
      mojo::PendingReceiver<screens_common::mojom::DrivePinningPageHandler>
          receiver) override;
  void CreateGestureNavigationPageHandler(
      mojo::PendingReceiver<screens_common::mojom::GestureNavigationPageHandler>
          receiver) override;

  void CreateGaiaInfoScreenHandler(
      mojo::PendingRemote<screens_common::mojom::GaiaInfoPage> page,
      mojo::PendingReceiver<screens_common::mojom::GaiaInfoPageHandler>
          receiver) override;

  void CreateConsumerUpdatePageHandler(
      mojo::PendingRemote<screens_oobe::mojom::ConsumerUpdatePage> page,
      mojo::PendingReceiver<screens_oobe::mojom::ConsumerUpdatePageHandler>
          handler) override;

  void CreatePackagedLicensePageHandler(
      mojo::PendingReceiver<screens_oobe::mojom::PackagedLicensePageHandler>
          receiver) override;

  void CreateLocalDataLossWarningPageHandler(
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
