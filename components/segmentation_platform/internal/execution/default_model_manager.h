// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_SEGMENTATION_PLATFORM_INTERNAL_EXECUTION_DEFAULT_MODEL_MANAGER_H_
#define COMPONENTS_SEGMENTATION_PLATFORM_INTERNAL_EXECUTION_DEFAULT_MODEL_MANAGER_H_

#include <deque>
#include <map>
#include <memory>
#include <set>
#include <vector>

#include "base/functional/callback.h"
#include "base/logging.h"
#include "components/segmentation_platform/internal/database/segment_info_database.h"
#include "components/segmentation_platform/internal/proto/model_prediction.pb.h"
#include "components/segmentation_platform/public/model_provider.h"
#include "components/segmentation_platform/public/proto/model_metadata.pb.h"

namespace segmentation_platform {
using proto::SegmentId;

class SegmentInfoDatabase;

// DefaultModelManager provides support to query all default models available.
// It also provides useful methods to combine results from both the database and
// the default model.
class DefaultModelManager {
 public:
  DefaultModelManager(ModelProviderFactory* model_provider_factory,
                      const base::flat_set<SegmentId>& segment_ids);
  virtual ~DefaultModelManager();

  // Disallow copy/assign.
  DefaultModelManager(const DefaultModelManager&) = delete;
  DefaultModelManager& operator=(const DefaultModelManager&) = delete;

  // Callback for returning a list of segment infos associated with IDs.
  // The same segment ID can be repeated multiple times.
  enum class SegmentSource {
    DATABASE,
    DEFAULT_MODEL,
  };
  struct SegmentInfoWrapper {
    SegmentInfoWrapper();
    ~SegmentInfoWrapper();
    SegmentInfoWrapper(const SegmentInfoWrapper&) = delete;
    SegmentInfoWrapper& operator=(const SegmentInfoWrapper&) = delete;

    SegmentSource segment_source;
    proto::SegmentInfo segment_info;
  };
  using SegmentInfoList = std::vector<std::unique_ptr<SegmentInfoWrapper>>;
  using MultipleSegmentInfoCallback = base::OnceCallback<void(SegmentInfoList)>;

  // Utility function to get the segment info from both the database and the
  // default model for a given set of segment IDs. The result can contain
  // the same segment ID multiple times.
  virtual void GetAllSegmentInfoFromBothModels(
      const base::flat_set<SegmentId>& segment_ids,
      SegmentInfoDatabase* segment_database,
      MultipleSegmentInfoCallback callback);

  // Called to get the segment info from the default model for a given set of
  // segment IDs.
  virtual void GetAllSegmentInfoFromDefaultModel(
      const base::flat_set<SegmentId>& segment_ids,
      MultipleSegmentInfoCallback callback);

  // Returns the default provider or `nulllptr` when unavailable.
  DefaultModelProvider* GetDefaultProvider(SegmentId segment_id);

  void SetDefaultProvidersForTesting(
      std::map<SegmentId, std::unique_ptr<DefaultModelProvider>>&& providers);

 private:
  void GetNextSegmentInfoFromDefaultModel(
      std::unique_ptr<SegmentInfoList> result,
      std::deque<SegmentId> remaining_segment_ids,
      MultipleSegmentInfoCallback callback);

  void OnFetchDefaultModel(std::unique_ptr<SegmentInfoList> result,
                           std::deque<SegmentId> remaining_segment_ids,
                           MultipleSegmentInfoCallback callback,
                           SegmentId segment_id,
                           proto::SegmentationModelMetadata metadata,
                           int64_t model_version);

  void OnGetAllSegmentInfoFromDatabase(
      const base::flat_set<SegmentId>& segment_ids,
      MultipleSegmentInfoCallback callback,
      std::unique_ptr<SegmentInfoDatabase::SegmentInfoList> segment_infos);

  void OnGetAllSegmentInfoFromDefaultModel(
      MultipleSegmentInfoCallback callback,
      std::unique_ptr<SegmentInfoDatabase::SegmentInfoList>
          segment_infos_from_db,
      SegmentInfoList segment_infos_from_default_model);

  // Default model providers.
  std::map<SegmentId, std::unique_ptr<DefaultModelProvider>>
      default_model_providers_;
  const raw_ptr<ModelProviderFactory, DanglingUntriaged>
      model_provider_factory_;

  base::WeakPtrFactory<DefaultModelManager> weak_ptr_factory_{this};
};

}  // namespace segmentation_platform

#endif  // COMPONENTS_SEGMENTATION_PLATFORM_INTERNAL_EXECUTION_DEFAULT_MODEL_MANAGER_H_
