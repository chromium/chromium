// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/lens/test_lens_overlay_controller.h"

#include "chrome/browser/profiles/profile.h"
#include "components/prefs/pref_service.h"
#include "components/tabs/public/tab_interface.h"

namespace lens {

TestLensOverlayController::TestLensOverlayController(
    tabs::TabInterface* tab,
    LensSearchController* lens_search_controller,
    PrefService* pref_service)
    : LensOverlayController(tab, lens_search_controller, pref_service) {}

}  // namespace lens
