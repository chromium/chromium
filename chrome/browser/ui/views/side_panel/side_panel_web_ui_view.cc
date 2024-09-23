// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/views/side_panel/side_panel_web_ui_view.h"

#include "base/metrics/user_metrics.h"
#include "base/metrics/user_metrics_action.h"
#include "chrome/browser/extensions/api/bookmark_manager_private/bookmark_manager_private_api.h"
#include "chrome/browser/feature_engagement/tracker_factory.h"
#include "chrome/browser/ui/bookmarks/bookmark_utils.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/browser/ui/browser_window.h"
#include "chrome/browser/ui/views/frame/browser_view.h"
#include "chrome/browser/ui/views/side_panel/side_panel_content_proxy.h"
#include "chrome/browser/ui/views/side_panel/side_panel_util.h"
#include "chrome/common/webui_url_constants.h"
#include "chrome/grit/generated_resources.h"
#include "components/feature_engagement/public/feature_constants.h"
#include "components/feature_engagement/public/tracker.h"
#include "ui/base/metadata/metadata_impl_macros.h"
#include "ui/views/controls/menu/menu_runner.h"

SidePanelWebUIView::SidePanelWebUIView(base::RepeatingClosure on_show_cb,
                                       base::RepeatingClosure close_cb,
                                       WebUIContentsWrapper* contents_wrapper)
    : on_show_cb_(std::move(on_show_cb)),
      close_cb_(std::move(close_cb)),
      contents_wrapper_(contents_wrapper) {
  const bool is_ready_to_show = contents_wrapper->is_ready_to_show();
  SidePanelUtil::GetSidePanelContentProxy(this)->SetAvailable(is_ready_to_show);
  SetVisible(is_ready_to_show);
  SetID(kSidePanelWebViewId);
  contents_wrapper_->SetHost(weak_factory_.GetWeakPtr());
  SetWebContents(contents_wrapper_->web_contents());
}

SidePanelWebUIView::~SidePanelWebUIView() = default;

void SidePanelWebUIView::ViewHierarchyChanged(
    const views::ViewHierarchyChangedDetails& details) {
  WebView::ViewHierarchyChanged(details);
  // Ensure the WebContents is in a visible state after being added to the
  // side panel so the correct lifecycle hooks are triggered.
  if (details.is_add && details.child == this)
    contents_wrapper_->web_contents()->WasShown();
}

void SidePanelWebUIView::ShowUI() {
  SetVisible(true);
  SidePanelUtil::GetSidePanelContentProxy(this)->SetAvailable(true);
  if (on_show_cb_)
    on_show_cb_.Run();
}

void SidePanelWebUIView::CloseUI() {
  if (close_cb_)
    close_cb_.Run();
}

void SidePanelWebUIView::ShowCustomContextMenu(
    gfx::Point point,
    std::unique_ptr<ui::MenuModel> menu_model) {
  ConvertPointToScreen(this, &point);
  context_menu_model_ = std::move(menu_model);
  context_menu_runner_ = std::make_unique<views::MenuRunner>(
      context_menu_model_.get(),
      views::MenuRunner::HAS_MNEMONICS | views::MenuRunner::CONTEXT_MENU);
  context_menu_runner_->RunMenuAt(
      GetWidget(), nullptr, gfx::Rect(point, gfx::Size()),
      views::MenuAnchorPosition::kTopLeft, ui::MENU_SOURCE_MOUSE,
      contents_wrapper_->web_contents()->GetContentNativeView());
}

void SidePanelWebUIView::HideCustomContextMenu() {
  if (context_menu_runner_)
    context_menu_runner_->Cancel();
}

bool SidePanelWebUIView::HandleKeyboardEvent(
    content::WebContents* source,
    const input::NativeWebKeyboardEvent& event) {
  return unhandled_keyboard_event_handler_.HandleKeyboardEvent(
      event, GetFocusManager());
}

BEGIN_METADATA(SidePanelWebUIView)
END_METADATA
