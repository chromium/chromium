// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_WEBUI_TAB_STRIP_INTERNALS_TAB_STRIP_INTERNALS_UI_H_
#define CHROME_BROWSER_UI_WEBUI_TAB_STRIP_INTERNALS_TAB_STRIP_INTERNALS_UI_H_

#include <memory>

#include "chrome/browser/ui/webui/tab_strip_internals/tab_strip_internals.mojom.h"
#include "chrome/common/webui_url_constants.h"
#include "content/public/browser/internal_webui_config.h"
#include "mojo/public/cpp/bindings/receiver.h"
#include "ui/webui/mojo_web_ui_controller.h"

class TabStripInternalsUI;
class TabStripInternalsPageHandler;

// Registers chrome://tab-strip-internals as a debug-only WebUI that
// is conditionally enabled via the `kInternalOnlyUisEnabled` pref.
class TabStripInternalsUIConfig
    : public content::DefaultInternalWebUIConfig<TabStripInternalsUI> {
 public:
  TabStripInternalsUIConfig()
      : DefaultInternalWebUIConfig(chrome::kChromeUITabStripInternalsHost) {}

  // Overridden implementation of `DefaultInternalWebUIConfig::IsWebUIEnabled`.
  bool IsWebUIEnabled(content::BrowserContext* browser_context) override;
};

// The Web UI controller for the chrome://tab-strip-internals page.
class TabStripInternalsUI
    : public ui::MojoWebUIController,
      public tab_strip_internals::mojom::PageHandlerFactory {
 public:
  explicit TabStripInternalsUI(content::WebUI* web_ui);

  TabStripInternalsUI(const TabStripInternalsUI&) = delete;
  TabStripInternalsUI& operator=(const TabStripInternalsUI&) = delete;
  ~TabStripInternalsUI() override;

  // Instantiates the implementor of the mojom::PageHandlerFactory mojo
  // interface passing the pending receiver that will be internally bound.
  void BindInterface(
      mojo::PendingReceiver<tab_strip_internals::mojom::PageHandlerFactory>
          receiver);

 private:
  // Instantiates the implementor of the mojom::PageHandler mojo interface.
  void CreatePageHandler(
      mojo::PendingRemote<tab_strip_internals::mojom::Page> page,
      mojo::PendingReceiver<tab_strip_internals::mojom::PageHandler> receiver)
      override;

  std::unique_ptr<TabStripInternalsPageHandler> page_handler_;

  mojo::Receiver<tab_strip_internals::mojom::PageHandlerFactory>
      page_factory_receiver_{this};

  WEB_UI_CONTROLLER_TYPE_DECL();
};

#endif  // CHROME_BROWSER_UI_WEBUI_TAB_STRIP_INTERNALS_TAB_STRIP_INTERNALS_UI_H_
