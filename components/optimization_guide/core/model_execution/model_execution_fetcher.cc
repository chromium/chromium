// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/optimization_guide/core/model_execution/model_execution_fetcher.h"

#include "base/metrics/histogram_functions.h"
#include "base/strings/strcat.h"
#include "components/optimization_guide/core/model_execution/feature_keys.h"
#include "components/optimization_guide/core/optimization_guide_util.h"

namespace optimization_guide {

// static
void ModelExecutionFetcher::RecordRequestStatusHistogram(
    ModelBasedCapabilityKey feature,
    FetcherRequestStatus status) {
  base::UmaHistogramEnumeration(
      base::StrCat({"OptimizationGuide.ModelExecutionFetcher.RequestStatus.",
                    GetStringNameForModelExecutionFeature(feature)}),
      status);
}

}  // namespace optimization_guide
