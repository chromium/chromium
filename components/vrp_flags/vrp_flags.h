// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_VRP_FLAGS_VRP_FLAGS_H_
#define COMPONENTS_VRP_FLAGS_VRP_FLAGS_H_

#include "base/component_export.h"

namespace vrp_flags {

namespace switches {
COMPONENT_EXPORT(VRP_FLAGS) extern const char kVrpFlags[];
}

// Returns true if the --vrp-flags flag is set on the command line.
// This function memoizes the result for efficiency.
COMPONENT_EXPORT(VRP_FLAGS) bool IsEnabled();

// Performs early initialization tasks if VRP flags are enabled, such as
// mapping victim.test to 127.0.0.1:8000 for the cross-renderer CTF.
COMPONENT_EXPORT(VRP_FLAGS) void PostEarlyInitialization();

}  // namespace vrp_flags

#endif  // COMPONENTS_VRP_FLAGS_VRP_FLAGS_H_
