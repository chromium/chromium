// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_VARIATIONS_ENTROPY_PROVIDER_H_
#define COMPONENTS_VARIATIONS_ENTROPY_PROVIDER_H_

#include <stddef.h>
#include <stdint.h>

#include <cstdint>
#include <optional>
#include <string>
#include <string_view>

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
  // Creates a SHA1EntropyProvider with the given `entropy_source`, which
  // should contain a large amount of entropy - for example, a textual
  // representation of a persistent randomly-generated 128-bit value.
  explicit SHA1EntropyProvider(std::string_view entropy_source);

  SHA1EntropyProvider(const SHA1EntropyProvider&) = delete;
  SHA1EntropyProvider& operator=(const SHA1EntropyProvider&) = delete;

  ~SHA1EntropyProvider() override;

  // base::FieldTrial::EntropyProvider implementation:
  double GetEntropyForTrial(std::string_view trial_name,
                            uint32_t randomization_seed) const override;

 private:
  const std::string entropy_source_;
};

// A |value| in the range [0, range).
struct ValueInRange {
  uint32_t value;
  uint32_t range;
};

// NormalizedMurmurHashEntropyProvider is an entropy provider suitable for low
// entropy sources (below 16 bits). It uses MurmurHash3_32 to hash the study
// name along with all possible low entropy sources. It finds the index where
// the actual low entropy source's hash would fall in the sorted list of all
// those hashes, and uses that as the final value. For more info, see:
// https://docs.google.com/document/d/1cPF5PruriWNP2Z5gSkq4MBTm0wSZqLyIJkUO9ekibeo
//
// Note: this class should be kept consistent with
// NormalizedMurmurHashEntropyProvider on the Java side.
class COMPONENT_EXPORT(VARIATIONS) NormalizedMurmurHashEntropyProvider final
    : public base::FieldTrial::EntropyProvider {
 public:
  // Creates a provider with |entropy_value| in the range of possible values.
  explicit NormalizedMurmurHashEntropyProvider(ValueInRange entropy_value);

  NormalizedMurmurHashEntropyProvider(
      const NormalizedMurmurHashEntropyProvider&) = default;

  ~NormalizedMurmurHashEntropyProvider() override;

  // base::FieldTrial::EntropyProvider:
  double GetEntropyForTrial(std::string_view trial_name,
                            uint32_t randomization_seed) const override;

  uint32_t entropy_value() const { return entropy_value_.value; }
  uint32_t entropy_domain() const { return entropy_value_.range; }

 private:
  const ValueInRange entropy_value_;
};

class SessionEntropyProvider : public base::FieldTrial::EntropyProvider {
 public:
  SessionEntropyProvider() = default;
  ~SessionEntropyProvider() override;

  double GetEntropyForTrial(std::string_view trial_name,
                            uint32_t randomization_seed) const override;
};

class COMPONENT_EXPORT(VARIATIONS) EntropyProviders {
 public:
  // Construct providers from the given entropy sources. Note:
  //  - If `high_entropy_source` is empty, no high entropy provider is created.
  //  - A limited entropy provider is created when `limited_entropy_value` is a
  //    non-empty string. `limited_entropy_value` is a 128-bit GUID as a string.
  //    It is used to randomize any study that is constrained to a layer with
  //    LIMITED entropy mode.
  //  - The value of `enable_benchmarking` will be returned by
  //    benchmarking_enabled(). If true, the caller should suppress
  //    randomization.
  EntropyProviders(std::string_view high_entropy_value,
                   ValueInRange low_entropy_value,
                   std::string_view limited_entropy_value,
                   bool enable_benchmarking = false);

  EntropyProviders(const EntropyProviders&) = delete;
  EntropyProviders& operator=(const EntropyProviders&) = delete;
  virtual ~EntropyProviders();

  // Accessors are virtual for testing purposes.

  // Gets the high entropy source, if available, otherwise returns low entropy.
  virtual const base::FieldTrial::EntropyProvider& default_entropy() const;
  // Gets the low entropy source.
  virtual const base::FieldTrial::EntropyProvider& low_entropy() const;
  virtual const base::FieldTrial::EntropyProvider& session_entropy() const;

  // Returns the limited entropy provider.
  // NOTE: the caller should call has_limited_entropy() before calling this to
  // ensure the limited entropy provider is available. The limited entropy
  // provider can be constructed by supplying `limited_entropy_value` in the
  // constructor.
  virtual const base::FieldTrial::EntropyProvider& limited_entropy() const;

  bool default_entropy_is_high_entropy() const {
    return high_entropy_.has_value();
  }

  // Whether a limited entropy source was created. A limited entropy source can
  // be created by instantiating with the `limited_entropy_randomization_source`
  // parameter.
  bool has_limited_entropy() const { return limited_entropy_.has_value(); }

  size_t low_entropy_value() const { return low_entropy_.entropy_value(); }
  size_t low_entropy_domain() const { return low_entropy_.entropy_domain(); }

  bool benchmarking_enabled() const { return benchmarking_enabled_; }

 private:
  std::optional<SHA1EntropyProvider> high_entropy_;
  std::optional<SHA1EntropyProvider> limited_entropy_;
  NormalizedMurmurHashEntropyProvider low_entropy_;
  SessionEntropyProvider session_entropy_;
  bool benchmarking_enabled_;
};

}  // namespace variations

#endif  // COMPONENTS_VARIATIONS_ENTROPY_PROVIDER_H_
