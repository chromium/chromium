// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/component_updater/android/loader_policies/first_party_sets_component_loader_policy.h"

#include <cstdint>
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
#include "components/component_updater/installer_policies/first_party_sets_component_installer_policy.h"

namespace {

// Persisted to logs, should never change.
constexpr char kFirstPartySetsComponentMetricsSuffix[] = "RelatedWebsiteSets";

}  // namespace

namespace component_updater {

FirstPartySetComponentLoaderPolicy::FirstPartySetComponentLoaderPolicy(
    SetsReadyOnceCallback on_sets_ready)
    : on_sets_ready_(std::move(on_sets_ready)) {}

FirstPartySetComponentLoaderPolicy::~FirstPartySetComponentLoaderPolicy() =
    default;

void FirstPartySetComponentLoaderPolicy::ComponentLoaded(
    const base::Version& version,
    base::flat_map<std::string, base::ScopedFD>& fd_map,
    base::Value::Dict manifest) {
  auto keys_fd_iterator = fd_map.find(kFpsMetadataComponentFileName);
  if (keys_fd_iterator == fd_map.end()) {
    VLOG(1) << "FirstPartySetComponentLoaderPolicy#ComponentLoaded "
               "failed because sets.json is not found in the fd map";
    std::move(on_sets_ready_).Run(base::Version(), base::File());
    return;
  }
  std::move(on_sets_ready_)
      .Run(version, base::File(std::move(keys_fd_iterator->second)));
}

void FirstPartySetComponentLoaderPolicy::ComponentLoadFailed(
    ComponentLoadResult /*error*/) {
  // If the component did not load, we must still inform the rest of Chromium
  // that we have finished attempting to load the component so that it can stop
  // blocking on this.
  std::move(on_sets_ready_).Run(base::Version(), base::File());
}

void FirstPartySetComponentLoaderPolicy::GetHash(
    std::vector<uint8_t>* hash) const {
  FirstPartySetsComponentInstallerPolicy::GetPublicKeyHash(hash);
}

std::string FirstPartySetComponentLoaderPolicy::GetMetricsSuffix() const {
  return kFirstPartySetsComponentMetricsSuffix;
}

}  // namespace component_updater
