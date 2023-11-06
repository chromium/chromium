// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_WEBUI_TAB_SEARCH_TAB_SEARCH_UI_H_
#define CHROME_BROWSER_UI_WEBUI_TAB_SEARCH_TAB_SEARCH_UI_H_

#include <memory>

#include "base/timer/elapsed_timer.h"
#include "chrome/browser/ui/webui/metrics_reporter/metrics_reporter.h"
#include "chrome/browser/ui/webui/tab_search/tab_search.mojom.h"
#include "chrome/browser/ui/webui/tab_search/tab_search_page_handler.h"
#include "chrome/browser/ui/webui/webui_load_timer.h"
#include "mojo/public/cpp/bindings/pending_receiver.h"
#include "mojo/public/cpp/bindings/pending_remote.h"
#include "mojo/public/cpp/bindings/receiver.h"
#include "ui/webui/mojo_bubble_web_ui_controller.h"
#include "ui/webui/resources/cr_components/color_change_listener/color_change_listener.mojom.h"
#include "ui/webui/resources/js/metrics_reporter/metrics_reporter.mojom.h"

namespace ui {
class ColorChangeHandler;
}

class TabSearchUI : public ui::MojoBubbleWebUIController,
                    public tab_search::mojom::PageHandlerFactory {
 public:
  explicit TabSearchUI(content::WebUI* web_ui);
  TabSearchUI(const TabSearchUI&) = delete;
  TabSearchUI& operator=(const TabSearchUI&) = delete;
  ~TabSearchUI() override;

  void BindInterface(
      mojo::PendingReceiver<color_change_listener::mojom::PageHandler>
          pending_receiver);

  // Instantiates the implementor of the mojom::PageHandlerFactory mojo
  // interface passing the pending receiver that will be internally bound.
  void BindInterface(
      mojo::PendingReceiver<tab_search::mojom::PageHandlerFactory> receiver);
  void BindInterface(
      mojo::PendingReceiver<metrics_reporter::mojom::PageMetricsHost> receiver);

  TabSearchPageHandler* page_handler_for_testing() {
    return page_handler_.get();
  }

 private:
  // tab_search::mojom::PageHandlerFactory
  void CreatePageHandler(
      mojo::PendingRemote<tab_search::mojom::Page> page,
      mojo::PendingReceiver<tab_search::mojom::PageHandler> receiver) override;

  bool ShowTabOrganizationFRE();
  int TabIndex();

  std::unique_ptr<ui::ColorChangeHandler> color_provider_handler_;
  std::unique_ptr<TabSearchPageHandler> page_handler_;
  MetricsReporter metrics_reporter_;

  mojo::Receiver<tab_search::mojom::PageHandlerFactory> page_factory_receiver_{
      this};

  WebuiLoadTimer webui_load_timer_;

  // A timer used to track the duration between when the WebUI is constructed
  // and when the TabSearchPageHandler is constructed.
  absl::optional<base::ElapsedTimer> page_handler_timer_;

  WEB_UI_CONTROLLER_TYPE_DECL();
};

#endif  // CHROME_BROWSER_UI_WEBUI_TAB_SEARCH_TAB_SEARCH_UI_H_
