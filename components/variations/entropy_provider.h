// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_VARIATIONS_ENTROPY_PROVIDER_H_
#define COMPONENTS_VARIATIONS_ENTROPY_PROVIDER_H_

#include <stddef.h>
#include <stdint.h>

#include <functional>
#include <random>
#include <string>

#include "base/compiler_specific.h"
#include "base/component_export.h"
#include "base/metrics/field_trial.h"

namespace variations {

// SHA1EntropyProvider is an entropy provider suitable for high entropy sources.
// It works by taking the first 64 bits of the SHA1 hash of the entropy source
// concatenated with the trial name, or randomization seed and using that for
// the final entropy value.
class COMPONENT_EXPORT(VARIATIONS) SHA1EntropyProvider
    : public base::FieldTrial::EntropyProvider {
 public:
  // Creates a SHA1EntropyProvider with the given |entropy_source|, which
  // should contain a large amount of entropy - for example, a textual
  // representation of a persistent randomly-generated 128-bit value.
  explicit SHA1EntropyProvider(const std::string& entropy_source);

  SHA1EntropyProvider(const SHA1EntropyProvider&) = delete;
  SHA1EntropyProvider& operator=(const SHA1EntropyProvider&) = delete;

  ~SHA1EntropyProvider() override;

  // base::FieldTrial::EntropyProvider implementation:
  double GetEntropyForTrial(base::StringPiece trial_name,
                            uint32_t randomization_seed) const override;

 private:
  const std::string entropy_source_;
};

// NormalizedMurmurHashEntropyProvider is an entropy provider suitable for low
// entropy sources (below 16 bits). It uses MurmurHash3_32 to hash the study
// name along with all possible low entropy sources. It finds the index where
// the actual low entropy source's hash would fall in the sorted list of all
// those hashes, and uses that as the final value. For more info, see:
// https://docs.google.com/document/d/1cPF5PruriWNP2Z5gSkq4MBTm0wSZqLyIJkUO9ekibeo
class COMPONENT_EXPORT(VARIATIONS) NormalizedMurmurHashEntropyProvider
    : public base::FieldTrial::EntropyProvider {
 public:
  NormalizedMurmurHashEntropyProvider(uint16_t low_entropy_source,
                                      size_t low_entropy_source_max);

  NormalizedMurmurHashEntropyProvider(
      const NormalizedMurmurHashEntropyProvider&) = delete;
  NormalizedMurmurHashEntropyProvider& operator=(
      const NormalizedMurmurHashEntropyProvider&) = delete;

  ~NormalizedMurmurHashEntropyProvider() override;

  // base::FieldTrial::EntropyProvider:
  double GetEntropyForTrial(base::StringPiece trial_name,
                            uint32_t randomization_seed) const override;

 private:
  const uint16_t low_entropy_source_;
  const size_t low_entropy_source_max_;
};

}  // namespace variations

#endif  // COMPONENTS_VARIATIONS_ENTROPY_PROVIDER_H_
