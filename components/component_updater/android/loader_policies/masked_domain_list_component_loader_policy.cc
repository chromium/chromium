// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/component_updater/android/loader_policies/masked_domain_list_component_loader_policy.h"

#include <stdint.h>

#include <optional>
#include <string>
#include <utility>
#include <vector>

#include "base/containers/flat_map.h"
#include "base/containers/span.h"
#include "base/files/file_util.h"
#include "base/files/scoped_file.h"
#include "base/logging.h"
#include "base/task/thread_pool.h"
#include "base/values.h"
#include "base/version.h"
#include "components/component_updater/android/component_loader_policy.h"
#include "mojo/public/cpp/base/proto_wrapper.h"
#include "mojo/public/cpp/base/proto_wrapper_passkeys.h"

namespace {

// Persisted to logs, should never change.
constexpr char kMaskedDomainListComponentMetricsSuffix[] = "MaskedDomainList";

constexpr char kMaskedDomainListProto[] = "masked_domain_list.MaskedDomainList";

}  // namespace

namespace component_updater {

// Helper class to read file to named proto class for mojo wrapper.
class ReadMaskedDomainListProto {
 public:
  static std::optional<mojo_base::ProtoWrapper> ReadFile(base::ScopedFD fd) {
    std::string raw_mdl;
    if (base::ReadStreamToString(
            base::FileToFILE(base::File(std::move(fd)), "r"), &raw_mdl)) {
      return mojo_base::ProtoWrapper(
          base::as_bytes(base::make_span(raw_mdl)), kMaskedDomainListProto,
          mojo_base::ProtoWrapperBytes::GetPassKey());
    }
    return std::nullopt;
  }
};

MaskedDomainListComponentLoaderPolicy::MaskedDomainListComponentLoaderPolicy(
    ListReadyRepeatingCallback on_list_ready)
    : on_list_ready_(std::move(on_list_ready)) {}

MaskedDomainListComponentLoaderPolicy::
    ~MaskedDomainListComponentLoaderPolicy() = default;

void MaskedDomainListComponentLoaderPolicy::ComponentLoaded(
    const base::Version& version,
    base::flat_map<std::string, base::ScopedFD>& fd_map,
    base::Value::Dict manifest) {
  auto keys_fd_iterator = fd_map.find(kMaskedDomainListFileName);
  if (keys_fd_iterator == fd_map.end()) {
    VLOG(1) << "MaskedDomainListComponentLoaderPolicy#ComponentLoaded "
               "failed because "
            << kMaskedDomainListFileName << " was not found in the fd map";
    return;
  }

  base::ThreadPool::PostTaskAndReplyWithResult(
      FROM_HERE, {base::MayBlock(), base::TaskPriority::USER_BLOCKING},
      base::BindOnce(&ReadMaskedDomainListProto::ReadFile,
                     std::move(keys_fd_iterator->second)),
      base::BindOnce(on_list_ready_, version));
}

void MaskedDomainListComponentLoaderPolicy::ComponentLoadFailed(
    ComponentLoadResult /*error*/) {}

void MaskedDomainListComponentLoaderPolicy::GetHash(
    std::vector<uint8_t>* hash) const {
  component_updater::MaskedDomainListComponentInstallerPolicy::GetPublicKeyHash(
      hash);
}

std::string MaskedDomainListComponentLoaderPolicy::GetMetricsSuffix() const {
  return kMaskedDomainListComponentMetricsSuffix;
}

}  // namespace component_updater
