// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/ash/launcher/chrome_launcher_controller.h"

#include <algorithm>
#include <set>
#include <utility>

#include "ash/public/cpp/ash_pref_names.h"
#include "ash/public/cpp/remote_shelf_item_delegate.h"
#include "ash/public/cpp/shelf_item.h"
#include "ash/public/cpp/shelf_model.h"
#include "ash/public/cpp/shelf_prefs.h"
#include "ash/public/cpp/window_animation_types.h"
#include "ash/public/interfaces/constants.mojom.h"
#include "base/strings/pattern.h"
#include "base/strings/string_util.h"
#include "base/strings/utf_string_conversions.h"
#include "base/threading/thread_task_runner_handle.h"
#include "chrome/browser/chromeos/arc/arc_util.h"
#include "chrome/browser/chromeos/crostini/crostini_util.h"
#include "chrome/browser/extensions/chrome_app_icon_loader.h"
#include "chrome/browser/extensions/extension_util.h"
#include "chrome/browser/prefs/pref_service_syncable_util.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/profiles/profile_manager.h"
#include "chrome/browser/ui/app_list/app_list_client_impl.h"
#include "chrome/browser/ui/app_list/app_list_syncable_service_factory.h"
#include "chrome/browser/ui/app_list/app_sync_ui_state.h"
#include "chrome/browser/ui/app_list/arc/arc_app_icon_loader.h"
#include "chrome/browser/ui/app_list/arc/arc_app_utils.h"
#include "chrome/browser/ui/app_list/crostini/crostini_app_icon_loader.h"
#include "chrome/browser/ui/app_list/internal_app/internal_app_icon_loader.h"
#include "chrome/browser/ui/app_list/md_icon_normalizer.h"
#include "chrome/browser/ui/ash/chrome_keyboard_controller_client.h"
#include "chrome/browser/ui/ash/chrome_launcher_prefs.h"
#include "chrome/browser/ui/ash/launcher/app_shortcut_launcher_item_controller.h"
#include "chrome/browser/ui/ash/launcher/app_window_launcher_controller.h"
#include "chrome/browser/ui/ash/launcher/app_window_launcher_item_controller.h"
#include "chrome/browser/ui/ash/launcher/arc_app_window_launcher_controller.h"
#include "chrome/browser/ui/ash/launcher/browser_shortcut_launcher_item_controller.h"
#include "chrome/browser/ui/ash/launcher/browser_status_monitor.h"
#include "chrome/browser/ui/ash/launcher/chrome_launcher_controller_util.h"
#include "chrome/browser/ui/ash/launcher/crostini_app_window_shelf_controller.h"
#include "chrome/browser/ui/ash/launcher/internal_app_window_shelf_controller.h"
#include "chrome/browser/ui/ash/launcher/launcher_arc_app_updater.h"
#include "chrome/browser/ui/ash/launcher/launcher_controller_helper.h"
#include "chrome/browser/ui/ash/launcher/launcher_crostini_app_updater.h"
#include "chrome/browser/ui/ash/launcher/launcher_extension_app_updater.h"
#include "chrome/browser/ui/ash/launcher/multi_profile_app_window_launcher_controller.h"
#include "chrome/browser/ui/ash/launcher/multi_profile_browser_status_monitor.h"
#include "chrome/browser/ui/ash/launcher/shelf_spinner_controller.h"
#include "chrome/browser/ui/ash/multi_user/multi_user_util.h"
#include "chrome/browser/ui/ash/multi_user/multi_user_window_manager.h"
#include "chrome/browser/ui/ash/session_controller_client.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/browser/ui/browser_finder.h"
#include "chrome/browser/ui/browser_list.h"
#include "chrome/browser/ui/browser_window.h"
#include "chrome/browser/ui/tabs/tab_strip_model.h"
#include "chrome/browser/web_applications/components/web_app_helpers.h"
#include "chrome/common/pref_names.h"
#include "chrome/grit/chrome_unscaled_resources.h"
#include "chrome/grit/chromium_strings.h"
#include "chrome/grit/generated_resources.h"
#include "chrome/grit/theme_resources.h"
#include "chromeos/strings/grit/chromeos_strings.h"
#include "components/account_id/account_id.h"
#include "components/arc/arc_prefs.h"
#include "components/favicon/content/content_favicon_driver.h"
#include "components/strings/grit/components_strings.h"
#include "components/sync_preferences/pref_service_syncable.h"
#include "components/user_manager/user_manager.h"
#include "content/public/browser/web_contents.h"
#include "content/public/common/service_manager_connection.h"
#include "extensions/common/extension.h"
#include "services/service_manager/public/cpp/connector.h"
#include "ui/aura/window.h"
#include "ui/base/l10n/l10n_util.h"
#include "ui/base/resource/resource_bundle.h"
#include "ui/base/ui_base_features.h"
#include "ui/display/types/display_constants.h"
#include "ui/resources/grit/ui_resources.h"

using extension_misc::kChromeAppId;
using extension_misc::kGmailAppId;

namespace {

// Calls ItemSelected with |source|, default arguments, and no callback.
void SelectItemWithSource(ash::ShelfItemDelegate* delegate,
                          ash::ShelfLaunchSource source,
                          int64_t display_id) {
  delegate->ItemSelected(nullptr, display_id, source, base::DoNothing());
}

// Returns true if the given |item| has a pinned shelf item type.
bool ItemTypeIsPinned(const ash::ShelfItem& item) {
  return item.type == ash::TYPE_PINNED_APP ||
         item.type == ash::TYPE_BROWSER_SHORTCUT;
}

}  // namespace

// A class to get events from ChromeOS when a user gets changed or added.
class ChromeLauncherControllerUserSwitchObserver
    : public user_manager::UserManager::UserSessionStateObserver {
 public:
  ChromeLauncherControllerUserSwitchObserver(
      ChromeLauncherController* controller)
      : controller_(controller) {
    DCHECK(user_manager::UserManager::IsInitialized());
    user_manager::UserManager::Get()->AddSessionStateObserver(this);
  }
  ~ChromeLauncherControllerUserSwitchObserver() override {
    user_manager::UserManager::Get()->RemoveSessionStateObserver(this);
  }

  // user_manager::UserManager::UserSessionStateObserver overrides:
  void UserAddedToSession(const user_manager::User* added_user) override;

  // ChromeLauncherControllerUserSwitchObserver:
  void OnUserProfileReadyToSwitch(Profile* profile);

 private:
  // Add a user to the session.
  void AddUser(Profile* profile);

  // The owning ChromeLauncherController.
  ChromeLauncherController* controller_;

  // Users which were just added to the system, but which profiles were not yet
  // (fully) loaded.
  std::set<std::string> added_user_ids_waiting_for_profiles_;

  DISALLOW_COPY_AND_ASSIGN(ChromeLauncherControllerUserSwitchObserver);
};

