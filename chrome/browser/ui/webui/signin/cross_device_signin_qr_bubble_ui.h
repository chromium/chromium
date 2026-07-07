// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_WEBUI_SIGNIN_CROSS_DEVICE_SIGNIN_QR_BUBBLE_UI_H_
#define CHROME_BROWSER_UI_WEBUI_SIGNIN_CROSS_DEVICE_SIGNIN_QR_BUBBLE_UI_H_

#include "chrome/browser/ui/webui/signin/cross_device_signin_qr_bubble/cross_device_signin_qr_bubble.mojom.h"
#include "chrome/common/webui_url_constants.h"
#include "content/public/browser/webui_config.h"
#include "content/public/common/url_constants.h"
#include "mojo/public/cpp/bindings/pending_receiver.h"
#include "mojo/public/cpp/bindings/receiver.h"
#include "ui/webui/mojo_web_ui_controller.h"

class CrossDeviceSigninQrBubbleUI;

class CrossDeviceSigninQrBubbleUIConfig
    : public content::DefaultWebUIConfig<CrossDeviceSigninQrBubbleUI> {
 public:
  CrossDeviceSigninQrBubbleUIConfig();

  // content::WebUIConfig:
  bool IsWebUIEnabled(content::BrowserContext* browser_context) override;
};

// WebUI controller for the cross-device QR code bubble.
class CrossDeviceSigninQrBubbleUI
    : public ui::MojoWebUIController,
      public cross_device_signin::mojom::PageHandlerFactory {
 public:
  explicit CrossDeviceSigninQrBubbleUI(content::WebUI* web_ui);
  ~CrossDeviceSigninQrBubbleUI() override;

  CrossDeviceSigninQrBubbleUI(const CrossDeviceSigninQrBubbleUI&) = delete;
  CrossDeviceSigninQrBubbleUI& operator=(const CrossDeviceSigninQrBubbleUI&) =
      delete;

  void BindInterface(
      mojo::PendingReceiver<cross_device_signin::mojom::PageHandlerFactory>
          receiver);

 private:
  // cross_device_signin::mojom::PageHandlerFactory:
  void CreateCrossDeviceSigninQrBubbleHandler(
      mojo::PendingReceiver<cross_device_signin::mojom::PageHandler> receiver)
      override;

  std::unique_ptr<cross_device_signin::mojom::PageHandler> page_handler_;

  mojo::Receiver<cross_device_signin::mojom::PageHandlerFactory>
      page_factory_receiver_{this};

  WEB_UI_CONTROLLER_TYPE_DECL();
};

#endif  // CHROME_BROWSER_UI_WEBUI_SIGNIN_CROSS_DEVICE_SIGNIN_QR_BUBBLE_UI_H_
