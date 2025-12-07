// Copyright 2015 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/toolbar/test_toolbar_action_view_model.h"

#include <string>

#include "extensions/browser/permissions/site_permissions_helper.h"
#include "ui/base/models/image_model.h"
#include "ui/gfx/native_ui_types.h"

TestToolbarActionViewModel::TestToolbarActionViewModel(const std::string& id)
    : id_(id) {
  // Needs a non-empty accessible name to pass accessibility checks.
  SetAccessibleName(u"Default name");
}

TestToolbarActionViewModel::~TestToolbarActionViewModel() = default;

std::string TestToolbarActionViewModel::GetId() const {
  return id_;
}

base::CallbackListSubscription
TestToolbarActionViewModel::RegisterUpdateObserver(
    base::RepeatingClosure observer) {
  return observers_.Add(observer);
}

ui::ImageModel TestToolbarActionViewModel::GetIcon(
    content::WebContents* web_contents,
    const gfx::Size& size) {
  return ui::ImageModel();
}

std::u16string TestToolbarActionViewModel::GetActionName() const {
  return action_name_;
}

std::u16string TestToolbarActionViewModel::GetActionTitle(
    content::WebContents* web_contents) const {
  return action_title_;
}

std::u16string TestToolbarActionViewModel::GetAccessibleName(
    content::WebContents* web_contents) const {
  return accessible_name_;
}

std::u16string TestToolbarActionViewModel::GetTooltip(
    content::WebContents* web_contents) const {
  return tooltip_;
}

ToolbarActionViewModel::HoverCardState
TestToolbarActionViewModel::GetHoverCardState(
    content::WebContents* web_contents) const {
  ToolbarActionViewModel::HoverCardState state;
  state.site_access = ToolbarActionViewModel::HoverCardState::SiteAccess::
      kExtensionDoesNotWantAccess;
  state.policy = ToolbarActionViewModel::HoverCardState::AdminPolicy::kNone;
  return state;
}

bool TestToolbarActionViewModel::IsEnabled(
    content::WebContents* web_contents) const {
  return is_enabled_;
}

bool TestToolbarActionViewModel::IsShowingPopup() const {
  return popup_showing_;
}

void TestToolbarActionViewModel::HidePopup() {
  popup_showing_ = false;
}

gfx::NativeView TestToolbarActionViewModel::GetPopupNativeView() {
  return gfx::NativeView();
}

ui::MenuModel* TestToolbarActionViewModel::GetContextMenu(
    extensions::ExtensionContextMenuModel::ContextMenuSource
        context_menu_source) {
  return nullptr;
}

void TestToolbarActionViewModel::ExecuteUserAction(InvocationSource source) {
  ++execute_action_count_;
}

void TestToolbarActionViewModel::TriggerPopupForAPI(
    ShowPopupCallback callback) {}

extensions::SitePermissionsHelper::SiteInteraction
TestToolbarActionViewModel::GetSiteInteraction(
    content::WebContents* web_contents) const {
  return extensions::SitePermissionsHelper::SiteInteraction::kNone;
}

void TestToolbarActionViewModel::ShowPopup(bool by_user) {
  popup_showing_ = true;
}

void TestToolbarActionViewModel::SetActionName(const std::u16string& name) {
  action_name_ = name;
  NotifyObservers();
}

void TestToolbarActionViewModel::SetActionTitle(const std::u16string& title) {
  action_title_ = title;
  NotifyObservers();
}

void TestToolbarActionViewModel::SetAccessibleName(const std::u16string& name) {
  accessible_name_ = name;
  NotifyObservers();
}

void TestToolbarActionViewModel::SetTooltip(const std::u16string& tooltip) {
  tooltip_ = tooltip;
  NotifyObservers();
}

void TestToolbarActionViewModel::SetEnabled(bool is_enabled) {
  is_enabled_ = is_enabled;
  NotifyObservers();
}

void TestToolbarActionViewModel::NotifyObservers() {
  observers_.Notify();
}
