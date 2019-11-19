// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/ash/multi_user/multi_profile_support.h"

#include <set>
#include <vector>

#include "ash/public/cpp/multi_user_window_manager.h"
#include "ash/public/cpp/multi_user_window_manager_observer.h"
#include "base/metrics/histogram_macros.h"
#include "base/strings/string_util.h"
#include "chrome/browser/browser_process.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/profiles/profile_manager.h"
#include "chrome/browser/ui/ash/launcher/chrome_launcher_controller.h"
#include "chrome/browser/ui/ash/multi_user/multi_user_util.h"
#include "chrome/browser/ui/ash/session_controller_client_impl.h"
#include "chrome/browser/ui/ash/session_util.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/browser/ui/browser_finder.h"
#include "chrome/browser/ui/browser_list.h"
#include "chrome/browser/ui/browser_window.h"
#include "extensions/browser/app_window/app_window.h"
#include "extensions/browser/app_window/app_window_registry.h"
#include "ui/aura/client/aura_constants.h"
#include "ui/aura/window.h"
#include "ui/base/ui_base_types.h"

namespace {

// Used for UMA metrics. Do not reorder.
enum TeleportWindowType {
  TELEPORT_WINDOW_BROWSER = 0,
  TELEPORT_WINDOW_INCOGNITO_BROWSER,
  TELEPORT_WINDOW_V1_APP,
  TELEPORT_WINDOW_V2_APP,
  DEPRECATED_TELEPORT_WINDOW_PANEL,
  TELEPORT_WINDOW_POPUP,
  TELEPORT_WINDOW_UNKNOWN,
  NUM_TELEPORT_WINDOW_TYPES
};

// Records the type of window which was transferred to another desktop.
void RecordUMAForTransferredWindowType(aura::Window* window) {
  // We need to figure out what kind of window this is to record the transfer.
  Browser* browser = chrome::FindBrowserWithWindow(window);
  TeleportWindowType window_type = TELEPORT_WINDOW_UNKNOWN;
  if (browser) {
    if (browser->profile()->IsOffTheRecord()) {
      window_type = TELEPORT_WINDOW_INCOGNITO_BROWSER;
    } else if (browser->deprecated_is_app()) {
      window_type = TELEPORT_WINDOW_V1_APP;
    } else if (browser->is_type_popup()) {
      window_type = TELEPORT_WINDOW_POPUP;
    } else {
      window_type = TELEPORT_WINDOW_BROWSER;
    }
  } else {
    // Unit tests might come here without a profile manager.
    if (!g_browser_process->profile_manager())
      return;
    // If it is not a browser, it is probably be a V2 application. In that case
    // one of the AppWindowRegistry instances should know about it.
    extensions::AppWindow* app_window = NULL;
    std::vector<Profile*> profiles =
        g_browser_process->profile_manager()->GetLoadedProfiles();
    for (std::vector<Profile*>::iterator it = profiles.begin();
         it != profiles.end() && app_window == NULL; it++) {
      app_window =
          extensions::AppWindowRegistry::Get(*it)->GetAppWindowForNativeWindow(
              window);
    }
    if (app_window)
      window_type = TELEPORT_WINDOW_V2_APP;
  }
  UMA_HISTOGRAM_ENUMERATION("MultiProfile.TeleportWindowType", window_type,
                            NUM_TELEPORT_WINDOW_TYPES);
}

}  // namespace

// This class keeps track of all applications which were started for a user.
// When an app gets created, the window will be tagged for that user. Note
// that the destruction does not need to be tracked here since the universal
// window observer will take care of that.
class AppObserver : public extensions::AppWindowRegistry::Observer {
 public:
  explicit AppObserver(const std::string& user_id) : user_id_(user_id) {}
  ~AppObserver() override {}

  // AppWindowRegistry::Observer overrides:
  void OnAppWindowAdded(extensions::AppWindow* app_window) override {
    aura::Window* window = app_window->GetNativeWindow();
    DCHECK(window);
    MultiUserWindowManagerHelper::GetWindowManager()->SetWindowOwner(
        window, AccountId::FromUserEmail(user_id_));
  }

 private:
  std::string user_id_;

  DISALLOW_COPY_AND_ASSIGN(AppObserver);
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
  ProfileManager* profile_manager = g_browser_process->profile_manager();
  // might be nullptr in unit tests.
  if (!profile_manager)
    return;

  std::vector<Profile*> profiles = profile_manager->GetLoadedProfiles();
  for (auto it = profiles.begin(); it != profiles.end(); ++it) {
    const AccountId account_id = multi_user_util::GetAccountIdFromProfile(*it);
    AccountIdToAppWindowObserver::iterator app_observer_iterator =
        account_id_to_app_observer_.find(account_id);
    if (app_observer_iterator != account_id_to_app_observer_.end()) {
      extensions::AppWindowRegistry::Get(*it)->RemoveObserver(
          app_observer_iterator->second);
      delete app_observer_iterator->second;
      account_id_to_app_observer_.erase(app_observer_iterator);
    }
  }
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
  if (profile)
    AddUser(profile);
}

void MultiProfileSupport::AddUser(content::BrowserContext* context) {
  Profile* profile = Profile::FromBrowserContext(context);
  const AccountId& account_id(
      multi_user_util::GetAccountIdFromProfile(profile));
  if (account_id_to_app_observer_.find(account_id) !=
      account_id_to_app_observer_.end())
    return;

  account_id_to_app_observer_[account_id] =
      new AppObserver(account_id.GetUserEmail());
  extensions::AppWindowRegistry::Get(profile)->AddObserver(
      account_id_to_app_observer_[account_id]);

  // Account all existing application windows of this user accordingly.
  const extensions::AppWindowRegistry::AppWindowList& app_windows =
      extensions::AppWindowRegistry::Get(profile)->app_windows();
  extensions::AppWindowRegistry::AppWindowList::const_iterator it =
      app_windows.begin();
  for (; it != app_windows.end(); ++it)
    account_id_to_app_observer_[account_id]->OnAppWindowAdded(*it);

  // Account all existing browser windows of this user accordingly.
  for (auto* browser : *BrowserList::GetInstance()) {
    if (browser->profile()->IsSameProfile(profile))
      OnBrowserAdded(browser);
  }
}

void MultiProfileSupport::OnBrowserAdded(Browser* browser) {
  // A unit test (e.g. CrashRestoreComplexTest.RestoreSessionForThreeUsers) can
  // come here with no valid window.
  if (!browser->window() || !browser->window()->GetNativeWindow())
    return;
  multi_user_window_manager_->SetWindowOwner(
      browser->window()->GetNativeWindow(),
      multi_user_util::GetAccountIdFromProfile(browser->profile()));
}

void MultiProfileSupport::OnWindowOwnerEntryChanged(aura::Window* window,
                                                    const AccountId& account_id,
                                                    bool was_minimized,
                                                    bool teleported) {
  if (was_minimized)
    RecordUMAForTransferredWindowType(window);

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
  ChromeLauncherController* chrome_launcher_controller =
      ChromeLauncherController::instance();
  // Some unit tests have no ChromeLauncherController.
  if (!chrome_launcher_controller)
    return;
  chrome_launcher_controller->ActiveUserChanged(
      multi_user_window_manager_->CurrentAccountId());
}
