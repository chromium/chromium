// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_ATTRIBUTION_REPORTING_IS_ORIGIN_SUITABLE_H_
#define COMPONENTS_ATTRIBUTION_REPORTING_IS_ORIGIN_SUITABLE_H_

#include "base/component_export.h"

namespace url {
class Origin;
}  // namespace url

namespace attribution_reporting {

// TODO: Fix this layering violation when Private Aggregation supports multiple
// coordinators.
COMPONENT_EXPORT(ATTRIBUTION_REPORTING_IS_ORIGIN_SUITABLE)
bool IsOriginSuitable(const url::Origin&);

}  // namespace attribution_reporting

#endif  // COMPONENTS_ATTRIBUTION_REPORTING_IS_ORIGIN_SUITABLE_H_
