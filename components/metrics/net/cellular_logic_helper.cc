// Copyright 2016 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/metrics/net/cellular_logic_helper.h"

#include "base/time/time.h"
#include "build/build_config.h"
#include "build/chromeos_buildflags.h"
#include "net/base/network_change_notifier.h"

namespace metrics {

namespace {

#if BUILDFLAG(IS_ANDROID) || BUILDFLAG(IS_IOS)
const int kStandardUploadIntervalSeconds = 5 * 60;  // Five minutes.
#else
const int kStandardUploadIntervalSeconds = 30 * 60;  // Thirty minutes.
#endif

// Android-only cellular settings.
#if BUILDFLAG(IS_ANDROID)
const int kStandardUploadIntervalCellularSeconds = 15 * 60;  // Fifteen minutes.
#endif

}  // namespace

base::TimeDelta GetUploadInterval(bool use_cellular_upload_interval) {
#if BUILDFLAG(IS_ANDROID)
  if (use_cellular_upload_interval)
    return base::Seconds(kStandardUploadIntervalCellularSeconds);
#endif
  return base::Seconds(kStandardUploadIntervalSeconds);
}

bool ShouldUseCellularUploadInterval() {
#if BUILDFLAG(IS_ANDROID)
  return net::NetworkChangeNotifier::IsConnectionCellular(
      net::NetworkChangeNotifier::GetConnectionType());
#else
  return false;
#endif
}

}  // namespace metrics
