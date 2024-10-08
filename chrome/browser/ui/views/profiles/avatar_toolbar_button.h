// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_VIEWS_PROFILES_AVATAR_TOOLBAR_BUTTON_H_
#define CHROME_BROWSER_UI_VIEWS_PROFILES_AVATAR_TOOLBAR_BUTTON_H_

#include "base/callback_list.h"
#include "base/functional/callback_forward.h"
#include "base/gtest_prod_util.h"
#include "base/memory/raw_ptr.h"
#include "base/memory/weak_ptr.h"
#include "base/observer_list.h"
#include "base/observer_list_types.h"
#include "base/time/time.h"
#include "build/chromeos_buildflags.h"
#include "chrome/browser/ui/views/toolbar/toolbar_button.h"
#include "components/signin/public/base/signin_buildflags.h"
#include "ui/base/metadata/metadata_header_macros.h"
#include "ui/events/event.h"

class AvatarToolbarButtonDelegate;
class Browser;
class BrowserView;
struct AccountInfo;

// Enum used for testing. It allows overriding different delay values based on
// their usage in the `AvatarToolbarButton` through helper testing functions.
enum class AvatarDelayType {
  // Delay for the name to stop showing.
  kNameGreeting,
  // Delay for the SigninPending mode to show the "Verify it's you" text.
  kSigninPendingText,
};

// This class takes care the Profile Avatar Button.
// Primarily applies UI configuration.
// It's data (text, icon, etc...) content are computed through the
// `AvatarToolbarButtonDelegate`, when relying on Chrome and Profile changes in
// order to adapt the expected content shown in the button.
class AvatarToolbarButton : public ToolbarButton {
  METADATA_HEADER(AvatarToolbarButton, ToolbarButton)

 public:
  class Observer : public base::CheckedObserver {
   public:
    virtual void OnMouseExited() {}
    virtual void OnBlur() {}
    virtual void OnIPHPromoChanged(bool has_promo) {}
    virtual void OnIconUpdated() {}

    ~Observer() override = default;
  };

  explicit AvatarToolbarButton(BrowserView* browser);
  AvatarToolbarButton(const AvatarToolbarButton&) = delete;
  AvatarToolbarButton& operator=(const AvatarToolbarButton&) = delete;
  ~AvatarToolbarButton() override;

  void UpdateText();

  // Expands the pill to show the intercept text.
  // Returns a callback to be used when the shown text should be hidden.
  [[nodiscard]] base::ScopedClosureRunner ShowExplicitText(
      const std::u16string& text,
      std::optional<std::u16string> accessibility_label);

  // Changes the button pressed action.
  // Returns a callback to be used when the new action should stop being used.
  [[nodiscard]] base::ScopedClosureRunner SetExplicitButtonAction(
      base::RepeatingClosure explicit_closure);

  // Returns whether the button currently has a explicit action already set.
  bool HasExplicitButtonAction() const;

  // Control whether the button action is active or not.
  // One reason to disable the action; when a bubble is shown from this button
  // (and not the profile menu), we want to disable the button action, however
  // the button should remain in an "active" state from a UI perspective.
  void SetButtonActionDisabled(bool disabled);
  bool IsButtonActionDisabled() const;

  // Attempts showing the In-Produce-Help for profile Switching.
  void MaybeShowProfileSwitchIPH();

  // Attempts showing the In-Produce-Help when a supervised user signs-in in a
  // profile or takes over an existing non-signed in profile.
  void MaybeShowSupervisedUserSignInIPH(const AccountInfo& account_info);

  // Attempts showing the In-Product-Help in a subsequent web sign-in when the
  // explicit browser sign-in preference was remembered.
  void MaybeShowExplicitBrowserSigninPreferenceRememberedIPH(
      const AccountInfo& account_info);

  // Attempts showing the In-Produce-Help for web sign out.
  void MaybeShowWebSignoutIPH(const std::string& gaia_id);

  // Returns true if a text is set and is visible.
  bool IsLabelPresentAndVisible() const;

  // ToolbarButton:
  void OnMouseExited(const ui::MouseEvent& event) override;
  void OnBlur() override;
  void OnThemeChanged() override;
  void UpdateIcon() override;
  void Layout(PassKey) override;
  int GetIconSize() const override;
  SkColor GetForegroundColor(ButtonState state) const override;
  std::optional<SkColor> GetHighlightTextColor() const override;
  std::optional<SkColor> GetHighlightBorderColor() const override;
  bool ShouldPaintBorder() const override;
  bool ShouldBlendHighlightColor() const override;
  void AddedToWidget() override;
  void PaintButtonContents(gfx::Canvas* canvas) override;

  void ButtonPressed(bool is_source_accelerator = false);

  // Methods to register or remove observers.
  void AddObserver(Observer* observer);
  void RemoveObserver(Observer* observer);

  // Can be used in tests to reduce or remove the delay before showing the IPH.
  static void SetIPHMinDelayAfterCreationForTesting(base::TimeDelta delay);

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
  // Force stop any ongoing delay, this expects the proper state to be active.
  void TriggerTimeoutForTesting(AvatarDelayType delay_type);
  // Specific override for the SigninPending text delay. Setting a zero value
  // make it possible to test the creation of browser after the delay has
  // reached.
  // The delay start time is shared in a ProfileUserData which makes it harder
  // to access in case no browser are visible anymore, making the
  // `TriggerTimeoutForTesting()` not enough for testing.
  [[nodiscard]] static base::AutoReset<std::optional<base::TimeDelta>>
  CreateScopedZeroDelayOverrideSigninPendingTextForTesting();

 private:
  FRIEND_TEST_ALL_PREFIXES(AvatarToolbarButtonTest,
                           HighlightMeetsMinimumContrast);

  // ui::PropertyHandler:
  void AfterPropertyChange(const void* key, int64_t old_value) override;

  // Updates the layout insets depending on whether it is a chip or a button.
  void UpdateLayoutInsets();

  // Updates the inkdrop highlight and ripple properties depending on the state
  // and whether the chip is expanded.
  void UpdateInkdrop();

  // Used as a callback to reset the explicit button action.
  void ResetButtonAction();

  void UpdateAccessibilityLabel();

  // Lists of observers.
  base::ObserverList<Observer, true> observer_list_;

  std::unique_ptr<AvatarToolbarButtonDelegate> delegate_;

  const raw_ptr<Browser> browser_;

  // Time when this object was created.
  const base::TimeTicks creation_time_;

  // Do not show the IPH right when creating the window, so that the IPH has a
  // separate animation.
  static base::TimeDelta g_iph_min_delay_after_creation;

  // Controls the action of the button, on press.
  // Setting this to true will stop the button reaction but the button will
  // remain in active state, not affecting it's UI in any way.
  bool button_action_disabled_ = false;
  // Explicit button action set by external calls.
  base::RepeatingClosure explicit_button_pressed_action_;
  // Internal pointer to the current explicit closure. This is used to
  // invalidate an existing reset callback if an explicit action is being set
  // while an existing already exists. Priority to the last call.
  raw_ptr<base::ScopedClosureRunner> reset_button_action_button_closure_ptr_ =
      nullptr;

  base::WeakPtrFactory<AvatarToolbarButton> weak_ptr_factory_{this};
};

#endif  // CHROME_BROWSER_UI_VIEWS_PROFILES_AVATAR_TOOLBAR_BUTTON_H_
