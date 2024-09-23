// Copyright 2015 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/ash/session/session_util.h"

#include "ash/public/cpp/multi_user_window_manager.h"
#include "build/build_config.h"
#include "chrome/browser/ash/profiles/profile_helper.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/profiles/profile_manager.h"
#include "chrome/browser/ui/ash/multi_user/multi_user_util.h"
#include "chrome/browser/ui/ash/multi_user/multi_user_window_manager_helper.h"
#include "chrome/grit/theme_resources.h"
#include "components/user_manager/user_manager.h"
#include "content/public/browser/browser_context.h"
#include "ui/aura/window.h"
#include "ui/base/resource/resource_bundle.h"
#include "ui/gfx/image/image_skia_operations.h"

namespace {

// Gets the browser context (profile) associated with |window|. Either the
// profile of the user who owns the window or the profile of the desktop on
// which the window is positioned (for teleported windows) is returned, based on
// |presenting|.
const content::BrowserContext* GetBrowserContextForWindow(
    const aura::Window* window,
    bool presenting) {
  DCHECK(window);
  auto* window_manager = MultiUserWindowManagerHelper::GetWindowManager();
  // Speculative fix for multi-profile crash. crbug.com/661821
  if (!window_manager)
    return nullptr;

  const AccountId& account_id =
      presenting ? window_manager->GetUserPresentingWindow(window)
                 : window_manager->GetWindowOwner(window);
  return account_id.is_valid()
             ? multi_user_util::GetProfileFromAccountId(account_id)
             : nullptr;
}

}  // namespace

const content::BrowserContext* GetActiveBrowserContext() {
  DCHECK(user_manager::UserManager::Get()->GetLoggedInUsers().size());
  return ProfileManager::GetActiveUserProfile();
}

bool CanShowWindowForUser(
    const aura::Window* window,
    const GetActiveBrowserContextCallback& get_context_callback) {
  DCHECK(window);
  if (user_manager::UserManager::Get()->GetLoggedInUsers().size() > 1u) {
    const content::BrowserContext* active_browser_context =
        get_context_callback.Run();
    const content::BrowserContext* owner_browser_context =
        GetBrowserContextForWindow(window, false);
    const content::BrowserContext* shown_browser_context =
        GetBrowserContextForWindow(window, true);

    if (owner_browser_context && active_browser_context &&
        owner_browser_context != active_browser_context &&
        shown_browser_context != active_browser_context) {
      return false;
    }
  }
  return true;
}

gfx::ImageSkia GetAvatarImageForContext(content::BrowserContext* context) {
  return GetAvatarImageForUser(ash::ProfileHelper::Get()->GetUserByProfile(
      Profile::FromBrowserContext(context)));
}

gfx::ImageSkia GetAvatarImageForUser(const user_manager::User* user) {
  static const gfx::ImageSkia* holder =
      ui::ResourceBundle::GetSharedInstance().GetImageSkiaNamed(
          IDR_AVATAR_HOLDER);
  static const gfx::ImageSkia* holder_mask =
      ui::ResourceBundle::GetSharedInstance().GetImageSkiaNamed(
          IDR_AVATAR_HOLDER_MASK);

  gfx::ImageSkia user_image = user->GetImage();
  gfx::ImageSkia resized = gfx::ImageSkiaOperations::CreateResizedImage(
      user_image, skia::ImageOperations::RESIZE_BEST, holder->size());
  gfx::ImageSkia masked =
      gfx::ImageSkiaOperations::CreateMaskedImage(resized, *holder_mask);
  return gfx::ImageSkiaOperations::CreateSuperimposedImage(*holder, masked);
}
