// Copyright 2014 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_PROFILE_CHOOSER_CONSTANTS_H_
#define CHROME_BROWSER_UI_PROFILE_CHOOSER_CONSTANTS_H_

#include "third_party/skia/include/core/SkColor.h"

namespace profiles {

// Different views that can be displayed in the profile chooser bubble.
enum BubbleViewMode {
  // Shows the default avatar bubble.
  BUBBLE_VIEW_MODE_PROFILE_CHOOSER,
  // Shows a web view for primary sign in.
  BUBBLE_VIEW_MODE_GAIA_SIGNIN,
  // Shows a web view for adding secondary accounts.
  BUBBLE_VIEW_MODE_GAIA_ADD_ACCOUNT,
  // Shows a web view for reauthenticating an account.
  BUBBLE_VIEW_MODE_GAIA_REAUTH,
  // Shows a view for incognito that displays the number of incognito windows.
  BUBBLE_VIEW_MODE_INCOGNITO,
};

}  // namespace profiles

#endif  // CHROME_BROWSER_UI_PROFILE_CHOOSER_CONSTANTS_H_
