// Copyright 2013 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/ash/shelf/chrome_shelf_controller.h"

#include <memory>
#include <set>
#include <utility>

#include "ash/components/arc/arc_prefs.h"
#include "ash/components/arc/arc_util.h"
#include "ash/constants/app_types.h"
#include "ash/constants/ash_pref_names.h"
#include "ash/metrics/login_unlock_throughput_recorder.h"
#include "ash/public/cpp/multi_user_window_manager.h"
#include "ash/public/cpp/shelf_item.h"
#include "ash/public/cpp/shelf_model.h"
#include "ash/public/cpp/shelf_prefs.h"
#include "ash/public/cpp/shelf_types.h"
#include "ash/public/cpp/window_animation_types.h"
#include "ash/public/cpp/window_properties.h"
#include "ash/shell.h"
#include "base/containers/contains.h"
#include "base/debug/dump_without_crashing.h"
#include "base/feature_list.h"
#include "base/functional/bind.h"
#include "base/functional/callback_helpers.h"
#include "base/memory/raw_ptr.h"
#include "base/metrics/histogram_macros.h"
#include "base/ranges/algorithm.h"
#include "base/strings/pattern.h"
#include "base/strings/string_util.h"
#include "base/strings/utf_string_conversions.h"
#include "base/task/sequenced_task_runner.h"
#include "base/task/single_thread_task_runner.h"
#include "base/task/thread_pool.h"
#include "chrome/browser/apps/app_service/app_service_proxy.h"
#include "chrome/browser/apps/app_service/app_service_proxy_factory.h"
#include "chrome/browser/apps/app_service/extension_apps_utils.h"
#include "chrome/browser/apps/icon_standardizer.h"
#include "chrome/browser/ash/app_list/app_list_client_impl.h"
#include "chrome/browser/ash/app_list/app_list_controller_delegate.h"
#include "chrome/browser/ash/app_list/app_list_syncable_service_factory.h"
#include "chrome/browser/ash/app_list/app_service/app_service_app_icon_loader.h"
#include "chrome/browser/ash/app_list/arc/arc_app_utils.h"
#include "chrome/browser/ash/app_list/md_icon_normalizer.h"
#include "chrome/browser/ash/arc/arc_util.h"
#include "chrome/browser/ash/crosapi/browser_util.h"
#include "chrome/browser/extensions/chrome_app_icon_loader.h"
#include "chrome/browser/extensions/extension_util.h"
#include "chrome/browser/prefs/pref_service_syncable_util.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/profiles/profile_manager.h"
#include "chrome/browser/ui/apps/app_info_dialog.h"
#include "chrome/browser/ui/ash/app_icon_color_cache.h"
#include "chrome/browser/ui/ash/keyboard/chrome_keyboard_controller_client.h"
#include "chrome/browser/ui/ash/multi_user/multi_user_util.h"
#include "chrome/browser/ui/ash/multi_user/multi_user_window_manager_helper.h"
#include "chrome/browser/ui/ash/session_controller_client_impl.h"
#include "chrome/browser/ui/ash/shelf/app_service/app_service_app_window_arc_tracker.h"
#include "chrome/browser/ui/ash/shelf/app_service/app_service_app_window_shelf_controller.h"
#include "chrome/browser/ui/ash/shelf/app_service/shelf_app_service_app_updater.h"
#include "chrome/browser/ui/ash/shelf/app_shortcut_shelf_item_controller.h"
#include "chrome/browser/ui/ash/shelf/app_window_shelf_controller.h"
#include "chrome/browser/ui/ash/shelf/app_window_shelf_item_controller.h"
#include "chrome/browser/ui/ash/shelf/browser_app_shelf_controller.h"
#include "chrome/browser/ui/ash/shelf/browser_app_shelf_item_controller.h"
#include "chrome/browser/ui/ash/shelf/browser_shortcut_shelf_item_controller.h"
#include "chrome/browser/ui/ash/shelf/browser_status_monitor.h"
#include "chrome/browser/ui/ash/shelf/chrome_shelf_controller_util.h"
#include "chrome/browser/ui/ash/shelf/chrome_shelf_item_factory.h"
#include "chrome/browser/ui/ash/shelf/chrome_shelf_prefs.h"
#include "chrome/browser/ui/ash/shelf/shelf_controller_helper.h"
#include "chrome/browser/ui/ash/shelf/shelf_extension_app_updater.h"
#include "chrome/browser/ui/ash/shelf/shelf_spinner_controller.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/browser/ui/browser_finder.h"
#include "chrome/browser/ui/browser_list.h"
#include "chrome/browser/ui/browser_window.h"
#include "chrome/browser/ui/chrome_pages.h"
#include "chrome/browser/ui/settings_window_manager_chromeos.h"
#include "chrome/browser/ui/tabs/tab_enums.h"
#include "chrome/browser/ui/tabs/tab_strip_model.h"
#include "chrome/browser/ui/webui/settings/ash/app_management/app_management_uma.h"
#include "chrome/browser/web_applications/web_app_helpers.h"
#include "chrome/browser/web_applications/web_app_utils.h"
#include "chrome/common/chrome_features.h"
#include "chrome/common/extensions/manifest_handlers/app_launch_info.h"
#include "chrome/common/pref_names.h"
#include "chrome/grit/chrome_unscaled_resources.h"
#include "chrome/grit/chromium_strings.h"
#include "chrome/grit/generated_resources.h"
#include "chrome/grit/theme_resources.h"
#include "chromeos/strings/grit/chromeos_strings.h"
#include "components/account_id/account_id.h"
#include "components/app_constants/constants.h"
#include "components/favicon/content/content_favicon_driver.h"
#include "components/services/app_service/public/cpp/app_types.h"
#include "components/strings/grit/components_strings.h"
#include "components/sync_preferences/pref_service_syncable.h"
#include "components/user_manager/user_manager.h"
#include "content/public/browser/web_contents.h"
#include "extensions/browser/extension_registry.h"
#include "extensions/browser/extension_system.h"
#include "extensions/browser/management_policy.h"
#include "extensions/common/extension.h"
#include "ui/aura/client/aura_constants.h"
#include "ui/aura/window.h"
#include "ui/base/l10n/l10n_util.h"
#include "ui/base/resource/resource_bundle.h"
#include "ui/base/ui_base_features.h"
#include "ui/display/types/display_constants.h"
#include "ui/resources/grit/ui_resources.h"

using app_constants::kChromeAppId;

namespace {

// Returns true if the given |item| has a pinned shelf item type.
bool ItemTypeIsPinned(const ash::ShelfItem& item) {
  return ash::IsPinnedShelfItemType(item.type);
}

// Invoked on a worker thread to create standard icon image.
gfx::ImageSkia CreateStandardImageOnWorkerThread(const gfx::ImageSkia& image) {
  gfx::ImageSkia standard_image = apps::CreateStandardIconImage(image);
  if (!standard_image.isNull())
    standard_image.MakeThreadSafe();
  return standard_image;
}

// Report shelf buttons initialized to LoginUnlockThroughputRecorder.
void ReportInitShelfIconList(const ash::ShelfModel* model) {
  // Shell is not always initializaed in tests.
  if (!ash::Shell::HasInstance())
    return;

  ash::Shell::Get()->login_unlock_throughput_recorder()->InitShelfIconList(
      model);
}

// Report shelf buttons updated to LoginUnlockThroughputRecorder.
void ReportUpdateShelfIconList(const ash::ShelfModel* model) {
  // Shell is not always initializaed in tests.
  if (!ash::Shell::HasInstance())
    return;

  ash::Shell::Get()->login_unlock_throughput_recorder()->UpdateShelfIconList(
      model);
}

}  // namespace

