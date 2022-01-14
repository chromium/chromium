// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/metrics/net/cellular_logic_helper.h"

#include "base/feature_list.h"
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

#if BUILDFLAG(IS_CHROMEOS_ASH)
// A feature to control whether we upload UMA logs more frequently.
const base::Feature kMoreFrequentUmaUploads{"MoreFrequentUmaUploads",
                                            base::FEATURE_DISABLED_BY_DEFAULT};
// The interval between these more-frequent uploads.
constexpr base::TimeDelta kMoreFrequentUploadInterval = base::Minutes(5);
#endif  // IS_CHROMEOS_ASH

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
#elif BUILDFLAG(IS_CHROMEOS_ASH)
  if (base::FeatureList::IsEnabled(kMoreFrequentUmaUploads)) {
    return kMoreFrequentUploadInterval;
  }
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
