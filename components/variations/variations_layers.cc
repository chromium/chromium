// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/variations/variations_layers.h"

#include <stddef.h>
#include <stdint.h>
#include <cstdint>
#include <memory>
#include <type_traits>

#include "base/check_op.h"
#include "base/metrics/field_trial.h"
#include "base/metrics/histogram_functions.h"
#include "base/numerics/checked_math.h"
#include "components/variations/entropy_provider.h"

namespace variations {

namespace {

void LogInvalidLayerReason(InvalidLayerReason reason) {
  base::UmaHistogramEnumeration("Variations.InvalidLayerReason", reason);
}

// Iterates through the members of the given layer proto definition, and
// returns the member which contains that slot (if any).
const Layer::LayerMember* FindActiveMemberBySlot(uint32_t chosen_slot,
                                                 const Layer& layer_proto) {
  for (const Layer::LayerMember& member : layer_proto.members()) {
    if (!member.id())
      continue;

    for (const Layer::LayerMember::SlotRange& slot : member.slots()) {
      if (slot.start() <= chosen_slot && chosen_slot <= slot.end())
        return &member;
    }
  }
  return nullptr;
}

// The result of SelectSlot.
struct SlotSelection {
  // The slot selected.
  ValueInRange slot;
  // The remainder after dividing pseudorandom range to slots.
  ValueInRange pseudorandom_remainder;
};

SlotSelection SelectSlot(ValueInRange pseudorandom, uint32_t num_slots) {
  DCHECK_GT(pseudorandom.range, 0u);
  DCHECK_GT(num_slots, 0u);
  DCHECK_EQ(pseudorandom.range % num_slots, 0u);
  // Since range and num_slots are both non-zero, and num_slots is a divisor of
  // range, slot_size is also guaranteed to be non-zero.
  uint32_t slot_size = pseudorandom.range / num_slots;
  return {
      .slot =
          {
              .value = pseudorandom.value / slot_size,
              .range = num_slots,
          },
      .pseudorandom_remainder =
          {
              .value = pseudorandom.value % slot_size,
              .range = slot_size,
          },
  };
}

ValueInRange CombineRanges(ValueInRange major, ValueInRange minor) {
  return {
      .value = major.value * minor.range + minor.value,
      .range = major.range * minor.range,
  };
}

ValueInRange SlotOfMember(const Layer::LayerMember& chosen_member,
                          uint32_t chosen_slot) {
  uint32_t slots_in_member = 0;
  uint32_t slots_in_member_less_than_chosen_slot = 0;
  for (const Layer::LayerMember::SlotRange& range : chosen_member.slots()) {
    const uint32_t range_size = range.end() - range.start() + 1;
    slots_in_member += range_size;
    if (chosen_slot > range.end()) {
      slots_in_member_less_than_chosen_slot += range_size;
    } else if (chosen_slot > range.start()) {
      slots_in_member_less_than_chosen_slot += chosen_slot - range.start();
    }
  }
  return {
      .value = slots_in_member_less_than_chosen_slot,
      .range = slots_in_member,
  };
}

// Computes a new entropy provider that can be used for uniform low-entropy
// randomization of studies in the layer member.
//
// The concept here is that the layer "divides" the pseudorandom range into
// different members, where "which member" is the "quotient", and now we are
// extracting the "remainder" of that division (as well as the range of the
// remainder, which will be the domain of the new provider).
//
// We define the remainder more specifically as the number of values in the
// pseudorandom function's range which give the same quotient (member) which are
// less than the given pseudorandom value. This makes the range of the
// remainder be the number of values in the range that map to the member.
//
// For example if |range| is [0,10) and we have a layer with 5 slots, and
// member M that contains slots 0 and 3, then there are 4 values in |range|
// that will activate that member [0,1,6,7], so the |remainder.range| will be 4.
// If |pseudorandom.value| is 7, then [0,1,6] are less than 7, so the
// |remainder.value| will be 3.
//
// The remainder is undefined for values not actually selected by the member,
// and this function should not be called with a chosen slot that is not in
// the member.
NormalizedMurmurHashEntropyProvider ComputeRemainderEntropy(
    const Layer::LayerMember& chosen_member,
    SlotSelection selection) {
  ValueInRange slot_of_member =
      SlotOfMember(chosen_member, selection.slot.value);
  ValueInRange remainder =
      CombineRanges(slot_of_member, selection.pseudorandom_remainder);
  return NormalizedMurmurHashEntropyProvider(remainder);
}

// Selects the entropy provider based on the entropy mode of the layer. Note
// that the caller bears the responsibility of checking with a limited entropy
// provider exists before calling this function.
const base::FieldTrial::EntropyProvider& SelectEntropyProvider(
    const EntropyProviders& entropy_providers,
    const Layer::EntropyMode& entropy_mode) {
  if (entropy_mode == Layer::LIMITED) {
    return entropy_providers.limited_entropy();
  } else if (entropy_mode == Layer::LOW) {
    return entropy_providers.low_entropy();
  } else {
    return entropy_providers.default_entropy();
  }
}

}  // namespace

VariationsLayers::VariationsLayers(const VariationsSeed& seed,
                                   const EntropyProviders& entropy_providers)
    : nil_entropy({0, 1}) {
  // Don't activate any layer-constrained studies in benchmarking mode to
  // maintain deterministic behavior.
  if (entropy_providers.benchmarking_enabled())
    return;

  std::map<uint32_t, int> counts_by_id;
  for (const Layer& layer_proto : seed.layers()) {
    ++counts_by_id[layer_proto.id()];
    // Avoid multiple logs if one ID is used multiple times.
    if (counts_by_id[layer_proto.id()] == 2) {
      LogInvalidLayerReason(InvalidLayerReason::LayerIDNotUnique);
    };
  }

  // TODO(crbug.com/1154033): Support a way to expire old/unused layers so they
  // no longer get processed by the clients.
  for (const Layer& layer_proto : seed.layers()) {
    // Only constructs a layer if its ID is unique. We want to discard all
    // layers with the same ID because changing layer ID re-randomizes the field
    // trials that reference it (if the layer doesn't have a salt. See
    // ConstructLayer()).
    const bool is_layer_id_unique = counts_by_id[layer_proto.id()] == 1;
    if (is_layer_id_unique) {
      ConstructLayer(entropy_providers, layer_proto);
    }
  }
}

VariationsLayers::VariationsLayers() : nil_entropy({0, 1}) {}

VariationsLayers::~VariationsLayers() = default;

void VariationsLayers::ConstructLayer(const EntropyProviders& entropy_providers,
                                      const Layer& layer_proto) {
  if (!layer_proto.unknown_fields().empty()) {
    LogInvalidLayerReason(InvalidLayerReason::kUnknownFields);
    return;
  }
  if (layer_proto.id() == 0) {
    LogInvalidLayerReason(InvalidLayerReason::kInvalidId);
    return;
  }
  if (layer_proto.num_slots() == 0) {
    LogInvalidLayerReason(InvalidLayerReason::kNoSlots);
    return;
  }
  if (layer_proto.members_size() == 0) {
    LogInvalidLayerReason(InvalidLayerReason::kNoMembers);
    return;
  }

  if (layer_proto.entropy_mode() != Layer::LOW &&
      layer_proto.entropy_mode() != Layer::DEFAULT &&
      layer_proto.entropy_mode() != Layer::LIMITED) {
    LogInvalidLayerReason(InvalidLayerReason::kInvalidEntropyMode);
    return;
  }

  // There must be a limited entropy provider when processing a limited layer. A
  // limited entropy provider does not exist for an ineligible platform (e.g.
  // WebView), or if the client is not in the enabled group of the limited
  // entropy synthetic trial.
  // TODO(crbug.com/1508150): clean up the synthetic trial after it has
  // completed.
  if (layer_proto.entropy_mode() == Layer::LIMITED &&
      !entropy_providers.has_limited_entropy()) {
    LogInvalidLayerReason(InvalidLayerReason::kLimitedLayerDropped);
    return;
  }

  // Using the size of the domain as the output range maximizes the number of
  // possible pseudorandom outputs when using the low entropy source.
  size_t range = entropy_providers.low_entropy_domain();
  if (range % layer_proto.num_slots() != 0) {
    // We can't support uniform selection on layers with a slot count that
    // doesn't divide the low entropy range, so don't support them at all.
    LogInvalidLayerReason(
        InvalidLayerReason::kSlotsDoNotDivideLowEntropyDomain);
    return;
  }

  if (!AreSlotBoundsValid(layer_proto)) {
    LogInvalidLayerReason(InvalidLayerReason::kInvalidSlotBounds);
    return;
  }

  const auto& entropy_provider =
      SelectEntropyProvider(entropy_providers, layer_proto.entropy_mode());
  uint32_t salt = layer_proto.salt() ? layer_proto.salt() : layer_proto.id();
  ValueInRange pseudorandom = {
      .value = entropy_provider.GetPseudorandomValue(salt, range),
      .range = static_cast<uint32_t>(range),
  };
  SlotSelection selection = SelectSlot(pseudorandom, layer_proto.num_slots());
  const auto* chosen_member =
      FindActiveMemberBySlot(selection.slot.value, layer_proto);
  if (!chosen_member) {
    // No member is active for the chosen slot.
    return;
  }

  // Store the active member info, along with the remainder entropy.
  active_member_for_layer_.emplace(
      layer_proto.id(), LayerInfo{
                            .active_member_id = chosen_member->id(),
                            .entropy_mode = layer_proto.entropy_mode(),
                            .remainder_entropy = ComputeRemainderEntropy(
                                *chosen_member, selection),
                        });
}

const VariationsLayers::LayerInfo* VariationsLayers::FindActiveLayer(
    uint32_t layer_id) const {
  auto layer_iter = active_member_for_layer_.find(layer_id);
  if (layer_iter == active_member_for_layer_.end()) {
    return nullptr;
  }
  return &(layer_iter->second);
}

// static
bool VariationsLayers::AreSlotBoundsValid(const Layer& layer_proto) {
  for (const auto& member : layer_proto.members()) {
    uint32_t next_slot_after_processed_ranges = 0;
    for (const auto& range : member.slots()) {
      // Ranges should be non-overlapping. We also require them to be in
      // increasing order so that we can easily validate that they are not
      // overlapping.
      if (range.start() < next_slot_after_processed_ranges) {
        return false;
      }

      static_assert(std::is_same<decltype(range.start()), uint32_t>::value,
                    "range start of a layer member must be an unsigned number");
      static_assert(std::is_same<decltype(range.end()), uint32_t>::value,
                    "range end of a layer member must be an unsigned number");
      // Since `range.start()` and `range.end()` are both unsigned (uint32_t),
      // there is no need to check that they are non-negative.
      if (range.end() >= layer_proto.num_slots()) {
        return false;
      }
      if (range.start() > range.end()) {
        return false;
      }

      // Note this won't overflow because the above if-clauses ensures
      // `range.end() < layer_proto.num_slots()`. Therefore `range.end()` is not
      // the max representable uint32_t. Will CHECK if it expectedly overflows.
      next_slot_after_processed_ranges =
          base::CheckAdd(range.end(), 1).ValueOrDie();
    }
  }
  return true;
}

bool VariationsLayers::IsLayerActive(uint32_t layer_id) const {
  return FindActiveLayer(layer_id) != nullptr;
}

bool VariationsLayers::IsLayerMemberActive(uint32_t layer_id,
                                           uint32_t member_id) const {
  const auto* layer_info = FindActiveLayer(layer_id);
  if (layer_info == nullptr) {
    return false;
  }
  return layer_info->active_member_id &&
         member_id == layer_info->active_member_id;
}

bool VariationsLayers::ActiveLayerMemberDependsOnHighEntropy(
    uint32_t layer_id) const {
  const auto* layer_info = FindActiveLayer(layer_id);
  if (layer_info == nullptr) {
    return false;
  }
  return layer_info->entropy_mode == Layer::DEFAULT;
}

const base::FieldTrial::EntropyProvider& VariationsLayers::GetRemainderEntropy(
    uint32_t layer_id) const {
  const auto* layer_info = FindActiveLayer(layer_id);
  if (layer_info == nullptr) {
    // TODO(crbug.com/1519262): Remove CreateTrialsForStudy fuzzer, then
    // uncomment this.
    // NOTREACHED();
    return nil_entropy;
  }
  return layer_info->remainder_entropy;
}

}  // namespace variations
