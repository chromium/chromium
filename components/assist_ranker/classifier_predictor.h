// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_ASSIST_RANKER_CLASSIFIER_PREDICTOR_H_
#define COMPONENTS_ASSIST_RANKER_CLASSIFIER_PREDICTOR_H_

#include <memory>
#include <vector>

#include "base/compiler_specific.h"
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
  static std::unique_ptr<ClassifierPredictor> Create(
      const PredictorConfig& config,
      const base::FilePath& model_path,
      scoped_refptr<network::SharedURLLoaderFactory> url_loader_factory)
      WARN_UNUSED_RESULT;

  // Performs inferencing on the specified RankerExample. The example is first
  // preprocessed using the model config. Returns false if a prediction could
  // not be made (e.g. the model is not loaded yet).
  bool Predict(RankerExample example,
               std::vector<float>* prediction) WARN_UNUSED_RESULT;

  // Performs inferencing on the specified feature vector. Returns false if
  // a prediction could not be made.
  bool Predict(const std::vector<float>& features,
               std::vector<float>* prediction) WARN_UNUSED_RESULT;

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
