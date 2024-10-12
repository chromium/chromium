// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_WEBUI_ON_DEVICE_TRANSLATION_INTERNALS_ON_DEVICE_TRANSLATION_INTERNALS_UI_H_
#define CHROME_BROWSER_UI_WEBUI_ON_DEVICE_TRANSLATION_INTERNALS_ON_DEVICE_TRANSLATION_INTERNALS_UI_H_

#include "chrome/browser/ui/webui/on_device_translation_internals/on_device_translation_internals.mojom.h"
#include "content/public/browser/web_ui_controller.h"
#include "content/public/browser/webui_config.h"
#include "ui/webui/mojo_web_ui_controller.h"

class OnDeviceTranslationInternalsPageHandlerImpl;

// WebUIConfig for chrome://on-device-translation-internals
class OnDeviceTranslationInternalsUIConfig : public content::WebUIConfig {
 public:
  OnDeviceTranslationInternalsUIConfig();
  ~OnDeviceTranslationInternalsUIConfig() override;

  // content::WebUIConfig:
  std::unique_ptr<content::WebUIController> CreateWebUIController(
      content::WebUI* web_ui,
      const GURL& url) override;
};

// The WebUI controller for chrome://on-device-translation-internals.
class OnDeviceTranslationInternalsUI
    : public ui::MojoWebUIController,
      public on_device_translation_internals::mojom::PageHandlerFactory {
 public:
  explicit OnDeviceTranslationInternalsUI(content::WebUI* web_ui);
  ~OnDeviceTranslationInternalsUI() override;

  OnDeviceTranslationInternalsUI(const OnDeviceTranslationInternalsUI&) =
      delete;
  OnDeviceTranslationInternalsUI& operator=(
      const OnDeviceTranslationInternalsUI&) = delete;

  void BindInterface(
      mojo::PendingReceiver<
          on_device_translation_internals::mojom::PageHandlerFactory> receiver);

 private:
  // on_device_translation_internals::mojom::PageHandlerFactory impls.
  void CreatePageHandler(
      mojo::PendingRemote<on_device_translation_internals::mojom::Page> page,
      mojo::PendingReceiver<on_device_translation_internals::mojom::PageHandler>
          receiver) override;

  std::unique_ptr<OnDeviceTranslationInternalsPageHandlerImpl>
      on_device_translation_internals_page_handler_;
  mojo::Receiver<on_device_translation_internals::mojom::PageHandlerFactory>
      on_device_translation_internals_page_factory_receiver_{this};

  WEB_UI_CONTROLLER_TYPE_DECL();
};

#endif  // CHROME_BROWSER_UI_WEBUI_ON_DEVICE_TRANSLATION_INTERNALS_ON_DEVICE_TRANSLATION_INTERNALS_UI_H_
