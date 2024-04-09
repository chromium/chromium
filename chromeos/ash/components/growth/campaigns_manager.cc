// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chromeos/ash/components/growth/campaigns_manager.h"

#include <optional>

#include "ash/constants/ash_switches.h"
#include "base/base64.h"
#include "base/command_line.h"
#include "base/files/file_path.h"
#include "base/files/file_util.h"
#include "base/json/json_reader.h"
#include "base/logging.h"
#include "base/strings/string_number_conversions.h"
#include "base/strings/stringprintf.h"
#include "base/syslog_logging.h"
#include "base/task/thread_pool.h"
#include "base/time/time.h"
#include "chromeos/ash/components/growth/campaigns_constants.h"
#include "chromeos/ash/components/growth/campaigns_matcher.h"
#include "chromeos/ash/components/growth/growth_metrics.h"
#include "components/prefs/pref_service.h"

namespace growth {

namespace {

CampaignsManager* g_instance = nullptr;

inline constexpr char kCampaignFileName[] = "campaigns.json";

inline constexpr char kEventKey[] = "event_to_be_cleared";
inline constexpr char kEventTemplate[] =
    "name:%s;comparator:any;window:365;storage:365";

inline constexpr char kOobeCompleteFlagFilePath[] =
    "/home/chronos/.oobe_completed";

std::optional<base::Value::Dict> ParseCampaignsFile(
    const std::string& campaigns_data) {
  std::optional<base::Value> value(base::JSONReader::Read(campaigns_data));
  if (!value || !value->is_dict()) {
    LOG(ERROR) << "Failed to parse campaigns file: " << campaigns_data;
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
    LOG(ERROR) << "Failed to read campaigns file from disk.";
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
    LOG(ERROR) << "Growth campaign id not found";
    return;
  }

  SYSLOG(INFO) << "Growth Campaign " << *id
               << " is selected for slot: " << base::NumberToString(int(slot));
}

std::string GetEventName(growth::CampaignEvent event, const std::string& id) {
  const char* event_name = nullptr;
  switch (event) {
    case growth::CampaignEvent::kImpression:
      event_name = growth::kCampaignEventNameImpression;
      break;
    case growth::CampaignEvent::kDismissed:
      event_name = growth::kCampaignEventNameDismissed;
      break;
    case growth::CampaignEvent::kAppOpened:
      event_name = growth::kCampaignEventNameAppOpened;
      break;
  }

  std::string event_name_with_id = base::StringPrintf(event_name, id.c_str());
  return growth::kGrowthCampaignsEventNamePrefix + event_name_with_id;
}

// Gets the Oobe timestamp on a sequence that allows file-access.
base::Time GetOobeTimestampBackground() {
  base::File::Info file_info;
  if (base::GetFileInfo(base::FilePath(kOobeCompleteFlagFilePath),
                        &file_info)) {
    return file_info.creation_time;
  }

  return base::Time::Min();
}

}  // namespace

// static
CampaignsManager* CampaignsManager::Get() {
  DCHECK(g_instance);
  return g_instance;
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
  base::CommandLine* command_line = base::CommandLine::ForCurrentProcess();
  if (command_line->HasSwitch(ash::switches::kGrowthCampaigns)) {
    const auto& value =
        command_line->GetSwitchValueASCII(ash::switches::kGrowthCampaigns);
    std::string decoded_str;
    if (base::Base64Decode(value, &decoded_str)) {
      OnCampaignsLoaded(std::move(load_callback),
                        ParseCampaignsFile(decoded_str));
      return;
    } else {
      LOG(ERROR) << "Failed decode base64 encoded campaigns string.";
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
  if (match_result) {
    RecordGetCampaignBySlot(slot);
  }

  RecordCampaignMatchDuration(base::TimeTicks::Now() - match_start);
  LogCampaignInSystemLog(match_result, slot);

  RegisterTrialForCampaign(match_result);

  return match_result;
}

const std::string& CampaignsManager::GetOpenedAppId() const {
  return matcher_.opened_app_id();
}

void CampaignsManager::SetOpenedApp(const std::string& app_id) {
  matcher_.SetOpenedApp(app_id);

  if (!app_id.empty()) {
    NotifyEventForTargeting(CampaignEvent::kAppOpened, app_id);
  }
}

void CampaignsManager::SetActiveUrl(const GURL& url) {
  matcher_.SetActiveUrl(url);
}

void CampaignsManager::PerformAction(int campaign_id, const Action* action) {
  CHECK(action);

  auto* params = action->GetParams();
  auto action_type = action->GetActionType();
  if (!action_type || !params) {
    LOG(ERROR) << "Invalid action when performing action.";
    RecordCampaignsManagerError(CampaignsManagerError::kInvalidAction);
    return;
  }

  PerformAction(campaign_id, action_type.value(), params);
}

void CampaignsManager::PerformAction(int campaign_id,
                                     const ActionType action_type,
                                     const base::Value::Dict* params) {
  auto& action_performer = actions_map_.at(action_type);
  if (!action_performer) {
    // TODO(b/306023057): Record unrecognized action error.
    return;
  }

  action_performer->Run(
      campaign_id, params,
      base::BindOnce(
          [](growth::ActionType action_type, growth::ActionResult result,
             std::optional<growth::ActionResultReason> reason) {
            if (result == growth::ActionResult::kSuccess) {
              return;
            }

            LOG(ERROR) << "Error running action. Action type: "
                       << int(action_type) << ". Error code:"
                       << static_cast<int>(reason.value_or(
                              growth::ActionResultReason::kUnknown));
            RecordCampaignsManagerError(
                CampaignsManagerError::kPerformActionFailed);
          },
          action_type));
}

void CampaignsManager::ClearEvent(CampaignEvent event, const std::string& id) {
  std::map<std::string, std::string> conditions_params;
  // Event can be put in any key starting with `event_`.
  // Please see `components/feature_engagement/README.md#featureconfig`.
  conditions_params[kEventKey] =
      base::StringPrintf(kEventTemplate, GetEventName(event, id).c_str());
  client_->ClearConfig(conditions_params);
}

void CampaignsManager::NotifyEventForTargeting(CampaignEvent event,
                                               const std::string& id) {
  NotifyEvent(GetEventName(event, id));
}

void CampaignsManager::OnCampaignsComponentLoaded(
    base::OnceClosure load_callback,
    bool in_oobe,
    const std::optional<const base::FilePath>& path) {
  RecordCampaignsComponentDownloadDuration(
      base::TimeTicks::Now() - campaigns_download_start_time_, in_oobe);
  if (!path.has_value()) {
    LOG(ERROR) << "Failed to load campaign component.";
    RecordCampaignsManagerError(
        CampaignsManagerError::kCampaignsComponentLoadFail);
    OnCampaignsLoaded(std::move(load_callback), /*campaigns=*/std::nullopt);
    return;
  }
  // Read the campaigns file from component mounted path.
  base::ThreadPool::PostTaskAndReplyWithResult(
      FROM_HERE, {base::MayBlock()}, base::BindOnce(&ReadCampaignsFile, *path),
      base::BindOnce(&CampaignsManager::OnCampaignsLoaded,
                     weak_factory_.GetWeakPtr(), std::move(load_callback)));
}

void CampaignsManager::OnCampaignsLoaded(
    base::OnceClosure load_callback,
    std::optional<base::Value::Dict> campaigns_dict) {
  // Load campaigns into campaigns store.
  if (campaigns_dict.has_value()) {
    // Update campaigns store.
    campaigns_ = std::move(campaigns_dict.value());
  } else {
    LOG(ERROR) << "No campaign is loaded.";
  }

  // Load campaigns into `CampaignMatcher` for selecting campaigns.
  matcher_.SetCampaigns(&campaigns_);

  base::ThreadPool::PostTaskAndReplyWithResult(
      FROM_HERE, {base::MayBlock()},
      base::BindOnce(&GetOobeTimestampBackground),
      base::BindOnce(&CampaignsManager::OnOobeTimestampLoaded,
                     weak_factory_.GetWeakPtr(), std::move(load_callback)));
}

void CampaignsManager::OnOobeTimestampLoaded(base::OnceClosure load_callback,
                                             base::Time oobe_time) {
  matcher_.SetOobeCompleteTime(oobe_time);

  campaigns_loaded_ = true;

  std::move(load_callback).Run();
  NotifyCampaignsLoaded();
}

void CampaignsManager::NotifyCampaignsLoaded() {
  for (auto& observer : observers_) {
    observer.OnCampaignsLoadCompleted();
  }
}

void CampaignsManager::SetOobeCompleteTimeForTesting(base::Time time) {
  matcher_.SetOobeCompleteTime(time);
}

void CampaignsManager::RegisterTrialForCampaign(
    const Campaign* campaign) const {
  if (!campaign) {
    return;
  }

  std::optional<int> id = growth::GetCampaignId(campaign);
  if (!id) {
    // TODO(b/308684443): Add error metrics in a second CL.
    LOG(ERROR) << "Growth campaign id not found";
    return;
  }

  client_->RegisterSyntheticFieldTrial(
      /*study_id=*/growth::GetStudyId(campaign),
      /*campaign_id=*/*id);
}

void CampaignsManager::NotifyEvent(const std::string& event) {
  client_->NotifyEvent(event);
}

}  // namespace growth
