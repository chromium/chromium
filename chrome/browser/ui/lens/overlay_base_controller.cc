// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/lens/overlay_base_controller.h"

#include "components/tabs/public/tab_interface.h"

OverlayBaseController::OverlayBaseController(tabs::TabInterface* tab,
                                             PrefService* pref_service)
    : tab_(tab), pref_service_(pref_service) {}

OverlayBaseController::~OverlayBaseController() {
  state_ = State::kOff;
}
