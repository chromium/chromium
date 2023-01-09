// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/ash/desks/desks_client.h"

#include <cstddef>
#include <memory>
#include <string>

#include "ash/accessibility/accessibility_controller_impl.h"
#include "ash/constants/ash_features.h"
#include "ash/public/cpp/desk_template.h"
#include "ash/public/cpp/session/session_controller.h"
#include "ash/shell.h"
#include "ash/strings/grit/ash_strings.h"
#include "ash/wm/desks/desk.h"
#include "ash/wm/desks/desks_bar_view.h"
#include "ash/wm/desks/desks_controller.h"
#include "ash/wm/desks/desks_histogram_enums.h"
#include "ash/wm/desks/desks_restore_util.h"
#include "ash/wm/desks/templates/saved_desk_util.h"
#include "ash/wm/overview/overview_controller.h"
#include "ash/wm/overview/overview_grid.h"
#include "ash/wm/overview/overview_session.h"
#include "base/functional/bind.h"
#include "base/functional/callback_helpers.h"
#include "base/guid.h"
#include "base/metrics/histogram_functions.h"
#include "base/scoped_observation.h"
#include "base/task/single_thread_task_runner.h"
#include "base/timer/timer.h"
#include "base/values.h"
#include "chrome/browser/apps/app_service/app_service_proxy.h"
#include "chrome/browser/apps/app_service/app_service_proxy_factory.h"
#include "chrome/browser/apps/app_service/browser_app_instance_observer.h"
#include "chrome/browser/apps/app_service/browser_app_instance_registry.h"
#include "chrome/browser/ash/crosapi/browser_util.h"
#include "chrome/browser/ash/profiles/profile_helper.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/profiles/profile_manager.h"
#include "chrome/browser/sync/desk_sync_service_factory.h"
#include "chrome/browser/ui/ash/desks/desks_templates_app_launch_handler.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/browser/ui/browser_list.h"
#include "chrome/browser/ui/browser_window.h"
#include "components/app_constants/constants.h"
#include "components/app_restore/app_restore_info.h"
#include "components/app_restore/window_properties.h"
#include "components/desks_storage/core/desk_model_wrapper.h"
#include "components/desks_storage/core/desk_sync_service.h"
#include "components/desks_storage/core/local_desk_data_manager.h"
#include "components/sessions/core/session_id.h"
#include "ui/aura/client/aura_constants.h"
#include "ui/aura/window.h"
#include "ui/base/l10n/l10n_util.h"
#include "ui/views/widget/widget.h"

namespace {

DesksClient* g_desks_client_instance = nullptr;

// Used to generate unique IDs for desk template launches.
int32_t g_launch_id = 0;

// Timeout time used in LaunchPerformanceTracker.
constexpr base::TimeDelta kLaunchPerformanceTimeout = base::Minutes(3);

// Histogram name to track estimated time it takes to load a template. Used by
// LaunchPerformanceTracker. Note that this is in a different spot than the
// other metrics because the class that uses this is owned by `this`.
constexpr char kTimeToLoadTemplateHistogramName[] =
    "Ash.DeskTemplate.TimeToLoadTemplate";

constexpr char kCrxAppPrefix[] = "_crx_";

// Launch data is cleared after this time.
constexpr base::TimeDelta kClearLaunchDataDuration = base::Seconds(20);

// Returns true if `profile` is a supported profile in desk template feature.
bool IsSupportedProfile(Profile* profile) {
  // Public users & guest users are not supported.
  return profile && profile->IsRegularProfile();
}

// Creates a set of window IDs for the launch tracker to monitor for.
std::set<int> GetWindowIDSetFromTemplate(
    const ash::DeskTemplate* desk_template) {
  std::set<int> window_ids;
  const app_restore::RestoreData* desk_restore_data =
      desk_template->desk_restore_data();

  for (const auto& app : desk_restore_data->app_id_to_launch_list()) {
    for (const auto& window : app.second)
      window_ids.insert(window.first);
  }

  return window_ids;
}

// Records the time to load a template based on the starting time `time_started`
// passed into this function and a call to base::Time::Now called at the
// beginning of this function.
void RecordTimeToLoadTemplateHistogram(const base::Time time_started) {
  base::UmaHistogramMediumTimes(kTimeToLoadTemplateHistogramName,
                                base::Time::Now() - time_started);
}

}  // namespace

