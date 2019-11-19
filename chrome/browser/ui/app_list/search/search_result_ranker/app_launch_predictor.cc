// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/app_list/search/search_result_ranker/app_launch_predictor.h"

#include <cmath>

#include "ash/public/cpp/app_list/app_list_features.h"
#include "base/logging.h"
#include "base/metrics/field_trial_params.h"
#include "base/stl_util.h"

namespace app_list {
namespace {

constexpr int kHoursADay = 24;
constexpr base::TimeDelta kSaveInternal = base::TimeDelta::FromHours(1);

// A bin with index i has 5 adjacent bins as: i + 0, i + 1, i + 2, i + 22, and
// i + 23 which stand for the bin i itself, 1 hour later, 2 hours later,
// 2 hours earlier and 1 hour earlier. Each adjacent bin contributes to the
// final Rank score with weights from BinWeightsFromFlagOrDefault();
constexpr int kAdjacentHourBin[] = {0, 1, 2, 22, 23};

}  // namespace

MrfuAppLaunchPredictor::MrfuAppLaunchPredictor() = default;
MrfuAppLaunchPredictor::~MrfuAppLaunchPredictor() = default;

void MrfuAppLaunchPredictor::Train(const std::string& app_id) {
  // Updates the score for this |app_id|.
  ++num_of_trains_;
  Score& score = scores_[app_id];
  UpdateScore(&score);
  score.last_score += 1.0f - decay_coeff_;
}

base::flat_map<std::string, float> MrfuAppLaunchPredictor::Rank() {
  // Updates all scores and return app_id to score map.
  base::flat_map<std::string, float> output;
  for (auto& pair : scores_) {
    UpdateScore(&pair.second);
    output[pair.first] = pair.second.last_score;
  }
  return output;
}

const char MrfuAppLaunchPredictor::kPredictorName[] = "MrfuAppLaunchPredictor";
const char* MrfuAppLaunchPredictor::GetPredictorName() const {
  return kPredictorName;
}

bool MrfuAppLaunchPredictor::ShouldSave() {
  // MrfuAppLaunchPredictor doesn't need materialization.
  return false;
}

AppLaunchPredictorProto MrfuAppLaunchPredictor::ToProto() const {
  // MrfuAppLaunchPredictor doesn't need materialization.
  NOTREACHED();
  return AppLaunchPredictorProto();
}

bool MrfuAppLaunchPredictor::FromProto(const AppLaunchPredictorProto& proto) {
  // MrfuAppLaunchPredictor doesn't need materialization.
  NOTREACHED();
  return false;
}

void MrfuAppLaunchPredictor::UpdateScore(Score* score) {
  // Updates last_score and num_of_trains_at_last_update.
  const int trains_since_last_time =
      num_of_trains_ - score->num_of_trains_at_last_update;
  if (trains_since_last_time > 0) {
    score->last_score *= std::pow(decay_coeff_, trains_since_last_time);
    score->num_of_trains_at_last_update = num_of_trains_;
  }
}

SerializedMrfuAppLaunchPredictor::SerializedMrfuAppLaunchPredictor()
    : MrfuAppLaunchPredictor(), last_save_timestamp_(base::Time::Now()) {}

SerializedMrfuAppLaunchPredictor::~SerializedMrfuAppLaunchPredictor() = default;

const char SerializedMrfuAppLaunchPredictor::kPredictorName[] =
    "SerializedMrfuAppLaunchPredictor";
const char* SerializedMrfuAppLaunchPredictor::GetPredictorName() const {
  return kPredictorName;
}

bool SerializedMrfuAppLaunchPredictor::ShouldSave() {
  const base::Time now = base::Time::Now();
  if (now - last_save_timestamp_ >= kSaveInternal) {
    last_save_timestamp_ = now;
    return true;
  }
  return false;
}

AppLaunchPredictorProto SerializedMrfuAppLaunchPredictor::ToProto() const {
  AppLaunchPredictorProto output;
  auto& predictor_proto =
      *output.mutable_serialized_mrfu_app_launch_predictor();
  predictor_proto.set_num_of_trains(num_of_trains_);
  for (const auto& pair : scores_) {
    auto& score_item = (*predictor_proto.mutable_scores())[pair.first];
    score_item.set_last_score(pair.second.last_score);
    score_item.set_num_of_trains_at_last_update(
        pair.second.num_of_trains_at_last_update);
  }
  return output;
}

bool SerializedMrfuAppLaunchPredictor::FromProto(
    const AppLaunchPredictorProto& proto) {
  if (proto.predictor_case() !=
      AppLaunchPredictorProto::kSerializedMrfuAppLaunchPredictor) {
    return false;
  }

  const auto& predictor_proto = proto.serialized_mrfu_app_launch_predictor();
  num_of_trains_ = predictor_proto.num_of_trains();

  scores_.clear();
  for (const auto& pair : predictor_proto.scores()) {
    // Skip the case where the last_score has already dropped to 0.0f.
    if (pair.second.last_score() == 0.0f)
      continue;
    auto& score_item = scores_[pair.first];
    score_item.last_score = pair.second.last_score();
    score_item.num_of_trains_at_last_update =
        pair.second.num_of_trains_at_last_update();
  }

  return true;
}

HourAppLaunchPredictor::HourAppLaunchPredictor()
    : last_save_timestamp_(base::Time::Now()),
      bin_weights_(BinWeightsFromFlagOrDefault()) {}

HourAppLaunchPredictor::~HourAppLaunchPredictor() = default;

void HourAppLaunchPredictor::Train(const std::string& app_id) {
  auto& frequency_table = (*proto_.mutable_hour_app_launch_predictor()
                                ->mutable_binned_frequency_table())[GetBin()];

  frequency_table.set_total_counts(frequency_table.total_counts() + 1);
  (*frequency_table.mutable_frequency())[app_id] += 1;
}

base::flat_map<std::string, float> HourAppLaunchPredictor::Rank() {
  base::flat_map<std::string, float> output;
  const int bin = GetBin();
  const bool is_weekend = bin >= kHoursADay;
  const int hour = bin % kHoursADay;
  const auto& frequency_table_map =
      proto_.hour_app_launch_predictor().binned_frequency_table();

  for (size_t i = 0; i < base::size(kAdjacentHourBin); ++i) {
    // Finds adjacent bin and weight.
    const int adj_bin =
        (hour + kAdjacentHourBin[i]) % kHoursADay + kHoursADay * is_weekend;
    const auto find_frequency_table = frequency_table_map.find(adj_bin);
    if (find_frequency_table == frequency_table_map.end())
      continue;

    const auto& frequency_table = find_frequency_table->second;
    const float weight = bin_weights_[i];

    // Accumulates the frequency to the output.
    if (frequency_table.total_counts() > 0) {
      const int total_counts = frequency_table.total_counts();
      for (const auto& pair : frequency_table.frequency()) {
        output[pair.first] +=
            static_cast<float>(pair.second) / total_counts * weight;
      }
    }
  }
  return output;
}

const char HourAppLaunchPredictor::kPredictorName[] = "HourAppLaunchPredictor";
const char* HourAppLaunchPredictor::GetPredictorName() const {
  return kPredictorName;
}

bool HourAppLaunchPredictor::ShouldSave() {
  const base::Time now = base::Time::Now();
  if (now - last_save_timestamp_ >= kSaveInternal) {
    last_save_timestamp_ = now;
    return true;
  }
  return false;
}

AppLaunchPredictorProto HourAppLaunchPredictor::ToProto() const {
  return proto_;
}

bool HourAppLaunchPredictor::FromProto(const AppLaunchPredictorProto& proto) {
  if (proto.predictor_case() !=
      AppLaunchPredictorProto::kHourAppLaunchPredictor) {
    return false;
  }

  const HourAppLaunchPredictorProto& predictor_proto =
      proto.hour_app_launch_predictor();

  const int today = base::Time::Now().ToDeltaSinceWindowsEpoch().InDays();

  // If last_decay_timestamp is not set, just copy the proto.
  if (!predictor_proto.has_last_decay_timestamp()) {
    proto_ = proto;
    proto_.mutable_hour_app_launch_predictor()->set_last_decay_timestamp(today);
    return true;
  }

  // If last decay is within 7 days, just copy the proto.
  if (today - predictor_proto.last_decay_timestamp() <= 7) {
    proto_ = proto;
    return true;
  }

  proto_.Clear();
  for (const auto& table : predictor_proto.binned_frequency_table()) {
    auto& new_table = (*proto_.mutable_hour_app_launch_predictor()
                            ->mutable_binned_frequency_table())[table.first];

    int total_counts = 0;
    for (const auto& frequency : table.second.frequency()) {
      const int new_frequency = frequency.second * kWeeklyDecayCoeff;
      if (new_frequency > 0) {
        total_counts += new_frequency;
        (*new_table.mutable_frequency())[frequency.first] = new_frequency;
      }
    }
    new_table.set_total_counts(total_counts);
  }
  proto_.mutable_hour_app_launch_predictor()->set_last_decay_timestamp(today);
  return true;
}

int HourAppLaunchPredictor::GetBin() const {
  base::Time::Exploded now;
  base::Time::Now().LocalExplode(&now);

  const bool is_weekend = now.day_of_week == 6 || now.day_of_week == 0;

  // To distinguish workdays with weekends, we use now.hour for workdays, and
  // now.hour + 24 for weekends.
  if (!is_weekend) {
    return now.hour;
  } else {
    return now.hour + kHoursADay;
  }
}

std::vector<float> HourAppLaunchPredictor::BinWeightsFromFlagOrDefault() {
  const std::vector<float> default_weights = {0.6, 0.15, 0.05, 0.05, 0.15};
  std::vector<float> weights(5);

  // Get weights for adjacent bins. Every weight has to be within [0.0, 1.0]
  // And the sum weights[1] + ..., + weights[4] also needs to be in [0.0, 1.0]
  // so that the weight[0] is set to be 1.0 - (weights[1] + ..., + weights[4]).
  weights[1] = static_cast<float>(base::GetFieldTrialParamByFeatureAsDouble(
      app_list_features::kEnableZeroStateAppsRanker, "weight_1_hour_later_bin",
      -1.0));
  if (weights[1] < 0.0 || weights[1] > 1.0)
    return default_weights;

  weights[2] = static_cast<float>(base::GetFieldTrialParamByFeatureAsDouble(
      app_list_features::kEnableZeroStateAppsRanker, "weight_2_hour_later_bin",
      -1.0));
  if (weights[2] < 0.0 || weights[2] > 1.0)
    return default_weights;

  weights[3] = static_cast<float>(base::GetFieldTrialParamByFeatureAsDouble(
      app_list_features::kEnableZeroStateAppsRanker,
      "weight_2_hour_earlier_bin", -1.0));
  if (weights[3] < 0.0 || weights[3] > 1.0)
    return default_weights;

  weights[4] = static_cast<float>(base::GetFieldTrialParamByFeatureAsDouble(
      app_list_features::kEnableZeroStateAppsRanker,
      "weight_1_hour_earlier_bin", -1.0));
  if (weights[4] < 0.0 || weights[4] > 1.0)
    return default_weights;

  weights[0] = 1.0 - weights[1] - weights[2] - weights[3] - weights[4];
  if (weights[0] < 0.0 || weights[0] > 1.0)
    return default_weights;

  return weights;
}

void FakeAppLaunchPredictor::SetShouldSave(bool should_save) {
  should_save_ = should_save;
}

void FakeAppLaunchPredictor::Train(const std::string& app_id) {
  // Increases 1.0 for rank score of app_id.
  (*proto_.mutable_fake_app_launch_predictor()
        ->mutable_rank_result())[app_id] += 1.0f;
}

base::flat_map<std::string, float> FakeAppLaunchPredictor::Rank() {
  // Outputs proto_.fake_app_launch_predictor().rank_result() as Rank result.
  base::flat_map<std::string, float> output;
  for (const auto& pair : proto_.fake_app_launch_predictor().rank_result()) {
    output[pair.first] = pair.second;
  }
  return output;
}

const char FakeAppLaunchPredictor::kPredictorName[] = "FakeAppLaunchPredictor";
const char* FakeAppLaunchPredictor::GetPredictorName() const {
  return kPredictorName;
}

bool FakeAppLaunchPredictor::ShouldSave() {
  return should_save_;
}

AppLaunchPredictorProto FakeAppLaunchPredictor::ToProto() const {
  return proto_;
}

bool FakeAppLaunchPredictor::FromProto(const AppLaunchPredictorProto& proto) {
  if (proto.predictor_case() !=
      AppLaunchPredictorProto::kFakeAppLaunchPredictor) {
    return false;
  }
  proto_ = proto;
  return true;
}

}  // namespace app_list
