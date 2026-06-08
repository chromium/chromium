// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_CONTEXTUAL_SEARCH_FOOTPRINTS_PUBLIC_CONSTANTS_H_
#define COMPONENTS_CONTEXTUAL_SEARCH_FOOTPRINTS_PUBLIC_CONSTANTS_H_

#include <stdint.h>

namespace contextual_search {

// Setting ID for CONTEXTUAL_SEARCH_DRIVE_DISCLAIMER_ACCEPTED.
inline constexpr int32_t kContextualSearchDriveDisclaimerAccepted = 274;

// Must stay in sync with PERSONAL_CONTEXT_SEARCH_USING_WORKSPACE in
// google3/personalization/footprints/boq/activity_controls/proto/
// activity_controls_setting_enum.proto
inline constexpr int32_t kPersonalContextSearchUsingWorkspace = 183;

}  // namespace contextual_search

#endif  // COMPONENTS_CONTEXTUAL_SEARCH_FOOTPRINTS_PUBLIC_CONSTANTS_H_
