// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_SEGMENTATION_PLATFORM_INTERNAL_DATABASE_SEGMENT_INFO_DATABASE_H_
#define COMPONENTS_SEGMENTATION_PLATFORM_INTERNAL_DATABASE_SEGMENT_INFO_DATABASE_H_

#include <memory>
#include <vector>

#include "base/callback.h"
#include "components/leveldb_proto/public/proto_database.h"
#include "components/segmentation_platform/internal/proto/model_metadata.pb.h"
#include "components/segmentation_platform/internal/proto/model_prediction.pb.h"
#include "components/segmentation_platform/public/proto/segmentation_platform.pb.h"
#include "third_party/abseil-cpp/absl/types/optional.h"

namespace segmentation_platform {

using proto::SegmentId;

namespace proto {
class SegmentInfo;
class PredictionResult;
}  // namespace proto

// Represents a DB layer that stores model metadata and prediction results to
// the disk.
class SegmentInfoDatabase {
 public:
  using SuccessCallback = base::OnceCallback<void(bool)>;
  using SegmentInfoList = std::vector<std::pair<SegmentId, proto::SegmentInfo>>;
  using MultipleSegmentInfoCallback =
      base::OnceCallback<void(std::unique_ptr<SegmentInfoList>)>;
  using SegmentInfoCallback =
      base::OnceCallback<void(absl::optional<proto::SegmentInfo>)>;
  using SegmentInfoProtoDb = leveldb_proto::ProtoDatabase<proto::SegmentInfo>;

  explicit SegmentInfoDatabase(std::unique_ptr<SegmentInfoProtoDb> database);
  virtual ~SegmentInfoDatabase();

  // Disallow copy/assign.
  SegmentInfoDatabase(const SegmentInfoDatabase&) = delete;
  SegmentInfoDatabase& operator=(const SegmentInfoDatabase&) = delete;

  virtual void Initialize(SuccessCallback callback);

  // Convenient method to return combined info for all the segments in the
  // database.
  virtual void GetAllSegmentInfo(MultipleSegmentInfoCallback callback);

  // Called to get metadata for a given list of segments.
  virtual void GetSegmentInfoForSegments(
      const std::vector<SegmentId>& segment_ids,
      MultipleSegmentInfoCallback callback);

  // Called to get the metadata for a given segment.
  virtual void GetSegmentInfo(SegmentId segment_id,
                              SegmentInfoCallback callback);

  // Called to save or update metadata for a segment. The previous data is
  // overwritten. If |segment_info| is empty, the segment will be deleted.
  // TODO(shaktisahu): How does the client know if a segment is to be deleted?
  virtual void UpdateSegment(SegmentId segment_id,
                             absl::optional<proto::SegmentInfo> segment_info,
                             SuccessCallback callback);

  // Called to write the model execution results for a given segment. It will
  // first read the currently stored result, and then overwrite it with
  // |result|. If |result| is null, the existing result will be deleted.
  virtual void SaveSegmentResult(SegmentId segment_id,
                                 absl::optional<proto::PredictionResult> result,
                                 SuccessCallback callback);

 private:
  void OnDatabaseInitialized(SuccessCallback callback,
                             leveldb_proto::Enums::InitStatus status);
  void OnMultipleSegmentInfoLoaded(
      MultipleSegmentInfoCallback callback,
      bool success,
      std::unique_ptr<std::vector<proto::SegmentInfo>> all_infos);
  void OnGetSegmentInfo(SegmentInfoCallback callback,
                        bool success,
                        std::unique_ptr<proto::SegmentInfo> info);
  void OnGetSegmentInfoForUpdatingResults(
      absl::optional<proto::PredictionResult> result,
      SuccessCallback callback,
      absl::optional<proto::SegmentInfo> segment_info);

  std::unique_ptr<SegmentInfoProtoDb> database_;

  base::WeakPtrFactory<SegmentInfoDatabase> weak_ptr_factory_{this};
};

}  // namespace segmentation_platform

#endif  // COMPONENTS_SEGMENTATION_PLATFORM_INTERNAL_DATABASE_SEGMENT_INFO_DATABASE_H_
