// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_ATTRIBUTION_REPORTING_DEBUG_TYPES_H_
#define COMPONENTS_ATTRIBUTION_REPORTING_DEBUG_TYPES_H_

#include <string_view>

#include "base/component_export.h"
#include "base/containers/enum_set.h"
#include "base/types/expected.h"
#include "components/attribution_reporting/debug_types.mojom.h"

namespace attribution_reporting {

struct ParseError;

COMPONENT_EXPORT(ATTRIBUTION_REPORTING)
std::string_view SerializeDebugDataType(mojom::DebugDataType);

COMPONENT_EXPORT(ATTRIBUTION_REPORTING)
base::expected<mojom::DebugDataType, ParseError> ParseSourceDebugDataType(
    std::string_view);

COMPONENT_EXPORT(ATTRIBUTION_REPORTING)
base::expected<mojom::DebugDataType, ParseError> ParseTriggerDebugDataType(
    std::string_view);

using DebugDataTypes = base::EnumSet<mojom::DebugDataType,
                                     mojom::DebugDataType::kMinValue,
                                     mojom::DebugDataType::kMaxValue>;

COMPONENT_EXPORT(ATTRIBUTION_REPORTING)
DebugDataTypes SourceDebugDataTypes();

COMPONENT_EXPORT(ATTRIBUTION_REPORTING)
DebugDataTypes TriggerDebugDataTypes();

}  // namespace attribution_reporting

#endif  // COMPONENTS_ATTRIBUTION_REPORTING_DEBUG_TYPES_H_
