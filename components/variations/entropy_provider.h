// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_VARIATIONS_ENTROPY_PROVIDER_H_
#define COMPONENTS_VARIATIONS_ENTROPY_PROVIDER_H_

#include <stddef.h>
#include <stdint.h>

#include <string>

#include "base/component_export.h"
#include "base/metrics/field_trial.h"
#include "third_party/abseil-cpp/absl/types/optional.h"

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
class COMPONENT_EXPORT(VARIATIONS) NormalizedMurmurHashEntropyProvider final
    : public base::FieldTrial::EntropyProvider {
 public:
  // Creates a provider with |entropy_value| in the domain [0, entropy_domain).
  NormalizedMurmurHashEntropyProvider(uint16_t entropy_value,
                                      size_t entropy_domain);

  NormalizedMurmurHashEntropyProvider(
      const NormalizedMurmurHashEntropyProvider&) = default;

  ~NormalizedMurmurHashEntropyProvider() override;

  // base::FieldTrial::EntropyProvider:
  double GetEntropyForTrial(base::StringPiece trial_name,
                            uint32_t randomization_seed) const override;

  size_t entropy_domain() const { return entropy_domain_; }

 private:
  const uint16_t entropy_value_;
  const size_t entropy_domain_;
};

class COMPONENT_EXPORT(VARIATIONS) EntropyProviders {
 public:
  // Construct providers from the given entropy sources.
  // If |high_entropy_source| is empty, no high entropy provider is created.
  EntropyProviders(const std::string& high_entropy_value,
                   uint16_t low_entropy_value,
                   size_t low_entropy_domain);
  EntropyProviders(const EntropyProviders&) = delete;
  EntropyProviders& operator=(const EntropyProviders&) = delete;
  virtual ~EntropyProviders();

  // Accessors are virtual for testing purposes.

  // Gets the high entropy source, if available, otherwise returns low entropy.
  virtual const base::FieldTrial::EntropyProvider& default_entropy() const;
  // Gets the low entropy source.
  virtual const base::FieldTrial::EntropyProvider& low_entropy() const;

  bool default_entropy_is_high_entropy() const {
    return high_entropy_.has_value();
  }

  size_t low_entropy_domain() const { return low_entropy_.entropy_domain(); }

 private:
  absl::optional<SHA1EntropyProvider> high_entropy_;
  NormalizedMurmurHashEntropyProvider low_entropy_;
};

}  // namespace variations

#endif  // COMPONENTS_VARIATIONS_ENTROPY_PROVIDER_H_
