// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_VIEWS_TOOLBAR_WEBUI_OVERFLOW_BUTTON_H_
#define CHROME_BROWSER_UI_VIEWS_TOOLBAR_WEBUI_OVERFLOW_BUTTON_H_

#include <map>
#include <memory>
#include <string>
#include <variant>
#include <vector>

#include "base/memory/raw_ptr.h"
#include "chrome/browser/ui/views/toolbar/overflow_menu.h"
#include "components/browser_apis/ui_controllers/toolbar/toolbar_ui_api.mojom.h"
#include "components/browser_apis/ui_controllers/toolbar/toolbar_ui_api_data_model.mojom.h"
#include "ui/actions/action_id.h"
#include "ui/base/interaction/element_identifier.h"
#include "ui/base/mojom/menu_source_type.mojom.h"
#include "ui/gfx/geometry/rect.h"

class WebUIToolbarControlDelegate;

// WebUIOverflowButton manages the C++-side logic around showing the toolbar's
// overflow menu. The WebUI renderer tells it to show a menu, along with what
// buttons should be shown. It then creates and shows an OverflowMenu with the
// requested menu items. If a menu item is clicked, the WebUIOverflowButton
// handles delegating the requested action.
class WebUIOverflowButton : public OverflowMenu::Delegate {
 public:
  WebUIOverflowButton(WebUIToolbarControlDelegate* delegate,
                      OverflowMenu::PinnedActionsInfo* pinned_actions_info);
  WebUIOverflowButton(const WebUIOverflowButton&) = delete;
  WebUIOverflowButton& operator=(const WebUIOverflowButton&) = delete;
  ~WebUIOverflowButton() override;

  void ShowOverflowMenu(
      const std::vector<toolbar_ui_api::mojom::OverflowMenuItemPtr>& controls,
      const gfx::Rect& screen_rect,
      ui::mojom::MenuSourceType source,
      toolbar_ui_api::mojom::ToolbarUIService::ShowOverflowMenuCallback
          callback);

  OverflowMenu* overflow_menu_for_testing() { return overflow_menu_.get(); }

  // OverflowMenu::Delegate:
  void ExecuteCommand(
      const OverflowMenu::OverflowableElement& element) override;
  bool IsCurrentlyOverflowed(
      const OverflowMenu::OverflowableElement& element) const override;
  bool IsEnabled(
      const OverflowMenu::OverflowableElement& element) const override;
  void OnMenuClosed() override;

 private:
  void UpdateState();

  // Represents information about an element that has overflowed.
  struct OverflowedElementInfo {
    // True if the control is enabled / not greyed out.
    bool is_enabled = true;
  };

  // Similar to OverflowableElement, but only contains the underlying ID and can
  // be used in a map.
  using OverflowableElementId =
      std::variant<ui::ElementIdentifier, actions::ActionId>;

  static OverflowableElementId OverflowableElementInfoToId(
      const OverflowMenu::OverflowableElement& element);

  raw_ptr<WebUIToolbarControlDelegate> delegate_;
  raw_ptr<OverflowMenu::PinnedActionsInfo> pinned_actions_info_;

  // The last displayed overflow menu. It's only destroyed when we try to show a
  // new one, even if the menu is no longer visible. There's no callback
  // currently to know when it's destroyed. It's safe to keep around.
  std::unique_ptr<OverflowMenu> overflow_menu_;
  std::map<OverflowableElementId, OverflowedElementInfo> overflowed_elements_;
};

#endif  // CHROME_BROWSER_UI_VIEWS_TOOLBAR_WEBUI_OVERFLOW_BUTTON_H_
