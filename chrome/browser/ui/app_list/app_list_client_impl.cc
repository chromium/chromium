// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/app_list/app_list_client_impl.h"

#include <stddef.h>

#include <utility>
#include <vector>

#include "ash/public/cpp/app_list/app_list_controller.h"
#include "ash/public/cpp/new_window_delegate.h"
#include "ash/public/cpp/tablet_mode.h"
#include "base/bind.h"
#include "base/metrics/histogram_functions.h"
#include "base/metrics/histogram_macros.h"
#include "base/metrics/user_metrics.h"
#include "base/strings/strcat.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/profiles/profile_manager.h"
#include "chrome/browser/search_engines/template_url_service_factory.h"
#include "chrome/browser/ui/app_list/app_list_controller_delegate.h"
#include "chrome/browser/ui/app_list/app_list_model_updater.h"
#include "chrome/browser/ui/app_list/app_list_notifier_impl.h"
#include "chrome/browser/ui/app_list/app_list_syncable_service.h"
#include "chrome/browser/ui/app_list/app_list_syncable_service_factory.h"
#include "chrome/browser/ui/app_list/app_sync_ui_state_watcher.h"
#include "chrome/browser/ui/app_list/search/app_result.h"
#include "chrome/browser/ui/app_list/search/chrome_search_result.h"
#include "chrome/browser/ui/app_list/search/cros_action_history/cros_action_recorder.h"
#include "chrome/browser/ui/app_list/search/search_controller.h"
#include "chrome/browser/ui/app_list/search/search_controller_factory.h"
#include "chrome/browser/ui/app_list/search/search_result_ranker/app_launch_data.h"
#include "chrome/browser/ui/app_list/search/search_result_ranker/ranking_item_util.h"
#include "chrome/browser/ui/ash/launcher/chrome_launcher_controller.h"
#include "chrome/browser/ui/ash/launcher/chrome_launcher_controller_util.h"
#include "chrome/browser/ui/browser_commands.h"
#include "chrome/browser/ui/browser_navigator.h"
#include "chrome/browser/ui/browser_navigator_params.h"
#include "extensions/common/extension.h"
#include "ui/display/display.h"
#include "ui/display/screen.h"
#include "ui/display/types/display_constants.h"
#include "ui/gfx/geometry/rect.h"

namespace {

AppListClientImpl* g_app_list_client_instance = nullptr;

bool IsTabletMode() {
  return ash::TabletMode::Get() && ash::TabletMode::Get()->InTabletMode();
}

}  // namespace

AppListClientImpl::AppListClientImpl()
    : app_list_controller_(ash::AppListController::Get()),
      app_list_notifier_(
          std::make_unique<AppListNotifierImpl>(app_list_controller_)) {
  app_list_controller_->SetClient(this);
  user_manager::UserManager::Get()->AddSessionStateObserver(this);

  DCHECK(!g_app_list_client_instance);
  g_app_list_client_instance = this;
}

AppListClientImpl::~AppListClientImpl() {
  SetProfile(nullptr);

  user_manager::UserManager::Get()->RemoveSessionStateObserver(this);

  DCHECK_EQ(this, g_app_list_client_instance);
  g_app_list_client_instance = nullptr;

  if (app_list_controller_)
    app_list_controller_->SetClient(nullptr);
}

// static
AppListClientImpl* AppListClientImpl::GetInstance() {
  return g_app_list_client_instance;
}

void AppListClientImpl::OnAppListControllerDestroyed() {
  // |app_list_controller_| could be released earlier, e.g. starting a kiosk
  // next session.
  app_list_controller_ = nullptr;
  if (current_model_updater_)
    current_model_updater_->SetActive(false);
}

void AppListClientImpl::StartSearch(const std::u16string& trimmed_query) {
  if (search_controller_) {
    search_controller_->Start(trimmed_query);
    OnSearchStarted();
  }
}

