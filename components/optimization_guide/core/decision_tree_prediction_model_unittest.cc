// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/optimization_guide/core/decision_tree_prediction_model.h"

#include <memory>
#include <utility>

#include "base/containers/flat_map.h"
#include "base/containers/flat_set.h"
#include "components/optimization_guide/core/prediction_model.h"
#include "components/optimization_guide/proto/models.pb.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace optimization_guide {

proto::PredictionModel GetValidDecisionTreePredictionModel() {
  proto::PredictionModel prediction_model;
  prediction_model.mutable_model()->mutable_threshold()->set_value(5.0);

  proto::DecisionTree decision_tree_model = proto::DecisionTree();
  decision_tree_model.set_weight(2.0);

  proto::TreeNode* tree_node = decision_tree_model.add_nodes();
  tree_node->mutable_node_id()->set_value(0);
  tree_node->mutable_binary_node()->mutable_left_child_id()->set_value(1);
  tree_node->mutable_binary_node()->mutable_right_child_id()->set_value(2);
  tree_node->mutable_binary_node()
      ->mutable_inequality_left_child_test()
      ->mutable_feature_id()
      ->mutable_id()
      ->set_value("agg1");
  tree_node->mutable_binary_node()
      ->mutable_inequality_left_child_test()
      ->set_type(proto::InequalityTest::LESS_OR_EQUAL);
  tree_node->mutable_binary_node()
      ->mutable_inequality_left_child_test()
      ->mutable_threshold()
      ->set_float_value(1.0);

  tree_node = decision_tree_model.add_nodes();
  tree_node->mutable_node_id()->set_value(1);
  tree_node->mutable_leaf()->mutable_vector()->add_value()->set_double_value(
      2.);

  tree_node = decision_tree_model.add_nodes();
  tree_node->mutable_node_id()->set_value(2);
  tree_node->mutable_leaf()->mutable_vector()->add_value()->set_double_value(
      4.);

  *prediction_model.mutable_model()->mutable_decision_tree() =
      decision_tree_model;
  return prediction_model;
}

proto::PredictionModel GetValidEnsemblePredictionModel() {
  proto::PredictionModel prediction_model;
  prediction_model.mutable_model()->mutable_threshold()->set_value(5.0);
  proto::Ensemble ensemble = proto::Ensemble();
  *ensemble.add_members()->mutable_submodel() =
      *GetValidDecisionTreePredictionModel().mutable_model();

  *ensemble.add_members()->mutable_submodel() =
      *GetValidDecisionTreePredictionModel().mutable_model();

  *prediction_model.mutable_model()->mutable_ensemble() = ensemble;
  return prediction_model;
}

TEST(DecisionTreePredictionModel, ValidDecisionTreeModel) {
  proto::PredictionModel prediction_model =
      GetValidDecisionTreePredictionModel();

  proto::ModelInfo* model_info = prediction_model.mutable_model_info();
  model_info->set_version(1);
  model_info->add_supported_model_types(
      proto::ModelType::MODEL_TYPE_DECISION_TREE);
  model_info->add_supported_host_model_features("agg1");

  std::unique_ptr<PredictionModel> model =
      PredictionModel::Create(prediction_model);
  EXPECT_TRUE(model);

  double prediction_score;
  EXPECT_EQ(OptimizationTargetDecision::kPageLoadDoesNotMatch,
            model->Predict({{"agg1", 1.0}}, &prediction_score));
  EXPECT_EQ(4., prediction_score);
  EXPECT_EQ(OptimizationTargetDecision::kPageLoadMatches,
            model->Predict({{"agg1", 2.0}}, &prediction_score));
  EXPECT_EQ(8., prediction_score);
}

