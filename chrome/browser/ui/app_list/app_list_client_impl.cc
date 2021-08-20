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
#include "chrome/browser/ui/app_list/search/ranking/launch_data.h"
#include "chrome/browser/ui/app_list/search/search_controller.h"
#include "chrome/browser/ui/app_list/search/search_controller_factory.h"
#include "chrome/browser/ui/app_list/search/search_result_ranker/ranking_item_util.h"
#include "chrome/browser/ui/ash/shelf/chrome_shelf_controller.h"
#include "chrome/browser/ui/ash/shelf/chrome_shelf_controller_util.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/browser/ui/browser_commands.h"
#include "chrome/browser/ui/browser_list.h"
#include "chrome/browser/ui/browser_navigator.h"
#include "chrome/browser/ui/browser_navigator_params.h"
#include "chrome/browser/ui/browser_window.h"
#include "components/session_manager/core/session_manager.h"
#include "extensions/common/extension.h"
#include "ui/display/display.h"
#include "ui/display/screen.h"
#include "ui/display/types/display_constants.h"
#include "ui/gfx/geometry/rect.h"

namespace {

AppListClientImpl* g_app_list_client_instance = nullptr;

// Parameters used by the time duration metrics.
constexpr base::TimeDelta kTimeMetricsMin = base::TimeDelta::FromSeconds(1);
constexpr base::TimeDelta kTimeMetricsMax = base::TimeDelta::FromDays(7);
constexpr int kTimeMetricsBucketCount = 100;

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
  session_manager::SessionManager::Get()->AddObserver(this);

  DCHECK(!g_app_list_client_instance);
  g_app_list_client_instance = this;
}

