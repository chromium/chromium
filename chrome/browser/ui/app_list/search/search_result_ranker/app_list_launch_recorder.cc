// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/app_list/search/search_result_ranker/app_list_launch_recorder.h"

#include <string>

#include "base/task/post_task.h"

namespace app_list {

AppListLaunchRecorder::LaunchInfo::LaunchInfo() = default;
AppListLaunchRecorder::LaunchInfo::LaunchInfo(const LaunchInfo& other) =
    default;
AppListLaunchRecorder::LaunchInfo::~LaunchInfo() = default;

AppListLaunchRecorder* AppListLaunchRecorder::GetInstance() {
  static base::NoDestructor<AppListLaunchRecorder> recorder;
  return recorder.get();
}

AppListLaunchRecorder::AppListLaunchRecorder() = default;
AppListLaunchRecorder::~AppListLaunchRecorder() = default;

std::unique_ptr<AppListLaunchRecorder::LaunchEventSubscription>
AppListLaunchRecorder::RegisterCallback(const LaunchEventCallback& callback) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  return callback_list_.Add(callback);
}

}  // namespace app_list