// A class to get events from ChromeOS when a user gets changed or added.
class ChromeShelfControllerUserSwitchObserver
    : public user_manager::UserManager::UserSessionStateObserver {
 public:
  explicit ChromeShelfControllerUserSwitchObserver(
      ChromeShelfController* controller)
      : controller_(controller) {
    DCHECK(user_manager::UserManager::IsInitialized());
    user_manager::UserManager::Get()->AddSessionStateObserver(this);
  }

  ChromeShelfControllerUserSwitchObserver(
      const ChromeShelfControllerUserSwitchObserver&) = delete;
  ChromeShelfControllerUserSwitchObserver& operator=(
      const ChromeShelfControllerUserSwitchObserver&) = delete;

  ~ChromeShelfControllerUserSwitchObserver() override {
    user_manager::UserManager::Get()->RemoveSessionStateObserver(this);
  }

  // user_manager::UserManager::UserSessionStateObserver overrides:
  void UserAddedToSession(const user_manager::User* added_user) override;

  // ChromeShelfControllerUserSwitchObserver:
  void OnUserProfileReadyToSwitch(Profile* profile);

 private:
  // Add a user to the session.
  void AddUser(Profile* profile);

  // The owning ChromeShelfController.
  raw_ptr<ChromeShelfController, ExperimentalAsh> controller_;

  // Users which were just added to the system, but which profiles were not yet
  // (fully) loaded.
  std::set<std::string> added_user_ids_waiting_for_profiles_;
};

void ChromeShelfControllerUserSwitchObserver::UserAddedToSession(
    const user_manager::User* active_user) {
  Profile* profile =
      multi_user_util::GetProfileFromAccountId(active_user->GetAccountId());
  // If we do not have a profile yet, we postpone forwarding the notification
  // until it is loaded.
  if (!profile) {
    added_user_ids_waiting_for_profiles_.insert(
        active_user->GetAccountId().GetUserEmail());
  } else {
    AddUser(profile);
  }
}

void ChromeShelfControllerUserSwitchObserver::OnUserProfileReadyToSwitch(
    Profile* profile) {
  if (!added_user_ids_waiting_for_profiles_.empty()) {
    // Check if the profile is from a user which was on the waiting list.
    // TODO(alemate): added_user_ids_waiting_for_profiles_ should be
    // a set<AccountId>
    std::string user_id =
        multi_user_util::GetAccountIdFromProfile(profile).GetUserEmail();
    std::set<std::string>::iterator it =
        base::ranges::find(added_user_ids_waiting_for_profiles_, user_id);
    if (it != added_user_ids_waiting_for_profiles_.end()) {
      added_user_ids_waiting_for_profiles_.erase(it);
      AddUser(profile->GetOriginalProfile());
    }
  }
}

void ChromeShelfControllerUserSwitchObserver::AddUser(Profile* profile) {
  MultiUserWindowManagerHelper::GetInstance()->AddUser(profile);
  controller_->AdditionalUserAddedToSession(profile->GetOriginalProfile());
}

// static
ChromeShelfController* ChromeShelfController::instance_ = nullptr;

ChromeShelfController::ChromeShelfController(Profile* profile,
                                             ash::ShelfModel* model,
                                             ChromeShelfItemFactory* creator)
    : model_(model),
      shelf_item_factory_(creator),
      shelf_prefs_(std::make_unique<ChromeShelfPrefs>(profile)) {
  DCHECK(!instance_);
  instance_ = this;

  CHECK(model_);

  if (!profile) {
    // If no profile was passed, we take the currently active profile and use it
    // as the owner of the current desktop.
    // Use the original profile as on chromeos we may get a temporary off the
    // record profile, unless in guest session (where off the record profile is
    // the right one).
    profile = ProfileManager::GetActiveUserProfile();
    if (!profile->IsGuestSession() && !profile->IsSystemProfile())
      profile = profile->GetOriginalProfile();
  }

  if (chrome::SettingsWindowManager::UseDeprecatedSettingsWindow(profile)) {
    settings_window_observer_ = std::make_unique<SettingsWindowObserver>();
  }

  // All profile relevant settings get bound to the current profile.
  AttachProfile(profile);
  DCHECK_EQ(profile, profile_);
  model_->AddObserver(this);

  shelf_spinner_controller_ = std::make_unique<ShelfSpinnerController>(this);

  // Create either the real window manager or a stub.
  MultiUserWindowManagerHelper::CreateInstance();

  // On Chrome OS using multi profile we want to switch the content of the shelf
  // with a user change. Note that for unit tests the instance can be nullptr.
  if (SessionControllerClientImpl::IsMultiProfileAvailable()) {
    user_switch_observer_ =
        std::make_unique<ChromeShelfControllerUserSwitchObserver>(this);
  }

  std::unique_ptr<AppServiceAppWindowShelfController> app_service_controller =
      std::make_unique<AppServiceAppWindowShelfController>(this);
  app_service_app_window_controller_ = app_service_controller.get();
  app_window_controllers_.emplace_back(std::move(app_service_controller));
  if (web_app::IsWebAppsCrosapiEnabled() &&
      apps::AppServiceProxyFactory::IsAppServiceAvailableForProfile(profile)) {
    apps::AppServiceProxy* proxy =
        apps::AppServiceProxyFactory::GetForProfile(profile);
    DCHECK(proxy);
    CHECK(proxy->BrowserAppInstanceRegistry());
    browser_app_shelf_controller_ = std::make_unique<BrowserAppShelfController>(
        profile, *proxy->BrowserAppInstanceRegistry(), *model_,
        *shelf_item_factory_, *shelf_spinner_controller_);
  } else {
    // Create the browser monitor which will inform the shelf of status changes.
    browser_status_monitor_ = std::make_unique<BrowserStatusMonitor>(this);
  }
}

ChromeShelfController::~ChromeShelfController() {
  // Reset the BrowserStatusMonitor as it has a weak pointer to this.
  browser_status_monitor_.reset();

  // Reset the app window controllers here since it has a weak pointer to this.
  app_service_app_window_controller_ = nullptr;
  app_window_controllers_.clear();

  // Destroy the ShelfSpinnerController before clearing delegates.
  shelf_spinner_controller_.reset();

  // Destroy local shelf item delegates; some subclasses have complex cleanup.
  model_->DestroyItemDelegates();

  model_->RemoveObserver(this);

  // Release all profile dependent resources.
  ReleaseProfile();

  // Get rid of the multi user window manager instance.
  MultiUserWindowManagerHelper::DeleteInstance();

  if (instance_ == this)
    instance_ = nullptr;
}

void ChromeShelfController::Init() {
  if (!crosapi::browser_util::IsLacrosPrimaryBrowser()) {
    CreateBrowserShortcutItem(/*pinned=*/true);
    UpdateBrowserItemState();
  }

  // Tag all open browser windows with the appropriate shelf id property. This
  // associates each window with the shelf item for the active web contents.
  for (auto* browser : *BrowserList::GetInstance()) {
    if (IsBrowserRepresentedInBrowserList(browser, model_) &&
        browser->tab_strip_model()->GetActiveWebContents()) {
      SetShelfIDForBrowserWindowContents(
          browser, browser->tab_strip_model()->GetActiveWebContents());
    }
  }

  UpdatePinnedAppsFromSync();
  if (browser_status_monitor_) {
    browser_status_monitor_->Initialize();
  }
  ReportInitShelfIconList(model_);
}

ash::ShelfID ChromeShelfController::CreateAppItem(
    std::unique_ptr<ash::ShelfItemDelegate> item_delegate,
    ash::ShelfItemStatus status,
    const std::u16string& title) {
  return InsertAppItem(std::move(item_delegate), status, model_->item_count(),
                       ash::TYPE_APP, title);
}

const ash::ShelfItem* ChromeShelfController::GetItem(
    const ash::ShelfID& id) const {
  return model_->ItemByID(id);
}

void ChromeShelfController::SetItemType(const ash::ShelfID& id,
                                        ash::ShelfItemType type) {
  const ash::ShelfItem* item = GetItem(id);
  if (item && item->type != type) {
    ash::ShelfItem new_item = *item;
    new_item.type = type;
    model_->Set(model_->ItemIndexByID(id), new_item);
  }
}

void ChromeShelfController::SetItemStatus(const ash::ShelfID& id,
                                          ash::ShelfItemStatus status) {
  const ash::ShelfItem* item = GetItem(id);
  if (item && item->status != status) {
    ash::ShelfItem new_item = *item;
    new_item.status = status;
    model_->Set(model_->ItemIndexByID(id), new_item);
  }
}

void ChromeShelfController::SetItemTitle(const ash::ShelfID& id,
                                         const std::u16string& title) {
  const ash::ShelfItem* item = GetItem(id);
  if (item && item->title != title) {
    ash::ShelfItem new_item = *item;
    new_item.title = title;
    model_->Set(model_->ItemIndexByID(id), new_item);
  }
}

