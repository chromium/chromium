// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/optimization_guide/core/decision_tree_prediction_model.h"

#include <utility>

namespace optimization_guide {

DecisionTreePredictionModel::DecisionTreePredictionModel(
    const proto::PredictionModel& prediction_model)
    : PredictionModel(prediction_model) {}

DecisionTreePredictionModel::~DecisionTreePredictionModel() = default;

bool DecisionTreePredictionModel::ValidatePredictionModel() const {
  // Only the top-level ensemble or decision tree must have a threshold. Any
  // submodels of an ensemble will have model weights but no threshold.
  if (!model_.has_threshold())
    return false;
  return ValidateModel(model_);
}

bool DecisionTreePredictionModel::ValidateModel(
    const proto::Model& model) const {
  if (model.has_ensemble()) {
    return ValidateEnsembleModel(model.ensemble());
  }
  if (model.has_decision_tree()) {
    return ValidateDecisionTree(model.decision_tree());
  }
  return false;
}

bool DecisionTreePredictionModel::ValidateEnsembleModel(
    const proto::Ensemble& ensemble) const {
  if (ensemble.members_size() == 0)
    return false;

  for (const auto& member : ensemble.members()) {
    if (!ValidateModel(member.submodel())) {
      return false;
    }
  }
  return true;
}

bool DecisionTreePredictionModel::ValidateDecisionTree(
    const proto::DecisionTree& tree) const {
  if (tree.nodes_size() == 0)
    return false;
  return ValidateTreeNode(tree, tree.nodes(0), 0);
}

bool DecisionTreePredictionModel::ValidateLeaf(const proto::Leaf& leaf) const {
  return leaf.has_vector() && leaf.vector().value_size() == 1 &&
         leaf.vector().value(0).has_double_value();
}

bool DecisionTreePredictionModel::ValidateInequalityTest(
    const proto::InequalityTest& inequality_test) const {
  if (!inequality_test.has_threshold())
    return false;
  if (!inequality_test.threshold().has_float_value())
    return false;
  if (!inequality_test.has_feature_id())
    return false;
  if (!inequality_test.feature_id().has_id())
    return false;
  if (!inequality_test.has_type())
    return false;
  return true;
}

bool DecisionTreePredictionModel::ValidateTreeNode(
    const proto::DecisionTree& tree,
    const proto::TreeNode& node,
    int node_index) const {
  if (node.has_leaf())
    return ValidateLeaf(node.leaf());

  if (!node.has_binary_node())
    return false;

  proto::BinaryNode binary_node = node.binary_node();
  if (!binary_node.has_inequality_left_child_test())
    return false;

  if (!ValidateInequalityTest(binary_node.inequality_left_child_test()))
    return false;

  if (!binary_node.left_child_id().has_value())
    return false;
  if (!binary_node.right_child_id().has_value())
    return false;

  if (binary_node.left_child_id().value() >= tree.nodes_size())
    return false;
  if (binary_node.right_child_id().value() >= tree.nodes_size())
    return false;

  // Assure that no parent has an child index less than itself in order to
  // prevent loops.
  if (node_index >= binary_node.left_child_id().value())
    return false;
  if (node_index >= binary_node.right_child_id().value())
    return false;

  if (!ValidateTreeNode(tree, tree.nodes(binary_node.left_child_id().value()),
                        binary_node.left_child_id().value())) {
    return false;
  }
  if (!ValidateTreeNode(tree, tree.nodes(binary_node.right_child_id().value()),
                        binary_node.right_child_id().value())) {
    return false;
  }
  return true;
}

OptimizationTargetDecision DecisionTreePredictionModel::Predict(
    const base::flat_map<std::string, float>& model_features,
    double* prediction_score) {
  *prediction_score = 0.0;
  // TODO(mcrouse): Add metrics to record if the model evaluation fails.
  if (!EvaluateModel(model_, model_features, prediction_score))
    return OptimizationTargetDecision::kUnknown;
  if (*prediction_score > model_.threshold().value())
    return OptimizationTargetDecision::kPageLoadMatches;
  return OptimizationTargetDecision::kPageLoadDoesNotMatch;
}

bool DecisionTreePredictionModel::TraverseTree(
    const proto::DecisionTree& tree,
    const proto::TreeNode& node,
    const base::flat_map<std::string, float>& model_features,
    double* result) {
  if (node.has_leaf()) {
    *result = node.leaf().vector().value(0).double_value();
    return true;
  }

  proto::BinaryNode binary_node = node.binary_node();
  float threshold =
      binary_node.inequality_left_child_test().threshold().float_value();
  std::string feature_name =
      binary_node.inequality_left_child_test().feature_id().id().value();
  auto it = model_features.find(feature_name);
  if (it == model_features.end())
    return false;
  switch (binary_node.inequality_left_child_test().type()) {
    case proto::InequalityTest::LESS_OR_EQUAL:
      if (it->second <= threshold)
        return TraverseTree(tree,
                            tree.nodes(binary_node.left_child_id().value()),
                            model_features, result);
      return TraverseTree(tree,
                          tree.nodes(binary_node.right_child_id().value()),
                          model_features, result);
    case proto::InequalityTest::LESS_THAN:
      if (it->second < threshold)
        return TraverseTree(tree,
                            tree.nodes(binary_node.left_child_id().value()),
                            model_features, result);
      return TraverseTree(tree,
                          tree.nodes(binary_node.right_child_id().value()),
                          model_features, result);
    case proto::InequalityTest::GREATER_OR_EQUAL:
      if (it->second >= threshold)
        return TraverseTree(tree,
                            tree.nodes(binary_node.left_child_id().value()),
                            model_features, result);
      return TraverseTree(tree,
                          tree.nodes(binary_node.right_child_id().value()),
                          model_features, result);
    case proto::InequalityTest::GREATER_THAN:
      if (it->second > threshold)
        return TraverseTree(tree,
                            tree.nodes(binary_node.left_child_id().value()),
                            model_features, result);
      return TraverseTree(tree,
                          tree.nodes(binary_node.right_child_id().value()),
                          model_features, result);
    default:
      return false;
  }
}

bool DecisionTreePredictionModel::EvaluateDecisionTree(
    const proto::DecisionTree& tree,
    const base::flat_map<std::string, float>& model_features,
    double* result) {
  if (TraverseTree(tree, tree.nodes(0), model_features, result)) {
    *result *= tree.weight();
    return true;
  }
  return false;
}

bool DecisionTreePredictionModel::EvaluateEnsembleModel(
    const proto::Ensemble& ensemble,
    const base::flat_map<std::string, float>& model_features,
    double* result) {
  if (ensemble.members_size() == 0)
    return false;

  double score = 0.0;
  for (const auto& member : ensemble.members()) {
    if (!EvaluateModel(member.submodel(), model_features, &score)) {
      *result = 0.0;
      return false;
    }

    *result += score;
  }
  *result = *result / ensemble.members_size();
  return true;
}

bool DecisionTreePredictionModel::EvaluateModel(
    const proto::Model& model,
    const base::flat_map<std::string, float>& model_features,
    double* result) {
  DCHECK(result);
  // Clear the result value.
  *result = 0.0;

  if (model.has_ensemble()) {
    return EvaluateEnsembleModel(model.ensemble(), model_features, result);
  }
  if (model.has_decision_tree()) {
    return EvaluateDecisionTree(model.decision_tree(), model_features, result);
  }
  return false;
}

}  // namespace optimization_guide
