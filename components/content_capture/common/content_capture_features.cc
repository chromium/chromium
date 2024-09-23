// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/content_capture/common/content_capture_features.h"

#include "base/feature_list.h"
#include "base/metrics/field_trial_params.h"
#include "build/build_config.h"

namespace content_capture {
namespace features {

#if BUILDFLAG(IS_ANDROID)
BASE_FEATURE(kContentCapture,
             "ContentCapture",
             base::FEATURE_ENABLED_BY_DEFAULT);

BASE_FEATURE(kContentCaptureTriggeringForExperiment,
             "ContentCaptureTriggeringForExperiment",
             base::FEATURE_DISABLED_BY_DEFAULT);
#else
BASE_FEATURE(kContentCapture,
             "ContentCapture",
             base::FEATURE_DISABLED_BY_DEFAULT);

BASE_FEATURE(kContentCaptureTriggeringForExperiment,
             "ContentCaptureTriggeringForExperiment",
             base::FEATURE_DISABLED_BY_DEFAULT);
#endif

bool IsContentCaptureEnabled() {
  return base::FeatureList::IsEnabled(kContentCapture);
}

bool ShouldTriggerContentCaptureForExperiment() {
  return base::FeatureList::IsEnabled(kContentCaptureTriggeringForExperiment);
}

int TaskInitialDelayInMilliseconds() {
  return base::GetFieldTrialParamByFeatureAsInt(
      kContentCapture, "task_initial_delay_in_milliseconds", 500);
}

}  // namespace features
}  // namespace content_capture