void ChromeShelfController::ReplaceWithAppShortcutOrRemove(
    const ash::ShelfID& id) {
  CHECK(!id.IsNull());
  if (IsPinned(id)) {
    // Create a new shortcut delegate.
    SetItemStatus(id, ash::STATUS_CLOSED);
    model_->ReplaceShelfItemDelegate(
        id, std::make_unique<AppShortcutShelfItemController>(id));
  } else {
    RemoveShelfItem(id);
  }
}

void ChromeShelfController::UnpinShelfItemInternal(const ash::ShelfID& id) {
  const ash::ShelfItem* item = GetItem(id);
  if (item && item->status != ash::STATUS_CLOSED)
    UnpinRunningAppInternal(model_->ItemIndexByID(id));
  else
    RemoveShelfItem(id);
}

void ChromeShelfController::SetItemStatusOrRemove(const ash::ShelfID& id,
                                                  ash::ShelfItemStatus status) {
  if (!IsPinned(id) && status == ash::STATUS_CLOSED)
    RemoveShelfItem(id);
  else
    SetItemStatus(id, status);
}

bool ChromeShelfController::ShouldSyncItemWithReentrancy(
    const ash::ShelfItem& item) {
  return should_sync_pin_changes_ && ShouldSyncItem(item);
}

bool ChromeShelfController::ShouldSyncItem(const ash::ShelfItem& item) {
  return ItemTypeIsPinned(item);
}

bool ChromeShelfController::IsPinned(const ash::ShelfID& id) const {
  const ash::ShelfItem* item = GetItem(id);
  return item && ItemTypeIsPinned(*item);
}

void ChromeShelfController::SetAppStatus(const std::string& app_id,
                                         ash::ShelfItemStatus status) {
  ash::ShelfID id(app_id);
  const ash::ShelfItem* item = GetItem(id);
  if (item) {
    SetItemStatusOrRemove(id, status);
  } else if (status != ash::STATUS_CLOSED && !app_id.empty()) {
    InsertAppItem(
        std::make_unique<AppShortcutShelfItemController>(ash::ShelfID(app_id)),
        status, model_->item_count(), ash::TYPE_APP);
  }
}

void ChromeShelfController::Close(const ash::ShelfID& id) {
  ash::ShelfItemDelegate* delegate = model_->GetShelfItemDelegate(id);
  if (!delegate)
    return;  // May happen if menu closed.
  delegate->Close();
}

bool ChromeShelfController::IsOpen(const ash::ShelfID& id) const {
  const ash::ShelfItem* item = GetItem(id);
  return item && item->status != ash::STATUS_CLOSED;
}

bool ChromeShelfController::IsPlatformApp(const ash::ShelfID& id) {
  const extensions::Extension* extension =
      GetExtensionForAppID(id.app_id, profile());
  // An extension can be synced / updated at any time and therefore not be
  // available.
  return extension ? extension->is_platform_app() : false;
}

bool ChromeShelfController::UninstallAllowed(const std::string& app_id) {
  const extensions::Extension* extension =
      extensions::ExtensionRegistry::Get(profile())->GetInstalledExtension(
          app_id);
  const extensions::ManagementPolicy* policy =
      extensions::ExtensionSystem::Get(profile())->management_policy();
  return extension && policy->UserMayModifySettings(extension, nullptr) &&
         !policy->MustRemainInstalled(extension, nullptr);
}

void ChromeShelfController::LaunchApp(const ash::ShelfID& id,
                                      ash::ShelfLaunchSource source,
                                      int event_flags,
                                      int64_t display_id) {
  shelf_controller_helper_->LaunchApp(id, source, event_flags, display_id);
}

void ChromeShelfController::SetItemImage(const ash::ShelfID& shelf_id,
                                         const gfx::ImageSkia& image) {
  DCHECK(!image.isNull());
  if (const auto* item = GetItem(shelf_id)) {
    ash::ShelfItem new_item = *item;
    new_item.image = image;
    new_item.notification_badge_color =
        ash::AppIconColorCache::GetInstance().GetLightVibrantColorForApp(
            new_item.id.app_id, image);
    model_->Set(model_->ItemIndexByID(shelf_id), new_item);
  }

  ReportUpdateShelfIconList(model_);
}

void ChromeShelfController::UpdateItemImage(const std::string& app_id) {
  if (auto* icon_loader = GetAppIconLoaderForApp(app_id))
    icon_loader->UpdateImage(app_id);
}

void ChromeShelfController::UpdateAppState(content::WebContents* contents,
                                           bool remove) {
  ash::ShelfID shelf_id(shelf_controller_helper_->GetAppID(contents));

  // If the tab changed apps, remove its association with the previous app item.
  if (web_contents_to_app_id_.find(contents) != web_contents_to_app_id_.end()) {
    ash::ShelfID old_id(web_contents_to_app_id_[contents]);
    if (old_id != shelf_id && GetItem(old_id)) {
      // Since GetAppState() will use |web_contents_to_app_id_| we remove
      // the connection before calling it.
      web_contents_to_app_id_.erase(contents);
      SetItemStatusOrRemove(old_id, GetAppState(old_id.app_id));
    }
  }

  if (remove)
    web_contents_to_app_id_.erase(contents);
  else
    web_contents_to_app_id_[contents] = shelf_id.app_id;

  SetItemStatusOrRemove(shelf_id, GetAppState(shelf_id.app_id));
}

void ChromeShelfController::UpdateV1AppState(const std::string& app_id) {
  for (Browser* browser : *BrowserList::GetInstance()) {
    if (!browser->is_type_normal() ||
        !multi_user_util::IsProfileFromActiveUser(browser->profile())) {
      continue;
    }
    for (int i = 0; i < browser->tab_strip_model()->count(); ++i) {
      content::WebContents* const web_contents =
          browser->tab_strip_model()->GetWebContentsAt(i);
      if (shelf_controller_helper_->GetAppID(web_contents) != app_id)
        continue;
      UpdateAppState(web_contents, false /*remove*/);
      if (browser->tab_strip_model()->GetActiveWebContents() == web_contents)
        SetShelfIDForBrowserWindowContents(browser, web_contents);
    }
  }
}

std::string ChromeShelfController::GetAppIDForWebContents(
    content::WebContents* contents) {
  std::string app_id = shelf_controller_helper_->GetAppID(contents);
  if (!app_id.empty())
    return app_id;

  return kChromeAppId;
}

ash::ShelfID ChromeShelfController::GetShelfIDForAppId(
    const std::string& app_id) const {
  // If there is no dedicated app item, use the browser shortcut item.
  const ash::ShelfItem* item =
      !app_id.empty() ? GetItem(ash::ShelfID(app_id)) : nullptr;
  return item ? item->id : ash::ShelfID(kChromeAppId);
}

ash::ShelfAction ChromeShelfController::ActivateWindowOrMinimizeIfActive(
    ui::BaseWindow* window,
    bool allow_minimize) {
  // We might have to teleport a window back to the current user.
  aura::Window* native_window = window->GetNativeWindow();
  const AccountId& current_account_id =
      multi_user_util::GetAccountIdFromProfile(profile());
  if (!MultiUserWindowManagerHelper::GetInstance()->IsWindowOnDesktopOfUser(
          native_window, current_account_id)) {
    MultiUserWindowManagerHelper::GetWindowManager()->ShowWindowForUser(
        native_window, current_account_id);
    window->Activate();
    return ash::SHELF_ACTION_WINDOW_ACTIVATED;
  }

  AppListClientImpl* app_list_client = AppListClientImpl::GetInstance();
  if (window->IsActive() && allow_minimize &&
      !(app_list_client && app_list_client->app_list_target_visibility())) {
    window->Minimize();
    return ash::SHELF_ACTION_WINDOW_MINIMIZED;
  }

  window->Show();
  window->Activate();
  return ash::SHELF_ACTION_WINDOW_ACTIVATED;
}

