// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/variations/service/limited_entropy_randomization.h"

#include <math.h>

#include <cstdint>
#include <limits>

#include "base/check_op.h"
#include "base/containers/contains.h"
#include "base/debug/crash_logging.h"
#include "base/debug/dump_without_crashing.h"
#include "base/logging.h"
#include "base/metrics/histogram_functions.h"
#include "base/numerics/checked_math.h"
#include "base/numerics/safe_conversions.h"
#include "base/rand_util.h"
#include "base/version_info/version_info.h"
#include "build/build_config.h"
#include "components/variations/client_filterable_state.h"
#include "components/variations/limited_layer_entropy_cost_tracker.h"
#include "components/variations/study_filtering.h"
#include "components/variations/variations_layers.h"
#include "components/variations/variations_seed_processor.h"
#include "third_party/abseil-cpp/absl/container/flat_hash_map.h"

#define SR_CRASH_KEY "SeedRejection"

namespace variations {
namespace {

using LayerByIdMap = absl::flat_hash_map<uint32_t, raw_ptr<const Layer>>;

void LogSeedRejectionReason(SeedRejectionReason reason) {
  base::UmaHistogramEnumeration(kSeedRejectionReasonHistogram, reason);
  SCOPED_CRASH_KEY_NUMBER(SR_CRASH_KEY, "reason", static_cast<int>(reason));

  // TODO: crbug.com/442498684 - Temporarily sampled due to noisiness during
  // debugging. Remove sampling once hitting this codepath is expected to be
  // rare.
  constexpr double kSampleRate = 0.001;
  if (base::RandDouble() < kSampleRate) {
    base::debug::DumpWithoutCrashing();
  }
}

// Builds a map of layers by id from the given seed, logging the seed rejection
// reason if the seed is invalid.
std::optional<LayerByIdMap> BuildLayerByIdMap(const VariationsSeed& seed) {
  LayerByIdMap layer_by_id_map;
  layer_by_id_map.reserve(seed.layers_size());
  for (const Layer& layer : seed.layers()) {
    SCOPED_CRASH_KEY_NUMBER(SR_CRASH_KEY, "layer_id", layer.id());
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
  const auto ref_layer_id = ref.layer_id();
  SCOPED_CRASH_KEY_NUMBER(SR_CRASH_KEY, "ref_layer_id", ref_layer_id);
  if (ref_layer_id == 0) {
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
  const auto iter = layer_by_id_map.find(ref_layer_id);
  if (iter == layer_by_id_map.end()) {
    LogSeedRejectionReason(SeedRejectionReason::kDanglingLayerReference);
    return nullptr;
  }
  const Layer* layer = iter->second;
  for (const uint32_t member_id : layer_member_ids) {
    if (!base::Contains(layer->members(), member_id, &Layer::LayerMember::id)) {
      SCOPED_CRASH_KEY_NUMBER(SR_CRASH_KEY, "ref_member_id", member_id);
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

// Returns true if the layer is a low entropy layer.
bool IsLowEntropyLayer(const Layer& layer) {
  return layer.entropy_mode() == Layer::LOW;
}

// Returns true if the study consumes entropy. This is true if the study has
// permanent consistency and uses experiment ids.
bool ConsumesEntropy(const Study& study) {
  if (study.consistency() != Study::PERMANENT) {
    return false;
  }
  for (const auto& experiment : study.experiment()) {
    if (experiment.has_google_web_experiment_id() ||
        experiment.has_google_web_trigger_experiment_id() ||
        experiment.has_google_app_experiment_id()) {
      return true;
    }
  }
  return false;
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

// Returns true if the study applies to the client's form factor.
bool AppliesToClientFormFactor(const Study& study,
                               const ClientFilterableState& client_state) {
  return internal::CheckStudyFormFactor(study.filter(),
                                        client_state.form_factor);
}

}  // namespace

double GetGoogleWebEntropyLimitInBits() {
#if BUILDFLAG(IS_ANDROID)
  return 21.0;
#elif BUILDFLAG(IS_IOS) || BUILDFLAG(IS_WIN)
  return 18.0;
#elif BUILDFLAG(IS_MAC) || BUILDFLAG(IS_CHROMEOS)
  return 16.0;
#else
  return 1.0;
#endif
}

// TODO(crbug.com/428216544): Refactor, along with variations_layers.cc, to
// consolidate the logic for checking the layer configuration in the seed.
MisconfiguredEntropyResult SeedHasMisconfiguredEntropy(
    const ClientFilterableState& client_state,
    const VariationsSeed& seed,
    double entropy_limit_in_bits) {
  SCOPED_CRASH_KEY_STRING32(SR_CRASH_KEY, "seed_version", seed.version());
  SCOPED_CRASH_KEY_NUMBER(SR_CRASH_KEY, "entropy_limit", entropy_limit_in_bits);

  std::optional<LayerByIdMap> layer_by_id_map = BuildLayerByIdMap(seed);
  if (!layer_by_id_map.has_value()) {
    // Seed rejection reason already logged.
    return MisconfiguredEntropyResult{.is_misconfigured = true};
  }
  // We don't know which layer is the active limited layer for the client's
  // platform and channel. We'll set up the active limited layer and the entropy
  // tracker once we find the first relevant study. We'll also track whether
  // there's an active low entropy layer and whether there are any legacy,
  // non-layer-constrained, low-entropy studies.
  const Layer* active_limited_layer = nullptr;
  const Layer* active_low_layer = nullptr;
  size_t num_legacy_studies = 0;
  std::optional<LimitedLayerEntropyCostTracker> entropy_tracker;
  for (const Study& study : seed.study()) {
    SCOPED_CRASH_KEY_STRING256(SR_CRASH_KEY, "study_name", study.name());
    SCOPED_CRASH_KEY_NUMBER(
        SR_CRASH_KEY, "active_limited_layer",
        active_limited_layer ? active_limited_layer->id() : 0);
    SCOPED_CRASH_KEY_NUMBER(SR_CRASH_KEY, "active_low_layer",
                            active_low_layer ? active_low_layer->id() : 0);

    if (!AppliesToClientPlatform(study, client_state) ||
        !AppliesToClientChannel(study, client_state) ||
        !AppliesToClientVersion(study, client_state) ||
        !AppliesToClientFormFactor(study, client_state)) {
      continue;
    }

    if (!HasLayerReference(study)) {
      if (ConsumesEntropy(study)) {
        num_legacy_studies++;
      }
      continue;
    }

    const Layer* current_layer = FindLayerForStudy(*layer_by_id_map, study);
    if (!current_layer) {
      // Seed rejection reason already logged.
      return MisconfiguredEntropyResult{.is_misconfigured = true};
    }
    // Could this be an active low entropy layer?
    if (IsLowEntropyLayer(*current_layer)) {
      if (ConsumesEntropy(study)) {
        active_low_layer = current_layer;
      }
      continue;
    }

    // Skip non-limited layers (for example, DEFAULT layers).
    if (!IsLimitedLayer(*current_layer)) {
      continue;
    }

    SCOPED_CRASH_KEY_NUMBER(SR_CRASH_KEY, "current_layer", current_layer->id());

    // Update the active limited layer and the entropy tracker or ensure that
    // the active limited layer matches the current layer.
    if (active_limited_layer == nullptr) {
      active_limited_layer = current_layer;
      entropy_tracker.emplace(*active_limited_layer, entropy_limit_in_bits);
      if (!entropy_tracker->IsValid()) {
        // The entropy tracker may have been invalidated by the layer config.
        LogSeedRejectionReason(SeedRejectionReason::kInvalidLayerConfiguration);
        return MisconfiguredEntropyResult{.is_misconfigured = true};
      }
    } else if (active_limited_layer != current_layer) {
      LogSeedRejectionReason(SeedRejectionReason::kMoreThenOneLimitedLayer);
      return MisconfiguredEntropyResult{.is_misconfigured = true};
    }
    if (!entropy_tracker->AddEntropyUsedByStudy(study)) {
      // The entropy tracker may have been invalidated by the study config, or
      // the entropy limit may have been exceeded.
      LogSeedRejectionReason(
          entropy_tracker->IsValid()
              ? SeedRejectionReason::kHighEntropyUsage
              : SeedRejectionReason::kInvalidLayerConfiguration);
      return MisconfiguredEntropyResult{.is_misconfigured = true};
    }
  }
  // Limited and low entropy systems should not be active at the same time.
  if (active_limited_layer && (active_low_layer || num_legacy_studies > 0)) {
    SCOPED_CRASH_KEY_NUMBER(SR_CRASH_KEY, "active_limited_layer",
                            active_limited_layer->id());
    SCOPED_CRASH_KEY_NUMBER(SR_CRASH_KEY, "active_low_layer",
                            active_low_layer ? active_low_layer->id() : 0);
    SCOPED_CRASH_KEY_NUMBER(SR_CRASH_KEY, "legacy_studies", num_legacy_studies);
    LogSeedRejectionReason(SeedRejectionReason::kActiveLowAndLimitedEntropy);
    return MisconfiguredEntropyResult{.is_misconfigured = true};
  }

  // No entropy or layer issues found.
  return MisconfiguredEntropyResult{
      .is_misconfigured = false,
      .seed_has_active_limited_layer = (active_limited_layer != nullptr),
      .seed_has_active_low_layer = (active_low_layer != nullptr)};
}

}  // namespace variations
