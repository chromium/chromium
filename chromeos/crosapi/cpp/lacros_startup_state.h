// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROMEOS_CROSAPI_CPP_LACROS_STARTUP_STATE_H_
#define CHROMEOS_CROSAPI_CPP_LACROS_STARTUP_STATE_H_

#include "base/component_export.h"

namespace crosapi {

namespace lacros_startup_state {

// Transfers the Lacros startup state from the browser level to lower levels
// like components. If |is_lacros_enabled| is true, Lacros is enabled, which
// means that Lacros is the only browser and Ash is only used for system
// operations.
// Note: As the state cannot state wile Ash is running (profile migration,
// browser restart and other things required) this will be set when Ash
// determines if Lacros should get launched or not.
COMPONENT_EXPORT(CROSAPI)
void SetLacrosStartupState(bool is_lacros_enabled);

// Mirroring the Lacros enabled flag for components and other lower than browser
// components for dependent feature development.
COMPONENT_EXPORT(CROSAPI) bool IsLacrosEnabled();

}  // namespace lacros_startup_state

}  // namespace crosapi

#endif  //  CHROMEOS_CROSAPI_CPP_LACROS_STARTUP_STATE_H_
