// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_ORIGIN_MATCHER_FEATURES_H_
#define COMPONENTS_ORIGIN_MATCHER_FEATURES_H_

#include "base/component_export.h"
#include "base/features.h"

namespace origin_matcher {

// When enabled, OriginMatcher::operator= will use the new, hopefully faster
// implementation that does not serialize and deserialize all the rules.
COMPONENT_EXPORT(ORIGIN_MATCHER)
BASE_DECLARE_FEATURE(kOriginMatcherNewCopyAssignment);

}  // namespace origin_matcher

#endif  // COMPONENTS_ORIGIN_MATCHER_FEATURES_H_
