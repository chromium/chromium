// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/feature_engagement/internal/tracker_impl.h"

#include <memory>
#include <optional>
#include <utility>

#include "base/check_is_test.h"
#include "base/command_line.h"
#include "base/feature_list.h"
#include "base/files/file_path.h"
#include "base/functional/bind.h"
#include "base/logging.h"
#include "base/metrics/field_trial_params.h"
#include "base/metrics/histogram_functions.h"
#include "base/metrics/user_metrics.h"
#include "base/no_destructor.h"
#include "base/synchronization/lock.h"
#include "base/task/sequenced_task_runner.h"
#include "base/task/single_thread_task_runner.h"
#include "base/time/clock.h"
#include "build/build_config.h"
#include "build/chromeos_buildflags.h"
#include "components/feature_engagement/internal/availability_model_impl.h"
#include "components/feature_engagement/internal/blocked_iph_features.h"
#include "components/feature_engagement/internal/chrome_variations_configuration.h"
#include "components/feature_engagement/internal/display_lock_controller_impl.h"
#include "components/feature_engagement/internal/editable_configuration.h"
#include "components/feature_engagement/internal/event_model.h"
#include "components/feature_engagement/internal/event_model_impl.h"
#include "components/feature_engagement/internal/feature_config_condition_validator.h"
#include "components/feature_engagement/internal/feature_config_event_storage_validator.h"
#include "components/feature_engagement/internal/in_memory_event_store.h"
#include "components/feature_engagement/internal/init_aware_event_model.h"
#include "components/feature_engagement/internal/never_availability_model.h"
#include "components/feature_engagement/internal/never_event_storage_validator.h"
#include "components/feature_engagement/internal/noop_display_lock_controller.h"
#include "components/feature_engagement/internal/once_condition_validator.h"
#include "components/feature_engagement/internal/persistent_event_store.h"
#include "components/feature_engagement/internal/proto/availability.pb.h"
#include "components/feature_engagement/internal/stats.h"
#include "components/feature_engagement/internal/system_time_provider.h"
#include "components/feature_engagement/internal/testing_clock_time_provider.h"
#include "components/feature_engagement/public/configuration.h"
#include "components/feature_engagement/public/feature_constants.h"
#include "components/feature_engagement/public/feature_list.h"
#include "components/feature_engagement/public/group_constants.h"
#include "components/feature_engagement/public/group_list.h"
#include "components/leveldb_proto/public/proto_database_provider.h"

