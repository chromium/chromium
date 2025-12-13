// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/startup/default_browser_prompt/pin_infobar/pin_infobar_mac_util.h"

#include "base/apple/bundle_locations.h"
#include "chrome/browser/mac/dock.h"

namespace default_browser {

bool ShouldOfferToPin() {
  return dock::ChromeIsInTheDock() == dock::ChromeInDockFalse;
}

void PinChromeToDock() {
  dock::AddIcon(base::apple::OuterBundle().bundlePath, nil);
}

}  // namespace default_browser
