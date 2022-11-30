// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/install_static/install_details.h"

// Returns the payload of the module's details. This should be linked into the
// primary module (i.e., chrome_elf) via the "primary_module" source set and
// exported for use by other modules in the process; see
// install_static::InitializeFromPrimaryModule.
extern "C" const install_static::InstallDetails::Payload*
GetInstallDetailsPayload() {
  return install_static::InstallDetails::Get().GetPayload();
}
