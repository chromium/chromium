// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/metrics/expired_histogram_util.h"

#include "base/feature_list.h"
#include "base/metrics/field_trial_params.h"
#include "base/metrics/statistics_recorder.h"
#include "components/metrics/expired_histograms_checker.h"

namespace metrics {
namespace {

BASE_FEATURE(kExpiredHistogramLogicFeature,
             "ExpiredHistogramLogic",
             base::FEATURE_DISABLED_BY_DEFAULT);

const base::FeatureParam<std::string> kAllowlistParam{
    &kExpiredHistogramLogicFeature, "allowlist", ""};

}  // namespace

void EnableExpiryChecker(base::span<const uint32_t> expired_histograms_hashes) {
  DCHECK(base::FeatureList::GetInstance());
  if (base::FeatureList::IsEnabled(kExpiredHistogramLogicFeature)) {
    std::string allowlist = kAllowlistParam.Get();
    base::StatisticsRecorder::SetRecordChecker(
        std::make_unique<ExpiredHistogramsChecker>(expired_histograms_hashes,
                                                   allowlist));
  }
}

}  // namespace metrics