void ChromeShelfController::ActiveUserChanged(const AccountId& account_id) {
  // Store the order of running applications for the user which gets inactive.
  RememberUnpinnedRunningApplicationOrder();
  // Coming here the default profile is already switched. All profile specific
  // resources get released and the new profile gets attached instead.
  ReleaseProfile();
  // When coming here, the active user has already be changed so that we can
  // set it as active.
  AttachProfile(ProfileManager::GetActiveUserProfile());
  if (browser_status_monitor_) {
    // Update the V1 applications.
    browser_status_monitor_->ActiveUserChanged(account_id.GetUserEmail());
  }
  // Save/restore spinners belonging to the old/new user. Must be called before
  // notifying the AppWindowControllers, as some of them assume spinners owned
  // by the new user have already been added to the shelf.
  shelf_spinner_controller_->ActiveUserChanged(account_id);
  // Switch the running applications to the new user.
  for (auto& controller : app_window_controllers_)
    controller->ActiveUserChanged(account_id.GetUserEmail());
  // Update the user specific shell properties from the new user profile.
  // Shelf preferences are loaded in ChromeShelfController::AttachProfile.
  UpdatePinnedAppsFromSync();

  // Restore the order of running, but unpinned applications for the activated
  // user.
  RestoreUnpinnedRunningApplicationOrder(account_id.GetUserEmail());
}

void ChromeShelfController::AdditionalUserAddedToSession(Profile* profile) {
  AddAppUpdaterAndIconLoader(profile);

  // Switch the running applications to the new user.
  for (auto& controller : app_window_controllers_)
    controller->AdditionalUserAddedToSession(profile);
}

ash::ShelfItemDelegate::AppMenuItems
ChromeShelfController::GetAppMenuItemsForTesting(const ash::ShelfItem& item) {
  ash::ShelfItemDelegate* delegate = model_->GetShelfItemDelegate(item.id);
  return delegate ? delegate->GetAppMenuItems(ui::EF_NONE, base::NullCallback())
                  : ash::ShelfItemDelegate::AppMenuItems();
}

std::vector<aura::Window*> ChromeShelfController::GetArcWindows() {
  if (app_service_app_window_controller_)
    return app_service_app_window_controller_->GetArcWindows();
  return std::vector<aura::Window*>();
}

void ChromeShelfController::ActivateShellApp(const std::string& app_id,
                                             int window_index) {
  const ash::ShelfItem* item = GetItem(ash::ShelfID(app_id));
  if (item &&
      (item->type == ash::TYPE_APP || item->type == ash::TYPE_PINNED_APP)) {
    ash::ShelfItemDelegate* delegate = model_->GetShelfItemDelegate(item->id);
    AppWindowShelfItemController* item_controller =
        delegate->AsAppWindowShelfItemController();
    item_controller->ActivateIndexedApp(window_index);
  }
}

bool ChromeShelfController::IsWebContentHandledByApplication(
    content::WebContents* web_contents,
    const std::string& app_id) {
  return (web_contents_to_app_id_.find(web_contents) !=
          web_contents_to_app_id_.end()) &&
         (web_contents_to_app_id_[web_contents] == app_id);
}

gfx::Image ChromeShelfController::GetAppMenuIcon(
    content::WebContents* web_contents) const {
  ui::ResourceBundle& rb = ui::ResourceBundle::GetSharedInstance();
  if (!web_contents)
    return rb.GetImageNamed(IDR_DEFAULT_FAVICON);
  const Profile* profile =
      Profile::FromBrowserContext(web_contents->GetBrowserContext());
  if (profile->IsIncognitoProfile())
    return rb.GetImageNamed(IDR_ASH_SHELF_LIST_INCOGNITO_BROWSER);
  favicon::FaviconDriver* favicon_driver =
      favicon::ContentFaviconDriver::FromWebContents(web_contents);
  gfx::Image result = favicon_driver->GetFavicon();
  if (result.IsEmpty())
    return rb.GetImageNamed(IDR_DEFAULT_FAVICON);
  return result;
}

std::u16string ChromeShelfController::GetAppMenuTitle(
    content::WebContents* web_contents) const {
  if (!web_contents)
    return l10n_util::GetStringUTF16(IDS_NEW_TAB_TITLE);
  const std::u16string& title = web_contents->GetTitle();
  if (!title.empty())
    return title;
  WebContentsToAppIDMap::const_iterator iter =
      web_contents_to_app_id_.find(web_contents);
  if (iter != web_contents_to_app_id_.end()) {
    std::string app_id = iter->second;
    const extensions::Extension* extension =
        GetExtensionForAppID(app_id, profile());
    if (extension)
      return base::UTF8ToUTF16(extension->name());
  }
  return l10n_util::GetStringUTF16(IDS_NEW_TAB_TITLE);
}

BrowserShortcutShelfItemController*
ChromeShelfController::GetBrowserShortcutShelfItemControllerForTesting() {
  ash::ShelfID id(kChromeAppId);
  ash::ShelfItemDelegate* delegate = model_->GetShelfItemDelegate(id);
  DCHECK(delegate) << "There should be always be a browser shortcut item.";
  return static_cast<BrowserShortcutShelfItemController*>(delegate);
}

void ChromeShelfController::UpdateBrowserItemState() {
  ash::ShelfItemStatus browser_status = ash::STATUS_CLOSED;
  for (auto* browser : *BrowserList::GetInstance()) {
    if (IsBrowserRepresentedInBrowserList(browser, model_)) {
      browser_status = ash::STATUS_RUNNING;
      break;
    }
  }

  if (browser_status == ash::STATUS_CLOSED) {
    // If browser shortcut icon is not pinned, remove it.
    // Practically, this happens when Lacros is the primary browser.
    int item_index =
        model_->GetItemIndexForType(ash::TYPE_UNPINNED_BROWSER_SHORTCUT);
    if (item_index >= 0) {
      model_->RemoveItemAt(item_index);
      ReportUpdateShelfIconList(model_);
      return;
    }
  }

  const ash::ShelfID chrome_id(kChromeAppId);
  if (browser_status == ash::STATUS_RUNNING &&
      model_->ItemIndexByID(chrome_id) < 0) {
    // If browser short cut is not present, create it.
    // This happens iff browser shortcut is not pinned.
    CreateBrowserShortcutItem(/*pinned=*/false);
  }

  int browser_index = model_->ItemIndexByID(chrome_id);
  if (browser_index < 0) {
    DCHECK_EQ(browser_status, ash::STATUS_CLOSED);
    return;
  }
  ash::ShelfItem browser_item = model_->items()[browser_index];
  if (browser_status == browser_item.status) {
    // Nothing is changed.
    return;
  }

  browser_item.status = browser_status;
  model_->Set(browser_index, browser_item);

  ReportUpdateShelfIconList(model_);
}

void ChromeShelfController::SetShelfIDForBrowserWindowContents(
    Browser* browser,
    content::WebContents* web_contents) {
  // We need to set the window ShelfID for V1 applications since they are
  // content which might change and as such change the application type.
  // The browser window may not exist in unit tests.
  if (!browser || !browser->window() || !browser->window()->GetNativeWindow() ||
      !multi_user_util::IsProfileFromActiveUser(browser->profile())) {
    return;
  }

  const std::string app_id = GetAppIDForWebContents(web_contents);
  browser->window()->GetNativeWindow()->SetProperty(ash::kAppIDKey,
                                                    new std::string(app_id));

  const ash::ShelfID shelf_id = GetShelfIDForAppId(app_id);
  browser->window()->GetNativeWindow()->SetProperty(
      ash::kShelfIDKey, new std::string(shelf_id.Serialize()));
}

void ChromeShelfController::OnUserProfileReadyToSwitch(Profile* profile) {
  if (user_switch_observer_.get())
    user_switch_observer_->OnUserProfileReadyToSwitch(profile);
}

ShelfSpinnerController* ChromeShelfController::GetShelfSpinnerController() {
  return shelf_spinner_controller_.get();
}

ChromeShelfController::ScopedPinSyncDisabler
ChromeShelfController::GetScopedPinSyncDisabler() {
  // Only one temporary disabler should not exist at a time.
  DCHECK(should_sync_pin_changes_);
  return std::make_unique<base::AutoReset<bool>>(&should_sync_pin_changes_,
                                                 false);
}

void ChromeShelfController::SetShelfControllerHelperForTest(
    std::unique_ptr<ShelfControllerHelper> helper) {
  shelf_controller_helper_ = std::move(helper);
}

void ChromeShelfController::SetAppIconLoadersForTest(
    std::vector<std::unique_ptr<AppIconLoader>>& loaders) {
  app_icon_loaders_.clear();
  for (auto& loader : loaders)
    app_icon_loaders_[profile_].push_back(std::move(loader));
}

void ChromeShelfController::SetProfileForTest(Profile* profile) {
  profile_ = profile;
  latest_active_profile_ = profile;
}

bool ChromeShelfController::AllowedToSetAppPinState(const std::string& app_id,
                                                    bool target_pin) const {
  return model_->AllowedToSetAppPinState(app_id, target_pin);
}