void AppListClientImpl::OpenSearchResult(const std::string& result_id,
                                         int event_flags,
                                         ash::AppListLaunchedFrom launched_from,
                                         ash::AppListLaunchType launch_type,
                                         int suggestion_index,
                                         bool launch_as_default) {
  if (!search_controller_)
    return;

  ChromeSearchResult* result = search_controller_->FindSearchResult(result_id);
  if (!result)
    return;

  app_list::AppLaunchData app_launch_data;
  app_launch_data.id = result_id;
  app_launch_data.ranking_item_type =
      app_list::RankingItemTypeFromSearchResult(*result);
  app_launch_data.launch_type = launch_type;
  app_launch_data.launched_from = launched_from;
  app_launch_data.suggestion_index = suggestion_index;
  app_launch_data.score = result->relevance();

  if (launch_type == ash::AppListLaunchType::kAppSearchResult &&
      launched_from == ash::AppListLaunchedFrom::kLaunchedFromSearchBox &&
      app_launch_data.ranking_item_type == app_list::RankingItemType::kApp &&
      search_controller_->GetLastQueryLength() != 0) {
    ash::RecordSuccessfulAppLaunchUsingSearch(
        launched_from, search_controller_->GetLastQueryLength());
  }

  // Send training signal to search controller.
  search_controller_->Train(std::move(app_launch_data));

  RecordSearchResultOpenTypeHistogram(launched_from, result->metrics_type(),
                                      IsTabletMode());

  if (launch_as_default)
    RecordDefaultSearchResultOpenTypeHistogram(result->metrics_type());

  if (!search_controller_->GetLastQueryLength() &&
      launched_from == ash::AppListLaunchedFrom::kLaunchedFromSearchBox)
    RecordZeroStateSuggestionOpenTypeHistogram(result->metrics_type());

  // OpenResult may cause |result| to be deleted.
  search_controller_->OpenResult(result, event_flags);
}

void AppListClientImpl::InvokeSearchResultAction(const std::string& result_id,
                                                 int action_index) {
  if (!search_controller_)
    return;
  ChromeSearchResult* result = search_controller_->FindSearchResult(result_id);
  if (result)
    search_controller_->InvokeResultAction(result, action_index);
}

void AppListClientImpl::GetSearchResultContextMenuModel(
    const std::string& result_id,
    GetContextMenuModelCallback callback) {
  if (!search_controller_) {
    std::move(callback).Run(nullptr);
    return;
  }
  ChromeSearchResult* result = search_controller_->FindSearchResult(result_id);
  if (!result) {
    std::move(callback).Run(nullptr);
    return;
  }
  result->GetContextMenuModel(base::BindOnce(
      [](GetContextMenuModelCallback callback,
         std::unique_ptr<ui::SimpleMenuModel> menu_model) {
        std::move(callback).Run(std::move(menu_model));
      },
      std::move(callback)));
}

void AppListClientImpl::ViewClosing() {
  display_id_ = display::kInvalidDisplayId;
  if (search_controller_)
    search_controller_->ViewClosing();
}

void AppListClientImpl::ViewShown(int64_t display_id) {
  if (current_model_updater_) {
    base::RecordAction(base::UserMetricsAction("Launcher_Show"));
    base::UmaHistogramSparse("Apps.AppListBadgedAppsCount",
                             current_model_updater_->BadgedItemCount());
  }
  display_id_ = display_id;
}

void AppListClientImpl::ActivateItem(int profile_id,
                                     const std::string& id,
                                     int event_flags) {
  auto* requested_model_updater = profile_model_mappings_[profile_id];

  // Pointless to notify the AppListModelUpdater of the activated item if the
  // |requested_model_updater| is not the current one, which means that the
  // active profile is changed. The same rule applies to the GetContextMenuModel
  // and ContextMenuItemSelected.
  if (requested_model_updater != current_model_updater_ ||
      !requested_model_updater) {
    return;
  }

  // Send a training signal to the search controller.
  const auto* item = current_model_updater_->FindItem(id);
  if (item) {
    app_list::AppLaunchData app_launch_data;
    app_launch_data.id = id;
    app_launch_data.ranking_item_type =
        app_list::RankingItemTypeFromChromeAppListItem(*item);
    app_launch_data.launched_from = ash::AppListLaunchedFrom::kLaunchedFromGrid;
    search_controller_->Train(std::move(app_launch_data));
  }

  requested_model_updater->ActivateChromeItem(id, event_flags);
}