void ChromeLauncherControllerUserSwitchObserver::UserAddedToSession(
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

void ChromeLauncherControllerUserSwitchObserver::OnUserProfileReadyToSwitch(
    Profile* profile) {
  if (!added_user_ids_waiting_for_profiles_.empty()) {
    // Check if the profile is from a user which was on the waiting list.
    // TODO(alemate): added_user_ids_waiting_for_profiles_ should be
    // a set<AccountId>
    std::string user_id =
        multi_user_util::GetAccountIdFromProfile(profile).GetUserEmail();
    std::set<std::string>::iterator it =
        std::find(added_user_ids_waiting_for_profiles_.begin(),
                  added_user_ids_waiting_for_profiles_.end(), user_id);
    if (it != added_user_ids_waiting_for_profiles_.end()) {
      added_user_ids_waiting_for_profiles_.erase(it);
      AddUser(profile->GetOriginalProfile());
    }
  }
}

void ChromeLauncherControllerUserSwitchObserver::AddUser(Profile* profile) {
  MultiUserWindowManager::GetInstance()->AddUser(profile);
  controller_->AdditionalUserAddedToSession(profile->GetOriginalProfile());
}

// static
ChromeLauncherController* ChromeLauncherController::instance_ = nullptr;

ChromeLauncherController::ChromeLauncherController(Profile* profile,
                                                   ash::ShelfModel* model)
    : model_(model), observer_binding_(this), weak_ptr_factory_(this) {
  DCHECK(!instance_);
  instance_ = this;

  // ShelfModel initializes the app list item and back button.
  DCHECK(model_);
  DCHECK_EQ(2, model_->item_count());
  DCHECK_EQ(ash::kBackButtonId, model_->items()[0].id.app_id);
  DCHECK_EQ(ash::kAppListId, model_->items()[1].id.app_id);

  // Start observing the shelf controller.
  if (ConnectToShelfController()) {
    ash::mojom::ShelfObserverAssociatedPtrInfo ptr_info;
    observer_binding_.Bind(mojo::MakeRequest(&ptr_info));
    shelf_controller_->AddObserver(std::move(ptr_info));
  }

  if (!profile) {
    // If no profile was passed, we take the currently active profile and use it
    // as the owner of the current desktop.
    // Use the original profile as on chromeos we may get a temporary off the
    // record profile, unless in guest session (where off the record profile is
    // the right one).
    profile = ProfileManager::GetActiveUserProfile();
    if (!profile->IsGuestSession() && !profile->IsSystemProfile())
      profile = profile->GetOriginalProfile();

    app_sync_ui_state_ = AppSyncUIState::Get(profile);
    if (app_sync_ui_state_)
      app_sync_ui_state_->AddObserver(this);
  }

  // All profile relevant settings get bound to the current profile.
  AttachProfile(profile);
  DCHECK_EQ(profile, profile_);
  model_->AddObserver(this);

  shelf_spinner_controller_.reset(new ShelfSpinnerController(this));

  // Create either the real window manager or a stub.
  MultiUserWindowManager::CreateInstance();

  // On Chrome OS using multi profile we want to switch the content of the shelf
  // with a user change. Note that for unit tests the instance can be NULL.
  if (SessionControllerClient::IsMultiProfileAvailable()) {
    user_switch_observer_.reset(
        new ChromeLauncherControllerUserSwitchObserver(this));
  }

  std::unique_ptr<AppWindowLauncherController> extension_app_window_controller;
  // Create our v1/v2 application / browser monitors which will inform the
  // launcher of status changes.
  if (SessionControllerClient::IsMultiProfileAvailable()) {
    // If running in separated destkop mode, we create the multi profile version
    // of status monitor.
    browser_status_monitor_.reset(new MultiProfileBrowserStatusMonitor(this));
    browser_status_monitor_->Initialize();
    extension_app_window_controller.reset(
        new MultiProfileAppWindowLauncherController(this));
  } else {
    // Create our v1/v2 application / browser monitors which will inform the
    // launcher of status changes.
    browser_status_monitor_.reset(new BrowserStatusMonitor(this));
    browser_status_monitor_->Initialize();
    extension_app_window_controller.reset(
        new ExtensionAppWindowLauncherController(this));
  }
  app_window_controllers_.push_back(std::move(extension_app_window_controller));
  app_window_controllers_.push_back(
      std::make_unique<ArcAppWindowLauncherController>(this));
  if (crostini::IsCrostiniUIAllowedForProfile(profile)) {
    std::unique_ptr<CrostiniAppWindowShelfController> crostini_controller =
        std::make_unique<CrostiniAppWindowShelfController>(this);
    crostini_app_window_shelf_controller_ = crostini_controller.get();
    app_window_controllers_.emplace_back(std::move(crostini_controller));
  }
  app_window_controllers_.push_back(
      std::make_unique<InternalAppWindowShelfController>(this));
}

ChromeLauncherController::~ChromeLauncherController() {
  // Reset the BrowserStatusMonitor as it has a weak pointer to this.
  browser_status_monitor_.reset();

  // Reset the app window controllers here since it has a weak pointer to this.
  app_window_controllers_.clear();

  // Destroy the ShelfSpinnerController before clearing delegates.
  shelf_spinner_controller_.reset();

  // Destroy local shelf item delegates; some subclasses have complex cleanup.
  model_->DestroyItemDelegates();

  model_->RemoveObserver(this);

  // Release all profile dependent resources.
  ReleaseProfile();

  // Get rid of the multi user window manager instance.
  MultiUserWindowManager::DeleteInstance();

  if (instance_ == this)
    instance_ = nullptr;
}

void ChromeLauncherController::Init() {
  CreateBrowserShortcutLauncherItem();
  UpdateAppLaunchersFromPref();
  SetVirtualKeyboardBehaviorFromPrefs();
}

ash::ShelfID ChromeLauncherController::CreateAppLauncherItem(
    std::unique_ptr<ash::ShelfItemDelegate> item_delegate,
    ash::ShelfItemStatus status,
    const base::string16& title) {
  return InsertAppLauncherItem(std::move(item_delegate), status,
                               model_->item_count(), ash::TYPE_APP, title);
}

const ash::ShelfItem* ChromeLauncherController::GetItem(
    const ash::ShelfID& id) const {
  const int index = model_->ItemIndexByID(id);
  if (index >= 0 && index < model_->item_count())
    return &model_->items()[index];
  return nullptr;
}

void ChromeLauncherController::SetItemType(const ash::ShelfID& id,
                                           ash::ShelfItemType type) {
  const ash::ShelfItem* item = GetItem(id);
  if (item && item->type != type) {
    ash::ShelfItem new_item = *item;
    new_item.type = type;
    model_->Set(model_->ItemIndexByID(id), new_item);
  }
}

void ChromeLauncherController::SetItemStatus(const ash::ShelfID& id,
                                             ash::ShelfItemStatus status) {
  const ash::ShelfItem* item = GetItem(id);
  if (item && item->status != status) {
    ash::ShelfItem new_item = *item;
    new_item.status = status;
    model_->Set(model_->ItemIndexByID(id), new_item);
  }
}

void ChromeLauncherController::SetItemTitle(const ash::ShelfID& id,
                                            const base::string16& title) {
  const ash::ShelfItem* item = GetItem(id);
  if (item && item->title != title) {
    ash::ShelfItem new_item = *item;
    new_item.title = title;
    model_->Set(model_->ItemIndexByID(id), new_item);
  }
}

void ChromeLauncherController::CloseLauncherItem(const ash::ShelfID& id) {
  CHECK(!id.IsNull());
  if (IsPinned(id)) {
    // Create a new shortcut delegate.
    SetItemStatus(id, ash::STATUS_CLOSED);
    model_->SetShelfItemDelegate(id,
                                 AppShortcutLauncherItemController::Create(id));
  } else {
    RemoveShelfItem(id);
  }
}

void ChromeLauncherController::UnpinShelfItemInternal(const ash::ShelfID& id) {
  const ash::ShelfItem* item = GetItem(id);
  if (item && item->status != ash::STATUS_CLOSED)
    UnpinRunningAppInternal(model_->ItemIndexByID(id));
  else
    RemoveShelfItem(id);
}

bool ChromeLauncherController::IsPinned(const ash::ShelfID& id) {
  const ash::ShelfItem* item = GetItem(id);
  return item && ItemTypeIsPinned(*item);
}

void ChromeLauncherController::SetV1AppStatus(const std::string& app_id,
                                              ash::ShelfItemStatus status) {
  ash::ShelfID id(app_id);
  const ash::ShelfItem* item = GetItem(id);
  if (item) {
    if (!IsPinned(id) && status == ash::STATUS_CLOSED)
      RemoveShelfItem(id);
    else
      SetItemStatus(id, status);
  } else if (status != ash::STATUS_CLOSED && !app_id.empty()) {
    InsertAppLauncherItem(
        AppShortcutLauncherItemController::Create(ash::ShelfID(app_id)), status,
        model_->item_count(), ash::TYPE_APP);
  }
}

void ChromeLauncherController::Close(const ash::ShelfID& id) {
  ash::ShelfItemDelegate* delegate = model_->GetShelfItemDelegate(id);
  if (!delegate)
    return;  // May happen if menu closed.
  delegate->Close();
}

bool ChromeLauncherController::IsOpen(const ash::ShelfID& id) {
  const ash::ShelfItem* item = GetItem(id);
  return item && item->status != ash::STATUS_CLOSED;
}

bool ChromeLauncherController::IsPlatformApp(const ash::ShelfID& id) {
  const extensions::Extension* extension =
      GetExtensionForAppID(id.app_id, profile());
  // An extension can be synced / updated at any time and therefore not be
  // available.
  return extension ? extension->is_platform_app() : false;
}

void ChromeLauncherController::LaunchApp(const ash::ShelfID& id,
                                         ash::ShelfLaunchSource source,
                                         int event_flags,
                                         int64_t display_id) {
  launcher_controller_helper_->LaunchApp(id, source, event_flags, display_id);
}

void ChromeLauncherController::ActivateApp(const std::string& app_id,
                                           ash::ShelfLaunchSource source,
                                           int event_flags,
                                           int64_t display_id) {
  // If there is an existing delegate for this app, select it.
  const ash::ShelfID shelf_id(app_id);
  ash::ShelfItemDelegate* delegate = model_->GetShelfItemDelegate(shelf_id);
  if (delegate) {
    SelectItemWithSource(delegate, source, display_id);
    return;
  }

  // Create a temporary delegate to see if there are running app instances.
  std::unique_ptr<AppShortcutLauncherItemController> item_delegate =
      AppShortcutLauncherItemController::Create(shelf_id);
  if (!item_delegate->GetRunningApplications().empty())
    SelectItemWithSource(item_delegate.get(), source, display_id);
  else
    LaunchApp(shelf_id, source, event_flags, display_id);
}

void ChromeLauncherController::SetLauncherItemImage(
    const ash::ShelfID& shelf_id,
    const gfx::ImageSkia& image) {
  DCHECK(!image.isNull());
  const ash::ShelfItem* item = GetItem(shelf_id);
  if (item) {
    ash::ShelfItem new_item = *item;
    new_item.image = image;
    // Update the image in Ash's ShelfModel, ShelfItemChanged strips images.
    if (shelf_controller_)
      shelf_controller_->UpdateShelfItem(new_item);
    else
      model_->Set(model_->ItemIndexByID(shelf_id), new_item);
  }
}

void ChromeLauncherController::UpdateLauncherItemImage(
    const std::string& app_id) {
  AppIconLoader* icon_loader = GetAppIconLoaderForApp(app_id);
  if (icon_loader)
    icon_loader->UpdateImage(app_id);
}

void ChromeLauncherController::UpdateAppState(content::WebContents* contents,
                                              bool remove) {
  ash::ShelfID shelf_id(launcher_controller_helper_->GetAppID(contents));

  // Check if the gMail app is loaded and it matches the given content.
  // This special treatment is needed to address crbug.com/234268.
  if (shelf_id.IsNull() && ContentCanBeHandledByGmailApp(contents))
    shelf_id = ash::ShelfID(kGmailAppId);

  // If the tab changed apps, remove its association with the previous app item.
  if (web_contents_to_app_id_.find(contents) != web_contents_to_app_id_.end()) {
    ash::ShelfID old_id(web_contents_to_app_id_[contents]);
    if (old_id != shelf_id && GetItem(old_id) != nullptr) {
      // Since GetAppState() will use |web_contents_to_app_id_| we remove
      // the connection before calling it.
      web_contents_to_app_id_.erase(contents);
      SetItemStatus(old_id, GetAppState(old_id.app_id));
    }
  }

  if (remove)
    web_contents_to_app_id_.erase(contents);
  else
    web_contents_to_app_id_[contents] = shelf_id.app_id;

  SetItemStatus(shelf_id, GetAppState(shelf_id.app_id));
}

ash::ShelfID ChromeLauncherController::GetShelfIDForWebContents(
    content::WebContents* contents) {
  std::string app_id = launcher_controller_helper_->GetAppID(contents);
  if (app_id.empty() && ContentCanBeHandledByGmailApp(contents))
    app_id = kGmailAppId;

  // If there is no dedicated app item, use the browser shortcut item.
  const ash::ShelfItem* item = GetItem(ash::ShelfID(app_id));
  return item ? item->id : ash::ShelfID(kChromeAppId);
}

void ChromeLauncherController::SetRefocusURLPatternForTest(
    const ash::ShelfID& id,
    const GURL& url) {
  const ash::ShelfItem* item = GetItem(id);
  if (item && !IsPlatformApp(id) &&
      (item->type == ash::TYPE_PINNED_APP || item->type == ash::TYPE_APP)) {
    ash::ShelfItemDelegate* delegate = model_->GetShelfItemDelegate(id);
    AppShortcutLauncherItemController* item_controller =
        static_cast<AppShortcutLauncherItemController*>(delegate);
    item_controller->set_refocus_url(url);
  } else {
    NOTREACHED() << "Invalid launcher item or type";
  }
}

ash::ShelfAction ChromeLauncherController::ActivateWindowOrMinimizeIfActive(
    ui::BaseWindow* window,
    bool allow_minimize) {
  // We might have to teleport a window back to the current user.
  aura::Window* native_window = window->GetNativeWindow();
  const AccountId& current_account_id =
      multi_user_util::GetAccountIdFromProfile(profile());
  MultiUserWindowManager* manager = MultiUserWindowManager::GetInstance();
  if (!manager->IsWindowOnDesktopOfUser(native_window, current_account_id)) {
    manager->ShowWindowForUser(native_window, current_account_id);
    window->Activate();
    return ash::SHELF_ACTION_WINDOW_ACTIVATED;
  }

  AppListClientImpl* app_list_client = AppListClientImpl::GetInstance();
  if (window->IsActive() && allow_minimize &&
      !(app_list_client && app_list_client->app_list_target_visibility())) {
    window->Minimize();
    return ash::SHELF_ACTION_WINDOW_MINIMIZED;
  }

  if (app_list_client && app_list_client->IsHomeLauncherEnabledInTabletMode()) {
    // Run slide down animation to show the window.
    wm::SetWindowVisibilityAnimationType(
        native_window, ash::wm::WINDOW_VISIBILITY_ANIMATION_TYPE_SLIDE_DOWN);
  }

  window->Show();
  window->Activate();
  return ash::SHELF_ACTION_WINDOW_ACTIVATED;
}

void ChromeLauncherController::ActiveUserChanged(
    const std::string& user_email) {
  // Store the order of running applications for the user which gets inactive.
  RememberUnpinnedRunningApplicationOrder();
  // Coming here the default profile is already switched. All profile specific
  // resources get released and the new profile gets attached instead.
  ReleaseProfile();
  // When coming here, the active user has already be changed so that we can
  // set it as active.
  AttachProfile(ProfileManager::GetActiveUserProfile());
  // Update the V1 applications.
  browser_status_monitor_->ActiveUserChanged(user_email);
  // Switch the running applications to the new user.
  for (auto& controller : app_window_controllers_)
    controller->ActiveUserChanged(user_email);
  // Update the user specific shell properties from the new user profile.
  // Shelf preferences are loaded in ChromeLauncherController::AttachProfile.
  UpdateAppLaunchersFromPref();
  SetVirtualKeyboardBehaviorFromPrefs();

  // Restore the order of running, but unpinned applications for the activated
  // user.
  RestoreUnpinnedRunningApplicationOrder(user_email);
}

void ChromeLauncherController::AdditionalUserAddedToSession(Profile* profile) {
  // Switch the running applications to the new user.
  for (auto& controller : app_window_controllers_)
    controller->AdditionalUserAddedToSession(profile);
}

ash::MenuItemList ChromeLauncherController::GetAppMenuItemsForTesting(
    const ash::ShelfItem& item) {
  ash::ShelfItemDelegate* delegate = model_->GetShelfItemDelegate(item.id);
  return delegate ? delegate->GetAppMenuItems(ui::EF_NONE)
                  : ash::MenuItemList();
}

std::vector<content::WebContents*>
ChromeLauncherController::GetV1ApplicationsFromAppId(
    const std::string& app_id) {
  // Use the app's shelf item to find that app's windows.
  const ash::ShelfItem* item = GetItem(ash::ShelfID(app_id));
  if (!item)
    return std::vector<content::WebContents*>();

  // This should only be called for apps.
  DCHECK(item->type == ash::TYPE_APP || item->type == ash::TYPE_PINNED_APP);

  ash::ShelfItemDelegate* delegate = model_->GetShelfItemDelegate(item->id);
  AppShortcutLauncherItemController* item_controller =
      static_cast<AppShortcutLauncherItemController*>(delegate);
  return item_controller->GetRunningApplications();
}

void ChromeLauncherController::ActivateShellApp(const std::string& app_id,
                                                int window_index) {
  const ash::ShelfItem* item = GetItem(ash::ShelfID(app_id));
  if (item &&
      (item->type == ash::TYPE_APP || item->type == ash::TYPE_PINNED_APP)) {
    ash::ShelfItemDelegate* delegate = model_->GetShelfItemDelegate(item->id);
    AppWindowLauncherItemController* item_controller =
        delegate->AsAppWindowLauncherItemController();
    item_controller->ActivateIndexedApp(window_index);
  }
}

bool ChromeLauncherController::IsWebContentHandledByApplication(
    content::WebContents* web_contents,
    const std::string& app_id) {
  if ((web_contents_to_app_id_.find(web_contents) !=
       web_contents_to_app_id_.end()) &&
      (web_contents_to_app_id_[web_contents] == app_id))
    return true;
  return (app_id == kGmailAppId && ContentCanBeHandledByGmailApp(web_contents));
}

bool ChromeLauncherController::ContentCanBeHandledByGmailApp(
    content::WebContents* web_contents) {
  if (GetItem(ash::ShelfID(kGmailAppId)) != nullptr) {
    const GURL url = web_contents->GetURL();
    // We need to extend the application matching for the gMail app beyond the
    // manifest file's specification. This is required because of the namespace
    // overlap with the offline app ("/mail/mu/").
    if (!base::MatchPattern(url.path(), "/mail/mu/*") &&
        base::MatchPattern(url.path(), "/mail/*") &&
        GetExtensionForAppID(kGmailAppId, profile()) &&
        GetExtensionForAppID(kGmailAppId, profile())->OverlapsWithOrigin(url))
      return true;
  }
  return false;
}

gfx::Image ChromeLauncherController::GetAppListIcon(
    content::WebContents* web_contents) const {
  ui::ResourceBundle& rb = ui::ResourceBundle::GetSharedInstance();
  if (IsIncognito(web_contents))
    return rb.GetImageNamed(IDR_ASH_SHELF_LIST_INCOGNITO_BROWSER);
  favicon::FaviconDriver* favicon_driver =
      favicon::ContentFaviconDriver::FromWebContents(web_contents);
  gfx::Image result = favicon_driver->GetFavicon();
  if (result.IsEmpty())
    return rb.GetImageNamed(IDR_DEFAULT_FAVICON);
  return result;
}

base::string16 ChromeLauncherController::GetAppListTitle(
    content::WebContents* web_contents) const {
  base::string16 title = web_contents->GetTitle();
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

BrowserShortcutLauncherItemController*
ChromeLauncherController::GetBrowserShortcutLauncherItemController() {
  ash::ShelfID id(kChromeAppId);
  ash::mojom::ShelfItemDelegate* delegate = model_->GetShelfItemDelegate(id);
  DCHECK(delegate) << "There should be always be a browser shortcut item.";
  return static_cast<BrowserShortcutLauncherItemController*>(delegate);
}

void ChromeLauncherController::OnUserProfileReadyToSwitch(Profile* profile) {
  if (user_switch_observer_.get())
    user_switch_observer_->OnUserProfileReadyToSwitch(profile);
}

ShelfSpinnerController* ChromeLauncherController::GetShelfSpinnerController() {
  return shelf_spinner_controller_.get();
}

ChromeLauncherController::ScopedPinSyncDisabler
ChromeLauncherController::GetScopedPinSyncDisabler() {
  // Only one temporary disabler should not exist at a time.
  DCHECK(should_sync_pin_changes_);
  return std::make_unique<base::AutoReset<bool>>(&should_sync_pin_changes_,
                                                 false);
}

void ChromeLauncherController::SetLauncherControllerHelperForTest(
    std::unique_ptr<LauncherControllerHelper> helper) {
  launcher_controller_helper_ = std::move(helper);
}

void ChromeLauncherController::SetAppIconLoadersForTest(
    std::vector<std::unique_ptr<AppIconLoader>>& loaders) {
  app_icon_loaders_.clear();
  for (auto& loader : loaders)
    app_icon_loaders_.push_back(std::move(loader));
}

void ChromeLauncherController::SetProfileForTest(Profile* profile) {
  profile_ = profile;
}

void ChromeLauncherController::FlushForTesting() {
  observer_binding_.FlushForTesting();
}

void ChromeLauncherController::PinAppWithID(const std::string& app_id) {
  model_->PinAppWithID(app_id);
}

bool ChromeLauncherController::IsAppPinned(const std::string& app_id) {
  return model_->IsAppPinned(app_id);
}

void ChromeLauncherController::UnpinAppWithID(const std::string& app_id) {
  model_->UnpinAppWithID(app_id);
}

AppIconLoader* ChromeLauncherController::GetAppIconLoaderForApp(
    const std::string& app_id) {
  for (const auto& app_icon_loader : app_icon_loaders_) {
    if (app_icon_loader->CanLoadImageForApp(app_id))
      return app_icon_loader.get();
  }

  return nullptr;
}

///////////////////////////////////////////////////////////////////////////////
// LauncherAppUpdater::Delegate:

void ChromeLauncherController::OnAppInstalled(
    content::BrowserContext* browser_context,
    const std::string& app_id) {
  if (IsAppPinned(app_id)) {
    // Clear and re-fetch to ensure icon is up-to-date.
    AppIconLoader* app_icon_loader = GetAppIconLoaderForApp(app_id);
    if (app_icon_loader) {
      app_icon_loader->ClearImage(app_id);
      app_icon_loader->FetchImage(app_id);
    }
  }

  UpdateAppLaunchersFromPref();
}

void ChromeLauncherController::OnAppUninstalledPrepared(
    content::BrowserContext* browser_context,
    const std::string& app_id) {
  // Since we might have windowed apps of this type which might have
  // outstanding locks which needs to be removed.
  const Profile* profile = Profile::FromBrowserContext(browser_context);
  ash::ShelfID shelf_id(app_id);
  if (GetItem(shelf_id) != nullptr)
    CloseWindowedAppsFromRemovedExtension(app_id, profile);

  // Some apps may be removed locally. Unpin the item without removing the pin
  // position from profile preferences. When needed, it is automatically deleted
  // on app list model update.
  if (IsAppPinned(app_id) && profile == this->profile())
    UnpinShelfItemInternal(shelf_id);
}

///////////////////////////////////////////////////////////////////////////////
// AppIconLoaderDelegate:

void ChromeLauncherController::OnAppImageUpdated(const std::string& app_id,
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
    // Update the image in Ash's ShelfModel, ShelfItemChanged strips images.
    if (shelf_controller_)
      shelf_controller_->UpdateShelfItem(item);
    else
      model_->Set(index, item);
    // It's possible we're waiting on more than one item, so don't break.
  }
}

///////////////////////////////////////////////////////////////////////////////
// ChromeLauncherController protected:

bool ChromeLauncherController::ConnectToShelfController() {
  if (shelf_controller_.is_bound())
    return true;

  auto* connection = content::ServiceManagerConnection::GetForProcess();
  auto* connector = connection ? connection->GetConnector() : nullptr;
  // Unit tests may not have a connector.
  if (!connector)
    return false;

  connector->BindInterface(ash::mojom::kServiceName, &shelf_controller_);
  return true;
}

///////////////////////////////////////////////////////////////////////////////
// ChromeLauncherController private:

ash::ShelfID ChromeLauncherController::CreateAppShortcutLauncherItem(
    const ash::ShelfID& shelf_id,
    int index) {
  return InsertAppLauncherItem(
      AppShortcutLauncherItemController::Create(shelf_id), ash::STATUS_CLOSED,
      index, ash::TYPE_PINNED_APP);
}

void ChromeLauncherController::RememberUnpinnedRunningApplicationOrder() {
  RunningAppListIds list;
  for (int i = 0; i < model_->item_count(); i++) {
    if (model_->items()[i].type == ash::TYPE_APP)
      list.push_back(model_->items()[i].id.app_id);
  }
  const std::string user_email =
      multi_user_util::GetAccountIdFromProfile(profile()).GetUserEmail();
  last_used_running_application_order_[user_email] = list;
}

void ChromeLauncherController::RestoreUnpinnedRunningApplicationOrder(
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

void ChromeLauncherController::RemoveShelfItem(const ash::ShelfID& id) {
  const int index = model_->ItemIndexByID(id);
  if (index >= 0 && index < model_->item_count())
    model_->RemoveItemAt(index);
}

void ChromeLauncherController::PinRunningAppInternal(
    int index,
    const ash::ShelfID& shelf_id) {
  DCHECK_EQ(GetItem(shelf_id)->type, ash::TYPE_APP);
  SetItemType(shelf_id, ash::TYPE_PINNED_APP);
  int running_index = model_->ItemIndexByID(shelf_id);
  if (running_index < index)
    --index;
  if (running_index != index)
    model_->Move(running_index, index);
}

void ChromeLauncherController::UnpinRunningAppInternal(int index) {
  DCHECK(index >= 0 && index < model_->item_count());
  ash::ShelfItem item = model_->items()[index];
  DCHECK_EQ(item.type, ash::TYPE_PINNED_APP);
  SetItemType(item.id, ash::TYPE_APP);
}

void ChromeLauncherController::SyncPinPosition(const ash::ShelfID& shelf_id) {
  DCHECK(should_sync_pin_changes_);
  DCHECK(!shelf_id.IsNull());

  const int max_index = model_->item_count();
  const int index = model_->ItemIndexByID(shelf_id);
  DCHECK_GT(index, 0);

  ash::ShelfID shelf_id_before;
  std::vector<ash::ShelfID> shelf_ids_after;

  for (int i = index - 1; i > 0; --i) {
    shelf_id_before = model_->items()[i].id;
    if (IsPinned(shelf_id_before))
      break;
  }

  for (int i = index + 1; i < max_index; ++i) {
    const ash::ShelfID& shelf_id_after = model_->items()[i].id;
    if (IsPinned(shelf_id_after))
      shelf_ids_after.push_back(shelf_id_after);
  }

  SetPinPosition(profile(), shelf_id, shelf_id_before, shelf_ids_after);
}

void ChromeLauncherController::OnSyncModelUpdated() {
  UpdateAppLaunchersFromPref();
}

void ChromeLauncherController::OnIsSyncingChanged() {
  UpdateAppLaunchersFromPref();

  // Initialize the local prefs if this is the first time sync has occurred.
  if (!PrefServiceSyncableFromProfile(profile())->IsSyncing())
    return;
  InitLocalPref(profile()->GetPrefs(), ash::prefs::kShelfAlignmentLocal,
                ash::prefs::kShelfAlignment);
  InitLocalPref(profile()->GetPrefs(), ash::prefs::kShelfAutoHideBehaviorLocal,
                ash::prefs::kShelfAutoHideBehavior);
}

void ChromeLauncherController::ScheduleUpdateAppLaunchersFromPref() {
  base::ThreadTaskRunnerHandle::Get()->PostTask(
      FROM_HERE,
      base::BindOnce(&ChromeLauncherController::UpdateAppLaunchersFromPref,
                     weak_ptr_factory_.GetWeakPtr()));
}

void ChromeLauncherController::UpdateAppLaunchersFromPref() {
  // Do not sync pin changes during this function to avoid cyclical updates.
  // This function makes the shelf model reflect synced prefs, and should not
  // cyclically trigger sync changes (eg. ShelfItemAdded calls SyncPinPosition).
  ScopedPinSyncDisabler scoped_pin_sync_disabler = GetScopedPinSyncDisabler();

  const std::vector<ash::ShelfID> pinned_apps = GetPinnedAppsFromPrefs(
      profile()->GetPrefs(), launcher_controller_helper_.get());

  int index = 0;
  // Skip the app list and back button if they exist.
  if (model_->items()[0].type == ash::TYPE_BACK_BUTTON)
    ++index;
  if (model_->items()[1].type == ash::TYPE_APP_LIST)
    ++index;

  // Apply pins in two steps. At the first step, go through the list of apps to
  // pin, move existing pin to current position specified by |index| or create
  // the new pin at that position.
  for (const auto& pref_shelf_id : pinned_apps) {
    // Update apps icon if applicable.
    OnAppUpdated(profile(), pref_shelf_id.app_id);

    // Find existing pin or app from the right of current |index|.
    int app_index = index;
    for (; app_index < model_->item_count(); ++app_index) {
      const ash::ShelfItem& item = model_->items()[app_index];
      if (item.id == pref_shelf_id)
        break;
    }
    if (app_index < model_->item_count()) {
      // Found existing pin or running app.
      const ash::ShelfItem item = model_->items()[app_index];
      if (ItemTypeIsPinned(item)) {
        // Just move to required position or keep it inplace.
        model_->Move(app_index, index);
      } else {
        PinRunningAppInternal(index, item.id);
      }
      DCHECK_EQ(model_->ItemIndexByID(item.id), index);
    } else {
      // This is fresh pin. Create new one.
      DCHECK_NE(pref_shelf_id.app_id, kChromeAppId);
      CreateAppShortcutLauncherItem(pref_shelf_id, index);
    }
    ++index;
  }

  // At second step remove any pin to the right from the current index.
  while (index < model_->item_count()) {
    const ash::ShelfItem item = model_->items()[index];
    if (item.type == ash::TYPE_PINNED_APP)
      UnpinShelfItemInternal(item.id);
    else
      ++index;
  }

  UpdatePolicyPinnedAppsFromPrefs();
}

void ChromeLauncherController::UpdatePolicyPinnedAppsFromPrefs() {
  for (int index = 0; index < model_->item_count(); index++) {
    ash::ShelfItem item = model_->items()[index];
    const bool pinned_by_policy =
        GetPinnableForAppID(item.id.app_id, profile()) ==
        AppListControllerDelegate::PIN_FIXED;
    if (item.pinned_by_policy != pinned_by_policy) {
      item.pinned_by_policy = pinned_by_policy;
      model_->Set(index, item);
    }
  }
}

void ChromeLauncherController::SetVirtualKeyboardBehaviorFromPrefs() {
  using keyboard::mojom::KeyboardEnableFlag;
  if (!ChromeKeyboardControllerClient::HasInstance())  // May be null in tests
    return;
  auto* client = ChromeKeyboardControllerClient::Get();
  const PrefService* service = profile()->GetPrefs();
  if (service->HasPrefPath(prefs::kTouchVirtualKeyboardEnabled)) {
    // Since these flags are mutually exclusive, setting one clears the other.
    client->SetEnableFlag(
        service->GetBoolean(prefs::kTouchVirtualKeyboardEnabled)
            ? KeyboardEnableFlag::kPolicyEnabled
            : KeyboardEnableFlag::kPolicyDisabled);
  } else {
    client->ClearEnableFlag(KeyboardEnableFlag::kPolicyDisabled);
    client->ClearEnableFlag(KeyboardEnableFlag::kPolicyEnabled);
  }
}

ash::ShelfItemStatus ChromeLauncherController::GetAppState(
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

ash::ShelfID ChromeLauncherController::InsertAppLauncherItem(
    std::unique_ptr<ash::ShelfItemDelegate> item_delegate,
    ash::ShelfItemStatus status,
    int index,
    ash::ShelfItemType shelf_item_type,
    const base::string16& title) {
  CHECK(item_delegate);
  CHECK(!GetItem(item_delegate->shelf_id()));
  ash::ShelfItem item;
  item.status = status;
  item.type = shelf_item_type;
  item.id = item_delegate->shelf_id();
  item.title = title;
  // Set the delegate first to avoid constructing one in ShelfItemAdded.
  model_->SetShelfItemDelegate(item.id, std::move(item_delegate));
  model_->AddAt(index, item);
  return item.id;
}

void ChromeLauncherController::CreateBrowserShortcutLauncherItem() {
  // Do not sync the pin position of the browser shortcut item yet; its initial
  // position before prefs have loaded is unimportant and the sync service may
  // not yet be initialized.
  ScopedPinSyncDisabler scoped_pin_sync_disabler = GetScopedPinSyncDisabler();

  ash::ShelfItem browser_shortcut;
  browser_shortcut.type = ash::TYPE_BROWSER_SHORTCUT;
  browser_shortcut.id = ash::ShelfID(kChromeAppId);
  ui::ResourceBundle& rb = ui::ResourceBundle::GetSharedInstance();
  browser_shortcut.image = *rb.GetImageSkiaNamed(IDR_CHROME_APP_ICON_192);
  browser_shortcut.title = l10n_util::GetStringUTF16(IDS_PRODUCT_NAME);
  std::unique_ptr<BrowserShortcutLauncherItemController> item_delegate =
      std::make_unique<BrowserShortcutLauncherItemController>(model_);
  BrowserShortcutLauncherItemController* item_controller = item_delegate.get();
  // Set the delegate first to avoid constructing another one in ShelfItemAdded.
  model_->SetShelfItemDelegate(browser_shortcut.id, std::move(item_delegate));
  // Add the item towards the start of the shelf, it will be ordered by weight.
  model_->AddAt(0, browser_shortcut);
  item_controller->UpdateBrowserItemState();
}

bool ChromeLauncherController::IsIncognito(
    const content::WebContents* web_contents) const {
  const Profile* profile =
      Profile::FromBrowserContext(web_contents->GetBrowserContext());
  return profile->IsOffTheRecord() && !profile->IsGuestSession() &&
         !profile->IsSystemProfile();
}

int ChromeLauncherController::FindInsertionPoint() {
  for (int i = model_->item_count() - 1; i > 0; --i) {
    if (ItemTypeIsPinned(model_->items()[i]))
      return i;
  }
  return 0;
}

void ChromeLauncherController::CloseWindowedAppsFromRemovedExtension(
    const std::string& app_id,
    const Profile* profile) {
  // This function cannot rely on the controller's enumeration functionality
  // since the extension has already been unloaded.
  const BrowserList* browser_list = BrowserList::GetInstance();
  std::vector<Browser*> browser_to_close;
  for (BrowserList::const_reverse_iterator it =
           browser_list->begin_last_active();
       it != browser_list->end_last_active(); ++it) {
    Browser* browser = *it;
    if (!browser->is_type_tabbed() && browser->is_type_popup() &&
        browser->is_app() &&
        app_id == web_app::GetAppIdFromApplicationName(browser->app_name()) &&
        profile == browser->profile()) {
      browser_to_close.push_back(browser);
    }
  }
  while (!browser_to_close.empty()) {
    TabStripModel* tab_strip = browser_to_close.back()->tab_strip_model();
    if (!tab_strip->empty())
      tab_strip->CloseWebContentsAt(0, TabStripModel::CLOSE_NONE);
    browser_to_close.pop_back();
  }
}

void ChromeLauncherController::AttachProfile(Profile* profile_to_attach) {
  profile_ = profile_to_attach;
  // Either add the profile to the list of known profiles and make it the active
  // one for some functions of LauncherControllerHelper or create a new one.
  if (!launcher_controller_helper_.get()) {
    launcher_controller_helper_ =
        std::make_unique<LauncherControllerHelper>(profile_);
  } else {
    launcher_controller_helper_->set_profile(profile_);
  }

  // TODO(skuhne): The AppIconLoaderImpl has the same problem. Each loaded
  // image is associated with a profile (its loader requires the profile).
  // Since icon size changes are possible, the icon could be requested to be
  // reloaded. However - having it not multi profile aware would cause problems
  // if the icon cache gets deleted upon user switch.
  std::unique_ptr<AppIconLoader> chrome_app_icon_loader =
      std::make_unique<extensions::ChromeAppIconLoader>(
          profile_, extension_misc::EXTENSION_ICON_MEDIUM,
          base::BindRepeating(&app_list::MaybeResizeAndPadIconForMd), this);
  app_icon_loaders_.push_back(std::move(chrome_app_icon_loader));

  if (arc::IsArcAllowedForProfile(profile_)) {
    std::unique_ptr<AppIconLoader> arc_app_icon_loader =
        std::make_unique<ArcAppIconLoader>(
            profile_, extension_misc::EXTENSION_ICON_MEDIUM, this);
    app_icon_loaders_.push_back(std::move(arc_app_icon_loader));
  }

  std::unique_ptr<AppIconLoader> internal_app_icon_loader =
      std::make_unique<InternalAppIconLoader>(
          profile_, extension_misc::EXTENSION_ICON_MEDIUM, this);
  app_icon_loaders_.push_back(std::move(internal_app_icon_loader));

  if (crostini::IsCrostiniUIAllowedForProfile(profile_)) {
    std::unique_ptr<AppIconLoader> crostini_app_icon_loader =
        std::make_unique<CrostiniAppIconLoader>(
            profile_, extension_misc::EXTENSION_ICON_MEDIUM, this);
    app_icon_loaders_.push_back(std::move(crostini_app_icon_loader));
  }

  pref_change_registrar_.Init(profile()->GetPrefs());
  pref_change_registrar_.Add(
      prefs::kPolicyPinnedLauncherApps,
      base::Bind(&ChromeLauncherController::UpdateAppLaunchersFromPref,
                 base::Unretained(this)));
  // Handling of prefs::kArcEnabled change should be called deferred to avoid
  // race condition when OnAppUninstalledPrepared for ARC apps is called after
  // UpdateAppLaunchersFromPref.
  pref_change_registrar_.Add(
      arc::prefs::kArcEnabled,
      base::Bind(&ChromeLauncherController::ScheduleUpdateAppLaunchersFromPref,
                 base::Unretained(this)));
  pref_change_registrar_.Add(
      prefs::kTouchVirtualKeyboardEnabled,
      base::Bind(&ChromeLauncherController::SetVirtualKeyboardBehaviorFromPrefs,
                 base::Unretained(this)));

  std::unique_ptr<LauncherAppUpdater> extension_app_updater(
      new LauncherExtensionAppUpdater(this, profile()));
  app_updaters_.push_back(std::move(extension_app_updater));

  if (arc::IsArcAllowedForProfile(profile())) {
    std::unique_ptr<LauncherAppUpdater> arc_app_updater(
        new LauncherArcAppUpdater(this, profile()));
    app_updaters_.push_back(std::move(arc_app_updater));
  }

  if (crostini::IsCrostiniUIAllowedForProfile(profile())) {
    std::unique_ptr<LauncherAppUpdater> crostini_app_updater(
        new LauncherCrostiniAppUpdater(this, profile()));
    app_updaters_.push_back(std::move(crostini_app_updater));
  }

  app_list::AppListSyncableService* app_service =
      app_list::AppListSyncableServiceFactory::GetForProfile(profile());
  if (app_service)
    app_service->AddObserverAndStart(this);

  PrefServiceSyncableFromProfile(profile())->AddObserver(this);
}

void ChromeLauncherController::ReleaseProfile() {
  if (app_sync_ui_state_)
    app_sync_ui_state_->RemoveObserver(this);

  app_updaters_.clear();

  pref_change_registrar_.RemoveAll();

  app_list::AppListSyncableService* app_service =
      app_list::AppListSyncableServiceFactory::GetForProfile(profile());
  if (app_service)
    app_service->RemoveObserver(this);

  PrefServiceSyncableFromProfile(profile())->RemoveObserver(this);
}

///////////////////////////////////////////////////////////////////////////////
// ash::mojom::ShelfObserver:

void ChromeLauncherController::OnShelfItemAdded(int32_t index,
                                                const ash::ShelfItem& item) {
  DCHECK(shelf_controller_) << " Unexpected model sync";
  DCHECK(!applying_remote_shelf_model_changes_) << " Unexpected model change";

  // Ignore the back button; it should already exist in the local ShelfModel.
  if (item.id.app_id == ash::kBackButtonId) {
    DCHECK_EQ(0, model_->ItemIndexByID(item.id));
    return;
  }

  // Ignore the AppList item; it should already exist in the local ShelfModel.
  if (item.id.app_id == ash::kAppListId) {
    DCHECK_EQ(1, model_->ItemIndexByID(item.id));
    return;
  }

  // Ash items should be sent without images for efficiency.
  DCHECK(item.image.isNull()) << " Chrome does not need item images from Ash";
  DCHECK_LE(index, model_->item_count()) << " Index out of bounds";
  DCHECK_GT(index, 1) << " Items can not preceed the AppList";
  index = std::min(std::max(index, 1), model_->item_count());
  base::AutoReset<bool> reset(&applying_remote_shelf_model_changes_, true);
  model_->AddAt(index, item);
}

void ChromeLauncherController::OnShelfItemRemoved(const ash::ShelfID& id) {
  DCHECK(shelf_controller_) << " Unexpected model sync";
  DCHECK(!applying_remote_shelf_model_changes_) << " Unexpected model change";
  const int index = model_->ItemIndexByID(id);
  DCHECK_GE(index, 0) << " No item found with the id: " << id;
  DCHECK_NE(index, 0) << " The AppList shelf item cannot be removed";
  if (index <= 0)
    return;
  base::AutoReset<bool> reset(&applying_remote_shelf_model_changes_, true);
  model_->RemoveItemAt(index);
}

void ChromeLauncherController::OnShelfItemMoved(const ash::ShelfID& id,
                                                int32_t index) {
  DCHECK(shelf_controller_) << " Unexpected model sync";
  DCHECK(!applying_remote_shelf_model_changes_) << " Unexpected model change";
  const int current_index = model_->ItemIndexByID(id);
  DCHECK_GE(current_index, 0) << " No item found with the id: " << id;
  DCHECK_NE(current_index, 0) << " The AppList shelf item cannot be moved";
  if (current_index <= 0)
    return;
  DCHECK_GT(index, 0) << " Items can not preceed the AppList";
  DCHECK_LT(index, model_->item_count()) << " Index out of bounds";
  index = std::min(std::max(index, 1), model_->item_count() - 1);
  DCHECK_NE(current_index, index) << " The item is already at the given index";
  if (current_index == index)
    return;
  base::AutoReset<bool> reset(&applying_remote_shelf_model_changes_, true);
  model_->Move(current_index, index);
}

void ChromeLauncherController::OnShelfItemUpdated(const ash::ShelfItem& item) {
  DCHECK(shelf_controller_) << " Unexpected model sync";
  DCHECK(!applying_remote_shelf_model_changes_) << " Unexpected model change";
  const int index = model_->ItemIndexByID(item.id);
  DCHECK_GE(index, 0) << " No item found with the id: " << item.id;
  if (index < 0)
    return;
  base::AutoReset<bool> reset(&applying_remote_shelf_model_changes_, true);

  // Keep existing images, Ash items should be sent without them for efficiency.
  DCHECK(item.image.isNull()) << " Chrome does not need item images from Ash";
  ash::ShelfItem new_item = item;
  new_item.image = model_->items()[index].image;
  model_->Set(index, new_item);
}

void ChromeLauncherController::OnShelfItemDelegateChanged(
    const ash::ShelfID& id,
    ash::mojom::ShelfItemDelegatePtr delegate) {
  DCHECK(shelf_controller_) << " Unexpected model sync";
  DCHECK(!applying_remote_shelf_model_changes_) << " Unexpected model change";
  base::AutoReset<bool> reset(&applying_remote_shelf_model_changes_, true);
  if (delegate.is_bound()) {
    model_->SetShelfItemDelegate(id,
                                 std::make_unique<ash::RemoteShelfItemDelegate>(
                                     id, std::move(delegate)));
  } else {
    model_->SetShelfItemDelegate(id, nullptr);
  }
}

///////////////////////////////////////////////////////////////////////////////
// ash::ShelfModelObserver:

void ChromeLauncherController::ShelfItemAdded(int index) {
  if (shelf_controller_ && !applying_remote_shelf_model_changes_)
    shelf_controller_->AddShelfItem(index, model_->items()[index]);

  // Perform item init, and ensure these changes are reported to Ash.
  base::AutoReset<bool> reset(&applying_remote_shelf_model_changes_, false);

  ash::ShelfItem item = model_->items()[index];
  // Construct a ShelfItemDelegate for the item if one does not yet exist.
  // The delegate needs to be set before FetchImage() so that shelf item
  // icon could be set properly when FetchImage() calls OnAppImageUpdated()
  // synchronously.
  if (!model_->GetShelfItemDelegate(item.id)) {
    model_->SetShelfItemDelegate(
        item.id, AppShortcutLauncherItemController::Create(item.id));
  }

  // Fetch the app icon, this may synchronously update the item's image.
  const std::string& app_id = model_->items()[index].id.app_id;
  AppIconLoader* app_icon_loader = GetAppIconLoaderForApp(app_id);
  if (app_icon_loader)
    app_icon_loader->FetchImage(app_id);

  // Update the item with any other missing Chrome-specific info.
  if (item.type == ash::TYPE_APP || item.type == ash::TYPE_PINNED_APP) {
    bool needs_update = false;
    if (item.title.empty()) {
      needs_update = true;
      item.title = LauncherControllerHelper::GetAppTitle(profile(), app_id);
    }
    ash::ShelfItemStatus status = GetAppState(app_id);
    if (status != item.status && status != ash::STATUS_CLOSED) {
      needs_update = true;
      item.status = status;
    }
    if (needs_update) {
      // Ensure these changes are reported back to Ash.
      base::AutoReset<bool> reset(&applying_remote_shelf_model_changes_, false);
      model_->Set(index, item);
    }
  }

  // Update the pin position preference as needed.
  if (ItemTypeIsPinned(item) && should_sync_pin_changes_)
    SyncPinPosition(item.id);
}

void ChromeLauncherController::ShelfItemRemoved(
    int index,
    const ash::ShelfItem& old_item) {
  if (shelf_controller_ && !applying_remote_shelf_model_changes_)
    shelf_controller_->RemoveShelfItem(old_item.id);

  // Remove the pin position from preferences as needed.
  if (ItemTypeIsPinned(old_item) && should_sync_pin_changes_)
    RemovePinPosition(profile(), old_item.id);

  AppIconLoader* app_icon_loader = GetAppIconLoaderForApp(old_item.id.app_id);
  if (app_icon_loader)
    app_icon_loader->ClearImage(old_item.id.app_id);
}

void ChromeLauncherController::ShelfItemMoved(int start_index,
                                              int target_index) {
  const ash::ShelfItem& item = model_->items()[target_index];
  if (shelf_controller_ && !applying_remote_shelf_model_changes_)
    shelf_controller_->MoveShelfItem(item.id, target_index);

  // Update the pin position preference as needed.
  DCHECK_NE(ash::TYPE_BACK_BUTTON, item.type);
  DCHECK_NE(ash::TYPE_APP_LIST, item.type);
  if (ItemTypeIsPinned(item) && should_sync_pin_changes_)
    SyncPinPosition(item.id);
}

void ChromeLauncherController::ShelfItemChanged(
    int index,
    const ash::ShelfItem& old_item) {
  ash::ShelfItem item = model_->items()[index];
  if (shelf_controller_ && !applying_remote_shelf_model_changes_) {
    // Avoid passing item images here, ash will retain its existing local image.
    // Images are synced elsewhere to save costs on these updates (eg. status).
    item.image = gfx::ImageSkia();
    shelf_controller_->UpdateShelfItem(item);
  }

  if (!should_sync_pin_changes_)
    return;

  // Add or remove the pin position from preferences as needed.
  if (!ItemTypeIsPinned(old_item) && ItemTypeIsPinned(item))
    SyncPinPosition(item.id);
  else if (ItemTypeIsPinned(old_item) && !ItemTypeIsPinned(item))
    RemovePinPosition(profile(), old_item.id);
}

void ChromeLauncherController::ShelfItemDelegateChanged(
    const ash::ShelfID& id,
    ash::ShelfItemDelegate* old_delegate,
    ash::ShelfItemDelegate* delegate) {
  if (shelf_controller_ && !applying_remote_shelf_model_changes_) {
    shelf_controller_->SetShelfItemDelegate(
        id, delegate ? delegate->CreateInterfacePtrAndBind()
                     : ash::mojom::ShelfItemDelegatePtr());
  }
}

///////////////////////////////////////////////////////////////////////////////
// AppSyncUIStateObserver:

void ChromeLauncherController::OnAppSyncUIStatusChanged() {
  // Update the app list button title to reflect the syncing status.
  base::string16 title = l10n_util::GetStringUTF16(
      app_sync_ui_state_->status() == AppSyncUIState::STATUS_SYNCING
          ? IDS_ASH_SHELF_APP_LIST_LAUNCHER_SYNCING_TITLE
          : IDS_ASH_SHELF_APP_LIST_LAUNCHER_TITLE);

  const int app_list_index = model_->GetItemIndexForType(ash::TYPE_APP_LIST);
  DCHECK_GE(app_list_index, 0);
  ash::ShelfItem item = model_->items()[app_list_index];
  if (item.title != title) {
    item.title = title;
    model_->Set(app_list_index, item);
  }
}
