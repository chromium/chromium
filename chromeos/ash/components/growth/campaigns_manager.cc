// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chromeos/ash/components/growth/campaigns_manager.h"

#include <optional>
#include <string_view>
#include <utility>

#include "ash/constants/ash_features.h"
#include "ash/constants/ash_pref_names.h"
#include "ash/constants/ash_switches.h"
#include "base/base64.h"
#include "base/command_line.h"
#include "base/files/file_path.h"
#include "base/files/file_util.h"
#include "base/json/json_reader.h"
#include "base/logging.h"
#include "base/strings/strcat.h"
#include "base/strings/string_number_conversions.h"
#include "base/strings/stringprintf.h"
#include "base/syslog_logging.h"
#include "base/task/thread_pool.h"
#include "base/time/time.h"
#include "chromeos/ash/components/growth/campaigns_logger.h"
#include "chromeos/ash/components/growth/campaigns_matcher.h"
#include "chromeos/ash/components/growth/campaigns_model.h"
#include "chromeos/ash/components/growth/campaigns_utils.h"
#include "chromeos/ash/components/growth/growth_metrics.h"
#include "components/account_id/account_id.h"
#include "components/prefs/pref_registry_simple.h"
#include "components/prefs/pref_service.h"
#include "components/signin/public/identity_manager/tribool.h"
#include "components/user_manager/user_manager.h"

