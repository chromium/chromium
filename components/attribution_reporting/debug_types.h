// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_ATTRIBUTION_REPORTING_DEBUG_TYPES_H_
#define COMPONENTS_ATTRIBUTION_REPORTING_DEBUG_TYPES_H_

#include <string_view>

#include "base/component_export.h"
#include "components/attribution_reporting/debug_types.mojom-forward.h"

namespace attribution_reporting {

COMPONENT_EXPORT(ATTRIBUTION_REPORTING)
std::string_view SerializeDebugDataType(mojom::DebugDataType);

}  // namespace attribution_reporting

#endif  // COMPONENTS_ATTRIBUTION_REPORTING_DEBUG_TYPES_H_
