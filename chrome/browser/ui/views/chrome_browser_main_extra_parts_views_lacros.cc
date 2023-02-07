// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/views/chrome_browser_main_extra_parts_views_lacros.h"

#include "chrome/browser/ui/lacros/float_controller_lacros.h"
#include "chrome/browser/ui/lacros/immersive_context_lacros.h"
#include "chrome/browser/ui/lacros/snap_controller_lacros.h"
#include "chromeos/ui/base/tablet_state.h"
#include "chromeos/ui/wm/features.h"

ChromeBrowserMainExtraPartsViewsLacros::
    ChromeBrowserMainExtraPartsViewsLacros() = default;

ChromeBrowserMainExtraPartsViewsLacros::
    ~ChromeBrowserMainExtraPartsViewsLacros() = default;

void ChromeBrowserMainExtraPartsViewsLacros::PreProfileInit() {
  if (chromeos::wm::features::IsWindowLayoutMenuEnabled()) {
    float_controller_ = std::make_unique<FloatControllerLacros>();
  }

  immersive_context_ = std::make_unique<ImmersiveContextLacros>();
  snap_controller_ = std::make_unique<SnapControllerLacros>();
  tablet_state_ = std::make_unique<chromeos::TabletState>();

  ChromeBrowserMainExtraPartsViews::PreProfileInit();
}
