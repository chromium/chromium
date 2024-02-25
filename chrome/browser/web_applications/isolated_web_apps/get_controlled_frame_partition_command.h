// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_WEB_APPLICATIONS_ISOLATED_WEB_APPS_GET_CONTROLLED_FRAME_PARTITION_COMMAND_H_
#define CHROME_BROWSER_WEB_APPLICATIONS_ISOLATED_WEB_APPS_GET_CONTROLLED_FRAME_PARTITION_COMMAND_H_

#include <optional>
#include <string>

#include "base/functional/callback_forward.h"
#include "base/values.h"
#include "chrome/browser/web_applications/isolated_web_apps/isolated_web_app_url_info.h"
#include "chrome/browser/web_applications/web_app_command_scheduler.h"

class Profile;

namespace content {
class StoragePartitionConfig;
}  // namespace content

namespace web_app {

class AppLock;

// Runs |callback| with the StoragePartitionConfig that should be used for a
// <controlledframe> with the given |partition_name|, and registers the
// StoragePartition with the web_app system if needed.

std::optional<content::StoragePartitionConfig>
GetControlledFramePartitionWithLock(Profile* profile,
                                    const IsolatedWebAppUrlInfo& url_info,
                                    const std::string& partition_name,
                                    bool in_memory,
                                    AppLock& lock,
                                    base::Value::Dict& debug_info);

}  // namespace web_app

#endif  // CHROME_BROWSER_WEB_APPLICATIONS_ISOLATED_WEB_APPS_GET_CONTROLLED_FRAME_PARTITION_COMMAND_H_
