// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_TEST_BASE_BROWSER_PROCESS_PLATFORM_PART_TEST_API_CHROMEOS_H_
#define CHROME_TEST_BASE_BROWSER_PROCESS_PLATFORM_PART_TEST_API_CHROMEOS_H_

#include "base/memory/raw_ptr.h"
#include "chrome/browser/component_updater/cros_component_manager.h"

class BrowserProcessPlatformPart;
class Profile;

// Used to override parts of BrowserProcessPlatformParts in tests.
class BrowserProcessPlatformPartTestApi {
 public:
  explicit BrowserProcessPlatformPartTestApi(
      BrowserProcessPlatformPart* platform_part);
  BrowserProcessPlatformPartTestApi(const BrowserProcessPlatformPartTestApi&) =
      delete;
  BrowserProcessPlatformPartTestApi& operator=(
      const BrowserProcessPlatformPartTestApi&) = delete;
  ~BrowserProcessPlatformPartTestApi();

  // Initializes cros component manager for tests. Expects that cros component
  // manager has not previously been initialized.
  void InitializeCrosComponentManager(
      scoped_refptr<component_updater::CrOSComponentManager>
          cros_component_manager);

  // Shuts down the cros component manager set by
  // InitializeCrosComponentManager().
  void ShutdownCrosComponentManager();

  bool CanRestoreUrlsForProfile(const Profile* profile);

 private:
  const raw_ptr<BrowserProcessPlatformPart, ExperimentalAsh> platform_part_;
};

#endif  // CHROME_TEST_BASE_BROWSER_PROCESS_PLATFORM_PART_TEST_API_CHROMEOS_H_
