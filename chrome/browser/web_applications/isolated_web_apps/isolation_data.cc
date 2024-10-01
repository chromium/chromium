// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/web_applications/isolated_web_apps/isolation_data.h"

#include "base/containers/to_value_list.h"
#include "base/values.h"
#include "base/version.h"

namespace web_app {

namespace {

void PersistFieldsForUpdateImpl(IsolationData::Builder& builder,
                                const IsolationData& isolation_data) {
  builder.SetControlledFramePartitions(
      isolation_data.controlled_frame_partitions());
  if (isolation_data.update_manifest_url()) {
    builder.SetUpdateManifestUrl(*isolation_data.update_manifest_url());
  }
}

}  // namespace

IsolationData::IsolationData(
    IsolatedWebAppStorageLocation location,
    base::Version version,
    std::set<std::string> controlled_frame_partitions,
    std::optional<PendingUpdateInfo> pending_update_info,
    std::optional<IsolatedWebAppIntegrityBlockData> integrity_block_data,
    std::optional<GURL> update_manifest_url)
    : location_(std::move(location)),
      version_(std::move(version)),
      controlled_frame_partitions_(std::move(controlled_frame_partitions)),
      pending_update_info_(std::move(pending_update_info)),
      integrity_block_data_(std::move(integrity_block_data)),
      update_manifest_url_(std::move(update_manifest_url)) {}

IsolationData::~IsolationData() = default;
IsolationData::IsolationData(const IsolationData&) = default;
IsolationData& IsolationData::operator=(const IsolationData&) = default;
IsolationData::IsolationData(IsolationData&&) = default;
IsolationData& IsolationData::operator=(IsolationData&&) = default;

base::Value IsolationData::AsDebugValue() const {
  auto debug_dict =
      base::Value::Dict()
          .Set("isolated_web_app_location", location_.ToDebugValue())
          .Set("version", version_.GetString())
          .Set("controlled_frame_partitions (on-disk)",
               base::ToValueList(controlled_frame_partitions_))
          .Set("pending_update_info", pending_update_info_
                                          ? pending_update_info_->AsDebugValue()
                                          : base::Value())
          .Set("integrity_block_data",
               integrity_block_data_ ? integrity_block_data_->AsDebugValue()
                                     : base::Value());

  if (update_manifest_url_) {
    debug_dict.Set("update_manifest_url", update_manifest_url_->spec());
  }

  return base::Value(std::move(debug_dict));
}

IsolationData::PendingUpdateInfo::PendingUpdateInfo(
    IsolatedWebAppStorageLocation location,
    base::Version version,
    std::optional<IsolatedWebAppIntegrityBlockData> integrity_block_data)
    : location(std::move(location)),
      version(std::move(version)),
      integrity_block_data(std::move(integrity_block_data)) {}

IsolationData::PendingUpdateInfo::~PendingUpdateInfo() = default;

IsolationData::PendingUpdateInfo::PendingUpdateInfo(const PendingUpdateInfo&) =
    default;
IsolationData::PendingUpdateInfo& IsolationData::PendingUpdateInfo::operator=(
    const PendingUpdateInfo&) = default;

base::Value IsolationData::PendingUpdateInfo::AsDebugValue() const {
  return base::Value(
      base::Value::Dict()
          .Set("isolated_web_app_location", location.ToDebugValue())
          .Set("version", version.GetString())
          .Set("integrity_block_data",
               integrity_block_data ? integrity_block_data->AsDebugValue()
                                    : base::Value()));
}

IsolationData::Builder::Builder(IsolatedWebAppStorageLocation location,
                                base::Version version)
    : location_(std::move(location)), version_(std::move(version)) {}

IsolationData::Builder::Builder(const IsolationData& isolation_data)
    : location_(isolation_data.location()),
      version_(isolation_data.version()),
      controlled_frame_partitions_(
          isolation_data.controlled_frame_partitions()),
      pending_update_info_(isolation_data.pending_update_info()),
      integrity_block_data_(isolation_data.integrity_block_data()),
      update_manifest_url_(isolation_data.update_manifest_url()) {}

IsolationData::Builder::~Builder() = default;

IsolationData::Builder::Builder(const IsolationData::Builder&) = default;
IsolationData::Builder& IsolationData::Builder::operator=(
    const IsolationData::Builder&) = default;
IsolationData::Builder::Builder(IsolationData::Builder&&) = default;
IsolationData::Builder& IsolationData::Builder::operator=(
    IsolationData::Builder&&) = default;

IsolationData::Builder& IsolationData::Builder::SetControlledFramePartitions(
    std::set<std::string> controlled_frame_partitions) & {
  controlled_frame_partitions_ = std::move(controlled_frame_partitions);
  return *this;
}

IsolationData::Builder&& IsolationData::Builder::SetControlledFramePartitions(
    std::set<std::string> controlled_frame_partitions) && {
  controlled_frame_partitions_ = std::move(controlled_frame_partitions);
  return std::move(*this);
}

IsolationData::Builder& IsolationData::Builder::SetPendingUpdateInfo(
    IsolationData::PendingUpdateInfo pending_update_info) & {
  CHECK_EQ(pending_update_info.location.dev_mode(), location_.dev_mode());
  pending_update_info_ = std::move(pending_update_info);
  return *this;
}

IsolationData::Builder&& IsolationData::Builder::SetPendingUpdateInfo(
    IsolationData::PendingUpdateInfo pending_update_info) && {
  CHECK_EQ(pending_update_info.location.dev_mode(), location_.dev_mode());
  pending_update_info_ = std::move(pending_update_info);
  return std::move(*this);
}

IsolationData::Builder& IsolationData::Builder::ClearPendingUpdateInfo() & {
  pending_update_info_ = std::nullopt;
  return *this;
}

IsolationData::Builder&& IsolationData::Builder::ClearPendingUpdateInfo() && {
  pending_update_info_ = std::nullopt;
  return std::move(*this);
}

IsolationData::Builder& IsolationData::Builder::SetIntegrityBlockData(
    IsolatedWebAppIntegrityBlockData integrity_block_data) & {
  integrity_block_data_ = std::move(integrity_block_data);
  return *this;
}

IsolationData::Builder&& IsolationData::Builder::SetIntegrityBlockData(
    IsolatedWebAppIntegrityBlockData integrity_block_data) && {
  integrity_block_data_ = std::move(integrity_block_data);
  return std::move(*this);
}

IsolationData::Builder& IsolationData::Builder::SetUpdateManifestUrl(
    GURL update_manifest_url) & {
  CHECK(location_.dev_mode())
      << "This field is supposed to be used only with dev mode installs via "
         "chrome://web-app-internals.";
  update_manifest_url_ = std::move(update_manifest_url);
  return *this;
}

IsolationData::Builder&& IsolationData::Builder::SetUpdateManifestUrl(
    GURL update_manifest_url) && {
  CHECK(location_.dev_mode())
      << "This field is supposed to be used only with dev mode installs via "
         "chrome://web-app-internals.";
  update_manifest_url_ = std::move(update_manifest_url);
  return std::move(*this);
}

IsolationData::Builder& IsolationData::Builder::PersistFieldsForUpdate(
    const IsolationData& isolation_data) & {
  PersistFieldsForUpdateImpl(*this, isolation_data);
  return *this;
}

IsolationData::Builder&& IsolationData::Builder::PersistFieldsForUpdate(
    const IsolationData& isolation_data) && {
  PersistFieldsForUpdateImpl(*this, isolation_data);
  return std::move(*this);
}

IsolationData IsolationData::Builder::Build() && {
  return IsolationData(
      std::move(location_), std::move(version_),
      std::move(controlled_frame_partitions_), std::move(pending_update_info_),
      std::move(integrity_block_data_), std::move(update_manifest_url_));
}

}  // namespace web_app
