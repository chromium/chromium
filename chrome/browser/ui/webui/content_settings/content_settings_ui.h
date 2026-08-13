// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_WEBUI_CONTENT_SETTINGS_CONTENT_SETTINGS_UI_H_
#define CHROME_BROWSER_UI_WEBUI_CONTENT_SETTINGS_CONTENT_SETTINGS_UI_H_

#include <memory>

#include "chrome/browser/ui/webui/content_settings/content_settings_internals.mojom.h"
#include "chrome/common/webui_url_constants.h"
#include "content/public/browser/internal_webui_config.h"
#include "mojo/public/cpp/bindings/pending_receiver.h"
#include "mojo/public/cpp/bindings/receiver.h"
#include "ui/webui/mojo_web_ui_controller.h"

namespace content_settings_internals {

class ContentSettingsHandler;
class ContentSettingsUI;

class ContentSettingsUIConfig
    : public content::DefaultInternalWebUIConfig<ContentSettingsUI> {
 public:
  ContentSettingsUIConfig()
      : DefaultInternalWebUIConfig(chrome::kChromeUIContentSettingsHost) {}
};

// MojoWebUIController for chrome://content-settings WebUI page.
class ContentSettingsUI : public ui::MojoWebUIController,
                          public mojom::PageHandlerFactory {
 public:
  explicit ContentSettingsUI(content::WebUI* web_ui);

  ~ContentSettingsUI() override;

  ContentSettingsUI(const ContentSettingsUI&) = delete;
  ContentSettingsUI& operator=(const ContentSettingsUI&) = delete;

  void BindInterface(mojo::PendingReceiver<mojom::PageHandlerFactory> receiver);

 private:
  // mojom::PageHandlerFactory:
  void CreatePageHandler(
      mojo::PendingReceiver<mojom::PageHandler> receiver) override;

  std::unique_ptr<ContentSettingsHandler> handler_;
  mojo::Receiver<mojom::PageHandlerFactory> page_factory_receiver_{this};
  WEB_UI_CONTROLLER_TYPE_DECL();
};

}  // namespace content_settings_internals

#endif  // CHROME_BROWSER_UI_WEBUI_CONTENT_SETTINGS_CONTENT_SETTINGS_UI_H_
