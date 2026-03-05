// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_WEBUI_WEBUI_TOOLBAR_ADAPTERS_BROWSER_CONTROLS_ADAPTER_H_
#define CHROME_BROWSER_UI_WEBUI_WEBUI_TOOLBAR_ADAPTERS_BROWSER_CONTROLS_ADAPTER_H_

#include "chrome/browser/ui/webui/webui_toolbar/utils/split_tabs_utils.h"
#include "ui/base/window_open_disposition.h"

namespace browser_controls_api {

// Abstracts the underlying platform dependencies. The adapters translates
// requests from the browser controls service to underlying platform calls. The
// adapter may be overwritten in test, to provide a controlled and predictable
// environment under test.
class BrowserControlsAdapter {
 public:
  virtual ~BrowserControlsAdapter() = default;

  virtual void Reload(bool bypass_cache, WindowOpenDisposition disposition) = 0;
  virtual void Stop() = 0;
  virtual void CreateNewSplitTab() = 0;
  // These should probably be pulled to their own adapter.
  virtual webui_toolbar::TabSplitStatus ComputeSplitTabStatus() = 0;
  virtual bool IsButtonPinned(
      toolbar_ui_api::mojom::ToolbarButtonType type) = 0;
};

}  // namespace browser_controls_api

#endif  // CHROME_BROWSER_UI_WEBUI_WEBUI_TOOLBAR_ADAPTERS_BROWSER_CONTROLS_ADAPTER_H_
