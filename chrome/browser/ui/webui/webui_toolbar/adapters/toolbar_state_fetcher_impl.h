// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_WEBUI_WEBUI_TOOLBAR_ADAPTERS_TOOLBAR_STATE_FETCHER_IMPL_H_
#define CHROME_BROWSER_UI_WEBUI_WEBUI_TOOLBAR_ADAPTERS_TOOLBAR_STATE_FETCHER_IMPL_H_

#include "base/functional/callback.h"
#include "chrome/browser/ui/webui/webui_toolbar/adapters/toolbar_state_fetcher.h"

namespace toolbar_ui_api {

// State fetcher using a simple repeating callback.
class ToolbarStateFetcherImpl : public ToolbarStateFetcher {
 public:
  using CallbackType = base::RepeatingCallback<mojom::ToolbarStatePtr()>;

  explicit ToolbarStateFetcherImpl(CallbackType state_fetcher);
  ~ToolbarStateFetcherImpl() override;

  mojom::ToolbarStatePtr GetToolbarState() override;

 private:
  CallbackType state_fetcher_;
};

}  // namespace toolbar_ui_api

#endif  // CHROME_BROWSER_UI_WEBUI_WEBUI_TOOLBAR_ADAPTERS_TOOLBAR_STATE_FETCHER_IMPL_H_
