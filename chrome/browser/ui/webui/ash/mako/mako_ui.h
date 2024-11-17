// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_WEBUI_ASH_MAKO_MAKO_UI_H_
#define CHROME_BROWSER_UI_WEBUI_ASH_MAKO_MAKO_UI_H_

#include "chrome/browser/ui/webui/ash/lobster/lobster_page_handler.h"
#include "chrome/browser/ui/webui/top_chrome/top_chrome_webui_config.h"
#include "chrome/browser/ui/webui/top_chrome/untrusted_top_chrome_web_ui_controller.h"
#include "chromeos/ash/services/orca/public/mojom/orca_service.mojom.h"
#include "third_party/skia/include/core/SkRegion.h"
#include "ui/webui/color_change_listener/color_change_handler.h"
#include "ui/webui/resources/cr_components/color_change_listener/color_change_listener.mojom.h"

namespace ash {

class MakoUntrustedUI;

// WebUIConfig for chrome://mako
class MakoUntrustedUIConfig
    : public DefaultTopChromeWebUIConfig<MakoUntrustedUI> {
 public:
  MakoUntrustedUIConfig();
  ~MakoUntrustedUIConfig() override;

  // DefaultTopChromeWebUIConfig:
  bool IsWebUIEnabled(content::BrowserContext* browser_context) override;
  bool ShouldAutoResizeHost() override;
};

// The WebUI for chrome://mako
class MakoUntrustedUI : public UntrustedTopChromeWebUIController {
 public:
  explicit MakoUntrustedUI(content::WebUI* web_ui);
  ~MakoUntrustedUI() override;

  void BindInterface(
      mojo::PendingReceiver<orca::mojom::EditorClient> pending_receiver);

  void BindInterface(
      mojo::PendingReceiver<color_change_listener::mojom::PageHandler>
          receiver);

  void BindInterface(
      mojo::PendingReceiver<lobster::mojom::UntrustedLobsterPageHandler>
          pending_receiver);

  static constexpr std::string GetWebUIName() { return "MakoUntrusted"; }

 private:
  WEB_UI_CONTROLLER_TYPE_DECL();

  std::optional<SkRegion> draggable_region_ = std::nullopt;
  std::unique_ptr<ui::ColorChangeHandler> color_provider_handler_;
  std::unique_ptr<LobsterPageHandler> lobster_page_handler_;
};

}  // namespace ash

#endif  // CHROME_BROWSER_UI_WEBUI_ASH_MAKO_MAKO_UI_H_
