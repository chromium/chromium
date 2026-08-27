// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_WEBUI_TAB_SEARCH_SEARCH_HANDLER_H_
#define CHROME_BROWSER_UI_WEBUI_TAB_SEARCH_SEARCH_HANDLER_H_

#include <string>
#include <vector>

#include "chrome/browser/ui/webui/tab_search/tab_search.mojom.h"
#include "mojo/public/cpp/bindings/pending_receiver.h"
#include "mojo/public/cpp/bindings/receiver.h"

class SearchHandler : public tab_search::mojom::SearchHandler {
 public:
  explicit SearchHandler(
      mojo::PendingReceiver<tab_search::mojom::SearchHandler> receiver);
  SearchHandler(const SearchHandler&) = delete;
  SearchHandler& operator=(const SearchHandler&) = delete;
  ~SearchHandler() override;

  // tab_search::mojom::SearchHandler:
  void GetRangesIgnoringCaseAndAccents(
      const std::string& search_text,
      const std::vector<std::string>& targets,
      GetRangesIgnoringCaseAndAccentsCallback callback) override;

 private:
  mojo::Receiver<tab_search::mojom::SearchHandler> receiver_;
};

#endif  // CHROME_BROWSER_UI_WEBUI_TAB_SEARCH_SEARCH_HANDLER_H_