// Listens to `BrowserAppInstanceRegistry` events. Its job is to store app ids
// for lacros windows so that when a lacros window is part of a saved desk, we
// can figure out the app id (if any).
class LacrosAppWindowObserver : public apps::BrowserAppInstanceObserver {
 public:
  explicit LacrosAppWindowObserver(
      apps::BrowserAppInstanceRegistry& browser_app_instance_registry) {
    browser_app_instance_registry_observation_.Observe(
        &browser_app_instance_registry);
  }

  LacrosAppWindowObserver(const LacrosAppWindowObserver&) = delete;
  LacrosAppWindowObserver& operator=(const LacrosAppWindowObserver&) = delete;
  ~LacrosAppWindowObserver() override = default;

  // BrowserAppInstanceObserver:
  void OnBrowserAppAdded(const apps::BrowserAppInstance& instance) override {
    if (!instance.app_id.empty()) {
      app_ids_by_window_[instance.window] = instance.app_id;
    }
  }

  void OnBrowserAppRemoved(const apps::BrowserAppInstance& instance) override {
    app_ids_by_window_.erase(instance.window);
  }

  absl::optional<std::string> GetAppIdForWindow(aura::Window* window) const {
    auto it = app_ids_by_window_.find(window);
    if (it == app_ids_by_window_.end()) {
      return absl::nullopt;
    }
    return kCrxAppPrefix + it->second;
  }

 private:
  base::flat_map<aura::Window*, std::string> app_ids_by_window_;

  base::ScopedObservation<apps::BrowserAppInstanceRegistry,
                          apps::BrowserAppInstanceObserver>
      browser_app_instance_registry_observation_{this};
};

// Tracks a set of WindowIDs through the launching process, records a
// launch performance metric when the set of window_ids have all been
// launched
class DesksClient::LaunchPerformanceTracker
    : public app_restore::AppRestoreInfo::Observer {
 public:
  LaunchPerformanceTracker(const std::set<int>& window_ids,
                           base::GUID template_id,
                           DesksClient* templates_client)
      : tracked_window_ids_(window_ids),
        time_launch_started_(base::Time::Now()),
        template_id_(template_id),
        templates_client_(templates_client) {
    scoped_observation_.Observe(app_restore::AppRestoreInfo::GetInstance());
    timeout_timer_ = std::make_unique<base::OneShotTimer>();
    timeout_timer_->Start(
        FROM_HERE, kLaunchPerformanceTimeout,
        base::BindOnce(&DesksClient::LaunchPerformanceTracker::OnTimeout,
                       weak_ptr_factory_.GetWeakPtr()));
  }

  LaunchPerformanceTracker(const LaunchPerformanceTracker&) = delete;
  LaunchPerformanceTracker& operator=(const LaunchPerformanceTracker&) = delete;
  ~LaunchPerformanceTracker() override {}

  // Removes window ID from tracked set because the window has been launched.
  // app_restore::AppRestoreInfo::Observer:
  void OnWidgetInitialized(views::Widget* widget) override {
    tracked_window_ids_.erase(widget->GetNativeWindow()->GetProperty(
        app_restore::kRestoreWindowIdKey));
    MaybeRecordMetric();
  }

  // Removes `window_id` from the tracked set because the window has already
  // been launched by another process.
  void OnMovedSingleInstanceApp(int32_t window_id) {
    tracked_window_ids_.erase(window_id);
    MaybeRecordMetric();
  }

 private:
  // Records performance metric iff `tracked_window_ids_` are empty.
  void MaybeRecordMetric() {
    if (tracked_window_ids_.empty()) {
      RecordTimeToLoadTemplateHistogram(time_launch_started_);
      templates_client_->RemoveLaunchPerformanceTracker(template_id_);
    }
  }

  // Called when timeout timer runs out. Records time metric.
  void OnTimeout() {
    tracked_window_ids_.clear();
    MaybeRecordMetric();
  }

  std::set<int> tracked_window_ids_;
  base::Time time_launch_started_;
  base::GUID template_id_;
  std::unique_ptr<base::OneShotTimer> timeout_timer_;

  // Pointer back to the owning templates client. This is done to facilitate
  // this object's removal from the mapping of template id's to trackers after
  // this object has recorded its metric.
  DesksClient* templates_client_;

  base::ScopedObservation<app_restore::AppRestoreInfo,
                          app_restore::AppRestoreInfo::Observer>
      scoped_observation_{this};
  base::WeakPtrFactory<LaunchPerformanceTracker> weak_ptr_factory_{this};
};

