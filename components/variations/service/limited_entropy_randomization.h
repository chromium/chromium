// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_VARIATIONS_SERVICE_LIMITED_ENTROPY_RANDOMIZATION_H_
#define COMPONENTS_VARIATIONS_SERVICE_LIMITED_ENTROPY_RANDOMIZATION_H_

#include <string_view>

// Provides functions to validate that the variations seed is
// correctly configured to respect an entropy limit. See below for details.
//
// This limit only applies to field trials configured to use the "limited
// entropy" layer – that is, a layer with `EntropyMode.LIMITED`. For brevity,
// documentation in this file will refer to this layer as the "limited layer".
// There is at most one limited layer in the seed with filters that are
// applicable to the client. For now, it's the server's responsibility to ensure
// this invariant. As an optimization, the client code should be updated to
// consider the filters when calculating entropy (TODO(b/319681288)).
//
// Consider each client's chosen groups across all studies which use limited
// entropy. While some group combinations may be more likely than others (based
// on group percentages), the combination with the minimum probability must have
// a probability above an entropy limit we define (see below). For brevity,
// documentation in this file will refer to information revealed by these chosen
// groups as "entropy".
//
// The entropy limit defined here is analogous to the "low entropy source" used
// elsewhere in the variations codebase, but uses a different implementation
// approach to achieve the result of limiting the total entropy.
//
// See https://en.wikipedia.org/wiki/Entropy_(information_theory) for more
// information about "entropy" as a mathematical concept.
namespace variations {

class VariationsSeed;
struct ClientFilterableState;

// TODO(crbug.com/428216544): Unify with the other existing seed rejection
// reasons. These values are persisted to logs. Once launched, entries should
// not be renumbered and numeric values should not be reused.
enum class SeedRejectionReason {
  kHighEntropyUsage = 0,
  kMoreThenOneLimitedLayer = 1,
  kLayerHasInvalidSlotBounds = 2,
  kLayerDoesNotContainSlots = 3,
  kInvalidLayerId = 4,
  kDuplicatedLayerId = 5,
  kInvalidLayerReference = 6,
  kDanglingLayerReference = 7,
  kDanglingLayerMemberReference = 8,
  kEmptyLayerReference = 9,
  kInvalidLayerConfiguration = 10,
  kMaxValue = kInvalidLayerConfiguration,
};

// The histogram name for the seed rejection reason.
inline constexpr std::string_view kSeedRejectionReasonHistogram =
    "Variations.LimitedEntropy.SeedRejectionReason";

// The maximum amount of total entropy, in bits, for field trials with Google
// web experiment ids.
//
// The cumulative probability of group assignments across all such field trials
// on the client must be at least 1 / (2 ^ GetGoogleWebEntropyLimitInBits()).
double GetGoogleWebEntropyLimitInBits();

// Returns true if the entropy from the variations seed is misconfigured, or
// entropy cannot be computed. If this returns true, the caller is expected to
// reject the seed.
//
// * client_state: The client state to use for filtering studies.
// * seed: The seed to check for misconfigured entropy.
// * entropy_limit_in_bits: The entropy limit to use for checking. Exposed for
//     testing. Should be set to GetGoogleWebEntropyLimitInBits() in production.
bool SeedHasMisconfiguredEntropy(
    const ClientFilterableState& client_state,
    const VariationsSeed& seed,
    double entropy_limit_in_bits = GetGoogleWebEntropyLimitInBits());

}  // namespace variations

#endif  // COMPONENTS_VARIATIONS_SERVICE_LIMITED_ENTROPY_RANDOMIZATION_H_
