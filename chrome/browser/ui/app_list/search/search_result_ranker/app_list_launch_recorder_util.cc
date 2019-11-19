// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/app_list/search/search_result_ranker/app_list_launch_recorder_util.h"

#include "base/metrics/histogram_macros.h"

namespace app_list {

void LogMetricsProviderError(MetricsProviderError error) {
  UMA_HISTOGRAM_ENUMERATION("Apps.AppListLaunchRecorderError", error);
}

}  // namespace app_list
