// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_INVALIDATION_INVALIDATION_FEATURES_H_
#define COMPONENTS_INVALIDATION_INVALIDATION_FEATURES_H_

#include "base/component_export.h"
#include "base/feature_list.h"

namespace invalidation {

// Turns on invalidations with direct messages by substituting
// InvalidationService with InvalidationListener.
COMPONENT_EXPORT(INVALIDATION_FEATURES)
BASE_DECLARE_FEATURE(kInvalidationsWithDirectMessages);

COMPONENT_EXPORT(INVALIDATION_FEATURES)
bool IsInvalidationsWithDirectMessagesEnabled();

}  // namespace invalidation

#endif  // COMPONENTS_INVALIDATION_INVALIDATION_FEATURES_H_
