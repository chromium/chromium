// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_SAFE_BROWSING_CONTENT_BROWSER_CLIENT_SIDE_PHISHING_MODEL_OPTIMIZATION_GUIDE_H_
#define COMPONENTS_SAFE_BROWSING_CONTENT_BROWSER_CLIENT_SIDE_PHISHING_MODEL_OPTIMIZATION_GUIDE_H_

#include <map>
#include <memory>

#include "base/callback_list.h"
#include "base/containers/flat_map.h"
#include "base/files/file.h"
#include "base/gtest_prod_util.h"
#include "base/memory/raw_ptr.h"
#include "base/memory/read_only_shared_memory_region.h"
#include "base/memory/weak_ptr.h"
#include "base/sequence_checker.h"
#include "base/synchronization/lock.h"
#include "base/thread_annotations.h"
#include "components/optimization_guide/core/optimization_target_model_observer.h"
#include "components/safe_browsing/content/browser/client_side_phishing_model.h"
#include "components/safe_browsing/core/common/fbs/client_model_generated.h"
#include "components/safe_browsing/core/common/proto/client_model.pb.h"
#include "components/safe_browsing/core/common/proto/csd.pb.h"

namespace optimization_guide {
class OptimizationGuideModelProvider;
}  // namespace optimization_guide

namespace safe_browsing {

enum class CSDModelTypeOptimizationGuide {
  kNone = 0,
  kProtobuf = 1,
  kFlatbuffer = 2
};

// This holds the currently active client side phishing detection model.
//
// The data to populate it is fetched periodically from Google to get the most
// up-to-date model. We assume it is updated at most every few hours.
//
// This class lives on UI thread and can only be called there. In particular
// GetModelStr() returns a string reference, which assumes the string won't be
// used and updated at the same time.

class ClientSidePhishingModelOptimizationGuide
    : public optimization_guide::OptimizationTargetModelObserver {
 public:
  ClientSidePhishingModelOptimizationGuide(
      optimization_guide::OptimizationGuideModelProvider* opt_guide,
      const scoped_refptr<base::SequencedTaskRunner>& background_task_runner);

  ~ClientSidePhishingModelOptimizationGuide() override;

  // optimization_guide::OptimizationTargetModelObserver implementation
  void OnModelUpdated(
      optimization_guide::proto::OptimizationTarget optimization_target,
      const optimization_guide::ModelInfo& model_info) override;

  // Register a callback to be notified whenever the model changes. All
  // notifications will occur on the UI thread.
  base::CallbackListSubscription RegisterCallback(
      base::RepeatingCallback<void()> callback);

  // Returns whether we currently have a model.
  bool IsEnabled() const;

  static bool VerifyCSDFlatBufferIndicesAndFields(
      const flat::ClientSideModel* model);

  // Returns model type (protobuf or flatbuffer).
  CSDModelTypeOptimizationGuide GetModelType() const;

  // Returns the model string, as a serialized protobuf or flatbuffer.
  const std::string& GetModelStr() const;

  // Returns the shared memory region for the flatbuffer.
  base::ReadOnlySharedMemoryRegion GetModelSharedMemoryRegion() const;

  const base::File& GetVisualTfLiteModel() const;

  // Overrides the model string for use in tests.
  void SetModelStrForTesting(const std::string& model_str);
  void SetVisualTfLiteModelForTesting(base::File file);
  // Overrides model type.
  void SetModelTypeForTesting(CSDModelTypeOptimizationGuide model_type);
  // Removes mapping.
  void ClearMappedRegionForTesting();
  // Get flatbuffer memory address.
  void* GetFlatBufferMemoryAddressForTesting();
  // Notifies all the callbacks of a change in model.
  void NotifyCallbacksOfUpdateForTesting();

  const base::flat_map<std::string, TfLiteModelMetadata::Threshold>&
  GetVisualTfLiteModelThresholds() const;

  // This function is used to override internal model for testing in
  // client_side_phishing_model_unittest
  void MaybeOverrideModel();

  void OnModelAndVisualTfLiteFileLoaded(
      std::pair<std::string, base::File> model_and_tflite);

  void SetModelAndVisualTfLiteForTesting(
      const base::FilePath& model_file_path,
      const base::FilePath& visual_tf_lite_model_path);

  // Updates the internal model string, when one is received from testing in
  // client_side_phishing_model_unittest
  void SetModelStringForTesting(const std::string& model_str,
                                base::File visual_tflite_model);

 private:
  static const int kInitialClientModelFetchDelayMs;

  void NotifyCallbacksOnUI();

  // Callback when the file overriding the model has been read in
  // client_side_phishing_model_unittest
  void OnGetOverridenModelData(
      CSDModelTypeOptimizationGuide model_type,
      std::pair<std::string, base::File> model_and_tflite);

  // The list of callbacks to notify when a new model is ready. Guarded by
  // sequence_checker_. Will always be notified on the UI thread.
  base::RepeatingCallbackList<void()> callbacks_
      GUARDED_BY_CONTEXT(sequence_checker_);

  // Model protobuf string. Guarded by sequence_checker_.
  std::string model_str_ GUARDED_BY_CONTEXT(sequence_checker_);

  // Visual TFLite model file. Guarded by sequence_checker_.
  absl::optional<base::File> visual_tflite_model_
      GUARDED_BY_CONTEXT(sequence_checker_);

  // Thresholds in visual TFLite model file to be used for comparison after
  // visual classification
  base::flat_map<std::string, TfLiteModelMetadata::Threshold> thresholds_;

  // Model type as inferred by feature flag. Guarded by sequence_checker_.
  CSDModelTypeOptimizationGuide model_type_ GUARDED_BY_CONTEXT(
      sequence_checker_) = CSDModelTypeOptimizationGuide::kNone;

  // MappedReadOnlyRegion where the flatbuffer has been copied to. Guarded by
  // sequence_checker_.
  base::MappedReadOnlyRegion mapped_region_
      GUARDED_BY_CONTEXT(sequence_checker_) = base::MappedReadOnlyRegion();

  FRIEND_TEST_ALL_PREFIXES(ClientSidePhishingModelOptimizationGuideTest,
                           CanOverrideWithFlag);

  // Optimization Guide service that provides the client side detection
  // model files for this service. Optimization Guide Service is a
  // BrowserContextKeyedServiceFactory and should not be used after Shutdown
  raw_ptr<optimization_guide::OptimizationGuideModelProvider> opt_guide_;

  scoped_refptr<base::SequencedTaskRunner> background_task_runner_;

  SEQUENCE_CHECKER(sequence_checker_);

  base::TimeTicks beginning_time_;

  base::WeakPtrFactory<ClientSidePhishingModelOptimizationGuide>
      weak_ptr_factory_{this};
};

}  // namespace safe_browsing

#endif  // COMPONENTS_SAFE_BROWSING_CONTENT_BROWSER_CLIENT_SIDE_PHISHING_MODEL_OPTIMIZATION_GUIDE_H_
