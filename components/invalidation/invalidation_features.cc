// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/invalidation/invalidation_features.h"

#include "base/feature_list.h"

namespace invalidation {

BASE_FEATURE(kInvalidationsWithDirectMessages,
             "InvalidationsWithDirectMessages",
             base::FEATURE_DISABLED_BY_DEFAULT);

bool IsInvalidationsWithDirectMessagesEnabled() {
  return base::FeatureList::IsEnabled(kInvalidationsWithDirectMessages);
}

}  // namespace invalidation
