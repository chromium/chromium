// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROMEOS_ASH_EXPERIENCES_ARC_TEST_MOCK_ARC_DLC_INSTALL_HARDWARE_CHECKER_H_
#define CHROMEOS_ASH_EXPERIENCES_ARC_TEST_MOCK_ARC_DLC_INSTALL_HARDWARE_CHECKER_H_

#include <gmock/gmock.h>

#include "chromeos/ash/experiences/arc/dlc_installer/arc_dlc_install_hardware_checker.h"

namespace arc {
class MockArcDlcInstallHardwareChecker : public ArcDlcInstallHardwareChecker {
 public:
  MockArcDlcInstallHardwareChecker();
  MockArcDlcInstallHardwareChecker(const MockArcDlcInstallHardwareChecker&) =
      delete;
  MockArcDlcInstallHardwareChecker& operator=(
      const MockArcDlcInstallHardwareChecker&) = delete;
  ~MockArcDlcInstallHardwareChecker() override;

  MOCK_METHOD(void,
              IsCompatible,
              (base::OnceCallback<void(bool)> callback),
              (override));
};

MockArcDlcInstallHardwareChecker::MockArcDlcInstallHardwareChecker() = default;

MockArcDlcInstallHardwareChecker::~MockArcDlcInstallHardwareChecker() = default;

}  // namespace arc

#endif  // CHROMEOS_ASH_EXPERIENCES_ARC_TEST_MOCK_ARC_DLC_INSTALL_HARDWARE_CHECKER_H_
