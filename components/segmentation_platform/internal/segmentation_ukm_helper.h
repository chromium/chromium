// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_SEGMENTATION_PLATFORM_INTERNAL_SEGMENTATION_UKM_HELPER_H_
#define COMPONENTS_SEGMENTATION_PLATFORM_INTERNAL_SEGMENTATION_UKM_HELPER_H_

#include <optional>
#include <vector>

#include "base/containers/flat_set.h"
#include "base/no_destructor.h"
#include "base/time/time.h"
#include "components/segmentation_platform/internal/proto/model_prediction.pb.h"
#include "components/segmentation_platform/public/model_provider.h"
#include "components/segmentation_platform/public/proto/segmentation_platform.pb.h"
#include "services/metrics/public/cpp/ukm_source_id.h"

namespace base {
class Clock;
}

namespace ukm::builders {
class Segmentation_ModelExecution;
}  // namespace ukm::builders

namespace segmentation_platform {

using proto::SegmentId;
struct SelectedSegment;

// A helper class to record segmentation model execution results in UKM.
class SegmentationUkmHelper {
 public:
  static SegmentationUkmHelper* GetInstance();
  SegmentationUkmHelper(const SegmentationUkmHelper&) = delete;
  SegmentationUkmHelper& operator=(const SegmentationUkmHelper&) = delete;

  // Record segmentation model information and input/output after the
  // executing the model, and return the UKM source ID.
  ukm::SourceId RecordModelExecutionResult(
      SegmentId segment_id,
      int64_t model_version,
      const ModelProvider::Request& input_tensor,
      const std::vector<float>& results);

  // Record segmentation model training data as UKM message.
  // `segment_id`: The segment ID of the model to record data.
  // `model_version`: The current model version specified in model config.
  // `ukm_source_id`: Optional UKM source ID to attach the URL to recorded data.
  //                  The caller has to create the source and attach URL. If the
  //                  value is kInvalidSourceId, then this function creates an
  //                  empty source without URL.
  // `input_tensors`: Contains the values for training inputs.
  // `outputs`: Contains the values for outputs.
  // `output_indexes`: Contains the indexes for outputs that needs to be
  //                   included in the ukm message.
  // `prediction_result`: The most recent model execution result.
  // `selected_segment`: Legacy option no longer used in new models. The
  //                     recently selected segment for the feature that is tied
  //                     to the ML model.
  // Returns the UKM source ID of the recorded data, invalid if not recorded.
  ukm::SourceId RecordTrainingData(
      SegmentId segment_id,
      int64_t model_version,
      ukm::SourceId ukm_source_id,
      const ModelProvider::Request& input_tensors,
      const ModelProvider::Response& outputs,
      const std::vector<int>& output_indexes,
      std::optional<proto::PredictionResult> prediction_result,
      std::optional<SelectedSegment> selected_segment);

  // Returns whether a segment needs to upload training tensors.
  bool IsUploadRequested(const proto::SegmentInfo& segment_info) const;

  // Helper method to encode a float number into int64.
  static int64_t FloatToInt64(float f);

  // Helper method to check if data is allowed to upload through ukm
  // given a clock and the signal storage length.
  static bool AllowedToUploadData(base::TimeDelta signal_storage_length,
                                  base::Clock* clock);

 private:
  void Initialize();

  bool AddInputsToUkm(ukm::builders::Segmentation_ModelExecution* ukm_builder,
                      SegmentId segment_id,
                      int64_t model_version,
                      const ModelProvider::Request& input_tensor);

  bool AddOutputsToUkm(ukm::builders::Segmentation_ModelExecution* ukm_builder,
                       const ModelProvider::Response& outputs,
                       const std::vector<int>& output_indexes);

  friend class base::NoDestructor<SegmentationUkmHelper>;
  friend class SegmentationUkmHelperTest;
  SegmentationUkmHelper();
  ~SegmentationUkmHelper();

  int sampling_rate_;
  base::flat_set<SegmentId> allowed_segment_ids_;
};

}  // namespace segmentation_platform

#endif  // COMPONENTS_SEGMENTATION_PLATFORM_INTERNAL_SEGMENTATION_UKM_HELPER_H_
