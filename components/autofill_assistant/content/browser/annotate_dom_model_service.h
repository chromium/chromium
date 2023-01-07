// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_AUTOFILL_ASSISTANT_CONTENT_BROWSER_ANNOTATE_DOM_MODEL_SERVICE_H_
#define COMPONENTS_AUTOFILL_ASSISTANT_CONTENT_BROWSER_ANNOTATE_DOM_MODEL_SERVICE_H_

#include <memory>
#include <vector>

#include "base/callback.h"
#include "base/files/file.h"
#include "base/files/file_path.h"
#include "base/memory/raw_ptr.h"
#include "base/memory/weak_ptr.h"
#include "components/autofill_assistant/content/common/proto/semantic_feature_overrides.pb.h"
#include "components/keyed_service/core/keyed_service.h"
#include "components/optimization_guide/core/optimization_target_model_observer.h"
#include "third_party/abseil-cpp/absl/types/optional.h"

namespace optimization_guide {
class OptimizationGuideModelProvider;
}  // namespace optimization_guide

namespace autofill_assistant {

// Service that manages models required to support annotating DOM in the
// browser for Autofill Assistant. Currently, the service should only be used
// in the browser as it relies on the Optimization Guide.
class AnnotateDomModelService
    : public KeyedService,
      public optimization_guide::OptimizationTargetModelObserver {
 public:
  using NotifyModelAvailableCallback = base::OnceCallback<void(bool)>;

  AnnotateDomModelService(
      optimization_guide::OptimizationGuideModelProvider* opt_guide,
      const scoped_refptr<base::SequencedTaskRunner>& background_task_runner);
  ~AnnotateDomModelService() override;

  // KeyedService implementation:
  void Shutdown() override;

  // optimization_guide::OptimizationTargetModelObserver implementation:
  void OnModelUpdated(
      optimization_guide::proto::OptimizationTarget optimization_target,
      const optimization_guide::ModelInfo& model_info) override;

  // Returns the annotate dom model file. If this returns a nullopt, see the
  // |NotifyOnModelFileAvailable| for an asynchronous notification of the model
  // being available.
  absl::optional<base::File> GetModelFile() const;

  // Returns the model of the version. If this returns a nullopt, see the
  // |NotifyOnModelFileAvailable| for an asynchronous notification of the model
  // being available.
  absl::optional<int64_t> GetModelVersion() const;

  // Returns the overrides policy as a serialized binary proto representation
  // that will be passed to renderer processes.
  std::string GetOverridesPolicy() const;
  virtual bool SetOverridesPolicy(SemanticSelectorPolicy policy);

  // If the model file is not available, requestors can ask to be notified, via
  // |callback|. This enables a two-step approach to relabily get the model file
  // when it becomes available if the requestor needs the file right when it
  // becomes available. This is to ensure that if the |callback| becomes empty,
  // only the notification gets dropped, rather than the model file which has
  // to be closed on a background thread.
  void NotifyOnModelFileAvailable(NotifyModelAvailableCallback callback);

  void SetModelFileForTest(base::File model_file);

 private:
  void OnModelFileLoaded(int64_t model_version, base::File model_file);

  // Optimization Guide Service that provides model files for this service.
  raw_ptr<optimization_guide::OptimizationGuideModelProvider> opt_guide_ =
      nullptr;

  // The file that contains the annotate DOM model. Available when the
  // file path has been provided by the Optimization Guide and has been
  // successfully loaded.
  absl::optional<base::File> model_file_;
  // The version of the current model.
  absl::optional<int64_t> model_version_;

  // A serialized binary representation of a SemanticSelectorPolicy proto.
  std::string overrides_policy_binary_proto_;

  // The set of callbacks associated with requests for the language detection
  // model.
  std::vector<NotifyModelAvailableCallback> pending_model_requests_;

  scoped_refptr<base::SequencedTaskRunner> background_task_runner_;

  base::WeakPtrFactory<AnnotateDomModelService> weak_ptr_factory_{this};
};

}  //  namespace autofill_assistant

#endif  // COMPONENTS_AUTOFILL_ASSISTANT_CONTENT_BROWSER_ANNOTATE_DOM_MODEL_SERVICE_H_
