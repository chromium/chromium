// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_COMPONENT_UPDATER_ANDROID_LOADER_POLICIES_TRUST_TOKEN_KEY_COMMITMENTS_COMPONENT_LOADER_POLICY_H_
#define COMPONENTS_COMPONENT_UPDATER_ANDROID_LOADER_POLICIES_TRUST_TOKEN_KEY_COMMITMENTS_COMPONENT_LOADER_POLICY_H_

#include <stdint.h>

#include <string>
#include <vector>

#include "base/containers/flat_map.h"
#include "base/files/scoped_file.h"
#include "base/functional/callback.h"
#include "base/values.h"
#include "components/component_updater/android/component_loader_policy.h"

namespace base {
class Version;
}  // namespace base

namespace component_updater {

// TrustTokenKeyCommitmentsComponentLoaderPolicy defines a loader responsible
// for receiving updated Trust Tokens key commitments config and passing them to
// the network service via Mojo.
class TrustTokenKeyCommitmentsComponentLoaderPolicy
    : public ComponentLoaderPolicy {
 public:
  // |on_commitments_ready| will be called on the UI thread when
  // key commitments become ready.
  explicit TrustTokenKeyCommitmentsComponentLoaderPolicy(
      base::RepeatingCallback<void(const std::string&)> on_commitments_ready);
  ~TrustTokenKeyCommitmentsComponentLoaderPolicy() override;

  TrustTokenKeyCommitmentsComponentLoaderPolicy(
      const TrustTokenKeyCommitmentsComponentLoaderPolicy&) = delete;
  TrustTokenKeyCommitmentsComponentLoaderPolicy& operator=(
      const TrustTokenKeyCommitmentsComponentLoaderPolicy&) = delete;

 private:
  // The following methods override ComponentLoaderPolicy.
  void ComponentLoaded(const base::Version& version,
                       base::flat_map<std::string, base::ScopedFD>& fd_map,
                       base::Value::Dict manifest) override;
  void ComponentLoadFailed(ComponentLoadResult error) override;
  void GetHash(std::vector<uint8_t>* hash) const override;
  std::string GetMetricsSuffix() const override;

  base::RepeatingCallback<void(const std::string&)> on_commitments_ready_;
};

}  // namespace component_updater

#endif  // COMPONENTS_COMPONENT_UPDATER_ANDROID_LOADER_POLICIES_TRUST_TOKEN_KEY_COMMITMENTS_COMPONENT_LOADER_POLICY_H_
