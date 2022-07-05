// Copyright 2022 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/services/app_service/public/cpp/preferred_apps_test_util.h"

#include "base/run_loop.h"

namespace apps_util {

PreferredAppUpdateWaiter::PreferredAppUpdateWaiter(
    apps::PreferredAppsListHandle& handle) {
  observation_.Observe(&handle);
}

PreferredAppUpdateWaiter::~PreferredAppUpdateWaiter() = default;

void PreferredAppUpdateWaiter::WaitForPreferredAppUpdate(
    const std::string& app_id) {
  waiting_app_id_ = app_id;
  run_loop_ = std::make_unique<base::RunLoop>();
  run_loop_->Run();
}

// apps::PreferredAppsListHandle::Observer:
void PreferredAppUpdateWaiter::OnPreferredAppChanged(const std::string& app_id,
                                                     bool is_preferred_app) {
  if (run_loop_ && run_loop_->running() && app_id == waiting_app_id_) {
    run_loop_->Quit();
  }
}

void PreferredAppUpdateWaiter::OnPreferredAppsListWillBeDestroyed(
    apps::PreferredAppsListHandle* handle) {
  observation_.Reset();
}

}  // namespace apps_util
