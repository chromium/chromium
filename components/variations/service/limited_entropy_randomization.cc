// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/variations/service/limited_entropy_randomization.h"

#include <math.h>

#include <cstdint>
#include <limits>

#include "base/check_op.h"
#include "base/metrics/histogram_functions.h"
#include "base/numerics/checked_math.h"
#include "base/numerics/safe_conversions.h"
#include "components/variations/variations_layers.h"
#include "components/variations/variations_seed_processor.h"

namespace variations {

namespace {

// These values are persisted to logs. Entries should not be renumbered and
// numeric values should never be reused.
enum class SeedRejectionReason {
  kHighEntropyUsage = 0,
  kMoreThenOneLimitedLayer = 1,
  kLimitedLayerHasInvalidSlotBounds = 2,
  kLimitedLayerDoesNotContainSlots = 3,
  kMaxValue = kLimitedLayerDoesNotContainSlots,
};

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

void LogSeedRejectionReason(SeedRejectionReason reason) {
  base::UmaHistogramEnumeration("Variations.LimitedEntropy.SeedRejectionReason",
                                reason);
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
  // study is p, the entropy revealed by this assignment is
  // -log2(p): https://en.wikipedia.org/wiki/Entropy_(information_theory).
  // Hence, the entropy is maximal for clients assigned to the smallest group
  // in the study.
  return ConvertToBitsOfEntropy(min_weight, total_weight);
}

// Returns the maximal amount of entropy (in bits) used across all studies
// constrained to the given limited layer. This function requires the following
// conditions to be met:
// - The total number of slots in `limited_layer` is larger than 0.
// - For each `SlotRange`, range end is larger than or equal to range start.
// - The sum of the ranges (`num_slots_in_member` below) cannot exceed the
//   number of slots in the layer.
double GetEntropyUsedByLimitedLayer(const Layer& limited_layer,
                                    const VariationsSeed& seed) {
  // The caller should be responsible for the following. Adding the checks for
  // documentation.
  CHECK_EQ(Layer::LIMITED, limited_layer.entropy_mode());
  CHECK_GE(limited_layer.num_slots(), 0u);
  CHECK(VariationsLayers::AreSlotBoundsValid(limited_layer));

  // Entropy used by each layer member keyed by its ID. Using uint32_t as the
  // key type since the ID of a layer member proto is a uint32_t.
  std::map<uint32_t, double> entropy_used;

  for (const Layer::LayerMember& member : limited_layer.members()) {
    uint32_t num_slots_in_member = 0;
    for (const Layer::LayerMember::SlotRange& range : member.slots()) {
      // Adding one since the range is inclusive.
      num_slots_in_member += range.end() - range.start() + 1;
    }
    entropy_used[member.id()] =
        ConvertToBitsOfEntropy(num_slots_in_member, limited_layer.num_slots());
  }

  bool includes_entropy_used_by_study = false;
  for (const Study& study : seed.study()) {
    if (!study.has_layer() || study.layer().layer_id() != limited_layer.id()) {
      continue;
    }
    for (const Layer::LayerMember& member : limited_layer.members()) {
      // Includes entropy from the study if it references this `member`. Study
      // might reference a non-existent layer, in which case the study will not
      // be assigned (see ShouldAddStudy() in
      // components/variations/study_filtering.cc). Entropy calculation should
      // also exclude such studies.
      if (!VariationsLayers::IsReferencingLayerMemberId(study.layer(),
                                                        member.id())) {
        continue;
      }
      // TODO(b/319681288): Consider mutual exclusivity among studies
      // referencing the same layer member from the study's filter values.
      double entropy_used_by_study = GetEntropyUsedByStudy(study);
      if (entropy_used_by_study > 0) {
        entropy_used[member.id()] += entropy_used_by_study;
        includes_entropy_used_by_study = true;
      }
    }
  }

  // The entropy used is zero when none of the studies constrained to the
  // limited layer use any entropy. The results stored in `entropy_used` is
  // not applicable here because they include entropy used from layer members.
  // Those entropy usage only applies when studies that use entropy are
  // constrained to these layer members.
  if (!includes_entropy_used_by_study) {
    return 0.0;
  }

  double max_entropy_used = 0.0;
  for (const auto& entry : entropy_used) {
    // All layer members are included in the entropy calculation, including
    // empty ones – ones not referenced by any study. A client assigned to an
    // empty layer member would have the visible assignment state of "no study
    // assigned", which itself reveals information and should be accounted for
    // in the entropy calculation.
    max_entropy_used = std::max(max_entropy_used, entry.second);
  }
  return max_entropy_used;
}

}  // namespace

bool SeedHasMisconfiguredEntropy(const VariationsLayers& layers,
                                 const VariationsSeed& seed) {
  const Layer* limited_layer = nullptr;
  for (const Layer& layer : seed.layers()) {
    bool is_limited_layer = layer.entropy_mode() == Layer::LIMITED;
    if (!is_limited_layer) {
      continue;
    }
    if (limited_layer) {
      // There should be one layer with `LIMITED` entropy mode that's applicable
      // to the client.
      // TODO(b/319681288): The entropy calculation does not cover cases when
      // there is more than one layer with LIMITED entropy mode and we should
      // reject the seed in case the combined layers use more entropy than the
      // limit.
      LogSeedRejectionReason(SeedRejectionReason::kMoreThenOneLimitedLayer);
      return true;
    }
    limited_layer = &layer;
  }
  if (!limited_layer) {
    // At most one layer with `LIMITED` entropy mode will be applicable to the
    // client; it's fine if no such layers exist (don't reject the seed).
    return false;
  }

  // If the layer with LIMITED entropy mode is not active,
  // `layers.IsLayerMemberActive()` must be false for all members. This means,
  // for this client, the studies that are constrained to the layer will not be
  // assigned, and thus the entropy limit will not be breached. In this case,
  // other studies should not be affected (i.e., the seed should not be
  // rejected).
  if (!layers.IsLayerActive(limited_layer->id())) {
    return false;
  }

  // As of January 2024, the following two checks already run as part of the
  // layer validity check. Hence, these clauses should never be triggered.
  // However, these are necessary conditions for the entropy calculation. To
  // avoid a long-distance dependency on the layer validity check, these
  // necessary conditions are re-checked here.
  if (!VariationsLayers::AreSlotBoundsValid(*limited_layer)) {
    LogSeedRejectionReason(
        SeedRejectionReason::kLimitedLayerHasInvalidSlotBounds);
    return true;
  }
  if (limited_layer->num_slots() == 0) {
    LogSeedRejectionReason(
        SeedRejectionReason::kLimitedLayerDoesNotContainSlots);
    return true;
  }

  // GetEntropyUsedByLimitedLayer() must be called when
  // VariationsLayers::IsLayerActive() is true for this `limited_layer`.
  double entropy_used = GetEntropyUsedByLimitedLayer(*limited_layer, seed);
  bool is_entropy_misconfigured = entropy_used > kGoogleWebEntropyLimitInBits;
  if (is_entropy_misconfigured) {
    LogSeedRejectionReason(SeedRejectionReason::kHighEntropyUsage);
  }
  return is_entropy_misconfigured;
}

double GetEntropyUsedByLimitedLayerForTesting(const Layer& limited_layer,
                                              const VariationsSeed& seed) {
  return GetEntropyUsedByLimitedLayer(limited_layer, seed);
}

}  // namespace variations
