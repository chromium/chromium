// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/toolbar/test_toolbar_action_view_controller.h"

#include "base/strings/string16.h"
#include "base/strings/utf_string_conversions.h"
#include "chrome/browser/ui/toolbar/toolbar_action_view_delegate.h"
#include "ui/gfx/image/image.h"
#include "ui/gfx/image/image_skia.h"

TestToolbarActionViewController::TestToolbarActionViewController(
    const std::string& id)
    : id_(id) {
  // Needs a non-empty accessible name to pass accessibility checks.
  SetAccessibleName(base::ASCIIToUTF16("Default name"));
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

gfx::Image TestToolbarActionViewController::GetIcon(
    content::WebContents* web_contents,
    const gfx::Size& size) {
  return gfx::Image();
}

base::string16 TestToolbarActionViewController::GetActionName() const {
  return action_name_;
}

base::string16 TestToolbarActionViewController::GetAccessibleName(
    content::WebContents* web_contents) const {
  return accessible_name_;
}

base::string16 TestToolbarActionViewController::GetTooltip(
    content::WebContents* web_contents) const {
  return tooltip_;
}

bool TestToolbarActionViewController::IsEnabled(
    content::WebContents* web_contents) const {
  return is_enabled_;
}

bool TestToolbarActionViewController::WantsToRun(
    content::WebContents* web_contents) const {
  return wants_to_run_;
}

bool TestToolbarActionViewController::HasPopup(
    content::WebContents* web_contents) const {
  return true;
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

ui::MenuModel* TestToolbarActionViewController::GetContextMenu() {
  return nullptr;
}

bool TestToolbarActionViewController::ExecuteAction(bool by_user) {
  ++execute_action_count_;
  return false;
}

void TestToolbarActionViewController::UpdateState() {
  UpdateDelegate();
}

bool TestToolbarActionViewController::DisabledClickOpensMenu() const {
  return disabled_click_opens_menu_;
}

ToolbarActionViewController::PageInteractionStatus
TestToolbarActionViewController::GetPageInteractionStatus(
    content::WebContents* web_contents) const {
  return PageInteractionStatus::kNone;
}

void TestToolbarActionViewController::ShowPopup(bool by_user) {
  popup_showing_ = true;
  delegate_->OnPopupShown(by_user);
}

void TestToolbarActionViewController::SetActionName(
    const base::string16& name) {
  action_name_ = name;
  UpdateDelegate();
}

void TestToolbarActionViewController::SetAccessibleName(
    const base::string16& name) {
  accessible_name_ = name;
  UpdateDelegate();
}

void TestToolbarActionViewController::SetTooltip(
    const base::string16& tooltip) {
  tooltip_ = tooltip;
  UpdateDelegate();
}

void TestToolbarActionViewController::SetEnabled(bool is_enabled) {
  is_enabled_ = is_enabled;
  UpdateDelegate();
}

void TestToolbarActionViewController::SetWantsToRun(bool wants_to_run) {
  wants_to_run_ = wants_to_run;
  UpdateDelegate();
}

void TestToolbarActionViewController::SetDisabledClickOpensMenu(
    bool disabled_click_opens_menu) {
  disabled_click_opens_menu_ = disabled_click_opens_menu;
  UpdateDelegate();
}

void TestToolbarActionViewController::UpdateDelegate() {
  if (delegate_)
    delegate_->UpdateState();
}
