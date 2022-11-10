// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_WEBUI_ASH_LAUNCHER_INTERNALS_LAUNCHER_INTERNALS_UI_H_
#define CHROME_BROWSER_UI_WEBUI_ASH_LAUNCHER_INTERNALS_LAUNCHER_INTERNALS_UI_H_

#include "chrome/browser/ui/webui/ash/launcher_internals/launcher_internals.mojom.h"
#include "chrome/browser/ui/webui/ash/launcher_internals/launcher_internals_handler.h"
#include "chrome/common/webui_url_constants.h"
#include "content/public/browser/webui_config.h"
#include "content/public/common/url_constants.h"
#include "mojo/public/cpp/bindings/pending_receiver.h"
#include "mojo/public/cpp/bindings/pending_remote.h"
#include "mojo/public/cpp/bindings/receiver.h"
#include "ui/webui/mojo_web_ui_controller.h"

namespace ash {

class LauncherInternalsUI;

// WebUIConfig for chrome://launcher-internals
class LauncherInternalsUIConfig
    : public content::DefaultWebUIConfig<LauncherInternalsUI> {
 public:
  LauncherInternalsUIConfig()
      : DefaultWebUIConfig(content::kChromeUIScheme,
                           chrome::kChromeUILauncherInternalsHost) {}
};

// The WebUI controller for chrome://launcher-internals.
class LauncherInternalsUI
    : public ui::MojoWebUIController,
      public launcher_internals::mojom::PageHandlerFactory {
 public:
  explicit LauncherInternalsUI(content::WebUI* web_ui);
  ~LauncherInternalsUI() override;

  LauncherInternalsUI(const LauncherInternalsUI&) = delete;
  LauncherInternalsUI& operator=(const LauncherInternalsUI&) = delete;

  void BindInterface(
      mojo::PendingReceiver<launcher_internals::mojom::PageHandlerFactory>
          receiver);

 private:
  // launcher_internals::mojom::PageHandlerFactory:
  void CreatePageHandler(
      mojo::PendingRemote<launcher_internals::mojom::Page> page) override;

  std::unique_ptr<LauncherInternalsHandler> page_handler_;
  mojo::Receiver<launcher_internals::mojom::PageHandlerFactory>
      factory_receiver_{this};

  WEB_UI_CONTROLLER_TYPE_DECL();
};

}  // namespace ash

#endif  // CHROME_BROWSER_UI_WEBUI_ASH_LAUNCHER_INTERNALS_LAUNCHER_INTERNALS_UI_H_
