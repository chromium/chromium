// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/test/base/browser_process_platform_part_test_api_chromeos.h"

#include <utility>

#include "chrome/browser/browser_process_platform_part_chromeos.h"
#include "chrome/browser/component_updater/cros_component_manager.h"

BrowserProcessPlatformPartTestApi::BrowserProcessPlatformPartTestApi(
    BrowserProcessPlatformPart* platform_part)
    : platform_part_(platform_part) {}

BrowserProcessPlatformPartTestApi::~BrowserProcessPlatformPartTestApi() {
  DCHECK(!platform_part_->using_testing_cros_component_manager_);
}

void BrowserProcessPlatformPartTestApi::InitializeCrosComponentManager(
    std::unique_ptr<component_updater::CrOSComponentManager>
        cros_component_manager) {
  DCHECK(!platform_part_->using_testing_cros_component_manager_);
  DCHECK(!platform_part_->cros_component_manager_);

  platform_part_->using_testing_cros_component_manager_ = true;
  platform_part_->cros_component_manager_ = std::move(cros_component_manager);
}

void BrowserProcessPlatformPartTestApi::ShutdownCrosComponentManager() {
  DCHECK(platform_part_->using_testing_cros_component_manager_);
  platform_part_->using_testing_cros_component_manager_ = false;
  platform_part_->cros_component_manager_.reset();
}
