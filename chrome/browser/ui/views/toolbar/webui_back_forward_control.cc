// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/views/toolbar/webui_back_forward_control.h"

#include "chrome/browser/ui/browser_window/public/browser_window_interface.h"
#include "chrome/browser/ui/toolbar/back_forward_menu_model.h"
#include "chrome/browser/ui/views/toolbar/webui_toolbar_web_view.h"
#include "components/browser_apis/ui_controllers/toolbar/toolbar_ui_api_data_model.mojom.h"
#include "ui/base/mojom/menu_source_type.mojom.h"
#include "ui/views/controls/menu/menu_runner.h"
#include "ui/views/view.h"

WebUIBackForwardControl::WebUIBackForwardControl(
    WebUIToolbarWebView* webui_toolbar_web_view,
    BackForwardButton::Direction direction)
    : webui_toolbar_web_view_(webui_toolbar_web_view),
      direction_(direction),
      menu_model_(
          webui_toolbar_web_view->browser_->GetBrowserForMigrationOnly(),
          direction == BackForwardButton::Direction::kBack
              ? BackForwardMenuModel::ModelType::kBackward
              : BackForwardMenuModel::ModelType::kForward) {}

WebUIBackForwardControl::~WebUIBackForwardControl() = default;

void WebUIBackForwardControl::HandleContextMenu(
    views::Widget* widget,
    gfx::Point screen_location,
    ui::mojom::MenuSourceType source) {
  menu_runner_ = std::make_unique<views::MenuRunner>(
      &menu_model_, views::MenuRunner::HAS_MNEMONICS);
  menu_runner_->RunMenuAt(webui_toolbar_web_view_->GetWidget(), nullptr,
                          gfx::Rect(screen_location, gfx::Size()),
                          views::MenuAnchorPosition::kTopLeft, source);
}

void WebUIBackForwardControl::SetEnabled(bool enabled) {
  enabled_ = enabled;
  webui_toolbar_web_view_->OnBackForwardStateChanged();
}

void WebUIBackForwardControl::SetVisible(bool visible) {
  visible_ = visible;
  webui_toolbar_web_view_->OnBackForwardStateChanged();
}

bool WebUIBackForwardControl::GetVisible() const {
  return visible_;
}

void WebUIBackForwardControl::SetLeadingMargin(int margin) {
  if (direction_ == BackForwardButton::Direction::kBack) {
    webui_toolbar_web_view_->SetBackButtonLeadingMargin(margin);
  }
}

toolbar_ui_api::mojom::ButtonStatePtr WebUIBackForwardControl::GetButtonState()
    const {
  return toolbar_ui_api::mojom::ButtonState::New(/*enabled=*/enabled_,
                                                 /*visible=*/visible_);
}
