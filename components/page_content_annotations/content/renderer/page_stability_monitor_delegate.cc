// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/page_content_annotations/content/renderer/page_stability_monitor_delegate.h"

#include "base/time/time.h"
#include "components/page_content_annotations/core/page_content_annotations_features.h"

namespace page_content_annotations {

base::TimeDelta PageStabilityMonitorDelegate::GetTimeoutDelay() const {
  return features::kPageStabilityTimeout.Get();
}

base::TimeDelta PageStabilityMonitorDelegate::GetMinWait() const {
  return features::kPageStabilityMinWait.Get();
}

base::TimeDelta PageStabilityMonitorDelegate::GetInitialPaintTimeout() const {
  return features::kPaintStabilityInitialPaintTimeout.Get();
}

base::TimeDelta PageStabilityMonitorDelegate::GetSubsequentPaintTimeout()
    const {
  return features::kPaintStabilitySubsequentPaintTimeout.Get();
}

}  // namespace page_content_annotations
