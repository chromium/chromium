// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_WEBUI_WEBUI_TOOLBAR_TESTING_TOY_BROWSER_H_
#define CHROME_BROWSER_UI_WEBUI_WEBUI_TOOLBAR_TESTING_TOY_BROWSER_H_

#include <memory>
#include <set>
#include <vector>

#include "chrome/app/chrome_command_ids.h"
#include "components/browser_apis/browser_controls/browser_controls_api_data_model.mojom.h"
#include "components/browser_apis/ui_controllers/toolbar/toolbar_ui_api_data_model.mojom.h"
#include "ui/base/window_open_disposition.h"

namespace browser_controls_api {

class BrowserControlsAdapter;

namespace testing {

struct ToyBrowserCommand {
  int command_id;
  WindowOpenDisposition disposition;
};

class ToyBrowserControlsAdapter;

// A very very simple browser which can be easily interrogated under test.
class ToyBrowser {
 public:
  ToyBrowser() = default;
  ToyBrowser(const ToyBrowser&&) = delete;
  ToyBrowser operator=(const ToyBrowser&) = delete;
  ~ToyBrowser() = default;

  // Retrieves a BrowserControlsAdapter which interops with the toy browser.
  std::unique_ptr<BrowserControlsAdapter> GetAdapter();

  const std::vector<ToyBrowserCommand>& received_commands() const {
    return received_commands_;
  }

  // Noop if the pin state doesn't change.
  void PinButton(toolbar_ui_api::mojom::ToolbarButtonType type);
  void UnpinButton(toolbar_ui_api::mojom::ToolbarButtonType type);
  bool IsButtonPinned(toolbar_ui_api::mojom::ToolbarButtonType type) const;

  bool is_split_tab() const { return is_split_tab_; }

 private:
  friend class ToyBrowserControlsAdapter;
  std::vector<ToyBrowserCommand> received_commands_;
  std::set<toolbar_ui_api::mojom::ToolbarButtonType> pinned_buttons_;
  // True when split tab is created. This state currently sticks, with no way
  // to unset it.
  bool is_split_tab_ = false;
};

}  // namespace testing
}  // namespace browser_controls_api

#endif  // CHROME_BROWSER_UI_WEBUI_WEBUI_TOOLBAR_TESTING_TOY_BROWSER_H_
