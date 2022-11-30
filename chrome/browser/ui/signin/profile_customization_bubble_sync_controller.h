// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_SIGNIN_PROFILE_CUSTOMIZATION_BUBBLE_SYNC_CONTROLLER_H_
#define CHROME_BROWSER_UI_SIGNIN_PROFILE_CUSTOMIZATION_BUBBLE_SYNC_CONTROLLER_H_

#include "third_party/skia/include/core/SkColor.h"

class Browser;

// Implemented in
// chrome/browser/ui/views/profiles/profile_customization_bubble_sync_controller.cc
void ApplyProfileColorAndShowCustomizationBubbleWhenNoValueSynced(
    Browser* browser,
    SkColor suggested_profile_color);

#endif  // CHROME_BROWSER_UI_SIGNIN_PROFILE_CUSTOMIZATION_BUBBLE_SYNC_CONTROLLER_H_
