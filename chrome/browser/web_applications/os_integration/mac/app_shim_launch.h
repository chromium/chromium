// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_WEB_APPLICATIONS_OS_INTEGRATION_MAC_APP_SHIM_LAUNCH_H_
#define CHROME_BROWSER_WEB_APPLICATIONS_OS_INTEGRATION_MAC_APP_SHIM_LAUNCH_H_

#include <memory>
#include <string>
#include <vector>

#include "base/files/file_path.h"
#include "base/functional/callback_forward.h"
#include "base/process/process.h"
#include "url/gurl.h"

namespace web_app {

struct ShortcutInfo;

enum class LaunchShimUpdateBehavior {
  kDoNotRecreate,
  kRecreateIfInstalled,
  kRecreateUnconditionally,
};

inline bool RecreateShimsRequested(LaunchShimUpdateBehavior update_behavior) {
  switch (update_behavior) {
    case LaunchShimUpdateBehavior::kDoNotRecreate:
      return false;
    case LaunchShimUpdateBehavior::kRecreateIfInstalled:
    case LaunchShimUpdateBehavior::kRecreateUnconditionally:
      return true;
  }
  NOTREACHED_IN_MIGRATION();
}

enum class ShimLaunchMode {
  // Launch the app shim as a normal application.
  kNormal,
  // Launch the app shim in background mode, invisible to the user.
  kBackground,
};

// Callback type for LaunchShim. If |shim_process| is valid then the
// app shim was launched.
using ShimLaunchedCallback =
    base::OnceCallback<void(base::Process shim_process)>;

// Callback on termination takes no arguments.
using ShimTerminatedCallback = base::OnceClosure;

// Launch the shim specified by |shortcut_info|. Update the shim prior to launch
// if requested. Return in |launched_callback| the pid that was launched (or an
// invalid pid if none was launched). If |launched_callback| returns a valid
// pid, then |terminated_callback| will be called when that process terminates.
void LaunchShim(LaunchShimUpdateBehavior update_behavior,
                ShimLaunchMode launch_mode,
                ShimLaunchedCallback launched_callback,
                ShimTerminatedCallback terminated_callback,
                std::unique_ptr<ShortcutInfo> shortcut_info);

// Launch the shim specified by `shim_path` as if the user launched it directly,
// except making sure that it connects to the currently running chrome or
// browser_test instance.
// If `urls` is not empty, the app is launched to handle those urls.
// Return in `launched_callback` the pid that was launched (or an invalid pid
// if none was launched). If `launched_callback` returns a valid pid, then
// `terminated_callback` will be called when that process terminates.
void LaunchShimForTesting(
    const base::FilePath& shim_path,
    const std::vector<GURL>& urls,
    ShimLaunchedCallback launched_callback,
    ShimTerminatedCallback terminated_callback,
    const base::FilePath& chromium_path = base::FilePath());

// Waits for the shim with the given `app_id` and `shim_path` to terminate. If
// there is no running application matching `app_id` and `shim_path` returns
// immediately.
// If `terminate_shim` is true, causes the shim to terminate before waiting.
void WaitForShimToQuitForTesting(const base::FilePath& shim_path,
                                 const std::string& app_id,
                                 bool terminate_shim = false);

}  // namespace web_app

#endif  // CHROME_BROWSER_WEB_APPLICATIONS_OS_INTEGRATION_MAC_APP_SHIM_LAUNCH_H_
