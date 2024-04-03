// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_COMMON_PRIVACY_BUDGET_PRIVACY_BUDGET_FEATURES_H_
#define CHROME_COMMON_PRIVACY_BUDGET_PRIVACY_BUDGET_FEATURES_H_

#include <string>

#include "base/feature_list.h"
#include "base/metrics/field_trial_params.h"

namespace features {

// # Encoding of IdentifiableSurfaces:
//
// When used in fieldtrial parameters, nn IdentifiableSurface is encoded as the
// decimal representation of its ToUkmMetricHash() result. An
// IdentifiableSurface::Type is encoded as the decimal representation of the
// enum value.

// Feature for the Identifiability Study Meta experiment.
//
// If the feature is enabled, the meta experiment is activated on a random set
// of clients according to the selection probability.
BASE_DECLARE_FEATURE(kIdentifiabilityStudyMetaExperiment);

// Probability with which the meta experiment will be activated on each client.
//
// Parameter name: "ActivationProbability"
// Parameter type: double
//
// A value of the parameter outside the interval [0, 1] will be interpreted as
// the default probability, which depends on the channel and is encoded in
// chrome/browser/privacy_budget/identifiability_study_state.cc
extern const base::FeatureParam<double>
    kIdentifiabilityStudyMetaExperimentActivationProbability;

// Default activation probability for the Identifiability Study Meta Experiment.
// This is a value outside [0, 1], which will be replaced by the default
// probability depending on the channel in
// chrome/browser/privacy_budget/identifiability_study_state.cc
constexpr double
    kIdentifiabilityStudyMetaExperimentDefaultActivationProbability = -1;

// Root feature for all identifiability study logic.
//
// If the feature is disabled, then this browser instance will not be
// participating in the identifiability study. I.e. no identifiability metrics
// will be recorded or reported.
//
// Enabling the feature doesn't automatically make this client part of the study
// either.
BASE_DECLARE_FEATURE(kIdentifiabilityStudy);

// Each time the key study parameters change, the study generation also
// increments. Reporting the study generation alongside metrics allows the data
// from different study generations to be grouped independently.
//
// Changes to the study generation resets all persisted state for the
// identifiability study.
//
// Parameter name: "Gen"
// Parameter type: int
extern const base::FeatureParam<int> kIdentifiabilityStudyGeneration;

// Surfaces that should be excluded from reports.
//
// Parameter name: "BlockedHashes"
// Parameter type: Comma separated list of decimal integers, each of which
//     represents an IdentifiableSurface.
//
// When specifying these values on the command-line, the commas should be
// escaped using URL encoding. I.e. '1,2' -> '1%2C2'.
//
// E.g.:
//  * "258, 257" : Matches IdentifiableSurface::FromTypeAndToken(kWebFeature, 1)
//                 and IdentifiableSurface::FromTypeAndToken(kWebFeature, 2)
//
// From 03/2021 the code no longer supports revoking a surface that was once
// blocked either via "BlockedHashes" or "BlockedTypes".
extern const base::FeatureParam<std::string>
    kIdentifiabilityStudyBlockedMetrics;

// Surface types that should be excluded from reports.
//
// Parameter name: "BlockedTypes"
// Parameter type: Comma separated list of decimal integers, each of which
//                 represents an IdentifiableSurface::Type.
//
// When specifying these values on the command-line, the commas should be
// escaped using URL encoding. I.e. '1,2' -> '1%2C2'.
//
// E.g.:
//  * "1, 2" : Matches all surfaces with types kWebFeature and kCanvasReadback.
//
// From 03/2021 the code no longer supports revoking a surface that was once
// blocked either via "BlockedHashes" or "BlockedTypes".
extern const base::FeatureParam<std::string> kIdentifiabilityStudyBlockedTypes;

// Number of expected identifiable surfaces that will be sampled.
//
// Each site you visit implicitly or explicitly observes various identifiable
// bits of information. This is the number of potentially identifiable surfaces
// that we expect will be sampled per client.
//
// This parameter only affects random sampling of surfaces. Setting this to 0
// will disable random sampling.
//
// Parameter name: "Rho"
// Parameter type: int
extern const base::FeatureParam<int> kIdentifiabilityStudyExpectedSurfaceCount;

// Surface types allowed in the random assignment. If the parameter is empty or
// invalid all types can be sampled.

// This does not have any effect on meta surfaces (type 7), nor on reserved
// surfaces (type 0), which are sampled in any case. In particular, enabling
// random sampling and setting this parameter to "0" is a valid way to only
// enable the meta surfaces experiment.
//
// Parameter name: "AllowedRandomTypes"
// Parameter type: Comma separated list of decimal integers, each of which
//     represents an IdentifiableSurface::Type.
//
// When specifying these values on the command-line, the commas should be
// escaped using URL encoding. I.e. '1,2' -> '1%2C2'.
//
// E.g.:
//  * "1, 2" : Matches all surfaces with types kWebFeature and kCanvasReadback.
extern const base::FeatureParam<std::string>
    kIdentifiabilityStudyAllowedRandomTypes;

// Largest meaningful surface count. Based on observed data. In very rare cases
// the overall number of surfaces will exceed this limit, but based on prior
// measurements these cases account for a tiny fraction of the entire
// population.
constexpr int kMaxIdentifiabilityStudyExpectedSurfaceCount = 1500;

// The limit for the optimistic naive budget for the identifiability study.
//
// Each surface that's reported back via metrics reduces the available budget.
// No report sent back should exceed this budget. The unit of measurement for
// the budget is currently one _median_ identifiable surface. Some surfaces may
// be more expensive and some may be less expensive.
//
// This value cannot exceed kMaxIdentifiabilityStudyActiveSurfaceBudget.
//
// Parameter name: "Max"
// Parameter type: int
extern const base::FeatureParam<int> kIdentifiabilityStudyActiveSurfaceBudget;

// This is a hardcoded maximum for the identifiability study budget. The actual
// budget cannot exceed this value even if it's sent via a server-side
// configuration.
//
// I.e. the following is always true:
//
//     active_surface_budget_ <= kMaxIdentifiabilityStudyActiveSurfaceBudget
//
// This restriction prevents `active_surface_budget_` being increased past this
// hardcoded limit from a server-side configuration.
constexpr int kMaxIdentifiabilityStudyActiveSurfaceBudget = 40;

// Relative cost of individual surfaces.
//
// Parameter name: "HashCost"
// Parameter type: Comma separated list of <surface-hash-in-decimal;cost-factor>
//                 pairs.
//
// By default all surfaces cost 1 _average_ surface. Exceptions are noted
// individually and by type. This parameter contains the per-surface costs.
//
// Costs are always specified in units of _average_ surface. The value can be
// a float expressed in decimal.
//
// When specifying these values on the command-line, the commas and semicolons
// should be escaped using URL encoding. I.e. '1;2,3;4' -> '1%3B2%2C3%3B4'.
//
// E.g.:
//   * "257;0.5" : Sets the relative cost of 0.5 for
//                 IdentifiableSurface::FromTypeAndToken(kWebFeature, 1).
extern const base::FeatureParam<std::string> kIdentifiabilityStudyPerHashCost;

// Selection rate for clusters of related surfaces.
// Surface equivalence classes.
//
// Parameter name: "Classes"
// Parameter type: Comma separated list of classes. Each class is a semicolon
//                 separated list of surfaces. See examples below.
//
// The first surface in the list is the representative surface that forms the
// basis for determining the cost for the entire class. I.e. the cost of the
// first surface in the list is assumed to be the cost of _any subset_ of
// surfaces in the set.
//
// Every surface in an equivalence class is assumed to be pairwise perfectly
// correlated with all other surfaces in the set. For more details see
// definition of SurfaceSetValuation::EquivalenceClassIdentifierMap.
//
// It is an error for a surface to appear in more than one equivalence class.
//
// For more details see `SurfaceSetValuation`.
//
// E.g.:
//   * "1;2;3,4;5;6" : Defines two classes: {1,2,3} and {4,5,6}. The surface
//     with ID 1 defines the cost of the entire class {1,2,3}. Similarly the
//     surface with ID 4 defines the cost of the entire class {4,5,6}.
//
extern const base::FeatureParam<std::string>
    kIdentifiabilityStudySurfaceEquivalenceClasses;

// Selection rate for clusters of related surface types.
//
// Parameter name: "TypeCost"
// Parameter type: Comma separated list of <surface-type-in-decimal;cost-factor>
//                 pairs.
//
// By default all surfaces cost 1 _average_ surface. Exceptions are noted
// individually and by type. This parameter contains the per-type costs.
//
// Costs are always specified in units of _average_ surface. The value can be
// a float expressed in decimal.
//
// When specifying these values on the command-line, the commas and semicolons
// should be escaped using URL encoding. I.e. '1;2,3;4' -> '1%3B2%2C3%3B4'.
//
// E.g.:
//   * "1;0.5" : Sets the relative cost of 0.5 for all surfaces of type
//               kWebFeature.
extern const base::FeatureParam<std::string> kIdentifiabilityStudyPerTypeCost;

// Surface Sampling Blocks.
//
// Parameter name: "Blocks"
// Parameter type: Comma separated list of blocks. Each block is a semicolon
//                 separated list of surfaces. See examples below.
//
// If this parameter specifies more than one block then at the start of an
// experiment generation the browser picks one of the blocks at random (see
// `BlockWeights` for details on the random distribution). All surfaces in the
// selected block are considered to be in the active set.
//
// * It is valid for a single surface to be a member of multiple blocks. The
//   study only uses one block.
//
// * The index of the selected block is persisted. If a new configuration has
//   the same generation (Gen) but a different value for the `Blocks` parameter,
//   the client will select the block at the same offset as the one it
//   previously selected.
//
// * Specifying a surface that is blocked (either via `BlockedHashes` or
//   `BlockedTypes`) is an error.
//
// * A non-empty value for this parameter enables assigned block sampling and
//   disables random sampling.
//
// E.g.:
//   * "1;2;3,4;5;6,7;8;9" : Defines three blocks: {1,2,3}, {4,5,6}, and
//     {7,8,9}.
extern const base::FeatureParam<std::string> kIdentifiabilityStudyBlocks;

// Selection Weights for Blocks.
//
// Parameter name: "BlockWeights"
// Parameter type: Comma separated list of relative weights expressed as
//                 integers.
//
// If this parameter is specified then it must specify a weight for each block
// that is defined using the `Blocks` parameter. Each integer defines the
// relative weight assigned to the block of surfaces at the corresponding index.
// Random selection of a block uses the multinomial distribution resulting from
// normalizing the weights.
//
// * All weights must be non-zero positive integers.
//
// * There must be exactly as many weights as there are blocks. If not, the
//   client assumes that the distribution should be uniform.
//
// * If this parameter is not specified, then the client assumes that the blocks
//   are to be selected based on a uniform distribution.
//
// E.g.:
//   * "5,7,3" assigns the probabilities ⅓, 7/15, ⅕ respectively to the three
//     blocks defined in `Blocks`.
extern const base::FeatureParam<std::string> kIdentifiabilityStudyBlockWeights;

// Per surface relative cost.
//
// Parameter name: "HashCost"
// Parameter type: Comma separated list of <surface;cost> pairs.
//
// By default all surfaces cost 1 *average* surface. Exceptions are noted
// individually and by type. This parameter contains individual costs.
//
// Costs are always specified in units of _average_ surface. The value can be
// a float expressed in decimal.
//
// When specifying these values on the command-line, the commas and semicolons
// should be escaped using URL encoding. I.e. '1;2,3;4' -> '1%3B2%2C3%3B4'.
//
// See SurfaceSetValuation for details on the costing model.
//
// E.g.:
//   * "261;0.5" : Sets the relative cost of 0.5 for surface with ID 261, which
//   is a surface of type kWebFeature and token 1.
extern const base::FeatureParam<std::string> kIdentifiabilityStudyPerHashCost;

// Per type relative cost.
//
// Parameter name: "TypeCost"
// Parameter type: Comma separated list of <surface-type;cost> pairs.
//
// By default all surfaces cost 1 _average_ surface. Exceptions are noted
// individually and by type. This parameter contains the per-type costs.
//
// Costs are always specified in units of _average_ surface. The value can be
// a float expressed in decimal.
//
// When specifying these values on the command-line, the commas and semicolons
// should be escaped using URL encoding. I.e. '1;2,3;4' -> '1%3B2%2C3%3B4'.
//
// See SurfaceSetValuation for details on the costing model.
//
// E.g.:
//   * "1;0.5" : Sets the relative cost of 0.5 for all surfaces of type
//               kWebFeature.
extern const base::FeatureParam<std::string> kIdentifiabilityStudyPerTypeCost;

// This is a hardcoded maximum for the probability of any single surface to be
// reported as part of the experiment.
//
// For example, given the following parameters:
//     kIdentifiabilityStudyBlocks = "1;2,1;3,4;5"
//     kIdentifiabilityStudyBlockWeights = "1,1,1"
// the surface 1 has probability 2/3 to be chosen.
//
// If, in the finch client configuration, a surface appears with total
// probability higher than this threshold, the study will be deactivated for
// this client and this client will not report any surface.
constexpr double kMaxProbabilityPerSurface = 0.5;

}  // namespace features

#endif  // CHROME_COMMON_PRIVACY_BUDGET_PRIVACY_BUDGET_FEATURES_H_
