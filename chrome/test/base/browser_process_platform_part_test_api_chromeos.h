// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_TEST_BASE_BROWSER_PROCESS_PLATFORM_PART_TEST_API_CHROMEOS_H_
#define CHROME_TEST_BASE_BROWSER_PROCESS_PLATFORM_PART_TEST_API_CHROMEOS_H_

#include <memory>

#include "base/macros.h"

class BrowserProcessPlatformPart;

namespace component_updater {
class CrOSComponentManager;
}

// Used to override parts of BrowserProcessPlatformParts in tests.
class BrowserProcessPlatformPartTestApi {
 public:
  explicit BrowserProcessPlatformPartTestApi(
      BrowserProcessPlatformPart* platform_part);
  ~BrowserProcessPlatformPartTestApi();

  // Initializes cros component manager for tests. Expects that cros component
  // manager has not previously been initialized.
  void InitializeCrosComponentManager(
      std::unique_ptr<component_updater::CrOSComponentManager>
          cros_component_manager);

  // Shuts down the cros component manager set by
  // InitializeCrosComponentManager().
  void ShutdownCrosComponentManager();

 private:
  BrowserProcessPlatformPart* const platform_part_;

  DISALLOW_COPY_AND_ASSIGN(BrowserProcessPlatformPartTestApi);
};

#endif  // CHROME_TEST_BASE_BROWSER_PROCESS_PLATFORM_PART_TEST_API_CHROMEOS_H_
