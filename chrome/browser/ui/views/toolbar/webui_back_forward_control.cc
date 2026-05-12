// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/views/toolbar/webui_back_forward_control.h"

#include "chrome/browser/ui/browser_element_identifiers.h"
#include "chrome/browser/ui/browser_window/public/browser_window_interface.h"
#include "chrome/browser/ui/toolbar/back_forward_menu_model.h"
#include "chrome/browser/ui/views/toolbar/webui_toolbar_web_view.h"
#include "components/browser_apis/ui_controllers/toolbar/toolbar_ui_api_data_model.mojom.h"
#include "ui/base/mojom/menu_source_type.mojom.h"
#include "ui/gfx/geometry/rect.h"
#include "ui/views/controls/menu/menu_runner.h"
#include "ui/views/view.h"

WebUIBackForwardControl::WebUIBackForwardControl(
    WebUIToolbarControlDelegate* delegate,
    BackForwardButton::Direction direction)
    : delegate_(delegate),
      direction_(direction),
      menu_model_(delegate->GetBrowser()->GetBrowserForMigrationOnly(),
                  direction == BackForwardButton::Direction::kBack
                      ? BackForwardMenuModel::ModelType::kBackward
                      : BackForwardMenuModel::ModelType::kForward) {}

WebUIBackForwardControl::~WebUIBackForwardControl() = default;

void WebUIBackForwardControl::HandleContextMenu(
    views::Widget* widget,
    const gfx::Rect& screen_rect,
    ui::mojom::MenuSourceType source) {
  // Reset the menu runner first so that it doesn't hold a dangling pointer
  // to the old menu model adapter when it is being destroyed.
  menu_runner_.reset();

  menu_model_adapter_ = std::make_unique<views::MenuModelAdapter>(
      &menu_model_, base::BindRepeating(
                        &WebUIToolbarControlDelegate::OnBackForwardStateChanged,
                        base::Unretained(delegate_)));
  std::unique_ptr<views::MenuItemView> root = menu_model_adapter_->CreateMenu();
  root->SetSubmenuId(direction_ == BackForwardButton::Direction::kBack
                         ? kToolbarBackButtonMenuElementId
                         : kToolbarForwardButtonMenuElementId);
  menu_runner_ = std::make_unique<views::MenuRunner>(
      std::move(root), views::MenuRunner::HAS_MNEMONICS);
  menu_runner_->RunMenuAt(widget, nullptr, screen_rect,
                          views::MenuAnchorPosition::kTopLeft, source);

  delegate_->OnBackForwardStateChanged();
}

void WebUIBackForwardControl::SetEnabled(bool enabled) {
  if (enabled_ == enabled) {
    return;
  }
  enabled_ = enabled;
  delegate_->OnBackForwardStateChanged();
}

void WebUIBackForwardControl::SetIsPinned(bool is_pinned) {
  if (is_pinned_ == is_pinned) {
    return;
  }
  is_pinned_ = is_pinned;
  delegate_->OnPreferredSizeChanged();
  delegate_->OnBackForwardStateChanged();
}

bool WebUIBackForwardControl::IsPinned() const {
  return is_pinned_;
}

toolbar_ui_api::mojom::BackForwardButtonStatePtr
WebUIBackForwardControl::GetButtonState() const {
  return toolbar_ui_api::mojom::BackForwardButtonState::New(
      /*enabled=*/enabled_, /*should_be_shown=*/is_pinned_,
      /*is_context_menu_visible=*/menu_runner_ && menu_runner_->IsRunning());
}
