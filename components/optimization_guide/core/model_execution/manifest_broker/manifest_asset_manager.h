// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_OPTIMIZATION_GUIDE_CORE_MODEL_EXECUTION_MANIFEST_BROKER_MANIFEST_ASSET_MANAGER_H_
#define COMPONENTS_OPTIMIZATION_GUIDE_CORE_MODEL_EXECUTION_MANIFEST_BROKER_MANIFEST_ASSET_MANAGER_H_

#include <memory>
#include <optional>
#include <string>

#include "base/containers/flat_map.h"
#include "base/files/file_path.h"
#include "base/functional/callback.h"
#include "base/memory/weak_ptr.h"
#include "base/values.h"
#include "base/version.h"
#include "components/optimization_guide/core/model_execution/manifest_broker/manifest.h"

namespace optimization_guide {

// Manages the state of assets defined in the on-device model manifest.
class ManifestAssetManager {
 public:
  // Delegate to bridge the gap to the platform-specific download mechanism
  // (e.g., Chrome Component Updater on Desktop, AICore on Android).
  class Delegate {
   public:
    virtual ~Delegate() = default;

    // Registers the component installer for asset `asset_id`. The policy should
    // hold a weak pointer to the manager and call its `OnAssetReady` and
    // `OnAssetUninstalled` methods when appropriate.
    virtual void RegisterOnDemandComponent(
        const std::string& asset_id,
        const std::string& public_key_hex,
        const std::string& target_version,
        base::WeakPtr<ManifestAssetManager> manager) = 0;

    // Uninstalls the component and frees disk space.
    virtual void Uninstall(const std::string& asset_id,
                           const std::string& public_key_hex,
                           base::WeakPtr<ManifestAssetManager>) = 0;

    // Triggers an immediate update check for the component.
    virtual void RequestUpdate(const std::string& asset_id,
                               const std::string& public_key_hex,
                               bool is_background) = 0;
  };

  explicit ManifestAssetManager(std::unique_ptr<Delegate> delegate);
  ~ManifestAssetManager();

  ManifestAssetManager(const ManifestAssetManager&) = delete;
  ManifestAssetManager& operator=(const ManifestAssetManager&) = delete;

  // Updates a new manifest and update registration for all eligible assets.
  void UpdateManifest(Manifest manifest);

  // Returns the installation directory for `asset_id` if it is currently ready.
  std::optional<base::FilePath> GetInstallDirectory(
      const std::string& asset_id) const;

  // Returns whether the component installation is valid.
  static bool VerifyInstallation(const base::FilePath& install_dir,
                                 const base::DictValue& manifest);

  // Called when a component has been successfully installed or updated.
  void OnAssetReady(const std::string& asset_id,
                    const base::Version& version,
                    const base::FilePath& install_dir);

  // Called when a component has been completely uninstalled.
  void OnAssetUninstalled(const std::string& asset_id);

  // Called when the component installer has finished registering the asset.
  void InstallerRegistered(const std::string& asset_id,
                           bool is_already_installed);

 private:
  enum class AssetState {
    // Delegate->RegisterOnDemandComponent called, waiting for callback.
    kRegistering,
    // Component registered, sitting idle.
    kRegistered,
    // Delegate->RequestUpdate called, actively downloading.
    kOnDemandDownloading,
    // Component is fully downloaded and verified.
    kReady,
    // Delegate->Uninstall called, waiting for completion.
    kUninstalling,
  };

  struct AssetContext {
    AssetContext();
    explicit AssetContext(AssetState state);
    ~AssetContext();
    AssetContext(const AssetContext&);
    AssetContext& operator=(const AssetContext&);

    AssetState state;
    std::optional<base::FilePath> install_dir;
    std::optional<base::Version> version;
  };

  void UpdateRegistration();

  std::unique_ptr<Delegate> delegate_;
  std::optional<Manifest> manifest_;

  // Tracks the state of all components known to the manager.
  base::flat_map<std::string, AssetContext> asset_contexts_;

  base::WeakPtrFactory<ManifestAssetManager> weak_ptr_factory_{this};
};

}  // namespace optimization_guide

#endif  // COMPONENTS_OPTIMIZATION_GUIDE_CORE_MODEL_EXECUTION_MANIFEST_BROKER_MANIFEST_ASSET_MANAGER_H_
