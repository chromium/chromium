// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/cocoa/renderer_context_menu/render_view_context_menu_mac_remote_cocoa.h"

#include "chrome/browser/headless/headless_mode_util.h"
#include "components/remote_cocoa/common/menu.mojom.h"
#include "content/public/browser/render_widget_host_view.h"
#include "ui/views/widget/widget.h"

RenderViewContextMenuMacRemoteCocoa::RenderViewContextMenuMacRemoteCocoa(
    content::RenderFrameHost& render_frame_host,
    const content::ContextMenuParams& params,
    content::RenderWidgetHostView* parent_view)
    : RenderViewContextMenuMac(render_frame_host, params),
      target_view_id_(parent_view->GetNSViewId()),
      target_view_bounds_(parent_view->GetViewBounds()) {}

RenderViewContextMenuMacRemoteCocoa::~RenderViewContextMenuMacRemoteCocoa() {
  if (runner_) {
    runner_.ExtractAsDangling()->Release();
  }
}

void RenderViewContextMenuMacRemoteCocoa::Show() {
  views::Widget* widget = views::Widget::GetTopLevelWidgetForNativeView(
      source_web_contents_->GetNativeView());

  if (!widget || headless::IsHeadlessMode()) {
    return;
  }

  if (runner_) {
    runner_.ExtractAsDangling()->Release();
  }
  runner_ =
      new views::MenuRunnerImplRemoteCocoa(&menu_model_, base::DoNothing());
  runner_->RunMenu(
      widget,
      target_view_bounds_.origin() + gfx::Vector2d(params_.x, params_.y),
      target_view_id_);
}

void RenderViewContextMenuMacRemoteCocoa::CancelToolkitMenu() {
  runner_->Cancel();
}

void RenderViewContextMenuMacRemoteCocoa::UpdateToolkitMenuItem(
    int command_id,
    bool enabled,
    bool hidden,
    const std::u16string& title) {
  runner_->UpdateMenuItem(command_id, enabled, hidden, title);
}
