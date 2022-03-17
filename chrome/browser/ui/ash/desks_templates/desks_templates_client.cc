// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/ash/desks_templates/desks_templates_client.h"

#include <memory>

#include "ash/constants/ash_features.h"
#include "ash/public/cpp/desk_template.h"
#include "ash/public/cpp/session/session_controller.h"
#include "ash/public/cpp/session/session_observer.h"
#include "ash/wm/desks/desks_controller.h"
#include "base/bind.h"
#include "base/callback_helpers.h"
#include "base/guid.h"
#include "base/metrics/histogram_functions.h"
#include "base/metrics/histogram_macros.h"
#include "base/scoped_observation.h"
#include "base/timer/timer.h"
#include "chrome/browser/apps/app_service/app_service_proxy.h"
#include "chrome/browser/apps/app_service/app_service_proxy_factory.h"
#include "chrome/browser/ash/profiles/profile_helper.h"
#include "chrome/browser/profiles/profile_manager.h"
#include "chrome/browser/sync/desk_sync_service_factory.h"
#include "chrome/browser/ui/ash/desks_templates/desks_templates_app_launch_handler.h"
#include "components/app_constants/constants.h"
#include "components/app_restore/full_restore_info.h"
#include "components/app_restore/window_properties.h"
#include "components/desks_storage/core/desk_sync_service.h"
#include "components/desks_storage/core/local_desk_data_manager.h"
#include "components/sync/model/model_type_store.h"
#include "ui/views/widget/widget.h"

