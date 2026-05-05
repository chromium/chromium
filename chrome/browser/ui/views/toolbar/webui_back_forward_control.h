// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_VIEWS_TOOLBAR_WEBUI_BACK_FORWARD_CONTROL_H_
#define CHROME_BROWSER_UI_VIEWS_TOOLBAR_WEBUI_BACK_FORWARD_CONTROL_H_

#include "base/memory/raw_ptr.h"
#include "chrome/browser/ui/toolbar/back_forward_menu_model.h"
#include "chrome/browser/ui/views/toolbar/back_forward_button.h"
#include "components/browser_apis/browser_controls/browser_controls_api_data_model.mojom.h"
#include "components/browser_apis/ui_controllers/toolbar/toolbar_ui_api_data_model.mojom-forward.h"
#include "ui/base/mojom/menu_source_type.mojom-forward.h"
#include "ui/gfx/geometry/rect.h"
#include "ui/views/controls/menu/menu_model_adapter.h"
#include "ui/views/controls/menu/menu_runner.h"

namespace views {
class Widget;
}  // namespace views

class WebUIToolbarWebView;

// A WebUI-based implementation of the Back/Forward control.
// This class manages the communication with the WebUI via Mojo.
class WebUIBackForwardControl {
 public:
  WebUIBackForwardControl(WebUIToolbarWebView* webui_toolbar_web_view,
                          BackForwardButton::Direction direction);
  WebUIBackForwardControl(const WebUIBackForwardControl&) = delete;
  WebUIBackForwardControl& operator=(const WebUIBackForwardControl&) = delete;
  ~WebUIBackForwardControl();

  void HandleContextMenu(views::Widget* widget,
                         const gfx::Rect& screen_rect,
                         ui::mojom::MenuSourceType source);

  void SetEnabled(bool enabled);
  void SetIsPinned(bool is_pinned);
  // Returns true if the home button is pinned, and so should be shown if
  // there's enough room for it on the toolbar. Always returns true for the back
  // button.
  bool IsPinned() const;

  toolbar_ui_api::mojom::BackForwardButtonStatePtr GetButtonState() const;

 private:
  FRIEND_TEST_ALL_PREFIXES(WebUIToolbarWebViewPixelBrowserTest,
                           CheckBackButtonColor);
  FRIEND_TEST_ALL_PREFIXES(WebUIToolbarWebViewPixelBrowserTest,
                           CheckForwardButtonColor);
  FRIEND_TEST_ALL_PREFIXES(WebUIToolbarWebViewPixelBrowserTest,
                           BackForwardButtonsModifierClick);
  FRIEND_TEST_ALL_PREFIXES(WebUIToolbarButtonPressAndDragTest,
                           PressAndDragDown);

  const raw_ptr<WebUIToolbarWebView> webui_toolbar_web_view_;
  const BackForwardButton::Direction direction_;
  BackForwardMenuModel menu_model_;
  std::unique_ptr<views::MenuModelAdapter> menu_model_adapter_;
  std::unique_ptr<views::MenuRunner> menu_runner_;
  bool enabled_ = true;
  bool is_pinned_ = true;
};

#endif  // CHROME_BROWSER_UI_VIEWS_TOOLBAR_WEBUI_BACK_FORWARD_CONTROL_H_
