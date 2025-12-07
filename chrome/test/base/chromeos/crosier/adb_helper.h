// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_TEST_BASE_CHROMEOS_CROSIER_ADB_HELPER_H_
#define CHROME_TEST_BASE_CHROMEOS_CROSIER_ADB_HELPER_H_

#include <string_view>

#include "base/files/scoped_temp_dir.h"
#include "chrome/test/base/chromeos/crosier/helper/test_sudo_helper_client.h"

namespace base {
class FilePath;
}

// Helper to run adb command via the TestSudoHelper.
class AdbHelper {
 public:
  AdbHelper();
  AdbHelper(const AdbHelper&) = delete;
  AdbHelper& operator=(const AdbHelper&) = delete;
  ~AdbHelper();

  // Starts adb server and connect to the first "emulator" device.
  void Intialize();

  // Installs the apk at the given path on the DUT.
  bool InstallApk(const base::FilePath& apk_path);

  // Runs the given command via adb.
  bool Command(std::string_view command);

 private:
  // Waits for the first emulator device to be ready and extract serial.
  void WaitForDevice();

  bool initialized_ = false;

  // Device serial that is passed with "-s" to adb.
  std::string serial_;

  // A temp dir to store Android vendor keys.
  base::ScopedTempDir vendor_key_dir_;
  base::FilePath vendor_key_file_;
};

#endif  // CHROME_TEST_BASE_CHROMEOS_CROSIER_ADB_HELPER_H_
