// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/component_updater/android/loader_policies/trust_token_key_commitments_component_loader_policy.h"

#include <stdint.h>
#include <stdio.h>
#include <unistd.h>

#include <memory>
#include <string>
#include <utility>
#include <vector>

#include "base/bind.h"
#include "base/callback.h"
#include "base/containers/flat_map.h"
#include "base/files/file_util.h"
#include "base/logging.h"
#include "base/values.h"
#include "base/version.h"
#include "components/component_updater/installer_policies/trust_token_key_commitments_component_installer_policy.h"
#include "third_party/abseil-cpp/absl/types/optional.h"

namespace {

// Attempts to load key commitments as raw JSON from their storage file,
// returning the loaded commitments on success and nullopt on failure.
// TODO(crbug.com/1180964) move reading string from fd to base/file_util.h
absl::optional<std::string> LoadKeyCommitmentsFromDisk(int fd) {
  base::ScopedFILE file_stream(fdopen(fd, "r"));
  if (!file_stream.get()) {
    return absl::nullopt;
  }
  std::string commitments;
  if (!base::ReadStreamToString(file_stream.get(), &commitments))
    return absl::nullopt;

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
    const base::flat_map<std::string, int>& fd_map,
    std::unique_ptr<base::DictionaryValue> manifest) {
  int keys_fd = -1;
  for (auto& key_value : fd_map) {
    if (key_value.first == kTrustTokenKeyCommitmentsFileName) {
      keys_fd = key_value.second;
    } else {
      // Close unused fds.
      close(key_value.second);
    }
  }
  if (keys_fd == -1) {
    VLOG(1) << "TrustTokenKeyCommitmentsComponentLoaderPolicy#ComponentLoaded "
               "failed because keys.json is not found in the fd map";
    ComponentLoadFailed();
    return;
  }
  component_updater::TrustTokenKeyCommitmentsComponentInstallerPolicy::
      LoadTrustTokensFromString(
          base::BindOnce(&LoadKeyCommitmentsFromDisk, keys_fd),
          on_commitments_ready_);
}

void TrustTokenKeyCommitmentsComponentLoaderPolicy::ComponentLoadFailed() {}

void TrustTokenKeyCommitmentsComponentLoaderPolicy::GetHash(
    std::vector<uint8_t>* hash) const {
  component_updater::TrustTokenKeyCommitmentsComponentInstallerPolicy::
      GetPublicKeyHash(hash);
}

}  // namespace component_updater
