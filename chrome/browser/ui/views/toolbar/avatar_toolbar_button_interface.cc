// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/views/toolbar/avatar_toolbar_button_interface.h"

#include "base/time/time.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/ui/browser_element_identifiers.h"
#include "chrome/browser/ui/interaction/browser_elements.h"
#include "chrome/browser/ui/views/frame/browser_view.h"
#include "chrome/browser/ui/views/profiles/avatar_toolbar_button.h"
#include "chrome/browser/ui/views/profiles/avatar_toolbar_button_state_manager.h"
#include "chrome/browser/ui/views/toolbar/toolbar_view.h"

// static
bool AvatarToolbarButtonInterface::CanShowForProfile(Profile* profile) {
#if BUILDFLAG(IS_CHROMEOS)
  // ChromeOS only badges Incognito, Guest, Enterprise Isolated and captive
  // portal signin icons in the browser window.
  return profile->IsIncognitoProfile() || profile->IsGuestSession() ||
         profile->IsEnterpriseIsolatedModeProfile() ||
         (profile->IsOffTheRecord() &&
          profile->GetOTRProfileID().IsCaptivePortal());
#else
  // DevTools profiles are OffTheRecord, so hide it there.
  return profile->IsIncognitoProfile() || profile->IsGuestSession() ||
         profile->IsEnterpriseIsolatedModeProfile() ||
         profile->IsRegularProfile();
#endif
}

// static
views::BubbleAnchor AvatarToolbarButtonInterface::GetBubbleAnchor(
    BrowserWindowInterface& browser) {
  if (auto* avatar_toolbar_button = BrowserElements::From(&browser)->GetElement(
          kToolbarAvatarButtonElementId)) {
    return views::BubbleAnchor(avatar_toolbar_button);
  }

  // Fallback to the Views AvatarToolbarButton if it is hidden (e.g. overflowed
  // into the toolbar overflow menu).
  if (auto* browser_view = BrowserView::GetBrowserViewForBrowser(&browser);
      browser_view && browser_view->toolbar()) {
    return views::BubbleAnchor(
        browser_view->toolbar()->avatar_toolbar_button());
  }

  return views::BubbleAnchor();
}

// static
base::AutoReset<std::optional<base::TimeDelta>> AvatarToolbarButtonInterface::
    CreateScopedInfiniteDelayOverrideForTesting(  // IN-TEST
        AvatarDelayType delay_type) {
  return AvatarToolbarButtonStateManager::
      CreateScopedInfiniteDelayOverrideForTesting(delay_type);  // IN-TEST
}

#if BUILDFLAG(ENABLE_DICE_SUPPORT)
// static
base::AutoReset<std::optional<base::TimeDelta>> AvatarToolbarButtonInterface::
    CreateScopedZeroDelayOverrideSigninPendingTextForTesting() {  // IN-TEST
  return AvatarToolbarButtonStateManager::
      CreateScopedZeroDelayOverrideSigninPendingTextForTesting();  // IN-TEST
}
#endif  // BUILDFLAG(ENABLE_DICE_SUPPORT)
