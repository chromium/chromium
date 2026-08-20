// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_VIEWS_TABS_SHARED_NEW_TAB_BUTTON_H_
#define CHROME_BROWSER_UI_VIEWS_TABS_SHARED_NEW_TAB_BUTTON_H_

#include <memory>
#include <optional>

#include "base/functional/callback.h"
#include "base/memory/raw_ptr.h"
#include "chrome/browser/ui/views/tabs/shared/tab_strip_flat_edge_button.h"
#include "ui/base/metadata/metadata_header_macros.h"
#include "ui/events/event.h"
#include "ui/views/context_menu_controller.h"

class BrowserWindowInterface;
class NewTabButtonMenuModel;

namespace views {
class ActionViewController;
class MenuRunner;
}  // namespace views

namespace shared {

class NewTabButton : public TabStripFlatEdgeButton,
                     public views::ContextMenuController {
  METADATA_HEADER(NewTabButton, TabStripFlatEdgeButton)
 public:
  NewTabButton(BrowserWindowInterface* browser,
               const int button_size,
               const int icon_size,
               std::optional<float> corner_radius = std::nullopt);
  ~NewTabButton() override;

  void SetOnContextMenuWillShowCallback(base::RepeatingClosure callback);
  void SetOnContextMenuClosedCallback(base::RepeatingClosure callback);
  void SetMiddleClickCallbackForTesting(base::RepeatingClosure callback);

  // views::ContextMenuController:
  void ShowContextMenuForViewImpl(
      views::View* source,
      const gfx::Point& point,
      ui::mojom::MenuSourceType source_type) override;

 protected:
  // views::Button:
  void NotifyClick(const ui::Event& event) override;

 private:
  void OnContextMenuClosed();

  std::unique_ptr<views::ActionViewController> action_view_controller_;
  std::unique_ptr<NewTabButtonMenuModel> context_menu_model_;
  std::unique_ptr<views::MenuRunner> context_menu_runner_;

  base::RepeatingClosure on_context_menu_will_show_callback_;
  base::RepeatingClosure on_context_menu_closed_callback_;
  base::RepeatingClosure middle_click_callback_for_testing_;

  raw_ptr<BrowserWindowInterface> browser_;
};

}  // namespace shared

#endif  // CHROME_BROWSER_UI_VIEWS_TABS_SHARED_NEW_TAB_BUTTON_H_
