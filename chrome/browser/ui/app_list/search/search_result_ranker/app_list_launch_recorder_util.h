// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_APP_LIST_SEARCH_SEARCH_RESULT_RANKER_APP_LIST_LAUNCH_RECORDER_UTIL_H_
#define CHROME_BROWSER_UI_APP_LIST_SEARCH_SEARCH_RESULT_RANKER_APP_LIST_LAUNCH_RECORDER_UTIL_H_

namespace app_list {

// Represents errors states of the app list launch metrics provider. These
// values persist to logs. Entries should not be renumbered and numeric values
// should never be reused.
enum class MetricsProviderError {
  kStateReadError = 0,
  kStateWriteError = 1,
  kStateFromProtoError = 2,
  kStateToProtoError = 3,
  kNoStateProto = 4,
  kInvalidUserId = 5,
  kInvalidSecret = 6,
  kMaxEventsPerUploadExceeded = 7,
  kLaunchTypeUnspecified = 8,
  kMaxValue = kLaunchTypeUnspecified,
};

void LogMetricsProviderError(MetricsProviderError error);

}  // namespace app_list

#endif  // CHROME_BROWSER_UI_APP_LIST_SEARCH_SEARCH_RESULT_RANKER_APP_LIST_LAUNCH_RECORDER_UTIL_H_
