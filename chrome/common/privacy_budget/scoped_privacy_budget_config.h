// Copyright 2020 The Chromium Authors. All rights reserved.
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

  // An expected surface count of one implies that the probability of selecting
  // a surface is 1/1.
  constexpr static int kDefaultExpectedSurfaceCount = 1;

  // These fields correspond to the equivalent features described in
  // privacy_budget_features.h
  //
  // The default values enable the identifiability study with a sampling rate of
  // 1, which means every surface is included in UKM reports.
  struct Parameters {
    Parameters();
    Parameters(const Parameters&);
    Parameters(Parameters&&);
    ~Parameters();

    bool enabled = true;
    int generation = kDefaultGeneration;

    IdentifiableSurfaceList blocked_surfaces;
    IdentifiableSurfaceTypeList blocked_types;
    int expected_surface_count = kDefaultExpectedSurfaceCount;
    int active_surface_budget = std::numeric_limits<int>::max();
    IdentifiableSurfaceCostMap per_surface_cost;
    IdentifiableSurfaceTypeCostMap per_type_cost;
    SurfaceSetEquivalentClassesList equivalence_classes;
    IdentifiableSurfaceBlocks blocks;
    std::vector<double> block_weights;
  };

  enum Presets {
    // Represents the default state of `Parameters` which enables the study with
    // the default Parameters values.
    kEnable,

    // Disables the study. The other parameters are undefined and should not be
    // relied upon.
    kDisable
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
