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

// If enabled, this feature will gate the functionality of removing the existing
// tab chips from the composebox and instead add favicon coins next to the "+"
// button.
BASE_DECLARE_FEATURE(kTabFaviconChipsToCoins);

// Gates context menu and favicon coins for omnibox.
BASE_DECLARE_FEATURE(kContextManagementInOmnibox);

}  // namespace omnibox

#endif  // COMPONENTS_OMNIBOX_COMMON_COMPOSEBOX_FEATURES_H_
