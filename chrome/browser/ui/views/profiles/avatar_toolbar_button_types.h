// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_VIEWS_PROFILES_AVATAR_TOOLBAR_BUTTON_TYPES_H_
#define CHROME_BROWSER_UI_VIEWS_PROFILES_AVATAR_TOOLBAR_BUTTON_TYPES_H_

#include "components/signin/public/base/signin_buildflags.h"

// Enum used for testing. It allows overriding different delay values based on
// their usage in the `AvatarToolbarButton` through helper testing functions.
enum class AvatarDelayType {
  // Delay for the name to stop showing.
  kNameGreeting,
  // Delay for the on sign-in state.
  kOnSignin,
#if BUILDFLAG(ENABLE_DICE_SUPPORT)
  // Delay for the SigninPending mode to show the "Verify it's you" text.
  kSigninPendingText,
  // Delay for the promo that are shown by expanding the button.
  kPromo,
  // Delay for the Promo trigger for signed out profiles.
  kSignedOutPromo,
#endif  // BUILDFLAG(ENABLE_DICE_SUPPORT)
};

#endif  // CHROME_BROWSER_UI_VIEWS_PROFILES_AVATAR_TOOLBAR_BUTTON_TYPES_H_
