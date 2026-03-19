// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_VIEWS_TOOLBAR_WEBUI_HOME_CONTROL_H_
#define CHROME_BROWSER_UI_VIEWS_TOOLBAR_WEBUI_HOME_CONTROL_H_

#include "base/gtest_prod_util.h"
#include "base/memory/raw_ptr.h"
#include "chrome/browser/ui/views/toolbar/pinned_action_toolbar_button_menu_model.h"
#include "components/browser_apis/browser_controls/browser_controls_api.mojom.h"
#include "components/browser_apis/ui_controllers/toolbar/toolbar_ui_api_data_model.mojom.h"
#include "components/prefs/pref_member.h"
#include "ui/base/models/menu_model.h"
#include "ui/base/mojom/menu_source_type.mojom.h"
#include "ui/gfx/geometry/point.h"
#include "ui/views/controls/menu/menu_runner.h"

class WebUIToolbarWebView;

// WebUIHomeControl implements C++-side functionality for the WebUI-based
// implementation of the home button in the toolbar.
class WebUIHomeControl {
 public:
  explicit WebUIHomeControl(WebUIToolbarWebView* webui_toolbar_web_view);
  WebUIHomeControl(const WebUIHomeControl&) = delete;
  WebUIHomeControl& operator=(const WebUIHomeControl&) = delete;
  ~WebUIHomeControl();

  // Initializes the control. Should be called when the parent view is added to
  // the widget.
  void Init();

  // Returns true if the home button should be visible.
  bool IsVisible() const;

  // Handles context menu requests from the WebUI.
  void HandleContextMenu(const gfx::Point& screen_location,
                         ui::mojom::MenuSourceType source);

 private:
  FRIEND_TEST_ALL_PREFIXES(WebUIToolbarWebViewPixelBrowserTest,
                           CheckHomeButtonColor);
  FRIEND_TEST_ALL_PREFIXES(WebUIToolbarWebViewHomeButtonBrowserTest,
                           RightClickHomeButton);
  FRIEND_TEST_ALL_PREFIXES(WebUIToolbarWebViewHomeButtonBrowserTest,
                           LongPressHomeButton);
  void UpdateVisibility(const toolbar_ui_api::mojom::HomeControlState* state);
  void UpdateState();

  raw_ptr<WebUIToolbarWebView> webui_toolbar_web_view_;
  BooleanPrefMember pin_state_;
  bool is_visible_ = false;

  ui::mojom::MenuSourceType last_source_type_for_testing_ =
      ui::mojom::MenuSourceType::kNone;

  PinnedActionToolbarButtonMenuModel home_menu_;
  std::unique_ptr<views::MenuRunner> menu_runner_;
};

#endif  // CHROME_BROWSER_UI_VIEWS_TOOLBAR_WEBUI_HOME_CONTROL_H_
