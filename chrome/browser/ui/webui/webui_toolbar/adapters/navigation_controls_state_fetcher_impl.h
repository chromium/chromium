// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_WEBUI_WEBUI_TOOLBAR_ADAPTERS_NAVIGATION_CONTROLS_STATE_FETCHER_IMPL_H_
#define CHROME_BROWSER_UI_WEBUI_WEBUI_TOOLBAR_ADAPTERS_NAVIGATION_CONTROLS_STATE_FETCHER_IMPL_H_

#include "base/functional/callback.h"
#include "chrome/browser/ui/webui/webui_toolbar/adapters/navigation_controls_state_fetcher.h"

namespace toolbar_ui_api {

// State fetcher using a simple repeating callback.
class NavigationControlsStateFetcherImpl
    : public NavigationControlsStateFetcher {
 public:
  using CallbackType =
      base::RepeatingCallback<mojom::NavigationControlsStatePtr()>;

  explicit NavigationControlsStateFetcherImpl(CallbackType state_fetcher);
  ~NavigationControlsStateFetcherImpl() override;

  mojom::NavigationControlsStatePtr GetNavigationControlsState() override;

 private:
  CallbackType state_fetcher_;
};

}  // namespace toolbar_ui_api

#endif  // CHROME_BROWSER_UI_WEBUI_WEBUI_TOOLBAR_ADAPTERS_NAVIGATION_CONTROLS_STATE_FETCHER_IMPL_H_
