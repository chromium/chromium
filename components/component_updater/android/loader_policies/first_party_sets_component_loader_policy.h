// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_COMPONENT_UPDATER_ANDROID_LOADER_POLICIES_FIRST_PARTY_SETS_COMPONENT_LOADER_POLICY_H_
#define COMPONENTS_COMPONENT_UPDATER_ANDROID_LOADER_POLICIES_FIRST_PARTY_SETS_COMPONENT_LOADER_POLICY_H_

#include <stdint.h>

#include <string>
#include <vector>

#include "base/containers/flat_map.h"
#include "base/files/scoped_file.h"
#include "base/values.h"
#include "base/version.h"
#include "components/component_updater/android/component_loader_policy.h"
#include "components/component_updater/installer_policies/first_party_sets_component_installer_policy.h"

namespace component_updater {
class FirstPartySetComponentLoaderPolicy : public ComponentLoaderPolicy {
 public:
  using SetsReadyOnceCallback = component_updater::
      FirstPartySetsComponentInstallerPolicy::SetsReadyOnceCallback;

  // `on_sets_ready` will be called on the UI thread when we either have sets,
  // or know we won't get them.
  explicit FirstPartySetComponentLoaderPolicy(
      SetsReadyOnceCallback on_sets_ready);
  ~FirstPartySetComponentLoaderPolicy() override;

  FirstPartySetComponentLoaderPolicy(
      const FirstPartySetComponentLoaderPolicy&) = delete;
  FirstPartySetComponentLoaderPolicy& operator=(
      const FirstPartySetComponentLoaderPolicy&) = delete;

 private:
  // The following methods override ComponentLoaderPolicy.
  void ComponentLoaded(const base::Version& version,
                       base::flat_map<std::string, base::ScopedFD>& fd_map,
                       base::Value::Dict manifest) override;
  void ComponentLoadFailed(ComponentLoadResult error) override;
  void GetHash(std::vector<uint8_t>* hash) const override;
  std::string GetMetricsSuffix() const override;

  SetsReadyOnceCallback on_sets_ready_;
};

}  // namespace component_updater

#endif  // COMPONENTS_COMPONENT_UPDATER_ANDROID_LOADER_POLICIES_FIRST_PARTY_SETS_COMPONENT_LOADER_POLICY_H_