DesksClient::DesksClient() : desks_controller_(ash::DesksController::Get()) {
  DCHECK(!g_desks_client_instance);
  g_desks_client_instance = this;
  if (ash::SessionController::Get()) {
    ash::SessionController::Get()->AddObserver(this);
  }
}

DesksClient::~DesksClient() {
  DCHECK_EQ(this, g_desks_client_instance);
  g_desks_client_instance = nullptr;
  if (ash::SessionController::Get()) {
    ash::SessionController::Get()->RemoveObserver(this);
  }
}

// static
DesksClient* DesksClient::Get() {
  return g_desks_client_instance;
}

void DesksClient::OnActiveUserSessionChanged(const AccountId& account_id) {
  Profile* profile =
      ash::ProfileHelper::Get()->GetProfileByAccountId(account_id);
  if (profile == active_profile_ || !IsSupportedProfile(profile))
    return;

  // Start lacros app window observer.
  if (auto* proxy = apps::AppServiceProxyFactory::GetForProfile(profile)) {
    if (auto* registry = proxy->BrowserAppInstanceRegistry()) {
      lacros_app_window_observer_ =
          std::make_unique<LacrosAppWindowObserver>(*registry);
    }
  }

  active_profile_ = profile;
  DCHECK(active_profile_);

  if (ash::features::IsSavedDesksEnabled()) {
    save_and_recall_desks_storage_manager_ =
        std::make_unique<desks_storage::LocalDeskDataManager>(
            active_profile_->GetPath(), account_id);

    if (ash::saved_desk_util::AreDesksTemplatesEnabled() &&
        ash::features::IsDeskTemplateSyncEnabled()) {
      saved_desk_storage_manager_ =
          std::make_unique<desks_storage::DeskModelWrapper>(
              save_and_recall_desks_storage_manager_.get());
    }

  } else {
    if (!ash::features::IsDeskTemplateSyncEnabled()) {
      desk_templates_storage_manager_ =
          std::make_unique<desks_storage::LocalDeskDataManager>(
              active_profile_->GetPath(), account_id);
    }
  }

  auto policy_desk_templates_it =
      preconfigured_desk_templates_json_.find(account_id);
  if (policy_desk_templates_it != preconfigured_desk_templates_json_.end())
    GetDeskModel()->SetPolicyDeskTemplates(policy_desk_templates_it->second);
}

// TODO(aprilzhou): Refactor DesksClient to remove unnecessary callback. It's
// causing the code to be less readable.
void DesksClient::CaptureActiveDeskAndSaveTemplate(
    CaptureActiveDeskAndSaveTemplateCallback callback,
    ash::DeskTemplateType template_type) {
  if (!active_profile_) {
    std::move(callback).Run(DeskActionError::kNoCurrentUserError,
                            /*desk_template=*/nullptr);
    return;
  }

  desks_controller_->CaptureActiveDeskAsSavedDesk(
      base::BindOnce(&DesksClient::OnCapturedDeskTemplate,
                     weak_ptr_factory_.GetWeakPtr(), std::move(callback)),
      template_type,
      /*root_window_to_show=*/nullptr);
}

