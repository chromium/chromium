// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/app_list/search/ranking/mrfu_cache.h"

#include <cmath>

#include "base/files/file_path.h"
#include "base/logging.h"
#include "base/macros.h"
#include "base/time/time.h"

namespace app_list {
namespace {

// We boost scores with the equation
//
//   score = score + (1 - score) * k
//
// where k is a boost coefficient. This is hard to reason about, so instead
// set a 'boost factor', which is the answer to question: "how many consecutive
// uses should it take for a score to start at 0 and reach 2/3?". We can then
// define k based on the boost factor. Note 2/3 is chosen arbitrarily, but
// it's a reasonably high score.
//
// Here's our terminology:
//  - x is the score
//  - f(x) is one decay followed by one boost
//  - f_n(x) is f applied n times to x
//  - k is the boost coefficient
//  - D is the decay coefficient
//  - N is the boost factor
//
// We want an equation for the score after using an new item N times - ie.
// f_N(0) - and then solve for k.
//
//    f(x) = Dx + (1-Dx)k
//         = k + (1-k)Dx
//
//  f_n(x) = (1-k)^n D^n x + k sum((1-k)^i D^i, 0 <= i < n)
//
//  f_N(0) = k sum( (1-k)^i D^i, 0 <= i < N)
//         = k (1 - D^N (1-k)^N) / (1 + D(k-1))              identity on sum
//
// Therefore we're looking for the value of k that satisfies
//
//   k (1 - D^N (1-k)^N) / (1 + D(k-1)) = 2/3
//
// which isn't easily solvable, so this function approximates it numerically.
float ApproximateBoostCoefficient(float decay_coefficient, float boost_factor) {
  float D = decay_coefficient;
  float N = boost_factor;
  float target = 2.0f / 3.0f;

  float k_min = 0.0f;
  float k_max = 1.0f;
  for (int i = 0; i < 10; ++i) {
    float k = (k_min + k_max) / 2.0f;
    float value = k * (1 - pow(D, N) * pow(1 - k, N)) / (1 + D * k - D);
    if (value < target) {
      k_min = k;
    } else {
      k_max = k;
    }
  }

  return (k_min + k_max) / 2.0f;
}

}  // namespace

MrfuCache::MrfuCache(const base::FilePath& path, const Params& params)
    : proto_(path, params.write_delay, base::DoNothing(), base::DoNothing()) {
  // See header comment for explanation.
  decay_coeff_ = exp(log(0.5) / params.half_life);
  boost_coeff_ = ApproximateBoostCoefficient(decay_coeff_, params.boost_factor);
}

MrfuCache::~MrfuCache() {}

void MrfuCache::Decay(Score* score) {
  int64_t update_count = proto_->update_count();
  int64_t count_delta = update_count - score->last_update_count();
  if (count_delta > 0) {
    score->set_score(score->score() * std::pow(decay_coeff_, count_delta));
    score->set_last_update_count(update_count);
    proto_.QueueWrite();
  }
}

void MrfuCache::Use(const std::string& item) {
  if (!proto_.initialized())
    return;

  // Get the Score for |item| from the proto. If it doesn't exist, create an
  // empty score.
  Score* score;
  auto* items = proto_->mutable_items();
  const auto& it = items->find(item);
  if (it != items->end()) {
    score = &it->second;
  } else {
    auto ret = items->insert({item, Score()});
    DCHECK(ret.second);
    score = &ret.first->second;
  }

  // The order of these three steps is important: first move 'time' forward one
  // step, then decay the score, then add the boost for the current use.
  proto_->set_update_count(proto_->update_count() + 1);
  Decay(score);
  score->set_score(score->score() + boost_coeff_ * (1.0f - score->score()));
  proto_.QueueWrite();
}

float MrfuCache::Get(const std::string& item) {
  if (!proto_.initialized())
    return 0.0f;

  auto* items = proto_->mutable_items();
  const auto& it = items->find(item);
  if (it == items->end())
    return 0.0f;

  // |score| may not be current, so |Decay| it if needed.
  Score* score = &it->second;
  Decay(score);
  return score->score();
}

}  // namespace app_list
