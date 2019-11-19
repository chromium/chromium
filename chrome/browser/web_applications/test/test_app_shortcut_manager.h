// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_WEB_APPLICATIONS_TEST_TEST_APP_SHORTCUT_MANAGER_H_
#define CHROME_BROWSER_WEB_APPLICATIONS_TEST_TEST_APP_SHORTCUT_MANAGER_H_

#include <map>

#include "base/memory/weak_ptr.h"
#include "base/optional.h"
#include "chrome/browser/web_applications/components/app_shortcut_manager.h"
#include "chrome/browser/web_applications/components/web_app_helpers.h"

class Profile;

namespace web_app {

class TestAppShortcutManager : public AppShortcutManager {
 public:
  explicit TestAppShortcutManager(Profile* profile);
  ~TestAppShortcutManager() override;

  size_t num_create_shortcuts_calls() const {
    return num_create_shortcuts_calls_;
  }

  void set_can_create_shortcuts(bool can_create_shortcuts) {
    can_create_shortcuts_ = can_create_shortcuts;
  }

  base::Optional<bool> did_add_to_desktop() const {
    return did_add_to_desktop_;
  }

  void SetNextCreateShortcutsResult(const AppId& app_id, bool success);

  // AppShortcutManager:
  bool CanCreateShortcuts() const override;
  void CreateShortcuts(const AppId& app_id,
                       bool on_desktop,
                       CreateShortcutsCallback callback) override;
  void GetShortcutInfoForApp(const AppId& app_id,
                             GetShortcutInfoCallback callback) override;

 private:
  size_t num_create_shortcuts_calls_ = 0;
  base::Optional<bool> did_add_to_desktop_;

  bool can_create_shortcuts_ = true;
  std::map<AppId, bool> next_create_shortcut_results_;

  base::WeakPtrFactory<TestAppShortcutManager> weak_ptr_factory_{this};
};

}  // namespace web_app

#endif  // CHROME_BROWSER_WEB_APPLICATIONS_TEST_TEST_APP_SHORTCUT_MANAGER_H_
