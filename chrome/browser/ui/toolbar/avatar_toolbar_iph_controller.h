// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_TOOLBAR_AVATAR_TOOLBAR_IPH_CONTROLLER_H_
#define CHROME_BROWSER_UI_TOOLBAR_AVATAR_TOOLBAR_IPH_CONTROLLER_H_

#include "base/auto_reset.h"
#include "base/memory/raw_ptr.h"
#include "base/memory/raw_ref.h"
#include "base/memory/weak_ptr.h"
#include "base/time/time.h"
#include "ui/base/unowned_user_data/scoped_unowned_user_data.h"

class BrowserWindowInterface;
class Profile;

namespace signin {
class IdentityManager;
}  // namespace signin

// Owns the profile switch and supervised sign-in IPHs anchored to a window's
// avatar toolbar button.
// Owned by BrowserWindowFeatures.
class AvatarToolbarIphController {
 public:
  DECLARE_USER_DATA(AvatarToolbarIphController);

  AvatarToolbarIphController(BrowserWindowInterface& browser,
                             Profile& profile,
                             signin::IdentityManager* identity_manager);
  AvatarToolbarIphController(const AvatarToolbarIphController&) = delete;
  AvatarToolbarIphController& operator=(const AvatarToolbarIphController&) =
      delete;
  ~AvatarToolbarIphController();

  static AvatarToolbarIphController* From(BrowserWindowInterface* browser);

  // Attempts to show the profile switch IPH. If profile customization is shown,
  // the IPH should be shown after it.
  void MaybeShowProfileSwitchIPH();

  // Attempts to show the IPH for new signed-in supervised profiles created by
  // FRE, the profile picker, profile menu or chrome menu. If profile
  // customization is shown, the IPH should be shown after it.
  // TODO(351333491): Specify whether this should also be invoked on signing in
  // or turning on sync for an existing profile.
  void MaybeShowSupervisedUserProfileSignInIPH();

  // Overrides the minimum IPH delay after controller creation.
  [[nodiscard]] static base::AutoReset<base::TimeDelta>
  SetScopedIPHMinDelayAfterCreationForTesting(base::TimeDelta delay);

 private:
  bool ShouldShowAvatarToolbarIPH();

  // Recheck avatar availability for both new requests and delayed retries.
  void MaybeShowProfileSwitchIPHImpl();
  void MaybeShowSupervisedUserProfileSignInIPHImpl();

  const raw_ref<BrowserWindowInterface> browser_;
  const raw_ref<Profile> profile_;
  // May be null for profiles that do not support sign-in.
  const raw_ptr<signin::IdentityManager> identity_manager_;
  const base::TimeTicks creation_time_;
  ui::ScopedUnownedUserData<AvatarToolbarIphController>
      scoped_unowned_user_data_;
  base::WeakPtrFactory<AvatarToolbarIphController> weak_ptr_factory_{this};
};

#endif  // CHROME_BROWSER_UI_TOOLBAR_AVATAR_TOOLBAR_IPH_CONTROLLER_H_
