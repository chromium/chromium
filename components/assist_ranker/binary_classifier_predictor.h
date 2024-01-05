// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_ASSIST_RANKER_BINARY_CLASSIFIER_PREDICTOR_H_
#define COMPONENTS_ASSIST_RANKER_BINARY_CLASSIFIER_PREDICTOR_H_

#include "base/memory/weak_ptr.h"
#include "components/assist_ranker/base_predictor.h"
#include "components/assist_ranker/proto/ranker_example.pb.h"

namespace base {
class FilePath;
}

namespace network {
class SharedURLLoaderFactory;
}

namespace assist_ranker {

class GenericLogisticRegressionInference;

// Predictor class for models that output a binary decision.
class BinaryClassifierPredictor final : public BasePredictor {
 public:
  BinaryClassifierPredictor(const BinaryClassifierPredictor&) = delete;
  BinaryClassifierPredictor& operator=(const BinaryClassifierPredictor&) =
      delete;

  ~BinaryClassifierPredictor() override;

  // Returns an new predictor instance with the given |config| and initialize
  // its model loader. The |request_context getter| is passed to the
  // predictor's model_loader which holds it as scoped_refptr.
  [[nodiscard]] static std::unique_ptr<BinaryClassifierPredictor> Create(
      const PredictorConfig& config,
      const base::FilePath& model_path,
      scoped_refptr<network::SharedURLLoaderFactory> url_loader_factory);

  // Fills in a boolean decision given a RankerExample. Returns false if a
  // prediction could not be made (e.g. the model is not loaded yet).
  [[nodiscard]] bool Predict(const RankerExample& example, bool* prediction);

  // Returns a score between 0 and 1. Returns false if a
  // prediction could not be made (e.g. the model is not loaded yet).
  [[nodiscard]] bool PredictScore(const RankerExample& example,
                                  float* prediction);

  // Validates that the loaded RankerModel is a valid BinaryClassifier model.
  static RankerModelStatus ValidateModel(const RankerModel& model);

  base::WeakPtr<BinaryClassifierPredictor> AsWeakPtr() {
    return weak_ptr_factory_.GetWeakPtr();
  }

 protected:
  // Instatiates the inference module.
  bool Initialize() override;

 private:
  friend class BinaryClassifierPredictorTest;
  BinaryClassifierPredictor(const PredictorConfig& config);

  // TODO(hamelphi): Use an abstract BinaryClassifierInferenceModule in order to
  // generalize to other models.
  std::unique_ptr<GenericLogisticRegressionInference> inference_module_;

  base::WeakPtrFactory<BinaryClassifierPredictor> weak_ptr_factory_{this};
};

}  // namespace assist_ranker
#endif  // COMPONENTS_ASSIST_RANKER_BINARY_CLASSIFIER_PREDICTOR_H_
