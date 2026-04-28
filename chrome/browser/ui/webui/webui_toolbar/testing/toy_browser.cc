// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/webui/webui_toolbar/testing/toy_browser.h"

#include "chrome/browser/ui/webui/webui_toolbar/adapters/browser_controls_adapter.h"
#include "chrome/browser/ui/webui/webui_toolbar/utils/split_tabs_utils.h"
#include "chrome/browser/ui/webui/webui_toolbar/utils/toolbar_button_utils.h"

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

  void Back(WindowOpenDisposition disposition) override {
    toy_browser_->received_commands_.push_back({
        .command_id = IDC_BACK,
        .disposition = disposition,
    });
  }

  void Forward(WindowOpenDisposition disposition) override {
    toy_browser_->received_commands_.push_back({
        .command_id = IDC_FORWARD,
        .disposition = disposition,
    });
  }

  void BackButtonHovered() override {
    toy_browser_->back_button_hovered_ = true;
  }

  void CreateNewSplitTab() override { toy_browser_->is_split_tab_ = true; }

  void NavigateHome(WindowOpenDisposition disposition) override {
    toy_browser_->received_commands_.push_back(
        {.command_id = IDC_HOME, .disposition = disposition});
  }

  webui_toolbar::TabSplitStatus ComputeSplitTabStatus() override {
    webui_toolbar::TabSplitStatus status;

    if (toy_browser_->is_split_tab_) {
      status.is_split = true;
    }

    return status;
  }

 private:
  raw_ptr<ToyBrowser> toy_browser_;
};

std::unique_ptr<BrowserControlsAdapter> ToyBrowser::GetAdapter() {
  return std::make_unique<ToyBrowserControlsAdapter>(this);
}

}  // namespace browser_controls_api::testing