namespace {

DesksTemplatesClient* g_desks_templates_client_instance = nullptr;

// TODO(https://crbug.com/1284774): Remove metrics from this file.
// Histogram names.
constexpr char kWindowCountHistogramName[] = "Ash.DeskTemplate.WindowCount";
constexpr char kTabCountHistogramName[] = "Ash.DeskTemplate.TabCount";
constexpr char kWindowAndTabCountHistogramName[] =
    "Ash.DeskTemplate.WindowAndTabCount";
constexpr char kLaunchFromTemplateHistogramName[] =
    "Ash.DeskTemplate.LaunchFromTemplate";
constexpr char kUserTemplateCountHistogramName[] =
    "Ash.DeskTemplate.UserTemplateCount";
constexpr char kTimeToLoadTemplateHistogramName[] =
    "Ash.DeskTemplate.TimeToLoadTemplate";

// Error strings
constexpr char kMaximumDesksOpenedError[] =
    "The maximum number of desks is already open.";
constexpr char kMissingTemplateDataError[] =
    "The desk template has invalid or missing data.";
constexpr char kStorageError[] = "The operation failed due to a storage error.";
constexpr char kNoCurrentUserError[] = "There is no active profile.";
constexpr char kBadProfileError[] =
    "Either the profile is not valid or there is not an active proflile.";
constexpr char kNoSavedTemplatesError[] = "You can create up to 6 templates.";

// Timeout time used in LaunchPerformanceTracker
constexpr base::TimeDelta kLaunchPerformanceTimeout = base::Minutes(3);

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
class DesksTemplatesClient::LaunchPerformanceTracker
    : public full_restore::FullRestoreInfo::Observer {
 public:
  LaunchPerformanceTracker(base::Time time_launch_started,
                           const std::set<int>& window_ids,
                           base::GUID template_id,
                           DesksTemplatesClient* templates_client)
      : tracked_window_ids_(window_ids),
        time_launch_started_(time_launch_started),
        template_id_(template_id),
        templates_client_(templates_client) {
    scoped_observation_.Observe(full_restore::FullRestoreInfo::GetInstance());
    timeout_timer_ = std::make_unique<base::OneShotTimer>();
    timeout_timer_->Start(
        FROM_HERE, kLaunchPerformanceTimeout,
        base::BindOnce(
            &DesksTemplatesClient::LaunchPerformanceTracker::OnTimeout,
            weak_ptr_factory_.GetWeakPtr()));
  }

  LaunchPerformanceTracker(const LaunchPerformanceTracker&) = delete;
  LaunchPerformanceTracker& operator=(const LaunchPerformanceTracker&) = delete;
  ~LaunchPerformanceTracker() override {}

  // Removes window ID from tracked set because the window has been launched.
  // full_restore::FullRestoreInfo::Observer
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
  DesksTemplatesClient* templates_client_;

  base::ScopedObservation<full_restore::FullRestoreInfo,
                          full_restore::FullRestoreInfo::Observer>
      scoped_observation_{this};
  base::WeakPtrFactory<LaunchPerformanceTracker> weak_ptr_factory_{this};
};

DesksTemplatesClient::DesksTemplatesClient()
    : desks_controller_(ash::DesksController::Get()) {
  DCHECK(!g_desks_templates_client_instance);
  g_desks_templates_client_instance = this;
  ash::SessionController::Get()->AddObserver(this);
}

DesksTemplatesClient::~DesksTemplatesClient() {
  DCHECK_EQ(this, g_desks_templates_client_instance);
  g_desks_templates_client_instance = nullptr;
  ash::SessionController::Get()->RemoveObserver(this);
}

// static
DesksTemplatesClient* DesksTemplatesClient::Get() {
  return g_desks_templates_client_instance;
}

void DesksTemplatesClient::OnActiveUserSessionChanged(
    const AccountId& account_id) {
  Profile* profile =
      ash::ProfileHelper::Get()->GetProfileByAccountId(account_id);
  if (profile == active_profile_ || !IsSupportedProfile(profile))
    return;

  active_profile_ = profile;
  DCHECK(active_profile_);

  if (!chromeos::features::IsDeskTemplateSyncEnabled()) {
    storage_manager_ = std::make_unique<desks_storage::LocalDeskDataManager>(
        active_profile_->GetPath());
  }

  auto policy_desk_templates_it =
      preconfigured_desk_templates_json_.find(account_id);
  if (policy_desk_templates_it != preconfigured_desk_templates_json_.end())
    GetDeskModel()->SetPolicyDeskTemplates(policy_desk_templates_it->second);
}

void DesksTemplatesClient::CaptureActiveDeskAndSaveTemplate(
    CaptureActiveDeskAndSaveTemplateCallback callback) {
  if (!active_profile_) {
    std::move(callback).Run(/*desk_template=*/nullptr,
                            std::string(kNoCurrentUserError));
    return;
  }

  desks_controller_->CaptureActiveDeskAsTemplate(
      base::BindOnce(&DesksTemplatesClient::OnCapturedDeskTemplate,
                     weak_ptr_factory_.GetWeakPtr(), std::move(callback)));
}

void DesksTemplatesClient::UpdateDeskTemplate(
    const std::string& template_uuid,
    const std::u16string& template_name,
    UpdateDeskTemplateCallback callback) {
  if (!active_profile_) {
    std::move(callback).Run(std::string(kNoCurrentUserError));
    return;
  }

  GetDeskModel()->GetEntryByUUID(
      template_uuid,
      base::BindOnce(&DesksTemplatesClient::OnGetTemplateToBeUpdated,
                     weak_ptr_factory_.GetWeakPtr(), template_name,
                     std::move(callback)));
}

void DesksTemplatesClient::DeleteDeskTemplate(
    const std::string& template_uuid,
    DeleteDeskTemplateCallback callback) {
  if (!active_profile_) {
    std::move(callback).Run(std::string(kNoCurrentUserError));
    return;
  }

  GetDeskModel()->DeleteEntry(
      template_uuid,
      base::BindOnce(&DesksTemplatesClient::OnDeleteDeskTemplate,
                     weak_ptr_factory_.GetWeakPtr(), std::move(callback)));
}

void DesksTemplatesClient::GetDeskTemplates(GetDeskTemplatesCallback callback) {
  if (!active_profile_) {
    std::move(callback).Run(/*desk_templates=*/{},
                            std::string(kNoCurrentUserError));
    return;
  }

  GetDeskModel()->GetAllEntries(
      base::BindOnce(&DesksTemplatesClient::OnGetAllTemplates,
                     weak_ptr_factory_.GetWeakPtr(), std::move(callback)));
}

void DesksTemplatesClient::GetTemplateJson(const std::string uuid,
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
      base::BindOnce(&DesksTemplatesClient::OnGetTemplateJson,
                     weak_ptr_factory_.GetWeakPtr(), std::move(callback)));
}

void DesksTemplatesClient::LaunchDeskTemplate(
    const std::string& template_uuid,
    LaunchDeskTemplateCallback callback) {
  base::Time launch_started = base::Time::Now();

  if (!active_profile_) {
    std::move(callback).Run(std::string(kNoCurrentUserError));
    return;
  }

  if (launch_template_for_test_) {
    OnGetTemplateForDeskLaunch(
        std::move(callback), base::Time(),
        desks_storage::DeskModel::GetEntryByUuidStatus::kOk,
        launch_template_for_test_->Clone());
    return;
  }

  GetDeskModel()->GetEntryByUUID(
      template_uuid,
      base::BindOnce(&DesksTemplatesClient::OnGetTemplateForDeskLaunch,
                     weak_ptr_factory_.GetWeakPtr(), std::move(callback),
                     launch_started));
}