void AppListClientImpl::GetContextMenuModel(
    int profile_id,
    const std::string& id,
    GetContextMenuModelCallback callback) {
  auto* requested_model_updater = profile_model_mappings_[profile_id];
  if (requested_model_updater != current_model_updater_ ||
      !requested_model_updater) {
    std::move(callback).Run(nullptr);
    return;
  }
  requested_model_updater->GetContextMenuModel(
      id, base::BindOnce(
              [](GetContextMenuModelCallback callback,
                 std::unique_ptr<ui::SimpleMenuModel> menu_model) {
                std::move(callback).Run(std::move(menu_model));
              },
              std::move(callback)));
}

void AppListClientImpl::OnAppListVisibilityWillChange(bool visible) {
  app_list_target_visibility_ = visible;
  if (visible && search_controller_)
    search_controller_->Start(std::u16string());
}

void AppListClientImpl::OnAppListVisibilityChanged(bool visible) {
  app_list_visible_ = visible;
  if (visible && search_controller_)
    search_controller_->AppListShown();
}

void AppListClientImpl::OnItemAdded(
    int profile_id,
    std::unique_ptr<ash::AppListItemMetadata> item) {
  auto* requested_model_updater = profile_model_mappings_[profile_id];
  if (!requested_model_updater)
    return;
  requested_model_updater->OnItemAdded(std::move(item));
}

void AppListClientImpl::OnItemUpdated(
    int profile_id,
    std::unique_ptr<ash::AppListItemMetadata> item) {
  auto* requested_model_updater = profile_model_mappings_[profile_id];
  if (!requested_model_updater)
    return;
  requested_model_updater->OnItemUpdated(std::move(item));
}

void AppListClientImpl::OnFolderDeleted(
    int profile_id,
    std::unique_ptr<ash::AppListItemMetadata> item) {
  auto* requested_model_updater = profile_model_mappings_[profile_id];
  if (!requested_model_updater)
    return;
  DCHECK(item->is_folder);
  requested_model_updater->OnFolderDeleted(std::move(item));
}

void AppListClientImpl::OnPageBreakItemDeleted(int profile_id,
                                               const std::string& id) {
  auto* requested_model_updater = profile_model_mappings_[profile_id];
  if (!requested_model_updater)
    return;
  requested_model_updater->OnPageBreakItemDeleted(id);
}

void AppListClientImpl::OnSearchResultVisibilityChanged(const std::string& id,
                                                        bool visibility) {
  if (!search_controller_)
    return;

  ChromeSearchResult* result = search_controller_->FindSearchResult(id);
  if (result == nullptr) {
    return;
  }
  result->OnVisibilityChanged(visibility);
}

void AppListClientImpl::OnQuickSettingsChanged(
    const std::string& setting_name,
    const std::map<std::string, int>& values) {
  // CrOS action recorder.
  app_list::CrOSActionRecorder::GetCrosActionRecorder()->RecordAction(
      {base::StrCat({"SettingsChanged-", setting_name})}, values);
}

void AppListClientImpl::ActiveUserChanged(user_manager::User* active_user) {
  if (!active_user->is_profile_created())
    return;

  UpdateProfile();
}

void AppListClientImpl::UpdateProfile() {
  Profile* profile = ProfileManager::GetActiveUserProfile();
  app_list::AppListSyncableService* syncable_service =
      app_list::AppListSyncableServiceFactory::GetForProfile(profile);
  // AppListSyncableService is null in tests.
  if (syncable_service)
    SetProfile(profile);
}

void AppListClientImpl::SetProfile(Profile* new_profile) {
  if (profile_ == new_profile)
    return;

  if (profile_) {
    DCHECK(current_model_updater_);
    current_model_updater_->SetActive(false);

    search_controller_.reset();
    app_sync_ui_state_watcher_.reset();
    current_model_updater_ = nullptr;
  }

  template_url_service_observer_.RemoveAll();

  profile_ = new_profile;
  if (!profile_)
    return;

  // If we are in guest mode, the new profile should be an OffTheRecord profile.
  // Otherwise, this may later hit a check (same condition as this one) in
  // Browser::Browser when opening links in a browser window (see
  // http://crbug.com/460437).
  DCHECK(!profile_->IsGuestSession() || profile_->IsOffTheRecord())
      << "Guest mode must use OffTheRecord profile";

  template_url_service_observer_.Add(
      TemplateURLServiceFactory::GetForProfile(profile_));

  app_list::AppListSyncableService* syncable_service =
      app_list::AppListSyncableServiceFactory::GetForProfile(profile_);

  current_model_updater_ = syncable_service->GetModelUpdater();
  current_model_updater_->SetActive(true);

  // On ChromeOS, there is no way to sign-off just one user. When signing off
  // all users, AppListClientImpl instance is destructed before profiles are
  // unloaded. So we don't need to remove elements from
  // |profile_model_mappings_| explicitly.
  profile_model_mappings_[current_model_updater_->model_id()] =
      current_model_updater_;

  app_sync_ui_state_watcher_ =
      std::make_unique<AppSyncUIStateWatcher>(profile_, current_model_updater_);

  SetUpSearchUI();
  OnTemplateURLServiceChanged();

  // Clear search query.
  current_model_updater_->UpdateSearchBox(std::u16string(),
                                          false /* initiated_by_user */);
}

