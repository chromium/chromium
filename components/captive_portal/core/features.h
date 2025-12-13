// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_CAPTIVE_PORTAL_CORE_FEATURES_H_
#define COMPONENTS_CAPTIVE_PORTAL_CORE_FEATURES_H_

#include "base/component_export.h"
#include "base/feature_list.h"

namespace captive_portal::features {

// Used to gate the switch to http://connectivitycheck.gstatic.com/
// for captive portal detection (from http://www.gstatic.com/)
// TODO(crbug.com/433898681): Remove the feature flag once the launch sticks.
COMPONENT_EXPORT(CAPTIVE_PORTAL)
BASE_DECLARE_FEATURE(kCaptivePortalUpdatedOrigin);

}  // namespace captive_portal::features

#endif  // COMPONENTS_CAPTIVE_PORTAL_CORE_FEATURES_H_
