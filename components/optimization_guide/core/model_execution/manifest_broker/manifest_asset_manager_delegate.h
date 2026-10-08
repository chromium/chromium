// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_OPTIMIZATION_GUIDE_CORE_MODEL_EXECUTION_MANIFEST_BROKER_MANIFEST_ASSET_MANAGER_DELEGATE_H_
#define COMPONENTS_OPTIMIZATION_GUIDE_CORE_MODEL_EXECUTION_MANIFEST_BROKER_MANIFEST_ASSET_MANAGER_DELEGATE_H_

#include <optional>
#include <string>
#include <string_view>
#include <vector>

#include "base/byte_size.h"
#include "base/callback_list.h"
#include "base/files/file_path.h"
#include "base/functional/callback.h"
#include "base/memory/weak_ptr.h"
#include "base/values.h"
#include "base/version.h"

namespace optimization_guide {

// Converts a 64-character hex-encoded SHA256 public key hash into a 32-char
// CRX component ID, or returns `std::nullopt` if `public_key_hex` is not a
// valid SHA256 hex string.
std::optional<std::string> GetCrxIdFromPublicKeyHex(
    std::string_view public_key_hex);

// Delegate for `ManifestMonitor` to avoid depending on the InstallPolicy
// directly.
class ManifestMonitorDelegate {
 public:
  virtual ~ManifestMonitorDelegate() = default;

  // Returns the base install directory for on-demand models.
  virtual base::CallbackListSubscription ListenForManifestReady(
      base::RepeatingCallback<void(base::FilePath)> on_ready) = 0;

  // Gets the available free disk space in the install directory on a
  // background thread.
  virtual void GetFreeDiskSpace(
      base::OnceCallback<void(std::optional<base::ByteSize>)> callback)
      const = 0;
};

// Delegate to bridge the gap to the platform-specific download mechanism
// (e.g., Chrome Component Updater on Desktop, AICore on Android).
class ManifestAssetManagerDelegate : public ManifestMonitorDelegate {
 public:
  // Interface for callbacks from `ManifestAssetManagerDelegate` to
  // `ManifestAssetManager`.
  class AssetManager {
   public:
    virtual ~AssetManager() = default;

    // Called when a component has been successfully installed or updated.
    virtual void OnAssetReady(const std::string& public_key,
                              const base::Version& version,
                              const base::FilePath& install_dir) = 0;

    // Called when a component has been completely uninstalled.
    virtual void OnAssetUninstalled(const std::string& public_key) = 0;

    // Called when the component installer has finished registering the asset.
    virtual void InstallerRegistered(const std::string& public_key,
                                     const std::string& version,
                                     bool is_already_installed) = 0;
  };

  ~ManifestAssetManagerDelegate() override = default;

  // Registers the component installer for `public_key_hex`. The policy should
  // hold a weak pointer to `manager` and call its `InstallerRegistered`,
  // `OnAssetReady`, and `OnAssetUninstalled` methods when appropriate.
  virtual void RegisterOnDemandComponent(
      const std::string& public_key_hex,
      const std::string& target_version,
      const std::string& component_name,
      base::WeakPtr<AssetManager> manager) = 0;

  // Uninstalls a component and frees disk space.
  virtual void Uninstall(const std::string& public_key_hex,
                         base::WeakPtr<AssetManager> manager) = 0;

  // Triggers an immediate update check for a component.
  virtual void RequestUpdate(const std::string& public_key_hex,
                             bool is_background) = 0;

  struct InstalledAsset {
    std::string public_key_hex;
    // The inferred version of the asset, or `std::nullopt` if the asset
    // directory exists on disk but has no usable version.
    std::optional<base::Version> version;

    bool operator==(const InstalledAsset&) const = default;
  };

  // Lists all manifest-controlled assets currently installed on disk.
  virtual void GetInstalledAssets(
      base::OnceCallback<void(std::vector<InstalledAsset>)> callback) const = 0;

  // Returns whether the component installation is valid.
  static bool VerifyInstallation(const base::FilePath& install_dir,
                                 const base::DictValue& manifest);
};

}  // namespace optimization_guide

#endif  // COMPONENTS_OPTIMIZATION_GUIDE_CORE_MODEL_EXECUTION_MANIFEST_BROKER_MANIFEST_ASSET_MANAGER_DELEGATE_H_