bool ChromeShelfController::IsAppPinned(const std::string& app_id) {
  return model_->IsAppPinned(app_id);
}

void ChromeShelfController::UnpinAppWithID(const std::string& app_id) {
  model_->UnpinAppWithID(app_id);
}

void ChromeShelfController::ReplacePinnedItem(const std::string& old_app_id,
                                              const std::string& new_app_id) {
  if (!model_->IsAppPinned(old_app_id) || model_->IsAppPinned(new_app_id))
    return;
  const int index = model_->ItemIndexByAppID(old_app_id);

  const ash::ShelfID new_shelf_id(new_app_id);
  ash::ShelfItem item;
  item.type = ash::TYPE_PINNED_APP;
  item.id = new_shelf_id;

  // Remove old_app at index and replace with new app.
  model_->RemoveItemAt(index);
  model_->AddAt(index, item,
                std::make_unique<AppShortcutShelfItemController>(item.id));

  ReportUpdateShelfIconList(model_);
}

void ChromeShelfController::PinAppAtIndex(const std::string& app_id,
                                          int target_index) {
  if (target_index < 0 || model_->IsAppPinned(app_id))
    return;

  const ash::ShelfID new_shelf_id(app_id);
  ash::ShelfItem item;
  item.type = ash::TYPE_PINNED_APP;
  item.id = new_shelf_id;

  model_->AddAt(target_index, item,
                std::make_unique<AppShortcutShelfItemController>(item.id));

  ReportUpdateShelfIconList(model_);
}

int ChromeShelfController::PinnedItemIndexByAppID(const std::string& app_id) {
  if (model_->IsAppPinned(app_id)) {
    ash::ShelfID shelf_id(app_id);
    return model_->ItemIndexByID(shelf_id);
  }
  return kInvalidIndex;
}

AppIconLoader* ChromeShelfController::GetAppIconLoaderForApp(
    const std::string& app_id) {
  for (const auto& app_icon_loader :
       app_icon_loaders_[latest_active_profile_]) {
    if (app_icon_loader->CanLoadImageForApp(app_id))
      return app_icon_loader.get();
  }

  return nullptr;
}

bool ChromeShelfController::CanDoShowAppInfoFlow(
    Profile* profile,
    const std::string& extension_id) {
  return CanShowAppInfoDialog(profile, extension_id);
}

void ChromeShelfController::DoShowAppInfoFlow(Profile* profile,
                                              const std::string& app_id) {
  apps::AppServiceProxy* proxy =
      apps::AppServiceProxyFactory::GetForProfile(profile);

  auto app_type = proxy->AppRegistryCache().GetAppType(app_id);

  // Apps that are not in the App Service may call this function.
  // E.g. extensions, apps that are using their platform specific IDs.
  if (app_type == apps::AppType::kUnknown) {
    return;
  }

  if (app_type == apps::AppType::kWeb ||
      app_type == apps::AppType::kSystemWeb) {
    chrome::ShowAppManagementPage(
        profile, app_id,
        ash::settings::AppManagementEntryPoint::kShelfContextMenuAppInfoWebApp);
  } else {
    chrome::ShowAppManagementPage(profile,
                                  apps::GetEscapedAppId(app_id, app_type),
                                  ash::settings::AppManagementEntryPoint::
                                      kShelfContextMenuAppInfoChromeApp);
  }
}

///////////////////////////////////////////////////////////////////////////////
// ShelfAppUpdater::Delegate:

void ChromeShelfController::OnAppInstalled(
    content::BrowserContext* browser_context,
    const std::string& app_id) {
  if (IsAppPinned(app_id) &&
      ShelfControllerHelper::IsAppHiddenFromShelf(profile(), app_id)) {
    ScopedPinSyncDisabler scoped_pin_sync_disabler = GetScopedPinSyncDisabler();
    UnpinShelfItemInternal(ash::ShelfID(app_id));
  }

  // When the app is pinned to the shelf, or added to the shelf, the app
  // probably isn't ready in AppService, so set the title, and load the icon
  // again on callback when the app is ready in AppService.
  int index = model_->ItemIndexByAppID(app_id);
  if (index != kInvalidIndex) {
    ash::ShelfItem item = model_->items()[index];
    if (item.type == ash::TYPE_APP || item.type == ash::TYPE_PINNED_APP) {
      AppIconLoader* app_icon_loader = GetAppIconLoaderForApp(app_id);
      if (app_icon_loader) {
        app_icon_loader->ClearImage(app_id);
        app_icon_loader->FetchImage(app_id);
      }

      bool needs_update = false;
      if (item.title.empty()) {
        needs_update = true;
        item.title =
            ShelfControllerHelper::GetAppTitle(latest_active_profile_, app_id);
      }

      ash::AppStatus app_status =
          ShelfControllerHelper::GetAppStatus(latest_active_profile_, app_id);
      if (app_status != item.app_status) {
        needs_update = true;
        item.app_status = app_status;
      }

      if (needs_update)
        model_->Set(index, item);
    }
  }

  UpdatePinnedAppsFromSync();
}

void ChromeShelfController::OnAppUpdated(
    content::BrowserContext* browser_context,
    const std::string& app_id,
    bool reload_icon) {
  // Ensure that icon loader tracks the icon for this app - in particular, this
  // is needed when updating ChromeShelfController after user change in
  // multi-profile sessions, as icon loaders get reset when clearing the state
  // from the previous profile.
  int index = model_->ItemIndexByAppID(app_id);
  if (index != kInvalidIndex) {
    ash::ShelfItem item = model_->items()[index];
    if (item.type == ash::TYPE_APP || item.type == ash::TYPE_PINNED_APP) {
      if (reload_icon) {
        AppIconLoader* app_icon_loader = GetAppIconLoaderForApp(app_id);
        if (app_icon_loader)
          app_icon_loader->FetchImage(app_id);
      }

      bool needs_update = false;
      ash::AppStatus app_status =
          ShelfControllerHelper::GetAppStatus(latest_active_profile_, app_id);
      if (app_status != item.app_status) {
        needs_update = true;
        item.app_status = app_status;
      }

      std::u16string title =
          ShelfControllerHelper::GetAppTitle(latest_active_profile_, app_id);
      if (item.title != title) {
        needs_update = true;
        item.title = title;
      }

      if (needs_update)
        model_->Set(index, item);
    }
  }
}

void ChromeShelfController::OnAppShowInShelfChanged(
    content::BrowserContext* browser_context,
    const std::string& app_id,
    bool show_in_shelf) {
  if (browser_context != profile())
    return;

  ScopedPinSyncDisabler scoped_pin_sync_disabler = GetScopedPinSyncDisabler();

  // If the app should be hidden from shelf, make sure it gets unpinned.
  if (!show_in_shelf) {
    if (IsAppPinned(app_id))
      UnpinShelfItemInternal(ash::ShelfID(app_id));
    return;
  }

  // If the app status changed to "shown in shelf", pin the app if user prefs
  // (or policy) indicate that the app should be pinned.
  const std::vector<ash::ShelfID> pinned_apps =
      shelf_prefs_->GetPinnedAppsFromSync(shelf_controller_helper_.get());

  // Find the app index within pinned apps.
  int index = -1;
  for (size_t i = 0; i < pinned_apps.size(); ++i) {
    if (pinned_apps[i].app_id == app_id) {
      index = i;
      break;
    }
  }

  // The app should not be pinned - nothing left to do.
  if (index == -1)
    return;

  // Update apps icon if applicable.
  OnAppUpdated(profile(), app_id, /*reload_icon=*/true);

  // Calculate the target app index within the model - find the last app in
  // `pinned_apps` that precedes `app_id`, and is in shelf model.
  int target_index_in_model = 0;
  for (int i = index - 1; i >= 0; --i) {
    int index_in_model = model_->ItemIndexByID(pinned_apps[i]);
    if (index_in_model >= 0 &&
        ItemTypeIsPinned(model_->items()[index_in_model])) {
      target_index_in_model = index_in_model + 1;
      break;
    }
  }

  EnsureAppPinnedInModelAtIndex(app_id, model_->ItemIndexByAppID(app_id),
                                target_index_in_model);

  // Set the pinned by policy flag.
  const int final_index = model_->ItemIndexByAppID(app_id);
  if (final_index >= 0)
    UpdatePinnedByPolicyForItemAtIndex(final_index);
}

