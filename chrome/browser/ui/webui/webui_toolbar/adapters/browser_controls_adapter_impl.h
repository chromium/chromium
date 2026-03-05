// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_WEBUI_WEBUI_TOOLBAR_ADAPTERS_BROWSER_CONTROLS_ADAPTER_IMPL_H_
#define CHROME_BROWSER_UI_WEBUI_WEBUI_TOOLBAR_ADAPTERS_BROWSER_CONTROLS_ADAPTER_IMPL_H_

#include "base/memory/raw_ref.h"
#include "chrome/browser/ui/webui/webui_toolbar/adapters/browser_controls_adapter.h"

class CommandUpdater;
class BrowserWindowInterface;

namespace browser_controls_api {

// Adapter implementation for the desktop platform.
class BrowserControlsAdapterImpl : public BrowserControlsAdapter {
 public:
  BrowserControlsAdapterImpl(BrowserWindowInterface* browser_interface,
                             CommandUpdater* command_updater);
  BrowserControlsAdapterImpl(const BrowserControlsAdapterImpl&&) = delete;
  BrowserControlsAdapterImpl operator=(const BrowserControlsAdapterImpl&&) =
      delete;
  ~BrowserControlsAdapterImpl() override;

  // BrowserControlsAdapter:
  void Reload(bool bypass_cache, WindowOpenDisposition disposition) override;
  void Stop() override;
  void CreateNewSplitTab() override;
  webui_toolbar::TabSplitStatus ComputeSplitTabStatus() override;
  bool IsButtonPinned(toolbar_ui_api::mojom::ToolbarButtonType type) override;

 private:
  // Not owned.
  const base::raw_ref<BrowserWindowInterface> browser_;
  const base::raw_ref<CommandUpdater> command_updater_;
};

}  // namespace browser_controls_api

#endif  // CHROME_BROWSER_UI_WEBUI_WEBUI_TOOLBAR_ADAPTERS_BROWSER_CONTROLS_ADAPTER_IMPL_H_
