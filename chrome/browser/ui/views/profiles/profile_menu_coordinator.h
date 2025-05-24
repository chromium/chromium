// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_VIEWS_PROFILES_PROFILE_MENU_COORDINATOR_H_
#define CHROME_BROWSER_UI_VIEWS_PROFILES_PROFILE_MENU_COORDINATOR_H_

#include "chrome/browser/ui/browser_user_data.h"
#include "ui/views/view_tracker.h"

class ProfileMenuViewBase;

// Handles the lifetime and showing/hidden state of the profile menu bubble.
// Owned by the associated browser.
class ProfileMenuCoordinator : public BrowserUserData<ProfileMenuCoordinator> {
 public:
  ~ProfileMenuCoordinator() override;

  // Shows the the profile bubble for this browser.
  //
  // If `explicit_signin_access_point` is set, the signin (or sync) flow will be
  // started with this access point. Otherwise, the default access point will be
  // used (`signin_metrics::AccessPoint::kAvatarBubbleSignIn*`).
  void Show(bool is_source_accelerator,
            std::optional<signin_metrics::AccessPoint>
                explicit_signin_access_point = std::nullopt);

  // Returns true if the bubble is currently showing for the owning browser.
  bool IsShowing() const;

  ProfileMenuViewBase* GetProfileMenuViewBaseForTesting();

 private:
  friend class BrowserUserData<ProfileMenuCoordinator>;

  explicit ProfileMenuCoordinator(Browser* browser);

  views::ViewTracker bubble_tracker_;

  BROWSER_USER_DATA_KEY_DECL();
};

#endif  // CHROME_BROWSER_UI_VIEWS_PROFILES_PROFILE_MENU_COORDINATOR_H_
