// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/web_applications/system_web_apps/test/test_system_web_app_manager.h"

#include <memory>
#include <string>
#include <utility>
#include "chrome/browser/ash/system_web_apps/types/system_web_app_delegate.h"

namespace web_app {

TestSystemWebAppManager::TestSystemWebAppManager(Profile* profile)
    : SystemWebAppManager(profile) {
  SetSystemAppsForTesting(
      base::flat_map<ash::SystemWebAppType,
                     std::unique_ptr<ash::SystemWebAppDelegate>>());
}

TestSystemWebAppManager::~TestSystemWebAppManager() = default;

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
