// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_VARIATIONS_SERVICE_LIMITED_ENTROPY_RANDOMIZATION_H_
#define COMPONENTS_VARIATIONS_SERVICE_LIMITED_ENTROPY_RANDOMIZATION_H_

// This file provides functions to validate that the variations seed is
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

class Layer;
class VariationsLayers;
class VariationsSeed;

// The maximum amount of total entropy, in bits, for field trials with Google
// web experiment ids.
//
// Precisely, the cumulative probability of group assignments across all such
// field trials on the client must be at least 1 / (2 ^
// `kGoogleWebEntropyLimitInBits`). This constant is expressed as a double so
// that any such fraction can be represented. A limit of 1 bit means at a
// minimum 50% of clients will have the same group assignment combinations
// across studies referencing the limited layer. This setting is used during the
// time when `LimitedEntropySyntheticTrial` is active to control the number of
// studies using the limited entropy mode.
// TODO(crbug.com/40948861): Adjust this limit after the synthetic trial
// concludes.
inline constexpr double kGoogleWebEntropyLimitInBits = 1.0;

// Returns true iff the entropy from the field trials in the seed is
// misconfigured, or entropy cannot be computed. The caller is expected to
// handle the return value and reject the seed if necessary.
bool SeedHasMisconfiguredEntropy(const VariationsLayers& layers,
                                 const VariationsSeed& seed);

// A test-only accessor for a function that implements the entropy calculations,
// to facilitate targeted testing. This allows tests to validate the entropy
// calculation logic, independently of the value of
// `kGoogleWebEntropyLimitInBits`.
double GetEntropyUsedByLimitedLayerForTesting(const Layer& limited_layer,
                                              const VariationsSeed& seed);

}  // namespace variations

#endif  // COMPONENTS_VARIATIONS_SERVICE_LIMITED_ENTROPY_RANDOMIZATION_H_
