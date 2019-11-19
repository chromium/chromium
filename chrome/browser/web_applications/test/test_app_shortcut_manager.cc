// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/web_applications/test/test_app_shortcut_manager.h"

#include "base/callback.h"
#include "base/threading/thread_task_runner_handle.h"

namespace web_app {

TestAppShortcutManager::TestAppShortcutManager(Profile* profile)
    : AppShortcutManager(profile) {}

TestAppShortcutManager::~TestAppShortcutManager() = default;

void TestAppShortcutManager::SetNextCreateShortcutsResult(const AppId& app_id,
                                                          bool success) {
  DCHECK(!base::Contains(next_create_shortcut_results_, app_id));
  next_create_shortcut_results_[app_id] = success;
}

bool TestAppShortcutManager::CanCreateShortcuts() const {
  return can_create_shortcuts_;
}

void TestAppShortcutManager::CreateShortcuts(const AppId& app_id,
                                             bool on_desktop,
                                             CreateShortcutsCallback callback) {
  ++num_create_shortcuts_calls_;
  did_add_to_desktop_ = on_desktop;

  bool success = true;

  auto it = next_create_shortcut_results_.find(app_id);
  if (it != next_create_shortcut_results_.end()) {
    success = it->second;
    next_create_shortcut_results_.erase(app_id);
  }

  base::ThreadTaskRunnerHandle::Get()->PostTask(
      FROM_HERE, base::BindOnce(&TestAppShortcutManager::OnShortcutsCreated,
                                weak_ptr_factory_.GetWeakPtr(), app_id,
                                std::move(callback), success));
}

void TestAppShortcutManager::GetShortcutInfoForApp(
    const AppId& app_id,
    GetShortcutInfoCallback callback) {
  NOTIMPLEMENTED();
}

}  // namespace web_app
