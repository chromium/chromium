// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/ash/desks/desks_client.h"

#include <memory>

#include "ash/constants/ash_features.h"
#include "ash/public/cpp/desk_template.h"
#include "ash/public/cpp/session/session_controller.h"
#include "ash/wm/desks/desk.h"
#include "ash/wm/desks/desks_controller.h"
#include "ash/wm/desks/templates/saved_desk_util.h"
#include "base/bind.h"
#include "base/callback_helpers.h"
#include "base/guid.h"
#include "base/metrics/histogram_functions.h"
#include "base/scoped_observation.h"
#include "base/timer/timer.h"
#include "chrome/browser/apps/app_service/app_service_proxy.h"
#include "chrome/browser/apps/app_service/app_service_proxy_factory.h"
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
#include "ui/views/widget/widget.h"

namespace {

DesksClient* g_desks_client_instance = nullptr;

// Used to generate unique IDs for desk template launches.
int32_t g_launch_id = 0;

// Error strings for the private API.
constexpr char kMaximumDesksOpenedError[] =
    "The maximum number of desks is already open.";
constexpr char kMissingTemplateDataError[] =
    "The desk template has invalid or missing data.";
constexpr char kStorageError[] = "Storage error.";
constexpr char kNoCurrentUserError[] = "There is no active profile.";
constexpr char kBadProfileError[] =
    "Either the profile is not valid or there is not an active proflile.";
constexpr char kNoSavedTemplatesError[] = "You can create up to 6 templates.";
constexpr char kNoSuchDeskError[] = "The desk cannot be found.";
constexpr char kInvalidDeskIdError[] = "The desk identifier is not valid.";
constexpr char kCantCloseDeskError[] = "The desk cannot be closed.";
constexpr char kCantGetAllDesksError[] = "Unable to retrieve all desks.";
constexpr char kNoSuchWindowError[] = "The window cannot be found.";
constexpr char kInvalidWindowIdError[] = "The window identifier is not valid.";

// Timeout time used in LaunchPerformanceTracker.
constexpr base::TimeDelta kLaunchPerformanceTimeout = base::Minutes(3);

// Histogram name to track estimated time it takes to load a template. Used by
// LaunchPerformanceTracker. Note that this is in a different spot than the
// other metrics because the class that uses this is owned by `this`.
constexpr char kTimeToLoadTemplateHistogramName[] =
    "Ash.DeskTemplate.TimeToLoadTemplate";

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

// Tracks a set of WindowIDs through the launching process, records a
// launch performance metric when the set of window_ids have all been
// launched
class DesksClient::LaunchPerformanceTracker
    : public app_restore::AppRestoreInfo::Observer {
 public:
  LaunchPerformanceTracker(base::Time time_launch_started,
                           const std::set<int>& window_ids,
                           base::GUID template_id,
                           DesksClient* templates_client)
      : tracked_window_ids_(window_ids),
        time_launch_started_(time_launch_started),
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
  ash::SessionController::Get()->AddObserver(this);
}

DesksClient::~DesksClient() {
  DCHECK_EQ(this, g_desks_client_instance);
  g_desks_client_instance = nullptr;
  ash::SessionController::Get()->RemoveObserver(this);
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

  active_profile_ = profile;
  DCHECK(active_profile_);

  if (chromeos::features::IsSavedDesksEnabled()) {
    save_and_recall_desks_storage_manager_ =
        std::make_unique<desks_storage::LocalDeskDataManager>(
            active_profile_->GetPath(), account_id);

    if (ash::saved_desk_util::AreDesksTemplatesEnabled() &&
        chromeos::features::IsDeskTemplateSyncEnabled()) {
      saved_desk_storage_manager_ =
          std::make_unique<desks_storage::DeskModelWrapper>(
              save_and_recall_desks_storage_manager_.get());
    }

  } else {
    if (!chromeos::features::IsDeskTemplateSyncEnabled()) {
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

void DesksClient::CaptureActiveDeskAndSaveTemplate(
    CaptureActiveDeskAndSaveTemplateCallback callback) {
  if (!active_profile_) {
    std::move(callback).Run(/*desk_template=*/nullptr, kNoCurrentUserError);
    return;
  }

  desks_controller_->CaptureActiveDeskAsTemplate(
      base::BindOnce(&DesksClient::OnCapturedDeskTemplate,
                     weak_ptr_factory_.GetWeakPtr(), std::move(callback)),
      ash::DeskTemplateType::kTemplate,
      /*root_window_to_show=*/nullptr);
}

void DesksClient::UpdateDeskTemplate(const std::string& template_uuid,
                                     const std::u16string& template_name,
                                     UpdateDeskTemplateCallback callback) {
  if (!active_profile_) {
    std::move(callback).Run(kNoCurrentUserError);
    return;
  }

  GetDeskModel()->GetEntryByUUID(
      template_uuid, base::BindOnce(&DesksClient::OnGetTemplateToBeUpdated,
                                    weak_ptr_factory_.GetWeakPtr(),
                                    template_name, std::move(callback)));
}

void DesksClient::DeleteDeskTemplate(const std::string& template_uuid,
                                     DeleteDeskTemplateCallback callback) {
  if (!active_profile_) {
    std::move(callback).Run(std::string(kNoCurrentUserError));
    return;
  }

  GetDeskModel()->DeleteEntry(
      template_uuid,
      base::BindOnce(&DesksClient::OnDeleteDeskTemplate,
                     weak_ptr_factory_.GetWeakPtr(), std::move(callback)));
}

void DesksClient::GetDeskTemplates(GetDeskTemplatesCallback callback) {
  if (!active_profile_) {
    std::move(callback).Run(/*desk_templates=*/{},
                            std::string(kNoCurrentUserError));
    return;
  }

  auto result = GetDeskModel()->GetAllEntries();

  std::move(callback).Run(
      result.entries,
      std::string(result.status !=
                          desks_storage::DeskModel::GetAllEntriesStatus::kOk
                      ? kStorageError
                      : ""));
}

void DesksClient::GetTemplateJson(const std::string uuid,
                                  Profile* profile,
                                  GetTemplateJsonCallback callback) {
  if (!active_profile_ || active_profile_ != profile) {
    std::move(callback).Run(/*desk_templates=*/{},
                            std::string(kBadProfileError));
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
    const std::string& template_uuid,
    LaunchDeskCallback callback,
    const std::u16string& customized_desk_name) {
  base::Time launch_started = base::Time::Now();

  if (!active_profile_) {
    std::move(callback).Run(std::string(kNoCurrentUserError), {});
    return;
  }

  if (launch_template_for_test_) {
    OnGetTemplateForDeskLaunch(
        std::move(callback), customized_desk_name, base::Time(),
        desks_storage::DeskModel::GetEntryByUuidStatus::kOk,
        launch_template_for_test_->Clone());
    return;
  }

  GetDeskModel()->GetEntryByUUID(
      template_uuid,
      base::BindOnce(&DesksClient::OnGetTemplateForDeskLaunch,
                     weak_ptr_factory_.GetWeakPtr(), std::move(callback),
                     customized_desk_name, launch_started));
}

void DesksClient::LaunchEmptyDesk(LaunchDeskCallback callback,
                                  const std::u16string& customized_desk_name) {
  if (!desks_controller_->CanCreateDesks()) {
    std::move(callback).Run(kMaximumDesksOpenedError, {});
    return;
  }

  const ash::Desk* new_desk = desks_controller_->CreateNewDeskForTemplate(
      /*activate_desk=*/true, customized_desk_name);
  std::move(callback).Run(/*error=*/"", new_desk->uuid());
}

void DesksClient::RemoveDesk(const base::GUID& desk_uuid,
                             bool combine_desk,
                             ErrorHandlingCallBack callback) {
  // Return error if `desk_uuid` is invalid.
  if (!desk_uuid.is_valid()) {
    std::move(callback).Run(kInvalidDeskIdError);
    return;
  }

  ash::Desk* desk = desks_controller_->GetDeskByUuid(desk_uuid);
  // Can't clean up desk when desk identifier is incorrect.
  if (!desk) {
    std::move(callback).Run(kNoSuchDeskError);
    return;
  }

  // Can't clean up desk when there is no more than 1 desk left.
  if (desks_controller_->CanRemoveDesks()) {
    desks_controller_->RemoveDesk(desk, ash::DesksCreationRemovalSource::kApi,
                                  combine_desk
                                      ? ash::DeskCloseType::kCombineDesks
                                      : ash::DeskCloseType::kCloseAllWindows);
  } else {
    std::move(callback).Run(kCantCloseDeskError);
    return;
  }

  std::move(callback).Run(/*error=*/"");
}

void DesksClient::GetAllDesks(GetAllDesksCallback callback) {
  std::vector<const ash::Desk*> desks;
  desks_controller_->GetAllDesks(desks);
  // There should be at least one default desk.
  std::move(callback).Run(desks, desks.empty() ? kCantGetAllDesksError : "");
}

void DesksClient::LaunchAppsFromTemplate(
    std::unique_ptr<ash::DeskTemplate> desk_template,
    base::Time time_launch_started,
    base::TimeDelta delay) {
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

  // Make window IDs of the template unique. This is a requirement for launching
  // templates concurrently since the contained window IDs are used as lookup
  // keys in many places. We must also do this *before* creating the performance
  // tracker below.
  restore_data->MakeWindowIdsUniqueForDeskTemplate();

  template_ids_to_launch_performance_trackers_[desk_template->uuid()] =
      std::make_unique<LaunchPerformanceTracker>(
          time_launch_started, GetWindowIDSetFromTemplate(desk_template.get()),
          desk_template->uuid(), this);

  DCHECK(active_profile_);
  const int32_t launch_id = desk_template->launch_id();

  auto& handler = app_launch_handlers_[launch_id];
  // Some tests reach into this class and install a handler ahead of time. In
  // all other cases, we create a handler for the launch here.
  if (!handler)
    handler = std::make_unique<DesksTemplatesAppLaunchHandler>(active_profile_);

  handler->set_delay(delay);
  handler->LaunchTemplate(*desk_template);

  // Install a timer that will clear the launch handler after a given duration.
  base::ThreadTaskRunnerHandle::Get()->PostDelayedTask(
      FROM_HERE,
      base::BindOnce(&DesksClient::OnLaunchComplete,
                     weak_ptr_factory_.GetWeakPtr(), launch_id),
      kClearLaunchDataDuration);
}

desks_storage::DeskModel* DesksClient::GetDeskModel() {
  if (chromeos::features::IsSavedDesksEnabled()) {
    if (!ash::saved_desk_util::AreDesksTemplatesEnabled() ||
        !chromeos::features::IsDeskTemplateSyncEnabled()) {
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
    if (chromeos::features::IsDeskTemplateSyncEnabled()) {
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
    std::move(callback).Run(kInvalidWindowIdError);
    return;
  }

  aura::Window* window = GetWindowByBrowserSessionId(browser_session_id);
  if (!window) {
    std::move(callback).Run(kNoSuchWindowError);
    return;
  }
  window->SetProperty(aura::client::kWindowWorkspaceKey,
                      all_desk
                          ? aura::client::kWindowWorkspaceVisibleOnAllWorkspaces
                          : aura::client::kWindowWorkspaceUnassignedWorkspace);
  std::move(callback).Run("");
}

void DesksClient::OnGetTemplateForDeskLaunch(
    LaunchDeskCallback callback,
    std::u16string customized_desk_name,
    base::Time time_launch_started,
    desks_storage::DeskModel::GetEntryByUuidStatus status,
    std::unique_ptr<ash::DeskTemplate> saved_desk) {
  if (status != desks_storage::DeskModel::GetEntryByUuidStatus::kOk) {
    std::move(callback).Run(kStorageError, {});
    return;
  }
  if (!desks_controller_->CanCreateDesks()) {
    std::move(callback).Run(kMaximumDesksOpenedError, {});
    return;
  }

  // We prioritize `customized_desk_name` over the saved desk's name. An example
  // is that for call center application use case, we launch the same template
  // for different customer and assign desk name to be customer's name.
  const auto& template_name = customized_desk_name.empty()
                                  ? saved_desk->template_name()
                                  : customized_desk_name;
  const bool activate_desk =
      saved_desk->type() == ash::DeskTemplateType::kTemplate;
  const ash::Desk* new_desk =
      desks_controller_->CreateNewDeskForTemplate(activate_desk, template_name);

  if (!saved_desk->desk_restore_data()) {
    std::move(callback).Run(kMissingTemplateDataError, {});
    return;
  }

  // Copy the index of the newly created desk to the saved desk. This ensures
  // that apps appear on the right desk even if the user switches to another.
  saved_desk->SetDeskIndex(desks_controller_->GetDeskIndex(new_desk));

  // Launch the windows as specified in the saved desk to a new desk.
  LaunchAppsFromTemplate(std::move(saved_desk), time_launch_started,
                         base::TimeDelta());
  std::move(callback).Run("", new_desk->uuid());
}

void DesksClient::OnCaptureActiveDeskAndSaveTemplate(
    DesksClient::CaptureActiveDeskAndSaveTemplateCallback callback,
    std::unique_ptr<ash::DeskTemplate> desk_template,
    desks_storage::DeskModel::AddOrUpdateEntryStatus status) {
  std::move(callback).Run(
      std::move(desk_template),
      std::string(status !=
                          desks_storage::DeskModel::AddOrUpdateEntryStatus::kOk
                      ? kNoSavedTemplatesError
                      : ""));
}

void DesksClient::OnDeleteDeskTemplate(
    DesksClient::DeleteDeskTemplateCallback callback,
    desks_storage::DeskModel::DeleteEntryStatus status) {
  std::move(callback).Run(
      std::string(status != desks_storage::DeskModel::DeleteEntryStatus::kOk
                      ? kNoCurrentUserError
                      : ""));
}

void DesksClient::OnUpdateDeskTemplate(
    DesksClient::UpdateDeskTemplateCallback callback,
    desks_storage::DeskModel::AddOrUpdateEntryStatus status) {
  std::move(callback).Run(std::string(
      status != desks_storage::DeskModel::AddOrUpdateEntryStatus::kOk
          ? kStorageError
          : ""));
}

void DesksClient::OnGetTemplateToBeUpdated(
    const std::u16string& template_name,
    DesksClient::UpdateDeskTemplateCallback callback,
    desks_storage::DeskModel::GetEntryByUuidStatus status,
    std::unique_ptr<ash::DeskTemplate> entry) {
  if (status != desks_storage::DeskModel::GetEntryByUuidStatus::kOk) {
    std::move(callback).Run(kStorageError);
    return;
  }

  entry->set_template_name(template_name);
  GetDeskModel()->AddOrUpdateEntry(
      std::move(entry),
      base::BindOnce(&DesksClient::OnUpdateDeskTemplate,
                     weak_ptr_factory_.GetWeakPtr(), std::move(callback)));
}

void DesksClient::OnCapturedDeskTemplate(
    CaptureActiveDeskAndSaveTemplateCallback callback,
    std::unique_ptr<ash::DeskTemplate> desk_template) {
  if (!desk_template)
    return;

  auto desk_template_clone = desk_template->Clone();
  GetDeskModel()->AddOrUpdateEntry(
      std::move(desk_template_clone),
      base::BindOnce(&DesksClient::OnCaptureActiveDeskAndSaveTemplate,
                     weak_ptr_factory_.GetWeakPtr(), std::move(callback),
                     std::move(desk_template)));
}

void DesksClient::OnGetTemplateJson(
    DesksClient::GetTemplateJsonCallback callback,
    desks_storage::DeskModel::GetTemplateJsonStatus status,
    const std::string& json_representation) {
  std::move(callback).Run(
      json_representation,
      std::string(status != desks_storage::DeskModel::GetTemplateJsonStatus::kOk
                      ? kStorageError
                      : ""));
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
