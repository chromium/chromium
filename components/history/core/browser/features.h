// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_HISTORY_CORE_BROWSER_FEATURES_H_
#define COMPONENTS_HISTORY_CORE_BROWSER_FEATURES_H_

#include "base/feature_list.h"

namespace history {

// If true, navigations with a transition qualifier of
// PAGE_TRANSITION_FROM_API_3 are not returned from queries for visible rows.
extern const base::Feature kHideFromApi3Transitions;

}  // namespace history

#endif  // COMPONENTS_HISTORY_CORE_BROWSER_FEATURES_H_
