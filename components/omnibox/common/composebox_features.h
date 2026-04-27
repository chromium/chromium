// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_OMNIBOX_COMMON_COMPOSEBOX_FEATURES_H_
#define COMPONENTS_OMNIBOX_COMMON_COMPOSEBOX_FEATURES_H_

#include "base/feature_list.h"
#include "base/metrics/field_trial_params.h"

namespace omnibox {

// If enabled, the Composebox Context Menu will show the current/most recent tab
// in the context menu and below it, an "Add tabs" menu item which, when
// clicked, shows the users current tab in a flyover.
BASE_DECLARE_FEATURE(kContextMenuTabFlyover);

}  // namespace omnibox

#endif  // COMPONENTS_OMNIBOX_COMMON_COMPOSEBOX_FEATURES_H_