void DesksClient::DeleteDeskTemplate(const base::GUID& template_uuid,
                                     DeleteDeskTemplateCallback callback) {
  if (!active_profile_) {
    std::move(callback).Run(DeskActionError::kNoCurrentUserError);

    return;
  }

  GetDeskModel()->DeleteEntry(
      template_uuid,
      base::BindOnce(&DesksClient::OnDeleteDeskTemplate,
                     weak_ptr_factory_.GetWeakPtr(), std::move(callback)));
}

void DesksClient::GetDeskTemplates(GetDeskTemplatesCallback callback) {
  if (!active_profile_) {
    std::move(callback).Run(DeskActionError::kNoCurrentUserError,
                            /*desk_templates=*/{});
    return;
  }

  auto result = GetDeskModel()->GetAllEntries();

  std::move(callback).Run(
      result.status != desks_storage::DeskModel::GetAllEntriesStatus::kOk
          ? absl::make_optional(DeskActionError::kStorageError)
          : absl::nullopt,
      result.entries);
}

void DesksClient::GetTemplateJson(const base::GUID& uuid,
                                  Profile* profile,
                                  GetTemplateJsonCallback callback) {
  if (!active_profile_ || active_profile_ != profile) {
    std::move(callback).Run(DeskActionError::kBadProfileError,
                            /*desk_templates=*/{});
    return;
  }

  apps::AppRegistryCache& app_cache =
      apps::AppServiceProxyFactory::GetForProfile(profile)->AppRegistryCache();

  GetDeskModel()->GetTemplateJson(
      uuid, &app_cache,
      base::BindOnce(&DesksClient::OnGetTemplateJson,
                     weak_ptr_factory_.GetWeakPtr(), std::move(callback)));
}

void DesksClient::LaunchDeskTemplate(
    const base::GUID& template_uuid,
    LaunchDeskCallback callback,
    const std::u16string& customized_desk_name) {
  if (!active_profile_) {
    std::move(callback).Run(DeskActionError::kNoCurrentUserError, {});
    return;
  }

  if (launch_template_for_test_) {
    OnGetTemplateForDeskLaunch(
        std::move(callback), customized_desk_name,
        desks_storage::DeskModel::GetEntryByUuidStatus::kOk,
        launch_template_for_test_->Clone());
    return;
  }

  desks_storage::DeskModel::GetEntryByUuidResult result =
      GetDeskModel()->GetEntryByUUID(template_uuid);

  OnGetTemplateForDeskLaunch(std::move(callback), customized_desk_name,
                             result.status, std::move(result.entry));
}

void DesksClient::LaunchEmptyDesk(LaunchDeskCallback callback,
                                  const std::u16string& customized_desk_name) {
  if (!desks_controller_->CanCreateDesks()) {
    std::move(callback).Run(DeskActionError::kDesksCountCheckFailedError, {});
    return;
  }

  // Don't launch desk if desk is being modified(activated/removed/switched) or
  // desk animation is in progress.
  if (desks_controller_->AreDesksBeingModified()) {
    std::move(callback).Run(DeskActionError::kDesksBeingModifiedError, {});
    return;
  }

  const ash::Desk* new_desk = CreateEmptyDeskAndActivate(customized_desk_name);
  std::move(callback).Run({}, new_desk->uuid());
}

void DesksClient::RemoveDesk(const base::GUID& desk_uuid,
                             bool combine_desk,
                             ErrorHandlingCallBack callback) {
  // Return error if `desk_uuid` is invalid.
  if (!desk_uuid.is_valid()) {
    std::move(callback).Run(DeskActionError::kInvalidIdError);
    return;
  }

  ash::Desk* desk = desks_controller_->GetDeskByUuid(desk_uuid);
  // Can't clean up desk when desk identifier is incorrect.
  if (!desk) {
    std::move(callback).Run(DeskActionError::kResourceNotFoundError);
    return;
  }

  // Don't remove desk if desk is being modified(activated/removed/switched) or
  // desk animation is in progress.
  if (desks_controller_->AreDesksBeingModified()) {
    std::move(callback).Run(DeskActionError::kDesksBeingModifiedError);
    return;
  }

  // Can't clean up desk when there is no more than 1 desk left.
  if (desks_controller_->CanRemoveDesks()) {
    desks_controller_->RemoveDesk(desk, ash::DesksCreationRemovalSource::kApi,
                                  combine_desk
                                      ? ash::DeskCloseType::kCombineDesks
                                      : ash::DeskCloseType::kCloseAllWindows);
  } else {
    std::move(callback).Run(DeskActionError::kDesksCountCheckFailedError);
    return;
  }

  std::move(callback).Run({});
}

