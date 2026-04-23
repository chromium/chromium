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

}  // namespace vrp_flags

#endif  // COMPONENTS_VRP_FLAGS_VRP_FLAGS_H_
