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
#include "components/variations/limited_layer_entropy_cost_tracker.h"
#include "components/variations/variations_layers.h"
#include "components/variations/variations_seed_processor.h"
#include "limited_entropy_randomization.h"

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

void LogSeedRejectionReason(SeedRejectionReason reason) {
  base::UmaHistogramEnumeration("Variations.LimitedEntropy.SeedRejectionReason",
                                reason);
}

// Checks if there is enough entropy to handle all studies constrained to the
// limited layer.
bool IsEnoughLimitedEntropyAvailable(
    const VariationsLayers& layers,
    const VariationsSeed& seed,
    double entropy_limit = kGoogleWebEntropyLimitInBits) {
  LimitedLayerEntropyCostTracker entropy_tracker(layers, seed, entropy_limit);
  for (const Study& study : seed.study()) {
    if (!entropy_tracker.TryAddEntropyUsedByStudy(study)) {
      return false;
    }
  }
  return true;
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
      // TODO(crbug.com/319681288): The entropy calculation does not cover cases
      // when there is more than one layer with LIMITED entropy mode and we
      // should reject the seed in case the combined layers use more entropy
      // than the limit.
      // TODO(crbug.com/337066023): Validate that GWS-Visible studies are not
      // constrained to non-limited and limited layers at the same time. Once
      // the server side check is enabled, GWS-visible studies should only
      // reference limited layers.
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

  // There should be enough entropy to assign all studies constrained to the
  // layer with LIMITED entropy mode. In fact, the entropy limit should have
  // already been checked server side during the seed generation.
  bool is_entropy_misconfigured =
      !IsEnoughLimitedEntropyAvailable(layers, seed);
  if (is_entropy_misconfigured) {
    LogSeedRejectionReason(SeedRejectionReason::kHighEntropyUsage);
  }
  return is_entropy_misconfigured;
}

bool IsEnoughLimitedEntropyAvailableForTesting(const VariationsLayers& layers,
                                               const VariationsSeed& seed,
                                               double entropy_limit) {
  return IsEnoughLimitedEntropyAvailable(layers, seed, entropy_limit);
}

}  // namespace variations
