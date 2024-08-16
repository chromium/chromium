// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_PERFORMANCE_MANAGER_PUBLIC_GRAPH_COMMON_TYPES_H_
#define COMPONENTS_PERFORMANCE_MANAGER_PUBLIC_GRAPH_COMMON_TYPES_H_

#include <stdint.h>

#include "base/types/strong_alias.h"

namespace performance_manager {

using WebLockNameHash = base::StrongAlias<class WebLockNameHashTag, uint32_t>;
using IndexedDBLockNameHash =
    base::StrongAlias<class IndexedDBLockNameHashTag, uint32_t>;

}  // namespace performance_manager

#endif  // COMPONENTS_PERFORMANCE_MANAGER_PUBLIC_GRAPH_COMMON_TYPES_H_
