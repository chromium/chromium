// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/component_updater/android/loader_policies/tpcd_metadata_component_loader_policy.h"

#include <cstdint>
#include <optional>
#include <string>
#include <utility>
#include <vector>

#include "base/containers/flat_map.h"
#include "base/files/file_util.h"
#include "base/files/scoped_file.h"
#include "base/logging.h"
#include "base/task/thread_pool.h"
#include "base/version.h"
#include "components/component_updater/android/component_loader_policy.h"
#include "components/component_updater/installer_policies/tpcd_metadata_component_installer_policy.h"

namespace {

// Persisted to logs, should never change.
constexpr char kTpcdMetadataComponentMetricsSuffix[] = "TpcdMetadata";

// Loads the raw metadata as a string from the component file in storage.
std::optional<std::string> LoadTpcdMetadataFromDisk(base::ScopedFD fd) {
  std::string raw_tpcd_metadata;
  if (base::ReadStreamToString(base::FileToFILE(base::File(std::move(fd)), "r"),
                               &raw_tpcd_metadata)) {
    return raw_tpcd_metadata;
  }
  return nullptr;
}

}  // namespace

namespace component_updater {

TpcdMetadataComponentLoaderPolicy::TpcdMetadataComponentLoaderPolicy(
    OnTpcdMetadataComponentReadyCallback on_component_ready_callback)
    : on_component_ready_callback_(std::move(on_component_ready_callback)) {}

TpcdMetadataComponentLoaderPolicy::~TpcdMetadataComponentLoaderPolicy() =
    default;

void TpcdMetadataComponentLoaderPolicy::ComponentLoaded(
    const base::Version& version,
    base::flat_map<std::string, base::ScopedFD>& fd_map,
    base::Value::Dict manifest) {
  auto keys_fd_iterator = fd_map.find(kTpcdMetadataComponentFileName);
  if (keys_fd_iterator == fd_map.end()) {
    VLOG(1) << "TpcdMetadataComponentLoaderPolicy#ComponentLoaded "
               "failed because "
            << kTpcdMetadataComponentFileName << " was not found in the fd map";
    return;
  }

  base::ThreadPool::PostTaskAndReplyWithResult(
      FROM_HERE, {base::MayBlock(), base::TaskPriority::BEST_EFFORT},
      base::BindOnce(&LoadTpcdMetadataFromDisk,
                     std::move(keys_fd_iterator->second)),
      base::BindOnce(
          [](OnTpcdMetadataComponentReadyCallback on_component_ready_callback,
             const std::optional<std::string>& maybe_contents) {
            if (maybe_contents.has_value()) {
              on_component_ready_callback.Run(maybe_contents.value());
            }
          },
          on_component_ready_callback_));
}

void TpcdMetadataComponentLoaderPolicy::ComponentLoadFailed(
    ComponentLoadResult /*error*/) {}

void TpcdMetadataComponentLoaderPolicy::GetHash(
    std::vector<uint8_t>* hash) const {
  component_updater::TpcdMetadataComponentInstallerPolicy::GetPublicKeyHash(
      hash);
}

std::string TpcdMetadataComponentLoaderPolicy::GetMetricsSuffix() const {
  return kTpcdMetadataComponentMetricsSuffix;
}

}  // namespace component_updater