void AppListClientImpl::SetUpSearchUI() {
  search_controller_ = app_list::CreateSearchController(
      profile_, current_model_updater_, this, GetNotifier());

  // Refresh the results used for the suggestion chips with empty query.
  // This fixes crbug.com/999287.
  StartSearch(std::u16string());
}

app_list::SearchController* AppListClientImpl::search_controller() {
  return search_controller_.get();
}

AppListModelUpdater* AppListClientImpl::GetModelUpdaterForTest() {
  return current_model_updater_;
}

void AppListClientImpl::OnTemplateURLServiceChanged() {
  DCHECK(current_model_updater_);

  TemplateURLService* template_url_service =
      TemplateURLServiceFactory::GetForProfile(profile_);
  const TemplateURL* default_provider =
      template_url_service->GetDefaultSearchProvider();
  const bool is_google =
      default_provider &&
      default_provider->GetEngineType(
          template_url_service->search_terms_data()) == SEARCH_ENGINE_GOOGLE;

  current_model_updater_->SetSearchEngineIsGoogle(is_google);
}

void AppListClientImpl::ShowAppList() {
  // This may not work correctly if the profile passed in is different from the
  // one the ash Shell is currently using.
  if (!app_list_controller_)
    return;
  app_list_controller_->ShowAppList();
}

Profile* AppListClientImpl::GetCurrentAppListProfile() const {
  return ChromeLauncherController::instance()->profile();
}

ash::AppListController* AppListClientImpl::GetAppListController() const {
  return app_list_controller_;
}

void AppListClientImpl::DismissView() {
  if (!app_list_controller_)
    return;
  app_list_controller_->DismissAppList();
}

aura::Window* AppListClientImpl::GetAppListWindow() {
  return app_list_controller_->GetWindow();
}

int64_t AppListClientImpl::GetAppListDisplayId() {
  return display_id_;
}

bool AppListClientImpl::IsAppPinned(const std::string& app_id) {
  return ChromeLauncherController::instance()->IsAppPinned(app_id);
}

bool AppListClientImpl::IsAppOpen(const std::string& app_id) const {
  return ChromeLauncherController::instance()->IsOpen(ash::ShelfID(app_id));
}

void AppListClientImpl::PinApp(const std::string& app_id) {
  ChromeLauncherController::instance()->PinAppWithID(app_id);
}

void AppListClientImpl::UnpinApp(const std::string& app_id) {
  ChromeLauncherController::instance()->UnpinAppWithID(app_id);
}

AppListControllerDelegate::Pinnable AppListClientImpl::GetPinnable(
    const std::string& app_id) {
  return GetPinnableForAppID(app_id,
                             ChromeLauncherController::instance()->profile());
}

void AppListClientImpl::CreateNewWindow(bool incognito) {
  ash::NewWindowDelegate::GetInstance()->NewWindow(incognito);
}

void AppListClientImpl::OpenURL(Profile* profile,
                                const GURL& url,
                                ui::PageTransition transition,
                                WindowOpenDisposition disposition) {
  NavigateParams params(profile, url, transition);
  params.disposition = disposition;
  Navigate(&params);
}

void AppListClientImpl::NotifySearchResultsForLogging(
    const std::u16string& trimmed_query,
    const ash::SearchResultIdWithPositionIndices& results,
    int position_index) {
  if (search_controller_) {
    search_controller_->OnSearchResultsImpressionMade(trimmed_query, results,
                                                      position_index);
  }
}

ash::AppListNotifier* AppListClientImpl::GetNotifier() {
  return app_list_notifier_.get();
}
