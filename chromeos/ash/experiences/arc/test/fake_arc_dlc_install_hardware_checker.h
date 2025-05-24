// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROMEOS_ASH_EXPERIENCES_ARC_TEST_FAKE_ARC_DLC_INSTALL_HARDWARE_CHECKER_H_
#define CHROMEOS_ASH_EXPERIENCES_ARC_TEST_FAKE_ARC_DLC_INSTALL_HARDWARE_CHECKER_H_

#include "chromeos/ash/experiences/arc/dlc_installer/arc_dlc_install_hardware_checker.h"

namespace arc {

// This is used in unit tests to simulate hardware compatibility checks
// for the ARC DLC installation process. It allows tests to control the return
// value of IsCompatible() via a preconfigured boolean result.
class FakeArcDlcInstallHardwareChecker : public ArcDlcInstallHardwareChecker {
 public:
  explicit FakeArcDlcInstallHardwareChecker(bool callback_result);

  FakeArcDlcInstallHardwareChecker(const FakeArcDlcInstallHardwareChecker&) =
      delete;
  FakeArcDlcInstallHardwareChecker& operator=(
      const FakeArcDlcInstallHardwareChecker&) = delete;

  ~FakeArcDlcInstallHardwareChecker() override;

  // Instead of performing an actual check, this method immediately invokes
  // the callback with the preconfigured callback_result_ value.
  void IsCompatible(base::OnceCallback<void(bool)> callback) override;

 private:
  bool callback_result_;  // Control the callback result in tests
};

}  // namespace arc

#endif  // CHROMEOS_ASH_EXPERIENCES_ARC_TEST_FAKE_ARC_DLC_INSTALL_HARDWARE_CHECKER_H_
