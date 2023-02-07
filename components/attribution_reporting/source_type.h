// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_ATTRIBUTION_REPORTING_SOURCE_TYPE_H_
#define COMPONENTS_ATTRIBUTION_REPORTING_SOURCE_TYPE_H_

#include "base/component_export.h"
#include "components/attribution_reporting/source_type.mojom-forward.h"

namespace attribution_reporting {

// Returns the string representation of the source type, suitable for inclusion
// in report bodies.
COMPONENT_EXPORT(ATTRIBUTION_REPORTING)
const char* SourceTypeName(mojom::SourceType);

}  // namespace attribution_reporting

#endif  // COMPONENTS_ATTRIBUTION_REPORTING_SOURCE_TYPE_H_
