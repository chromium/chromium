// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/component_updater/android/loader_policies/trust_token_key_commitments_component_loader_policy.h"

#include <stdint.h>

#include <optional>
#include <string>
#include <utility>
#include <vector>

#include "base/containers/flat_map.h"
#include "base/files/file_util.h"
#include "base/files/scoped_file.h"
#include "base/functional/bind.h"
#include "base/functional/callback.h"
#include "base/logging.h"
#include "base/values.h"
#include "base/version.h"
#include "components/component_updater/android/component_loader_policy.h"
#include "components/component_updater/installer_policies/trust_token_key_commitments_component_installer_policy.h"

namespace {

// Persisted to logs, should never change.
constexpr char kTrustTokenKeyCommitmentsComponentMetricsSuffix[] =
    "TrustTokenKeyCommitments";

// Attempts to load key commitments as raw JSON from their storage file,
// returning the loaded commitments on success and nullopt on failure.
std::optional<std::string> LoadKeyCommitmentsFromDisk(base::ScopedFD fd) {
  // Transfer the ownership of the file from `fd` to `file_stream`.
  base::ScopedFILE file_stream(
      base::FileToFILE(base::File(std::move(fd)), "r"));
  std::string commitments;
  if (!base::ReadStreamToString(file_stream.get(), &commitments)) {
    return std::nullopt;
  }

  return commitments;
}

}  // namespace

namespace component_updater {

TrustTokenKeyCommitmentsComponentLoaderPolicy::
    TrustTokenKeyCommitmentsComponentLoaderPolicy(
        base::RepeatingCallback<void(const std::string&)> on_commitments_ready)
    : on_commitments_ready_(std::move(on_commitments_ready)) {}

TrustTokenKeyCommitmentsComponentLoaderPolicy::
    ~TrustTokenKeyCommitmentsComponentLoaderPolicy() = default;

void TrustTokenKeyCommitmentsComponentLoaderPolicy::ComponentLoaded(
    const base::Version& version,
    base::flat_map<std::string, base::ScopedFD>& fd_map,
    base::Value::Dict manifest) {
  auto keys_fd_iterator = fd_map.find(kTrustTokenKeyCommitmentsFileName);
  if (keys_fd_iterator == fd_map.end()) {
    VLOG(1) << "TrustTokenKeyCommitmentsComponentLoaderPolicy#ComponentLoaded "
               "failed because keys.json is not found in the fd map";
    return;
  }
  component_updater::TrustTokenKeyCommitmentsComponentInstallerPolicy::
      LoadTrustTokensFromString(
          base::BindOnce(&LoadKeyCommitmentsFromDisk,
                         std::move(keys_fd_iterator->second)),
          on_commitments_ready_);
}

void TrustTokenKeyCommitmentsComponentLoaderPolicy::ComponentLoadFailed(
    ComponentLoadResult /*error*/) {}

void TrustTokenKeyCommitmentsComponentLoaderPolicy::GetHash(
    std::vector<uint8_t>* hash) const {
  component_updater::TrustTokenKeyCommitmentsComponentInstallerPolicy::
      GetPublicKeyHash(hash);
}

std::string TrustTokenKeyCommitmentsComponentLoaderPolicy::GetMetricsSuffix()
    const {
  return kTrustTokenKeyCommitmentsComponentMetricsSuffix;
}

}  // namespace component_updater
