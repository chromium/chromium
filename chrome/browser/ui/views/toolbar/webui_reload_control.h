// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_VIEWS_TOOLBAR_WEBUI_RELOAD_CONTROL_H_
#define CHROME_BROWSER_UI_VIEWS_TOOLBAR_WEBUI_RELOAD_CONTROL_H_

#include <memory>

#include "base/time/time.h"
#include "chrome/browser/ui/views/toolbar/reload_control.h"
#include "content/public/browser/context_menu_params.h"
#include "ui/gfx/geometry/rect.h"
#include "ui/menus/simple_menu_model.h"
#include "ui/views/metrics.h"

namespace views {
class Widget;
class MenuRunner;
}  // namespace views

class WebUIToolbarWebView;
class WebUIToolbarWebViewPixelBrowserTest;

// WebUIReloadControl implements C++-side functionality for the WebUI-based
// implementation of the reload button in the toolbar.
class WebUIReloadControl : public ReloadControl {
 public:
  explicit WebUIReloadControl(WebUIToolbarWebView* webui_toolbar_web_view);
  WebUIReloadControl(const WebUIReloadControl&) = delete;
  WebUIReloadControl& operator=(const WebUIReloadControl&) = delete;
  ~WebUIReloadControl() override;

  void Init();

  // ReloadControl overrides:
  void ChangeMode(ReloadControl::Mode mode, bool force) override;
  bool GetDevToolsStatusForTesting() const override;
  void SetDevToolsStatus(bool is_dev_tools_connected) override;

  bool HandleContextMenu(views::Widget* widget,
                         const gfx::Rect& screen_rect,
                         ui::mojom::MenuSourceType source);

  // ui::SimpleMenuModel::Delegate:
  bool IsCommandIdChecked(int command_id) const override;
  bool IsCommandIdEnabled(int command_id) const override;
  bool IsCommandIdVisible(int command_id) const override;
  bool GetAcceleratorForCommandId(int command_id,
                                  ui::Accelerator* accelerator) const override;
  void ExecuteCommand(int command_id, int event_flags) override;

  bool is_initialized() const { return is_initialized_; }

  void set_double_click_interval_for_testing(
      base::TimeDelta double_click_interval) {
    double_click_interval_ = double_click_interval;
  }

 private:
  FRIEND_TEST_ALL_PREFIXES(WebUIToolbarWebViewPixelBrowserTest,
                           CheckReloadButtonColor);
  FRIEND_TEST_ALL_PREFIXES(WebUIToolbarWebViewBrowserTest,
                           ContextMenuPositionE2E);

  void UpdateState();

  const raw_ptr<WebUIToolbarWebView> webui_toolbar_web_view_;

  // The maximum time allowed before two clicks are considered separate clicks
  // instead of a double click. This is pulled from the OS on construction and
  // then cached, even if the system value changes. This matches the behavior of
  // the legacy ReloadButton.
  base::TimeDelta double_click_interval_{views::GetDoubleClickInterval()};

  std::unique_ptr<ui::SimpleMenuModel> menu_model_;
  std::unique_ptr<views::MenuRunner> menu_runner_;
  bool is_dev_tools_connected_ = false;
  ReloadControl::Mode mode_ = ReloadControl::Mode::kReload;
  bool is_initialized_ = false;

  // The number of times ChangeMode() has been called with `force`. Passed to
  // Javascript, which will unconditionally reset button state whenever the
  // value changes. Overflow is unlikely, but benign, since the exact value is
  // checked.
  int reset_state_count_ = 0;
};

#endif  // CHROME_BROWSER_UI_VIEWS_TOOLBAR_WEBUI_RELOAD_CONTROL_H_
