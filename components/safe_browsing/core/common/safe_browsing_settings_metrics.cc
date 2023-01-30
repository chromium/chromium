// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/metrics/user_metrics.h"
#include "base/metrics/user_metrics_action.h"

namespace safe_browsing {

void LogShowEnhancedProtectionAction() {
  base::RecordAction(
      base::UserMetricsAction("Options_ShowSafeBrowsingEnhancedProtection"));
}

}  // namespace safe_browsing