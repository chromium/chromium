// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_APP_LIST_APP_LIST_CLIENT_IMPL_H_
#define CHROME_BROWSER_UI_APP_LIST_APP_LIST_CLIENT_IMPL_H_

#include <stdint.h>

#include <map>
#include <memory>
#include <string>
#include <utility>
#include <vector>

#include "ash/public/cpp/app_list/app_list_client.h"
#include "ash/public/cpp/shelf_types.h"
#include "base/callback_forward.h"
#include "base/compiler_specific.h"
#include "base/macros.h"
#include "base/memory/weak_ptr.h"
#include "base/scoped_observer.h"
#include "chrome/browser/ui/app_list/app_list_controller_delegate.h"
#include "components/search_engines/template_url_service.h"
#include "components/search_engines/template_url_service_observer.h"
#include "components/user_manager/user_manager.h"
#include "ui/display/types/display_constants.h"

namespace app_list {
class SearchController;
class SearchResourceManager;
}  // namespace app_list

class AppListClientWithProfileTest;
class AppListModelUpdater;
class AppSyncUIStateWatcher;
class Profile;

class AppListClientImpl
    : public ash::AppListClient,
      public AppListControllerDelegate,
      public user_manager::UserManager::UserSessionStateObserver,
      public TemplateURLServiceObserver {
 public:
  AppListClientImpl();
  ~AppListClientImpl() override;

  static AppListClientImpl* GetInstance();

  // ash::AppListClient:
  void OnAppListControllerDestroyed() override;
  void StartSearch(const base::string16& trimmed_query) override;
  void OpenSearchResult(const std::string& result_id,
                        int event_flags,
                        ash::AppListLaunchedFrom launched_from,
                        ash::AppListLaunchType launch_type,
                        int suggestion_index,
                        bool launch_as_default) override;
  void InvokeSearchResultAction(const std::string& result_id,
                                int action_index,
                                int event_flags) override;
  void GetSearchResultContextMenuModel(
      const std::string& result_id,
      GetContextMenuModelCallback callback) override;
  void ViewClosing() override;
  void ViewShown(int64_t display_id) override;
  void ActivateItem(int profile_id,
                    const std::string& id,
                    int event_flags) override;
  void GetContextMenuModel(int profile_id,
                           const std::string& id,
                           GetContextMenuModelCallback callback) override;
  void OnAppListVisibilityWillChange(bool visible) override;
  void OnAppListVisibilityChanged(bool visible) override;
  void OnFolderCreated(int profile_id,
                       std::unique_ptr<ash::AppListItemMetadata> item) override;
  void OnFolderDeleted(int profile_id,
                       std::unique_ptr<ash::AppListItemMetadata> item) override;
  void OnItemUpdated(int profile_id,
                     std::unique_ptr<ash::AppListItemMetadata> item) override;
  void OnPageBreakItemAdded(int profile_id,
                            const std::string& id,
                            const syncer::StringOrdinal& position) override;
  void OnPageBreakItemDeleted(int profile_id, const std::string& id) override;
  void GetNavigableContentsFactory(
      mojo::PendingReceiver<content::mojom::NavigableContentsFactory> receiver)
      override;
  void OnSearchResultVisibilityChanged(const std::string& id,
                                       bool visible) override;
  void NotifySearchResultsForLogging(
      const base::string16& trimmed_query,
      const ash::SearchResultIdWithPositionIndices& results,
      int position_index) override;

  // user_manager::UserManager::UserSessionStateObserver:
  void ActiveUserChanged(user_manager::User* active_user) override;

  // AppListControllerDelegate overrides:
  void DismissView() override;
  aura::Window* GetAppListWindow() override;
  int64_t GetAppListDisplayId() override;
  void GetAppInfoDialogBounds(GetAppInfoDialogBoundsCallback callback) override;
  bool IsAppPinned(const std::string& app_id) override;
  bool IsAppOpen(const std::string& app_id) const override;
  void PinApp(const std::string& app_id) override;
  void UnpinApp(const std::string& app_id) override;
  Pinnable GetPinnable(const std::string& app_id) override;
  void CreateNewWindow(Profile* profile, bool incognito) override;
  void OpenURL(Profile* profile,
               const GURL& url,
               ui::PageTransition transition,
               WindowOpenDisposition disposition) override;
  void ActivateApp(Profile* profile,
                   const extensions::Extension* extension,
                   AppListSource source,
                   int event_flags) override;
  void LaunchApp(Profile* profile,
                 const extensions::Extension* extension,
                 AppListSource source,
                 int event_flags,
                 int64_t display_id) override;

  // Associates this client with the current active user, called when this
  // client is accessed or active user is changed.
  void UpdateProfile();

  void ShowAppList();

  bool app_list_target_visibility() const {
    return app_list_target_visibility_;
  }
  bool app_list_visible() const { return app_list_visible_; }

  // Returns a pointer to control the app list views in ash.
  ash::AppListController* GetAppListController() const;

  AppListControllerDelegate* GetControllerDelegate();
  Profile* GetCurrentAppListProfile() const;

  app_list::SearchController* search_controller();

  AppListModelUpdater* GetModelUpdaterForTest();

 private:
  FRIEND_TEST_ALL_PREFIXES(AppListClientWithProfileTest, CheckDataRace);

  // Overridden from TemplateURLServiceObserver:
  void OnTemplateURLServiceChanged() override;

  // Configures the AppList for the given |profile|.
  void SetProfile(Profile* profile);

  // Updates the speech webview and start page for the current |profile_|.
  void SetUpSearchUI();

  ash::ShelfLaunchSource AppListSourceToLaunchSource(AppListSource source);

  // The current display id showing the app list.
  int64_t display_id_ = display::kInvalidDisplayId;

  // Unowned pointer to the associated profile. May change if SetProfile is
  // called.
  Profile* profile_ = nullptr;

  // Unowned pointer to the model updater owned by AppListSyncableService. Will
  // change if |profile_| changes.
  AppListModelUpdater* current_model_updater_ = nullptr;

  // Store the mappings between profiles and AppListModelUpdater instances.
  // In multi-profile mode, mojo callings from the Ash process to access the app
  // list items should be dealt with the correct AppListModelUpdater instance.
  // Otherwise data race may happen, like the issue 939755
  // (https://crbug.com/939755).
  // TODO: Replace the mojo interface functions provided by AppListClient with
  // callbacks.
  std::map<int, AppListModelUpdater*> profile_model_mappings_;

  std::unique_ptr<app_list::SearchResourceManager> search_resource_manager_;
  std::unique_ptr<app_list::SearchController> search_controller_;
  std::unique_ptr<AppSyncUIStateWatcher> app_sync_ui_state_watcher_;

  ScopedObserver<TemplateURLService, TemplateURLServiceObserver>
      template_url_service_observer_{this};

  ash::AppListController* app_list_controller_ = nullptr;

  bool app_list_target_visibility_ = false;
  bool app_list_visible_ = false;

  base::WeakPtrFactory<AppListClientImpl> weak_ptr_factory_{this};

  DISALLOW_COPY_AND_ASSIGN(AppListClientImpl);
};

#endif  // CHROME_BROWSER_UI_APP_LIST_APP_LIST_CLIENT_IMPL_H_
