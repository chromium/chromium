// Copyright 2013 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/ash/multi_user/multi_user_window_manager_browser_adaptor.h"

#include <set>
#include <vector>

#include "ash/multi_user/multi_user_window_manager.h"
#include "base/check_deref.h"
#include "base/metrics/histogram_macros.h"
#include "base/scoped_observation.h"
#include "base/strings/string_util.h"
#include "chrome/browser/ash/app_restore/full_restore_service.h"
#include "chrome/browser/ash/app_restore/full_restore_service_factory.h"
#include "chrome/browser/ash/browser_delegate/browser_controller.h"
#include "chrome/browser/ash/floating_workspace/floating_workspace_util.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/profiles/profile_manager.h"
#include "chrome/browser/ui/ash/multi_user/multi_user_util.h"
#include "chrome/browser/ui/ash/session/session_controller_client_impl.h"
#include "chrome/browser/ui/ash/session/session_util.h"
#include "chrome/browser/ui/ash/shelf/chrome_shelf_controller.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/browser/ui/browser_list.h"
#include "chrome/browser/ui/browser_window.h"
#include "chrome/browser/ui/browser_window/public/browser_window_interface_iterator.h"
#include "chromeos/ash/components/browser_context_helper/browser_context_helper.h"
#include "components/app_restore/full_restore_utils.h"
#include "extensions/browser/app_window/app_window.h"
#include "extensions/browser/app_window/app_window_registry.h"
#include "ui/aura/client/aura_constants.h"
#include "ui/aura/window.h"
#include "ui/base/ui_base_types.h"

namespace ash {

// This class keeps track of all applications which were started for a user.
// When an app gets created, the window will be tagged for that user. Note
// that the destruction does not need to be tracked here since the universal
// window observer will take care of that.
class MultiUserWindowManagerBrowserAdaptor::AppObserver
    : public extensions::AppWindowRegistry::Observer {
 public:
  AppObserver(MultiUserWindowManager* multi_user_window_manager,
              extensions::AppWindowRegistry* registry,
              const AccountId& account_id)
      : multi_user_window_manager_(CHECK_DEREF(multi_user_window_manager)),
        account_id_(account_id) {
    app_window_registry_observer_.Observe(registry);
  }

  AppObserver(const AppObserver&) = delete;
  AppObserver& operator=(const AppObserver&) = delete;

  ~AppObserver() override = default;

  // AppWindowRegistry::Observer overrides:
  void OnAppWindowAdded(extensions::AppWindow* app_window) override {
    aura::Window* window = app_window->GetNativeWindow();
    DCHECK(window);
    multi_user_window_manager_->SetWindowOwner(window, account_id_);
  }

 private:
  raw_ref<MultiUserWindowManager> multi_user_window_manager_;
  AccountId account_id_;

  base::ScopedObservation<extensions::AppWindowRegistry,
                          extensions::AppWindowRegistry::Observer>
      app_window_registry_observer_{this};
};

MultiUserWindowManagerBrowserAdaptor::MultiUserWindowManagerBrowserAdaptor(
    MultiUserWindowManager* multi_user_window_manager)
    : multi_user_window_manager_(CHECK_DEREF(multi_user_window_manager)) {
  multi_user_window_manager_observation_.Observe(
      &multi_user_window_manager_.get());

  BrowserList::AddObserver(this);
}

MultiUserWindowManagerBrowserAdaptor::~MultiUserWindowManagerBrowserAdaptor() {
  BrowserList::RemoveObserver(this);

  // Remove all app observers.
  account_id_to_app_observer_.clear();
}

void MultiUserWindowManagerBrowserAdaptor::AddUser(
    const AccountId& account_id) {
  // AddUser must not be called twice for the same account_id.
  CHECK(account_id_to_app_observer_.find(account_id) ==
        account_id_to_app_observer_.end());

  // This must be called after User's Profile gets ready.
  Profile* profile = Profile::FromBrowserContext(
      BrowserContextHelper::Get()->GetBrowserContextByAccountId(account_id));
  CHECK(profile);

  auto app_observer = std::make_unique<AppObserver>(
      &multi_user_window_manager_.get(),
      extensions::AppWindowRegistry::Get(profile), account_id);
  auto* app_observer_ptr = app_observer.get();
  account_id_to_app_observer_.try_emplace(account_id, std::move(app_observer));

  // Account all existing application windows of this user accordingly.
  const extensions::AppWindowRegistry::AppWindowList& app_windows =
      extensions::AppWindowRegistry::Get(profile)->app_windows();
  for (extensions::AppWindow* app_window : app_windows) {
    app_observer_ptr->OnAppWindowAdded(app_window);
  }

  // Account all existing browser windows of this user accordingly.
  ForEachCurrentBrowserWindowInterfaceOrderedByActivation(
      [&](BrowserWindowInterface* browser) {
        if (browser->GetProfile()->IsSameOrParent(profile)) {
          OnBrowserAdded(browser->GetBrowserForMigrationOnly());
        }
        return true;
      });
}

void MultiUserWindowManagerBrowserAdaptor::OnBrowserAdded(Browser* browser) {
  // A unit test (e.g. CrashRestoreComplexTest.RestoreSessionForThreeUsers) can
  // come here with no valid window.
  if (!browser->window() || !browser->window()->GetNativeWindow()) {
    return;
  }
  multi_user_window_manager_->SetWindowOwner(
      browser->window()->GetNativeWindow(),
      multi_user_util::GetAccountIdFromProfile(browser->profile()));
}

void MultiUserWindowManagerBrowserAdaptor::OnWindowOwnerEntryChanged(
    aura::Window* window,
    const AccountId& account_id,
    bool was_minimized,
    bool teleported) {
  const AccountId& owner = multi_user_window_manager_->GetWindowOwner(window);
  // Browser windows don't use kAvatarIconKey. See
  // BrowserFrameViewAsh::UpdateProfileIcons().
  if (owner.is_valid() &&
      !ash::BrowserController::GetInstance()->GetBrowserForWindow(window)) {
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

void MultiUserWindowManagerBrowserAdaptor::OnTransitionUserShelfToNewAccount() {
  Profile* profile = ProfileManager::GetActiveUserProfile();
  ::full_restore::SetActiveProfilePath(profile->GetPath());

  // Only init full restore when floating workspace is disabled or in safe mode.
  // TODO(b/312233508): Add fws test coverage for this case.
  if (!floating_workspace_util::ShouldHandleRestartRestore()) {
    auto* full_restore_service =
        full_restore::FullRestoreServiceFactory::GetForProfile(profile);
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

}  // namespace ash
