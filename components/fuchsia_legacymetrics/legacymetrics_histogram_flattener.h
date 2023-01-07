// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_FUCHSIA_LEGACYMETRICS_LEGACYMETRICS_HISTOGRAM_FLATTENER_H_
#define COMPONENTS_FUCHSIA_LEGACYMETRICS_LEGACYMETRICS_HISTOGRAM_FLATTENER_H_

#include <fuchsia/legacymetrics/cpp/fidl.h>
#include <vector>

#include "base/metrics/histogram_flattener.h"
#include "base/metrics/histogram_snapshot_manager.h"

namespace fuchsia_legacymetrics {

std::vector<fuchsia::legacymetrics::Histogram> GetLegacyMetricsDeltas();

}  // namespace fuchsia_legacymetrics

#endif  // COMPONENTS_FUCHSIA_LEGACYMETRICS_LEGACYMETRICS_HISTOGRAM_FLATTENER_H_
