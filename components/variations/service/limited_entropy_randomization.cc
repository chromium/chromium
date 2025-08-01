// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/variations/service/limited_entropy_randomization.h"

#include <math.h>

#include <cstdint>
#include <limits>

#include "base/check_op.h"
#include "base/containers/contains.h"
#include "base/metrics/histogram_functions.h"
#include "base/numerics/checked_math.h"
#include "base/numerics/safe_conversions.h"
#include "base/version_info/version_info.h"
#include "build/build_config.h"
#include "components/variations/client_filterable_state.h"
#include "components/variations/limited_layer_entropy_cost_tracker.h"
#include "components/variations/study_filtering.h"
#include "components/variations/variations_layers.h"
#include "components/variations/variations_seed_processor.h"
#include "third_party/abseil-cpp/absl/container/flat_hash_map.h"

namespace variations {
namespace {

using LayerByIdMap = absl::flat_hash_map<uint32_t, raw_ptr<const Layer>>;

void LogSeedRejectionReason(SeedRejectionReason reason) {
  base::UmaHistogramEnumeration(kSeedRejectionReasonHistogram, reason);
}

// Builds a map of layers by id from the given seed, logging the seed rejection
// reason if the seed is invalid.
std::optional<LayerByIdMap> BuildLayerByIdMap(const VariationsSeed& seed) {
  LayerByIdMap layer_by_id_map;
  layer_by_id_map.reserve(seed.layers_size());
  for (const Layer& layer : seed.layers()) {
    if (layer.id() == 0) {
      LogSeedRejectionReason(SeedRejectionReason::kInvalidLayerId);
      return std::nullopt;
    }
    if (layer.num_slots() == 0) {
      LogSeedRejectionReason(SeedRejectionReason::kLayerDoesNotContainSlots);
      return std::nullopt;
    }
    if (!VariationsLayers::AreSlotBoundsValid(layer)) {
      LogSeedRejectionReason(SeedRejectionReason::kLayerHasInvalidSlotBounds);
      return std::nullopt;
    }
    if (!layer_by_id_map.emplace(layer.id(), &layer).second) {
      LogSeedRejectionReason(SeedRejectionReason::kDuplicatedLayerId);
      return std::nullopt;
    }
  }
  return layer_by_id_map;
}

// Returns true if the study references a layer.
bool HasLayerReference(const Study& study) {
  return study.has_layer();
}

// Returns the layer referenced by the study, or nullptr if the layer member
// reference is invalid, logging the seed rejection reason.
//
// A layer member reference is invalid if:
//
// * The layer id of the reference is zero.
// * No layer is defined having the referenced layer id.
// * A layer member referenced by the study is not defined in the layer.
const Layer* FindLayerForStudy(const LayerByIdMap& layer_by_id_map,
                               const Study& study) {
  const auto& ref = study.layer();
  if (ref.layer_id() == 0) {
    LogSeedRejectionReason(SeedRejectionReason::kInvalidLayerReference);
    return nullptr;
  }
  const auto& layer_member_ids =
      ref.layer_member_ids().empty()
          ? VariationsLayers::FallbackLayerMemberIds(ref)
          : ref.layer_member_ids();
  if (layer_member_ids.empty()) {
    LogSeedRejectionReason(SeedRejectionReason::kEmptyLayerReference);
    return nullptr;
  }
  const auto iter = layer_by_id_map.find(ref.layer_id());
  if (iter == layer_by_id_map.end()) {
    LogSeedRejectionReason(SeedRejectionReason::kDanglingLayerReference);
    return nullptr;
  }
  const Layer* layer = iter->second;
  for (const uint32_t member_id : layer_member_ids) {
    if (!base::Contains(layer->members(), member_id, &Layer::LayerMember::id)) {
      LogSeedRejectionReason(
          SeedRejectionReason::kDanglingLayerMemberReference);
      return nullptr;
    }
  }
  return layer;
}

// Returns true if the layer is a limited layer.
bool IsLimitedLayer(const Layer& layer) {
  return layer.entropy_mode() == Layer::LIMITED;
}

// Returns true if the study applies to the client's platform.
bool AppliesToClientPlatform(const Study& study,
                             const ClientFilterableState& client_state) {
  return internal::CheckStudyPlatform(study.filter(), client_state.platform);
}

// Returns true if the study applies to the client's channel.
bool AppliesToClientChannel(const Study& study,
                            const ClientFilterableState& client_state) {
  return internal::CheckStudyChannel(study.filter(), client_state.channel);
}

// Returns true if the study applies to the client's version.
bool AppliesToClientVersion(const Study& study,
                            const ClientFilterableState& client_state) {
  return internal::CheckStudyVersion(study.filter(), client_state.version);
}

}  // namespace

double GetGoogleWebEntropyLimitInBits() {
  // TODO(crbug.com/422222582): Update this to platform-specific launch values.
  return 1.0;
}

// TODO(crbug.com/428216544): Refactor, along with variations_layers.cc, to
// consolidate the logic for checking the layer configuration in the seed.
bool SeedHasMisconfiguredEntropy(const ClientFilterableState& client_state,
                                 const VariationsSeed& seed,
                                 double entropy_limit_in_bits) {
  std::optional<LayerByIdMap> layer_by_id_map = BuildLayerByIdMap(seed);
  if (!layer_by_id_map.has_value()) {
    // Seed rejection reason already logged.
    return true;
  }
  // We don't know which layer is the active limited layer for the client's
  // platform and channel. We'll set up the active limited layer and the entropy
  // tracker once we find the first relevant study.
  const Layer* active_limited_layer = nullptr;
  std::optional<LimitedLayerEntropyCostTracker> entropy_tracker;
  for (const Study& study : seed.study()) {
    if (!HasLayerReference(study)) {
      continue;
    }
    const Layer* current_layer = FindLayerForStudy(*layer_by_id_map, study);
    if (!current_layer) {
      // Seed rejection reason already logged.
      return true;
    }
    if (!IsLimitedLayer(*current_layer) ||
        !AppliesToClientPlatform(study, client_state) ||
        !AppliesToClientChannel(study, client_state) ||
        !AppliesToClientVersion(study, client_state)) {
      continue;
    }
    // Update the active limited layer and the entropy tracker or ensure that
    // the active limited layer matches the current layer.
    if (active_limited_layer == nullptr) {
      active_limited_layer = current_layer;
      entropy_tracker.emplace(*active_limited_layer, entropy_limit_in_bits);
      if (!entropy_tracker->IsValid()) {
        // The entropy tracker may have been invalidated by the layer config.
        LogSeedRejectionReason(SeedRejectionReason::kInvalidLayerConfiguration);
        return true;
      }
    } else if (active_limited_layer != current_layer) {
      LogSeedRejectionReason(SeedRejectionReason::kMoreThenOneLimitedLayer);
      return true;
    }
    if (!entropy_tracker->AddEntropyUsedByStudy(study)) {
      // The entropy tracker may have been invalidated by the study config, or
      // the entropy limit may have been exceeded.
      LogSeedRejectionReason(
          entropy_tracker->IsValid()
              ? SeedRejectionReason::kHighEntropyUsage
              : SeedRejectionReason::kInvalidLayerConfiguration);
      return true;
    }
  }

  // No entropy or layer issues found.
  return false;
}

}  // namespace variations
