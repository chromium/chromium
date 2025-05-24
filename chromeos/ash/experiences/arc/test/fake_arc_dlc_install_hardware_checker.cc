// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chromeos/ash/experiences/arc/test/fake_arc_dlc_install_hardware_checker.h"

namespace arc {

FakeArcDlcInstallHardwareChecker::FakeArcDlcInstallHardwareChecker(
    bool callback_result)
    : callback_result_(callback_result) {}

FakeArcDlcInstallHardwareChecker::~FakeArcDlcInstallHardwareChecker() = default;

void FakeArcDlcInstallHardwareChecker::IsCompatible(
    base::OnceCallback<void(bool)> callback) {
  std::move(callback).Run(callback_result_);
}

}  // namespace arc