void DesksClient::GetAllDesks(GetAllDesksCallback callback) {
  std::vector<const ash::Desk*> desks;
  for (const auto& desk : desks_controller_->desks())
    desks.push_back(desk.get());
  // There should be at least one default desk.
  std::move(callback).Run(
      desks.empty() ? absl::make_optional(DeskActionError::kUnknownError)
                    : absl::nullopt,
      desks);
}

void DesksClient::LaunchAppsFromTemplate(
    std::unique_ptr<ash::DeskTemplate> desk_template) {
  DCHECK(desk_template);
  DCHECK_EQ(desk_template->launch_id(), 0);

  // Generate a unique ID for this launch. It is used to tell different template
  // launches apart.
  desk_template->set_launch_id(++g_launch_id);

  app_restore::RestoreData* restore_data =
      desk_template->mutable_desk_restore_data();
  if (!restore_data)
    return;
  if (restore_data->app_id_to_launch_list().empty())
    return;

  // Since we default the browser to launch as ash chrome, we want to to check
  // to see if lacros is enabled and primary. If so, update the app id of the
  // browser app to launch lacros instead of ash.
  if (crosapi::browser_util::IsLacrosEnabled() &&
      crosapi::browser_util::IsLacrosPrimaryBrowser()) {
    restore_data->UpdateBrowserAppIdToLacros();
  }

  // Make window IDs of the template unique. This is a requirement for launching
  // templates concurrently since the contained window IDs are used as lookup
  // keys in many places. We must also do this *before* creating the performance
  // tracker below.
  restore_data->MakeWindowIdsUniqueForDeskTemplate();

  template_ids_to_launch_performance_trackers_[desk_template->uuid()] =
      std::make_unique<LaunchPerformanceTracker>(
          GetWindowIDSetFromTemplate(desk_template.get()),
          desk_template->uuid(), this);

  DCHECK(active_profile_);
  const int32_t launch_id = desk_template->launch_id();

  auto& handler = app_launch_handlers_[launch_id];
  // Some tests reach into this class and install a handler ahead of time. In
  // all other cases, we create a handler for the launch here.
  if (!handler) {
    handler = std::make_unique<DesksTemplatesAppLaunchHandler>(active_profile_);
  }

  handler->LaunchTemplate(*desk_template);

  // Install a timer that will clear the launch handler after a given duration.
  base::SingleThreadTaskRunner::GetCurrentDefault()->PostDelayedTask(
      FROM_HERE,
      base::BindOnce(&DesksClient::OnLaunchComplete,
                     weak_ptr_factory_.GetWeakPtr(), launch_id),
      kClearLaunchDataDuration);
}

desks_storage::DeskModel* DesksClient::GetDeskModel() {
  if (ash::features::IsSavedDesksEnabled()) {
    if (!ash::saved_desk_util::AreDesksTemplatesEnabled() ||
        !ash::features::IsDeskTemplateSyncEnabled()) {
      DCHECK(save_and_recall_desks_storage_manager_.get());
      return save_and_recall_desks_storage_manager_.get();
    }
    DCHECK(saved_desk_storage_manager_);
    saved_desk_storage_manager_->SetDeskSyncBridge(
        static_cast<desks_storage::DeskSyncBridge*>(
            DeskSyncServiceFactory::GetForProfile(active_profile_)
                ->GetDeskModel()));
    return saved_desk_storage_manager_.get();
  } else {
    if (ash::features::IsDeskTemplateSyncEnabled()) {
      return DeskSyncServiceFactory::GetForProfile(active_profile_)
          ->GetDeskModel();
    }

    DCHECK(desk_templates_storage_manager_.get());
    return desk_templates_storage_manager_.get();
  }
}

