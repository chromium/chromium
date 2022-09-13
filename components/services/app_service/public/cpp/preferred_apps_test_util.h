// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_SERVICES_APP_SERVICE_PUBLIC_CPP_PREFERRED_APPS_TEST_UTIL_H_
#define COMPONENTS_SERVICES_APP_SERVICE_PUBLIC_CPP_PREFERRED_APPS_TEST_UTIL_H_

#include <memory>
#include <string>

#include "base/scoped_observation.h"
#include "components/services/app_service/public/cpp/preferred_apps_list_handle.h"

namespace base {
class RunLoop;
}

namespace apps_util {

// Utility to wait for a change in preferred apps settings to be reflected in a
// PreferredAppsList. This is useful for Lacros Crosapi tests where the
// preferred apps settings need to be synchronized between processes.
class PreferredAppUpdateWaiter
    : public apps::PreferredAppsListHandle::Observer {
 public:
  explicit PreferredAppUpdateWaiter(apps::PreferredAppsListHandle& handle);
  ~PreferredAppUpdateWaiter() override;

  void WaitForPreferredAppUpdate(const std::string& app_id);

  // apps::PreferredAppsListHandle::Observer:
  void OnPreferredAppChanged(const std::string& app_id,
                             bool is_preferred_app) override;
  void OnPreferredAppsListWillBeDestroyed(
      apps::PreferredAppsListHandle* handle) override;

 private:
  std::unique_ptr<base::RunLoop> run_loop_;
  std::string waiting_app_id_;

  base::ScopedObservation<apps::PreferredAppsListHandle,
                          apps::PreferredAppsListHandle::Observer>
      observation_{this};
};

}  // namespace apps_util

#endif  // COMPONENTS_SERVICES_APP_SERVICE_PUBLIC_CPP_PREFERRED_APPS_TEST_UTIL_H_
