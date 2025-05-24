// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_SAFE_BROWSING_CONTENT_BROWSER_CLIENT_SIDE_PHISHING_MODEL_H_
#define COMPONENTS_SAFE_BROWSING_CONTENT_BROWSER_CLIENT_SIDE_PHISHING_MODEL_H_

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
#include "base/task/sequenced_task_runner.h"
#include "base/task/thread_pool.h"
#include "base/thread_annotations.h"
#include "components/optimization_guide/core/optimization_target_model_observer.h"
#include "components/safe_browsing/core/common/fbs/client_model_generated.h"
#include "components/safe_browsing/core/common/proto/client_model.pb.h"
#include "components/safe_browsing/core/common/proto/csd.pb.h"

namespace optimization_guide {
class OptimizationGuideModelProvider;
}  // namespace optimization_guide

namespace safe_browsing {

enum class CSDModelType { kNone = 0, kFlatbuffer = 1 };

// This holds the currently active client side phishing detection model.
//
// The data to populate it is fetched periodically from Google to get the most
// up-to-date model. We assume it is updated at most every few hours.
//
// This class lives on UI thread and can only be called there. In particular
// GetModelStr() returns a string reference, which assumes the string won't be
// used and updated at the same time.

class ClientSidePhishingModel
    : public optimization_guide::OptimizationTargetModelObserver {
 public:
  ClientSidePhishingModel(
      optimization_guide::OptimizationGuideModelProvider* opt_guide);

  ~ClientSidePhishingModel() override;

  // optimization_guide::OptimizationTargetModelObserver implementation
  void OnModelUpdated(
      optimization_guide::proto::OptimizationTarget optimization_target,
      base::optional_ref<const optimization_guide::ModelInfo> model_info)
      override;

  // Enhanced Safe Browsing users receive an additional image embedding model to
  // be attached to CSD-Phishing ping to better train the models.
  void SubscribeToImageEmbedderOptimizationGuide();

  void UnsubscribeToImageEmbedderOptimizationGuide();

  // Register a callback to be notified whenever the model changes. All
  // notifications will occur on the UI thread.
  base::CallbackListSubscription RegisterCallback(
      base::RepeatingCallback<void()> callback);

  // Returns whether we currently have a model.
  bool IsEnabled() const;

  static bool VerifyCSDFlatBufferIndicesAndFields(
      const flat::ClientSideModel* model);

  // Returns model type (flatbuffer or none).
  CSDModelType GetModelType() const;

  // Returns the shared memory region for the flatbuffer.
  base::ReadOnlySharedMemoryRegion GetModelSharedMemoryRegion() const;

  const base::File& GetVisualTfLiteModel() const;

  const base::File& GetImageEmbeddingModel() const;

  bool HasImageEmbeddingModel();

  bool IsModelMetadataImageEmbeddingVersionMatching();

  int GetTriggerModelVersion();

  void SetVisualTfLiteModelForTesting(base::File file);
  // Overrides model type.
  void SetModelTypeForTesting(CSDModelType model_type);
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
      std::optional<optimization_guide::proto::Any> model_metadata,
      std::pair<std::string, base::File> model_and_tflite);

  void OnImageEmbeddingModelLoaded(
      std::optional<optimization_guide::proto::Any> model_metadata,
      base::File image_embedding_model_data);

  void SetModelAndVisualTfLiteForTesting(
      const base::FilePath& model_file_path,
      const base::FilePath& visual_tf_lite_model_path);

  // Updates the internal model string, when one is received from testing in
  // client_side_phishing_model_unittest
  void SetModelStringForTesting(const std::string& model_str,
                                base::File visual_tflite_model);

  bool IsSubscribedToImageEmbeddingModelUpdates();

 private:
  static const int kInitialClientModelFetchDelayMs;

  void NotifyCallbacksOnUI();

  // Callback when the file overriding the model has been read in
  // client_side_phishing_model_unittest
  void OnGetOverridenModelData(
      CSDModelType model_type,
      std::pair<std::string, base::File> model_and_tflite);

  // The list of callbacks to notify when a new model is ready. Guarded by
  // sequence_checker_. Will always be notified on the UI thread.
  base::RepeatingCallbackList<void()> callbacks_
      GUARDED_BY_CONTEXT(sequence_checker_);

  // Model protobuf string. Guarded by sequence_checker_.
  std::string model_str_ GUARDED_BY_CONTEXT(sequence_checker_);

  // Visual TFLite model file. Guarded by sequence_checker_.
  std::optional<base::File> visual_tflite_model_
      GUARDED_BY_CONTEXT(sequence_checker_);

  // Image Embedding TfLite model file. Guarded by sequence_checker_.
  std::optional<base::File> image_embedding_model_
      GUARDED_BY_CONTEXT(sequence_checker_);

  // Thresholds in visual TFLite model file to be used for comparison after
  // visual classification
  base::flat_map<std::string, TfLiteModelMetadata::Threshold> thresholds_;

  // Model type as inferred by feature flag. Guarded by sequence_checker_.
  CSDModelType model_type_ GUARDED_BY_CONTEXT(sequence_checker_) =
      CSDModelType::kNone;

  // MappedReadOnlyRegion where the flatbuffer has been copied to. Guarded by
  // sequence_checker_.
  base::MappedReadOnlyRegion mapped_region_
      GUARDED_BY_CONTEXT(sequence_checker_) = base::MappedReadOnlyRegion();

  FRIEND_TEST_ALL_PREFIXES(ClientSidePhishingModelTest, CanOverrideWithFlag);

  // Optimization Guide service that provides the client side detection
  // model files for this service. Optimization Guide Service is a
  // BrowserContextKeyedServiceFactory and should not be used after Shutdown
  raw_ptr<optimization_guide::OptimizationGuideModelProvider> opt_guide_;

  // These two integer values will be set from reading the metadata specified
  // under each optimization target. These two are used to match the model
  // pairings properly. If the two values match, then the image embedding model
  // will be sent to the renderer process along with the trigger models. They do
  // not reflect any versions used in the model file itself.
  std::optional<int> trigger_model_opt_guide_metadata_image_embedding_version_;
  std::optional<int>
      embedding_model_opt_guide_metadata_image_embedding_version_;

  // This value is set from a version set in the model file's metadata. This
  // value will be used to send to the CSD service class so that it can be added
  // to the debugging metadata so that we can understand what version has been
  // sent to the renderer.
  std::optional<int> trigger_model_version_;

  scoped_refptr<base::SequencedTaskRunner> background_task_runner_;

  // If the users subscribe to ESB, the code will add an observer to the
  // OptimizationGuide service for the image embedder model. We can choose to
  // remove the observer, but it will be on the list to be removed, and not
  // removed instantly. Therefore, if the user subscribes, unsubscribes, and
  // re-subscribes again in very quick succession, the code will crash because
  // the DCHECK fails, indicating that the observer is added already. Therefore,
  // this will be a one time use flag.
  bool subscribed_to_image_embedder_ = false;

  SEQUENCE_CHECKER(sequence_checker_);

  base::TimeTicks beginning_time_;

  base::WeakPtrFactory<ClientSidePhishingModel> weak_ptr_factory_{this};
};

}  // namespace safe_browsing

#endif  // COMPONENTS_SAFE_BROWSING_CONTENT_BROWSER_CLIENT_SIDE_PHISHING_MODEL_H_
