// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_VIEWS_TOOLBAR_WEBUI_SPLIT_TABS_CONTROL_H_
#define CHROME_BROWSER_UI_VIEWS_TOOLBAR_WEBUI_SPLIT_TABS_CONTROL_H_

#include "base/memory/raw_ptr.h"
#include "chrome/browser/ui/tabs/tab_strip_model_observer.h"
#include "chrome/browser/ui/webui/webui_toolbar/utils/split_tabs_utils.h"
#include "components/browser_apis/browser_controls/browser_controls_api.mojom.h"
#include "components/browser_apis/ui_controllers/toolbar/toolbar_ui_api_data_model.mojom.h"
#include "components/prefs/pref_member.h"
#include "ui/base/models/menu_model.h"
#include "ui/base/mojom/menu_source_type.mojom.h"
#include "ui/views/controls/menu/menu_runner.h"

class WebUIToolbarWebView;

// WebUISplitTabsControl implements C++-side functionality for the WebUI-based
// implementation of the split tabs button in the toolbar.
class WebUISplitTabsControl : public TabStripModelObserver {
 public:
  explicit WebUISplitTabsControl(WebUIToolbarWebView* toolbar_view);
  WebUISplitTabsControl(const WebUISplitTabsControl&) = delete;
  WebUISplitTabsControl& operator=(const WebUISplitTabsControl&) = delete;
  ~WebUISplitTabsControl() override;

  // Initializes the control. Should be called when the parent view is added to
  // the widget.
  void Init();

  // Returns true if the split tabs button should be visible.
  bool IsVisible() const;

  // Handles context menu requests from the WebUI.
  void HandleContextMenu(toolbar_ui_api::mojom::ContextMenuType menu_type,
                         const gfx::Point& screen_location,
                         ui::mojom::MenuSourceType source);

  // TabStripModelObserver:
  void OnTabStripModelChanged(
      TabStripModel* tab_strip_model,
      const TabStripModelChange& change,
      const TabStripSelectionChange& selection) override;
  void OnSplitTabChanged(const SplitTabChange& change) override;

 private:
  FRIEND_TEST_ALL_PREFIXES(WebUIToolbarWebViewPixelBrowserTest,
                           CheckSplitTabsButtonColor);
  FRIEND_TEST_ALL_PREFIXES(WebUIToolbarWebViewSplitTabsBrowserTest,
                           CheckSplitTabsButtonSourceType);

  void UpdateVisibility(
      const toolbar_ui_api::mojom::SplitTabsControlState* state);
  void UpdateState();
  void RunMenuAt(int x, int y, ui::mojom::MenuSourceType source_type);

  raw_ptr<WebUIToolbarWebView> toolbar_view_;
  BooleanPrefMember pin_state_;
  bool is_visible_ = false;

  toolbar_ui_api::mojom::ContextMenuType current_menu_type_ =
      toolbar_ui_api::mojom::ContextMenuType::kUnspecified;
  ui::mojom::MenuSourceType last_source_type_for_testing_ =
      ui::mojom::MenuSourceType::kNone;

  std::unique_ptr<ui::MenuModel> split_tab_menu_;
  std::unique_ptr<views::MenuRunner> menu_runner_;
};

#endif  // CHROME_BROWSER_UI_VIEWS_TOOLBAR_WEBUI_SPLIT_TABS_CONTROL_H_
