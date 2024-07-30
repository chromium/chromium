// Copyright 2013 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_ASH_SHELF_CHROME_SHELF_CONTROLLER_H_
#define CHROME_BROWSER_UI_ASH_SHELF_CHROME_SHELF_CONTROLLER_H_

#include <map>
#include <memory>
#include <string>
#include <vector>

#include "ash/public/cpp/shelf_item_delegate.h"
#include "ash/public/cpp/shelf_model_observer.h"
#include "ash/public/cpp/shelf_types.h"
#include "base/auto_reset.h"
#include "base/gtest_prod_util.h"
#include "base/memory/raw_ptr.h"
#include "base/memory/scoped_refptr.h"
#include "base/memory/weak_ptr.h"
#include "base/scoped_observation.h"
#include "base/task/sequenced_task_runner.h"
#include "chrome/browser/ash/app_list/app_list_syncable_service.h"
#include "chrome/browser/ui/app_icon_loader_delegate.h"
#include "chrome/browser/ui/ash/shelf/settings_window_observer.h"
#include "chrome/browser/ui/ash/shelf/shelf_app_updater.h"
#include "components/account_id/account_id.h"
#include "components/prefs/pref_change_registrar.h"
#include "components/sync_preferences/pref_service_syncable_observer.h"

class AppIconLoader;
class AppServiceAppWindowShelfController;
class AppWindowShelfController;
class BrowserShortcutShelfItemController;
class BrowserStatusMonitor;
class ChromeShelfControllerUserSwitchObserver;
class ChromeShelfItemFactory;
class ChromeShelfPrefs;
class Profile;
class ShelfControllerHelper;
class ShelfSpinnerController;

namespace apps {
class PromiseAppUpdate;
}

namespace ash {
class ShelfModel;
FORWARD_DECLARE_TEST(SpokenFeedbackTest, ShelfIconFocusForward);
FORWARD_DECLARE_TEST(SpokenFeedbackTest, SpeakingTextUnderMouseForShelfItem);
}  // namespace ash

namespace content {
class WebContents;
}

namespace gfx {
class Image;
}

namespace ui {
class BaseWindow;
}

namespace sync_preferences {
class PrefServiceSyncable;
}

class BrowserAppShelfController;

