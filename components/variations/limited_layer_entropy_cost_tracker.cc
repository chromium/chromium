// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/variations/limited_layer_entropy_cost_tracker.h"

#include <math.h>

#include <cstdint>
#include <limits>

#include "base/check_op.h"
#include "base/debug/dump_without_crashing.h"
#include "base/memory/ptr_util.h"
#include "base/metrics/histogram_functions.h"
#include "base/numerics/checked_math.h"
#include "base/numerics/safe_conversions.h"
#include "components/variations/variations_layers.h"
#include "components/variations/variations_seed_processor.h"

namespace variations {
namespace {

// Converts a probability value (represented by numerator/denominator) to an
// entropy value. Callers should ensure that both arguments are strictly
// positive and that `numerator` <= `denominator`. This always returns a
// non-negative number.
double ConvertToBitsOfEntropy(uint64_t numerator, uint64_t denominator) {
  CHECK_GT(numerator, 0u);
  CHECK_LE(numerator, denominator);
  return -log2(base::strict_cast<double>(numerator) /
               base::strict_cast<double>(denominator));
}

// Returns the number of bits of entropy used by a single study.
double GetEntropyUsedByStudy(const Study& study) {
  if (study.consistency() == Study::SESSION) {
    // Session-consistent studies do not consume entropy. They are randomized
    // for each Chrome browser process' lifetime; they use neither the low
    // entropy source nor the limited entropy randomization source.
    return 0.0;
  }
  // Use uint32_t to match the type of `probability_weight` field in the
  // experiment proto.
  uint32_t min_weight = std::numeric_limits<uint32_t>::max();
  uint64_t total_weight = 0;

  // The entropy limit applies specifically to the experiments that specify a
  // Google web experiment ID (or Google web trigger experiment ID).
  bool has_google_web_experiment = false;
  for (const auto& experiment : study.experiment()) {
    // This will CHECK if `total_weight` (a uint64_t) overflows, which is nearly
    // impossible since each `experiment.probability_weight()` is a uint32_t.
    // This is not expected to come up for valid variations seeds in production.
    total_weight = base::CheckAdd(total_weight, experiment.probability_weight())
                       .ValueOrDie();

    // Skip experiments with zero probability. They will not cause entropy
    // usage since they will never be assigned. Also, checking for non-zero
    // probability ensures that `has_google_web_experiment`
    // implies that `total_weight` > 0.
    if (experiment.probability_weight() > 0u &&
        VariationsSeedProcessor::HasGoogleWebExperimentId(experiment)) {
      has_google_web_experiment = true;
      min_weight = std::min(min_weight, experiment.probability_weight());
    }
  }
  if (!has_google_web_experiment) {
    return 0.0;
  }

  // By now, `has_google_web_experiment` being true implies 0 < `min_weight` <=
  // `total_weight`, which is required by ConvertToBitsOfEntropy().
  //
  // Mathematically, this returns -log2(`min_weight` / `total_weight`).
  // If the probability of a client being assigned to a specific group in the
  // study is p, the entropy revealed by this assignment is -log2(p):
  // https://en.wikipedia.org/wiki/Entropy_(information_theory). Hence, the
  // entropy is maximal for clients assigned to the smallest group in the study.
  return ConvertToBitsOfEntropy(min_weight, total_weight);
}

// Computes the entropy used by the limited layer member.
double GetLayerMemberEntropy(const Layer::LayerMember& member,
                             uint64_t num_slots) {
  uint32_t num_slots_in_member = 0;
  for (const Layer::LayerMember::SlotRange& range : member.slots()) {
    // Adding one since the range is inclusive.
    num_slots_in_member += range.end() - range.start() + 1;
  }
  return ConvertToBitsOfEntropy(num_slots_in_member, num_slots);
}

}  // namespace

LimitedLayerEntropyCostTracker::LimitedLayerEntropyCostTracker(
    const Layer& layer,
    double entropy_limit_in_bits)
    : entropy_limit_in_bits_(entropy_limit_in_bits),
      limited_layer_id_(layer.id()) {
  // The caller should have already validated the layer. However, as the layer
  // data comes from an external source, we verify it here again for safety,
  // instead of using a CHECK. Note that verify each condition individually in
  // order to dump a unique stack trace for each failure condition.
  if (limited_layer_id_ == 0u) {
    Invalidate();
    return;
  }
  if (entropy_limit_in_bits_ <= 0.0) {
    Invalidate();
    return;
  }
  const auto num_slots = layer.num_slots();
  if (num_slots <= 0u) {
    Invalidate();
    return;
  }
  const auto& layer_members = layer.members();
  if (layer_members.empty()) {
    Invalidate();
    return;
  }
  if (layer.entropy_mode() != Layer::LIMITED) {
    Invalidate();
    return;
  }
  if (!VariationsLayers::AreSlotBoundsValid(layer)) {
    Invalidate();
    return;
  }

  // Compute the entropy used by each layer member keyed by its memberID.
  entropy_used_by_member_id_.reserve(layer_members.size());
  for (const auto& member : layer_members) {
    if (member.id() == 0u) {
      Invalidate();
      return;
    }
    // All layer members are included in the entropy calculation, including
    // empty ones – ones not referenced by any study. A client assigned to an
    // empty layer member would have the visible assignment state of "no study
    // assigned", which itself reveals information and should be accounted for
    // in the entropy calculation.
    const bool inserted =
        entropy_used_by_member_id_
            .emplace(member.id(), GetLayerMemberEntropy(member, num_slots))
            .second;
    if (!inserted) {
      // => Duplicated layer member ID.
      Invalidate();
      return;
    }
  }
}

LimitedLayerEntropyCostTracker::~LimitedLayerEntropyCostTracker() = default;

bool LimitedLayerEntropyCostTracker::AddEntropyUsedByStudy(const Study& study) {
  if (!IsValid()) {
    return false;
  }
  // The caller should have already validated the study's layer references.
  // However, as the study data comes from an external source, we verify it
  // here again for safety, instead of using a CHECK. Note that verify each
  // condition individually in order to dump a unique stack trace for each
  // failure condition.
  if (!study.has_layer()) {
    Invalidate();
    return false;
  }
  const auto& layer_ref = study.layer();
  if (layer_ref.layer_id() != limited_layer_id_) {
    Invalidate();
    return false;
  }
  const auto& layer_member_ids =
      layer_ref.layer_member_ids().empty()
          ? VariationsLayers::FallbackLayerMemberIds(layer_ref)
          : layer_ref.layer_member_ids();
  if (layer_member_ids.empty()) {
    Invalidate();
    return false;
  }

  // Returns false if the entropy used by a layer member is already above the
  // entropy limit, meaning no more study can be assigned to the limited layer.
  if (entropy_limit_exceeded_) {
    return false;
  }

  // Returns true if the study does not consume entropy at all (e.g. a study
  // with no Google web experiment ID or Google web trigger experiment ID).
  double study_entropy = GetEntropyUsedByStudy(study);
  if (study_entropy <= 0) {
    return true;
  }

  // Update the entropy in the members referenced by the study. It is assumed
  // that layer member references have already been validated by the caller.
  for (const uint32_t member_id : layer_member_ids) {
    if (member_id == 0u) {
      Invalidate();
      return false;
    }
    const auto it = entropy_used_by_member_id_.find(member_id);
    if (it == entropy_used_by_member_id_.end()) {
      Invalidate();
      return false;
    }

    auto& entropy_used = it->second;
    entropy_used += study_entropy;
    includes_study_entropy_ = true;

    // TODO(siakabaro): The entropy used by a layer member could be over the
    // entropy limit if the layer member covers a very small percentage of the
    // population. In such a case, we need to need to pool the empty layer
    // members together and check if their combined entropy is not over the
    // limit.
    if (entropy_used > entropy_limit_in_bits_) {
      entropy_limit_exceeded_ = true;
    }
  }

  // Returns false if the entropy limit is reached.
  return !entropy_limit_exceeded_;
}

double LimitedLayerEntropyCostTracker::GetMaxEntropyUsedForTesting() const {
  if (!includes_study_entropy_) {
    return 0.0;
  }
  double max_entropy_used = 0.0;
  for (const auto& [member_id, entropy_used] : entropy_used_by_member_id_) {
    max_entropy_used = std::max(max_entropy_used, entropy_used);
  }
  return max_entropy_used;
}

void LimitedLayerEntropyCostTracker::Invalidate() {
  // The caller should have already validated the layer and study info before
  // any and all calls to the tracker. However, as the layer and study data
  // comes from an external source, there are additional safety checks made
  // throughout the tracker. We use these instead of CHECKS or DCHECKS and
  // verify each condition individually in order to dump a unique stack trace
  // for each failure condition.
  is_valid_ = false;
  base::debug::DumpWithoutCrashing();
}

}  // namespace variations
