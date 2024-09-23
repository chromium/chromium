// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/web_applications/isolated_web_apps/get_controlled_frame_partition_command.h"

#include <optional>
#include <string>

#include "base/functional/callback.h"
#include "base/task/sequenced_task_runner.h"
#include "base/values.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/web_applications/isolated_web_apps/isolated_web_app_url_info.h"
#include "chrome/browser/web_applications/isolated_web_apps/isolation_data.h"
#include "chrome/browser/web_applications/locks/app_lock.h"
#include "chrome/browser/web_applications/web_app.h"
#include "chrome/browser/web_applications/web_app_registry_update.h"
#include "chrome/browser/web_applications/web_app_sync_bridge.h"
#include "content/public/browser/storage_partition_config.h"

namespace web_app {

std::optional<content::StoragePartitionConfig>
GetControlledFramePartitionWithLock(Profile* profile,
                                    const IsolatedWebAppUrlInfo& url_info,
                                    const std::string& partition_name,
                                    bool in_memory,
                                    AppLock& lock,
                                    base::Value::Dict& debug_info) {
  debug_info.Set("app_id", url_info.app_id());
  debug_info.Set("partition_name", partition_name);
  debug_info.Set("in_memory", in_memory);

  if (in_memory) {
    std::optional<content::StoragePartitionConfig> config =
        lock.registrar().SaveAndGetInMemoryControlledFramePartitionConfig(
            url_info, partition_name);
    return config;
  }

  content::StoragePartitionConfig storage_partition_config =
      url_info.GetStoragePartitionConfigForControlledFrame(
          profile, partition_name, /*in_memory=*/false);

  // Register the StoragePartition with the web_app system.
  {
    ScopedRegistryUpdate update = lock.sync_bridge().BeginUpdate();
    WebApp* iwa = update->UpdateApp(url_info.app_id());
    CHECK(iwa && iwa->isolation_data().has_value());

    const IsolationData& isolation_data = *iwa->isolation_data();

    std::set<std::string> cf_partitions =
        isolation_data.controlled_frame_partitions();
    cf_partitions.insert(partition_name);

    iwa->SetIsolationData(
        IsolationData::Builder(isolation_data)
            .SetControlledFramePartitions(std::move(cf_partitions))
            .Build());
  }
  return storage_partition_config;
}

}  // namespace web_app
