// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_ASSIST_RANKER_CLASSIFIER_PREDICTOR_H_
#define COMPONENTS_ASSIST_RANKER_CLASSIFIER_PREDICTOR_H_

#include <memory>
#include <vector>

#include "components/assist_ranker/base_predictor.h"
#include "components/assist_ranker/nn_classifier.h"
#include "components/assist_ranker/proto/ranker_example.pb.h"

namespace base {
class FilePath;
}

namespace network {
class SharedURLLoaderFactory;
}

namespace assist_ranker {

// Predictor class for single-layer neural network models.
class ClassifierPredictor : public BasePredictor {
 public:
  ClassifierPredictor(const ClassifierPredictor&) = delete;
  ClassifierPredictor& operator=(const ClassifierPredictor&) = delete;

  ~ClassifierPredictor() override;

  // Returns an new predictor instance with the given |config| and initialize
  // its model loader. The |request_context getter| is passed to the
  // predictor's model_loader which holds it as scoped_refptr.
  [[nodiscard]] static std::unique_ptr<ClassifierPredictor> Create(
      const PredictorConfig& config,
      const base::FilePath& model_path,
      scoped_refptr<network::SharedURLLoaderFactory> url_loader_factory);

  // Performs inferencing on the specified RankerExample. The example is first
  // preprocessed using the model config. Returns false if a prediction could
  // not be made (e.g. the model is not loaded yet).
  [[nodiscard]] bool Predict(RankerExample example,
                             std::vector<float>* prediction);

  // Performs inferencing on the specified feature vector. Returns false if
  // a prediction could not be made.
  [[nodiscard]] bool Predict(const std::vector<float>& features,
                             std::vector<float>* prediction);

  // Validates that the loaded RankerModel is a valid BinaryClassifier model.
  static RankerModelStatus ValidateModel(const RankerModel& model);

 protected:
  // Instantiates the inference module.
  bool Initialize() override;

 private:
  friend class ClassifierPredictorTest;
  ClassifierPredictor(const PredictorConfig& config);

  NNClassifierModel model_;
};

}  // namespace assist_ranker
#endif  // COMPONENTS_ASSIST_RANKER_CLASSIFIER_PREDICTOR_H_
