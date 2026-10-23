// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/optimization_guide/core/model_execution/manifest_broker/asset_priorities.h"

#include <algorithm>

#include "components/optimization_guide/proto/manifest.pb.h"

namespace optimization_guide {

namespace {

AssetPriority ToAssetPriority(UsageTracker::Priority priority) {
  switch (priority) {
    case UsageTracker::Priority::kEvictable:
      return AssetPriority::kEvictable;
    case UsageTracker::Priority::kRetain:
      return AssetPriority::kRetain;
    case UsageTracker::Priority::kBestEffort:
      return AssetPriority::kBestEffort;
    case UsageTracker::Priority::kUserBlocking:
      return AssetPriority::kUserBlocking;
  }
}

}  // namespace

AssetPriorities::AssetPriorities() = default;

AssetPriorities::AssetPriorities(const Manifest& manifest,
                                 const UsageTracker& usage_tracker) {
  for (const auto& [use_case_name, _] :
       manifest.GetDeviceCategoryConfig().use_cases()) {
    if (usage_tracker.IsUninstallRequested(use_case_name)) {
      Raise(AssetPriority::kUninstall,
            *manifest.GetRequiredAssets(use_case_name));
    } else if (usage_tracker.HasUseCaseUsage(use_case_name)) {
      // Skip use cases that have never been used.
      // `GetPriority()` returns `kEvictable` for both expired (>30d) and
      // never-used use cases; raising `kEvictable` for a never-used use case
      // would overwrite `kUninstall` on shared assets (e.g. a shared safety
      // model) when another capability is uninstalled.
      Raise(ToAssetPriority(usage_tracker.GetPriority(use_case_name)),
            *manifest.GetRequiredAssets(use_case_name));
    }
  }
}

AssetPriorities::~AssetPriorities() = default;
AssetPriorities::AssetPriorities(const AssetPriorities&) = default;
AssetPriorities& AssetPriorities::operator=(const AssetPriorities&) = default;
AssetPriorities::AssetPriorities(AssetPriorities&&) = default;
AssetPriorities& AssetPriorities::operator=(AssetPriorities&&) = default;

void AssetPriorities::Raise(
    AssetPriority priority,
    const absl::flat_hash_set<Manifest::AssetId>& assets) {
  for (const auto& asset : assets) {
    auto [it, inserted] = priorities_.try_emplace(asset, priority);
    it->second = std::max(it->second, priority);
  }
}

void AssetPriorities::Clear() {
  priorities_.clear();
}

bool AssetPriorities::IsAtLeast(AssetPriority priority,
                                const Manifest::AssetId& asset_id) const {
  auto it = priorities_.find(asset_id);
  AssetPriority asset_priority =
      it != priorities_.end() ? it->second : AssetPriority::kEvictable;
  return asset_priority >= priority;
}

}  // namespace optimization_guide
