// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/lens/lens_metrics.h"

#include "base/metrics/histogram_functions.h"

namespace lens {

void RecordAmbientSearchQuery(AmbientSearchEntryPoint entry_point) {
  base::UmaHistogramEnumeration(kAmbientSearchQueryHistogramName, entry_point);
}

}  // namespace lens
