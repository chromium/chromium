// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_VIEWS_TABS_ORGANIZER_ORGANIZER_PANEL_UTILS_H_
#define CHROME_BROWSER_UI_VIEWS_TABS_ORGANIZER_ORGANIZER_PANEL_UTILS_H_

#include "base/feature_list.h"

namespace organizer_panel {

BASE_DECLARE_FEATURE(kOrganizerPanel);

// Returns whether the Organizer Panel feature is enabled.
bool IsOrganizerPanelFeatureEnabled();

}  // namespace organizer_panel

#endif  // CHROME_BROWSER_UI_VIEWS_TABS_ORGANIZER_ORGANIZER_PANEL_UTILS_H_
