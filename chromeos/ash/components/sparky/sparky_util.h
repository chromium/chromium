// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROMEOS_ASH_COMPONENTS_SPARKY_SPARKY_UTIL_H_
#define CHROMEOS_ASH_COMPONENTS_SPARKY_SPARKY_UTIL_H_

#include <cstdint>

#include "base/component_export.h"

namespace sparky {

// Round |bytes| to the next power of 2, where the next power of 2 is greater
// than or equal to |bytes|.
// RoundByteSize(3) will return 4.
// RoundByteSize(4) will return 4.
int64_t COMPONENT_EXPORT(CHROMEOS_ASH_COMPONENTS_SPARKY)
    RoundByteSize(int64_t bytes);

}  // namespace sparky

#endif  // CHROMEOS_ASH_COMPONENTS_SPARKY_SPARKY_UTIL_H_
