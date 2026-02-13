// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/views/toolbar/webui_reload_control.h"

#include "chrome/app/chrome_command_ids.h"
#include "chrome/browser/ui/views/toolbar/webui_toolbar_web_view.h"
#include "chrome/browser/ui/webui/webui_toolbar/webui_toolbar_ui.h"
#include "chrome/grit/generated_resources.h"
#include "ui/base/window_open_disposition_utils.h"
#include "ui/views/controls/menu/menu_runner.h"
#include "ui/views/widget/widget.h"

WebUIReloadControl::WebUIReloadControl(
    WebUIToolbarWebView* webui_toolbar_web_view)
    : webui_toolbar_web_view_(webui_toolbar_web_view) {
  menu_model_ = std::make_unique<ui::SimpleMenuModel>(this);
  menu_model_->AddItemWithStringId(IDC_RELOAD,
                                   IDS_RELOAD_MENU_NORMAL_RELOAD_ITEM);
  menu_model_->AddItemWithStringId(IDC_RELOAD_BYPASSING_CACHE,
                                   IDS_RELOAD_MENU_HARD_RELOAD_ITEM);
  menu_model_->AddItemWithStringId(IDC_RELOAD_CLEARING_CACHE,
                                   IDS_RELOAD_MENU_EMPTY_AND_HARD_RELOAD_ITEM);

  menu_runner_ = std::make_unique<views::MenuRunner>(
      menu_model_.get(), views::MenuRunner::HAS_MNEMONICS,
      base::BindRepeating(&WebUIReloadControl::OnContextMenuClosed,
                          base::Unretained(this)));

  // The accessibility and tooltip attributes are handled by the WebUI.
}

WebUIReloadControl::~WebUIReloadControl() = default;

void WebUIReloadControl::Init() {
  CHECK(!is_initialized_);
  is_initialized_ = true;

  // TODO: These two mojo messages are likely unnecessary if `mode_` and
  // `is_dev_tools_connected_` match the corresponding default values in
  // TypeScript.
  OnNavigationStatusChanged();
  OnDevToolsStatusChanged();
}

void WebUIReloadControl::ChangeMode(ReloadControl::Mode mode, bool force) {
  // TODO(crbug.com/444358999): Now the mode is always updated immediately from
  // the browser side, then a mojo IPC is sent to the renderer to make the
  // change accordingly. We may need to implement the timer/force updating logic
  // in the future.
  mode_ = mode;
  OnNavigationStatusChanged();
}

bool WebUIReloadControl::GetDevToolsStatusForTesting() const {
  return is_dev_tools_connected_;
}

void WebUIReloadControl::SetDevToolsStatus(bool is_dev_tools_connected) {
  is_dev_tools_connected_ = is_dev_tools_connected;
  OnDevToolsStatusChanged();
}

bool WebUIReloadControl::HandleContextMenu(views::Widget* widget,
                                           gfx::Point screen_location,
                                           ui::mojom::MenuSourceType source) {
  if (is_dev_tools_connected_) {
    auto* webui_toolbar_ui = webui_toolbar_web_view_->GetWebUIToolbarUI();
    CHECK(webui_toolbar_ui);
    webui_toolbar_ui->OnContextMenuStateChanged(
        browser_controls_api::mojom::ContextMenuType::kReload,
        browser_controls_api::mojom::ContextMenuState::kVisible);

    menu_runner_->RunMenuAt(webui_toolbar_web_view_->GetWidget(), nullptr,
                            gfx::Rect(screen_location, gfx::Size()),
                            views::MenuAnchorPosition::kBubbleBottomRight,
                            source);
  }
  return true;
}

void WebUIReloadControl::OnContextMenuClosed() {
  auto* webui_toolbar_ui = webui_toolbar_web_view_->GetWebUIToolbarUI();
  CHECK(webui_toolbar_ui);
  webui_toolbar_ui->OnContextMenuStateChanged(
      browser_controls_api::mojom::ContextMenuType::kReload,
      browser_controls_api::mojom::ContextMenuState::kHidden);
}

bool WebUIReloadControl::IsCommandIdChecked(int command_id) const {
  return false;
}

bool WebUIReloadControl::IsCommandIdEnabled(int command_id) const {
  return true;
}

bool WebUIReloadControl::IsCommandIdVisible(int command_id) const {
  return true;
}

bool WebUIReloadControl::GetAcceleratorForCommandId(
    int command_id,
    ui::Accelerator* accelerator) const {
  return webui_toolbar_web_view_->GetWidget()->GetAccelerator(command_id,
                                                              accelerator);
}

void WebUIReloadControl::ExecuteCommand(int command_id, int event_flags) {
  CHECK(webui_toolbar_web_view_->controller());
  webui_toolbar_web_view_->controller()->ExecuteCommandWithDisposition(
      command_id, ui::DispositionFromEventFlags(event_flags));
}

void WebUIReloadControl::OnNavigationStatusChanged() {
  auto* webui_toolbar_ui = webui_toolbar_web_view_->GetWebUIToolbarUI();

  // If the WebUI toolbar is not ready yet, skip updating the UI state. It will
  // be synchronized later on initialize.
  if (!webui_toolbar_ui) {
    return;
  }

  webui_toolbar_ui->OnNavigationStatusChanged(
      (mode_ == ReloadControl::Mode::kStop)
          ? browser_controls_api::mojom::NavigationState::kLoading
          : browser_controls_api::mojom::NavigationState::kNotLoading);
}

void WebUIReloadControl::OnDevToolsStatusChanged() {
  auto* webui_toolbar_ui = webui_toolbar_web_view_->GetWebUIToolbarUI();
  CHECK(webui_toolbar_ui);
  webui_toolbar_ui->OnDevToolsStatusChanged(
      is_dev_tools_connected_
          ? browser_controls_api::mojom::DevToolsState::kConnected
          : browser_controls_api::mojom::DevToolsState::kDisconnected);
}
