// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_VIEWS_PROFILES_AVATAR_TOOLBAR_BUTTON_H_
#define CHROME_BROWSER_UI_VIEWS_PROFILES_AVATAR_TOOLBAR_BUTTON_H_

#include "base/feature_list.h"
#include "base/memory/weak_ptr.h"
#include "base/scoped_observation.h"
#include "chrome/browser/ui/views/toolbar/toolbar_button.h"
#include "chrome/browser/ui/views/toolbar/toolbar_icon_container_view.h"
#include "ui/events/event.h"
#include "ui/views/metadata/metadata_header_macros.h"

class AvatarToolbarButtonDelegate;
class Browser;
class BrowserView;
class FeaturePromoControllerViews;

class AvatarToolbarButton : public ToolbarButton,
                            ToolbarIconContainerView::Observer {
 public:
  METADATA_HEADER(AvatarToolbarButton);

  // States of the button ordered in priority of getting displayed.
  enum class State {
    kIncognitoProfile,
    kGuestSession,
    kGenericProfile,
    kAnimatedUserIdentity,
    kSyncPaused,
    kSyncError,
    kPasswordsOnlySyncError,
    kNormal
  };

  class Observer {
   public:
    virtual ~Observer() = default;

    virtual void OnAvatarHighlightAnimationFinished() = 0;
  };

  // TODO(crbug.com/922525): Remove this constructor when this button always has
  // ToolbarIconContainerView as a parent.
  explicit AvatarToolbarButton(BrowserView* browser);
  AvatarToolbarButton(BrowserView* browser_view,
                      ToolbarIconContainerView* parent);
  AvatarToolbarButton(const AvatarToolbarButton&) = delete;
  AvatarToolbarButton& operator=(const AvatarToolbarButton&) = delete;
  ~AvatarToolbarButton() override;

  void UpdateText();
  void ShowAvatarHighlightAnimation();

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

  // ToolbarIconContainerView::Observer:
  void OnHighlightChanged() override;

  // Can be used in tests to reduce or remove the delay before showing the IPH.
  static void SetIPHMinDelayAfterCreationForTesting(base::TimeDelta delay);

 protected:
  // ToolbarButton:
  void NotifyClick(const ui::Event& event) override;

 private:
  FRIEND_TEST_ALL_PREFIXES(AvatarToolbarButtonTest,
                           HighlightMeetsMinimumContrast);

  // ui::PropertyHandler:
  void AfterPropertyChange(const void* key, int64_t old_value) override;

  std::u16string GetAvatarTooltipText() const;
  ui::ImageModel GetAvatarIcon(ButtonState state,
                               const gfx::Image& profile_identity_image) const;

  void SetInsets();

  // Attempts to show the in-product help for profile switching. This function
  // should only be called after the backend is initialized. Otherwise prefer
  // calling MaybeShowProfileSwitchIPH().
  void MaybeShowProfileSwitchIPHInitialized(bool success);

  std::unique_ptr<AvatarToolbarButtonDelegate> delegate_;

  Browser* const browser_;
  ToolbarIconContainerView* const parent_;

  // Time when this object was created.
  const base::TimeTicks creation_time_;

  // Do not show the IPH right when creating the window, so that the IPH has a
  // separate animation.
  static base::TimeDelta g_iph_min_delay_after_creation;

  FeaturePromoControllerViews* const feature_promo_controller_;

  base::ObserverList<Observer>::Unchecked observer_list_;

  base::WeakPtrFactory<AvatarToolbarButton> weak_ptr_factory_{this};
};

#endif  // CHROME_BROWSER_UI_VIEWS_PROFILES_AVATAR_TOOLBAR_BUTTON_H_
