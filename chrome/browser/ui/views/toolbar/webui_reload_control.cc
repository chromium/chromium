// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/views/toolbar/webui_reload_control.h"

#include "chrome/app/chrome_command_ids.h"
#include "chrome/browser/ui/views/toolbar/reload_button_web_view.h"
#include "chrome/browser/ui/webui/reload_button/reload_button_ui.h"
#include "chrome/grit/generated_resources.h"
#include "ui/base/window_open_disposition_utils.h"
#include "ui/views/controls/menu/menu_runner.h"
#include "ui/views/widget/widget.h"

WebUIReloadControl::WebUIReloadControl(
    ReloadButtonWebView* reload_button_web_view)
    : reload_button_web_view_(reload_button_web_view) {
  menu_model_ = std::make_unique<ui::SimpleMenuModel>(this);
  menu_model_->AddItemWithStringId(IDC_RELOAD,
                                   IDS_RELOAD_MENU_NORMAL_RELOAD_ITEM);
  menu_model_->AddItemWithStringId(IDC_RELOAD_BYPASSING_CACHE,
                                   IDS_RELOAD_MENU_HARD_RELOAD_ITEM);
  menu_model_->AddItemWithStringId(IDC_RELOAD_CLEARING_CACHE,
                                   IDS_RELOAD_MENU_EMPTY_AND_HARD_RELOAD_ITEM);

  menu_runner_ = std::make_unique<views::MenuRunner>(
      menu_model_.get(), views::MenuRunner::CONTEXT_MENU);

  // The accessibility and tooltip attributes are handled by the WebUI.
}

WebUIReloadControl::~WebUIReloadControl() = default;

void WebUIReloadControl::Init() {
  SetReloadButtonUIState();
}

void WebUIReloadControl::ChangeMode(ReloadControl::Mode mode, bool force) {
  // TODO(crbug.com/444358999): Now the mode is always updated immediately from
  // the browser side, then a mojo IPC is sent to the renderer to make the
  // change accordingly. We may need to implement the timer/force updating logic
  // in the future.
  mode_ = mode;
  SetReloadButtonUIState();
}

bool WebUIReloadControl::GetMenuEnabled() const {
  return is_menu_enabled_;
}

void WebUIReloadControl::SetMenuEnabled(bool is_menu_enabled) {
  is_menu_enabled_ = is_menu_enabled;
  SetReloadButtonUIState();
}

bool WebUIReloadControl::HandleContextMenu(
    views::Widget* widget,
    gfx::Point screen_location,
    const content::ContextMenuParams& params) {
  if (is_menu_enabled_) {
    menu_runner_->RunMenuAt(reload_button_web_view_->GetWidget(), nullptr,
                            gfx::Rect(screen_location, gfx::Size()),
                            views::MenuAnchorPosition::kBubbleBottomRight,
                            params.source_type);
  }
  return true;
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
  return reload_button_web_view_->GetWidget()->GetAccelerator(command_id,
                                                              accelerator);
}

void WebUIReloadControl::ExecuteCommand(int command_id, int event_flags) {
  CHECK(reload_button_web_view_->controller());
  reload_button_web_view_->controller()->ExecuteCommandWithDisposition(
      command_id, ui::DispositionFromEventFlags(event_flags));
}

void WebUIReloadControl::SetReloadButtonUIState() {
  CHECK(reload_button_web_view_->reload_button_ui());
  reload_button_web_view_->reload_button_ui()->SetReloadButtonState(
      /*is_loading=*/mode_ == ReloadControl::Mode::kStop, is_menu_enabled_);
}