void ChromeShelfController::OnAppUninstalledPrepared(
    content::BrowserContext* browser_context,
    const std::string& app_id,
    bool by_migration) {
  // Since we might have windowed apps of this type which might have
  // outstanding locks which needs to be removed.
  const Profile* profile = Profile::FromBrowserContext(browser_context);
  ash::ShelfID shelf_id(app_id);
  if (GetItem(shelf_id))
    CloseWindowedAppsFromRemovedExtension(app_id, profile);

  // Some apps may be removed locally. Unpin the item without removing the pin
  // position from profile preferences. When needed, it is automatically deleted
  // on app list model update.
  if (IsAppPinned(app_id) && profile == this->profile()) {
    bool show_in_shelf_changed = false;
    bool is_app_disabled = false;
    apps::AppServiceProxy* proxy =
        apps::AppServiceProxyFactory::GetForProfile(this->profile());
    proxy->AppRegistryCache().ForOneApp(
        app_id, [&show_in_shelf_changed,
                 &is_app_disabled](const apps::AppUpdate& update) {
          show_in_shelf_changed = update.ShowInShelfChanged();
          is_app_disabled =
              update.Readiness() == apps::Readiness::kDisabledByPolicy;
        });
    // If the app is hidden and disabled, we need to update the app pin state.
    // We don't remove the pin position from the preferences, in case we want to
    // restore the app pinned state when the app state has changed to blocked or
    // enabled.
    if (by_migration || (show_in_shelf_changed && is_app_disabled)) {
      ScopedPinSyncDisabler scoped_pin_sync_disabler =
          GetScopedPinSyncDisabler();
      UnpinShelfItemInternal(shelf_id);
    } else {
      UnpinShelfItemInternal(shelf_id);
    }
  }
}

///////////////////////////////////////////////////////////////////////////////
// AppIconLoaderDelegate:

void ChromeShelfController::OnAppImageUpdated(const std::string& app_id,
                                              const gfx::ImageSkia& image) {
  bool is_standard_icon = true;
  if (!AppServiceAppIconLoader::CanLoadImage(latest_active_profile_, app_id))
    is_standard_icon = false;

  if (is_standard_icon) {
    UpdateAppImage(app_id, image);
    return;
  }

  if (!standard_icon_task_runner_) {
    standard_icon_task_runner_ = base::ThreadPool::CreateSequencedTaskRunner(
        {base::TaskPriority::USER_VISIBLE,
         base::TaskShutdownBehavior::SKIP_ON_SHUTDOWN});
  }

  gfx::ImageSkia copy;
  if (image.IsThreadSafe()) {
    copy = image;
  } else {
    image.EnsureRepsForSupportedScales();
    copy = image.DeepCopy();
  }

  standard_icon_task_runner_->PostTaskAndReplyWithResult(
      FROM_HERE, base::BindOnce(&CreateStandardImageOnWorkerThread, copy),
      base::BindOnce(&ChromeShelfController::UpdateAppImage,
                     weak_ptr_factory_.GetWeakPtr(), app_id));
}

void ChromeShelfController::UpdateAppImage(const std::string& app_id,
                                           const gfx::ImageSkia& image) {
  // TODO: need to get this working for shortcuts.
  for (int index = 0; index < model_->item_count(); ++index) {
    ash::ShelfItem item = model_->items()[index];
    ash::ShelfItemDelegate* delegate = model_->GetShelfItemDelegate(item.id);
    if (!delegate || delegate->image_set_by_controller() ||
        item.id.app_id != app_id) {
      continue;
    }
    item.image = image;
    shelf_spinner_controller_->MaybeApplySpinningEffect(app_id, &item.image);
    item.notification_badge_color =
        ash::AppIconColorCache::GetInstance().GetLightVibrantColorForApp(app_id,
                                                                         image);
    model_->Set(index, item);
    // It's possible we're waiting on more than one item, so don't break.
  }

  ReportUpdateShelfIconList(model_);
}

///////////////////////////////////////////////////////////////////////////////
// ChromeShelfController private:

void ChromeShelfController::RememberUnpinnedRunningApplicationOrder() {
  RunningAppListIds list;
  for (int i = 0; i < model_->item_count(); i++) {
    if (model_->items()[i].type == ash::TYPE_APP)
      list.push_back(model_->items()[i].id.app_id);
  }
  const std::string user_email =
      multi_user_util::GetAccountIdFromProfile(profile()).GetUserEmail();
  last_used_running_application_order_[user_email] = list;
}

void ChromeShelfController::RestoreUnpinnedRunningApplicationOrder(
    const std::string& user_id) {
  const RunningAppListIdMap::iterator app_id_list =
      last_used_running_application_order_.find(user_id);
  if (app_id_list == last_used_running_application_order_.end())
    return;

  // Find the first insertion point for running applications.
  int running_index = model_->FirstRunningAppIndex();
  for (const std::string& app_id : app_id_list->second) {
    const ash::ShelfItem* item = GetItem(ash::ShelfID(app_id));
    if (item && item->type == ash::TYPE_APP) {
      int app_index = model_->ItemIndexByID(item->id);
      DCHECK_GE(app_index, 0);
      if (running_index != app_index)
        model_->Move(running_index, app_index);
      running_index++;
    }
  }
}

void ChromeShelfController::RemoveShelfItem(const ash::ShelfID& id) {
  const int index = model_->ItemIndexByID(id);
  if (index >= 0 && index < model_->item_count())
    model_->RemoveItemAt(index);

  ReportUpdateShelfIconList(model_);
}

void ChromeShelfController::PinRunningAppInternal(
    int index,
    const ash::ShelfID& shelf_id) {
  if (GetItem(shelf_id)->type == ash::TYPE_UNPINNED_BROWSER_SHORTCUT) {
    // If the item is unpinned browser shortcut, which should never be
    // pinned during the session, do nothing.
    return;
  }

  DCHECK_EQ(GetItem(shelf_id)->type, ash::TYPE_APP);
  SetItemType(shelf_id, ash::TYPE_PINNED_APP);
  int running_index = model_->ItemIndexByID(shelf_id);
  if (running_index < index)
    --index;
  if (running_index != index)
    model_->Move(running_index, index);
}

void ChromeShelfController::UnpinRunningAppInternal(int index) {
  DCHECK(index >= 0 && index < model_->item_count());
  const ash::ShelfItem& item = model_->items()[index];
  DCHECK_EQ(item.type, ash::TYPE_PINNED_APP);
  SetItemType(item.id, ash::TYPE_APP);
}

void ChromeShelfController::SyncPinPosition(const ash::ShelfID& shelf_id) {
  DCHECK(should_sync_pin_changes_);
  DCHECK(!shelf_id.IsNull());

  const int max_index = model_->item_count();
  const int index = model_->ItemIndexByID(shelf_id);
  DCHECK_GE(index, 0);

  ash::ShelfID shelf_id_before;
  std::vector<ash::ShelfID> shelf_ids_after;

  for (int i = index - 1; i >= 0; --i) {
    if (ShouldSyncItem(model_->items()[i])) {
      shelf_id_before = model_->items()[i].id;
      break;
    }
  }

  for (int i = index + 1; i < max_index; ++i) {
    const ash::ShelfID& shelf_id_after = model_->items()[i].id;
    if (ShouldSyncItem(model_->items()[i]))
      shelf_ids_after.push_back(shelf_id_after);
  }

  shelf_prefs_->SetPinPosition(shelf_id, shelf_id_before, shelf_ids_after);
}

void ChromeShelfController::OnSyncModelUpdated() {
  ScheduleUpdatePinnedAppsFromSync();
}

void ChromeShelfController::OnIsSyncingChanged() {
  UpdatePinnedAppsFromSync();

  InitLocalShelfPrefsIfOsPrefsAreSyncing();
}

void ChromeShelfController::InitLocalShelfPrefsIfOsPrefsAreSyncing() {
  // Wait until the initial sync happens.
  auto* pref_service = PrefServiceSyncableFromProfile(profile());
  bool is_syncing = pref_service->AreOsPrefsSyncing();
  if (!is_syncing)
    return;
  // Initialize the local prefs if this is the first time sync has occurred.
  shelf_prefs_->InitLocalPref(profile()->GetPrefs(),
                              ash::prefs::kShelfAlignmentLocal,
                              ash::prefs::kShelfAlignment);
  shelf_prefs_->InitLocalPref(profile()->GetPrefs(),
                              ash::prefs::kShelfAutoHideBehaviorLocal,
                              ash::prefs::kShelfAutoHideBehavior);
}

