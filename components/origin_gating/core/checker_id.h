// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_ORIGIN_GATING_CORE_CHECKER_ID_H_
#define COMPONENTS_ORIGIN_GATING_CORE_CHECKER_ID_H_

#include "base/types/id_type.h"

namespace origin_gating {

// Identifier used to look up and manage an OriginGatingChecker registered
// with the OriginGatingService.
using CheckerId = base::IdType32<class CheckerIdTag>;

}  // namespace origin_gating

#endif  // COMPONENTS_ORIGIN_GATING_CORE_CHECKER_ID_H_
