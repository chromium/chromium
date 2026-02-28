// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/webui/webui_toolbar/testing/toy_browser.h"

#include "chrome/browser/ui/webui/webui_toolbar/adapters/browser_controls_adapter.h"
#include "chrome/browser/ui/webui/webui_toolbar/utils/split_tabs_utils.h"

namespace browser_controls_api::testing {

class ToyBrowserControlsAdapter : public BrowserControlsAdapter {
 public:
  explicit ToyBrowserControlsAdapter(ToyBrowser* toy_browser)
      : toy_browser_(toy_browser) {}

  void Reload(bool bypass_cache, WindowOpenDisposition disposition) override {
    toy_browser_->received_commands_.push_back({
        .command_id = bypass_cache ? IDC_RELOAD_BYPASSING_CACHE : IDC_RELOAD,
        .disposition = disposition,
    });
  }

  void Stop() override {
    toy_browser_->received_commands_.push_back(
        {.command_id = IDC_STOP,
         .disposition = WindowOpenDisposition::CURRENT_TAB});
  }

  void CreateNewSplitTab() override { toy_browser_->is_split_tab_ = true; }

  webui_toolbar::TabSplitStatus ComputeSplitTabStatus() override {
    webui_toolbar::TabSplitStatus status;

    if (toy_browser_->is_split_tab_) {
      status.is_split = true;
    }

    return status;
  }

  bool IsButtonPinned(toolbar_ui_api::mojom::ToolbarButtonType type) override {
    return toy_browser_->IsButtonPinned(type);
  }

 private:
  raw_ptr<ToyBrowser> toy_browser_;
};

std::unique_ptr<BrowserControlsAdapter> ToyBrowser::GetAdapter() {
  return std::make_unique<ToyBrowserControlsAdapter>(this);
}

void ToyBrowser::PinButton(toolbar_ui_api::mojom::ToolbarButtonType type) {
  pinned_buttons_.insert(type);
}

void ToyBrowser::UnpinButton(toolbar_ui_api::mojom::ToolbarButtonType type) {
  auto found = pinned_buttons_.find(type);
  if (found != pinned_buttons_.end()) {
    pinned_buttons_.erase(found);
  }
}

bool ToyBrowser::IsButtonPinned(
    toolbar_ui_api::mojom::ToolbarButtonType type) const {
  return pinned_buttons_.contains(type);
}

}  // namespace browser_controls_api::testing
