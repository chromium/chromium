// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_WEB_APPLICATIONS_ISOLATED_WEB_APPS_REGISTER_CONTROLLED_FRAME_PARTITION_COMMAND_H_
#define CHROME_BROWSER_WEB_APPLICATIONS_ISOLATED_WEB_APPS_REGISTER_CONTROLLED_FRAME_PARTITION_COMMAND_H_

#include <string>

#include "base/functional/callback_forward.h"
#include "base/values.h"
#include "chrome/browser/web_applications/web_app_id.h"

namespace web_app {

class AppLock;

// Registers a <controlledframe>'s persisted StoragePartition with the web_app
// system so that its usage can be attributed to its owning IWA and cleaned up
// when the app is uninstalled.
base::Value RegisterControlledFramePartitionWithLock(
    const AppId& app_id,
    const std::string& partition_name,
    base::OnceClosure callback,
    AppLock& lock);

}  // namespace web_app

#endif  // CHROME_BROWSER_WEB_APPLICATIONS_ISOLATED_WEB_APPS_REGISTER_CONTROLLED_FRAME_PARTITION_COMMAND_H_
