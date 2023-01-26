// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_WEBUI_SIDE_PANEL_SEARCH_COMPANION_SEARCH_COMPANION_PAGE_HANDLER_H_
#define CHROME_BROWSER_UI_WEBUI_SIDE_PANEL_SEARCH_COMPANION_SEARCH_COMPANION_PAGE_HANDLER_H_

#include "base/memory/raw_ptr.h"
#include "chrome/browser/ui/webui/side_panel/search_companion/search_companion.mojom.h"
#include "mojo/public/cpp/bindings/pending_receiver.h"
#include "mojo/public/cpp/bindings/receiver.h"

class SearchCompanionSidePanelUI;

///////////////////////////////////////////////////////////////////////////////
// SearchCompanionPageHandler
//
//  A handler of the Search Companion side panel WebUI (based on Polymer).
//  (chrome/browser/resources/side_panel/search_companion/app.ts).
//  This class is created and owned by SearchCompanionUI and has the same
//  lifetime as the Side Panel view.
//
class SearchCompanionPageHandler
    : public side_panel::mojom::SearchCompanionPageHandler {
 public:
  explicit SearchCompanionPageHandler(
      mojo::PendingReceiver<side_panel::mojom::SearchCompanionPageHandler>
          receiver,
      SearchCompanionSidePanelUI* search_companion_ui);
  SearchCompanionPageHandler(const SearchCompanionPageHandler&) = delete;
  SearchCompanionPageHandler& operator=(const SearchCompanionPageHandler&) =
      delete;
  ~SearchCompanionPageHandler() override;

  // side_panel::mojom::SearchCompanionPageHandler:
  void ShowUI() override;

 private:
  mojo::Receiver<side_panel::mojom::SearchCompanionPageHandler> receiver_;
  raw_ptr<SearchCompanionSidePanelUI> search_companion_ui_ = nullptr;
};

#endif  // CHROME_BROWSER_UI_WEBUI_SIDE_PANEL_SEARCH_COMPANION_SEARCH_COMPANION_PAGE_HANDLER_H_
