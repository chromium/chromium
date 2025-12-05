// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_VIEWS_PROFILES_PROFILE_MENU_COORDINATOR_H_
#define CHROME_BROWSER_UI_VIEWS_PROFILES_PROFILE_MENU_COORDINATOR_H_

#include <optional>

#include "base/memory/raw_ref.h"
#include "base/memory/weak_ptr.h"
#include "chrome/browser/signin/signin_promo_util.h"
#include "ui/views/view_tracker.h"

class BrowserWindowInterface;
class Profile;
class ProfileMenuViewBase;

namespace signin_metrics {
enum class AccessPoint;
}  // namespace signin_metrics

// Handles the lifetime and showing/hidden state of the profile menu bubble.
// Owned by the associated browser.
class ProfileMenuCoordinator {
 public:
  ProfileMenuCoordinator(BrowserWindowInterface* browser, Profile* profile);
  ProfileMenuCoordinator(const ProfileMenuCoordinator&) = delete;
  ProfileMenuCoordinator& operator=(const ProfileMenuCoordinator&) = delete;
  ~ProfileMenuCoordinator();

  // Shows the the profile bubble for this browser.
  //
  // If `from_avatar_promo` is set, then trigger of the menu originated from a
  // promo that was shown on the AvatarButton.
  void Show(bool is_source_accelerator, bool from_avatar_promo = false);

  // Returns true if the bubble is currently showing for the owning browser.
  bool IsShowing() const;

  ProfileMenuViewBase* GetProfileMenuViewBaseForTesting();

 private:
  BrowserWindowInterface* GetBrowser();
  Profile* GetProfile();

  void ShowWithPromoResults(bool is_source_accelerator,
                            bool from_avatar_promo
#if BUILDFLAG(ENABLE_DICE_SUPPORT)
                            ,
                            signin::ProfileMenuAvatarButtonPromoInfo promo_info
#endif
  );

  // TODO(crbug.com/425953501): Replace with `ToolbarButtonProvider` once this
  // bug is fixed.
  const raw_ref<BrowserWindowInterface> browser_;

  const raw_ref<Profile> profile_;
  views::ViewTracker bubble_tracker_;

  base::WeakPtrFactory<ProfileMenuCoordinator> weak_pointer_factory_{this};
};

#endif  // CHROME_BROWSER_UI_VIEWS_PROFILES_PROFILE_MENU_COORDINATOR_H_