namespace feature_engagement {

namespace {

const char kEventDBName[] = "EventDB";
const char kAvailabilityDBName[] = "AvailabilityDB";

#if !BUILDFLAG(IS_ANDROID)

// Reads event data from `config` and - if valid - places it into `result` along
// with the event count in the appropriate window.
void MaybeGetEventData(Tracker::EventList& result,
                       const EventConfig& config,
                       const EventModel& event_model,
                       uint32_t current_day) {
  if (config.name.empty()) {
    return;
  }
  result.emplace_back(std::make_pair(
      config,
      event_model.GetEventCount(config.name, current_day, config.window)));
}

#endif  // !BUILDFLAG(IS_ANDROID)

}  // namespace

std::unique_ptr<Tracker> CreateDemoModeTracker(
    std::string chosen_feature_name) {
  DVLOG(2) << "Enabling demo mode. Chosen feature: " << chosen_feature_name;

  std::unique_ptr<EditableConfiguration> configuration =
      std::make_unique<EditableConfiguration>();

  // Create valid configurations for all features to ensure that the
  // OnceConditionValidator acknowledges that thet meet conditions once.
  std::vector<const base::Feature*> features = GetAllFeatures();
  for (auto* feature : features) {
    // If a particular feature has been chosen to use with demo mode, only
    // mark that feature with a valid configuration.
    bool valid_config = chosen_feature_name.empty()
                            ? true
                            : chosen_feature_name == feature->name;

    FeatureConfig feature_config;
    feature_config.valid = valid_config;
    feature_config.trigger.name = feature->name + std::string("_trigger");
    configuration->SetConfiguration(feature, feature_config);
  }

  auto raw_event_model = std::make_unique<EventModelImpl>(
      std::make_unique<InMemoryEventStore>(),
      std::make_unique<NeverEventStorageValidator>());

  return std::make_unique<TrackerImpl>(
      std::make_unique<InitAwareEventModel>(std::move(raw_event_model)),
      std::make_unique<NeverAvailabilityModel>(), std::move(configuration),
      std::make_unique<NoopDisplayLockController>(),
      std::make_unique<OnceConditionValidator>(),
      std::make_unique<SystemTimeProvider>(), nullptr, nullptr);
}

// This method is declared in //components/feature_engagement/public/
//     feature_engagement.h
// and should be linked in to any binary using Tracker::Create.
// static
Tracker* Tracker::Create(
    const base::FilePath& storage_dir,
    const scoped_refptr<base::SequencedTaskRunner>& background_task_runner,
    leveldb_proto::ProtoDatabaseProvider* db_provider,
    std::unique_ptr<TrackerEventExporter> event_exporter,
    const ConfigurationProviderList& configuration_providers,
    std::unique_ptr<SessionController> session_controller) {
  DVLOG(2) << "Creating Tracker";
  if (base::FeatureList::IsEnabled(kIPHDemoMode)) {
    // GetFieldTrialParamValueByFeature returns an empty string if the param is
    // not set.
    std::string chosen_feature_name = base::GetFieldTrialParamValueByFeature(
        kIPHDemoMode, kIPHDemoModeFeatureChoiceParam);
    return CreateDemoModeTracker(chosen_feature_name).release();
  }

  base::FilePath event_storage_dir =
      storage_dir.AppendASCII(std::string(kEventDBName));
  auto event_db = db_provider->GetDB<Event>(
      leveldb_proto::ProtoDbType::FEATURE_ENGAGEMENT_EVENT, event_storage_dir,
      background_task_runner);

  auto event_store =
      std::make_unique<PersistentEventStore>(std::move(event_db));

  auto configuration = std::make_unique<ChromeVariationsConfiguration>();
  configuration->LoadConfigs(configuration_providers, GetAllFeatures(),
                             GetAllGroups());

  auto event_storage_validator =
      std::make_unique<FeatureConfigEventStorageValidator>();
  event_storage_validator->InitializeFeatures(GetAllFeatures(), GetAllGroups(),
                                              *configuration);

  auto raw_event_model = std::make_unique<EventModelImpl>(
      std::move(event_store), std::move(event_storage_validator));

  auto event_model =
      std::make_unique<InitAwareEventModel>(std::move(raw_event_model));
  auto condition_validator =
      std::make_unique<FeatureConfigConditionValidator>();
  auto time_provider = std::make_unique<SystemTimeProvider>();

  base::FilePath availability_storage_dir =
      storage_dir.AppendASCII(std::string(kAvailabilityDBName));
  auto availability_db = db_provider->GetDB<Availability>(
      leveldb_proto::ProtoDbType::FEATURE_ENGAGEMENT_AVAILABILITY,
      availability_storage_dir, background_task_runner);
  auto availability_store_loader = base::BindOnce(
      &PersistentAvailabilityStore::LoadAndUpdateStore,
      availability_storage_dir, std::move(availability_db), GetAllFeatures());

  auto availability_model = std::make_unique<AvailabilityModelImpl>(
      std::move(availability_store_loader));

  return new TrackerImpl(
      std::move(event_model), std::move(availability_model),
      std::move(configuration), std::make_unique<DisplayLockControllerImpl>(),
      std::move(condition_validator), std::move(time_provider),
      std::move(event_exporter), std::move(session_controller));
}

TrackerImpl::TrackerImpl(
    std::unique_ptr<EventModel> event_model,
    std::unique_ptr<AvailabilityModel> availability_model,
    std::unique_ptr<Configuration> configuration,
    std::unique_ptr<DisplayLockController> display_lock_controller,
    std::unique_ptr<ConditionValidator> condition_validator,
    std::unique_ptr<TimeProvider> time_provider,
    std::unique_ptr<TrackerEventExporter> event_exporter,
    std::unique_ptr<SessionController> session_controller)
    : event_model_(std::move(event_model)),
      availability_model_(std::move(availability_model)),
      configuration_(std::move(configuration)),
      display_lock_controller_(std::move(display_lock_controller)),
      condition_validator_(std::move(condition_validator)),
      time_provider_(std::move(time_provider)),
      event_exporter_(std::move(event_exporter)),
      session_controller_(std::move(session_controller)),
      event_model_initialization_finished_(false),
      availability_model_initialization_finished_(false) {
  event_model_->Initialize(
      base::BindOnce(&TrackerImpl::OnEventModelInitializationFinished,
                     weak_ptr_factory_.GetWeakPtr()),
      time_provider_->GetCurrentDay());

  availability_model_->Initialize(
      base::BindOnce(&TrackerImpl::OnAvailabilityModelInitializationFinished,
                     weak_ptr_factory_.GetWeakPtr()),
      time_provider_->GetCurrentDay());
}

TrackerImpl::~TrackerImpl() = default;

void TrackerImpl::NotifyEvent(const std::string& event) {
  event_model_->IncrementEvent(event, time_provider_->GetCurrentDay());
  stats::RecordNotifyEvent(event, configuration_.get(),
                           event_model_->IsReady());
}

#if !BUILDFLAG(IS_ANDROID)
void TrackerImpl::NotifyUsedEvent(const base::Feature& feature) {
  const auto& feature_config = configuration_->GetFeatureConfig(feature);
  if (!feature_config.used.name.empty()) {
    NotifyEvent(feature_config.used.name);
  }
}

void TrackerImpl::ClearEventData(const base::Feature& feature) {
  const auto& feature_config = configuration_->GetFeatureConfig(feature);
  if (!feature_config.trigger.name.empty()) {
    event_model_->ClearEvent(feature_config.trigger.name);
  }
  if (!feature_config.used.name.empty()) {
    event_model_->ClearEvent(feature_config.used.name);
  }
  for (const auto& event_config : feature_config.event_configs) {
    event_model_->ClearEvent(event_config.name);
  }
}

Tracker::EventList TrackerImpl::ListEvents(const base::Feature& feature) const {
  EventList result;
  if (!IsInitialized()) {
    return result;
  }
  const auto& feature_config = configuration_->GetFeatureConfig(feature);
  const auto current_day = time_provider_->GetCurrentDay();
  MaybeGetEventData(result, feature_config.trigger, *event_model_, current_day);
  MaybeGetEventData(result, feature_config.used, *event_model_, current_day);
  for (const auto& event_config : feature_config.event_configs) {
    MaybeGetEventData(result, event_config, *event_model_, current_day);
  }
  return result;
}

#endif  // !BUILDFLAG(IS_ANDROID)

bool TrackerImpl::ShouldTriggerHelpUI(const base::Feature& feature) {
  return ShouldTriggerHelpUIWithSnooze(feature).ShouldShowIph();
}

TrackerImpl::TriggerDetails TrackerImpl::ShouldTriggerHelpUIWithSnooze(
    const base::Feature& feature) {
  if (session_controller_ && session_controller_->ShouldResetSession()) {
    condition_validator_->ResetSession();
  }
  if (IsFeatureBlockedByTest(feature)) {
    return TriggerDetails(false, false);
  }

  FeatureConfig feature_config = configuration_->GetFeatureConfig(feature);
  std::vector<GroupConfig> group_configs;
  for (auto group : feature_config.groups) {
    group_configs.push_back(configuration_->GetGroupConfigByName(group));
  }
  ConditionValidator::Result result = condition_validator_->MeetsConditions(
      feature, feature_config, group_configs, *event_model_,
      *availability_model_, *display_lock_controller_, configuration_.get(),
      *time_provider_);
  if (result.NoErrors()) {
    condition_validator_->NotifyIsShowing(
        feature, feature_config, configuration_->GetRegisteredFeatures());

    DCHECK_NE("", feature_config.trigger.name);
    event_model_->IncrementEvent(feature_config.trigger.name,
                                 time_provider_->GetCurrentDay());

    for (auto group : feature_config.groups) {
      GroupConfig group_config = configuration_->GetGroupConfigByName(group);
      event_model_->IncrementEvent(group_config.trigger.name,
                                   time_provider_->GetCurrentDay());
    }
  }

  stats::RecordShouldTriggerHelpUI(feature, feature_config, result);
  DVLOG(2) << "Trigger result for " << feature.name
           << ": trigger=" << result.NoErrors()
           << " tracking_only=" << feature_config.tracking_only << " "
           << result;

  bool should_show_iph = false;

  if (feature_config.tracking_only) {
    if (result.NoErrors()) {
      // Because tracking only features always return false to the client,
      // clients do not have the context to correctly dismiss. The dismiss is
      // needed because our condition validator thinks the feature is currently
      // showing. See https://crbug.com/1188679 for more details.
      Dismissed(feature);
    }
    should_show_iph = false;
  } else {
    should_show_iph = result.NoErrors();
  }

  if (should_show_iph) {
    DCHECK(start_times_.find(feature.name) == start_times_.end());
    start_times_[feature.name] = time_provider_->Now();
  }

  return TriggerDetails(should_show_iph, result.should_show_snooze);
}

bool TrackerImpl::WouldTriggerHelpUI(const base::Feature& feature) const {
  if (session_controller_ && session_controller_->ShouldResetSession()) {
    condition_validator_->ResetSession();
  }

  if (IsFeatureBlockedByTest(feature)) {
    return false;
  }

  FeatureConfig feature_config = configuration_->GetFeatureConfig(feature);
  std::vector<GroupConfig> group_configs;
  for (auto group : feature_config.groups) {
    group_configs.push_back(configuration_->GetGroupConfigByName(group));
  }
  ConditionValidator::Result result = condition_validator_->MeetsConditions(
      feature, feature_config, group_configs, *event_model_,
      *availability_model_, *display_lock_controller_, configuration_.get(),
      *time_provider_);
  DVLOG(2) << "Would trigger result for " << feature.name
           << ": trigger=" << result.NoErrors()
           << " tracking_only=" << feature_config.tracking_only << " "
           << result;
  return result.NoErrors() && !feature_config.tracking_only;
}

bool TrackerImpl::HasEverTriggered(const base::Feature& feature,
                                   bool from_window) const {
  const FeatureConfig feature_config =
      configuration_->GetFeatureConfig(feature);
  const EventConfig trigger_config = feature_config.trigger;

  uint32_t window_size =
      from_window ? trigger_config.window : trigger_config.storage;

  uint32_t event_count = event_model_->GetEventCount(
      trigger_config.name, time_provider_->GetCurrentDay(), window_size);

  return event_count > 0;
}

Tracker::TriggerState TrackerImpl::GetTriggerState(
    const base::Feature& feature) const {
  if (!IsInitialized()) {
    DVLOG(2) << "TriggerState for " << feature.name << ": "
             << static_cast<int>(Tracker::TriggerState::NOT_READY);
    return Tracker::TriggerState::NOT_READY;
  }

  FeatureConfig feature_config = configuration_->GetFeatureConfig(feature);
  std::vector<GroupConfig> group_configs;
  for (auto group : feature_config.groups) {
    group_configs.push_back(configuration_->GetGroupConfigByName(group));
  }
  ConditionValidator::Result result = condition_validator_->MeetsConditions(
      feature, configuration_->GetFeatureConfig(feature), group_configs,
      *event_model_, *availability_model_, *display_lock_controller_,
      configuration_.get(), *time_provider_);

  if (result.trigger_ok) {
    DVLOG(2) << "TriggerState for " << feature.name << ": "
             << static_cast<int>(Tracker::TriggerState::HAS_NOT_BEEN_DISPLAYED);
    return Tracker::TriggerState::HAS_NOT_BEEN_DISPLAYED;
  }

  DVLOG(2) << "TriggerState for " << feature.name << ": "
           << static_cast<int>(Tracker::TriggerState::HAS_BEEN_DISPLAYED);
  return Tracker::TriggerState::HAS_BEEN_DISPLAYED;
}

void TrackerImpl::Dismissed(const base::Feature& feature) {
  DVLOG(2) << "Dismissing " << feature.name;
  DCHECK(!IsFeatureBlockedByTest(feature));

  condition_validator_->NotifyDismissed(feature);
  stats::RecordUserDismiss();
  RecordShownTime(feature);
}

void TrackerImpl::DismissedWithSnooze(
    const base::Feature& feature,
    std::optional<SnoozeAction> snooze_action) {
  DCHECK(!IsFeatureBlockedByTest(feature));

  FeatureConfig feature_config = configuration_->GetFeatureConfig(feature);
  if (snooze_action == SnoozeAction::SNOOZED) {
    event_model_->IncrementSnooze(feature_config.trigger.name,
                                  time_provider_->GetCurrentDay(),
                                  time_provider_->Now());
  } else if (snooze_action == SnoozeAction::DISMISSED) {
    event_model_->DismissSnooze(feature_config.trigger.name);
  }
  Dismissed(feature);
  if (snooze_action.has_value())
    stats::RecordUserSnoozeAction(snooze_action.value());
}

std::unique_ptr<DisplayLockHandle> TrackerImpl::AcquireDisplayLock() {
  return display_lock_controller_->AcquireDisplayLock();
}

void TrackerImpl::SetPriorityNotification(const base::Feature& feature) {
  // If the handler hasn't been registered.
  auto iter = priority_notification_handlers_.find(feature.name);
  if (iter == priority_notification_handlers_.end()) {
    condition_validator_->SetPriorityNotification(feature.name);
    return;
  }

  // We already have a handler. Serve the request and remove the handler.
  condition_validator_->SetPriorityNotification(std::nullopt);
  std::move(iter->second).Run();
  priority_notification_handlers_.erase(feature.name);
}

std::optional<std::string> TrackerImpl::GetPendingPriorityNotification() {
  return condition_validator_->GetPendingPriorityNotification();
}

void TrackerImpl::RegisterPriorityNotificationHandler(
    const base::Feature& feature,
    base::OnceClosure callback) {
  // If we already have a pending notification, handle it right away.
  auto pending_priority_notification =
      condition_validator_->GetPendingPriorityNotification();
  if (pending_priority_notification.has_value() &&
      pending_priority_notification.value() == feature.name) {
    std::move(callback).Run();
    condition_validator_->SetPriorityNotification(std::nullopt);
    return;
  }

  // We don't have the notification yet. Cache the handler.
  priority_notification_handlers_.emplace(feature.name, std::move(callback));
}

void TrackerImpl::UnregisterPriorityNotificationHandler(
    const base::Feature& feature) {
  priority_notification_handlers_.erase(feature.name);
}

#if BUILDFLAG(IS_CHROMEOS_ASH)
void TrackerImpl::UpdateConfig(const base::Feature& feature,
                               const ConfigurationProvider* provider) {
  CHECK(IsInitialized());
  configuration_->UpdateConfig(feature, provider);
}
#endif

const Configuration* TrackerImpl::GetConfigurationForTesting() const {
  CHECK_IS_TEST();
  return configuration_.get();
}

void TrackerImpl::SetClockForTesting(const base::Clock& clock,
                                     base::Time initial_now) {
  CHECK_IS_TEST();
  time_provider_ =
      std::make_unique<TestingClockTimeProvider>(clock, initial_now);
}

bool TrackerImpl::IsInitialized() const {
  return event_model_->IsReady() && availability_model_->IsReady();
}

void TrackerImpl::AddOnInitializedCallback(OnInitializedCallback callback) {
  if (IsInitializationFinished()) {
    base::SingleThreadTaskRunner::GetCurrentDefault()->PostTask(
        FROM_HERE, base::BindOnce(std::move(callback), IsInitialized()));
    return;
  }

  on_initialized_callbacks_.push_back(std::move(callback));
}

void TrackerImpl::OnEventModelInitializationFinished(bool success) {
  DCHECK_EQ(success, event_model_->IsReady());
  event_model_initialization_finished_ = true;

  DVLOG(2) << "Event model initialization result = " << success;

  if (event_exporter_) {
    event_exporter_->ExportEvents(base::BindOnce(
        &TrackerImpl::OnReceiveExportedEvents, weak_ptr_factory_.GetWeakPtr()));
  } else {
    MaybePostInitializedCallbacks();
  }
}

void TrackerImpl::OnAvailabilityModelInitializationFinished(bool success) {
  DCHECK_EQ(success, availability_model_->IsReady());
  availability_model_initialization_finished_ = true;

  DVLOG(2) << "Availability model initialization result = " << success;

  MaybePostInitializedCallbacks();
}

bool TrackerImpl::IsInitializationFinished() const {
  bool event_migration_finished =
      event_exporter_ == nullptr || event_migration_finished_;
  return event_model_initialization_finished_ &&
         availability_model_initialization_finished_ &&
         event_migration_finished;
}

void TrackerImpl::MaybePostInitializedCallbacks() {
  if (!IsInitializationFinished())
    return;

  DVLOG(2) << "Initialization finished.";

  for (auto& callback : on_initialized_callbacks_) {
    base::SingleThreadTaskRunner::GetCurrentDefault()->PostTask(
        FROM_HERE, base::BindOnce(std::move(callback), IsInitialized()));
  }

  on_initialized_callbacks_.clear();
}

void TrackerImpl::RecordShownTime(const base::Feature& feature) {
  std::string feature_name = feature.name;
  auto iter = start_times_.find(feature_name);
  if (iter == start_times_.end())
    return;

  UmaHistogramTimes("InProductHelp.ShownTime." + feature_name,
                    time_provider_->Now() - iter->second);
  UmaHistogramMediumTimes("InProductHelp.ShownTime2." + feature_name,
                          time_provider_->Now() - iter->second);
  start_times_.erase(feature_name);
}

void TrackerImpl::OnReceiveExportedEvents(
    std::vector<TrackerEventExporter::EventData> events) {
  for (auto& event : events) {
    event_model_->IncrementEvent(event.event_name, event.day);
  }

  event_migration_finished_ = true;
  MaybePostInitializedCallbacks();
}

// static
void Tracker::PropagateTestStateToChildProcess(
    base::CommandLine& command_line) {
  auto* const blocked = BlockedIphFeatures::GetInstance();
  base::AutoLock lock(blocked->GetLock());
  blocked->MaybeWriteToCommandLine(command_line);
}

// static
bool TrackerImpl::IsFeatureBlockedByTest(const base::Feature& feature) {
  auto* const blocked = BlockedIphFeatures::GetInstance();
  base::AutoLock lock(blocked->GetLock());
  return blocked->IsFeatureBlocked(feature.name);
}

}  // namespace feature_engagement
