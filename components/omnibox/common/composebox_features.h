// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_OMNIBOX_COMMON_COMPOSEBOX_FEATURES_H_
#define COMPONENTS_OMNIBOX_COMMON_COMPOSEBOX_FEATURES_H_

#include "base/feature_list.h"
#include "base/metrics/field_trial_params.h"

namespace omnibox {

// If enabled, this feature will enable an updated tab context management UI.
// - The Composebox Context Menu will show the current and most recent tabs in a
// flyover.
// - Tab favicon chips will show next to the "+" button and in the context menu
// instead of in the co-browse tool bar and RHS dropdown.
BASE_DECLARE_FEATURE(kContextManagementInComposebox);

}  // namespace omnibox

#endif  // COMPONENTS_OMNIBOX_COMMON_COMPOSEBOX_FEATURES_H_
