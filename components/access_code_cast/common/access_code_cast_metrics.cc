// Copyright 2022 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/access_code_cast/common/access_code_cast_metrics.h"

#include "base/metrics/histogram_functions.h"

AccessCodeCastMetrics::AccessCodeCastMetrics() = default;
AccessCodeCastMetrics::~AccessCodeCastMetrics() = default;

// static
const char AccessCodeCastMetrics::kHistogramDialogOpenLocation[] =
    "AccessCodeCast.Ui.DialogOpenLocation";

// static
void AccessCodeCastMetrics::RecordDialogOpenLocation(
    AccessCodeCastDialogOpenLocation location) {
  base::UmaHistogramEnumeration(kHistogramDialogOpenLocation, location);
}