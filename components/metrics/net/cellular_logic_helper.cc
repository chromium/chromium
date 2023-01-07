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

// Standard interval between log uploads, in seconds.
#if BUILDFLAG(IS_ANDROID) || BUILDFLAG(IS_IOS)
const int kStandardUploadIntervalSeconds = 5 * 60;           // Five minutes.
const int kStandardUploadIntervalCellularSeconds = 15 * 60;  // Fifteen minutes.
#else
const int kStandardUploadIntervalSeconds = 30 * 60;  // Thirty minutes.
#endif

#if BUILDFLAG(IS_ANDROID)
const bool kDefaultCellularLogicEnabled = true;
#else
const bool kDefaultCellularLogicEnabled = false;
#endif

}  // namespace

base::TimeDelta GetUploadInterval(bool use_cellular_upload_interval) {
#if BUILDFLAG(IS_ANDROID) || BUILDFLAG(IS_IOS)
  if (use_cellular_upload_interval)
    return base::Seconds(kStandardUploadIntervalCellularSeconds);
#endif
  return base::Seconds(kStandardUploadIntervalSeconds);
}

bool ShouldUseCellularUploadInterval() {
  if (!kDefaultCellularLogicEnabled)
    return false;

  return net::NetworkChangeNotifier::IsConnectionCellular(
      net::NetworkChangeNotifier::GetConnectionType());
}

}  // namespace metrics
