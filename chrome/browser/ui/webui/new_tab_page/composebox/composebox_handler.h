// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_WEBUI_NEW_TAB_PAGE_COMPOSEBOX_COMPOSEBOX_HANDLER_H_
#define CHROME_BROWSER_UI_WEBUI_NEW_TAB_PAGE_COMPOSEBOX_COMPOSEBOX_HANDLER_H_

#include "chrome/browser/ui/webui/new_tab_page/composebox/composebox.mojom.h"
#include "components/omnibox/composebox/composebox_query_controller.h"
#include "mojo/public/cpp/bindings/pending_receiver.h"
#include "mojo/public/cpp/bindings/receiver.h"

class ComposeboxHandler : public composebox::mojom::ComposeboxPageHandler {
 public:
  explicit ComposeboxHandler(
      mojo::PendingReceiver<composebox::mojom::ComposeboxPageHandler> handler,
      std::unique_ptr<ComposeboxQueryController> query_controller);
  ~ComposeboxHandler() override;

  // composebox::mojom::ComposeboxPageHandler:
  void NotifySessionStarted() override;
  void NotifySessionAbandoned() override;

 private:
  mojo::Receiver<composebox::mojom::ComposeboxPageHandler> handler_;
  std::unique_ptr<ComposeboxQueryController> query_controller_;
};

#endif  // CHROME_BROWSER_UI_WEBUI_NEW_TAB_PAGE_COMPOSEBOX_COMPOSEBOX_HANDLER_H_