// Sets the preconfigured desk template. Data contains the contents of the JSON
// file with the template information
void DesksClient::SetPolicyPreconfiguredTemplate(
    const AccountId& account_id,
    std::unique_ptr<std::string> data) {
  Profile* profile =
      ash::ProfileHelper::Get()->GetProfileByAccountId(account_id);
  if (!data || !IsSupportedProfile(profile))
    return;

  std::string& in_map_data = preconfigured_desk_templates_json_[account_id];
  if (in_map_data == *data)
    return;

  in_map_data = *data;

  if (profile && profile == active_profile_)
    GetDeskModel()->SetPolicyDeskTemplates(*data);
}

void DesksClient::RemovePolicyPreconfiguredTemplate(
    const AccountId& account_id) {
  Profile* profile =
      ash::ProfileHelper::Get()->GetProfileByAccountId(account_id);
  if (!IsSupportedProfile(profile))
    return;

  DCHECK(profile);

  preconfigured_desk_templates_json_.erase(account_id);

  if (profile == active_profile_)
    GetDeskModel()->RemovePolicyDeskTemplates();
}

void DesksClient::NotifyMovedSingleInstanceApp(int32_t window_id) {
  for (auto& id_to_tracker : template_ids_to_launch_performance_trackers_)
    id_to_tracker.second->OnMovedSingleInstanceApp(window_id);
}

void DesksClient::SetAllDeskPropertyByBrowserSessionId(
    SessionID browser_session_id,
    bool all_desk,
    ErrorHandlingCallBack callback) {
  if (!browser_session_id.is_valid()) {
    std::move(callback).Run(DeskActionError::kInvalidIdError);
    return;
  }

  aura::Window* window = GetWindowByBrowserSessionId(browser_session_id);
  if (!window) {
    std::move(callback).Run(DeskActionError::kResourceNotFoundError);
    return;
  }
  window->SetProperty(aura::client::kWindowWorkspaceKey,
                      all_desk
                          ? aura::client::kWindowWorkspaceVisibleOnAllWorkspaces
                          : aura::client::kWindowWorkspaceUnassignedWorkspace);
  std::move(callback).Run(absl::nullopt);
}

base::GUID DesksClient::GetActiveDesk() {
  return desks_controller_->GetTargetActiveDesk()->uuid();
}

absl::optional<DesksClient::DeskActionError> DesksClient::SwitchDesk(
    const base::GUID& desk_uuid) {
  ash::Desk* desk = desks_controller_->GetDeskByUuid(desk_uuid);
  if (!desk) {
    return DeskActionError::kResourceNotFoundError;
  }

  // Don't switch desk if desk is being modified(activated/removed/switched) or
  // desk animation is in progress.
  if (desks_controller_->AreDesksBeingModified()) {
    return DeskActionError::kDesksBeingModifiedError;
  }

  desks_controller_->ActivateDesk(desk, ash::DesksSwitchSource::kApiSwitch);
  return absl::nullopt;
}

absl::optional<std::string> DesksClient::GetAppIdForLacrosWindow(
    aura::Window* window) const {
  return lacros_app_window_observer_
             ? lacros_app_window_observer_->GetAppIdForWindow(window)
             : absl::nullopt;
}

