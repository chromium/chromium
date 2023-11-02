// Copyright 2015 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_WEBUI_HISTORY_HISTORY_UI_H_
#define CHROME_BROWSER_UI_WEBUI_HISTORY_HISTORY_UI_H_

#include <memory>

#include "base/gtest_prod_util.h"
#include "components/prefs/pref_change_registrar.h"
#include "mojo/public/cpp/bindings/pending_receiver.h"
#include "ui/base/layout.h"
#include "ui/webui/mojo_web_ui_controller.h"
#include "ui/webui/resources/cr_components/history_clusters/history_clusters.mojom-forward.h"

namespace base {
class RefCountedMemory;
}

namespace history_clusters {
class HistoryClustersHandler;
}

class HistoryUI : public ui::MojoWebUIController {
 public:
  explicit HistoryUI(content::WebUI* web_ui);
  HistoryUI(const HistoryUI&) = delete;
  HistoryUI& operator=(const HistoryUI&) = delete;
  ~HistoryUI() override;

  static base::RefCountedMemory* GetFaviconResourceBytes(
      ui::ResourceScaleFactor scale_factor);

  // Instantiates the implementor of the history_clusters::mojom::PageHandler
  // mojo interface passing to it the pending receiver that will be internally
  // bound.
  void BindInterface(mojo::PendingReceiver<history_clusters::mojom::PageHandler>
                         pending_page_handler);

  // For testing only.
  history_clusters::HistoryClustersHandler*
  GetHistoryClustersHandlerForTesting() {
    return history_clusters_handler_.get();
  }

 private:
  std::unique_ptr<history_clusters::HistoryClustersHandler>
      history_clusters_handler_;
  PrefChangeRegistrar pref_change_registrar_;

  void UpdateDataSource();

  WEB_UI_CONTROLLER_TYPE_DECL();
};

#endif  // CHROME_BROWSER_UI_WEBUI_HISTORY_HISTORY_UI_H_
