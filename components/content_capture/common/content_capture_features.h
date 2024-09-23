// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_CONTENT_CAPTURE_COMMON_CONTENT_CAPTURE_FEATURES_H_
#define COMPONENTS_CONTENT_CAPTURE_COMMON_CONTENT_CAPTURE_FEATURES_H_

#include "base/feature_list.h"

namespace content_capture {

namespace features {

BASE_DECLARE_FEATURE(kContentCapture);

// ContentCapture is triggered in the unpredictable conditions which might be
// changed on different aiai release or configuration push, this feature allows
// us to trigger the ContentCapture independently to get the unbiased result.
BASE_DECLARE_FEATURE(kContentCaptureTriggeringForExperiment);

bool IsContentCaptureEnabled();
bool ShouldTriggerContentCaptureForExperiment();

int TaskInitialDelayInMilliseconds();

}  // namespace features

}  // namespace content_capture

#endif  // COMPONENTS_CONTENT_CAPTURE_COMMON_CONTENT_CAPTURE_FEATURES_H_
