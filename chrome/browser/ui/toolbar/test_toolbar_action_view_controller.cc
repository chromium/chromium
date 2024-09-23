// Copyright 2015 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/toolbar/test_toolbar_action_view_controller.h"

#include <string>

#include "chrome/browser/extensions/permissions/site_permissions_helper.h"
#include "chrome/browser/ui/toolbar/toolbar_action_view_delegate.h"
#include "ui/base/models/image_model.h"

TestToolbarActionViewController::TestToolbarActionViewController(
    const std::string& id)
    : id_(id) {
  // Needs a non-empty accessible name to pass accessibility checks.
  SetAccessibleName(u"Default name");
}

TestToolbarActionViewController::~TestToolbarActionViewController() {
}

std::string TestToolbarActionViewController::GetId() const {
  return id_;
}

void TestToolbarActionViewController::SetDelegate(
    ToolbarActionViewDelegate* delegate) {
  delegate_ = delegate;
}

ui::ImageModel TestToolbarActionViewController::GetIcon(
    content::WebContents* web_contents,
    const gfx::Size& size) {
  return ui::ImageModel();
}

std::u16string TestToolbarActionViewController::GetActionName() const {
  return action_name_;
}

std::u16string TestToolbarActionViewController::GetActionTitle(
    content::WebContents* web_contents) const {
  return action_title_;
}

std::u16string TestToolbarActionViewController::GetAccessibleName(
    content::WebContents* web_contents) const {
  return accessible_name_;
}

std::u16string TestToolbarActionViewController::GetTooltip(
    content::WebContents* web_contents) const {
  return tooltip_;
}

ToolbarActionViewController::HoverCardState
TestToolbarActionViewController::GetHoverCardState(
    content::WebContents* web_contents) const {
  ToolbarActionViewController::HoverCardState state;
  state.site_access = ToolbarActionViewController::HoverCardState::SiteAccess::
      kExtensionDoesNotWantAccess;
  state.policy =
      ToolbarActionViewController::HoverCardState::AdminPolicy::kNone;
  return state;
}

bool TestToolbarActionViewController::IsEnabled(
    content::WebContents* web_contents) const {
  return is_enabled_;
}

bool TestToolbarActionViewController::IsShowingPopup() const {
  return popup_showing_;
}

void TestToolbarActionViewController::HidePopup() {
  popup_showing_ = false;
  delegate_->OnPopupClosed();
}

gfx::NativeView TestToolbarActionViewController::GetPopupNativeView() {
  return nullptr;
}

ui::MenuModel* TestToolbarActionViewController::GetContextMenu(
    extensions::ExtensionContextMenuModel::ContextMenuSource
        context_menu_source) {
  return nullptr;
}

void TestToolbarActionViewController::ExecuteUserAction(
    InvocationSource source) {
  ++execute_action_count_;
}

void TestToolbarActionViewController::TriggerPopupForAPI(
    ShowPopupCallback callback) {}

void TestToolbarActionViewController::UpdateState() {
  UpdateDelegate();
}

extensions::SitePermissionsHelper::SiteInteraction
TestToolbarActionViewController::GetSiteInteraction(
    content::WebContents* web_contents) const {
  return extensions::SitePermissionsHelper::SiteInteraction::kNone;
}

void TestToolbarActionViewController::ShowPopup(bool by_user) {
  popup_showing_ = true;
  delegate_->OnPopupShown(by_user);
}

void TestToolbarActionViewController::SetActionName(
    const std::u16string& name) {
  action_name_ = name;
  UpdateDelegate();
}

void TestToolbarActionViewController::SetActionTitle(
    const std::u16string& title) {
  action_title_ = title;
  UpdateDelegate();
}

void TestToolbarActionViewController::SetAccessibleName(
    const std::u16string& name) {
  accessible_name_ = name;
  UpdateDelegate();
}

void TestToolbarActionViewController::SetTooltip(
    const std::u16string& tooltip) {
  tooltip_ = tooltip;
  UpdateDelegate();
}

void TestToolbarActionViewController::SetEnabled(bool is_enabled) {
  is_enabled_ = is_enabled;
  UpdateDelegate();
}

void TestToolbarActionViewController::UpdateDelegate() {
  if (delegate_)
    delegate_->UpdateState();
}
