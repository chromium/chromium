// Copyright 2020 The Chromium Authors. All rights reserved.
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

// Root feature for all identifiability study logic.
//
// If the feature is disabled, then this browser instance will not be
// participating in the identifiability study. I.e. no identifiability metrics
// will be recorded or reported.
//
// Enabling the feature doesn't automatically make this client part of the study
// either.
extern const base::Feature kIdentifiabilityStudy;

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
extern const base::FeatureParam<std::string> kIdentifiabilityStudyBlockedTypes;

// Base selection rate for surfaces that don't have a per-surface selection
// rate. Rates are always represented as the denominator of a 1-in-N selection.
//
// Parameter name: "Rho"
// Parameter type: int
extern const base::FeatureParam<int> kIdentifiabilityStudySurfaceSelectionRate;

// Largest meaningful sampling denominator. Picked out of hat.
constexpr int kMaxIdentifiabilityStudySurfaceSelectionRate = 1000000;

// The maximum number of surfaces that can be included in the identifiability
// study.
//
// Parameter name: "Max"
// Parameter type: int
extern const base::FeatureParam<int> kIdentifiabilityStudyMaxSurfaces;

// This is a hardcoded maximum for the number of identifiable surfaces that
// can be reported for a user. The actual ceiling for active surfaces cannot
// exceed this value even if it's sent via a server-side configuration.
//
// In other words this is the maximum value that can be configured via
// `kIdentifiabilityStudyMaxSurfaces`. Hence it's the
// `kMaxIdentifiabilityStudyMaxSurfaces`.
constexpr int kMaxIdentifiabilityStudyMaxSurfaces = 40;

// Surface equivalence classes.
//
// Parameter name: "Classes"
// Parameter type: Comma separated list of classes. Each class is a semicolon
//                 separated list of surfaces. See examples below.
//
//                 NOTE: The first surface in the list is the representative
//                 surface that forms the basis for determining the cost for
//                 the entire class. I.e. the cost of the first surface in the
//                 list is assumed to be the cost of _any subset_ of surfaces
//                 in the set.
//
// Every surface in an equivalence class is assumed to be pairwise perfectly
// correlated with all other surfaces in the set. For more details see
// definition of SurfaceSetValuation::EquivalenceClassIdentifierMap.
//
// E.g.:
//   * "1;2;3,4;5;6" : Defines two classes: {1,2,3} and {4,5,6}. The surface
//     with ID 1 defines the cost of the entire class {1,2,3}. Similarly the
//     surface with ID 4 defines the cost of the entire class {4,5,6}.
//
extern const base::FeatureParam<std::string>
    kIdentifiabilityStudySurfaceEquivalenceClasses;

// Selection rate for clusters of related surfaces.
//
// Parameter name: "HashRate"
// Parameter type: Comma separated list of <filter-string>;<rate> pairs.
//
// E.g.:
//   * "257;800" : Sets the selection rate to 1-in-800 for
//                 IdentifiableSurface::FromTypeAndToken(kWebFeature, 1).
extern const base::FeatureParam<std::string>
    kIdentifiabilityStudyPerSurfaceSettings;

// Selection rate for clusters of related surface types.
//
// Parameter name: "TypeRate"
// Parameter type: Comma separated list of <filter-string>;<rate> pairs.
//
// E.g.:
//   * "2;1000" : Sets the selection rate to 1-in-1000 for *all* surfaces with
//                type kCanvasReadback.
extern const base::FeatureParam<std::string>
    kIdentifiabilityStudyPerTypeSettings;

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
// E.g.:
//   * "1;0.5" : Sets the relative cost of 0.5 for all surfaces of type
//               kWebFeature.
extern const base::FeatureParam<std::string> kIdentifiabilityStudyPerTypeCost;

}  // namespace features

#endif  // CHROME_COMMON_PRIVACY_BUDGET_PRIVACY_BUDGET_FEATURES_H_
