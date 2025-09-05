// Copyright 2013 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/ash/multi_user/multi_user_window_manager_helper.h"

#include "ash/public/cpp/multi_user_window_manager.h"
#include "ash/shell.h"
#include "chrome/browser/ui/ash/multi_user/multi_profile_support.h"
#include "chrome/browser/ui/ash/session/session_controller_client_impl.h"
#include "components/account_id/account_id.h"
#include "components/session_manager/core/session.h"
#include "components/session_manager/core/session_manager.h"

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
  return ash::Shell::HasInstance()
             ? ash::Shell::Get()->multi_user_window_manager()
             : nullptr;
}

// static
MultiUserWindowManagerHelper* MultiUserWindowManagerHelper::CreateInstance() {
  CHECK(!g_multi_user_window_manager_instance);
  g_multi_user_window_manager_instance = new MultiUserWindowManagerHelper();
  return g_multi_user_window_manager_instance;
}

// static
void MultiUserWindowManagerHelper::DeleteInstance() {
  DCHECK(g_multi_user_window_manager_instance);
  delete g_multi_user_window_manager_instance;
  g_multi_user_window_manager_instance = nullptr;
}

// static
void MultiUserWindowManagerHelper::CreateInstanceForTest() {
  // TODO(crbug.com/425160398): Remove this method and use CreateInstance()
  // always.
  CreateInstance();
}

void MultiUserWindowManagerHelper::AddUser(const AccountId& account_id) {
  multi_profile_support_->AddUser(account_id);
}

MultiUserWindowManagerHelper::MultiUserWindowManagerHelper()
    : multi_profile_support_(std::make_unique<MultiProfileSupport>(
          MultiUserWindowManagerHelper::GetWindowManager())) {}

MultiUserWindowManagerHelper::~MultiUserWindowManagerHelper() = default;