void DesksClient::OnGetTemplateForDeskLaunch(
    LaunchDeskCallback callback,
    std::u16string customized_desk_name,
    desks_storage::DeskModel::GetEntryByUuidStatus status,
    std::unique_ptr<ash::DeskTemplate> saved_desk) {
  if (status != desks_storage::DeskModel::GetEntryByUuidStatus::kOk) {
    std::move(callback).Run(DeskActionError::kStorageError, {});
    return;
  }
  if (!desks_controller_->CanCreateDesks()) {
    std::move(callback).Run(DeskActionError::kDesksCountCheckFailedError, {});
    return;
  }

  // We prioritize `customized_desk_name` over the saved desk's name. An example
  // is that for call center application use case, we launch the same template
  // for different customer and assign desk name to be customer's name.
  const auto& template_name = customized_desk_name.empty()
                                  ? saved_desk->template_name()
                                  : customized_desk_name;

  const ash::Desk* new_desk = desks_controller_->CreateNewDeskForSavedDesk(
      saved_desk->type(), template_name);

  if (!saved_desk->desk_restore_data()) {
    std::move(callback).Run(DeskActionError::kUnknownError, {});
    return;
  }

  // Copy the index of the newly created desk to the saved desk. This ensures
  // that apps appear on the right desk even if the user switches to another.
  saved_desk->SetDeskIndex(desks_controller_->GetDeskIndex(new_desk));

  const auto saved_desk_type = saved_desk->type();
  const auto uuid = saved_desk->uuid();

  // Launch the windows as specified in the saved desk to a new desk.
  LaunchAppsFromTemplate(std::move(saved_desk));
  if (saved_desk_type == ash::DeskTemplateType::kSaveAndRecall) {
    GetDeskModel()->DeleteEntry(
        uuid, base::BindOnce(&DesksClient::OnRecallSavedDesk,
                             weak_ptr_factory_.GetWeakPtr(),
                             std::move(callback), new_desk->uuid()));
  } else {
    std::move(callback).Run(absl::nullopt, new_desk->uuid());
  }
}

void DesksClient::OnCaptureActiveDeskAndSaveTemplate(
    DesksClient::CaptureActiveDeskAndSaveTemplateCallback callback,
    desks_storage::DeskModel::AddOrUpdateEntryStatus status,
    std::unique_ptr<ash::DeskTemplate> desk_template) {
  if (status != desks_storage::DeskModel::AddOrUpdateEntryStatus::kOk) {
    std::move(callback).Run(DeskActionError::kUnknownError,
                            std::move(desk_template));
    return;
  }
  const auto saved_desk_type = desk_template->type();

  if (saved_desk_type == ash::DeskTemplateType::kSaveAndRecall) {
    // TODO(aprilzhou): Now it's the exact copy from ash/wm/desks/templates/
    // saved_desk_presenter.cc:Showlibrary. Should be removed once we provide
    // API to manage saved desk name.
    auto* overview_controller = ash::Shell::Get()->overview_controller();
    auto* overview_session = overview_controller->overview_session();
    if (!overview_session) {
      if (!overview_controller->StartOverview(
              ash::OverviewStartAction::kDevTools,
              ash::OverviewEnterExitType::kImmediateEnterWithoutFocus)) {
        // If for whatever reason we didn't enter overview mode, bail.
        std::move(callback).Run(absl::nullopt, std::move(desk_template));
        return;
      }
      overview_session = overview_controller->overview_session();
      DCHECK(overview_session);
    }

    overview_session->ShowSavedDeskLibrary(desk_template->uuid(),
                                           desk_template->template_name(),
                                           ash::Shell::GetPrimaryRootWindow());

    // We have successfully created a *new* desk template for Save & Recall,
    // so we are now going to close all the windows on the active desk and
    // also remove the desk.
    auto* active_desk = desks_controller_->active_desk();

    // If this is the only desk, we have to create a new desk before we can
    // remove the current one.
    if (!desks_controller_->CanRemoveDesks())
      desks_controller_->NewDesk(
          ash::DesksCreationRemovalSource::kEnsureDefaultDesk);

    // Remove the current desk, this will be done without animation.
    desks_controller_->RemoveDesk(
        active_desk, ash::DesksCreationRemovalSource::kSaveAndRecall,
        ash::DeskCloseType::kCloseAllWindows);
  }

  std::move(callback).Run(absl::nullopt, std::move(desk_template));
}

