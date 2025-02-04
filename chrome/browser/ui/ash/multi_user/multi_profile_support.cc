// Copyright 2013 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/ash/multi_user/multi_profile_support.h"

#include <set>
#include <vector>

#include "ash/public/cpp/multi_user_window_manager.h"
#include "base/metrics/histogram_macros.h"
#include "base/scoped_observation.h"
#include "base/strings/string_util.h"
#include "chrome/browser/ash/app_restore/full_restore_service.h"
#include "chrome/browser/ash/app_restore/full_restore_service_factory.h"
#include "chrome/browser/ash/floating_workspace/floating_workspace_util.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/profiles/profile_manager.h"
#include "chrome/browser/ui/ash/multi_user/multi_user_util.h"
#include "chrome/browser/ui/ash/session/session_controller_client_impl.h"
#include "chrome/browser/ui/ash/session/session_util.h"
#include "chrome/browser/ui/ash/shelf/chrome_shelf_controller.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/browser/ui/browser_finder.h"
#include "chrome/browser/ui/browser_list.h"
#include "chrome/browser/ui/browser_window.h"
#include "components/app_restore/full_restore_utils.h"
#include "extensions/browser/app_window/app_window.h"
#include "extensions/browser/app_window/app_window_registry.h"
#include "ui/aura/client/aura_constants.h"
#include "ui/aura/window.h"
#include "ui/base/ui_base_types.h"

// This class keeps track of all applications which were started for a user.
// When an app gets created, the window will be tagged for that user. Note
// that the destruction does not need to be tracked here since the universal
// window observer will take care of that.
class AppObserver : public extensions::AppWindowRegistry::Observer {
 public:
  explicit AppObserver(extensions::AppWindowRegistry* registry,
                       const std::string& user_id)
      : user_id_(user_id) {
    app_window_registry_observer_.Observe(registry);
  }

  AppObserver(const AppObserver&) = delete;
  AppObserver& operator=(const AppObserver&) = delete;

  ~AppObserver() override = default;

  // AppWindowRegistry::Observer overrides:
  void OnAppWindowAdded(extensions::AppWindow* app_window) override {
    aura::Window* window = app_window->GetNativeWindow();
    DCHECK(window);
    MultiUserWindowManagerHelper::GetWindowManager()->SetWindowOwner(
        window, AccountId::FromUserEmail(user_id_));
  }

 private:
  std::string user_id_;

  base::ScopedObservation<extensions::AppWindowRegistry,
                          extensions::AppWindowRegistry::Observer>
      app_window_registry_observer_{this};
};

// static
MultiProfileSupport* MultiProfileSupport::instance_ = nullptr;

MultiProfileSupport::MultiProfileSupport(const AccountId& current_account_id) {
  DCHECK(!instance_);
  instance_ = this;
  multi_user_window_manager_ =
      ash::MultiUserWindowManager::Create(this, current_account_id);
}

MultiProfileSupport::~MultiProfileSupport() {
  DCHECK_EQ(instance_, this);
  instance_ = nullptr;

  BrowserList::RemoveObserver(this);

  // This may trigger callbacks to us, delete it early on.
  multi_user_window_manager_.reset();

  // Remove all app observers.
  account_id_to_app_observer_.clear();
}

void MultiProfileSupport::Init() {
  // Since we are setting the SessionStateObserver and adding the user, this
  // function should get called only once.
  auto current_account_id = multi_user_window_manager_->CurrentAccountId();
  DCHECK(account_id_to_app_observer_.find(current_account_id) ==
         account_id_to_app_observer_.end());

  BrowserList::AddObserver(this);

  // Add an app window observer & all already running apps.
  Profile* profile =
      multi_user_util::GetProfileFromAccountId(current_account_id);
  if (profile) {
    AddUser(profile);
  }
}

void MultiProfileSupport::AddUser(content::BrowserContext* context) {
  Profile* profile = Profile::FromBrowserContext(context);
  const AccountId& account_id(
      multi_user_util::GetAccountIdFromProfile(profile));
  if (account_id_to_app_observer_.find(account_id) !=
      account_id_to_app_observer_.end()) {
    return;
  }

  account_id_to_app_observer_[account_id] = std::make_unique<AppObserver>(
      extensions::AppWindowRegistry::Get(profile), account_id.GetUserEmail());

  // Account all existing application windows of this user accordingly.
  const extensions::AppWindowRegistry::AppWindowList& app_windows =
      extensions::AppWindowRegistry::Get(profile)->app_windows();
  extensions::AppWindowRegistry::AppWindowList::const_iterator it =
      app_windows.begin();
  for (; it != app_windows.end(); ++it) {
    account_id_to_app_observer_[account_id]->OnAppWindowAdded(*it);
  }

  // Account all existing browser windows of this user accordingly.
  for (Browser* browser : *BrowserList::GetInstance()) {
    if (browser->profile()->IsSameOrParent(profile)) {
      OnBrowserAdded(browser);
    }
  }
}

void MultiProfileSupport::OnBrowserAdded(Browser* browser) {
  // A unit test (e.g. CrashRestoreComplexTest.RestoreSessionForThreeUsers) can
  // come here with no valid window.
  if (!browser->window() || !browser->window()->GetNativeWindow()) {
    return;
  }
  multi_user_window_manager_->SetWindowOwner(
      browser->window()->GetNativeWindow(),
      multi_user_util::GetAccountIdFromProfile(browser->profile()));
}

void MultiProfileSupport::OnWindowOwnerEntryChanged(aura::Window* window,
                                                    const AccountId& account_id,
                                                    bool was_minimized,
                                                    bool teleported) {
  const AccountId& owner = multi_user_window_manager_->GetWindowOwner(window);
  // Browser windows don't use kAvatarIconKey. See
  // BrowserNonClientFrameViewAsh::UpdateProfileIcons().
  if (owner.is_valid() && !chrome::FindBrowserWithWindow(window)) {
    const user_manager::User* const window_owner =
        user_manager::UserManager::IsInitialized()
            ? user_manager::UserManager::Get()->FindUser(owner)
            : nullptr;
    if (window_owner && teleported) {
      window->SetProperty(
          aura::client::kAvatarIconKey,
          new gfx::ImageSkia(GetAvatarImageForUser(window_owner)));
    } else {
      window->ClearProperty(aura::client::kAvatarIconKey);
    }
  }
}

void MultiProfileSupport::OnTransitionUserShelfToNewAccount() {
  Profile* profile = ProfileManager::GetActiveUserProfile();
  full_restore::SetActiveProfilePath(profile->GetPath());

  // Only init full restore when floating workspace is disabled or in safe mode.
  // TODO(b/312233508): Add fws test coverage for this case.
  if (!ash::floating_workspace_util::ShouldHandleRestartRestore()) {
    auto* full_restore_service =
        ash::full_restore::FullRestoreServiceFactory::GetForProfile(profile);
    if (full_restore_service) {
      full_restore_service->OnTransitionedToNewActiveUser(profile);
    }
  }

  ChromeShelfController* chrome_shelf_controller =
      ChromeShelfController::instance();
  // Some unit tests have no ChromeShelfController.
  if (!chrome_shelf_controller) {
    return;
  }
  chrome_shelf_controller->ActiveUserChanged(
      multi_user_window_manager_->CurrentAccountId());
}
