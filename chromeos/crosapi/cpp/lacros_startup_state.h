// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROMEOS_CROSAPI_CPP_LACROS_STARTUP_STATE_H_
#define CHROMEOS_CROSAPI_CPP_LACROS_STARTUP_STATE_H_

#include "base/component_export.h"

namespace crosapi {

namespace lacros_startup_state {

// Transfers the Lacros startup state from the browser level to lower levels
// like components.
COMPONENT_EXPORT(CROSAPI)
void SetLacrosStartupState(bool is_enabled, bool is_primary_enabled);

// Mirroring the Lacros enabled flag for components and other lower than browser
// components for dependent feature development.
COMPONENT_EXPORT(CROSAPI) bool IsLacrosEnabled();

// Mirroring the Lacros Primary enabled flag for components and other lower than
// browser components for dependent feature development.
COMPONENT_EXPORT(CROSAPI) bool IsLacrosPrimaryEnabled();

}  // namespace lacros_startup_state

}  // namespace crosapi

#endif  //  CHROMEOS_CROSAPI_CPP_LACROS_STARTUP_STATE_H_
