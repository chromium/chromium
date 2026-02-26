// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_WEBUI_WEBUI_TOOLBAR_ADAPTERS_NAVIGATION_CONTROLS_STATE_FETCHER_H_
#define CHROME_BROWSER_UI_WEBUI_WEBUI_TOOLBAR_ADAPTERS_NAVIGATION_CONTROLS_STATE_FETCHER_H_

#include "components/browser_apis/browser_controls/browser_controls_api.mojom.h"

namespace browser_controls_api {

// An adapter which fetches and combines the current toolbar control states.
class NavigationControlsStateFetcher {
 public:
  virtual ~NavigationControlsStateFetcher() = default;

  virtual mojom::NavigationControlsStatePtr GetNavigationControlsState() = 0;
};

}  // namespace browser_controls_api

#endif  // CHROME_BROWSER_UI_WEBUI_WEBUI_TOOLBAR_ADAPTERS_NAVIGATION_CONTROLS_STATE_FETCHER_H_
