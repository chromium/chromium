// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_VIEWS_TABS_SHARED_NEW_TAB_BUTTON_H_
#define CHROME_BROWSER_UI_VIEWS_TABS_SHARED_NEW_TAB_BUTTON_H_

#include <memory>

#include "base/memory/raw_ptr.h"
#include "chrome/browser/ui/views/tabs/shared/tab_strip_flat_edge_button.h"
#include "ui/base/metadata/metadata_header_macros.h"
#include "ui/events/event.h"

class BrowserWindowInterface;

namespace views {
class ActionViewController;
}  // namespace views

namespace shared {

class NewTabButton : public TabStripFlatEdgeButton {
  METADATA_HEADER(NewTabButton, TabStripFlatEdgeButton)
 public:
  NewTabButton(BrowserWindowInterface* browser,
               const int button_size,
               const int icon_size);
  ~NewTabButton() override;

  // views::View:
  void OnMouseEvent(ui::MouseEvent* event) override;

 private:
  std::unique_ptr<views::ActionViewController> action_view_controller_;

  raw_ptr<BrowserWindowInterface> browser_;
};

}  // namespace shared

#endif  // CHROME_BROWSER_UI_VIEWS_TABS_SHARED_NEW_TAB_BUTTON_H_
