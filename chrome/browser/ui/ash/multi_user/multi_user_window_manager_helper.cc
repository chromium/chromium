// Copyright 2013 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/ash/multi_user/multi_user_window_manager_helper.h"

#include "ash/public/cpp/multi_user_window_manager.h"
#include "ash/shell.h"
#include "chrome/browser/ui/ash/multi_user/multi_user_window_manager_browser_adaptor.h"
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
  multi_user_window_manager_browser_adaptor_->AddUser(account_id);
}

MultiUserWindowManagerHelper::MultiUserWindowManagerHelper()
    : multi_user_window_manager_browser_adaptor_(
          std::make_unique<ash::MultiUserWindowManagerBrowserAdaptor>(
              ash::Shell::Get()->multi_user_window_manager())) {}

MultiUserWindowManagerHelper::~MultiUserWindowManagerHelper() = default;