namespace growth {

namespace {

CampaignsManager* g_instance = nullptr;

inline constexpr char kCampaignFileName[] = "campaigns.json";

inline constexpr char kEventKey[] = "event_to_be_cleared";

inline constexpr char kOobeCompleteFlagFilePath[] =
    "/home/chronos/.oobe_completed";

// The synthetic trial name prefix for growth experiment. Formatted as
// `CrOSGrowthStudy{studyId}`, where `studyId` is an integer. For non
// experimental campaigns, `studyId` will be empty.
inline constexpr char kGrowthStudyName[] = "CrOSGrowthStudy";
// The synthetical trial group name for growth experiment. The campaign id
// will be unique for different groups.
inline constexpr char kGrowthGroupName[] = "CampaignId";

std::optional<base::Value::Dict> ParseCampaignsFile(
    const std::string& campaigns_data) {
  std::optional<base::Value> value(base::JSONReader::Read(campaigns_data));
  if (!value || !value->is_dict()) {
    CAMPAIGNS_LOG(ERROR) << "Failed to parse campaigns file.";
    CAMPAIGNS_LOG(VLOG) << "Malformed campaigns file: " << campaigns_data;
    RecordCampaignsManagerError(CampaignsManagerError::kCampaignsParsingFail);
    return std::nullopt;
  }

  return std::move(value->GetDict());
}

std::optional<base::Value::Dict> ReadCampaignsFile(
    const base::FilePath& campaigns_component_path) {
  const auto campaigns_load_start_time = base::TimeTicks::Now();

  std::string campaigns_data;
  if (!base::ReadFileToString(
          campaigns_component_path.Append(kCampaignFileName),
          &campaigns_data)) {
    CAMPAIGNS_LOG(ERROR) << "Failed to read campaigns file from disk.";
    RecordCampaignsManagerError(CampaignsManagerError::kCampaignsFileLoadFail);
    RecordCampaignsComponentReadDuration(base::TimeTicks::Now() -
                                         campaigns_load_start_time);
    return std::nullopt;
  }

  auto parse_result = ParseCampaignsFile(campaigns_data);

  RecordCampaignsComponentReadDuration(base::TimeTicks::Now() -
                                       campaigns_load_start_time);
  return parse_result;
}

void LogCampaignInSystemLog(const Campaign* campaign, Slot slot) {
  if (!campaign) {
    return;
  }

  std::optional<int> id = growth::GetCampaignId(campaign);
  if (!id) {
    // TODO(b/308684443): Add error metrics in a follow up CL.
    CAMPAIGNS_LOG(ERROR) << "Growth campaign id not found";
    return;
  }

  CAMPAIGNS_LOG(SYSLOG) << "Growth Campaign " << *id
                        << " is selected for slot: "
                        << base::NumberToString(int(slot));
}

// Gets the Oobe timestamp on a sequence that allows file-access.
base::Time GetOobeTimestampBackground() {
  base::File::Info file_info;
  if (base::GetFileInfo(base::FilePath(kOobeCompleteFlagFilePath),
                        &file_info)) {
    return file_info.creation_time;
  }

  // If the Oobe complete file is not found and there's no owner, assume that
  // the user campaigns is matching during Oobe flow and return the current time
  // as a close indicator of register time.
  const AccountId& owner_account_id =
      user_manager::UserManager::Get()->GetOwnerAccountId();
  return owner_account_id.is_valid() ? base::Time::Min() : base::Time::Now();
}

}  // namespace

// static
CampaignsManager* CampaignsManager::Get() {
  return g_instance;
}

// static
void CampaignsManager::RegisterProfilePrefs(PrefRegistrySimple* registry) {
  registry->RegisterListPref(ash::prefs::kGrowthPerksInterested);
}

CampaignsManager::CampaignsManager(CampaignsManagerClient* client,
                                   PrefService* local_state)
    : client_(client),
      matcher_(client, local_state),
      actions_map_(client->GetCampaignsActions()) {
  CHECK_EQ(g_instance, nullptr);
  g_instance = this;
}

CampaignsManager::~CampaignsManager() {
  CHECK_EQ(g_instance, this);
  g_instance = nullptr;
}

void CampaignsManager::AddObserver(Observer* observer) {
  observers_.AddObserver(observer);

  if (campaigns_loaded_) {
    // Do not notify the observer immediately in case blocking.
    base::SequencedTaskRunner::GetCurrentDefault()->PostTask(
        FROM_HERE, base::BindOnce(&CampaignsManager::NotifyCampaignsLoaded,
                                  weak_factory_.GetWeakPtr()));
  }
}

void CampaignsManager::RemoveObserver(Observer* observer) {
  observers_.RemoveObserver(observer);
}

void CampaignsManager::SetPrefs(PrefService* prefs) {
  // Pass `prefs` to `CampaignsMatcher` to enable prefs related targettings.
  matcher_.SetPrefs(prefs);
}

void CampaignsManager::LoadCampaigns(base::OnceClosure load_callback,
                                     bool in_oobe) {
  CAMPAIGNS_LOG(DEBUG) << "Start loading campaigns. `in_oobe`: "
                       << ToString(in_oobe);

  base::CommandLine* command_line = base::CommandLine::ForCurrentProcess();
  if (command_line->HasSwitch(ash::switches::kGrowthCampaigns)) {
    CAMPAIGNS_LOG(DEBUG)
        << "Switch `kGrowthCampaigns` is set. Loading campaigns from "
           "encoded campaigns string.";
    const auto& value =
        command_line->GetSwitchValueASCII(ash::switches::kGrowthCampaigns);
    std::string decoded_str;
    if (base::Base64Decode(value, &decoded_str)) {
      OnCampaignsLoaded(std::move(load_callback),
                        ParseCampaignsFile(decoded_str));
      return;
    } else {
      CAMPAIGNS_LOG(ERROR) << "Failed decode base64 encoded campaigns string.";
    }
  }

  campaigns_download_start_time_ = base::TimeTicks::Now();
  client_->LoadCampaignsComponent(base::BindOnce(
      &CampaignsManager::OnCampaignsComponentLoaded, weak_factory_.GetWeakPtr(),
      std::move(load_callback), in_oobe));
}

const Campaign* CampaignsManager::GetCampaignBySlot(Slot slot) const {
  CHECK(campaigns_loaded_)
      << "Getting campaign before campaigns finish loading";
  const auto match_start = base::TimeTicks::Now();
  auto* match_result = matcher_.GetCampaignBySlot(slot);
  RecordCampaignMatchDuration(base::TimeTicks::Now() - match_start);
  if (!match_result) {
    CAMPAIGNS_LOG(DEBUG) << "No campaign is selected for slot "
                         << static_cast<int>(slot);
    return nullptr;
  }

  CAMPAIGNS_LOG(DEBUG) << "Campaign: "
                       << growth::GetCampaignId(match_result).value()
                       << " is selected for slot " << static_cast<int>(slot);
  RecordGetCampaignBySlot(slot);
  LogCampaignInSystemLog(match_result, slot);
  RegisterTrialForCampaign(match_result);
  return match_result;
}

const GURL& CampaignsManager::GetActiveUrl() const {
  return matcher_.active_url();
}

void CampaignsManager::SetActiveUrl(const GURL& url) {
  matcher_.SetActiveUrl(url);
}

const std::string& CampaignsManager::GetOpenedAppId() const {
  return matcher_.opened_app_id();
}

void CampaignsManager::SetOpenedApp(std::string app_id) {
  if (!app_id.empty()) {
    RecordEvent(GetEventName(CampaignEvent::kAppOpened, app_id));
  }
  matcher_.SetOpenedApp(std::move(app_id));
}

const Trigger& CampaignsManager::GetTrigger() const {
  return matcher_.trigger();
}

void CampaignsManager::SetTrigger(const Trigger&& trigger_type) {
  matcher_.SetTrigger(std::move(trigger_type));
}

void CampaignsManager::SetIsUserOwner(bool is_user_owner) {
  matcher_.SetIsUserOwner(is_user_owner);
}

void CampaignsManager::PerformAction(int campaign_id,
                                     std::optional<int> group_id,
                                     const Action* action) {
  // In rare case when the caller fails to check action, log
  // an error metric if the action is not defined.
  if (!action) {
    CAMPAIGNS_LOG(ERROR) << "Missing action when performing action.";
    RecordCampaignsManagerError(
        CampaignsManagerError::kMissingActionPerformerAction);
    return;
  }

  auto* params = action->GetParams();
  auto action_type = action->GetActionType();
  if (!action_type || !params) {
    CAMPAIGNS_LOG(ERROR) << "Invalid action when performing action.";
    RecordCampaignsManagerError(CampaignsManagerError::kInvalidAction);
    return;
  }

  PerformAction(campaign_id, group_id, action_type.value(), params);
}

void CampaignsManager::PerformAction(int campaign_id,
                                     std::optional<int> group_id,
                                     const ActionType action_type,
                                     const base::Value::Dict* params) {
  auto& action_performer = actions_map_.at(action_type);
  if (!action_performer) {
    // TODO(b/306023057): Record unrecognized action error.
    return;
  }

  action_performer->Run(
      campaign_id, group_id, params,
      base::BindOnce(
          [](growth::ActionType action_type, growth::ActionResult result,
             std::optional<growth::ActionResultReason> reason) {
            if (result == growth::ActionResult::kSuccess) {
              return;
            }

            CAMPAIGNS_LOG(ERROR)
                << "Error running action. Action type: " << int(action_type)
                << ". Error code:"
                << static_cast<int>(
                       reason.value_or(growth::ActionResultReason::kUnknown));
            RecordCampaignsManagerError(
                CampaignsManagerError::kPerformActionFailed);
          },
          action_type));
}

void CampaignsManager::ClearEvent(CampaignEvent event, std::string_view id) {
  ClearEvent(GetEventName(event, id));
}

void CampaignsManager::ClearEvent(std::string_view event) {
  std::map<std::string, std::string> conditions_params;
  // Event can be put in any key starting with `event_`.
  // Please see `components/feature_engagement/README.md#featureconfig`.
  conditions_params[kEventKey] = base::StrCat(
      {"name:", event, ";comparator:any;window:3650;storage:3650"});
  client_->ClearConfig(conditions_params);
}

void CampaignsManager::RecordEvent(const std::string& event,
                                   bool trigger_campaigns) {
  const bool should_trigger_campaigns =
      trigger_campaigns &&
      ash::features::IsGrowthCampaignsTriggerByRecordEventEnabled();

  if (!campaigns_loaded_) {
    // Event is recorded before campaigns are loaded and feature engagement
    // tracker is initialized, defer recording and trigger to
    // `OnCampaignsComponentLoaded()` where campaigns are loaded and feature
    // engagement is ready.
    if (should_trigger_campaigns) {
      queued_events_record_and_trigger_.insert(event);
    } else {
      queued_events_record_only_.insert(event);
    }

    RecordCampaignsManagerError(
        CampaignsManagerError::kRecordEventBeforeCampaignsLoaded);
    return;
  }

  client_->RecordEvent(event, should_trigger_campaigns);
}

void CampaignsManager::RecordQueuedEventsAndMaybeTrigger() {
  CHECK(campaigns_loaded_);

  for (const auto& event : queued_events_record_only_) {
    RecordEvent(event, /*trigger_campaigns=*/false);
  }
  queued_events_record_only_.clear();

  for (const auto& event : queued_events_record_and_trigger_) {
    RecordEvent(event, /*trigger_campaigns=*/true);
  }
  queued_events_record_and_trigger_.clear();
}

void CampaignsManager::OnCampaignsComponentLoaded(
    base::OnceClosure load_callback,
    bool in_oobe,
    const std::optional<const base::FilePath>& path) {
  RecordCampaignsComponentDownloadDuration(
      base::TimeTicks::Now() - campaigns_download_start_time_, in_oobe);
  if (!path.has_value()) {
    CAMPAIGNS_LOG(ERROR) << "Failed to load campaign component.";
    RecordCampaignsManagerError(
        CampaignsManagerError::kCampaignsComponentLoadFail);
    OnCampaignsLoaded(std::move(load_callback), /*campaigns=*/std::nullopt);
    return;
  }

  CAMPAIGNS_LOG(DEBUG) << "Getting device registered time.";
  if (const auto registered_time = GetRegisteredTimeForTesting()) {
    CAMPAIGNS_LOG(DEBUG) << "Registered time for test is set.";
    OnOobeTimestampLoaded(std::move(load_callback), path,
                          registered_time.value());
    return;
  }

  base::ThreadPool::PostTaskAndReplyWithResult(
      FROM_HERE, {base::MayBlock()},
      base::BindOnce(&GetOobeTimestampBackground),
      base::BindOnce(&CampaignsManager::OnOobeTimestampLoaded,
                     weak_factory_.GetWeakPtr(), std::move(load_callback),
                     path));
}

void CampaignsManager::OnCampaignsLoaded(
    base::OnceClosure load_callback,
    std::optional<base::Value::Dict> campaigns_dict) {
  // Load campaigns into campaigns store.
  if (campaigns_dict.has_value()) {
    // Update campaigns store.
    campaigns_ = std::move(campaigns_dict.value());
  } else {
    CAMPAIGNS_LOG(ERROR) << "No campaign is loaded.";
  }

  // Load campaigns into `CampaignMatcher` for selecting campaigns.
  CAMPAIGNS_LOG(DEBUG) << "Filter and set campaigns.";
  matcher_.FilterAndSetCampaigns(&campaigns_);

  campaigns_loaded_ = true;

  std::move(load_callback).Run();
  RecordQueuedEventsAndMaybeTrigger();
  NotifyCampaignsLoaded();
}

void CampaignsManager::OnOobeTimestampLoaded(
    base::OnceClosure load_callback,
    const std::optional<const base::FilePath>& path,
    base::Time oobe_time) {
  CAMPAIGNS_LOG(DEBUG) << "Device registered time is: "
                       << oobe_time.InSecondsFSinceUnixEpoch();
  matcher_.SetOobeCompleteTime(oobe_time);

  CAMPAIGNS_LOG(DEBUG) << "Initializing feature_engagement::Tracker.";
  if (tracker_initialized_for_test_) {
    OnTrackerInitialized(std::move(load_callback), path,
                         /*init_success=*/true);
    return;
  }

  client_->AddOnTrackerInitializedCallback(base::BindOnce(
      &CampaignsManager::OnTrackerInitialized, weak_factory_.GetWeakPtr(),
      std::move(load_callback), path));
}

void CampaignsManager::OnTrackerInitialized(
    base::OnceClosure load_callback,
    const std::optional<const base::FilePath>& path,
    bool init_success) {
  if (!init_success) {
    // Only log error, but will continue loading campaigns.
    CAMPAIGNS_LOG(ERROR) << "Failed to initialize feature_engagement::Tracker.";
    RecordCampaignsManagerError(
        CampaignsManagerError::kTrackerInitializationFail);
  }

  CAMPAIGNS_LOG(DEBUG) << "Initialized feature_engagement::Tracker.";
  // Read the campaigns file from component mounted path.
  base::ThreadPool::PostTaskAndReplyWithResult(
      FROM_HERE, {base::MayBlock()}, base::BindOnce(&ReadCampaignsFile, *path),
      base::BindOnce(&CampaignsManager::OnCampaignsLoaded,
                     weak_factory_.GetWeakPtr(), std::move(load_callback)));
}

void CampaignsManager::NotifyCampaignsLoaded() {
  for (auto& observer : observers_) {
    observer.OnCampaignsLoadCompleted();
  }
}

void CampaignsManager::SetMantaCapabilityForTesting(signin::Tribool value) {
  matcher_.SetMantaCapabilityForTesting(value);  // IN-TEST
}

void CampaignsManager::SetOobeCompleteTimeForTesting(base::Time time) {
  oobe_complete_time_for_test_ = time;
}

void CampaignsManager::SetTrackerInitializedForTesting() {
  tracker_initialized_for_test_ = true;
}

const Campaigns* CampaignsManager::GetCampaignsBySlotForTesting(
    Slot slot) const {
  return GetCampaignsBySlot(&campaigns_, slot);
}

std::optional<base::Time> CampaignsManager::GetRegisteredTimeForTesting() {
  if (!oobe_complete_time_for_test_.is_null()) {
    return oobe_complete_time_for_test_;
  }

  base::CommandLine* command_line = base::CommandLine::ForCurrentProcess();
  if (command_line->HasSwitch(
          ash::switches::kGrowthCampaignsRegisteredTimeSecondsSinceUnixEpoch)) {
    CAMPAIGNS_LOG(DEBUG)
        << "Switch `kGrothCampaignsRegisteredTimeSecondsSinceUnixEpoch` is "
           "set.";
    const auto& value = command_line->GetSwitchValueASCII(
        ash::switches::kGrowthCampaignsRegisteredTimeSecondsSinceUnixEpoch);

    double seconds_since_epoch;
    CHECK(base::StringToDouble(value, &seconds_since_epoch));
    return base::Time::FromSecondsSinceUnixEpoch(seconds_since_epoch);
  }

  return std::nullopt;
}

void CampaignsManager::RegisterTrialForCampaign(
    const Campaign* campaign) const {
  if (!campaign) {
    return;
  }

  std::optional<int> campaign_id = growth::GetCampaignId(campaign);
  if (!campaign_id) {
    // TODO(b/308684443): Add error metrics in a second CL.
    CAMPAIGNS_LOG(ERROR) << "Growth campaign id not found";
    return;
  }

  std::optional<int> study_id = GetStudyId(campaign);

  // If `study_id` is not null, appends it to the end of `trial_name`.
  std::string trial_name(kGrowthStudyName);
  if (study_id) {
    base::StringAppendF(&trial_name, "%d", *study_id);
  }
  std::string group_name(kGrowthGroupName);
  base::StringAppendF(&group_name, "%d", *campaign_id);

  client_->RegisterSyntheticFieldTrial(trial_name, group_name);

  std::optional<bool> register_with_trigger_event =
      ShouldRegisterTrialWithTriggerEventName(campaign);
  if (!register_with_trigger_event.value_or(false)) {
    return;
  }

  // Register synthetic field trail with `event` name if this campaign is
  // triggered by `kEvent`. This is used for shared counterfactual control group
  // to be able to slice by different triggerings, e.g. `DocsOpened`,
  // `GmailOpened`, etc.
  if (GetTrigger().type != TriggerType::kEvent) {
    return;
  }

  // The first event name is used to register the synthetic field trail.
  // TODO: b/367838684 - Currently, this is only used for the G1 campaigns,
  // where it triggers at app open and has only one event. Extend this to
  // support multiple events.
  CHECK(!GetTrigger().events.empty());
  group_name += GetTrigger().events[0];
  client_->RegisterSyntheticFieldTrial(trial_name, group_name);
}

}  // namespace growth
