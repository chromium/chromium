// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_VIEWS_PROFILES_AVATAR_TOOLBAR_BUTTON_H_
#define CHROME_BROWSER_UI_VIEWS_PROFILES_AVATAR_TOOLBAR_BUTTON_H_

#include "base/feature_list.h"
#include "base/macros.h"
#include "base/memory/weak_ptr.h"
#include "base/scoped_observation.h"
#include "chrome/browser/ui/views/toolbar/toolbar_button.h"
#include "chrome/browser/ui/views/toolbar/toolbar_icon_container_view.h"
#include "ui/events/event.h"

class AvatarToolbarButtonDelegate;
class Browser;

class AvatarToolbarButton : public ToolbarButton,
                            ToolbarIconContainerView::Observer {
 public:
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
  explicit AvatarToolbarButton(Browser* browser);
  AvatarToolbarButton(Browser* browser, ToolbarIconContainerView* parent);
  ~AvatarToolbarButton() override;

  void UpdateText();
  void ShowAvatarHighlightAnimation();
  bool IsParentHighlighted() const;

  void AddObserver(Observer* observer);
  void RemoveObserver(Observer* observer);

  void NotifyHighlightAnimationFinished();

  // ToolbarButton:
  const char* GetClassName() const override;
  void OnMouseExited(const ui::MouseEvent& event) override;
  void OnBlur() override;
  void OnThemeChanged() override;
  void UpdateIcon() override;
  void Layout() override;

  // ToolbarIconContainerView::Observer:
  void OnHighlightChanged() override;

  static const char kAvatarToolbarButtonClassName[];

 protected:
  // ToolbarButton:
  void NotifyClick(const ui::Event& event) override;

 private:
  FRIEND_TEST_ALL_PREFIXES(AvatarToolbarButtonTest,
                           HighlightMeetsMinimumContrast);

  base::string16 GetAvatarTooltipText() const;
  ui::ImageModel GetAvatarIcon(ButtonState state,
                               const gfx::Image& profile_identity_image) const;

  void SetInsets();

  std::unique_ptr<AvatarToolbarButtonDelegate> delegate_;

  Browser* const browser_;
  ToolbarIconContainerView* const parent_;

  base::ObserverList<Observer>::Unchecked observer_list_;

  base::WeakPtrFactory<AvatarToolbarButton> weak_ptr_factory_{this};

  DISALLOW_COPY_AND_ASSIGN(AvatarToolbarButton);
};

#endif  // CHROME_BROWSER_UI_VIEWS_PROFILES_AVATAR_TOOLBAR_BUTTON_H_
