// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_OPTIMIZATION_GUIDE_CORE_MODEL_EXECUTION_MANIFEST_BROKER_OVERRIDE_MANIFEST_ASSET_MANAGER_DELEGATE_H_
#define COMPONENTS_OPTIMIZATION_GUIDE_CORE_MODEL_EXECUTION_MANIFEST_BROKER_OVERRIDE_MANIFEST_ASSET_MANAGER_DELEGATE_H_

#include "base/containers/flat_map.h"
#include "base/files/file_path.h"
#include "base/version.h"
#include "components/optimization_guide/core/model_execution/manifest_broker/manifest_asset_manager.h"

namespace optimization_guide {

// This class enables integration testing by allowing overrides for all of the
// assets that would be provided by component updater.
//
// It takes an input configuration with the following structure:
// {
//   "manifest_path": "/path/to/manifest/dir",
//   "components": {
//     "public_key_hex_1": {
//       "1.0.0.0": "/path/to/version/1",
//       "2.0.0.0": "/path/to/version/2"
//     }
//   }
// }
class OverrideManifestAssetManagerDelegate final
    : public ManifestAssetManager::Delegate {
 public:
  explicit OverrideManifestAssetManagerDelegate(
      const base::FilePath& override_path);
  ~OverrideManifestAssetManagerDelegate() override;

  base::CallbackListSubscription ListenForManifestReady(
      base::RepeatingCallback<void(base::FilePath)> on_ready) override;

  void GetFreeDiskSpace(base::OnceCallback<void(std::optional<base::ByteCount>)>
                            callback) const override;

  void RegisterOnDemandComponent(
      const std::string& public_key_hex,
      const std::string& target_version,
      const std::string& component_name,
      base::WeakPtr<ManifestAssetManager> manager) override;

  void Uninstall(const std::string& public_key_hex,
                 base::WeakPtr<ManifestAssetManager> manager) override;

  void RequestUpdate(const std::string& public_key_hex,
                     bool is_background) override;

 private:
  base::FilePath manifest_path_;
  base::flat_map<std::string, base::FilePath> component_overrides_;
};

}  // namespace optimization_guide

#endif  // COMPONENTS_OPTIMIZATION_GUIDE_CORE_MODEL_EXECUTION_MANIFEST_BROKER_OVERRIDE_MANIFEST_ASSET_MANAGER_DELEGATE_H_