void ChromeShelfController::ScheduleUpdatePinnedAppsFromSync() {
  base::SingleThreadTaskRunner::GetCurrentDefault()->PostTask(
      FROM_HERE,
      base::BindOnce(&ChromeShelfController::UpdatePinnedAppsFromSync,
                     weak_ptr_factory_.GetWeakPtr()));
}

void ChromeShelfController::UpdatePinnedAppsFromSync() {
  // Do not sync pin changes during this function to avoid cyclical updates.
  // This function makes the shelf model reflect synced prefs, and should not
  // cyclically trigger sync changes (eg. ShelfItemAdded calls SyncPinPosition).
  ScopedPinSyncDisabler scoped_pin_sync_disabler = GetScopedPinSyncDisabler();

  const std::vector<ash::ShelfID> pinned_apps =
      shelf_prefs_->GetPinnedAppsFromSync(shelf_controller_helper_.get());

  int index = 0;

  // Apply pins in two steps. At the first step, go through the list of apps to
  // pin, move existing pin to current position specified by |index| or create
  // the new pin at that position.
  for (const auto& pref_shelf_id : pinned_apps) {
    const std::string app_id = pref_shelf_id.app_id;
    // Do not show apps in the shelf if they are explicitly forbidden.
    if (ShelfControllerHelper::IsAppHiddenFromShelf(profile(), app_id))
      continue;

    // Update apps icon if applicable.
    OnAppUpdated(profile(), app_id, /*reload_icon=*/true);

    // Find existing pin or app from the right of current |index|.
    int app_index = index;
    for (; app_index < model_->item_count(); ++app_index) {
      const ash::ShelfItem& item = model_->items()[app_index];
      if (item.id == pref_shelf_id)
        break;
    }

    const bool item_pinned = EnsureAppPinnedInModelAtIndex(
        app_id,
        /*current_index=*/app_index < model_->item_count() ? app_index : -1,
        /*target_index=*/index);

    if (item_pinned)
      ++index;
  }

  // At second step remove any pin to the right from the current index.
  while (index < model_->item_count()) {
    const ash::ShelfItem& item = model_->items()[index];
    if (item.type == ash::TYPE_PINNED_APP)
      UnpinShelfItemInternal(item.id);
    else
      ++index;
  }

  UpdateAppsPinStatesFromPrefs();

  ReportUpdateShelfIconList(model_);
}

bool ChromeShelfController::EnsureAppPinnedInModelAtIndex(
    const std::string& app_id,
    int current_index,
    int target_index) {
  // Passing current app index in model as an argument is an optimization in
  // case this method is used while looping over items that avoids extra pass
  // over items in the model to find the app index.
  DCHECK_EQ(current_index, model_->ItemIndexByAppID(app_id));

  if (current_index >= 0) {
    const ash::ShelfItem item = model_->items()[current_index];
    if (ItemTypeIsPinned(item)) {
      model_->Move(current_index, target_index);
    } else {
      PinRunningAppInternal(target_index, item.id);
    }
    DCHECK_EQ(model_->ItemIndexByID(item.id), target_index);
    return true;
  }

  // app_id may be kChromeAppId. This happens when sync happens,
  // but Lacros becomes the primary browser so that the browser
  // shortcut is unpinned. Do nothing then.
  if (app_id == kChromeAppId)
    return false;

  // We need to create a new pin for a synced app.
  ash::ShelfItem item;
  std::unique_ptr<ash::ShelfItemDelegate> item_delegate;
  if (!shelf_item_factory()->CreateShelfItemForAppId(app_id, &item,
                                                     &item_delegate)) {
    return false;
  }

  InsertAppItem(std::move(item_delegate), ash::STATUS_CLOSED, target_index,
                ash::TYPE_PINNED_APP,
                /*title=*/std::u16string());
  return true;
}

void ChromeShelfController::UpdateAppsPinStatesFromPrefs() {
  for (int index = 0; index < model_->item_count(); index++) {
    UpdatePinnedByPolicyForItemAtIndex(index);
  }
}

void ChromeShelfController::UpdatePinnedByPolicyForItemAtIndex(
    int model_index) {
  ash::ShelfItem item = model_->items()[model_index];
  const bool pinned_by_policy =
      GetPinnableForAppID(item.id.app_id, profile()) ==
      AppListControllerDelegate::PIN_FIXED;

  if (item.pinned_by_policy != pinned_by_policy) {
    item.pinned_by_policy = pinned_by_policy;
    model_->Set(model_index, item);
  }

  ReportUpdateShelfIconList(model_);
}

void ChromeShelfController::UpdateForcedPinStateForItemAtIndex(
    int model_index) {
  ash::ShelfItem item = model_->items()[model_index];
  auto app_type = apps::AppServiceProxyFactory::GetForProfile(profile())
                      ->AppRegistryCache()
                      .GetAppType(item.id.app_id);

  const bool pin_state_forced_by_type =
      !IsAppPinEditable(app_type, item.id.app_id, profile());
  if (item.pin_state_forced_by_type != pin_state_forced_by_type) {
    item.pin_state_forced_by_type = pin_state_forced_by_type;
    model_->Set(model_index, item);
  }

  ReportUpdateShelfIconList(model_);
}

ash::ShelfItemStatus ChromeShelfController::GetAppState(
    const std::string& app_id) {
  for (auto& it : web_contents_to_app_id_) {
    if (it.second == app_id) {
      Browser* browser = chrome::FindBrowserWithWebContents(it.first);
      // Usually there should never be an item in our |web_contents_to_app_id_|
      // list which got deleted already. However - in some situations e.g.
      // Browser::SwapTabContent there is temporarily no associated browser.
      // TODO(jamescook): This test may not be necessary anymore.
      if (!browser)
        continue;
      return ash::STATUS_RUNNING;
    }
  }
  return ash::STATUS_CLOSED;
}

ash::ShelfID ChromeShelfController::InsertAppItem(
    std::unique_ptr<ash::ShelfItemDelegate> item_delegate,
    ash::ShelfItemStatus status,
    int index,
    ash::ShelfItemType shelf_item_type,
    const std::u16string& title) {
  CHECK(item_delegate);
  if (GetItem(item_delegate->shelf_id())) {
    static bool once = true;
    if (once) {
      base::debug::DumpWithoutCrashing();
      once = false;
    }
    return item_delegate->shelf_id();
  }

  ash::ShelfItem item;
  item.status = status;
  item.type = shelf_item_type;
  item.id = item_delegate->shelf_id();
  item.title = title;
  item.app_status = ShelfControllerHelper::GetAppStatus(
      latest_active_profile_, item_delegate->shelf_id().app_id);
  model_->AddAt(index, item, std::move(item_delegate));

  ReportUpdateShelfIconList(model_);
  return item.id;
}

void ChromeShelfController::CreateBrowserShortcutItem(bool pinned) {
  // Do not sync the pin position of the browser shortcut item yet; its initial
  // position before prefs have loaded is unimportant and the sync service may
  // not yet be initialized.
  ScopedPinSyncDisabler scoped_pin_sync_disabler = GetScopedPinSyncDisabler();

  ash::ShelfItem browser_shortcut;
  browser_shortcut.type =
      pinned ? ash::TYPE_BROWSER_SHORTCUT : ash::TYPE_UNPINNED_BROWSER_SHORTCUT;
  browser_shortcut.id = ash::ShelfID(kChromeAppId);
  ui::ResourceBundle& rb = ui::ResourceBundle::GetSharedInstance();
  browser_shortcut.image = *rb.GetImageSkiaNamed(IDR_CHROME_APP_ICON_192);
  browser_shortcut.notification_badge_color =
      ash::AppIconColorCache::GetInstance().GetLightVibrantColorForApp(
          kChromeAppId, browser_shortcut.image);
  browser_shortcut.title = l10n_util::GetStringUTF16(IDS_PRODUCT_NAME);

  // If pinned, add the item towards the start of the shelf, it will be ordered
  // by weight. Otherwise put at the end as usual.
  if (pinned) {
    model_->AddAt(0, browser_shortcut,
                  std::make_unique<BrowserShortcutShelfItemController>(model_));
  } else {
    model_->Add(browser_shortcut,
                std::make_unique<BrowserShortcutShelfItemController>(model_));
  }

  ReportUpdateShelfIconList(model_);
}

