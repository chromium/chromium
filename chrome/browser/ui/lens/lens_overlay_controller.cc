// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/lens/lens_overlay_controller.h"

LensOverlayController::LensOverlayController(tabs::TabModel* tab_model)
    : tab_model_(tab_model) {}

LensOverlayController::~LensOverlayController() = default;
