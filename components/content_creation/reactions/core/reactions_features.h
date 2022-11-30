// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_CONTENT_CREATION_REACTIONS_CORE_REACTIONS_FEATURES_H_
#define COMPONENTS_CONTENT_CREATION_REACTIONS_CORE_REACTIONS_FEATURES_H_

#include "base/feature_list.h"

namespace content_creation {

// Main feature for the Lightweight Reactions project.
BASE_DECLARE_FEATURE(kLightweightReactions);

// Returns true if the Lightweight Reactions feature is enabled.
bool IsLightweightReactionsEnabled();

}  // namespace content_creation

#endif  // COMPONENTS_CONTENT_CREATION_REACTIONS_CORE_REACTIONS_FEATURES_H_