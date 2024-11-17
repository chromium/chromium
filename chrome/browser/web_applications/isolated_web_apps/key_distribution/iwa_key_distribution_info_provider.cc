// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/web_applications/isolated_web_apps/key_distribution/iwa_key_distribution_info_provider.h"

#include <memory>

#include "base/base64.h"
#include "base/containers/map_util.h"
#include "base/files/file_util.h"
#include "base/no_destructor.h"
#include "base/task/thread_pool.h"
#include "base/types/expected.h"
#include "base/types/expected_macros.h"
#include "chrome/browser/web_applications/isolated_web_apps/key_distribution/proto/key_distribution.pb.h"

namespace web_app {

namespace {

IwaKeyDistributionInfoProvider::KeyRotations& GetDevModeKeyRotationData() {
  static base::NoDestructor<IwaKeyDistributionInfoProvider::KeyRotations>
      dev_mode_kr_data;
  return *dev_mode_kr_data;
}

base::expected<IwaKeyDistributionInfoProvider::KeyRotations,
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

  IwaKeyDistributionInfoProvider::KeyRotations key_rotations;
  if (key_distribution.has_key_rotation_data()) {
    for (const auto& [web_bundle_id, kr_info] :
         key_distribution.key_rotation_data().key_rotations()) {
      if (!kr_info.has_expected_key()) {
        key_rotations.emplace(web_bundle_id,
                              IwaKeyDistributionInfoProvider::KeyRotationInfo(
                                  /*public_key=*/std::nullopt));
        continue;
      }
      std::optional<std::vector<uint8_t>> decoded_public_key =
          base::Base64Decode(kr_info.expected_key());
      if (!decoded_public_key) {
        return base::unexpected(IwaKeyDistributionInfoProvider::
                                    ComponentUpdateError::kMalformedBase64Key);
      }
      key_rotations.emplace(web_bundle_id,
                            IwaKeyDistributionInfoProvider::KeyRotationInfo(
                                std::move(decoded_public_key)));
    }
  }

  return key_rotations;
}

std::unique_ptr<IwaKeyDistributionInfoProvider>&
GetGlobalIwaKeyDistributionInfoProviderInstance() {
  static base::NoDestructor<std::unique_ptr<IwaKeyDistributionInfoProvider>>
      instance;
  return *instance;
}

}  // namespace

BASE_FEATURE(kIwaKeyDistributionDevMode,
             "IwaKeyDistributionDevMode",
             base::FEATURE_DISABLED_BY_DEFAULT);

IwaKeyDistributionInfoProvider::KeyRotationInfo::KeyRotationInfo(
    std::optional<PublicKeyData> public_key)
    : public_key(std::move(public_key)) {}

IwaKeyDistributionInfoProvider::KeyRotationInfo::~KeyRotationInfo() = default;

IwaKeyDistributionInfoProvider::KeyRotationInfo::KeyRotationInfo(
    const KeyRotationInfo&) = default;

base::Value IwaKeyDistributionInfoProvider::KeyRotationInfo::AsDebugValue()
    const {
  return base::Value(base::Value::Dict().Set(
      "public_key", public_key ? base::Base64Encode(*public_key) : "null"));
}

IwaKeyDistributionInfoProvider* IwaKeyDistributionInfoProvider::GetInstance() {
  auto& instance = GetGlobalIwaKeyDistributionInfoProviderInstance();
  if (!instance) {
    instance.reset(new IwaKeyDistributionInfoProvider());
  }
  return instance.get();
}

void IwaKeyDistributionInfoProvider::DestroyInstanceForTesting() {
  GetGlobalIwaKeyDistributionInfoProviderInstance().reset();
}

const IwaKeyDistributionInfoProvider::KeyRotationInfo*
IwaKeyDistributionInfoProvider::GetKeyRotationInfo(
    const std::string& web_bundle_id) const {
  if (const auto* kr_info =
          base::FindOrNull(GetDevModeKeyRotationData(), web_bundle_id)) {
    return kr_info;
  }
  return data_ ? base::FindOrNull(data_->key_rotations, web_bundle_id)
               : nullptr;
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
    base::expected<KeyRotations, ComponentUpdateError> result) {
  if (data_ && data_->version > component_version) {
    // This might happen if two tasks with different versions have been posted
    // to the task runner in `LoadKeyDistributionData()`.
    DispatchComponentUpdateError(component_version,
                                 ComponentUpdateError::kStaleVersion);
    return;
  }

  ASSIGN_OR_RETURN(auto key_rotations, std::move(result),
                   [&](ComponentUpdateError error) {
                     DispatchComponentUpdateError(component_version, error);
                   });

  data_ = ComponentData(component_version, std::move(key_rotations));
  DispatchComponentUpdateSuccess(component_version);
}

void IwaKeyDistributionInfoProvider::AddObserver(Observer* observer) {
  observers_.AddObserver(observer);
}

void IwaKeyDistributionInfoProvider::RemoveObserver(Observer* observer) {
  observers_.RemoveObserver(observer);
}

void IwaKeyDistributionInfoProvider::RotateKeyForDevMode(
    base::PassKey<IwaInternalsHandler>,
    const std::string& web_bundle_id,
    const std::optional<std::vector<uint8_t>>& rotated_key) {
  GetDevModeKeyRotationData().insert_or_assign(web_bundle_id,
                                               KeyRotationInfo(rotated_key));
  DispatchComponentUpdateSuccess(base::Version());
}

base::Value IwaKeyDistributionInfoProvider::AsDebugValue() const {
  base::Value::Dict debug_data;

  if (!GetDevModeKeyRotationData().empty()) {
    auto* dev_mode_key_rotations =
        debug_data.EnsureDict("dev_mode_key_rotations");
    for (const auto& [web_bundle_id, kr_info] : GetDevModeKeyRotationData()) {
      dev_mode_key_rotations->Set(web_bundle_id, kr_info.AsDebugValue());
    }
  }
  if (data_) {
    debug_data.Set("component_version", data_->version.GetString());
    auto* key_rotations = debug_data.EnsureDict("key_rotations");
    for (const auto& [web_bundle_id, kr_info] : data_->key_rotations) {
      key_rotations->Set(web_bundle_id, kr_info.AsDebugValue());
    }
  } else {
    debug_data.Set("component_version", "null");
  }

  return base::Value(std::move(debug_data));
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

IwaKeyDistributionInfoProvider::ComponentData::ComponentData(
    base::Version version,
    KeyRotations key_rotations)
    : version(std::move(version)), key_rotations(std::move(key_rotations)) {}
IwaKeyDistributionInfoProvider::ComponentData::~ComponentData() = default;
IwaKeyDistributionInfoProvider::ComponentData::ComponentData(
    const ComponentData&) = default;

}  // namespace web_app
