// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_COMPONENT_UPDATER_ANDROID_LOADER_POLICIES_MASKED_DOMAIN_LIST_COMPONENT_LOADER_POLICY_H_
#define COMPONENTS_COMPONENT_UPDATER_ANDROID_LOADER_POLICIES_MASKED_DOMAIN_LIST_COMPONENT_LOADER_POLICY_H_

#include <stdint.h>

#include <string>
#include <vector>

#include "base/containers/flat_map.h"
#include "base/files/scoped_file.h"
#include "base/values.h"
#include "base/version.h"
#include "components/component_updater/android/component_loader_policy.h"
#include "components/component_updater/installer_policies/masked_domain_list_component_installer_policy.h"

namespace component_updater {
class MaskedDomainListComponentLoaderPolicy : public ComponentLoaderPolicy {
 public:
  using ListReadyRepeatingCallback = component_updater::
      MaskedDomainListComponentInstallerPolicy::ListReadyRepeatingCallback;

  // |on_mdl_ready| will be called on the UI thread when the MDL is ready.
  explicit MaskedDomainListComponentLoaderPolicy(
      ListReadyRepeatingCallback on_list_ready);
  ~MaskedDomainListComponentLoaderPolicy() override;

  MaskedDomainListComponentLoaderPolicy(
      const MaskedDomainListComponentLoaderPolicy&) = delete;
  MaskedDomainListComponentLoaderPolicy& operator=(
      const MaskedDomainListComponentLoaderPolicy&) = delete;

 private:
  // The following methods override ComponentLoaderPolicy.
  void ComponentLoaded(const base::Version& version,
                       base::flat_map<std::string, base::ScopedFD>& fd_map,
                       base::Value::Dict manifest) override;
  void ComponentLoadFailed(ComponentLoadResult error) override;
  void GetHash(std::vector<uint8_t>* hash) const override;
  std::string GetMetricsSuffix() const override;

  ListReadyRepeatingCallback on_list_ready_;
};

}  // namespace component_updater

#endif  // COMPONENTS_COMPONENT_UPDATER_ANDROID_LOADER_POLICIES_MASKED_DOMAIN_LIST_COMPONENT_LOADER_POLICY_H_
