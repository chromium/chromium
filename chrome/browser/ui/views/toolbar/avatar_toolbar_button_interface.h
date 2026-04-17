// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_VIEWS_TOOLBAR_AVATAR_TOOLBAR_BUTTON_INTERFACE_H_
#define CHROME_BROWSER_UI_VIEWS_TOOLBAR_AVATAR_TOOLBAR_BUTTON_INTERFACE_H_

#include <optional>

#include "base/auto_reset.h"
#include "base/functional/callback_helpers.h"
#include "base/observer_list_types.h"
#include "chrome/browser/ui/views/profiles/avatar_toolbar_button_types.h"
#include "components/signin/public/base/signin_buildflags.h"
#include "ui/views/bubble/bubble_anchor.h"

class BrowserWindowInterface;

namespace views {
class DialogDelegate;
}

// This class represents the abstract interface for the Profile Avatar Button,
// which can be implemented by both Views and WebUI.
class AvatarToolbarButtonInterface {
 public:
  virtual ~AvatarToolbarButtonInterface() = default;

  class Observer : public base::CheckedObserver {
   public:
    virtual void OnMouseExited() {}
    virtual void OnBlur() {}
    virtual void OnButtonPressed() {}
    virtual void OnIconUpdated() {}
    virtual void OnIPHPromoChanged(bool has_promo) {}
  };

  virtual bool IsMouseHovered() const = 0;
  virtual bool HasFocus() const = 0;

  // Returns the dialog delegate for any dialog currently anchored to the avatar
  // button, or nullptr if none exists.
  virtual views::DialogDelegate* GetDialogDelegate() = 0;

  // Returns the anchor to use for avatar button bubbles.
  views::BubbleAnchor GetBubbleAnchor(BrowserWindowInterface& browser);

  virtual void ButtonPressed(bool is_source_accelerator) = 0;

  // Sets the button state to show the provided text with the provided
  // accessibility label and action.
  //
  // If the `explicit_action` is set, it will override the default action of the
  // button, otherwise the default action will be used.
  //
  // Returns a callback to be used when the button state should be reset, i.e.
  // shown text should be hidden and the explicit action should stop being used.
  virtual base::ScopedClosureRunner SetExplicitButtonState(
      const std::u16string& text,
      std::optional<std::u16string> accessibility_label,
      std::optional<base::RepeatingCallback<void(bool is_source_accelerator)>>
          explicit_action) = 0;
  // Returns whether the button currently has an explicit state set.
  virtual bool HasExplicitButtonState() const = 0;

  // Methods to register or remove observers.
  virtual void AddObserver(Observer* observer) = 0;
  virtual void RemoveObserver(Observer* observer) = 0;

  // Triggers an update of the avatar icon.
  virtual void UpdateIcon() = 0;

  // Triggers an update of the avatar text.
  virtual void UpdateText() = 0;

  // Attempts showing the In-Product-Help for profile Switching.
  virtual void MaybeShowProfileSwitchIPH() = 0;

#if BUILDFLAG(IS_WIN) || BUILDFLAG(IS_MAC) || BUILDFLAG(IS_LINUX)
  // Attempts showing the In-Product-Help when a supervised user signs-in in a
  // profile.
  virtual void MaybeShowSupervisedUserSignInIPH() = 0;

  // Attempts showing the In-Product-Help listing benefits for signed-in users
  // after the sync-to-signin migration.
  virtual void MaybeShowSignInBenefitsIPH() = 0;
#endif  // BUILDFLAG(IS_WIN) || BUILDFLAG(IS_MAC) || BUILDFLAG(IS_LINUX)

  // Clears the active state (makes it inactive).
  virtual void ClearActiveStateForTesting() = 0;

#if BUILDFLAG(ENABLE_DICE_SUPPORT)
  // WARNING: Do not use this method to test the Promo flows. Only used when
  // necessary to bypass resetting the profile - e.g. when attempting to reach
  // the limit counts.
  virtual void ForceShowingPromoForTesting() = 0;

  // Returns whether the delay timer was running or not.
  // Stops the timer if it is running.
  virtual bool GetStateAndFireSignedOutTriggerDelayTimerForTesting() = 0;
#endif  // BUILDFLAG(ENABLE_DICE_SUPPORT)

  // Can be used in tests to reduce or remove the delay before showing the IPH.
  [[nodiscard]] static base::AutoReset<base::TimeDelta>
  SetScopedIPHMinDelayAfterCreationForTesting(base::TimeDelta delay);

  // These helper functions allow tests to be time independent; tests that are
  // time dependent tend to create a lot of flakiness.
  //
  // This function allows to set an infinite delay for time dependent parts. By
  // default tests should have this function called for all types, and then
  // calling `TriggerTimeoutForTesting()` when needing to force trigger the
  // ending of the delay. This allows to properly test the behavior before and
  // after delay expiry while controlling those events..
  [[nodiscard]] static base::AutoReset<std::optional<base::TimeDelta>>
  CreateScopedInfiniteDelayOverrideForTesting(AvatarDelayType delay_type);
  // Clears the active state (makes it inactive).

#if BUILDFLAG(ENABLE_DICE_SUPPORT)
  // Specific override for the SigninPending text delay. Setting a zero value
  // make it possible to test the creation of browser after the delay has
  // reached.
  // The delay start time is shared in a ProfileUserData which makes it harder
  // to access in case no browser are visible anymore, making the
  // `TriggerTimeoutForTesting()` not enough for testing.
  [[nodiscard]] static base::AutoReset<std::optional<base::TimeDelta>>
  CreateScopedZeroDelayOverrideSigninPendingTextForTesting();
#endif  // BUILDFLAG(ENABLE_DICE_SUPPORT)

 protected:
  // Do not show the IPH right when creating the window, so that the IPH has a
  // separate animation.
  static base::TimeDelta g_iph_min_delay_after_creation;
};

#endif  // CHROME_BROWSER_UI_VIEWS_TOOLBAR_AVATAR_TOOLBAR_BUTTON_INTERFACE_H_
