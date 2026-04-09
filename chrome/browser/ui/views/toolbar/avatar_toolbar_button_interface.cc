// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/views/toolbar/avatar_toolbar_button_interface.h"

#include "base/time/time.h"
#include "chrome/browser/ui/browser_element_identifiers.h"
#include "chrome/browser/ui/interaction/browser_elements.h"
#include "chrome/browser/ui/views/profiles/avatar_toolbar_button_state_manager.h"

// static
base::TimeDelta AvatarToolbarButtonInterface::iph_min_delay_after_creation_ =
    base::Seconds(2);

views::BubbleAnchor AvatarToolbarButtonInterface::GetBubbleAnchor(
    BrowserWindowInterface& browser) {
  ui::ElementContext context = BrowserElements::From(&browser)->GetContext();
  return views::BubbleAnchor(
      ui::ElementTracker::GetElementTracker()->GetUniqueElement(
          kToolbarAvatarButtonElementId, context));
}

// static
base::AutoReset<base::TimeDelta> AvatarToolbarButtonInterface::
    SetScopedIPHMinDelayAfterCreationForTesting(  // IN-TEST
        base::TimeDelta delay) {
  return base::AutoReset<base::TimeDelta>(&iph_min_delay_after_creation_,
                                          delay);
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
