// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_WEB_APPLICATIONS_ISOLATED_WEB_APPS_GET_CONTROLLED_FRAME_PARTITION_COMMAND_H_
#define CHROME_BROWSER_WEB_APPLICATIONS_ISOLATED_WEB_APPS_GET_CONTROLLED_FRAME_PARTITION_COMMAND_H_

#include <string>

#include "base/functional/callback_forward.h"
#include "base/values.h"
#include "chrome/browser/web_applications/isolated_web_apps/isolated_web_app_url_info.h"
#include "chrome/browser/web_applications/web_app_command_scheduler.h"
#include "third_party/abseil-cpp/absl/types/optional.h"

class Profile;

namespace content {
class StoragePartitionConfig;
}  // namespace content

namespace web_app {

class AppLock;

// Runs |callback| with the StoragePartitionConfig that should be used for a
// <controlledframe> with the given |partition_name|, and registers the
// StoragePartition with the web_app system if needed.
base::Value GetControlledFramePartitionWithLock(
    Profile* profile,
    const IsolatedWebAppUrlInfo& url_info,
    const std::string& partition_name,
    bool in_memory,
    base::OnceCallback<void(absl::optional<content::StoragePartitionConfig>)>
        callback,
    AppLock& lock);

}  // namespace web_app

#endif  // CHROME_BROWSER_WEB_APPLICATIONS_ISOLATED_WEB_APPS_GET_CONTROLLED_FRAME_PARTITION_COMMAND_H_