// ChromeShelfController helps manage Ash's shelf for Chrome prefs and apps.
// It helps synchronize shelf state with profile preferences and app content.
class ChromeShelfController
    : public ShelfAppUpdater::Delegate,
      public AppIconLoaderDelegate,
      private ash::ShelfModelObserver,
      private app_list::AppListSyncableService::Observer,
      private sync_preferences::PrefServiceSyncableObserver {
 public:
  // The value used for indicating that an index position doesn't exist.
  static const int kInvalidIndex = -1;

  // Returns the single ChromeShelfController instance.
  static ChromeShelfController* instance();

  ChromeShelfController(Profile* profile, ash::ShelfModel* model);

  ChromeShelfController(const ChromeShelfController&) = delete;
  ChromeShelfController& operator=(const ChromeShelfController&) = delete;

  ~ChromeShelfController() override;

  Profile* profile() const { return profile_; }
  ash::ShelfModel* shelf_model() const { return model_; }

  AppServiceAppWindowShelfController* app_service_app_window_controller() {
    return app_service_app_window_controller_;
  }

  // Initializes this ChromeShelfController.
  void Init();

  // Creates a new app item on the shelf for |item_delegate|.
  ash::ShelfID CreateAppItem(
      std::unique_ptr<ash::ShelfItemDelegate> item_delegate,
      ash::ShelfItemStatus status,
      bool pinned,
      const std::u16string& title = std::u16string());

  // Returns the shelf item with the given id, or null if |id| isn't found.
  const ash::ShelfItem* GetItem(const ash::ShelfID& id) const;

  // Updates the type of an item.
  void SetItemType(const ash::ShelfID& id, ash::ShelfItemType type);

  // Updates the running status of an item. It will also update the status of
  // browsers shelf item if needed.
  void SetItemStatus(const ash::ShelfID& id, ash::ShelfItemStatus status);

  // Updates the shelf item title (displayed in the tooltip).
  void SetItemTitle(const ash::ShelfID& id, const std::u16string& title);

  // If the shelf-item is pinned, its state is set to CLOSED and its delegate is
  // replaced with an AppShortcutShelfItemController.
  // If the shelf-item is unpinned, then it's removed from the shelf.
  void ReplaceWithAppShortcutOrRemove(const ash::ShelfID& id);

  // Returns true if the item identified by |id| is pinned.
  bool IsPinned(const ash::ShelfID& id) const;

  // This method is only used by BrowserStatusMonitor and tests. This method
  // relies on implicit assumptions and is likely unsuitable for other use
  // cases.
  //
  // Set the shelf item status for the application with the given |app_id|.
  // Adds or removes an item as needed to respect the running and pinned state.
  // If a new item is added, the AppShortcutShelfItemController delegate is
  // used.
  void SetAppStatus(const std::string& app_id, ash::ShelfItemStatus status);

  // Closes the specified item.
  void Close(const ash::ShelfID& id);

  // Returns true if the specified item is open.
  bool IsOpen(const ash::ShelfID& id) const;

  // Opens a new instance of the application identified by the ShelfID.
  // Used by the app-list, and by pinned-app shelf items. |display_id| is id of
  // the display from which the app is launched.
  void LaunchApp(const ash::ShelfID& id,
                 ash::ShelfLaunchSource source,
                 int event_flags,
                 int64_t display_id,
                 bool new_window = false);

  // Set the image for a specific shelf item (e.g. when set by the app).
  void SetItemImage(const ash::ShelfID& shelf_id, const gfx::ImageSkia& image);

  // Updates the image for a specific shelf item from the app's icon loader.
  void UpdateItemImage(const std::string& app_id);

  // Notifies the controller that |contents| changed so it can update the state
  // of v1 (non-packaged) apps in the shelf. If |remove| is true then it removes
  // the association of |contents| with an app.
  void UpdateAppState(content::WebContents* contents, bool remove);

  // Updates app state for all tabs where a specific v1 app is running.
  // This call is necessary if an app has been created for an existing
  // web page (see IDC_CREATE_SHORTCUT).
  void UpdateV1AppState(const std::string& app_id);

  // Activates a |window|. If |allow_minimize| is true and the system allows
  // it, the the window will get minimized instead.
  // Returns the action performed. Should be one of SHELF_ACTION_NONE,
  // SHELF_ACTION_WINDOW_ACTIVATED, or SHELF_ACTION_WINDOW_MINIMIZED.
  ash::ShelfAction ActivateWindowOrMinimizeIfActive(ui::BaseWindow* window,
                                                    bool allow_minimize);

  // Called when the active user has changed.
  void ActiveUserChanged(const AccountId& account_id);

  // Called when a user got added to the session.
  void AdditionalUserAddedToSession(Profile* profile);

  // Get the list of all running incarnations of this item.
  ash::ShelfItemDelegate::AppMenuItems GetAppMenuItemsForTesting(
      const ash::ShelfItem& item);

  // Get the list of all ARC app windows.
  std::vector<aura::Window*> GetArcWindows();

  // Checks if a given |web_contents| is known to be associated with an
  // application of type |app_id|.
  bool IsWebContentHandledByApplication(content::WebContents* web_contents,
                                        const std::string& app_id);

  // Get the favicon for the application menu entry for |web_contents|.
  // Returns the incognito icon if |web_contents| has an incognito profile.
  // Returns the default favicon if |web_contents| is null or has not loaded.
  gfx::Image GetAppMenuIcon(content::WebContents* web_contents) const;

  // Get the title for the application menu entry for |web_contents|.
  // Returns "New Tab" if |web_contents| is null or has not loaded.
  std::u16string GetAppMenuTitle(content::WebContents* web_contents) const;

  // Returns the ash::ShelfItemDelegate of BrowserShortcut.
  BrowserShortcutShelfItemController*
  GetBrowserShortcutShelfItemControllerForTesting();

  // Updates the browser shortcut item state.
  // This may create or delete the item, specifically if the browser icon
  // is not pinned. Practically, when Lacros is the primary browser.
  void UpdateBrowserItemState();

  // Sets the shelf id for the browser window if the browser is represented.
  void SetShelfIDForBrowserWindowContents(Browser* browser,
                                          content::WebContents* web_contents);

  // Called when the user profile is fully loaded and ready to switch to.
  void OnUserProfileReadyToSwitch(Profile* profile);

  // Controller to launch ARC and Crostini apps with a spinner.
  ShelfSpinnerController* GetShelfSpinnerController();

  // Temporarily prevent pinned shelf item changes from updating the sync model.
  using ScopedPinSyncDisabler = std::unique_ptr<base::AutoReset<bool>>;
  ScopedPinSyncDisabler GetScopedPinSyncDisabler();

  // Sets ShelfControllerHelper or AppIconLoader for test, taking ownership.
  void SetShelfControllerHelperForTest(
      std::unique_ptr<ShelfControllerHelper> helper);
  void SetAppIconLoadersForTest(
      std::vector<std::unique_ptr<AppIconLoader>>& loaders);

  void SetProfileForTest(Profile* profile);

  // Helpers that call through to corresponding ShelfModel functions.
  bool AllowedToSetAppPinState(const std::string& app_id,
                               bool target_pin) const;
  bool IsAppPinned(const std::string& app_id);
  void UnpinAppWithID(const std::string& app_id);

  // This method is only used by ApkWebAppService and tests. This method
  // relies on implicit assumptions and is likely unsuitable for other use
  // cases.
  //
  // Unpins app item with |old_app_id| and pins app |new_app_id| in its place.
  void ReplacePinnedItem(const std::string& old_app_id,
                         const std::string& new_app_id);

  // Pins app with |app_id| at |target_index| if it is not already pinned.
  void PinAppAtIndex(const std::string& app_id, int target_index);

  // Converts |app_id| to shelf_id and calls ShelfModel function ItemIndexbyID
  // to get index of item with id |app_id| or -1 if it's not pinned.
  int PinnedItemIndexByAppID(const std::string& app_id);

  // Whether the controller supports a Show App Info flow for a specific
  // extension.
  bool CanDoShowAppInfoFlow(const std::string& extension_id);

  // Show the dialog with the application's information. Call only if
  // CanDoShowAppInfoFlow() returns true.
  void DoShowAppInfoFlow(const std::string& app_id);

  // ShelfAppUpdater::Delegate:
  void OnAppInstalled(content::BrowserContext* browser_context,
                      const std::string& app_id) override;
  void OnAppUpdated(content::BrowserContext* browser_context,
                    const std::string& app_id,
                    bool reload_icon) override;
  void OnAppShowInShelfChanged(content::BrowserContext* browser_context,
                               const std::string& app_id,
                               bool show_in_shelf) override;
  void OnAppUninstalledPrepared(content::BrowserContext* browser_context,
                                const std::string& app_id,
                                bool by_migration) override;
  void OnPromiseAppUpdate(const apps::PromiseAppUpdate& update) override;
  void OnPromiseAppRemoved(const apps::PackageId& package_id) override;

  // AppIconLoaderDelegate:
  void OnAppImageUpdated(
      const std::string& app_id,
      const gfx::ImageSkia& image,
      bool is_placeholder_icon,
      const std::optional<gfx::ImageSkia>& badge_image) override;

  // Inserts a shelf item for an app at |index|. Note that |index| may be
  // adjusted by the model to meet ordering constraints.
  ash::ShelfID InsertAppItem(
      std::unique_ptr<ash::ShelfItem> item,
      std::unique_ptr<ash::ShelfItemDelegate> item_delegate,
      int index);

 private:
  friend class ChromeShelfControllerTestBase;
  friend class ShelfAppBrowserTest;
  friend class ShelfPlatformAppBrowserTest;
  friend class TestChromeShelfController;

  FRIEND_TEST_ALL_PREFIXES(ash::SpokenFeedbackTest, ShelfIconFocusForward);
  FRIEND_TEST_ALL_PREFIXES(ash::SpokenFeedbackTest,
                           SpeakingTextUnderMouseForShelfItem);

  using WebContentsToAppIDMap = std::map<content::WebContents*, std::string>;

  // Updates images of shelf items representing the app.
  void UpdateAppImage(const std::string& app_id,
                      const std::optional<gfx::ImageSkia>& badge_image,
                      bool is_placeholder_icon,
                      const gfx::ImageSkia& image);

  // Remembers / restores list of running applications.
  // Note that this order will neither be stored in the preference nor will it
  // remember the order of closed applications since it is only temporary.
  void RememberUnpinnedRunningApplicationOrder();
  void RestoreUnpinnedRunningApplicationOrder(const std::string& user_id);

  // Invoked when the associated browser or app is closed.
  void RemoveShelfItem(const ash::ShelfID& id);

  // Pin a running app with |shelf_id| internally to |index|.
  void PinRunningAppInternal(int index, const ash::ShelfID& shelf_id);

  // Unpin a locked application. This is an internal call which converts the
  // model type of the given app index from a shortcut into an unpinned running
  // app.
  void UnpinRunningAppInternal(int index);

  // Updates pin position for the item specified by |id| in sync model.
  void SyncPinPosition(const ash::ShelfID& id);

  // Re-syncs shelf model.
  void UpdatePinnedAppsFromSync();

  // Makes sure that the app with `app_id` is pinned at `target_index` within
  // shelf model if possible. `current_index` is the current app index in the
  // shelf model.
  // If the app is not in the model, the value should be -1.
  // Returns whether the app got pinned - for example, this may fail for chrome
  // app if it's not present in the shelf, or if creating a shelf item for the
  // app fails.
  bool EnsureAppPinnedInModelAtIndex(const std::string& app_id,
                                     int current_index,
                                     int target_index);

  // Schedules re-sync of shelf model.
  void ScheduleUpdatePinnedAppsFromSync();

  // Updates the policy-pinned and the forced-pin-state flag for each shelf
  // item.
  void UpdateAppsPinStatesFromPrefs();

  // Updates the policy-pinned flag for shelf item at `model_index` in shelf
  // model.
  void UpdatePinnedByPolicyForItemAtIndex(int model_index);

  // Updates the pin_state_forced_by_type flag for shelf item at `model_index`
  // in shelf model.
  void UpdateForcedPinStateForItemAtIndex(int model_index);

  // Returns the shelf item status for the given |app_id|, which can be either
  // STATUS_RUNNING (if there is such an app) or STATUS_CLOSED.
  ash::ShelfItemStatus GetAppState(const std::string& app_id);

  // Create the Chrome browser shortcut ShelfItem.
  void CreateBrowserShortcutItem(bool pinned);

  // Close all windowed V1 applications of a certain extension which was already
  // deleted.
  void CloseWindowedAppsFromRemovedExtension(const std::string& app_id,
                                             const Profile* profile);

  // Add the app updater and the app icon loder for a specific profile.
  void AddAppUpdaterAndIconLoader(Profile* profile);

  // Attach to a specific profile.
  void AttachProfile(Profile* profile_to_attach);

  // Forget the current profile to allow attaching to a new one.
  void ReleaseProfile();

  // ash::ShelfModelObserver:
  void ShelfItemAdded(int index) override;
  void ShelfItemRemoved(int index, const ash::ShelfItem& old_item) override;
  void ShelfItemMoved(int start_index, int target_index) override;
  void ShelfItemChanged(int index, const ash::ShelfItem& old_item) override;

  // app_list::AppListSyncableService::Observer:
  void OnSyncModelUpdated() override;

  // sync_preferences::PrefServiceSyncableObserver:
  void OnIsSyncingChanged() override;

  // Initializes local shelf prefs if OS prefs started syncing (which implies
  // that initial synced prefs values have been set).
  // Shelf prefs are tracked both as local and synced prefs. Synced pref is used
  // only to initialize local prefs when the user logs in for the first time.
  void InitLocalShelfPrefsIfOsPrefsAreSyncing();

  // An internal helper to unpin a shelf item; this does not update app sync.
  void UnpinShelfItemInternal(const ash::ShelfID& id);

  // Updates the running status of an item, or removes it if necessary.
  void SetItemStatusOrRemove(const ash::ShelfID& id,
                             ash::ShelfItemStatus status);

  // Returns whether the pin position of the item should be synced, taking into
  // account re-entrancy limitations.
  bool ShouldSyncItemWithReentrancy(const ash::ShelfItem& item);

  // Returns whether the pin position of the item should be synced, ignoring
  // re-entrancy limitations.
  bool ShouldSyncItem(const ash::ShelfItem& item);

  // Resolves the app icon image loader for the app.
  AppIconLoader* GetAppIconLoaderForApp(const std::string& app_id);

  // The currently loaded profile used for prefs and loading extensions. This is
  // NOT necessarily the profile new windows are created with. Note that in
  // multi-profile use cases this might change over time.
  raw_ptr<Profile> profile_ = nullptr;

  // The profile used to load icons and get the app update information. This is
  // the latest active user's profile when switch users in multi-profile use
  // cases.
  raw_ptr<Profile> latest_active_profile_ = nullptr;

  // The ShelfModel instance owned by ash::Shell's ShelfController.
  const raw_ptr<ash::ShelfModel> model_;

  // The central authority to create ShelfItems from app_ids.
  std::unique_ptr<ChromeShelfItemFactory> shelf_item_factory_;

  // The AppService app window shelf controller.
  raw_ptr<AppServiceAppWindowShelfController>
      app_service_app_window_controller_ = nullptr;

  // When true, changes to pinned shelf items should update the sync model.
  bool should_sync_pin_changes_ = true;

  // Used to get app info for tabs.
  std::unique_ptr<ShelfControllerHelper> shelf_controller_helper_;

  // TODO(crbug.com/40573204): Remove this once SystemWebApps are enabled by
  // default.
  // An observer that manages the shelf title and icon for settings windows.
  std::unique_ptr<SettingsWindowObserver> settings_window_observer_;

  // Used to load the images for app items.
  std::map<Profile*, std::vector<std::unique_ptr<AppIconLoader>>>
      app_icon_loaders_;

  // Direct access to app_id for a web contents.
  // NOTE: This tracks all WebContents, not just those associated with an app.
  WebContentsToAppIDMap web_contents_to_app_id_;

  // Used to track app windows.
  std::vector<std::unique_ptr<AppWindowShelfController>>
      app_window_controllers_;

  // Used to handle app load/unload events.
  std::map<Profile*, std::vector<std::unique_ptr<ShelfAppUpdater>>>
      app_updaters_;

  PrefChangeRegistrar pref_change_registrar_;

  // The owned browser status monitor.
  std::unique_ptr<BrowserStatusMonitor> browser_status_monitor_;

  // A special observer class to detect user switches.
  std::unique_ptr<ChromeShelfControllerUserSwitchObserver>
      user_switch_observer_;

  std::unique_ptr<ShelfSpinnerController> shelf_spinner_controller_;

  // Responsible for bridging between the shelf and sync/prefs.
  std::unique_ptr<ChromeShelfPrefs> shelf_prefs_;

  // Manages shelf item for browser-based apps and Lacros.
  std::unique_ptr<BrowserAppShelfController> browser_app_shelf_controller_;

  // The list of running & un-pinned applications for different users on hidden
  // desktops.
  using RunningAppListIds = std::vector<std::string>;
  using RunningAppListIdMap = std::map<std::string, RunningAppListIds>;
  RunningAppListIdMap last_used_running_application_order_;

  base::ScopedObservation<app_list::AppListSyncableService,
                          app_list::AppListSyncableService::Observer>
      app_list_syncable_service_observer_{this};

  base::ScopedObservation<sync_preferences::PrefServiceSyncable,
                          sync_preferences::PrefServiceSyncableObserver>
      pref_service_syncable_observer_{this};

  // A sequenced task runner to create standard icons and not spamming the
  // thread pool.
  scoped_refptr<base::SequencedTaskRunner> standard_icon_task_runner_;

  base::WeakPtrFactory<ChromeShelfController> weak_ptr_factory_{this};
};

#endif  // CHROME_BROWSER_UI_ASH_SHELF_CHROME_SHELF_CONTROLLER_H_
