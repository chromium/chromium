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
  METADATA_HEADER(AvatarToolbarButton, ToolbarButton)

 public:
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
  std::optional<SkColor> GetHighlightTextColor() const override;
  std::optional<SkColor> GetHighlightBorderColor() const override;
  bool ShouldPaintBorder() const override;
  bool ShouldBlendHighlightColor() const override;

  void ShowAvatarHighlightAnimation();

#if !BUILDFLAG(IS_CHROMEOS_LACROS) || BUILDFLAG(IS_CHROMEOS_ASH)
  // Expands the pill to show the signin text.
  void ShowSignInText();
  // Contracts the pill so that no text is shown.
  void HideSignInText();
#endif

  // Control whether the button action is active or not.
  // One reason to disable the action; when a bubble is shown from this button
  // (and not the profile menu), we want to disable the button action, however
  // the button should remain in an "active" state from a UI perspective.
  void SetButtonActionDisabled(bool disabled);
  bool IsButtonActionDisabled() const;

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

  // Controls the action of the button, on press.
  // Setting this to true will stop the button reaction but the button will
  // remain in active state, not affecting it's UI in any way.
  bool button_action_disabled_ = false;

  base::ObserverList<Observer>::Unchecked observer_list_;

  base::WeakPtrFactory<AvatarToolbarButton> weak_ptr_factory_{this};
};

#endif  // CHROME_BROWSER_UI_VIEWS_PROFILES_AVATAR_TOOLBAR_BUTTON_H_