void DesksClient::OnDeleteDeskTemplate(
    DesksClient::DeleteDeskTemplateCallback callback,
    desks_storage::DeskModel::DeleteEntryStatus status) {
  std::move(callback).Run(
      status != desks_storage::DeskModel::DeleteEntryStatus::kOk
          ? absl::make_optional(DeskActionError::kNoCurrentUserError)
          : absl::nullopt);
}

void DesksClient::OnRecallSavedDesk(
    DesksClient::LaunchDeskCallback callback,
    const base::GUID& desk_id,
    desks_storage::DeskModel::DeleteEntryStatus status) {
  std::move(callback).Run(
      status != desks_storage::DeskModel::DeleteEntryStatus::kOk
          ? absl::make_optional(DeskActionError::kNoCurrentUserError)
          : absl::nullopt,
      desk_id);
}

void DesksClient::OnCapturedDeskTemplate(
    CaptureActiveDeskAndSaveTemplateCallback callback,
    std::unique_ptr<ash::DeskTemplate> desk_template) {
  if (!desk_template) {
    std::move(callback).Run(DeskActionError::kUnknownError, {});
    return;
  }

  // Let FloatingWorkspaceService handle the save when the template type is
  // FloatingWorkspace.
  if (desk_template->type() == ash::DeskTemplateType::kFloatingWorkspace) {
    std::move(callback).Run(absl::nullopt, std::move(desk_template));
    return;
  }

  GetDeskModel()->AddOrUpdateEntry(
      std::move(desk_template),
      base::BindOnce(&DesksClient::OnCaptureActiveDeskAndSaveTemplate,
                     weak_ptr_factory_.GetWeakPtr(), std::move(callback)));
}

void DesksClient::OnGetTemplateJson(
    DesksClient::GetTemplateJsonCallback callback,
    desks_storage::DeskModel::GetTemplateJsonStatus status,
    const base::Value& json_representation) {
  std::move(callback).Run(
      status != desks_storage::DeskModel::GetTemplateJsonStatus::kOk
          ? absl::make_optional(DeskActionError::kStorageError)
          : absl::nullopt,
      json_representation);
}

void DesksClient::OnLaunchComplete(int32_t launch_id) {
  app_launch_handlers_.erase(launch_id);
}

void DesksClient::RemoveLaunchPerformanceTracker(base::GUID tracker_uuid) {
  template_ids_to_launch_performance_trackers_.erase(tracker_uuid);
}

aura::Window* DesksClient::GetWindowByBrowserSessionId(
    SessionID browser_session_id) {
  for (auto* browser : *BrowserList::GetInstance()) {
    if (browser->session_id() == browser_session_id)
      return browser->window()->GetNativeWindow();
  }
  return nullptr;
}

const ash::Desk* DesksClient::CreateEmptyDeskAndActivate(
    const std::u16string& customized_desk_name) {
  DCHECK(desks_controller_->CanCreateDesks());

  // If there is an ongoing animation, we should stop it before creating and
  // activating the new desk, which triggers its own animation.
  desks_controller_->ResetAnimation();

  // Desk name was set to a default name upon creation. If
  // `customized_desk_name` is provided, override desk name to be
  // `customized_desk_name` or `customized_desk_name ({counter})` to resolve
  // naming conflicts.
  std::u16string desk_name =
      desks_controller_->CreateUniqueDeskName(customized_desk_name);

  desks_controller_->NewDesk(ash::DesksCreationRemovalSource::kApi);
  ash::Desk* desk = desks_controller_->desks().back().get();

  if (!desk_name.empty()) {
    desk->SetName(desk_name, /*set_by_user=*/true);
    ash::Shell::Get()
        ->accessibility_controller()
        ->TriggerAccessibilityAlertWithMessage(l10n_util::GetStringFUTF8(
            IDS_ASH_VIRTUAL_DESKS_ALERT_NEW_DESK_CREATED, desk_name));
  }

  // Force update user prefs because `SetName()` does not trigger it.
  ash::desks_restore_util::UpdatePrimaryUserDeskNamesPrefs();

  desks_controller_->ActivateDesk(desk, ash::DesksSwitchSource::kApiLaunch);

  return desk;
}
