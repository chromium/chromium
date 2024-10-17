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

// Gets the active limited layer in the seed. There should be at most one layer
// with `LIMITED` entropy mode that's applicable to the client. Returns nullptr
// if any of the following conditions are met:
// 1. There is more than one limited layer in the seed.
// 2. There is no limited layer in the seed.
// 3. There is a single limited layer, but it's not active.
// 4. There is a single limited layer, but its slots bounds are not valid.
// 5. There is a single limited layer with no slots.
//
// Creates a dump without crashing when:
// - Condition 1 is met because there should be at most 1 limited layer in the
// seed applicable to the client.
// - Condition 4 or condition 5 is met because a limited layer with invalid
// bounds or with no slots is a misconfigured layer.
const Layer* GetActiveLimitedLayer(const VariationsLayers& layers,
                                   const VariationsSeed& seed) {
  const Layer* limited_layer = nullptr;
  for (const Layer& layer : seed.layers()) {
    bool is_limited_layer = layer.entropy_mode() == Layer::LIMITED;
    if (!is_limited_layer) {
      continue;
    }
    if (limited_layer) {
      // Returns nullptr if there is more than 1 limited later in the seed.
      // There should be at most one layer with `LIMITED` entropy mode that's
      // applicable to the client.
      base::debug::DumpWithoutCrashing();
      return nullptr;
    }
    limited_layer = &layer;
  }

  // At most one layer with `LIMITED` entropy mode will be applicable to the
  // client; it's fine if no such layers exist, so no need to create a dump.
  if (limited_layer == nullptr) {
    return nullptr;
  }

  // A layer is "active" for a client if the client's slot for that layer is
  // associated with a layer member. If the limited layer is not active for a
  // client, the studies that are constrained to the layer will not be assigned,
  // and thus the entropy limit will not be reached. In this case there is no
  // need to create a dump.
  if (!layers.IsLayerActive(limited_layer->id())) {
    return nullptr;
  }
  if (!VariationsLayers::AreSlotBoundsValid(*limited_layer)) {
    base::debug::DumpWithoutCrashing();
    return nullptr;
  }
  if (limited_layer->num_slots() == 0) {
    base::debug::DumpWithoutCrashing();
    return nullptr;
  }
  return limited_layer;
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
    const VariationsLayers& layers,
    const VariationsSeed& seed,
    double entropy_limit_in_bits)
    : entropy_limit_in_bits_(entropy_limit_in_bits) {
  // Store the ids of all limited layers found in the seed.
  for (const Layer& layer : seed.layers()) {
    if (layer.entropy_mode() == Layer::LIMITED) {
      limited_layers_ids_.insert(layer.id());
    }
  }
  const Layer* limited_layer = GetActiveLimitedLayer(layers, seed);
  if (!limited_layer) {
    // There is no valid active limited layer for the client, meaning no limited
    // entropy will be consumed by studies. A layer is "active" for a client if
    // the client's slot for that layer is associated with a layer member. A
    // "valid" layer is a layer that is not misconfigured. If the limited layer
    // is misconfigured the seed will be rejected by function
    // `CreateTrialsFromSeed` in
    // components/variations/service/variations_field_trial_creator_base.cc.
    return;
  }
  active_limited_layer_id_ = limited_layer->id();

  // Computes the entropy used by each layer member keyed by its ID.
  for (const Layer::LayerMember& member : limited_layer->members()) {
    // All layer members are included in the entropy calculation, including
    // empty ones – ones not referenced by any study. A client assigned to an
    // empty layer member would have the visible assignment state of "no study
    // assigned", which itself reveals information and should be accounted for
    // in the entropy calculation.
    entropy_used_by_layer_members_[member.id()] =
        GetLayerMemberEntropy(member, limited_layer->num_slots());
  }
}

LimitedLayerEntropyCostTracker::~LimitedLayerEntropyCostTracker() = default;

bool LimitedLayerEntropyCostTracker::TryAddEntropyUsedByStudy(
    const Study& study) {
  // Returns false if there is no active limited layer for the client but the
  // study is referencing a limited layer. This scenario could happen if there
  // is more than one limited layer in the seed or if the single limited layer
  // present in the seed is misconfigured.
  if (active_limited_layer_id_ == kInvalidLayerId && study.has_layer() &&
      limited_layers_ids_.contains(study.layer().layer_id())) {
    return false;
  }

  // Returns true if the study is not referencing the limited layer. In this
  // scenario, the study does not consume entropy on the limited layer. At this
  // stage, we already validated that the seed is not misconfigured, so
  // GWS-Visible studies should not be constrained to non-limited layers.
  if (active_limited_layer_id_ == kInvalidLayerId || !study.has_layer() ||
      study.layer().layer_id() != active_limited_layer_id_) {
    return true;
  }

  // Returns false if the entropy used by a layer member is already above the
  // entropy limit, meaning no more study can be assigned to the limited layer.
  if (entropy_limit_reached_) {
    return false;
  }

  // Returns true if the sudy does not consume entropy at all (e.g. a study with
  // no Google web experiment ID or Google web trigger experiment ID).
  double entropy_used_by_study = GetEntropyUsedByStudy(study);
  if (entropy_used_by_study <= 0) {
    return true;
  }

  // Considers all layer members when trying to add the entropy used by the
  // study. The `entropy_used_by_layer_members_` field contains all members of
  // the limited layer, including the members currently using 0 entropy.
  for (const auto& [member_id, member_entropy] :
       entropy_used_by_layer_members_) {
    // Includes entropy from the study if it references this `member`. Study
    // might reference a non-existent layer, in which case the study will not
    // be assigned (see ShouldAddStudy() in
    // components/variations/study_filtering.cc). Entropy calculation should
    // also exclude such studies.
    if (!VariationsLayers::IsReferencingLayerMemberId(study.layer(),
                                                      member_id)) {
      continue;
    }
    entropy_used_by_layer_members_[member_id] += entropy_used_by_study;
    includes_entropy_used_by_studies_ = true;
    // TODO(siakabaro): The entropy used by a layer member could be over the
    // entropy limit if the layer member covers a very small percentage of the
    // population. In such a case, we need to need to pool the empty layer
    // members together and check if their combined entropy is not over the
    // limit.
    if (entropy_used_by_layer_members_[member_id] > entropy_limit_in_bits_) {
      entropy_limit_reached_ = true;
    }
  }

  // Returns false if the entropy limit is reached.
  return !entropy_limit_reached_;
}

double LimitedLayerEntropyCostTracker::GetTotalEntropyUsedForTesting() {
  if (active_limited_layer_id_ == kInvalidLayerId) {
    return 0.0;
  }
  // The entropy used is zero when none of the studies constrained to the
  // limited layer use any entropy. The results stored in
  // `entropy_used_by_layer_members_` is not applicable here because they
  // include entropy used from layer members. Those entropy usage only applies
  // when studies that use entropy are constrained to these layer members.
  if (!entropy_limit_reached_ && !includes_entropy_used_by_studies_) {
    return 0.0;
  }

  double max_entropy_used = 0.0;
  for (const auto& [member_id, member_entropy] :
       entropy_used_by_layer_members_) {
    max_entropy_used = std::max(max_entropy_used, member_entropy);
  }
  return max_entropy_used;
}

}  // namespace variations
