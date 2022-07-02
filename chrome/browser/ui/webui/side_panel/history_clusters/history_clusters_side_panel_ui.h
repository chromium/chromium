// Copyright 2022 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_WEBUI_SIDE_PANEL_HISTORY_CLUSTERS_HISTORY_CLUSTERS_SIDE_PANEL_UI_H_
#define CHROME_BROWSER_UI_WEBUI_SIDE_PANEL_HISTORY_CLUSTERS_HISTORY_CLUSTERS_SIDE_PANEL_UI_H_

#include <memory>

#include "mojo/public/cpp/bindings/pending_receiver.h"
#include "ui/webui/mojo_bubble_web_ui_controller.h"
#include "ui/webui/resources/cr_components/history_clusters/history_clusters.mojom-forward.h"

namespace history_clusters {
class HistoryClustersHandler;
}

class HistoryClustersSidePanelUI : public ui::MojoBubbleWebUIController {
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

 private:
  std::unique_ptr<history_clusters::HistoryClustersHandler>
      history_clusters_handler_;

  WEB_UI_CONTROLLER_TYPE_DECL();
};

#endif  // CHROME_BROWSER_UI_WEBUI_SIDE_PANEL_HISTORY_CLUSTERS_HISTORY_CLUSTERS_SIDE_PANEL_UI_H_
