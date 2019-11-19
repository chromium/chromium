// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/ash/multi_user/multi_user_window_manager_helper.h"

#include "chrome/browser/ui/ash/multi_user/multi_profile_support.h"
#include "chrome/browser/ui/ash/multi_user/multi_user_window_manager_stub.h"
#include "chrome/browser/ui/ash/session_controller_client_impl.h"
#include "components/account_id/account_id.h"
#include "components/user_manager/user_info.h"
#include "components/user_manager/user_manager.h"

namespace {
MultiUserWindowManagerHelper* g_multi_user_window_manager_instance = nullptr;
}  // namespace

// static
MultiUserWindowManagerHelper* MultiUserWindowManagerHelper::GetInstance() {
  return g_multi_user_window_manager_instance;
}

// static
ash::MultiUserWindowManager* MultiUserWindowManagerHelper::GetWindowManager() {
  // In tests this may be called before the instance has been set.
  return g_multi_user_window_manager_instance
             ? g_multi_user_window_manager_instance->GetWindowManagerImpl()
             : nullptr;
}

// static
MultiUserWindowManagerHelper* MultiUserWindowManagerHelper::CreateInstance() {
  DCHECK(!g_multi_user_window_manager_instance);
  if (SessionControllerClientImpl::IsMultiProfileAvailable()) {
    g_multi_user_window_manager_instance = new MultiUserWindowManagerHelper(
        user_manager::UserManager::Get()->GetActiveUser()->GetAccountId());
  } else {
    g_multi_user_window_manager_instance = new MultiUserWindowManagerHelper(
        std::make_unique<MultiUserWindowManagerStub>());
  }
  return g_multi_user_window_manager_instance;
}

// static
bool MultiUserWindowManagerHelper::ShouldShowAvatar(aura::Window* window) {
  // Session restore can open a window for the first user before the instance
  // is created.
  if (!g_multi_user_window_manager_instance)
    return false;

  // Show the avatar icon if the window is on a different desktop than the
  // window's owner's desktop. The stub implementation does the right thing
  // for single-user mode.
  return !g_multi_user_window_manager_instance->IsWindowOnDesktopOfUser(
      window, GetWindowManager()->GetWindowOwner(window));
}

// static
void MultiUserWindowManagerHelper::DeleteInstance() {
  DCHECK(g_multi_user_window_manager_instance);
  delete g_multi_user_window_manager_instance;
  g_multi_user_window_manager_instance = nullptr;
}

// static
void MultiUserWindowManagerHelper::CreateInstanceForTest(
    const AccountId& account_id) {
  if (g_multi_user_window_manager_instance)
    DeleteInstance();
  g_multi_user_window_manager_instance =
      new MultiUserWindowManagerHelper(account_id);
}

// static
void MultiUserWindowManagerHelper::CreateInstanceForTest(
    std::unique_ptr<ash::MultiUserWindowManager> window_manager) {
  if (g_multi_user_window_manager_instance)
    DeleteInstance();
  g_multi_user_window_manager_instance =
      new MultiUserWindowManagerHelper(std::move(window_manager));
}

void MultiUserWindowManagerHelper::AddUser(content::BrowserContext* profile) {
  if (multi_profile_support_)
    multi_profile_support_->AddUser(profile);
}

bool MultiUserWindowManagerHelper::IsWindowOnDesktopOfUser(
    aura::Window* window,
    const AccountId& account_id) const {
  const AccountId& presenting_user =
      GetWindowManagerImpl()->GetUserPresentingWindow(window);
  return (!presenting_user.is_valid()) || presenting_user == account_id;
}

MultiUserWindowManagerHelper::MultiUserWindowManagerHelper(
    const AccountId& account_id)
    : multi_profile_support_(
          std::make_unique<MultiProfileSupport>(account_id)) {
  multi_profile_support_->Init();
}

MultiUserWindowManagerHelper::MultiUserWindowManagerHelper(
    std::unique_ptr<ash::MultiUserWindowManager> window_manager)
    : multi_user_window_manager_(std::move(window_manager)) {}

MultiUserWindowManagerHelper::~MultiUserWindowManagerHelper() = default;

const ash::MultiUserWindowManager*
MultiUserWindowManagerHelper::GetWindowManagerImpl() const {
  return multi_user_window_manager_
             ? multi_user_window_manager_.get()
             : multi_profile_support_->multi_user_window_manager();
}