AppListClientImpl::~AppListClientImpl() {
  SetProfile(nullptr);

  auto* user_manager = user_manager::UserManager::Get();
  user_manager->RemoveSessionStateObserver(this);

  // We assume that the current user is new if `state_for_new_user_` has value.
  if (state_for_new_user_.has_value() &&
      !state_for_new_user_->showing_recorded) {
    DCHECK(user_manager->IsCurrentUserNew());

    // Prefer the function to the macro because the usage data is recorded no
    // more than once per second.
    base::UmaHistogramEnumeration(
        "Apps.AppListUsageByNewUsers",
        AppListUsageStateByNewUsers::kNotUsedBeforeDestruction);
  }

  session_manager::SessionManager::Get()->RemoveObserver(this);

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

void AppListClientImpl::OpenSearchResult(
    int profile_id,
    const std::string& result_id,
    ash::AppListSearchResultType result_type,
    int event_flags,
    ash::AppListLaunchedFrom launched_from,
    ash::AppListLaunchType launch_type,
    int suggestion_index,
    bool launch_as_default) {
  if (!search_controller_)
    return;

  auto requested_model_updater_iter = profile_model_mappings_.find(profile_id);
  DCHECK(requested_model_updater_iter != profile_model_mappings_.end());
  DCHECK_EQ(current_model_updater_, requested_model_updater_iter->second);

  ChromeSearchResult* result = search_controller_->FindSearchResult(result_id);
  if (!result)
    return;

  app_list::LaunchData launch_data;
  launch_data.id = result_id;
  launch_data.result_type = result_type;
  launch_data.ranking_item_type =
      app_list::RankingItemTypeFromSearchResult(*result);
  launch_data.launch_type = launch_type;
  launch_data.launched_from = launched_from;
  launch_data.suggestion_index = suggestion_index;
  launch_data.score = result->relevance();

  if (launch_type == ash::AppListLaunchType::kAppSearchResult &&
      launched_from == ash::AppListLaunchedFrom::kLaunchedFromSearchBox &&
      launch_data.ranking_item_type == app_list::RankingItemType::kApp &&
      search_controller_->GetLastQueryLength() != 0) {
    ash::RecordSuccessfulAppLaunchUsingSearch(
        launched_from, search_controller_->GetLastQueryLength());
  }

  // Send training signal to search controller.
  search_controller_->Train(std::move(launch_data));

  RecordSearchResultOpenTypeHistogram(launched_from, result->metrics_type(),
                                      IsTabletMode());

  if (launch_as_default)
    RecordDefaultSearchResultOpenTypeHistogram(result->metrics_type());

  if (!search_controller_->GetLastQueryLength() &&
      launched_from == ash::AppListLaunchedFrom::kLaunchedFromSearchBox)
    RecordZeroStateSuggestionOpenTypeHistogram(result->metrics_type());

  if (launched_from == ash::AppListLaunchedFrom::kLaunchedFromSearchBox)
    RecordOpenedResultFromSearchBox(result_type);

  MaybeRecordLauncherAction(launched_from);

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
  MaybeRecordViewShown();

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
    app_list::LaunchData launch_data;
    launch_data.id = id;
    // We don't have easy access to the search result type here, so
    // launch_data.result_type isn't set. However we have no need to distinguish
    // the type of apps launched from the grid in SearchController::Train.
    launch_data.ranking_item_type =
        app_list::RankingItemTypeFromChromeAppListItem(*item);
    launch_data.launched_from = ash::AppListLaunchedFrom::kLaunchedFromGrid;
    search_controller_->Train(std::move(launch_data));
  }

  MaybeRecordLauncherAction(ash::AppListLaunchedFrom::kLaunchedFromGrid);
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
  if (user_manager::UserManager::Get()->IsCurrentUserNew()) {
    // In tests, the user before switching and the one after switching may
    // be both new. It should not happen in the real world.
    state_for_new_user_ = StateForNewUser();
  } else if (state_for_new_user_) {
    if (!state_for_new_user_->showing_recorded) {
      // We assume that the previous user before switching was new if
      // `state_for_new_user_` is not null.
      base::UmaHistogramEnumeration(
          "Apps.AppListUsageByNewUsers",
          AppListUsageStateByNewUsers::kNotUsedBeforeSwitchingAccounts);
    }
    state_for_new_user_.reset();
  }

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

  template_url_service_observation_.Reset();

  profile_ = new_profile;
  if (!profile_)
    return;

  // If we are in guest mode, the new profile should be an OffTheRecord profile.
  // Otherwise, this may later hit a check (same condition as this one) in
  // Browser::Browser when opening links in a browser window (see
  // http://crbug.com/460437).
  DCHECK(!profile_->IsGuestSession() || profile_->IsOffTheRecord())
      << "Guest mode must use OffTheRecord profile";

  template_url_service_observation_.Observe(
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

void AppListClientImpl::InitializeAsIfNewUserLoginForTest() {
  new_user_session_activation_time_ = base::Time::Now();
  state_for_new_user_ = StateForNewUser();
}

void AppListClientImpl::OnSessionStateChanged() {
  // Return early if the current user is not new or the session is not active.
  if (!user_manager::UserManager::Get()->IsCurrentUserNew() ||
      session_manager::SessionManager::Get()->session_state() !=
          session_manager::SessionState::ACTIVE) {
    return;
  }

  new_user_session_activation_time_ = base::Time::Now();
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
  return ChromeShelfController::instance()->profile();
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
  return ChromeShelfController::instance()->IsAppPinned(app_id);
}

bool AppListClientImpl::IsAppOpen(const std::string& app_id) const {
  return ChromeShelfController::instance()->IsOpen(ash::ShelfID(app_id));
}

void AppListClientImpl::PinApp(const std::string& app_id) {
  ChromeShelfController::instance()->PinAppWithID(app_id);
}

void AppListClientImpl::UnpinApp(const std::string& app_id) {
  ChromeShelfController::instance()->UnpinAppWithID(app_id);
}

AppListControllerDelegate::Pinnable AppListClientImpl::GetPinnable(
    const std::string& app_id) {
  return GetPinnableForAppID(app_id,
                             ChromeShelfController::instance()->profile());
}

void AppListClientImpl::CreateNewWindow(bool incognito,
                                        bool should_trigger_session_restore) {
  ash::NewWindowDelegate::GetInstance()->NewWindow(
      incognito, should_trigger_session_restore);
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

void AppListClientImpl::LoadIcon(int profile_id, const std::string& app_id) {
  auto* requested_model_updater = profile_model_mappings_[profile_id];
  if (requested_model_updater != current_model_updater_ ||
      !requested_model_updater) {
    return;
  }
  requested_model_updater->LoadAppIcon(app_id);
}

void AppListClientImpl::MaybeRecordViewShown() {
  // Record the time duration between session activation and the first launcher
  // showing if the current user is new.

  // We do not need to worry about the scenario below:
  // log in to a new account -> switch to another account -> switch back to the
  // initial account-> show the launcher
  // In this case, when showing the launcher, the current user is not
  // new anymore.
  // TODO(https://crbug.com/1211620): If this bug is fixed, we might need to
  // do some changes here.
  if (!user_manager::UserManager::Get()->IsCurrentUserNew()) {
    DCHECK(!state_for_new_user_);
    return;
  }

  if (state_for_new_user_->showing_recorded) {
    // Showing launcher was recorded before so return early.
    return;
  }

  state_for_new_user_->showing_recorded = true;

  DCHECK(new_user_session_activation_time_.has_value());
  const base::TimeDelta opening_duration =
      base::Time::Now() - *new_user_session_activation_time_;
  if (opening_duration >= base::TimeDelta()) {
    // `base::Time` may skew. Therefore only record when the time duration is
    // non-negative.
    UMA_HISTOGRAM_CUSTOM_TIMES(
        /*name=*/
        "Apps."
        "TimeDurationBetweenNewUserSessionActivationAndFirstLauncherOpening",
        /*sample=*/opening_duration, kTimeMetricsMin, kTimeMetricsMax,
        kTimeMetricsBucketCount);

    base::UmaHistogramEnumeration("Apps.AppListUsageByNewUsers",
                                  AppListUsageStateByNewUsers::kUsed);
  }
}

void AppListClientImpl::RecordOpenedResultFromSearchBox(
    ash::AppListSearchResultType result_type) {
  // Check whether there is any Chrome non-app browser window open and not
  // minimized.
  bool non_app_browser_open_and_not_minimzed = false;
  for (auto* browser : *BrowserList::GetInstance()) {
    if (browser->type() != Browser::TYPE_NORMAL ||
        browser->window()->IsMinimized()) {
      // Skip if `browser` is not a normal browser or `browser` is minimized.
      continue;
    }

    non_app_browser_open_and_not_minimzed = true;
    break;
  }

  if (non_app_browser_open_and_not_minimzed) {
    UMA_HISTOGRAM_ENUMERATION(
        "Apps.OpenedAppListSearchResultFromSearchBox."
        "ExistNonAppBrowserWindowOpenAndNotMinimized",
        result_type);
  } else {
    UMA_HISTOGRAM_ENUMERATION(
        "Apps.OpenedAppListSearchResultFromSearchBox."
        "NonAppBrowserWindowsEitherClosedOrMinimized",
        result_type);
  }
}

void AppListClientImpl::MaybeRecordLauncherAction(
    ash::AppListLaunchedFrom launched_from) {
  DCHECK(launched_from == ash::AppListLaunchedFrom::kLaunchedFromGrid ||
         launched_from ==
             ash::AppListLaunchedFrom::kLaunchedFromSuggestionChip ||
         launched_from == ash::AppListLaunchedFrom::kLaunchedFromSearchBox);

  // Return early if the current user is not new.
  if (!user_manager::UserManager::Get()->IsCurrentUserNew()) {
    DCHECK(!state_for_new_user_);
    return;
  }

  // The launcher action has been recorded so return early.
  if (state_for_new_user_->action_recorded)
    return;

  state_for_new_user_->action_recorded = true;
  base::UmaHistogramEnumeration("Apps.FirstLauncherActionByNewUsers",
                                launched_from);

  DCHECK(new_user_session_activation_time_.has_value());
  const base::TimeDelta launcher_action_duration =
      base::Time::Now() - *new_user_session_activation_time_;
  if (launcher_action_duration >= base::TimeDelta()) {
    // `base::Time` may skew. Therefore only record when the time duration is
    // non-negative.
    UMA_HISTOGRAM_CUSTOM_TIMES(
        /*name=*/
        "Apps.TimeBetweenNewUserSessionActivationAndFirstLauncherAction",
        /*sample=*/launcher_action_duration, kTimeMetricsMin, kTimeMetricsMax,
        kTimeMetricsBucketCount);
  }
}
