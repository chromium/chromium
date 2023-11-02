// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_WEBUI_SIDE_PANEL_HISTORY_CLUSTERS_HISTORY_CLUSTERS_SIDE_PANEL_UI_H_
#define CHROME_BROWSER_UI_WEBUI_SIDE_PANEL_HISTORY_CLUSTERS_HISTORY_CLUSTERS_SIDE_PANEL_UI_H_

#include <memory>

#include "base/memory/weak_ptr.h"
#include "chrome/browser/history_clusters/history_clusters_metrics_logger.h"
#include "content/public/browser/web_contents_observer.h"
#include "mojo/public/cpp/bindings/pending_receiver.h"
#include "ui/webui/mojo_bubble_web_ui_controller.h"
#include "ui/webui/resources/cr_components/history_clusters/history_clusters.mojom-forward.h"

namespace history_clusters {
class HistoryClustersHandler;
}

class HistoryClustersSidePanelUI : public ui::MojoBubbleWebUIController,
                                   public content::WebContentsObserver {
 public:
  explicit HistoryClustersSidePanelUI(content::WebUI* web_ui);
  HistoryClustersSidePanelUI(const HistoryClustersSidePanelUI&) = delete;
  HistoryClustersSidePanelUI& operator=(const HistoryClustersSidePanelUI&) =
      delete;
  ~HistoryClustersSidePanelUI() override;

  // Instantiates the implementor of the mojom::PageHandlerFactory mojo
  // interface passing the pending receiver that will be internally bound.
  void BindInterface(mojo::PendingReceiver<history_clusters::mojom::PageHandler>
                         pending_page_handler);

  // Gets a weak pointer to this object.
  base::WeakPtr<HistoryClustersSidePanelUI> GetWeakPtr();

  // Sets the side panel UI to show `query`.
  void SetQuery(const std::string& query);

  // Gets the last query issued by the WebUI, regardless if it was initiated
  // from the omnibox action or within the Side Panel WebUI itself.
  std::string GetLastQueryIssued() const;

  // Sets the metrics initial state for logging. We need this because we can't
  // start making logs until the WebUI has finished navigating.
  void set_metrics_initial_state(
      history_clusters::HistoryClustersInitialState metrics_initial_state) {
    metrics_initial_state_ = metrics_initial_state;
  }

  // WebContentsObserver:
  void DidFinishNavigation(
      content::NavigationHandle* navigation_handle) override;

 private:
  std::unique_ptr<history_clusters::HistoryClustersHandler>
      history_clusters_handler_;

  // The initial state that we have to cache here until the page finishes its
  // navigation to the WebUI host.
  history_clusters::HistoryClustersInitialState metrics_initial_state_ =
      history_clusters::HistoryClustersInitialState::kUnknown;

  // Used for `GetWeakPtr()`.
  base::WeakPtrFactory<HistoryClustersSidePanelUI> weak_ptr_factory_{this};

  WEB_UI_CONTROLLER_TYPE_DECL();
};

#endif  // CHROME_BROWSER_UI_WEBUI_SIDE_PANEL_HISTORY_CLUSTERS_HISTORY_CLUSTERS_SIDE_PANEL_UI_H_
