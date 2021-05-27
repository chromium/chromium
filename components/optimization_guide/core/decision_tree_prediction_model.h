// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_OPTIMIZATION_GUIDE_CORE_DECISION_TREE_PREDICTION_MODEL_H_
#define COMPONENTS_OPTIMIZATION_GUIDE_CORE_DECISION_TREE_PREDICTION_MODEL_H_

#include <memory>
#include <string>

#include "base/containers/flat_map.h"
#include "base/macros.h"
#include "components/optimization_guide/core/prediction_model.h"
#include "components/optimization_guide/proto/models.pb.h"

namespace optimization_guide {

// A concrete PredictionModel capable of evaluating the decision tree model type
// supported by the optimization guide.
class DecisionTreePredictionModel : public PredictionModel {
 public:
  explicit DecisionTreePredictionModel(
      const proto::PredictionModel& prediction_model);

  ~DecisionTreePredictionModel() override;

  // PredictionModel implementation:
  OptimizationTargetDecision Predict(
      const base::flat_map<std::string, float>& model_features,
      double* prediction_score) override;

 private:
  // Evaluates the provided model, either an ensemble or decision tree model,
  // with the |model_features| and stores the output in |result|. Returns false
  // if evaluation fails.
  bool EvaluateModel(const proto::Model& model,
                     const base::flat_map<std::string, float>& model_features,
                     double* result);

  // Evaluates the decision tree model with the |model_features| and
  // stores the output in |result|. Returns false if the evaluation fails.
  bool EvaluateDecisionTree(
      const proto::DecisionTree& tree,
      const base::flat_map<std::string, float>& model_features,
      double* result);

  // Evaluates an ensemble model with the |model_features| and
  // stores the output in |result|. Returns false if the evaluation fails.
  bool EvaluateEnsembleModel(
      const proto::Ensemble& ensemble,
      const base::flat_map<std::string, float>& model_features,
      double* result);

  // Performs a depth first traversal the  |tree| based on |model_features|
  // and stores the value of the leaf in |result|. Returns false if the
  // traversal or node evaluation fails.
  bool TraverseTree(const proto::DecisionTree& tree,
                    const proto::TreeNode& node,
                    const base::flat_map<std::string, float>& model_features,
                    double* result);

  // PredictionModel implementation:
  bool ValidatePredictionModel() const override;

  // Validates a model or submodel of an ensemble. Returns
  // false if the model is invalid.
  bool ValidateModel(const proto::Model& model) const;

  // Validates an ensemble model. Returns false if the ensemble
  // if invalid.
  bool ValidateEnsembleModel(const proto::Ensemble& ensemble) const;

  // Validates a decision tree model. Returns false if the
  // decision tree model is invalid.
  bool ValidateDecisionTree(const proto::DecisionTree& tree) const;

  // Validates a leaf. Returns false if the leaf is invalid.
  bool ValidateLeaf(const proto::Leaf& leaf) const;

  // Validates an inequality test. Returns false if the
  // inequality test is invalid.
  bool ValidateInequalityTest(
      const proto::InequalityTest& inequality_test) const;

  // Validates each node of a decision tree by traversing every
  // node of the |tree|. Returns false if any part of the tree is invalid.
  bool ValidateTreeNode(const proto::DecisionTree& tree,
                        const proto::TreeNode& node,
                        int node_index) const;

  DISALLOW_COPY_AND_ASSIGN(DecisionTreePredictionModel);
};

}  // namespace optimization_guide

#endif  // COMPONENTS_OPTIMIZATION_GUIDE_CORE_DECISION_TREE_PREDICTION_MODEL_H_