int ChromeShelfController::FindInsertionPoint() {
  for (int i = model_->item_count() - 1; i > 0; --i) {
    if (ItemTypeIsPinned(model_->items()[i]))
      return i;
  }
  return 0;
}

void ChromeShelfController::CloseWindowedAppsFromRemovedExtension(
    const std::string& app_id,
    const Profile* profile) {
  // This function cannot rely on the controller's enumeration functionality
  // since the extension has already been unloaded.
  std::vector<Browser*> browser_to_close;
  for (Browser* browser : BrowserList::GetInstance()->OrderedByActivation()) {
    if ((browser->is_type_app() || browser->is_type_app_popup()) &&
        app_id == web_app::GetAppIdFromApplicationName(browser->app_name()) &&
        profile == browser->profile()) {
      browser_to_close.push_back(browser);
    }
  }
  while (!browser_to_close.empty()) {
    TabStripModel* tab_strip = browser_to_close.back()->tab_strip_model();
    if (!tab_strip->empty())
      tab_strip->CloseWebContentsAt(0, TabCloseTypes::CLOSE_NONE);
    browser_to_close.pop_back();
  }
}

void ChromeShelfController::AddAppUpdaterAndIconLoader(Profile* profile) {
  latest_active_profile_ = ProfileManager::GetActiveUserProfile();

  // For chrome restart, additional users are added during the system
  // startup phase, but we should not run the switch user process.
  if (profile == latest_active_profile_) {
    // Either add the profile to the list of known profiles and make it the
    // active one for some functions of ShelfControllerHelper or create a new
    // one.
    if (!shelf_controller_helper_.get()) {
      shelf_controller_helper_ =
          std::make_unique<ShelfControllerHelper>(profile);
    } else {
      shelf_controller_helper_->set_profile(profile);
    }
  }

  if (!base::Contains(app_updaters_, profile)) {
    std::unique_ptr<ShelfAppUpdater> app_service_app_updater(
        new ShelfAppServiceAppUpdater(this, profile));
    app_updaters_[profile].push_back(std::move(app_service_app_updater));

    // Some special extensions open new windows, and on Chrome OS, those windows
    // should show the extension icon in the shelf. Extensions are not present
    // in the App Service, so use ShelfExtensionAppUpdater to handle
    // extensions life-cycle events.
    std::unique_ptr<ShelfExtensionAppUpdater> extension_app_updater(
        new ShelfExtensionAppUpdater(this, profile,
                                     /*extensions_only=*/true));
    app_updaters_[profile].push_back(std::move(extension_app_updater));
  }

  if (!base::Contains(app_icon_loaders_, profile)) {
    std::unique_ptr<AppIconLoader> app_service_app_icon_loader =
        std::make_unique<AppServiceAppIconLoader>(
            profile, extension_misc::EXTENSION_ICON_MEDIUM, this);
    app_icon_loaders_[profile].push_back(
        std::move(app_service_app_icon_loader));

    // Some special extensions open new windows, and on Chrome OS, those windows
    // should show the extension icon in the shelf. Extensions are not present
    // in the App Service, so try loading extensions icon using
    // ChromeAppIconLoader.
    std::unique_ptr<extensions::ChromeAppIconLoader> chrome_app_icon_loader =
        std::make_unique<extensions::ChromeAppIconLoader>(
            profile, extension_misc::EXTENSION_ICON_MEDIUM,
            base::BindRepeating(&app_list::MaybeResizeAndPadIconForMd), this);
    chrome_app_icon_loader->SetExtensionsOnly();
    app_icon_loaders_[profile].push_back(std::move(chrome_app_icon_loader));
  }
}

void ChromeShelfController::AttachProfile(Profile* profile_to_attach) {
  profile_ = profile_to_attach;
  latest_active_profile_ = profile_to_attach;

  AddAppUpdaterAndIconLoader(profile_to_attach);

  pref_change_registrar_.Init(profile()->GetPrefs());
  pref_change_registrar_.Add(
      prefs::kPolicyPinnedLauncherApps,
      base::BindRepeating(&ChromeShelfController::UpdatePinnedAppsFromSync,
                          base::Unretained(this)));
  pref_change_registrar_.Add(
      arc::prefs::kArcEnabled,
      base::BindRepeating(&ChromeShelfController::UpdatePinnedAppsFromSync,
                          base::Unretained(this)));

  app_list::AppListSyncableService* app_list_syncable_service =
      app_list::AppListSyncableServiceFactory::GetForProfile(profile());
  if (app_list_syncable_service)
    app_list_syncable_service->AddObserverAndStart(this);

  PrefServiceSyncableFromProfile(profile())->AddObserver(this);
  InitLocalShelfPrefsIfOsPrefsAreSyncing();
  shelf_prefs_->AttachProfile(profile_to_attach);
}

void ChromeShelfController::ReleaseProfile() {
  pref_change_registrar_.RemoveAll();

  app_list::AppListSyncableService* app_list_syncable_service =
      app_list::AppListSyncableServiceFactory::GetForProfile(profile());
  if (app_list_syncable_service)
    app_list_syncable_service->RemoveObserver(this);

  PrefServiceSyncableFromProfile(profile())->RemoveObserver(this);
}

///////////////////////////////////////////////////////////////////////////////
// ash::ShelfModelObserver:

void ChromeShelfController::ShelfItemAdded(int index) {
  ash::ShelfID id = model_->items()[index].id;
  // Fetch the app icon, this may synchronously update the item's image.
  AppIconLoader* app_icon_loader = GetAppIconLoaderForApp(id.app_id);
  if (app_icon_loader)
    app_icon_loader->FetchImage(id.app_id);

  // Update the item with any other missing Chrome-specific info.
  // Construct |item| after FetchImage, which might synchronously load an image.
  ash::ShelfItem item = model_->items()[index];
  if (item.type == ash::TYPE_APP || item.type == ash::TYPE_PINNED_APP) {
    bool needs_update = false;
    if (item.title.empty()) {
      needs_update = true;
      item.title =
          ShelfControllerHelper::GetAppTitle(latest_active_profile_, id.app_id);
    }
    if (!BrowserAppShelfControllerShouldHandleApp(id.app_id,
                                                  latest_active_profile_)) {
      ash::ShelfItemStatus status = GetAppState(id.app_id);
      if (status != item.status && status != ash::STATUS_CLOSED) {
        needs_update = true;
        item.status = status;
      }
    }

    ash::AppStatus app_status =
        ShelfControllerHelper::GetAppStatus(latest_active_profile_, id.app_id);
    if (app_status != item.app_status) {
      needs_update = true;
      item.app_status = app_status;
    }

    if (needs_update)
      model_->Set(index, item);
  }

  UpdateForcedPinStateForItemAtIndex(index);

  // Update the pin position preference as needed.
  if (ShouldSyncItemWithReentrancy(item))
    SyncPinPosition(item.id);

  ReportUpdateShelfIconList(model_);
}

void ChromeShelfController::ShelfItemRemoved(int index,
                                             const ash::ShelfItem& old_item) {
  // Remove the pin position from preferences as needed.
  if (ShouldSyncItemWithReentrancy(old_item))
    shelf_prefs_->RemovePinPosition(profile(), old_item.id);
  if (auto* app_icon_loader = GetAppIconLoaderForApp(old_item.id.app_id))
    app_icon_loader->ClearImage(old_item.id.app_id);
}

void ChromeShelfController::ShelfItemMoved(int start_index, int target_index) {
  // Update the pin position preference as needed.
  const ash::ShelfItem& item = model_->items()[target_index];
  if (ShouldSyncItemWithReentrancy(item))
    SyncPinPosition(item.id);
}

void ChromeShelfController::ShelfItemChanged(int index,
                                             const ash::ShelfItem& old_item) {
  // Add or remove the pin position from preferences as needed.
  const ash::ShelfItem& item = model_->items()[index];
  if (!ItemTypeIsPinned(old_item) && ShouldSyncItemWithReentrancy(item))
    SyncPinPosition(item.id);
  else if (ShouldSyncItemWithReentrancy(old_item) && !ItemTypeIsPinned(item))
    shelf_prefs_->RemovePinPosition(profile(), old_item.id);

  ReportUpdateShelfIconList(model_);
}
