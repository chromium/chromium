// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/app_list/search/search_result_ranker/frecency_store.h"

#include <algorithm>
#include <cmath>
#include <limits>
#include <utility>

#include "base/logging.h"
#include "base/stl_util.h"
#include "chrome/browser/ui/app_list/search/search_result_ranker/frecency_store.pb.h"

namespace app_list {

namespace {

// The lowest a value's score is allowed to be before it is deleted regardless
// of how full the store is.
static constexpr float kMinScoreBeforeDelete =
    std::numeric_limits<float>::epsilon();

}  // namespace

FrecencyStore::FrecencyStore(int value_limit, float decay_coeff)
    : value_limit_(value_limit), decay_coeff_(decay_coeff) {
  if (decay_coeff <= 0.0f || decay_coeff >= 1.0f) {
    LOG(ERROR) << "FrecencyStore decay_coeff has invalid value: " << decay_coeff
               << ", resetting to default.";
    decay_coeff_ = 0.75f;
  }
}

FrecencyStore::~FrecencyStore() {}

unsigned int FrecencyStore::Update(const std::string& value) {
  if (static_cast<unsigned int>(values_.size()) >= 2 * value_limit_)
    Cleanup();

  ValueData* data;
  auto it = values_.find(value);
  if (it != values_.end()) {
    data = &it->second;
  } else {
    auto ret = values_.insert(std::pair<std::string, FrecencyStore::ValueData>(
        value, {next_id_++, 0.0f, 0u}));
    DCHECK(ret.second);
    data = &ret.first->second;
  }

  ++num_updates_;
  DecayScore(data);
  data->last_score += 1.0f - decay_coeff_;

  return data->id;
}

void FrecencyStore::Rename(const std::string& value,
                           const std::string& new_value) {
  auto it = values_.find(value);

  // If |value| doesn't exist in our map, then we have nothing to rename, so
  // ignore. Also return if the rename is a no-op.
  if (it == values_.end() || value == new_value)
    return;
  ValueData data = it->second;
  values_.erase(it);

  // If |new_value| already exists, we have a conflict. The knowledge that
  // |value| should be renamed to |new_value| is newer than our previous
  // knowledge about |new_value|, so prioritise the rename and delete the old
  // |new_value|.
  values_[new_value] = data;
}

void FrecencyStore::Remove(const std::string& value) {
  values_.erase(value);
}

base::Optional<unsigned int> FrecencyStore::GetId(const std::string& value) {
  auto it = values_.find(value);
  if (it == values_.end())
    return base::nullopt;
  return it->second.id;
}

const FrecencyStore::ScoreTable& FrecencyStore::GetAll() {
  DecayAllScores();
  return values_;
}

void FrecencyStore::ToProto(FrecencyStoreProto* proto) const {
  proto->set_value_limit(value_limit_);
  proto->set_decay_coeff(decay_coeff_);
  proto->set_num_updates(num_updates_);
  proto->set_next_id(next_id_);

  auto* proto_values = proto->mutable_values();
  for (auto& pair : values_) {
    FrecencyStoreProto::ValueData proto_data;
    proto_data.set_id(pair.second.id);
    proto_data.set_last_score(pair.second.last_score);
    proto_data.set_last_num_updates(pair.second.last_num_updates);
    (*proto_values)[pair.first] = proto_data;
  }
}

void FrecencyStore::FromProto(const FrecencyStoreProto& proto) {
  value_limit_ = proto.value_limit();
  decay_coeff_ = proto.decay_coeff();
  num_updates_ = proto.num_updates();
  next_id_ = proto.next_id();

  ScoreTable values;
  for (const auto& pair : proto.values()) {
    values[pair.first] = {pair.second.id(), pair.second.last_score(),
                          pair.second.last_num_updates()};
  }
  values_.swap(values);
}

void FrecencyStore::DecayScore(ValueData* data) {
  int time_since_update = num_updates_ - data->last_num_updates;

  if (time_since_update > 0) {
    data->last_score *= std::pow(decay_coeff_, time_since_update);
    data->last_num_updates = num_updates_;
  }
}

void FrecencyStore::DecayAllScores() {
  for (auto iter = values_.begin(); iter != values_.end();) {
    ValueData& data = iter->second;
    DecayScore(&data);
    if (data.last_score <= kMinScoreBeforeDelete) {
      // C++11: the return value of erase(iter) is an iterator pointing to the
      // next element in the container.
      iter = values_.erase(iter);
    } else {
      ++iter;
    }
  }
}

void FrecencyStore::Cleanup() {
  if (static_cast<unsigned int>(values_.size()) < 2 * value_limit_)
    return;

  std::vector<std::pair<std::string, ValueData>> value_pairs;
  for (auto& pair : values_) {
    ValueData& data = pair.second;
    DecayScore(&data);
    if (data.last_score > kMinScoreBeforeDelete)
      value_pairs.emplace_back(pair.first, pair.second);
  }

  std::sort(value_pairs.begin(), value_pairs.end(),
            [](auto const& a, auto const& b) {
              return a.second.last_score > b.second.last_score;
            });

  ScoreTable values;
  for (unsigned int i = 0;
       i < value_limit_ && i < static_cast<unsigned int>(value_pairs.size());
       ++i) {
    values.insert(value_pairs[i]);
  }

  swap(values, values_);
}

}  // namespace app_list
