// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chromeos/ash/components/demo_mode/utils/demo_session_utils.h"

#include "chromeos/ash/components/install_attributes/install_attributes.h"

namespace ash::demo_mode {

bool IsDeviceInDemoMode() {
  if (!InstallAttributes::IsInitialized()) {
    // TODO(b/281905036): Add a log to indicate that the install
    // attributes haven't been initialized yet.
    return false;
  }

  return InstallAttributes::Get()->IsDeviceInDemoMode();
}

}  // namespace ash::demo_mode
