// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/web_applications/isolated_web_apps/key_rotation/iwa_key_rotation_info_provider.h"

#include "base/base64.h"
#include "base/containers/map_util.h"
#include "base/files/file_util.h"
#include "base/memory/singleton.h"
#include "base/task/thread_pool.h"
#include "base/types/expected.h"
#include "base/types/expected_macros.h"
#include "chrome/browser/web_applications/isolated_web_apps/key_rotation/proto/key_rotation.pb.h"

namespace web_app {

namespace {

base::expected<IwaKeyRotations,
               IwaKeyRotationInfoProvider::ComponentUpdateError>
LoadKeyRotationDataImpl(const base::FilePath& file_path) {
  std::string kr_proto_data;
  if (!base::ReadFileToString(file_path, &kr_proto_data)) {
    return base::unexpected(
        IwaKeyRotationInfoProvider::ComponentUpdateError::kFileNotFound);
  }

  IwaKeyRotations kr_proto;
  if (!kr_proto.ParseFromString(kr_proto_data)) {
    return base::unexpected(
        IwaKeyRotationInfoProvider::ComponentUpdateError::kProtoParsingFailure);
  }

  for (const auto& [web_bundle_id, kr_info] : kr_proto.key_rotations()) {
    if (kr_info.has_expected_key() &&
        !base::Base64Decode(kr_info.expected_key())) {
      return base::unexpected(IwaKeyRotationInfoProvider::ComponentUpdateError::
                                  kMalformedBase64Key);
    }
  }

  return kr_proto;
}

}  // namespace

IwaKeyRotationInfoProvider* IwaKeyRotationInfoProvider::GetInstance() {
  return base::Singleton<IwaKeyRotationInfoProvider>::get();
}

void IwaKeyRotationInfoProvider::LoadKeyRotationData(
    const base::Version& component_version,
    const base::FilePath& file_path) {
  if (data_ && data_->version > component_version) {
    DispatchComponentUpdateError(component_version,
                                 ComponentUpdateError::kStaleVersion);
    return;
  }
  // `base::Unretained(this)` is fine as this is a singleton that never goes
  // away.
  base::ThreadPool::PostTaskAndReplyWithResult(
      FROM_HERE, {base::MayBlock(), base::TaskPriority::USER_VISIBLE},
      base::BindOnce(&LoadKeyRotationDataImpl, file_path),
      base::BindOnce(&IwaKeyRotationInfoProvider::OnKeyRotationDataLoaded,
                     base::Unretained(this), component_version));
}

IwaKeyRotationInfoProvider::KeyLookupResult
IwaKeyRotationInfoProvider::GetExpectedSigningKey(
    std::string_view web_bundle_id) const {
  if (!data_) {
    return IwaKeyRotationInfoProvider::KeyNotFoundTag();
  }
  auto* kr_info = base::FindOrNull(data_->proto.key_rotations(), web_bundle_id);
  if (!kr_info) {
    return IwaKeyRotationInfoProvider::KeyNotFoundTag();
  }
  if (!kr_info->has_expected_key()) {
    return IwaKeyRotationInfoProvider::KeyDisabledTag();
  }
  // base64 encoding is checked in advance in `LoadKeyRotationDataImpl`. This
  // call must return a valid value.
  return ExpectedKey(*base::Base64Decode(kr_info->expected_key()));
}

IwaKeyRotationInfoProvider::IwaKeyRotationInfoProvider() = default;
IwaKeyRotationInfoProvider::~IwaKeyRotationInfoProvider() = default;

void IwaKeyRotationInfoProvider::OnKeyRotationDataLoaded(
    const base::Version& component_version,
    base::expected<IwaKeyRotations, ComponentUpdateError> result) {
  ASSIGN_OR_RETURN(auto proto, std::move(result),
                   [&](ComponentUpdateError error) {
                     DispatchComponentUpdateError(component_version, error);
                   });

  data_ = {.version = component_version, .proto = std::move(proto)};
  DispatchComponentUpdateSuccess(component_version);
}

void IwaKeyRotationInfoProvider::AddObserver(Observer* observer) {
  observers_.AddObserver(observer);
}

void IwaKeyRotationInfoProvider::RemoveObserver(Observer* observer) {
  observers_.RemoveObserver(observer);
}

void IwaKeyRotationInfoProvider::DispatchComponentUpdateSuccess(
    const base::Version& component_version) const {
  for (auto& observer : observers_) {
    observer.OnComponentUpdateSuccess(component_version);
  }
}

void IwaKeyRotationInfoProvider::DispatchComponentUpdateError(
    const base::Version& component_version,
    ComponentUpdateError error) const {
  for (auto& observer : observers_) {
    observer.OnComponentUpdateError(component_version, error);
  }
}

}  // namespace web_app
