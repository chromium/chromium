// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/views/toolbar/webui_pinned_toolbar_actions.h"

#include "base/notimplemented.h"
#include "chrome/browser/ui/browser_actions.h"
#include "chrome/browser/ui/browser_window/public/browser_window_interface.h"
#include "chrome/browser/ui/views/toolbar/webui_toolbar_web_view.h"

WebUIPinnedToolbarActions::WebUIPinnedToolbarActions(
    WebUIToolbarWebView* webui_toolbar_web_view)
    : webui_toolbar_web_view_(webui_toolbar_web_view),
      model_(PinnedToolbarActionsModel::Get(
          webui_toolbar_web_view->browser_->GetProfile())) {}

const std::vector<actions::ActionId>&
WebUIPinnedToolbarActions::PinnedActionIds() const {
  return model_->PinnedActionIds();
}

actions::ActionItem* WebUIPinnedToolbarActions::GetActionItemFor(
    actions::ActionId id) {
  return actions::ActionManager::Get().FindAction(
      id, webui_toolbar_web_view_->browser_->GetActions()->root_action_item());
}

bool WebUIPinnedToolbarActions::IsOverflowed(actions::ActionId id) {
  NOTIMPLEMENTED();
  return false;
}

views::View* WebUIPinnedToolbarActions::GetContainerView() {
  NOTIMPLEMENTED();
  return nullptr;
}

bool WebUIPinnedToolbarActions::ShouldAnyButtonsOverflow(
    gfx::Size available_size) const {
  NOTIMPLEMENTED();
  return false;
}

void WebUIPinnedToolbarActions::UpdateActionState(actions::ActionId id,
                                                  bool is_active) {
  NOTIMPLEMENTED();
}

void WebUIPinnedToolbarActions::ShowActionEphemerallyInToolbar(
    actions::ActionId id,
    bool show) {
  NOTIMPLEMENTED();
}

bool WebUIPinnedToolbarActions::IsActionPinned(actions::ActionId id) {
  NOTIMPLEMENTED();
  return false;
}

bool WebUIPinnedToolbarActions::IsActionPoppedOut(actions::ActionId id) {
  NOTIMPLEMENTED();
  return false;
}

bool WebUIPinnedToolbarActions::IsActionPinnedOrPoppedOut(
    actions::ActionId id) {
  return IsActionPinned(id) || IsActionPoppedOut(id);
}

void WebUIPinnedToolbarActions::PostOrQueueActionAfterAnimation(
    base::OnceClosure action) {
  NOTIMPLEMENTED();
}

ToolbarButton* WebUIPinnedToolbarActions::GetDownloadButton() {
  // TODO(https://crbug.com/474063115): Implement this.
  NOTIMPLEMENTED();
  return nullptr;
}

ToolbarButton* WebUIPinnedToolbarActions::GetCastButton() {
  // TODO(https://crbug.com/474062755): Implement this.
  NOTIMPLEMENTED();
  return nullptr;
}

views::BubbleAnchor WebUIPinnedToolbarActions::GetBubbleAnchor(
    actions::ActionId action_id) {
  NOTIMPLEMENTED();
  return nullptr;
}

void WebUIPinnedToolbarActions::SetActionElementIdentifier(
    actions::ActionId action_id,
    ui::ElementIdentifier element_id) {
  NOTIMPLEMENTED();
}

PinnedActionToolbarButton* WebUIPinnedToolbarActions::GetChromeLabsButton() {
  return nullptr;
}

void WebUIPinnedToolbarActions::UpdatePinnedStateAndAnnounce(
    actions::ActionId id,
    bool pin) {
  NOTIMPLEMENTED();
}
