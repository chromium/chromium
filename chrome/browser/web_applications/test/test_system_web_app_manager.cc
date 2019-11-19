// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/web_applications/test/test_system_web_app_manager.h"

#include <string>
#include <utility>

namespace web_app {

TestSystemWebAppManager::TestSystemWebAppManager(Profile* profile)
    : SystemWebAppManager(profile) {
  SetSystemApps(base::flat_map<SystemAppType, SystemAppInfo>());
}

TestSystemWebAppManager::~TestSystemWebAppManager() = default;

void TestSystemWebAppManager::SetSystemApps(
    base::flat_map<SystemAppType, SystemAppInfo> system_apps) {
  SetSystemAppsForTesting(std::move(system_apps));
}

void TestSystemWebAppManager::SetUpdatePolicy(
    SystemWebAppManager::UpdatePolicy policy) {
  SetUpdatePolicyForTesting(policy);
}

const base::Version& TestSystemWebAppManager::CurrentVersion() const {
  return current_version_;
}

const std::string& TestSystemWebAppManager::CurrentLocale() const {
  return current_locale_;
}

}  // namespace web_app
