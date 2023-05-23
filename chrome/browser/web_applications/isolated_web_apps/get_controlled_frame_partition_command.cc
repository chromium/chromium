// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/web_applications/isolated_web_apps/get_controlled_frame_partition_command.h"

#include <string>

#include "base/functional/callback.h"
#include "base/task/sequenced_task_runner.h"
#include "base/values.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/web_applications/isolated_web_apps/isolated_web_app_url_info.h"
#include "chrome/browser/web_applications/locks/app_lock.h"
#include "chrome/browser/web_applications/web_app.h"
#include "chrome/browser/web_applications/web_app_registry_update.h"
#include "content/public/browser/storage_partition_config.h"
#include "third_party/abseil-cpp/absl/types/optional.h"

namespace web_app {

base::Value GetControlledFramePartitionWithLock(
    Profile* profile,
    const IsolatedWebAppUrlInfo& url_info,
    const std::string& partition_name,
    bool in_memory,
    base::OnceCallback<void(absl::optional<content::StoragePartitionConfig>)>
        callback,
    AppLock& lock) {
  content::StoragePartitionConfig storage_partition_config =
      url_info.GetStoragePartitionConfigForControlledFrame(
          profile, partition_name, in_memory);

  // If persisted, register the StoragePartition with the web_app system.
  if (!in_memory) {
    ScopedRegistryUpdate update(&lock.sync_bridge());
    WebApp* iwa = update->UpdateApp(url_info.app_id());
    CHECK(iwa && iwa->isolation_data().has_value());

    WebApp::IsolationData isolation_data = *iwa->isolation_data();
    isolation_data.controlled_frame_partitions.insert(partition_name);
    iwa->SetIsolationData(isolation_data);
  }

  base::Value::Dict debug_info;
  debug_info.Set("app_id", url_info.app_id());
  debug_info.Set("partition_name", partition_name);

  base::SequencedTaskRunner::GetCurrentDefault()->PostTask(
      FROM_HERE, base::BindOnce(std::move(callback), storage_partition_config));
  return base::Value(debug_info.Clone());
}

}  // namespace web_app
