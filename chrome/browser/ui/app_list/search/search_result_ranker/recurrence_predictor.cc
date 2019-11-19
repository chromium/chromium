// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/app_list/search/search_result_ranker/recurrence_predictor.h"

#include <cmath>
#include <utility>

#include "base/time/time.h"
#include "chrome/browser/ui/app_list/search/search_result_ranker/frecency_store.pb.h"
#include "chrome/browser/ui/app_list/search/search_result_ranker/histogram_util.h"
#include "chrome/browser/ui/app_list/search/search_result_ranker/recurrence_predictor.pb.h"
#include "chrome/browser/ui/app_list/search/search_result_ranker/recurrence_ranker_util.h"

namespace app_list {
namespace {

constexpr int kHoursADay = 24;

}  // namespace

RecurrencePredictor::RecurrencePredictor(const std::string& model_identifier)
    : model_identifier_(model_identifier) {}

FakePredictor::FakePredictor(const std::string& model_identifier)
    : RecurrencePredictor(model_identifier) {
  // The fake predictor should only be used for testing, not in production.
  // Record an error so we know if it is being used.
  LogInitializationStatus(model_identifier_,
                          InitializationStatus::kFakePredictorUsed);
}

FakePredictor::FakePredictor(const FakePredictorConfig& config,
                             const std::string& model_identifier)
    : RecurrencePredictor(model_identifier) {
  // The fake predictor should only be used for testing, not in production.
  // Record an error so we know if it is being used.
  LogInitializationStatus(model_identifier_,
                          InitializationStatus::kFakePredictorUsed);
}

FakePredictor::~FakePredictor() = default;

const char FakePredictor::kPredictorName[] = "FakePredictor";
const char* FakePredictor::GetPredictorName() const {
  return kPredictorName;
}

void FakePredictor::Train(unsigned int target, unsigned int condition) {
  counts_[target] += 1.0f;
}

std::map<unsigned int, float> FakePredictor::Rank(unsigned int condition) {
  return counts_;
}

void FakePredictor::Cleanup(const std::vector<unsigned int>& valid_targets) {
  std::map<unsigned int, float> new_counts;

  for (unsigned int id : valid_targets) {
    const auto& it = counts_.find(id);
    if (it != counts_.end())
      new_counts[id] = it->second;
  }

  counts_.swap(new_counts);
}

void FakePredictor::ToProto(RecurrencePredictorProto* proto) const {
  auto* counts = proto->mutable_fake_predictor()->mutable_counts();
  for (auto& pair : counts_)
    (*counts)[pair.first] = pair.second;
}

void FakePredictor::FromProto(const RecurrencePredictorProto& proto) {
  if (!proto.has_fake_predictor()) {
    LogSerializationStatus(model_identifier_,
                           SerializationStatus::kFakePredictorLoadingError);
    return;
  }

  for (const auto& pair : proto.fake_predictor().counts())
    counts_[pair.first] = pair.second;
}

DefaultPredictor::DefaultPredictor(const DefaultPredictorConfig& config,
                                   const std::string& model_identifier)
    : RecurrencePredictor(model_identifier) {}
DefaultPredictor::~DefaultPredictor() {}

void DefaultPredictor::Train(unsigned int target, unsigned int condition) {}

std::map<unsigned int, float> DefaultPredictor::Rank(unsigned int condition) {
  NOTREACHED();
  return {};
}

const char DefaultPredictor::kPredictorName[] = "DefaultPredictor";
const char* DefaultPredictor::GetPredictorName() const {
  return kPredictorName;
}

void DefaultPredictor::ToProto(RecurrencePredictorProto* proto) const {}

void DefaultPredictor::FromProto(const RecurrencePredictorProto& proto) {}

ConditionalFrequencyPredictor::ConditionalFrequencyPredictor(
    const std::string& model_identifier)
    : RecurrencePredictor(model_identifier) {}
ConditionalFrequencyPredictor::ConditionalFrequencyPredictor(
    const ConditionalFrequencyPredictorConfig& config,
    const std::string& model_identifier)
    : RecurrencePredictor(model_identifier) {}
ConditionalFrequencyPredictor::~ConditionalFrequencyPredictor() = default;

ConditionalFrequencyPredictor::Events::Events() = default;
ConditionalFrequencyPredictor::Events::~Events() = default;
ConditionalFrequencyPredictor::Events::Events(const Events& other) = default;

const char ConditionalFrequencyPredictor::kPredictorName[] =
    "ConditionalFrequencyPredictor";
const char* ConditionalFrequencyPredictor::GetPredictorName() const {
  return kPredictorName;
}

void ConditionalFrequencyPredictor::Train(unsigned int target,
                                          unsigned int condition) {
  TrainWithDelta(target, condition, 1.0f);
}

void ConditionalFrequencyPredictor::TrainWithDelta(unsigned int target,
                                                   unsigned int condition,
                                                   float delta) {
  DCHECK_NE(delta, 0.0f);
  auto& events = table_[condition];
  events.freqs[target] += delta;
  events.total += delta;
}

std::map<unsigned int, float> ConditionalFrequencyPredictor::Rank(
    unsigned int condition) {
  const auto& it = table_.find(condition);
  // If the total frequency is zero, we can't return any meaningful results, so
  // return empty.
  if (it == table_.end() || it->second.total == 0.0f)
    return {};

  std::map<unsigned int, float> result;
  const auto& events = it->second;
  for (const auto& target_freq : events.freqs)
    result[target_freq.first] = target_freq.second / events.total;
  return result;
}

void ConditionalFrequencyPredictor::Cleanup(
    const std::vector<unsigned int>& valid_targets) {
  for (auto iter = table_.begin(); iter != table_.end();) {
    auto& events = iter->second;

    std::map<unsigned int, float> new_freqs;
    float new_total = 0.0f;
    for (unsigned int id : valid_targets) {
      const auto& it = events.freqs.find(id);
      if (it != events.freqs.end()) {
        new_freqs[id] = it->second;
        new_total += it->second;
      }
    }

    // Delete the whole condition out of the table if it contains no valid
    // targets.
    if (new_freqs.empty()) {
      // C++11: the return value of erase(iter) is an iterator pointing to the
      // next element in the container.
      iter = table_.erase(iter);
    } else {
      ++iter;
      events.freqs.swap(new_freqs);
      events.total = new_total;
    }
  }
}

void ConditionalFrequencyPredictor::CleanupConditions(
    const std::vector<unsigned int>& valid_conditions) {
  std::map<unsigned int, ConditionalFrequencyPredictor::Events> new_table;

  for (unsigned int id : valid_conditions) {
    const auto& it = table_.find(id);
    if (it != table_.end()) {
      new_table[id] = std::move(it->second);
    }
  }

  table_.swap(new_table);
}

void ConditionalFrequencyPredictor::ToProto(
    RecurrencePredictorProto* proto) const {
  auto* predictor = proto->mutable_conditional_frequency_predictor();
  for (const auto& condition_events : table_) {
    for (const auto& event_freq : condition_events.second.freqs) {
      auto* event = predictor->add_events();
      event->set_condition(condition_events.first);
      event->set_event(event_freq.first);
      event->set_freq(event_freq.second);
    }
  }
}

void ConditionalFrequencyPredictor::FromProto(
    const RecurrencePredictorProto& proto) {
  if (!proto.has_conditional_frequency_predictor()) {
    LogSerializationStatus(
        model_identifier_,
        SerializationStatus::kConditionalFrequencyPredictorLoadingError);
    return;
  }

  for (const auto& event : proto.conditional_frequency_predictor().events()) {
    auto& events = table_[event.condition()];
    events.freqs[event.event()] = event.freq();
    events.total += event.freq();
  }
}

FrecencyPredictor::FrecencyPredictor(const FrecencyPredictorConfig& config,
                                     const std::string& model_identifier)
    : RecurrencePredictor(model_identifier),
      decay_coeff_(config.decay_coeff()) {}
FrecencyPredictor::~FrecencyPredictor() = default;

const char FrecencyPredictor::kPredictorName[] = "FrecencyPredictor";
const char* FrecencyPredictor::GetPredictorName() const {
  return kPredictorName;
}

void FrecencyPredictor::Train(unsigned int target, unsigned int condition) {
  ++num_updates_;
  TargetData& data = targets_[target];
  DecayScore(&data);
  data.last_score += 1.0f - decay_coeff_;
}

std::map<unsigned int, float> FrecencyPredictor::Rank(unsigned int condition) {
  float total = 0.0f;
  for (auto& pair : targets_) {
    DecayScore(&pair.second);
    total += pair.second.last_score;
  }
  if (total == 0.0f)
    return {};

  std::map<unsigned int, float> result;
  for (auto& pair : targets_) {
    result[pair.first] = pair.second.last_score / total;
  }
  return result;
}

void FrecencyPredictor::Cleanup(
    const std::vector<unsigned int>& valid_targets) {
  std::map<unsigned int, FrecencyPredictor::TargetData> new_targets;

  for (unsigned int id : valid_targets) {
    const auto& it = targets_.find(id);
    if (it != targets_.end())
      new_targets[id] = it->second;
  }

  targets_.swap(new_targets);
}

void FrecencyPredictor::ToProto(RecurrencePredictorProto* proto) const {
  auto* predictor = proto->mutable_frecency_predictor();

  predictor->set_num_updates(num_updates_);

  for (const auto& pair : targets_) {
    auto* target_data = predictor->add_targets();
    target_data->set_id(pair.first);
    target_data->set_last_score(pair.second.last_score);
    target_data->set_last_num_updates(pair.second.last_num_updates);
  }
}

void FrecencyPredictor::FromProto(const RecurrencePredictorProto& proto) {
  if (!proto.has_frecency_predictor()) {
    LogSerializationStatus(model_identifier_,
                           SerializationStatus::kFrecencyPredictorLoadingError);
    return;
  }
  const auto& predictor = proto.frecency_predictor();

  num_updates_ = predictor.num_updates();

  std::map<unsigned int, TargetData> targets;
  for (const auto& target_data : predictor.targets()) {
    targets[target_data.id()] = {target_data.last_score(),
                                 target_data.last_num_updates()};
  }
  targets_.swap(targets);
}

void FrecencyPredictor::DecayScore(TargetData* data) {
  int time_since_update = num_updates_ - data->last_num_updates;

  if (time_since_update > 0) {
    data->last_score *= std::pow(decay_coeff_, time_since_update);
    data->last_num_updates = num_updates_;
  }
}

HourBinPredictor::HourBinPredictor(const HourBinPredictorConfig& config,
                                   const std::string& model_identifier)
    : RecurrencePredictor(model_identifier),
      weekly_decay_coeff_(config.weekly_decay_coeff()) {
  for (const auto& pair : config.bin_weights())
    bin_weights_[pair.bin()] = pair.weight();

  if (!proto_.has_last_decay_timestamp())
    SetLastDecayTimestamp(
        base::Time::Now().ToDeltaSinceWindowsEpoch().InDays());
}

HourBinPredictor::~HourBinPredictor() = default;

const char HourBinPredictor::kPredictorName[] = "HourBinPredictor";

const char* HourBinPredictor::GetPredictorName() const {
  return kPredictorName;
}

int HourBinPredictor::GetBinFromHourDifference(int hour_difference) const {
  base::Time shifted_time =
      base::Time::Now() + base::TimeDelta::FromHours(hour_difference);
  base::Time::Exploded exploded_time;
  shifted_time.LocalExplode(&exploded_time);

  const bool is_weekend =
      exploded_time.day_of_week == 6 || exploded_time.day_of_week == 0;

  // To distinguish workdays from weekend, use now.hour for workdays and
  // now.hour + 24 for weekend.
  if (!is_weekend) {
    return exploded_time.hour;
  } else {
    return exploded_time.hour + kHoursADay;
  }
}

int HourBinPredictor::GetBin() const {
  return GetBinFromHourDifference(0);
}

void HourBinPredictor::Train(unsigned int target, unsigned int condition) {
  int hour = GetBin();
  auto& frequency_table = (*proto_.mutable_binned_frequency_table())[hour];
  frequency_table.set_total_counts(frequency_table.total_counts() + 1);
  (*frequency_table.mutable_frequency())[target] += 1;
}

std::map<unsigned int, float> HourBinPredictor::Rank(unsigned int condition) {
  std::map<unsigned int, float> ranks;
  const auto& frequency_table_map = proto_.binned_frequency_table();
  for (const auto& hour_and_weight : bin_weights_) {
    // Find adjacent bin and weight.
    const int adj_bin = GetBinFromHourDifference(hour_and_weight.first);
    const float weight = hour_and_weight.second;

    const auto find_frequency_table = frequency_table_map.find(adj_bin);
    if (find_frequency_table == frequency_table_map.end())
      continue;
    const auto& frequency_table = find_frequency_table->second;

    // Accumulates the frequency to the output.
    if (frequency_table.total_counts() > 0) {
      const int total_counts = frequency_table.total_counts();
      for (const auto& pair : frequency_table.frequency()) {
        ranks[pair.first] +=
            static_cast<float>(pair.second) / total_counts * weight;
      }
    }
  }
  return ranks;
}

// TODO(921444): Unify the hour bin predictor with the cleanup system used for
// other predictors. This is different than other predictors so as to be exactly
// the same as the Roselle predictor.

void HourBinPredictor::ToProto(RecurrencePredictorProto* proto) const {
  *proto->mutable_hour_bin_predictor() = proto_;
}

void HourBinPredictor::FromProto(const RecurrencePredictorProto& proto) {
  if (!proto.has_hour_bin_predictor()) {
    LogSerializationStatus(model_identifier_,
                           SerializationStatus::kHourBinPredictorLoadingError);
    return;
  }

  proto_ = proto.hour_bin_predictor();
  if (ShouldDecay())
    DecayAll();
}

bool HourBinPredictor::ShouldDecay() {
  const int today = base::Time::Now().ToDeltaSinceWindowsEpoch().InDays();
  // Check if we should decay the frequency
  return today - proto_.last_decay_timestamp() > 7;
}

void HourBinPredictor::DecayAll() {
  SetLastDecayTimestamp(base::Time::Now().ToDeltaSinceWindowsEpoch().InDays());
  auto& frequency_table_map = *proto_.mutable_binned_frequency_table();
  for (auto it_table = frequency_table_map.begin();
       it_table != frequency_table_map.end();) {
    auto& frequency_table = *it_table->second.mutable_frequency();
    for (auto it_freq = frequency_table.begin();
         it_freq != frequency_table.end();) {
      const int new_frequency = it_freq->second * weekly_decay_coeff_;
      it_table->second.set_total_counts(it_table->second.total_counts() -
                                        it_freq->second + new_frequency);
      it_freq->second = new_frequency;

      // Remove item that has zero frequency
      if (it_freq->second == 0) {
        frequency_table.erase(it_freq++);
      } else {
        it_freq++;
      }
    }

    // Remove bin that has zero total_counts
    if (it_table->second.total_counts() == 0) {
      frequency_table_map.erase(it_table++);
    } else {
      it_table++;
    }
  }
}

MarkovPredictor::MarkovPredictor(const MarkovPredictorConfig& config,
                                 const std::string& model_identifier)
    : RecurrencePredictor(model_identifier) {
  frequencies_ =
      std::make_unique<ConditionalFrequencyPredictor>(model_identifier);
}
MarkovPredictor::~MarkovPredictor() = default;

const char MarkovPredictor::kPredictorName[] = "MarkovPredictor";
const char* MarkovPredictor::GetPredictorName() const {
  return kPredictorName;
}

void MarkovPredictor::Train(unsigned int target, unsigned int condition) {
  if (previous_target_)
    frequencies_->Train(target, previous_target_.value());
  previous_target_ = target;
}

std::map<unsigned int, float> MarkovPredictor::Rank(unsigned int condition) {
  if (previous_target_)
    return frequencies_->Rank(previous_target_.value());
  return std::map<unsigned int, float>();
}

void MarkovPredictor::Cleanup(const std::vector<unsigned int>& valid_targets) {
  frequencies_->CleanupConditions(valid_targets);
  frequencies_->Cleanup(valid_targets);
}

void MarkovPredictor::ToProto(RecurrencePredictorProto* proto) const {
  auto* predictor = proto->mutable_markov_predictor();
  frequencies_->ToProto(predictor->mutable_frequencies());
}

void MarkovPredictor::FromProto(const RecurrencePredictorProto& proto) {
  if (!proto.has_markov_predictor()) {
    LogSerializationStatus(model_identifier_,
                           SerializationStatus::kMarkovPredictorLoadingError);
    return;
  }

  frequencies_->FromProto(proto.markov_predictor().frequencies());
}

ExponentialWeightsEnsemble::ExponentialWeightsEnsemble(
    const ExponentialWeightsEnsembleConfig& config,
    const std::string& model_identifier)
    : RecurrencePredictor(model_identifier),
      learning_rate_(config.learning_rate()) {
  for (int i = 0; i < config.predictors_size(); ++i) {
    predictors_.push_back(
        {MakePredictor(config.predictors(i), model_identifier_),
         1.0f / config.predictors_size()});
  }
}

ExponentialWeightsEnsemble::~ExponentialWeightsEnsemble() = default;

const char ExponentialWeightsEnsemble::kPredictorName[] =
    "ExponentialWeightsEnsemble";
const char* ExponentialWeightsEnsemble::GetPredictorName() const {
  return kPredictorName;
}

void ExponentialWeightsEnsemble::Train(unsigned int target,
                                       unsigned int condition) {
  // Update predictor weights. Do this before training the constituent
  // predictors to avoid biasing towards fast-adjusting predictors.
  for (auto& predictor_weight : predictors_) {
    const auto& ranks = predictor_weight.first->Rank(condition);

    // Find the normalized score associated with the ground-truth |target|.
    // If the predictor didn't rank the ground truth target, consider that a
    // score of 0.
    float total_score = 0.0f;
    for (const auto& target_score : ranks)
      total_score += target_score.second;

    float score = 0.0f;
    const auto& it = ranks.find(target);
    if (total_score > 0.0f && it != ranks.end())
      score = it->second / total_score;

    // Perform an exponential weights update.
    predictor_weight.second *= std::exp(-learning_rate_ * (1 - score));
  }

  // Re-normalize weights.
  float total_weight = 0.0f;
  for (const auto& predictor_weight : predictors_)
    total_weight += predictor_weight.second;
  for (auto& predictor_weight : predictors_) {
    predictor_weight.second /= total_weight;
  }

  // Train constituent predictors.
  for (auto& predictor_weight : predictors_)
    predictor_weight.first->Train(target, condition);
}

std::map<unsigned int, float> ExponentialWeightsEnsemble::Rank(
    unsigned int condition) {
  std::map<unsigned int, float> result;
  for (const auto& predictor_weight : predictors_) {
    const auto& ranks = predictor_weight.first->Rank(condition);
    for (const auto& target_score : ranks) {
      // Weights are kept normalized by Train, so all scores remain in [0,1] if
      // the predictors' scores are in [0,1].
      result[target_score.first] +=
          target_score.second * predictor_weight.second;
    }
  }
  return result;
}

void ExponentialWeightsEnsemble::ToProto(
    RecurrencePredictorProto* proto) const {
  auto* ensemble = proto->mutable_exponential_weights_ensemble();

  for (const auto& predictor_weight : predictors_) {
    predictor_weight.first->ToProto(ensemble->add_predictors());
    ensemble->add_weights(predictor_weight.second);
  }
}

void ExponentialWeightsEnsemble::FromProto(
    const RecurrencePredictorProto& proto) {
  if (!proto.has_exponential_weights_ensemble()) {
    LogSerializationStatus(
        model_identifier_,
        SerializationStatus::kExponentialWeightsEnsembleLoadingError);
    return;
  }
  const auto& ensemble = proto.exponential_weights_ensemble();
  int num_predictors = static_cast<int>(predictors_.size());
  DCHECK_EQ(num_predictors, ensemble.predictors_size());
  DCHECK_EQ(num_predictors, ensemble.weights_size());

  for (int i = 0; i < num_predictors; ++i) {
    predictors_[i].first->FromProto(ensemble.predictors(i));
    predictors_[i].second = ensemble.weights(i);
  }
}

FrequencyPredictor::FrequencyPredictor(const std::string& model_identifier)
    : RecurrencePredictor(model_identifier) {}

FrequencyPredictor::FrequencyPredictor(const FrequencyPredictorConfig& config,
                                       const std::string& model_identifier)
    : RecurrencePredictor(model_identifier) {}

FrequencyPredictor::~FrequencyPredictor() = default;

const char FrequencyPredictor::kPredictorName[] = "FrequencyPredictor";
const char* FrequencyPredictor::GetPredictorName() const {
  return kPredictorName;
}

void FrequencyPredictor::Train(unsigned int target, unsigned int condition) {
  counts_[target] += 1.0f;
}

std::map<unsigned int, float> FrequencyPredictor::Rank(unsigned int condition) {
  float total = 0.0f;
  for (const auto& pair : counts_)
    total += pair.second;

  std::map<unsigned int, float> result;
  for (const auto& pair : counts_)
    result[pair.first] = pair.second / total;
  return result;
}

void FrequencyPredictor::Cleanup(
    const std::vector<unsigned int>& valid_targets) {
  std::map<unsigned int, int> new_counts;

  for (unsigned int id : valid_targets) {
    const auto& it = counts_.find(id);
    if (it != counts_.end())
      new_counts[id] = it->second;
  }

  counts_.swap(new_counts);
}

void FrequencyPredictor::ToProto(RecurrencePredictorProto* proto) const {
  auto* counts = proto->mutable_frequency_predictor()->mutable_counts();
  for (auto& pair : counts_)
    (*counts)[pair.first] = pair.second;
}

void FrequencyPredictor::FromProto(const RecurrencePredictorProto& proto) {
  if (!proto.has_frequency_predictor()) {
    LogSerializationStatus(
        model_identifier_,
        SerializationStatus::kFrequencyPredictorLoadingError);
    return;
  }

  for (const auto& pair : proto.frequency_predictor().counts())
    counts_[pair.first] = pair.second;
}

}  // namespace app_list
