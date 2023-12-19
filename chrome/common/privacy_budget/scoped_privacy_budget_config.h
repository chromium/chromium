// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_COMMON_PRIVACY_BUDGET_SCOPED_PRIVACY_BUDGET_CONFIG_H_
#define CHROME_COMMON_PRIVACY_BUDGET_SCOPED_PRIVACY_BUDGET_CONFIG_H_

#include <limits>
#include <map>
#include <vector>

#include "base/test/scoped_feature_list.h"
#include "chrome/common/privacy_budget/types.h"
#include "third_party/blink/public/common/privacy_budget/identifiable_surface.h"

namespace test {

// Configures the privacy budget feature settings for testing. One can also
// configure the service manually by setting the global feature lists
// directly. This class is merely a convenience.
//
// For testing only.
//
// Note 1: Since we are changing feature lists, the same caveats as
//     base::test::ScopedFeatureList apply. E.g. The Apply() method should be
//     called prior to starting the threads in multi threaded environments.
//
// Note 2: The configuration applied by this class is *scoped*. The destructor
//     reverts the configuration changes.
class ScopedPrivacyBudgetConfig {
 public:
  // The default generation is arbitrary. The only thing special about this
  // number is that it is the default.
  constexpr static int kDefaultGeneration = 17;

  enum class Presets {
    // Enables the study with random sampling and with a probability of 1 of
    // selecting each surface.
    kEnableRandomSampling,

    // Disables the study. The other parameters are undefined and should not be
    // relied upon.
    kDisable
  };

  // These fields correspond to the equivalent features described in
  // privacy_budget_features.h
  struct Parameters {
    Parameters();
    explicit Parameters(Presets);
    Parameters(const Parameters&);
    Parameters(Parameters&&);
    ~Parameters();

    bool enabled = true;
    int generation = kDefaultGeneration;

    IdentifiableSurfaceList blocked_surfaces;
    IdentifiableSurfaceTypeList blocked_types;
    int expected_surface_count = 0;
    int active_surface_budget = std::numeric_limits<int>::max();
    IdentifiableSurfaceCostMap per_surface_cost;
    IdentifiableSurfaceTypeCostMap per_type_cost;
    SurfaceSetEquivalentClassesList equivalence_classes;
    IdentifiableSurfaceBlocks blocks;
    std::vector<double> block_weights;
    std::vector<blink::IdentifiableSurface::Type> allowed_random_types;
  };

  // Doesn't do anything until Apply() is called.
  ScopedPrivacyBudgetConfig();

  // Applies the configuration indicated by `preset`.
  explicit ScopedPrivacyBudgetConfig(Presets preset);

  // Constructs and applies the configuration described in `parameters`. No need
  // to call Apply()
  explicit ScopedPrivacyBudgetConfig(const Parameters& parameters);

  ~ScopedPrivacyBudgetConfig();

  // Apply the configuration as described in `parameters`. Should only be called
  // once per instance.
  void Apply(const Parameters& parameters);

  ScopedPrivacyBudgetConfig(const ScopedPrivacyBudgetConfig&) = delete;
  ScopedPrivacyBudgetConfig& operator=(const ScopedPrivacyBudgetConfig&) =
      delete;

 private:
  base::test::ScopedFeatureList scoped_feature_list_;
  bool applied_ = false;
};

}  // namespace test

#endif  // CHROME_COMMON_PRIVACY_BUDGET_SCOPED_PRIVACY_BUDGET_CONFIG_H_