TEST(DecisionTreePredictionModel, InequalityLessThan) {
  proto::PredictionModel prediction_model =
      GetValidDecisionTreePredictionModel();

  prediction_model.mutable_model()
      ->mutable_decision_tree()
      ->mutable_nodes(0)
      ->mutable_binary_node()
      ->mutable_inequality_left_child_test()
      ->set_type(proto::InequalityTest::LESS_THAN);

  proto::ModelInfo* model_info = prediction_model.mutable_model_info();
  model_info->set_version(1);
  model_info->add_supported_model_types(
      proto::ModelType::MODEL_TYPE_DECISION_TREE);
  model_info->add_supported_host_model_features("agg1");

  std::unique_ptr<PredictionModel> model =
      PredictionModel::Create(std::move(prediction_model));
  EXPECT_TRUE(model);

  double prediction_score;
  EXPECT_EQ(OptimizationTargetDecision::kPageLoadDoesNotMatch,
            model->Predict({{"agg1", 0.5}}, &prediction_score));
  EXPECT_EQ(4., prediction_score);
  EXPECT_EQ(OptimizationTargetDecision::kPageLoadMatches,
            model->Predict({{"agg1", 2.0}}, &prediction_score));
  EXPECT_EQ(8., prediction_score);
}

TEST(DecisionTreePredictionModel, InequalityGreaterOrEqual) {
  proto::PredictionModel prediction_model =
      GetValidDecisionTreePredictionModel();

  prediction_model.mutable_model()
      ->mutable_decision_tree()
      ->mutable_nodes(0)
      ->mutable_binary_node()
      ->mutable_inequality_left_child_test()
      ->set_type(proto::InequalityTest::GREATER_OR_EQUAL);

  proto::ModelInfo* model_info = prediction_model.mutable_model_info();
  model_info->set_version(1);
  model_info->add_supported_model_types(
      proto::ModelType::MODEL_TYPE_DECISION_TREE);
  model_info->add_supported_host_model_features("agg1");

  std::unique_ptr<PredictionModel> model =
      PredictionModel::Create(prediction_model);
  EXPECT_TRUE(model);

  double prediction_score;
  EXPECT_EQ(OptimizationTargetDecision::kPageLoadMatches,
            model->Predict({{"agg1", 0.5}}, &prediction_score));
  EXPECT_EQ(8., prediction_score);
  EXPECT_EQ(OptimizationTargetDecision::kPageLoadDoesNotMatch,
            model->Predict({{"agg1", 1.0}}, &prediction_score));
  EXPECT_EQ(4., prediction_score);
}

TEST(DecisionTreePredictionModel, InequalityGreaterThan) {
  proto::PredictionModel prediction_model =
      GetValidDecisionTreePredictionModel();

  prediction_model.mutable_model()
      ->mutable_decision_tree()
      ->mutable_nodes(0)
      ->mutable_binary_node()
      ->mutable_inequality_left_child_test()
      ->set_type(proto::InequalityTest::GREATER_THAN);

  proto::ModelInfo* model_info = prediction_model.mutable_model_info();
  model_info->set_version(1);
  model_info->add_supported_model_types(
      proto::ModelType::MODEL_TYPE_DECISION_TREE);
  model_info->add_supported_host_model_features("agg1");

  std::unique_ptr<PredictionModel> model =
      PredictionModel::Create(std::move(prediction_model));
  EXPECT_TRUE(model);

  double prediction_score;
  EXPECT_EQ(OptimizationTargetDecision::kPageLoadMatches,
            model->Predict({{"agg1", 0.5}}, &prediction_score));
  EXPECT_EQ(8., prediction_score);
  EXPECT_EQ(OptimizationTargetDecision::kPageLoadDoesNotMatch,
            model->Predict({{"agg1", 2.0}}, &prediction_score));
  EXPECT_EQ(4., prediction_score);
}

TEST(DecisionTreePredictionModel, MissingInequalityTest) {
  proto::PredictionModel prediction_model =
      GetValidDecisionTreePredictionModel();

  prediction_model.mutable_model()
      ->mutable_decision_tree()
      ->mutable_nodes(0)
      ->mutable_binary_node()
      ->mutable_inequality_left_child_test()
      ->Clear();

  proto::ModelInfo* model_info = prediction_model.mutable_model_info();
  model_info->set_version(1);
  model_info->add_supported_model_types(
      proto::ModelType::MODEL_TYPE_DECISION_TREE);
  model_info->add_supported_host_model_features("agg1");

  std::unique_ptr<PredictionModel> model =
      PredictionModel::Create(std::move(prediction_model));
  EXPECT_FALSE(model);
}

