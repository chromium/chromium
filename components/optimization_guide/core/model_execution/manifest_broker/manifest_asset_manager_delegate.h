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
#include "base/containers/enum_set.h"
#include "base/files/file_path.h"
#include "base/functional/callback.h"
#include "base/memory/weak_ptr.h"
#include "base/values.h"
#include "base/version.h"
#include "components/optimization_guide/core/model_execution/manifest_provider.h"
#include "components/update_client/update_client.h"
#include "components/update_client/update_client_errors.h"

namespace optimization_guide {

// Converts a 64-character hex-encoded SHA256 public key hash into a 32-char
// CRX component ID, or returns `std::nullopt` if `public_key_hex` is not a
// valid SHA256 hex string.
std::optional<std::string> GetCrxIdFromPublicKeyHex(
    std::string_view public_key_hex);

// Delegate for `ManifestMonitor` to avoid depending on the InstallPolicy
// directly.
class ManifestMonitorDelegate : public ManifestProvider {
 public:
  enum class DiskSpaceCheckReason {
    kInitialization,
    kAssetRegistration,
  };

  ~ManifestMonitorDelegate() override = default;

  // Gets the free disk space in the install directory, or `std::nullopt` if
  // unknown. The callback may run synchronously.
  virtual void GetFreeDiskSpace(
      base::OnceCallback<void(std::optional<base::ByteSize>)> callback)
      const = 0;

  // Promotes queued and subsequent disk checks while any reason needs them.
  // Each caller must clear its reason when its disk-dependent work completes.
  void SetDiskSpaceCheckPriority(DiskSpaceCheckReason reason,
                                 bool user_visible);

 protected:
  // Applies the combined priority. Delegates without background disk work need
  // not override this.
  virtual void UpdateDiskSpaceCheckPriority(bool user_visible);

 private:
  base::EnumSet<DiskSpaceCheckReason,
                DiskSpaceCheckReason::kInitialization,
                DiskSpaceCheckReason::kAssetRegistration>
      disk_space_priority_reasons_;
};

// Delegate to bridge the gap to the platform-specific download mechanism
// (e.g., Chrome Component Updater on Desktop, AICore on Android).
class ManifestAssetManagerDelegate : public ManifestMonitorDelegate {
 public:
  // Interface for callbacks from `ManifestAssetManagerDelegate` to
  // `ManifestAssetManager`. All methods must be invoked on the `AssetManager`'s
  // sequence.
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

    // Called once per `RequestUpdate()` when the update finishes, after
    // `OnAssetReady()` if it installed the asset. Not called if a later
    // `RequestUpdate()`, `RegisterOnDemandComponent()`, or `Uninstall()` for
    // the same key supersedes it.
    virtual void OnUpdateRequestComplete(const std::string& public_key,
                                         update_client::Error error) = 0;

    // Called when the network connection transitions between metered and
    // unmetered, or reconnects to an unmetered network after being offline.
    virtual void OnMeteredConnectionChanged() = 0;

    // Called when a registered component reaches a terminal update state.
    virtual void OnAssetUpdateEvent(const std::string& public_key,
                                    update_client::ComponentState state,
                                    update_client::ErrorCategory error_category,
                                    int error_code) = 0;
  };

  ~ManifestAssetManagerDelegate() override = default;

  // Sets the manager that receives callbacks from this delegate. Must be
  // called before any other method.
  virtual void SetAssetManager(base::WeakPtr<AssetManager> manager) = 0;

  // Returns whether the current network connection is metered.
  virtual bool IsConnectionMetered() const;

  // Registers the component installer for `public_key_hex`.
  virtual void RegisterOnDemandComponent(const std::string& public_key_hex,
                                         const std::string& target_version,
                                         const std::string& component_name) = 0;

  // Uninstalls a component and frees disk space.
  virtual void Uninstall(const std::string& public_key_hex) = 0;

  // Triggers an immediate update check for a component. Completion is always
  // reported asynchronously via `AssetManager::OnUpdateRequestComplete()`.
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
