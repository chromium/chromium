// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_SEGMENTATION_PLATFORM_INTERNAL_DATABASE_SEGMENT_INFO_DATABASE_H_
#define COMPONENTS_SEGMENTATION_PLATFORM_INTERNAL_DATABASE_SEGMENT_INFO_DATABASE_H_

#include <memory>
#include <optional>
#include <vector>

#include "base/containers/flat_set.h"
#include "base/functional/callback.h"
#include "components/leveldb_proto/public/proto_database.h"
#include "components/segmentation_platform/internal/database/segment_info_cache.h"
#include "components/segmentation_platform/internal/proto/model_prediction.pb.h"
#include "components/segmentation_platform/public/proto/model_metadata.pb.h"
#include "components/segmentation_platform/public/proto/segmentation_platform.pb.h"
#include "components/segmentation_platform/public/trigger.h"

namespace segmentation_platform {

using proto::ModelSource;
using proto::SegmentId;

namespace proto {
class SegmentInfo;
class PredictionResult;
}  // namespace proto

// Represents a DB layer that stores model metadata and prediction results to
// the disk. At startup, all stored data is loaded into cache. Following
// interactions with this class will be directly from/to the cache. The disk
// will be updated asynchronously by this class.
class SegmentInfoDatabase {
 public:
  using SuccessCallback = base::OnceCallback<void(bool)>;
  using SegmentInfoList =
      std::vector<std::pair<SegmentId, const proto::SegmentInfo*>>;
  using MultipleSegmentInfoCallback =
      base::OnceCallback<void(std::unique_ptr<SegmentInfoList>)>;
  using SegmentInfoCallback =
      base::OnceCallback<void(std::optional<proto::SegmentInfo>)>;
  using SegmentInfoProtoDb = leveldb_proto::ProtoDatabase<proto::SegmentInfo>;
  using TrainingDataCallback =
      base::OnceCallback<void(std::optional<proto::TrainingData>)>;

  explicit SegmentInfoDatabase(std::unique_ptr<SegmentInfoProtoDb> database,
                               std::unique_ptr<SegmentInfoCache> cache);
  virtual ~SegmentInfoDatabase();

  // Disallow copy/assign.
  SegmentInfoDatabase(const SegmentInfoDatabase&) = delete;
  SegmentInfoDatabase& operator=(const SegmentInfoDatabase&) = delete;

  virtual void Initialize(SuccessCallback callback);

  // Called to get metadata for a given list of server model segments.
  virtual void GetSegmentInfoForSegments(
      const base::flat_set<SegmentId>& segment_ids,
      MultipleSegmentInfoCallback callback);

  // Called to get metadata for a given list of both server and default model
  // segments.
  virtual std::unique_ptr<SegmentInfoDatabase::SegmentInfoList>
  GetSegmentInfoForBothModels(const base::flat_set<SegmentId>& segment_ids);

  // Called to get the metadata for a given segment. ModelSource defines whether
  // to give metadata from server/default models for the given segment.
  virtual void GetSegmentInfo(SegmentId segment_id,
                              ModelSource model_source,
                              SegmentInfoCallback callback);

  // Gets the cached segment info. Segment info is always cached if available to
  // the service, can be used as replacement for GetSegmentInfo(). Returns
  // nullptr when not available.
  virtual const SegmentInfo* GetCachedSegmentInfo(SegmentId segment_id,
                                                  ModelSource model_source);

  // Called to get the training data for a given segment with given model source
  // and request ID. If delete_from_db is set to true, it will delete the
  // corresponding entry in the cache and in the database.
  virtual void GetTrainingData(SegmentId segment_id,
                               ModelSource model_source,
                               TrainingRequestId request_id,
                               bool delete_from_db,
                               TrainingDataCallback callback);

  // Called to save or update metadata for a segment. The previous data is
  // overwritten. If |segment_info| is empty, the segment will be deleted.
  // Updates are written to the cache and callback is returned to the client.
  // The database will be updated asynchronously after.
  // TODO(shaktisahu): How does the client know if a segment is to be deleted?
  virtual void UpdateSegment(SegmentId segment_id,
                             ModelSource model_source,
                             std::optional<proto::SegmentInfo> segment_info,
                             SuccessCallback callback);

  // Called to save or update metadata for multiple segments in a single
  // database call. The previous data for all the provided segments is
  // overwritten with new data. `segments_to_delete` includes list of
  // segment ids to be deleted from the database.
  virtual void UpdateMultipleSegments(
      const SegmentInfoList& segments_to_update,
      const std::vector<std::pair<SegmentId, ModelSource>>& segments_to_delete,
      SuccessCallback callback);

  // Called to write the model execution results for a given segment. It will
  // first read the currently stored result for given model source, and then
  // overwrite it with |result|. If |result| is null, the existing result will
  // be deleted.
  virtual void SaveSegmentResult(SegmentId segment_id,
                                 ModelSource model_source,
                                 std::optional<proto::PredictionResult> result,
                                 SuccessCallback callback);

  // Called to write partial training data for a given segment and model source.
  // New training data are appended to the existing ones.
  virtual void SaveTrainingData(SegmentId segment_id,
                                ModelSource model_source,
                                const proto::TrainingData& data,
                                SuccessCallback callback);

 private:
  void OnDatabaseInitialized(SuccessCallback callback,
                             leveldb_proto::Enums::InitStatus status);

  void OnLoadAllEntries(
      SuccessCallback callback,
      bool success,
      std::unique_ptr<std::vector<proto::SegmentInfo>> all_infos);

  std::unique_ptr<SegmentInfoProtoDb> database_;

  std::unique_ptr<SegmentInfoCache> cache_;

  base::WeakPtrFactory<SegmentInfoDatabase> weak_ptr_factory_{this};
};

}  // namespace segmentation_platform

#endif  // COMPONENTS_SEGMENTATION_PLATFORM_INTERNAL_DATABASE_SEGMENT_INFO_DATABASE_H_
