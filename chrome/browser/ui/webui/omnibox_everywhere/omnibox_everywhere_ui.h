// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_WEBUI_OMNIBOX_EVERYWHERE_OMNIBOX_EVERYWHERE_UI_H_
#define CHROME_BROWSER_UI_WEBUI_OMNIBOX_EVERYWHERE_OMNIBOX_EVERYWHERE_UI_H_

#include <memory>

#include "base/memory/raw_ptr.h"
#include "chrome/browser/ui/webui/omnibox_everywhere/debug/omnibox_everywhere_debug.mojom.h"
#include "chrome/browser/ui/webui/top_chrome/top_chrome_web_ui_controller.h"
#include "chrome/browser/ui/webui/top_chrome/top_chrome_webui_config.h"
#include "chrome/common/webui_url_constants.h"
#include "components/omnibox/browser/searchbox.mojom.h"
#include "content/public/common/url_constants.h"
#include "mojo/public/cpp/bindings/pending_receiver.h"
#include "mojo/public/cpp/bindings/pending_remote.h"
#include "mojo/public/cpp/bindings/receiver.h"
#include "ui/webui/resources/cr_components/composebox/composebox.mojom.h"

namespace omnibox_everywhere_debug {
class OmniboxEverywhereDebugPageHandler;
}

class ComposeboxEverywhereHandler;
class OmniboxEverywhereHandler;
class Profile;

namespace contextual_search {
class ContextualSearchSessionHandle;
}

class OmniboxEverywhereUI;

class OmniboxEverywhereUIConfig
    : public DefaultTopChromeWebUIConfig<OmniboxEverywhereUI> {
 public:
  OmniboxEverywhereUIConfig()
      : DefaultTopChromeWebUIConfig(content::kChromeUIScheme,
                                    chrome::kChromeUIOmniboxEverywhereHost) {}

  bool IsWebUIEnabled(content::BrowserContext* browser_context) override;
  bool ShouldCrashOnJavascriptErrorInDevelopmentBuild() const override;
};

class OmniboxEverywhereUI
    : public TopChromeWebUIController,
      public composebox::mojom::PageHandlerFactory,
      public searchbox::mojom::PageHandlerFactory,
      public omnibox_everywhere_debug::mojom::PageHandlerFactory {
 public:
  explicit OmniboxEverywhereUI(content::WebUI* web_ui);
  OmniboxEverywhereUI(const OmniboxEverywhereUI&) = delete;
  OmniboxEverywhereUI& operator=(const OmniboxEverywhereUI&) = delete;
  ~OmniboxEverywhereUI() override;

  static constexpr std::string_view GetWebUIName() {
    return "OmniboxEverywhere";
  }

  // composebox::mojom::PageHandlerFactory:
  void BindInterface(
      mojo::PendingReceiver<composebox::mojom::PageHandlerFactory> receiver);
  void CreatePageHandler(
      mojo::PendingReceiver<composebox::mojom::PageHandler>
          pending_page_handler,
      mojo::PendingRemote<searchbox::mojom::Page> pending_searchbox_page,
      mojo::PendingReceiver<searchbox::mojom::PageHandler>
          pending_searchbox_handler) override;

  // searchbox::mojom::PageHandlerFactory:
  void BindInterface(content::RenderFrameHost* host,
                     mojo::PendingReceiver<searchbox::mojom::PageHandlerFactory>
                         pending_page_handler);
  void CreatePageHandler(
      mojo::PendingRemote<searchbox::mojom::Page> page,
      mojo::PendingReceiver<searchbox::mojom::PageHandler> handler) override;

  // omnibox_everywhere_debug::mojom::PageHandlerFactory:
  void BindInterface(
      mojo::PendingReceiver<omnibox_everywhere_debug::mojom::PageHandlerFactory>
          receiver);
  void CreatePageHandler(
      mojo::PendingRemote<omnibox_everywhere_debug::mojom::Page> page,
      mojo::PendingReceiver<omnibox_everywhere_debug::mojom::PageHandler>
          handler) override;

  ComposeboxEverywhereHandler* composebox_handler() {
    return composebox_handler_.get();
  }
  OmniboxEverywhereHandler* omnibox_handler() { return omnibox_handler_.get(); }

 private:
  contextual_search::ContextualSearchSessionHandle*
  GetOrCreateContextualSessionHandle();
  void ClearContextualSessionHandle();

  raw_ptr<Profile> profile_;

  std::unique_ptr<ComposeboxEverywhereHandler> composebox_handler_;
  std::unique_ptr<OmniboxEverywhereHandler> omnibox_handler_;

  std::unique_ptr<omnibox_everywhere_debug::OmniboxEverywhereDebugPageHandler>
      debug_page_handler_;

  std::unique_ptr<contextual_search::ContextualSearchSessionHandle>
      shared_session_handle_;

  mojo::Receiver<composebox::mojom::PageHandlerFactory>
      composebox_page_factory_receiver_{this};
  mojo::Receiver<searchbox::mojom::PageHandlerFactory>
      searchbox_page_factory_receiver_{this};
  mojo::Receiver<omnibox_everywhere_debug::mojom::PageHandlerFactory>
      debug_page_factory_receiver_{this};

  WEB_UI_CONTROLLER_TYPE_DECL();
};

#endif  // CHROME_BROWSER_UI_WEBUI_OMNIBOX_EVERYWHERE_OMNIBOX_EVERYWHERE_UI_H_
