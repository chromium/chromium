// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_SERVICES_PRINT_COMPOSITOR_PUBLIC_CPP_PRINT_SERVICE_MOJO_TYPES_H_
#define COMPONENTS_SERVICES_PRINT_COMPOSITOR_PUBLIC_CPP_PRINT_SERVICE_MOJO_TYPES_H_

#include <stdint.h>

#include "base/containers/flat_map.h"

namespace printing {

// Create an alias for map<uint32, uint64> type.
using ContentToFrameMap = base::flat_map<uint32_t, uint64_t>;

}  // namespace printing

#endif  // COMPONENTS_SERVICES_PRINT_COMPOSITOR_PUBLIC_CPP_PRINT_SERVICE_MOJO_TYPES_H_
