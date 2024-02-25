// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_COMPONENT_UPDATER_ANDROID_LOADER_POLICIES_TPCD_METADATA_COMPONENT_LOADER_POLICY_H_
#define COMPONENTS_COMPONENT_UPDATER_ANDROID_LOADER_POLICIES_TPCD_METADATA_COMPONENT_LOADER_POLICY_H_

#include <string>
#include <vector>

#include "base/containers/flat_map.h"
#include "base/version.h"
#include "components/component_updater/android/component_loader_policy.h"
#include "components/component_updater/installer_policies/tpcd_metadata_component_installer_policy.h"

namespace component_updater {

class TpcdMetadataComponentLoaderPolicy : public ComponentLoaderPolicy {
 public:
  using OnTpcdMetadataComponentReadyCallback =
      component_updater::TpcdMetadataComponentInstallerPolicy::
          OnTpcdMetadataComponentReadyCallback;

  // `on_component_ready_callback` will be called once the tpcd metadata has
  // been downloaded.
  explicit TpcdMetadataComponentLoaderPolicy(
      OnTpcdMetadataComponentReadyCallback on_component_ready_callback);
  ~TpcdMetadataComponentLoaderPolicy() override;

  TpcdMetadataComponentLoaderPolicy(const TpcdMetadataComponentLoaderPolicy&) =
      delete;
  TpcdMetadataComponentLoaderPolicy& operator=(
      const TpcdMetadataComponentLoaderPolicy&) = delete;

 private:
  // The following methods override ComponentLoaderPolicy.
  void ComponentLoaded(const base::Version& version,
                       base::flat_map<std::string, base::ScopedFD>& fd_map,
                       base::Value::Dict manifest) override;
  void ComponentLoadFailed(ComponentLoadResult error) override;
  void GetHash(std::vector<uint8_t>* hash) const override;
  std::string GetMetricsSuffix() const override;

  OnTpcdMetadataComponentReadyCallback on_component_ready_callback_;
};

}  // namespace component_updater

#endif  // COMPONENTS_COMPONENT_UPDATER_ANDROID_LOADER_POLICIES_TPCD_METADATA_COMPONENT_LOADER_POLICY_H_
