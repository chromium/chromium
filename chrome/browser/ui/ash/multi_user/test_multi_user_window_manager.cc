// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/ash/multi_user/test_multi_user_window_manager.h"

#include "base/memory/ptr_util.h"
#include "chrome/browser/ui/ash/multi_user/multi_user_util.h"
#include "chrome/browser/ui/ash/multi_user/multi_user_window_manager_helper.h"
#include "chrome/browser/ui/browser_window.h"
#include "components/account_id/account_id.h"
#include "ui/aura/window.h"
#include "ui/views/widget/widget.h"

TestMultiUserWindowManager::~TestMultiUserWindowManager() {
  // This object is owned by the MultiUserWindowManager since the
  // SetInstanceForTest call. As such no uninstall is required.
}

// static
TestMultiUserWindowManager* TestMultiUserWindowManager::Create(
    Browser* visiting_browser,
    const AccountId& desktop_owner) {
  // Must use WrapUnique() as constructor is private.
  std::unique_ptr<TestMultiUserWindowManager> window_manager = base::WrapUnique(
      new TestMultiUserWindowManager(visiting_browser, desktop_owner));
  TestMultiUserWindowManager* raw_window_manager = window_manager.get();
  MultiUserWindowManagerHelper::CreateInstanceForTest(
      std::move(window_manager));
  return raw_window_manager;
}

void TestMultiUserWindowManager::SetWindowOwner(aura::Window* window,
                                                const AccountId& account_id) {
  NOTREACHED();
}

const AccountId& TestMultiUserWindowManager::GetWindowOwner(
    const aura::Window* window) const {
  // No matter which window will get queried - all browsers belong to the
  // original browser's user.
  return browser_owner_;
}

void TestMultiUserWindowManager::ShowWindowForUser(
    aura::Window* window,
    const AccountId& account_id) {
  // This class is only able to handle one additional window <-> user
  // association beside the creation parameters.
  // If no association has yet been requested remember it now.
  if (browser_owner_ != account_id)
    DCHECK(!created_window_);
  created_window_ = window;
  created_window_shown_for_ = account_id;

  if (browser_window_ == window)
    desktop_owner_ = account_id;

  if (account_id == current_account_id_)
    return;

  // Change the visibility of the window to update the view recursively.
  views::Widget* widget = views::Widget::GetWidgetForNativeView(window);
  widget->Hide();
  widget->Show();
  current_account_id_ = account_id;
}

bool TestMultiUserWindowManager::AreWindowsSharedAmongUsers() const {
  return browser_owner_ != desktop_owner_;
}

std::set<AccountId> TestMultiUserWindowManager::GetOwnersOfVisibleWindows()
    const {
  return {};
}

const AccountId& TestMultiUserWindowManager::GetUserPresentingWindow(
    const aura::Window* window) const {
  if (window == browser_window_)
    return desktop_owner_;
  if (created_window_ && window == created_window_)
    return created_window_shown_for_;
  // We can come here before the window gets registered.
  return browser_owner_;
}

const AccountId& TestMultiUserWindowManager::CurrentAccountId() const {
  return current_account_id_;
}

void TestMultiUserWindowManager::AddObserver(
    ash::MultiUserWindowManagerObserver* observer) {}

void TestMultiUserWindowManager::RemoveObserver(
    ash::MultiUserWindowManagerObserver* observer) {}

TestMultiUserWindowManager::TestMultiUserWindowManager(
    Browser* visiting_browser,
    const AccountId& desktop_owner)
    : browser_window_(visiting_browser->window()->GetNativeWindow()),
      browser_owner_(multi_user_util::GetAccountIdFromProfile(
          visiting_browser->profile())),
      desktop_owner_(desktop_owner),
      created_window_shown_for_(browser_owner_),
      current_account_id_(desktop_owner) {}
