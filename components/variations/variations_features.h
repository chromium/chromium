// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_VARIATIONS_VARIATIONS_FEATURES_H_
#define COMPONENTS_VARIATIONS_VARIATIONS_FEATURES_H_

#include "base/component_export.h"
#include "base/metrics/field_trial.h"

namespace variations {
namespace internal {

// A feature that supports more finely-grained control over the transmission of
// VariationIDs to Google web properties by allowing some VariationIDs to not be
// transmitted in all contexts. See IsFirstPartyContext() in
// variations_http_headers.cc for more details.
COMPONENT_EXPORT(VARIATIONS_FEATURES)
BASE_DECLARE_FEATURE(kRestrictGoogleWebVisibility);

}  // namespace internal
}  // namespace variations

#endif  // COMPONENTS_VARIATIONS_VARIATIONS_FEATURES_H_
