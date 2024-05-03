// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/test/base/browser_process_platform_part_test_api_chromeos.h"

#include <utility>

#include "chrome/browser/browser_process_platform_part_ash.h"
#include "components/component_updater/ash/component_manager_ash.h"

BrowserProcessPlatformPartTestApi::BrowserProcessPlatformPartTestApi(
    BrowserProcessPlatformPart* platform_part)
    : platform_part_(platform_part) {}

BrowserProcessPlatformPartTestApi::~BrowserProcessPlatformPartTestApi() {
  DCHECK(!platform_part_->using_testing_component_manager_ash_);
}

void BrowserProcessPlatformPartTestApi::InitializeComponentManager(
    scoped_refptr<component_updater::ComponentManagerAsh>
        component_manager_ash) {
  DCHECK(!platform_part_->using_testing_component_manager_ash_);
  DCHECK(!platform_part_->component_manager_ash_);

  platform_part_->using_testing_component_manager_ash_ = true;
  platform_part_->component_manager_ash_ = std::move(component_manager_ash);
}

void BrowserProcessPlatformPartTestApi::ShutdownComponentManager() {
  DCHECK(platform_part_->using_testing_component_manager_ash_);
  platform_part_->using_testing_component_manager_ash_ = false;
  platform_part_->component_manager_ash_.reset();
}

bool BrowserProcessPlatformPartTestApi::CanRestoreUrlsForProfile(
    const Profile* profile) {
  return platform_part_->CanRestoreUrlsForProfile(profile);
}
