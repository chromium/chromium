// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/services/sharing/public/cpp/sharing_webrtc_metrics.h"

#include "base/metrics/histogram_functions.h"
#include "base/strings/string_util.h"

namespace {

// Common prefix for all webrtc metric in Sharing service.
const char kMetricsPrefix[] = "Sharing.WebRtc";

}  // namespace

namespace sharing {

void LogWebRtcIceConfigFetched(int count) {
  base::UmaHistogramExactLinear(
      base::JoinString({kMetricsPrefix, "IceConfigFetched"}, "."), count,
      /*value_max=*/10);
}

}  // namespace sharing
