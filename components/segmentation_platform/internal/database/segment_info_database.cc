// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/segmentation_platform/internal/database/segment_info_database.h"

#include "base/callback_helpers.h"
#include "base/containers/contains.h"
#include "base/strings/string_number_conversions.h"

namespace segmentation_platform {

namespace {

std::string ToString(SegmentId segment_id) {
  return base::NumberToString(static_cast<int>(segment_id));
}

}  // namespace

SegmentInfoDatabase::SegmentInfoDatabase(
    std::unique_ptr<SegmentInfoProtoDb> database)
    : database_(std::move(database)) {}

SegmentInfoDatabase::~SegmentInfoDatabase() = default;

void SegmentInfoDatabase::Initialize(SuccessCallback callback) {
  database_->Init(
      leveldb_proto::CreateSimpleOptions(),
      base::BindOnce(&SegmentInfoDatabase::OnDatabaseInitialized,
                     weak_ptr_factory_.GetWeakPtr(), std::move(callback)));
}

void SegmentInfoDatabase::GetAllSegmentInfo(
    MultipleSegmentInfoCallback callback) {
  database_->LoadEntries(
      base::BindOnce(&SegmentInfoDatabase::OnMultipleSegmentInfoLoaded,
                     weak_ptr_factory_.GetWeakPtr(), std::move(callback)));
}

void SegmentInfoDatabase::OnMultipleSegmentInfoLoaded(
    MultipleSegmentInfoCallback callback,
    bool success,
    std::unique_ptr<std::vector<proto::SegmentInfo>> all_infos) {
  auto pairs = std::make_unique<SegmentInfoList>();
  if (success && all_infos) {
    for (auto& info : *all_infos.get()) {
      pairs->emplace_back(std::make_pair(info.segment_id(), std::move(info)));
    }
  }

  std::move(callback).Run(std::move(pairs));
}

void SegmentInfoDatabase::GetSegmentInfoForSegments(
    const std::vector<SegmentId>& segment_ids,
    MultipleSegmentInfoCallback callback) {
  std::vector<std::string> keys;
  for (SegmentId target : segment_ids)
    keys.emplace_back(ToString(target));

  database_->LoadEntriesWithFilter(
      base::BindRepeating(
          [](const std::vector<std::string>& key_dict, const std::string& key) {
            return base::Contains(key_dict, key);
          },
          keys),
      base::BindOnce(&SegmentInfoDatabase::OnMultipleSegmentInfoLoaded,
                     weak_ptr_factory_.GetWeakPtr(), std::move(callback)));
}

void SegmentInfoDatabase::GetSegmentInfo(SegmentId segment_id,
                                         SegmentInfoCallback callback) {
  database_->GetEntry(
      ToString(segment_id),
      base::BindOnce(&SegmentInfoDatabase::OnGetSegmentInfo,
                     weak_ptr_factory_.GetWeakPtr(), std::move(callback)));
}

void SegmentInfoDatabase::OnGetSegmentInfo(
    SegmentInfoCallback callback,
    bool success,
    std::unique_ptr<proto::SegmentInfo> info) {
  std::move(callback).Run(success && info ? absl::make_optional(*info)
                                          : absl::nullopt);
}

void SegmentInfoDatabase::UpdateSegment(
    SegmentId segment_id,
    absl::optional<proto::SegmentInfo> segment_info,
    SuccessCallback callback) {
  auto entries_to_save = std::make_unique<
      std::vector<std::pair<std::string, proto::SegmentInfo>>>();
  auto keys_to_delete = std::make_unique<std::vector<std::string>>();
  if (segment_info.has_value()) {
    entries_to_save->emplace_back(
        std::make_pair(ToString(segment_id), segment_info.value()));
  } else {
    keys_to_delete->emplace_back(ToString(segment_id));
  }

  database_->UpdateEntries(std::move(entries_to_save),
                           std::move(keys_to_delete), std::move(callback));
}

void SegmentInfoDatabase::SaveSegmentResult(
    SegmentId segment_id,
    absl::optional<proto::PredictionResult> result,
    SuccessCallback callback) {
  GetSegmentInfo(
      segment_id,
      base::BindOnce(&SegmentInfoDatabase::OnGetSegmentInfoForUpdatingResults,
                     weak_ptr_factory_.GetWeakPtr(), result,
                     std::move(callback)));
}

void SegmentInfoDatabase::OnGetSegmentInfoForUpdatingResults(
    absl::optional<proto::PredictionResult> result,
    SuccessCallback callback,
    absl::optional<proto::SegmentInfo> segment_info) {
  // Ignore results if the metadata no longer exists.
  if (!segment_info.has_value()) {
    std::move(callback).Run(false);
    return;
  }

  // Update results.
  if (result.has_value()) {
    segment_info->mutable_prediction_result()->CopyFrom(*result);
  } else {
    segment_info->clear_prediction_result();
  }

  auto entries_to_save = std::make_unique<
      std::vector<std::pair<std::string, proto::SegmentInfo>>>();
  entries_to_save->emplace_back(std::make_pair(
      ToString(segment_info->segment_id()), std::move(segment_info.value())));
  database_->UpdateEntries(std::move(entries_to_save),
                           std::make_unique<std::vector<std::string>>(),
                           std::move(callback));
}

void SegmentInfoDatabase::OnDatabaseInitialized(
    SuccessCallback callback,
    leveldb_proto::Enums::InitStatus status) {
  std::move(callback).Run(status == leveldb_proto::Enums::InitStatus::kOK);
}

}  // namespace segmentation_platform
