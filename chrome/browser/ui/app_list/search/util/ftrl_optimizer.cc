// Copyright 2022 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/app_list/search/util/ftrl_optimizer.h"

#include <cmath>

#include "base/files/file_path.h"

namespace app_list {
namespace {

// A version number to be incremented each time a code change invalidates the
// state stored on-disk in |proto_|.
constexpr int kVersion = 1;

double Total(const google::protobuf::RepeatedField<double>& values) {
  double total = 0.0;
  for (double v : values)
    total += v;
  return total;
}

// Normalize |values| to sum to 1 in-place.
void Normalize(google::protobuf::RepeatedField<double>& values) {
  double total = Total(values);
  if (total == 0.0)
    return;
  for (size_t i = 0; i < values.size(); ++i)
    values[i] = values[i] / total;
}

}  // namespace

FtrlOptimizer::FtrlOptimizer(const base::FilePath& path,
                             const Params& params,
                             std::vector<std::unique_ptr<FtrlExpert>>&& experts)
    : params_(params),
      experts_(std::move(experts)),
      proto_(path, params.write_delay) {
  DCHECK_GT(params.alpha, 0.0);
  DCHECK_GE(params.gamma, 0.0);
  DCHECK_LE(params.gamma, 1.0);

  proto_.RegisterOnRead(
      base::BindOnce(&FtrlOptimizer::OnProtoRead, weak_factory_.GetWeakPtr()));
  proto_.Init();
}

FtrlOptimizer::~FtrlOptimizer() {}

std::vector<double> FtrlOptimizer::Score(
    const std::vector<std::string>& items) {
  size_t num_items = items.size();
  size_t num_experts = experts_.size();

  std::vector<double> result(num_items, 0.0);
  if (!proto_.initialized())
    return result;

  const auto& weights = proto_->weights();
  DCHECK_EQ(proto_->weights_size(), num_experts);
  last_expert_scores_.clear();
  for (size_t i = 0; i < num_experts; ++i) {
    const auto& scores = experts_[i]->Score(items);
    DCHECK_EQ(scores.size(), num_items);
    for (size_t j = 0; j < num_items; ++j)
      result[j] += weights[i] * scores[j];
    last_expert_scores_.push_back(scores);
  }

  last_items_ = items;

  return result;
}

void FtrlOptimizer::Train(const std::string& item) {
  // Train each constituent expert.
  for (auto& expert : experts_)
    expert->Train(item);

  // If |last_items_| is empty, experts had no chance at prediction and we
  // should early exit. This could happen if |proto_| finishes initializing
  // after Score but before Train.
  if (!proto_.initialized() || last_items_.empty())
    return;

  // Compute the loss of each expert and update weights.
  auto& weights = *proto_->mutable_weights();
  for (size_t i = 0; i < weights.size(); ++i) {
    double loss = Loss(i, item);
    double fixed_share = params_.gamma / weights.size();
    double weight_factor = (1.0 - params_.gamma) * exp(-params_.alpha * loss);
    weights[i] = fixed_share + weight_factor * weights[i];
  }

  // Re-normalize the weights.
  Normalize(weights);
  DCHECK_LE(std::abs(Total(proto_->weights()) - 1.0), 1.0e-5);

  proto_.StartWrite();
}

double FtrlOptimizer::Loss(size_t expert, const std::string& item) {
  DCHECK(!last_items_.empty());
  DCHECK_LT(expert, last_expert_scores_.size());

  // Find the score of the launched item.
  auto& scores = last_expert_scores_[expert];
  double score = 0.0;
  DCHECK_EQ(scores.size(), last_items_.size());
  for (int i = 0; i < scores.size(); ++i) {
    if (last_items_[i] == item)
      score = scores[i];
  }

  // Find the rank of the item, ie. the number of items with higher score.
  size_t rank = 0;
  for (int i = 0; i < scores.size(); ++i) {
    if (scores[i] > score)
      ++rank;
  }

  // The loss is linear in the |rank|. A loss of 1.0 means |item| wasn't
  // included at all.
  DCHECK(!scores.empty());
  return static_cast<double>(rank) / scores.size();
}

void FtrlOptimizer::OnProtoRead(ReadStatus status) {
  if (!proto_->has_version() || proto_->version() != kVersion ||
      experts_.size() != proto_->weights_size()) {
    proto_.Purge();
    proto_->set_version(kVersion);
    for (size_t i = 0; i < experts_.size(); ++i)
      proto_->add_weights(1.0 / experts_.size());
  }
  DCHECK_LE(std::abs(Total(proto_->weights()) - 1.0), 1.0e-5);
}

}  // namespace app_list
