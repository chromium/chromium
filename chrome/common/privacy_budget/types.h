// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_COMMON_PRIVACY_BUDGET_TYPES_H_
#define CHROME_COMMON_PRIVACY_BUDGET_TYPES_H_

#include <type_traits>
#include <vector>

#include "base/containers/flat_map.h"
#include "base/containers/flat_set.h"
#include "chrome/common/privacy_budget/field_trial_param_conversions.h"
#include "third_party/blink/public/common/privacy_budget/identifiable_surface.h"

// Common container and map types. In order to verify successful encoding and
// decoding, each of these must be tested in
// field_trial_param_conversions_unittest.cc.
//
// In all cases, the choice of container assumes that:
//   1. Size is relatively low: Use of a contiguous container helps with data
//      locality.
//   2. Mutations are uncommon: A contiguous container is usually expensive to
//      mutate, but fast lookups and locality make up for it.
//
// If other characteristics are desired, then we should consider other container
// types. Please test encoding/decoding when using new container types.

using IdentifiableSurfaceSet = base::flat_set<blink::IdentifiableSurface>;

using IdentifiableSurfaceTypeSet =
    base::flat_set<blink::IdentifiableSurface::Type>;

using IdentifiableSurfaceList = std::vector<blink::IdentifiableSurface>;

using IdentifiableSurfaceTypeList =
    std::vector<blink::IdentifiableSurface::Type>;

// Sampling rates are represented as the denominator of a quotient 1/R. I.e.
// A sampling rate of 1 in 100 is represented using the integer 100.
using IdentifiableSurfaceSampleRateMap =
    base::flat_map<blink::IdentifiableSurface, unsigned int>;

// Sampling rates are represented as the denominator of a quotient 1/R. I.e.
// A sampling rate of 1 in 100 is represented using the integer 100.
using IdentifiableSurfaceTypeSampleRateMap =
    base::flat_map<blink::IdentifiableSurface::Type, unsigned int>;

// Costs are represented as a ratio relative to the "median" identifiability of
// a single API. This odd choice is due to backwards compatibility where prior
// versions of the study controlled client exposure via placing a limit on the
// _number_ of surfaces sampled.
//
// Relative costs are in the logarithmic domain. Explained below.
//
// Let's say there's a surface 𝐀 whose value can be used to uniformly divide the
// audience in to four segments. In terms of Shannon entropy one might say that
// the information content of surface 𝐀 is log₂4 bits. Now if there's another
// surface 𝐁 whose relative cost is 0.5, then 𝐁 would have a Shannon entropy of
// 0.5×log₂4 bits. In other words 𝐁 has the information content equivalent to
// what's needed to uniformly divide an audience into √4=2 equal parts.
//
// In general, if the median identifiability is 𝐦, and the relative
// identifiability of a surface is 𝐫, then the identifiability of that surface
// is mʳ.
using PrivacyBudgetCost = double;

using IdentifiableSurfaceCostMap =
    base::flat_map<blink::IdentifiableSurface, PrivacyBudgetCost>;

using IdentifiableSurfaceTypeCostMap =
    base::flat_map<blink::IdentifiableSurface::Type, PrivacyBudgetCost>;

// See SurfaceSetEquivalence for details on how equivalence classes work.
// SurfaceSetEquivalentClassesList contains a list of equivalence classes. Each
// class is encoded as a list of surfaces.
//
// **The first element in the list is considered to be the representative
// surface for that class.
//
// Obv an equivalence set which contains just zero or one members is
// nonsensical. For the purpose of ecoding/decoding such instances are ignored.
using SurfaceSetEquivalentClassesList = std::vector<IdentifiableSurfaceList>;

// Similar to the SurfaceSetEquivalentClassesList, but is semantically different
// in that the ordering doesn't matter. There's no assumption that the first
// element of each list is special in any meaningful way.
using IdentifiableSurfaceGroupList = std::vector<IdentifiableSurfaceList>;

namespace privacy_budget_internal {

template <>
struct SortWhenSerializing<IdentifiableSurfaceSet> : std::true_type {};
template <>
struct SortWhenSerializing<IdentifiableSurfaceTypeSet> : std::true_type {};

}  // namespace privacy_budget_internal
#endif  // CHROME_COMMON_PRIVACY_BUDGET_TYPES_H_
