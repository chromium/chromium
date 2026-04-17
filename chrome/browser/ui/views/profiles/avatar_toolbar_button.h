// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_VIEWS_PROFILES_AVATAR_TOOLBAR_BUTTON_H_
#define CHROME_BROWSER_UI_VIEWS_PROFILES_AVATAR_TOOLBAR_BUTTON_H_

#include "base/auto_reset.h"
#include "base/callback_list.h"
#include "base/functional/callback_forward.h"
#include "base/gtest_prod_util.h"
#include "base/memory/raw_ptr.h"
#include "base/memory/weak_ptr.h"
#include "base/observer_list.h"
#include "base/observer_list_types.h"
#include "base/time/time.h"
#include "chrome/browser/ui/views/profiles/avatar_toolbar_button_types.h"
#include "chrome/browser/ui/views/toolbar/avatar_toolbar_button_interface.h"
#include "chrome/browser/ui/views/toolbar/toolbar_button.h"
#include "components/signin/public/base/signin_buildflags.h"
#include "components/signin/public/identity_manager/identity_manager.h"
#include "ui/base/metadata/metadata_header_macros.h"
#include "ui/events/event.h"

class AvatarToolbarButtonStateManager;
class Browser;
class BrowserView;
struct AccountInfo;
class GaiaId;
class StateProvider;

// This class takes care the Profile Avatar Button.
// Primarily applies UI configuration.
// It's data (text, icon, etc...) content are computed through the
// `AvatarToolbarButtonStateManager`, when relying on Chrome and Profile changes
// in order to adapt the expected content shown in the button.
class AvatarToolbarButton : public ToolbarButton,
                            public AvatarToolbarButtonInterface,
                            signin::IdentityManager::Observer {
  METADATA_HEADER(AvatarToolbarButton, ToolbarButton)
 public:
  using Observer = AvatarToolbarButtonInterface::Observer;

  explicit AvatarToolbarButton(BrowserView* browser);
  AvatarToolbarButton(const AvatarToolbarButton&) = delete;
  AvatarToolbarButton& operator=(const AvatarToolbarButton&) = delete;
  ~AvatarToolbarButton() override;

  // Attempts showing the In-Product-Help in a subsequent web sign-in when the
  // explicit browser sign-in preference was remembered.
  void MaybeShowExplicitBrowserSigninPreferenceRememberedIPH(
      const AccountInfo& account_info);

  // Returns true if a text is set and is visible.
  bool IsLabelPresentAndVisible() const;

  // AvatarToolbarButtonInterface:
  bool IsMouseHovered() const override;
  bool HasFocus() const override;
  views::DialogDelegate* GetDialogDelegate() override;
  void ButtonPressed(bool is_source_accelerator) override;
  [[nodiscard]] base::ScopedClosureRunner SetExplicitButtonState(
      const std::u16string& text,
      std::optional<std::u16string> accessibility_label,
      std::optional<base::RepeatingCallback<void(bool is_source_accelerator)>>
          explicit_action) override;
  bool HasExplicitButtonState() const override;
  void AddObserver(Observer* observer) override;
  void RemoveObserver(Observer* observer) override;
  // void UpdateIcon() also overrides ToolbarButton
  void UpdateText() override;
  void MaybeShowProfileSwitchIPH() override;
#if BUILDFLAG(IS_WIN) || BUILDFLAG(IS_MAC) || BUILDFLAG(IS_LINUX)
  void MaybeShowSupervisedUserSignInIPH() override;
  void MaybeShowSignInBenefitsIPH() override;
#endif
  void ClearActiveStateForTesting() override;
#if BUILDFLAG(ENABLE_DICE_SUPPORT)
  void ForceShowingPromoForTesting() override;
  bool GetStateAndFireSignedOutTriggerDelayTimerForTesting() override;
#endif

  // ToolbarButton:
  void OnMouseExited(const ui::MouseEvent& event) override;
  void OnBlur() override;
  void OnThemeChanged() override;
  void UpdateIcon() override;
  void Layout(PassKey) override;
  SkColor GetForegroundColor(ButtonState state) const override;
  std::optional<SkColor> GetHighlightTextColor() const override;
  std::optional<SkColor> GetHighlightBorderColor() const override;
  bool ShouldPaintBorder() const override;
  bool ShouldBlendHighlightColor() const override;
  void AddedToWidget() override;
  void OnBoundsChanged(const gfx::Rect& previous_bounds) override;

 private:
  FRIEND_TEST_ALL_PREFIXES(AvatarToolbarButtonTest,
                           HighlightMeetsMinimumContrast);

  // signin::IdentityManager::Observer:
  void OnPrimaryAccountChanged(
      const signin::PrimaryAccountChangeEvent& event_details) override;
  void OnExtendedAccountInfoUpdated(const AccountInfo& info) override;

  // ui::PropertyHandler:
  void AfterPropertyChange(const void* key, int64_t old_value) override;

  // Swaps STATE_NORMAL icon between normal and hovered versions based on
  // ink drop highlight state. Called when the highlight changes in
  // forced-colors mode.
  void OnInkDropHighlightedChanged();

  // Updates the layout insets depending on whether it is a chip or a button.
  void UpdateLayoutInsets();

  // Updates the inkdrop highlight and ripple properties depending on the state
  // and whether the chip is expanded.
  void UpdateInkdrop();

  // Animates hiding/shrinking the button according to the text changes.
  void AnimateTextChange(StateProvider* state_provider,
                         const ui::ColorProvider* color_provider);
  void UpdateAccessibilityLabel();

  // LabelButton:
  gfx::Size CalculatePreferredSize(
      const views::SizeBounds& available_size) const override;

  // gfx::AnimationDelegate:
  void AnimationProgressed(const gfx::Animation* animation) override;
  void AnimationEnded(const gfx::Animation* animation) override;

  // Lists of observers.
  // TODO(crbug.com/484371187): Investigate if reentrancy can be removed.
  base::ObserverList<
      Observer,
      /*check_empty=*/true,
      base::ObserverListReentrancyPolicy::kAllowReentrancyUntriaged>
      observer_list_;

  std::unique_ptr<AvatarToolbarButtonStateManager> state_manager_;

  const raw_ptr<Browser> browser_;

  // Time when this object was created.
  const base::TimeTicks creation_time_;

  // Gaia Id of the account that was signed in from having it's choice
  // remembered following a web sign-in event but waiting for the available
  // account information to be fetched in order to show the sign in IPH.
  GaiaId gaia_id_for_signin_choice_remembered_;

  // Cached icons for the placeholder avatar in forced-colors mode, to avoid
  // recomputing on every ink drop highlight change. Empty when not in
  // forced-colors mode or when the icon is not a placeholder.
  ui::ImageModel forced_colors_normal_icon_;
  ui::ImageModel forced_colors_hovered_icon_;

  // Subscription for ink drop highlight changes (forced-colors mode).
  base::CallbackListSubscription ink_drop_highlight_subscription_;

  gfx::SlideAnimation slide_animation_;

  base::ScopedObservation<signin::IdentityManager,
                          signin::IdentityManager::Observer>
      identity_manager_observation_{this};

  base::WeakPtrFactory<AvatarToolbarButton> weak_ptr_factory_{this};
};

#endif  // CHROME_BROWSER_UI_VIEWS_PROFILES_AVATAR_TOOLBAR_BUTTON_H_
