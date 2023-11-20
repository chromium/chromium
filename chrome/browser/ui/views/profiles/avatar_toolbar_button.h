// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_VIEWS_PROFILES_AVATAR_TOOLBAR_BUTTON_H_
#define CHROME_BROWSER_UI_VIEWS_PROFILES_AVATAR_TOOLBAR_BUTTON_H_

#include "base/gtest_prod_util.h"
#include "base/memory/raw_ptr.h"
#include "base/memory/weak_ptr.h"
#include "base/observer_list.h"
#include "base/time/time.h"
#include "build/chromeos_buildflags.h"
#include "chrome/browser/ui/views/toolbar/toolbar_button.h"
#include "ui/base/metadata/metadata_header_macros.h"
#include "ui/events/event.h"

class AvatarToolbarButtonDelegate;
class Browser;
class BrowserView;

class AvatarToolbarButton : public ToolbarButton {
 public:
  METADATA_HEADER(AvatarToolbarButton);

  // States of the button ordered in priority of getting displayed.
  enum class State {
    kIncognitoProfile,
    kGuestSession,
    kSignInTextShowing,
    kAnimatedUserIdentity,
    kSyncPaused,
    // An error in sync-the-feature or sync-the-transport.
    kSyncError,
    kNormal
  };

  class Observer {
   public:
    virtual ~Observer() = default;

    virtual void OnAvatarHighlightAnimationFinished() = 0;
  };

  explicit AvatarToolbarButton(BrowserView* browser);
  AvatarToolbarButton(const AvatarToolbarButton&) = delete;
  AvatarToolbarButton& operator=(const AvatarToolbarButton&) = delete;
  ~AvatarToolbarButton() override;

  void UpdateText();
  absl::optional<SkColor> GetHighlightTextColor() const override;
  absl::optional<SkColor> GetHighlightBorderColor() const override;
  bool ShouldPaintBorder() const override;
  bool ShouldBlendHighlightColor() const override;

  void ShowAvatarHighlightAnimation();

#if !BUILDFLAG(IS_CHROMEOS_LACROS) || BUILDFLAG(IS_CHROMEOS_ASH)
  // Expands the pill to show the signin text.
  void ShowSignInText();
  // Contracts the pill so that no text is shown.
  void HideSignInText();

  void DisableActionButton();
  void ResetActionButton();
#endif

  void AddObserver(Observer* observer);
  void RemoveObserver(Observer* observer);

  void NotifyHighlightAnimationFinished();

  // Attempts showing the In-Produce-Help for profile Switching.
  void MaybeShowProfileSwitchIPH();

  // ToolbarButton:
  void OnMouseExited(const ui::MouseEvent& event) override;
  void OnBlur() override;
  void OnThemeChanged() override;
  void UpdateIcon() override;
  void Layout() override;
  int GetIconSize() const override;
  SkColor GetForegroundColor(ButtonState state) const override;

  // Returns true if a text is set and is visible.
  bool IsLabelPresentAndVisible() const;

  // Updates the inkdrop highlight and ripple properties depending on the state
  // and
  // whether the chip is expanded.
  void UpdateInkdrop();

  // Can be used in tests to reduce or remove the delay before showing the IPH.
  static void SetIPHMinDelayAfterCreationForTesting(base::TimeDelta delay);

 private:
  FRIEND_TEST_ALL_PREFIXES(AvatarToolbarButtonTest,
                           HighlightMeetsMinimumContrast);

  // Struct to store the button state before overriding the disabled state.
  class DisabledStateHelper {
   public:
    void Init(bool previous_enable_state, SkColor previous_disabled_text_color);

    bool GetPreviousEnableState() const;
    SkColor GetPreviousDisabledTextColor() const;

   private:
    bool init_ = false;

    bool previous_enable_state_ = true;
    SkColor previous_disabled_text_color_;
  };

  // ui::PropertyHandler:
  void AfterPropertyChange(const void* key, int64_t old_value) override;

  void ButtonPressed();

  std::u16string GetAvatarTooltipText() const;
  ui::ImageModel GetAvatarIcon(ButtonState state,
                               const gfx::Image& profile_identity_image) const;

  void SetInsets();

  // Updates the layout insets depending on whether it is a chip or a button.
  void UpdateLayoutInsets();

  std::unique_ptr<AvatarToolbarButtonDelegate> delegate_;

  const raw_ptr<Browser> browser_;

  // Time when this object was created.
  const base::TimeTicks creation_time_;

  // Do not show the IPH right when creating the window, so that the IPH has a
  // separate animation.
  static base::TimeDelta g_iph_min_delay_after_creation;

  DisabledStateHelper disabled_state_helper_;

  base::ObserverList<Observer>::Unchecked observer_list_;

  base::WeakPtrFactory<AvatarToolbarButton> weak_ptr_factory_{this};
};

#endif  // CHROME_BROWSER_UI_VIEWS_PROFILES_AVATAR_TOOLBAR_BUTTON_H_