TEST(DecisionTreePredictionModel, NoDecisionTreeThreshold) {
  proto::PredictionModel prediction_model =
      GetValidDecisionTreePredictionModel();

  prediction_model.mutable_model()->clear_threshold();

  proto::ModelInfo* model_info = prediction_model.mutable_model_info();
  model_info->set_version(1);
  model_info->add_supported_model_types(
      proto::ModelType::MODEL_TYPE_DECISION_TREE);
  model_info->add_supported_host_model_features("agg1");

  std::unique_ptr<PredictionModel> model =
      PredictionModel::Create(prediction_model);
  EXPECT_FALSE(model);
}

TEST(DecisionTreePredictionModel, EmptyTree) {
  proto::PredictionModel prediction_model =
      GetValidDecisionTreePredictionModel();

  prediction_model.mutable_model()->mutable_decision_tree()->clear_nodes();

  proto::ModelInfo* model_info = prediction_model.mutable_model_info();
  model_info->set_version(1);
  model_info->add_supported_model_types(
      proto::ModelType::MODEL_TYPE_DECISION_TREE);
  model_info->add_supported_host_model_features("agg1");

  std::unique_ptr<PredictionModel> model =
      PredictionModel::Create(std::move(prediction_model));
  EXPECT_FALSE(model);
}

TEST(DecisionTreePredictionModel, ModelFeatureNotInFeatureMap) {
  proto::PredictionModel prediction_model =
      GetValidDecisionTreePredictionModel();

  prediction_model.mutable_model()->mutable_decision_tree()->clear_nodes();

  proto::ModelInfo* model_info = prediction_model.mutable_model_info();
  model_info->set_version(1);
  model_info->add_supported_model_types(
      proto::ModelType::MODEL_TYPE_DECISION_TREE);
  model_info->add_supported_host_model_features("agg1");

  std::unique_ptr<PredictionModel> model =
      PredictionModel::Create(prediction_model);
  EXPECT_FALSE(model);
}

TEST(DecisionTreePredictionModel, DecisionTreeMissingLeaf) {
  proto::PredictionModel prediction_model =
      GetValidDecisionTreePredictionModel();

  prediction_model.mutable_model()
      ->mutable_decision_tree()
      ->mutable_nodes(1)
      ->mutable_leaf()
      ->Clear();

  proto::ModelInfo* model_info = prediction_model.mutable_model_info();
  model_info->set_version(1);
  model_info->add_supported_model_types(
      proto::ModelType::MODEL_TYPE_DECISION_TREE);
  model_info->add_supported_host_model_features("agg1");

  std::unique_ptr<PredictionModel> model =
      PredictionModel::Create(prediction_model);
  EXPECT_FALSE(model);
}

TEST(DecisionTreePredictionModel, DecisionTreeLeftChildIndexInvalid) {
  proto::PredictionModel prediction_model =
      GetValidDecisionTreePredictionModel();

  prediction_model.mutable_model()
      ->mutable_decision_tree()
      ->mutable_nodes(0)
      ->mutable_binary_node()
      ->mutable_left_child_id()
      ->set_value(3);

  proto::ModelInfo* model_info = prediction_model.mutable_model_info();
  model_info->set_version(1);
  model_info->add_supported_model_types(
      proto::ModelType::MODEL_TYPE_DECISION_TREE);
  model_info->add_supported_host_model_features("agg1");

  std::unique_ptr<PredictionModel> model =
      PredictionModel::Create(std::move(prediction_model));
  EXPECT_FALSE(model);
}

TEST(DecisionTreePredictionModel, DecisionTreeRightChildIndexInvalid) {
  proto::PredictionModel prediction_model =
      GetValidDecisionTreePredictionModel();

  prediction_model.mutable_model()
      ->mutable_decision_tree()
      ->mutable_nodes(0)
      ->mutable_binary_node()
      ->mutable_right_child_id()
      ->set_value(3);

  proto::ModelInfo* model_info = prediction_model.mutable_model_info();
  model_info->set_version(1);
  model_info->add_supported_model_types(
      proto::ModelType::MODEL_TYPE_DECISION_TREE);
  model_info->add_supported_host_model_features("agg1");

  std::unique_ptr<PredictionModel> model =
      PredictionModel::Create(prediction_model);
  EXPECT_FALSE(model);
}

