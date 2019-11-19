// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/app_list/search/search_result_ranker/app_list_launch_metrics_provider.h"

#include "chrome/browser/ui/app_list/search/search_result_ranker/app_list_launch_recorder.h"
#include "chrome/browser/ui/app_list/search/search_result_ranker/app_list_launch_recorder_util.h"

namespace app_list {
namespace {

using ::metrics::ChromeUserMetricsExtension;

}  // namespace

int AppListLaunchMetricsProvider::kMaxEventsPerUpload = 100;

AppListLaunchMetricsProvider::AppListLaunchMetricsProvider() = default;
AppListLaunchMetricsProvider::~AppListLaunchMetricsProvider() = default;

void AppListLaunchMetricsProvider::OnRecordingEnabled() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  subscription_ = AppListLaunchRecorder::GetInstance()->RegisterCallback(
      base::BindRepeating(&AppListLaunchMetricsProvider::OnAppListLaunch,
                          weak_factory_.GetWeakPtr()));
}

void AppListLaunchMetricsProvider::OnRecordingDisabled() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  subscription_.reset();
}

void AppListLaunchMetricsProvider::Initialize() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
}

void AppListLaunchMetricsProvider::ProvideCurrentSessionData(
    ChromeUserMetricsExtension* uma_proto) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
}

void AppListLaunchMetricsProvider::OnAppListLaunch(
    const AppListLaunchRecorder::LaunchInfo& launch_info) {
}

}  // namespace app_list
