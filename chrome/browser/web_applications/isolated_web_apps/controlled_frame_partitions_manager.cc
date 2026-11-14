// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/web_applications/isolated_web_apps/controlled_frame_partitions_manager.h"

#include <optional>
#include <string>
#include <utility>
#include <vector>

#include "base/check_deref.h"
#include "base/containers/extend.h"
#include "base/containers/map_util.h"
#include "base/containers/to_value_list.h"
#include "base/containers/to_vector.h"
#include "base/types/expected_macros.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/web_applications/isolated_web_apps/isolated_web_app_url_info.h"
#include "chrome/browser/web_applications/model/isolation_data.h"
#include "chrome/browser/web_applications/web_app.h"
#include "chrome/browser/web_applications/web_app_filter.h"
#include "chrome/browser/web_applications/web_app_provider.h"
#include "chrome/browser/web_applications/web_app_registrar.h"
#include "content/public/browser/isolated_web_apps_policy.h"
#include "content/public/browser/storage_partition_config.h"

namespace web_app {

DEFINE_USER_DATA(ControlledFramePartitionsManager);

// static
ControlledFramePartitionsManager& ControlledFramePartitionsManager::From(
    WebAppProvider& provider) {
  return CHECK_DEREF(Get(provider.unowned_user_data_host()));
}

// static
const ControlledFramePartitionsManager& ControlledFramePartitionsManager::From(
    const WebAppProvider& provider) {
  return CHECK_DEREF(Get(provider.unowned_user_data_host()));
}

ControlledFramePartitionsManager::ControlledFramePartitionsManager(
    Profile& profile,
    ui::UnownedUserDataHost& host)
    : profile_(profile), scoped_data_holder_(host, *this) {}

ControlledFramePartitionsManager::~ControlledFramePartitionsManager() = default;

std::optional<content::StoragePartitionConfig>
ControlledFramePartitionsManager::SaveAndGetInMemoryPartitionConfig(
    const IsolatedWebAppUrlInfo& url_info,
    const std::string& partition_name) {
  const auto& provider =
      CHECK_DEREF(WebAppProvider::GetForWebApps(&profile_.get()));
  // Safe on the UI thread: read-only membership check before recording the
  // ephemeral partition name.
  if (!provider.registrar_unsafe().AppMatches(url_info.app_id(),
                                              WebAppFilter::IsIsolatedApp())) {
    return std::nullopt;
  }

  in_memory_controlled_frame_partitions_[url_info.app_id()].insert(
      partition_name);

  return url_info.GetStoragePartitionConfigForControlledFrame(
      &profile_.get(), partition_name, /*in_memory=*/true);
}

std::vector<content::StoragePartitionConfig>
ControlledFramePartitionsManager::GetInMemoryPartitionConfigs(
    const IsolatedWebAppUrlInfo& url_info) const {
  const auto* stored_partitions = base::FindOrNull(
      in_memory_controlled_frame_partitions_, url_info.app_id());
  if (!stored_partitions) {
    return {};
  }
  return base::ToVector(*stored_partitions, [&](const std::string& partition) {
    return url_info.GetStoragePartitionConfigForControlledFrame(
        &profile_.get(), partition, /*in_memory=*/true);
  });
}

void ControlledFramePartitionsManager::ClearInMemoryPartitions(
    const webapps::AppId& app_id) {
  in_memory_controlled_frame_partitions_.erase(app_id);
}

base::Value ControlledFramePartitionsManager::AsDebugValue() const {
  base::DictValue in_memory_partitions;
  for (const auto& [app_id, partitions] :
       in_memory_controlled_frame_partitions_) {
    in_memory_partitions.Set(app_id, base::ToValueList(partitions));
  }
  return base::Value(
      base::DictValue().Set("in_memory_controlled_frame_partitions",
                            std::move(in_memory_partitions)));
}

std::vector<content::StoragePartitionConfig>
GetIsolatedWebAppStoragePartitionConfigs(const WebAppProvider& provider,
                                         const webapps::AppId& app_id) {
  Profile* profile = provider.profile();
  if (!content::AreIsolatedWebAppsEnabled(profile)) {
    return {};
  }
  // Safe on the UI thread: synchronous read-only lookup. We intentionally avoid
  // `IsIsolatedApp()` filters because this function is also called during
  // uninstallation after `is_uninstalling` is set to true.
  const WebApp* iwa = provider.registrar_unsafe().GetAppById(app_id);
  if (!iwa || !iwa->isolation_data()) {
    return {};
  }

  ASSIGN_OR_RETURN(
      auto url_info, IsolatedWebAppUrlInfo::Create(iwa->scope()),
      [](auto) { return std::vector<content::StoragePartitionConfig>{}; });

  // Start with IWA's base on-disk partition.
  std::vector<content::StoragePartitionConfig> partitions = {
      url_info.storage_partition_config(profile)};

  // Get all on-disk Controlled Frame partitions.
  base::Extend(
      partitions,
      base::ToVector(
          iwa->isolation_data()->controlled_frame_partitions(),
          [&](const std::string& partition) {
            return url_info.GetStoragePartitionConfigForControlledFrame(
                profile, partition, /*in_memory=*/false);
          }));

  // Get all in-memory Controlled Frame partitions.
  base::Extend(partitions, ControlledFramePartitionsManager::From(provider)
                               .GetInMemoryPartitionConfigs(url_info));

  return partitions;
}

}  // namespace web_app