TEST(DecisionTreePredictionModel, DecisionTreeWithLoopOnLeftChild) {
  proto::PredictionModel prediction_model =
      GetValidDecisionTreePredictionModel();

  proto::TreeNode* tree_node =
      prediction_model.mutable_model()->mutable_decision_tree()->mutable_nodes(
          1);

  tree_node->mutable_node_id()->set_value(0);
  tree_node->mutable_binary_node()
      ->mutable_inequality_left_child_test()
      ->mutable_feature_id()
      ->mutable_id()
      ->set_value("agg1");
  tree_node->mutable_binary_node()
      ->mutable_inequality_left_child_test()
      ->set_type(proto::InequalityTest::LESS_OR_EQUAL);
  tree_node->mutable_binary_node()
      ->mutable_inequality_left_child_test()
      ->mutable_threshold()
      ->set_float_value(1.0);

  tree_node->mutable_binary_node()->mutable_left_child_id()->set_value(0);
  tree_node->mutable_binary_node()->mutable_right_child_id()->set_value(2);

  proto::ModelInfo* model_info = prediction_model.mutable_model_info();
  model_info->set_version(1);
  model_info->add_supported_model_types(
      proto::ModelType::MODEL_TYPE_DECISION_TREE);
  model_info->add_supported_host_model_features("agg1");

  std::unique_ptr<PredictionModel> model =
      PredictionModel::Create(prediction_model);
  EXPECT_FALSE(model);
}

TEST(DecisionTreePredictionModel, DecisionTreeWithLoopOnRightChild) {
  proto::PredictionModel prediction_model =
      GetValidDecisionTreePredictionModel();

  proto::TreeNode* tree_node =
      prediction_model.mutable_model()->mutable_decision_tree()->mutable_nodes(
          1);

  tree_node->mutable_node_id()->set_value(0);
  tree_node->mutable_binary_node()
      ->mutable_inequality_left_child_test()
      ->mutable_feature_id()
      ->mutable_id()
      ->set_value("agg1");
  tree_node->mutable_binary_node()
      ->mutable_inequality_left_child_test()
      ->set_type(proto::InequalityTest::LESS_OR_EQUAL);
  tree_node->mutable_binary_node()
      ->mutable_inequality_left_child_test()
      ->mutable_threshold()
      ->set_float_value(1.0);

  tree_node->mutable_binary_node()->mutable_left_child_id()->set_value(2);
  tree_node->mutable_binary_node()->mutable_right_child_id()->set_value(0);

  proto::ModelInfo* model_info = prediction_model.mutable_model_info();
  model_info->set_version(1);
  model_info->add_supported_model_types(
      proto::ModelType::MODEL_TYPE_DECISION_TREE);
  model_info->add_supported_host_model_features("agg1");

  std::unique_ptr<PredictionModel> model =
      PredictionModel::Create(prediction_model);
  EXPECT_FALSE(model);
}

TEST(DecisionTreePredictionModel, ValidEnsembleModel) {
  proto::PredictionModel prediction_model = GetValidEnsemblePredictionModel();

  proto::ModelInfo* model_info = prediction_model.mutable_model_info();
  model_info->set_version(1);
  model_info->add_supported_model_types(
      proto::ModelType::MODEL_TYPE_DECISION_TREE);
  model_info->add_supported_host_model_features("agg1");

  std::unique_ptr<PredictionModel> model =
      PredictionModel::Create(prediction_model);
  EXPECT_TRUE(model);

  double prediction_score;
  EXPECT_EQ(OptimizationTargetDecision::kPageLoadDoesNotMatch,
            model->Predict({{"agg1", 1.0}}, &prediction_score));
  EXPECT_EQ(4., prediction_score);
  EXPECT_EQ(OptimizationTargetDecision::kPageLoadMatches,
            model->Predict({{"agg1", 2.0}}, &prediction_score));
  EXPECT_EQ(8., prediction_score);
}

TEST(DecisionTreePredictionModel, EnsembleWithNoMembers) {
  proto::PredictionModel prediction_model = GetValidEnsemblePredictionModel();
  prediction_model.mutable_model()
      ->mutable_ensemble()
      ->mutable_members()
      ->Clear();

  proto::ModelInfo* model_info = prediction_model.mutable_model_info();
  model_info->set_version(1);
  model_info->add_supported_model_types(
      proto::ModelType::MODEL_TYPE_DECISION_TREE);
  model_info->add_supported_host_model_features("agg1");

  std::unique_ptr<PredictionModel> model =
      PredictionModel::Create(prediction_model);
  EXPECT_FALSE(model);
}

}  // namespace optimization_guide
