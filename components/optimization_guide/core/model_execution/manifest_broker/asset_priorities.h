// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_OPTIMIZATION_GUIDE_CORE_MODEL_EXECUTION_MANIFEST_BROKER_ASSET_PRIORITIES_H_
#define COMPONENTS_OPTIMIZATION_GUIDE_CORE_MODEL_EXECUTION_MANIFEST_BROKER_ASSET_PRIORITIES_H_

#include "components/optimization_guide/core/model_execution/manifest_broker/manifest.h"
#include "components/optimization_guide/core/model_execution/usage_tracker.h"
#include "third_party/abseil-cpp/absl/container/flat_hash_map.h"
#include "third_party/abseil-cpp/absl/container/flat_hash_set.h"

namespace optimization_guide {

// Priorities for assets in the manifest.
enum class AssetPriority {
  // Unconditionally uninstalled when auto removal is disabled, because the user
  // explicitly requested uninstall.
  kUninstall = 0,
  // Evicted only when auto removal is enabled, otherwise retained on disk.
  kEvictable = 1,
  kRetain = 2,
  kSpeculative = 3,
  kBestEffort = 4,
  kUserBlocking = 5,
};

// Tracks the priority of assets in the manifest.
class AssetPriorities {
 public:
  AssetPriorities();
  // Computes asset priorities for all use cases in `manifest` based on
  // `usage_tracker`.
  AssetPriorities(const Manifest& manifest, const UsageTracker& usage_tracker);
  ~AssetPriorities();
  AssetPriorities(const AssetPriorities&);
  AssetPriorities& operator=(const AssetPriorities&);
  AssetPriorities(AssetPriorities&&);
  AssetPriorities& operator=(AssetPriorities&&);

  void Raise(AssetPriority priority,
             const absl::flat_hash_set<Manifest::AssetId>& assets);
  void Clear();

  bool IsAtLeast(AssetPriority priority,
                 const Manifest::AssetId& asset_id) const;

 private:
  absl::flat_hash_map<Manifest::AssetId, AssetPriority> priorities_;
};

}  // namespace optimization_guide

#endif  // COMPONENTS_OPTIMIZATION_GUIDE_CORE_MODEL_EXECUTION_MANIFEST_BROKER_ASSET_PRIORITIES_H_