void DesksTemplatesClient::LaunchAppsFromTemplate(
    std::unique_ptr<ash::DeskTemplate> desk_template,
    base::Time time_launch_started,
    base::TimeDelta delay) {
  DCHECK(desk_template);
  DCHECK_GT(desk_template->launch_id(), 0);

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
      base::BindOnce(&DesksTemplatesClient::OnLaunchComplete,
                     weak_ptr_factory_.GetWeakPtr(), launch_id),
      kClearLaunchDataDuration);
}

desks_storage::DeskModel* DesksTemplatesClient::GetDeskModel() {
  if (chromeos::features::IsDeskTemplateSyncEnabled()) {
    return DeskSyncServiceFactory::GetForProfile(active_profile_)
        ->GetDeskModel();
  }

  DCHECK(storage_manager_.get());
  return storage_manager_.get();
}

// Sets the preconfigured desk template. Data contains the contents of the JSON
// file with the template information
void DesksTemplatesClient::SetPolicyPreconfiguredTemplate(
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

void DesksTemplatesClient::RemovePolicyPreconfiguredTemplate(
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

void DesksTemplatesClient::NotifyMovedSingleInstanceApp(int32_t window_id) {
  for (auto& id_to_tracker : template_ids_to_launch_performance_trackers_)
    id_to_tracker.second->OnMovedSingleInstanceApp(window_id);
}

void DesksTemplatesClient::RecordWindowAndTabCountHistogram(
    ash::DeskTemplate* desk_template) {
  const app_restore::RestoreData* restore_data =
      desk_template->desk_restore_data();
  DCHECK(restore_data);

  int window_count = 0;
  int tab_count = 0;
  int total_count = 0;

  const auto& launch_list = restore_data->app_id_to_launch_list();
  for (const auto& iter : launch_list) {
    // Since apps aren't guaranteed to have the url field set up correctly, this
    // is necessary to ensure things are not double-counted.
    if (iter.first != app_constants::kChromeAppId) {
      ++window_count;
      ++total_count;
      continue;
    }

    for (const auto& window_iter : iter.second) {
      absl::optional<std::vector<GURL>> urls = window_iter.second->urls;
      if (!urls || urls->empty())
        continue;

      ++window_count;
      tab_count += urls->size();
      total_count += urls->size();
    }
  }

  base::UmaHistogramCounts100(kWindowCountHistogramName, window_count);
  base::UmaHistogramCounts100(kTabCountHistogramName, tab_count);
  base::UmaHistogramCounts100(kWindowAndTabCountHistogramName, total_count);
}

void DesksTemplatesClient::RecordLaunchFromTemplateHistogram() {
  base::UmaHistogramBoolean(kLaunchFromTemplateHistogramName, true);
}

void DesksTemplatesClient::RecordTemplateCountHistogram() {
  UMA_HISTOGRAM_EXACT_LINEAR(kUserTemplateCountHistogramName,
                             GetDeskModel()->GetEntryCount(),
                             GetDeskModel()->GetMaxEntryCount());
}

void DesksTemplatesClient::OnGetTemplateForDeskLaunch(
    LaunchDeskTemplateCallback callback,
    base::Time time_launch_started,
    desks_storage::DeskModel::GetEntryByUuidStatus status,
    std::unique_ptr<ash::DeskTemplate> entry) {
  if (status != desks_storage::DeskModel::GetEntryByUuidStatus::kOk) {
    std::move(callback).Run(std::string(kStorageError));
    return;
  }

  RecordLaunchFromTemplateHistogram();

  // Launch the windows as specified in the template to a new desk.
  const auto template_name = entry->template_name();
  desks_controller_->CreateAndActivateNewDeskForTemplate(
      template_name,
      base::BindOnce(&DesksTemplatesClient::OnCreateAndActivateNewDesk,
                     weak_ptr_factory_.GetWeakPtr(), std::move(entry),
                     std::move(callback), time_launch_started));
}

void DesksTemplatesClient::OnCreateAndActivateNewDesk(
    std::unique_ptr<ash::DeskTemplate> desk_template,
    LaunchDeskTemplateCallback callback,
    base::Time time_launch_started,
    bool on_create_activate_success) {
  if (!on_create_activate_success) {
    // This only returns false if the number of desks is at a maximum.
    std::move(callback).Run(std::string(kMaximumDesksOpenedError));
    return;
  }

  DCHECK(desk_template);
  if (!desk_template->desk_restore_data()) {
    std::move(callback).Run(std::string(kMissingTemplateDataError));
    return;
  }

  LaunchAppsFromTemplate(std::move(desk_template), time_launch_started,
                         base::TimeDelta());
  std::move(callback).Run(std::string(""));
}

void DesksTemplatesClient::OnCaptureActiveDeskAndSaveTemplate(
    DesksTemplatesClient::CaptureActiveDeskAndSaveTemplateCallback callback,
    std::unique_ptr<ash::DeskTemplate> desk_template,
    desks_storage::DeskModel::AddOrUpdateEntryStatus status) {
  std::move(callback).Run(
      std::move(desk_template),
      std::string(status !=
                          desks_storage::DeskModel::AddOrUpdateEntryStatus::kOk
                      ? kNoSavedTemplatesError
                      : ""));

  RecordTemplateCountHistogram();
}

void DesksTemplatesClient::OnDeleteDeskTemplate(
    DesksTemplatesClient::DeleteDeskTemplateCallback callback,
    desks_storage::DeskModel::DeleteEntryStatus status) {
  std::move(callback).Run(
      std::string(status != desks_storage::DeskModel::DeleteEntryStatus::kOk
                      ? kNoCurrentUserError
                      : ""));
  RecordTemplateCountHistogram();
}

void DesksTemplatesClient::OnUpdateDeskTemplate(
    DesksTemplatesClient::UpdateDeskTemplateCallback callback,
    desks_storage::DeskModel::AddOrUpdateEntryStatus status) {
  std::move(callback).Run(std::string(
      status != desks_storage::DeskModel::AddOrUpdateEntryStatus::kOk
          ? kStorageError
          : ""));
}

void DesksTemplatesClient::OnGetTemplateToBeUpdated(
    const std::u16string& template_name,
    DesksTemplatesClient::UpdateDeskTemplateCallback callback,
    desks_storage::DeskModel::GetEntryByUuidStatus status,
    std::unique_ptr<ash::DeskTemplate> entry) {
  if (status != desks_storage::DeskModel::GetEntryByUuidStatus::kOk) {
    std::move(callback).Run(std::string(kStorageError));
    return;
  }

  entry->set_template_name(template_name);
  GetDeskModel()->AddOrUpdateEntry(
      std::move(entry),
      base::BindOnce(&DesksTemplatesClient::OnUpdateDeskTemplate,
                     weak_ptr_factory_.GetWeakPtr(), std::move(callback)));
}

void DesksTemplatesClient::OnGetAllTemplates(
    GetDeskTemplatesCallback callback,
    desks_storage::DeskModel::GetAllEntriesStatus status,
    const std::vector<ash::DeskTemplate*>& entries) {
  std::move(callback).Run(
      entries,
      std::string(status != desks_storage::DeskModel::GetAllEntriesStatus::kOk
                      ? kStorageError
                      : ""));
}

void DesksTemplatesClient::OnCapturedDeskTemplate(
    CaptureActiveDeskAndSaveTemplateCallback callback,
    std::unique_ptr<ash::DeskTemplate> desk_template) {
  if (!desk_template)
    return;

  RecordWindowAndTabCountHistogram(desk_template.get());
  auto desk_template_clone = desk_template->Clone();
  GetDeskModel()->AddOrUpdateEntry(
      std::move(desk_template_clone),
      base::BindOnce(&DesksTemplatesClient::OnCaptureActiveDeskAndSaveTemplate,
                     weak_ptr_factory_.GetWeakPtr(), std::move(callback),
                     std::move(desk_template)));
}

void DesksTemplatesClient::OnGetTemplateJson(
    DesksTemplatesClient::GetTemplateJsonCallback callback,
    desks_storage::DeskModel::GetTemplateJsonStatus status,
    const std::string& json_representation) {
  std::move(callback).Run(
      json_representation,
      std::string(status != desks_storage::DeskModel::GetTemplateJsonStatus::kOk
                      ? kStorageError
                      : ""));
}

void DesksTemplatesClient::OnLaunchComplete(int32_t launch_id) {
  app_launch_handlers_.erase(launch_id);
}

void DesksTemplatesClient::RemoveLaunchPerformanceTracker(
    base::GUID tracker_uuid) {
  template_ids_to_launch_performance_trackers_.erase(tracker_uuid);
}
