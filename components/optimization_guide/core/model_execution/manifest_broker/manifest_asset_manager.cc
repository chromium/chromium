// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/optimization_guide/core/model_execution/manifest_broker/manifest_asset_manager.h"

#include <memory>
#include <optional>
#include <string>
#include <utility>

#include "base/containers/flat_map.h"
#include "base/files/file_path.h"
#include "base/files/file_util.h"
#include "base/functional/callback.h"
#include "base/logging.h"
#include "base/memory/weak_ptr.h"
#include "base/notimplemented.h"
#include "base/values.h"
#include "base/version.h"
#include "components/optimization_guide/core/model_execution/manifest_broker/manifest.h"

namespace optimization_guide {

ManifestAssetManager::AssetContext::AssetContext()
    : state(AssetState::kRegistering) {}
ManifestAssetManager::AssetContext::AssetContext(AssetState state)
    : state(state) {}
ManifestAssetManager::AssetContext::~AssetContext() = default;
ManifestAssetManager::AssetContext::AssetContext(const AssetContext&) = default;
ManifestAssetManager::AssetContext&
ManifestAssetManager::AssetContext::operator=(const AssetContext&) = default;

ManifestAssetManager::ManifestAssetManager(std::unique_ptr<Delegate> delegate)
    : delegate_(std::move(delegate)) {}

ManifestAssetManager::~ManifestAssetManager() = default;

void ManifestAssetManager::UpdateManifest(Manifest manifest) {
  manifest_ = std::move(manifest);
  UpdateRegistration();
}

std::optional<base::FilePath> ManifestAssetManager::GetInstallDirectory(
    const std::string& asset_id) const {
  auto it = asset_contexts_.find(asset_id);
  if (it != asset_contexts_.end() && it->second.install_dir) {
    return it->second.install_dir;
  }
  return std::nullopt;
}

// static
bool ManifestAssetManager::VerifyInstallation(const base::FilePath& install_dir,
                                              const base::DictValue& manifest) {
  // TODO(crbug.com/489511499): implement proper verification logic.
  return base::PathExists(install_dir);
}

void ManifestAssetManager::UpdateRegistration() {
  if (!manifest_) {
    return;
  }

  // TODO(crbug.com/489511499): use UsageTracker to narrow down which components
  // need registration.
  for (const auto& [asset_id, component] :
       manifest_->GetAssets().on_demand_components()) {
    auto it = asset_contexts_.find(asset_id);
    if (it == asset_contexts_.end()) {
      asset_contexts_.emplace(asset_id, AssetContext(AssetState::kRegistering));
      delegate_->RegisterOnDemandComponent(asset_id, component.public_key(),
                                           component.target_version(),
                                           weak_ptr_factory_.GetWeakPtr());
      continue;
    }

    AssetContext& context = it->second;

    if (context.state == AssetState::kRegistering ||
        context.state == AssetState::kUninstalling ||
        context.state == AssetState::kOnDemandDownloading) {
      // Can't do anything right now during
      // registering/downloading/uninstalling, wait for callbacks.
      continue;
    }

    if (context.state == AssetState::kRegistered) {
      // TODO(crbug.com/489511499): Requests foreground/background update
      // based on actual UsageTracker logic.
      context.state = AssetState::kOnDemandDownloading;
      delegate_->RequestUpdate(asset_id, component.public_key(),
                               /*is_background=*/false);
    }
  }
}

void ManifestAssetManager::InstallerRegistered(const std::string& asset_id,
                                               bool is_already_installed) {
  if (!asset_contexts_.contains(asset_id)) {
    // This should not happen since UpdateRegistration creates the context
    // before calling RegisterOnDemandComponent.
    LOG(ERROR) << "Installer registered for unknown asset: " << asset_id;
  }
  AssetContext& context = asset_contexts_[asset_id];
  context.state =
      is_already_installed ? AssetState::kReady : AssetState::kRegistered;
  UpdateRegistration();
}

void ManifestAssetManager::OnAssetReady(const std::string& asset_id,
                                        const base::Version& version,
                                        const base::FilePath& install_dir) {
  if (!asset_contexts_.contains(asset_id)) {
    LOG(ERROR) << "Asset ready for unknown asset: " << asset_id;
  }
  AssetContext& context = asset_contexts_[asset_id];
  context.state = AssetState::kReady;
  context.install_dir = install_dir;
  context.version = version;
  // TODO(crbug.com/489511499): notify `SolutionFactory`.
}

void ManifestAssetManager::OnAssetUninstalled(const std::string& asset_id) {
  asset_contexts_.erase(asset_id);
  // TODO(crbug.com/489511499): Update asset ledger and notify consumers.
  NOTIMPLEMENTED();
}

}  // namespace optimization_guide
