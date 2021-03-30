// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/metrics/structured/structured_metrics_features.h"

namespace metrics {
namespace structured {

// TODO(b/181724341): Remove this experimental once the feature is rolled out.
const base::Feature kBluetoothSessionizedMetrics{
    "BluetoothSessionizedMetrics", base::FEATURE_DISABLED_BY_DEFAULT};

}  // namespace structured
}  // namespace metrics
