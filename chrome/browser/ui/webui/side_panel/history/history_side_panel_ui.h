// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_WEBUI_SIDE_PANEL_HISTORY_HISTORY_SIDE_PANEL_UI_H_
#define CHROME_BROWSER_UI_WEBUI_SIDE_PANEL_HISTORY_HISTORY_SIDE_PANEL_UI_H_

#include <memory>

#include "base/memory/weak_ptr.h"
#include "chrome/browser/history_clusters/history_clusters_metrics_logger.h"
#include "chrome/browser/ui/webui/top_chrome/top_chrome_web_ui_controller.h"
#include "chrome/browser/ui/webui/top_chrome/top_chrome_webui_config.h"
#include "chrome/common/webui_url_constants.h"
#include "components/page_image_service/mojom/page_image_service.mojom.h"
#include "content/public/common/url_constants.h"
#include "mojo/public/cpp/bindings/pending_receiver.h"
#include "ui/webui/resources/cr_components/color_change_listener/color_change_listener.mojom.h"
#include "ui/webui/resources/cr_components/history/history.mojom-forward.h"
#include "ui/webui/resources/cr_components/history_clusters/history_clusters.mojom-forward.h"
#include "ui/webui/resources/cr_components/history_embeddings/history_embeddings.mojom.h"

class BrowsingHistoryHandler;

namespace ui {
class ColorChangeHandler;
}

namespace history_clusters {
class HistoryClustersHandler;
}

class HistoryEmbeddingsHandler;

namespace page_image_service {
class ImageServiceHandler;
}

namespace content {
class BrowserContext;
}

class BrowserWindowInterface;
class HistorySidePanelUI;

class HistorySidePanelUIConfig
    : public DefaultTopChromeWebUIConfig<HistorySidePanelUI> {
 public:
  HistorySidePanelUIConfig();

  // DefaultTopChromeWebUIConfig::
  bool IsPreloadable() override;
  bool IsWebUIEnabled(content::BrowserContext* browser_context) override;
  std::optional<int> GetCommandIdForTesting() override;
};

class HistorySidePanelUI : public TopChromeWebUIController {
 public:
  explicit HistorySidePanelUI(content::WebUI* web_ui);
  HistorySidePanelUI(const HistorySidePanelUI&) = delete;
  HistorySidePanelUI& operator=(const HistorySidePanelUI&) = delete;
  ~HistorySidePanelUI() override;

  // Instantiates the implementor of the mojom::PageHandlerFactory mojo
  // interface passing the pending receiver that will be internally bound.
  void BindInterface(
      mojo::PendingReceiver<history::mojom::PageHandler> pending_page_handler);
  void BindInterface(
      mojo::PendingReceiver<color_change_listener::mojom::PageHandler>
          pending_receiver);
  void BindInterface(mojo::PendingReceiver<history_clusters::mojom::PageHandler>
                         pending_page_handler);
  void BindInterface(
      mojo::PendingReceiver<page_image_service::mojom::PageImageServiceHandler>
          pending_page_handler);
  void BindInterface(
      mojo::PendingReceiver<history_embeddings::mojom::PageHandler>
          pending_page_handler);

  // Gets a weak pointer to this object.
  base::WeakPtr<HistorySidePanelUI> GetWeakPtr();

  // Sets the side panel UI to show `query`.
  void SetQuery(const std::string& query);

  // Gets the last query issued by the WebUI, regardless if it was initiated
  // from the omnibox action or within the Side Panel WebUI itself.
  std::string GetLastQueryIssued() const;

  static constexpr std::string GetWebUIName() { return "HistorySidePanel"; }

 private:
  std::unique_ptr<BrowsingHistoryHandler> browsing_history_handler_;
  std::unique_ptr<ui::ColorChangeHandler> color_provider_handler_;
  std::unique_ptr<history_clusters::HistoryClustersHandler>
      history_clusters_handler_;
  std::unique_ptr<page_image_service::ImageServiceHandler>
      image_service_handler_;
  std::unique_ptr<HistoryEmbeddingsHandler> history_embeddings_handler_;

  raw_ptr<BrowserWindowInterface> browser_window_interface_;

  // Used for `GetWeakPtr()`.
  base::WeakPtrFactory<HistorySidePanelUI> weak_ptr_factory_{this};

  WEB_UI_CONTROLLER_TYPE_DECL();
};

#endif  // CHROME_BROWSER_UI_WEBUI_SIDE_PANEL_HISTORY_HISTORY_SIDE_PANEL_UI_H_
