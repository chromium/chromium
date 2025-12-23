// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_WEBUI_RELOAD_BUTTON_RELOAD_BUTTON_UI_H_
#define CHROME_BROWSER_UI_WEBUI_RELOAD_BUTTON_RELOAD_BUTTON_UI_H_

#include <memory>

#include "chrome/browser/ui/webui/reload_button/reload_button.mojom-forward.h"
#include "chrome/browser/ui/webui/reload_button/reload_button_page_handler.h"
#include "chrome/browser/ui/webui/top_chrome/top_chrome_web_ui_controller.h"
#include "chrome/browser/ui/webui/top_chrome/top_chrome_webui_config.h"
#include "content/public/browser/webui_config.h"
#include "content/public/common/url_constants.h"
#include "mojo/public/cpp/bindings/pending_receiver.h"
#include "mojo/public/cpp/bindings/receiver.h"
#include "ui/webui/mojo_web_ui_controller.h"

namespace ui {
class ColorProvider;
}  // namespace ui

class ReloadButtonUI : public TopChromeWebUIController,
                       public reload_button::mojom::PageHandlerFactory {
 public:
  explicit ReloadButtonUI(content::WebUI* web_ui);
  ReloadButtonUI(const ReloadButtonUI&) = delete;
  ReloadButtonUI& operator=(const ReloadButtonUI&) = delete;
  ~ReloadButtonUI() override;

  static constexpr std::string_view GetWebUIName() { return "ReloadButtonUI"; }

  void BindInterface(
      mojo::PendingReceiver<reload_button::mojom::PageHandlerFactory> receiver);

  void SetReloadButtonState(bool is_loading, bool is_menu_enabled);

  ReloadButtonPageHandler* page_handler_for_testing();

  // TopChromeWebUIController:
  // The controller uses `requesting_origin` to:
  // 1. Decide which resources to expose, e.g. only expose "chrome://theme"
  //    resources to trusted "chrome://" origins.
  // 2. Generate correct CORS headers. Since resources added here often belong
  //    to a different origin than the page loading the, they need a CORS header
  //    that explicitly allow `current_origin`.
  void PopulateLocalResourceLoaderConfig(
      blink::mojom::LocalResourceLoaderConfig* config,
      const url::Origin& requesting_origin) override;

  // For testing:
  // Sets a custom CommandUpdater for testing purposes.
  void SetCommandUpdaterForTesting(CommandUpdater* command_updater);

  // Sets a custom ColorProvider for testing purposes.
  void SetColorProviderForTesting(const ui::ColorProvider* color_provider);

 private:
  // reload_button::mojom::PageHandlerFactory:
  void CreatePageHandler(
      mojo::PendingRemote<reload_button::mojom::Page> page,
      mojo::PendingReceiver<reload_button::mojom::PageHandler> receiver)
      override;

  CommandUpdater* GetCommandUpdater() const;

  std::unique_ptr<ReloadButtonPageHandler> page_handler_;
  mojo::Receiver<reload_button::mojom::PageHandlerFactory>
      page_factory_receiver_{this};

  // Initialized only in tests by SetCommandUpdaterForTesting().
  raw_ptr<CommandUpdater> command_updater_for_testing_ = nullptr;

  // Initialized only in tests by SetColorProviderForTesting().
  raw_ptr<const ui::ColorProvider> color_provider_for_testing_ = nullptr;

  WEB_UI_CONTROLLER_TYPE_DECL();
};

class ReloadButtonUIConfig
    : public DefaultTopChromeWebUIConfig<ReloadButtonUI> {
 public:
  ReloadButtonUIConfig();
  // DefaultTopChromeWebUIConfig overrides:
  bool IsWebUIEnabled(content::BrowserContext* browser_context) override;
};

#endif  // CHROME_BROWSER_UI_WEBUI_RELOAD_BUTTON_RELOAD_BUTTON_UI_H_
