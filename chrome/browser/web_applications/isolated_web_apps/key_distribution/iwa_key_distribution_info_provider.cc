// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/web_applications/isolated_web_apps/key_distribution/iwa_key_distribution_info_provider.h"

#include "base/base64.h"
#include "base/containers/map_util.h"
#include "base/files/file_util.h"
#include "base/memory/singleton.h"
#include "base/task/thread_pool.h"
#include "base/types/expected.h"
#include "base/types/expected_macros.h"
#include "chrome/browser/web_applications/isolated_web_apps/key_distribution/proto/key_distribution.pb.h"

namespace web_app {

namespace {

base::expected<IwaKeyDistribution,
               IwaKeyDistributionInfoProvider::ComponentUpdateError>
LoadKeyDistributionDataImpl(const base::FilePath& file_path) {
  std::string key_distribution_data;
  if (!base::ReadFileToString(file_path, &key_distribution_data)) {
    return base::unexpected(
        IwaKeyDistributionInfoProvider::ComponentUpdateError::kFileNotFound);
  }

  IwaKeyDistribution key_distribution;
  if (!key_distribution.ParseFromString(key_distribution_data)) {
    return base::unexpected(IwaKeyDistributionInfoProvider::
                                ComponentUpdateError::kProtoParsingFailure);
  }

  if (key_distribution.has_key_rotation_data()) {
    for (const auto& [web_bundle_id, kr_info] :
         key_distribution.key_rotation_data().key_rotations()) {
      if (kr_info.has_expected_key() &&
          !base::Base64Decode(kr_info.expected_key())) {
        return base::unexpected(IwaKeyDistributionInfoProvider::
                                    ComponentUpdateError::kMalformedBase64Key);
      }
    }
  }

  return key_distribution;
}

}  // namespace

IwaKeyDistributionInfoProvider* IwaKeyDistributionInfoProvider::GetInstance() {
  return base::Singleton<IwaKeyDistributionInfoProvider>::get();
}

void IwaKeyDistributionInfoProvider::LoadKeyDistributionData(
    const base::Version& component_version,
    const base::FilePath& file_path) {
  if (data_ && data_->version > component_version) {
    DispatchComponentUpdateError(component_version,
                                 ComponentUpdateError::kStaleVersion);
    return;
  }
  // `base::Unretained(this)` is fine as this is a singleton that never goes
  // away.
  task_runner_->PostTaskAndReplyWithResult(
      FROM_HERE, base::BindOnce(&LoadKeyDistributionDataImpl, file_path),
      base::BindOnce(
          &IwaKeyDistributionInfoProvider::OnKeyDistributionDataLoaded,
          base::Unretained(this), component_version));
}

IwaKeyDistributionInfoProvider::IwaKeyDistributionInfoProvider()
    : task_runner_(base::ThreadPool::CreateSequencedTaskRunner(
          {base::MayBlock(), base::TaskPriority::USER_VISIBLE})) {}

IwaKeyDistributionInfoProvider::~IwaKeyDistributionInfoProvider() = default;

void IwaKeyDistributionInfoProvider::OnKeyDistributionDataLoaded(
    const base::Version& component_version,
    base::expected<IwaKeyDistribution, ComponentUpdateError> result) {
  if (data_ && data_->version > component_version) {
    // This might happen if two tasks with different versions have been posted
    // to the task runner in `LoadKeyDistributionData()`.
    DispatchComponentUpdateError(component_version,
                                 ComponentUpdateError::kStaleVersion);
    return;
  }

  ASSIGN_OR_RETURN(auto proto, std::move(result),
                   [&](ComponentUpdateError error) {
                     DispatchComponentUpdateError(component_version, error);
                   });

  data_ = {.version = component_version, .proto = std::move(proto)};
  DispatchComponentUpdateSuccess(component_version);
}

void IwaKeyDistributionInfoProvider::AddObserver(Observer* observer) {
  observers_.AddObserver(observer);
}

void IwaKeyDistributionInfoProvider::RemoveObserver(Observer* observer) {
  observers_.RemoveObserver(observer);
}

void IwaKeyDistributionInfoProvider::DispatchComponentUpdateSuccess(
    const base::Version& component_version) const {
  for (auto& observer : observers_) {
    observer.OnComponentUpdateSuccess(component_version);
  }
}

void IwaKeyDistributionInfoProvider::DispatchComponentUpdateError(
    const base::Version& component_version,
    ComponentUpdateError error) const {
  for (auto& observer : observers_) {
    observer.OnComponentUpdateError(component_version, error);
  }
}

}  // namespace web_app
