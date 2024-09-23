// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_TEST_BASE_BROWSER_PROCESS_PLATFORM_PART_TEST_API_CHROMEOS_H_
#define CHROME_TEST_BASE_BROWSER_PROCESS_PLATFORM_PART_TEST_API_CHROMEOS_H_

#include "base/memory/raw_ptr.h"
#include "components/component_updater/ash/component_manager_ash.h"

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

  // Initializes ComponentManagerAsh for tests. Expects that ComponentManagerAsh
  // has not previously been initialized.
  void InitializeComponentManager(
      scoped_refptr<component_updater::ComponentManagerAsh>
          cros_component_manager);

  // Shuts down ComponentManagerAsh set by InitializeComponentManager().
  void ShutdownComponentManager();

  bool CanRestoreUrlsForProfile(const Profile* profile);

 private:
  const raw_ptr<BrowserProcessPlatformPart> platform_part_;
};

#endif  // CHROME_TEST_BASE_BROWSER_PROCESS_PLATFORM_PART_TEST_API_CHROMEOS_H_
