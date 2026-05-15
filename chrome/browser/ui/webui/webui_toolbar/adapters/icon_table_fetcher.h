// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_WEBUI_WEBUI_TOOLBAR_ADAPTERS_ICON_TABLE_FETCHER_H_
#define CHROME_BROWSER_UI_WEBUI_WEBUI_TOOLBAR_ADAPTERS_ICON_TABLE_FETCHER_H_

#include <vector>

#include "components/browser_apis/ui_controllers/toolbar/toolbar_ui_api_data_model.mojom.h"

namespace toolbar_ui_api {

// An adapter for managing updates to icon tables.
class IconTableFetcher {
 public:
  virtual ~IconTableFetcher() = default;

  // Gets the entire state of the current icon table.
  virtual std::vector<toolbar_ui_api::mojom::IconUpdatePtr> GetFullState() = 0;

  // Gets changes since the last time TakePendingUpdates() was called.
  virtual std::vector<toolbar_ui_api::mojom::IconUpdatePtr>
  TakePendingUpdates() = 0;
};

}  // namespace toolbar_ui_api

#endif  // CHROME_BROWSER_UI_WEBUI_WEBUI_TOOLBAR_ADAPTERS_ICON_TABLE_FETCHER_H_
