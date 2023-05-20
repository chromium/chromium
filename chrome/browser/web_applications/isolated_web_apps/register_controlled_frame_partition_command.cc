// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/web_applications/isolated_web_apps/register_controlled_frame_partition_command.h"

#include <string>

#include "base/functional/callback.h"
#include "base/task/sequenced_task_runner.h"
#include "base/values.h"
#include "chrome/browser/web_applications/locks/app_lock.h"
#include "chrome/browser/web_applications/web_app.h"
#include "chrome/browser/web_applications/web_app_id.h"
#include "chrome/browser/web_applications/web_app_registry_update.h"

namespace web_app {

base::Value RegisterControlledFramePartitionWithLock(
    const AppId& app_id,
    const std::string& partition_name,
    base::OnceClosure callback,
    AppLock& lock) {
  {
    ScopedRegistryUpdate update(&lock.sync_bridge());
    WebApp* iwa = update->UpdateApp(app_id);
    CHECK(iwa && iwa->isolation_data().has_value());

    // TODO(crbug.com/1445795): If the StoragePartition is flagged for deletion,
    // clear the flag.
    WebApp::IsolationData isolation_data = *iwa->isolation_data();
    isolation_data.controlled_frame_partitions.insert(partition_name);
    iwa->SetIsolationData(isolation_data);
  }

  base::Value::Dict debug_info;
  debug_info.Set("app_id", app_id);
  debug_info.Set("partition_name", partition_name);

  base::SequencedTaskRunner::GetCurrentDefault()->PostTask(FROM_HERE,
                                                           std::move(callback));
  return base::Value(debug_info.Clone());
}

}  // namespace web_app
