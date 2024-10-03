// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chromeos/ash/components/growth/campaigns_matcher.h"

#include <memory>
#include <optional>
#include <string_view>
#include <utility>

#include "ash/constants/ash_features.h"
#include "ash/constants/ash_pref_names.h"
#include "ash/constants/ash_switches.h"
#include "base/command_line.h"
#include "base/containers/contains.h"
#include "base/feature_list.h"
#include "base/features.h"
#include "base/logging.h"
#include "base/strings/stringprintf.h"
#include "base/time/time.h"
#include "base/version.h"
#include "base/version_info/version_info.h"
#include "chromeos/ash/components/demo_mode/utils/dimensions_utils.h"
#include "chromeos/ash/components/growth/campaigns_logger.h"
#include "chromeos/ash/components/growth/campaigns_manager_client.h"
#include "chromeos/ash/components/growth/campaigns_model.h"
#include "chromeos/ash/components/growth/campaigns_utils.h"
#include "chromeos/ash/components/growth/growth_metrics.h"
#include "components/account_id/account_id.h"
#include "components/prefs/pref_service.h"
#include "components/signin/public/identity_manager/account_capabilities.h"
#include "components/signin/public/identity_manager/identity_manager.h"
#include "components/signin/public/identity_manager/tribool.h"
#include "components/user_manager/user.h"
#include "components/user_manager/user_manager.h"
#include "components/version_info/version_info.h"
#include "third_party/re2/src/re2/re2.h"

namespace growth {
namespace {

inline constexpr char kEventKey[] = "event_to_be_checked";

base::Time GetDeviceCurrentTimeForScheduling() {
  base::CommandLine* command_line = base::CommandLine::ForCurrentProcess();
  if (command_line->HasSwitch(
          ash::switches::kGrowthCampaignsCurrentTimeSecondsSinceUnixEpoch)) {
    const auto& value = command_line->GetSwitchValueASCII(
        ash::switches::kGrowthCampaignsCurrentTimeSecondsSinceUnixEpoch);

    double seconds_since_epoch;
    CHECK(base::StringToDouble(value, &seconds_since_epoch));
    return base::Time::FromSecondsSinceUnixEpoch(seconds_since_epoch);
  }

  return base::Time::Now();
}

bool MatchPref(const base::Value::List* criterias,
               std::string_view pref_path,
               const PrefService* pref_service) {
  if (!pref_service) {
    CAMPAIGNS_LOG(ERROR) << "Matching pref before pref service is available";
    RecordCampaignsManagerError(
        CampaignsManagerError::kUserPrefUnavailableAtMatching);
    return false;
  }

  if (!criterias) {
    // No related targeting found in campaign targeting, returns true.
    return true;
  }

  auto& value = pref_service->GetValue(pref_path);

  // String list targeting.
  if (criterias) {
    return Contains(*criterias, value);
  }

  return false;
}

int GetMilestone() {
  return version_info::GetMajorVersionNumberAsInt();
}

const base::Version& GetVersion() {
  return version_info::GetVersion();
}

bool MatchTimeWindow(const base::Time& start_time,
                     const base::Time& end_time,
                     const base::Time& targeted_time) {
  return start_time <= targeted_time && end_time >= targeted_time;
}

bool MatchTimeWindow(const TimeWindowTargeting& time_window_targeting,
                     const base::Time& targeted_time) {
  return MatchTimeWindow(time_window_targeting.GetStartTime(),
                         time_window_targeting.GetEndTime(), targeted_time);
}

// Matched if any of the given `scheduling_targetings` is matched.
bool MatchSchedulings(const std::vector<std::unique_ptr<TimeWindowTargeting>>&
                          scheduling_targetings) {
  if (scheduling_targetings.empty()) {
    // Match campaign if there is no scheduling targeting criteria.
    return true;
  }

  const auto now = GetDeviceCurrentTimeForScheduling();
  for (const auto& scheduling_targeting : scheduling_targetings) {
    if (MatchTimeWindow(*scheduling_targeting, now)) {
      return true;
    }
  }

  CAMPAIGNS_LOG(DEBUG) << "Schedulings is NOT matched.";
  return false;
}

bool MatchExperimentTags(const base::Value::List* experiment_tags,
                         std::optional<const base::Feature*> feature) {
  if (!ash::features::IsGrowthCampaignsExperimentTagTargetingEnabled()) {
    // Campaign not match if experiment tag targeting is not enabled.
    return false;
  }

  // TODO: b/344673533 - verify that valid experiment targeting should have
  // both feature and experiment tag. Ignore the campaign if it is not the case.
  if (!feature.has_value()) {
    // Campaign matched if there is no targeted feature config.
    return true;
  }

  const auto* targeted_feature = feature.value();
  if (!targeted_feature) {
    // Campaign not matched if feaure config is invalid.
    CAMPAIGNS_LOG(DEBUG) << "Feature config is invalid.";
    return false;
  }

  if (!experiment_tags || experiment_tags->empty()) {
    // Campaign matched if there is no experiment tag targeting.
    CAMPAIGNS_LOG(DEBUG) << "No targeted experiment tag.";
    return true;
  }

  const auto exp_tag =
      base::GetFieldTrialParamValueByFeature(*targeted_feature, "exp_tag");

  if (exp_tag.empty()) {
    // Campaign not match if no experiment tag exists.
    CAMPAIGNS_LOG(DEBUG) << "No experiment tag exists.";
    return false;
  }

  // Campaign is matched if the tag from field trail param matches any of the
  // tag in the targeting criteria.
  const bool exp_tag_matched = base::Contains(*experiment_tags, exp_tag);
  if (!exp_tag_matched) {
    CAMPAIGNS_LOG(DEBUG) << "ExperimentTags is NOT matched.";
  }
  return exp_tag_matched;
}

// Match if the target values and the user pref has any overlap entries.
bool HasOverlapEntries(const base::Value::List& pref_values,
                       const base::Value::List& target_values) {
  for (auto& value : target_values) {
    if (base::Contains(pref_values, value)) {
      return true;
    }
  }
  return false;
}

// Match if the values in the List and Vector has any overlap entries.
bool HasOverlapEntries(const base::Value::List& list_values,
                       const std::vector<std::string>& vector_values) {
  for (const auto& value : vector_values) {
    if (base::Contains(list_values, value)) {
      return true;
    }
  }
  return false;
}

// Match if any value in the target is found in user pref.
bool MatchUserPref(const PrefService& pref_service,
                   const std::string* pref_name,
                   const base::Value::List* target_values) {
  if (!pref_name || pref_name->empty()) {
    growth::RecordCampaignsManagerError(
        growth::CampaignsManagerError::kTargetingUserPrefParsingFail);
    CAMPAIGNS_LOG(ERROR) << "Targeting user pref name missing.";
    return false;
  }

  if (!target_values || target_values->empty()) {
    growth::RecordCampaignsManagerError(
        growth::CampaignsManagerError::kTargetingUserPrefParsingFail);
    CAMPAIGNS_LOG(ERROR) << "Targeting user pref value missing.";
    return false;
  }

  auto* pref = pref_service.FindPreference(*pref_name);
  if (!pref) {
    growth::RecordCampaignsManagerError(
        growth::CampaignsManagerError::kTargetingUserPrefNotFound);
    CAMPAIGNS_LOG(ERROR) << "Targeting user pref not found: " << pref_name;
    return false;
  }

  auto* pref_value = pref->GetValue();
  CHECK(pref_value);

  if (pref_value->is_none() || pref_value->is_dict()) {
    CAMPAIGNS_LOG(ERROR) << "User pref type is not supported: " << pref_name;
    return false;
  }

  // If the user pref is a list, match if any entry in target values is in user
  // pref.
  if (pref_value->is_list()) {
    return HasOverlapEntries(pref_value->GetList(), *target_values);
  }

  // If the user pref is not a list, match if any entry in target values is the
  // pref value.
  return base::Contains(*target_values, *pref_value);
}

// TODO: b/354060160 - Add more data type to pref targeting.
// Match a user pref condition if any criterion matched.
//   [ // These criteria are logic OR.
//     {"name": "prefA", "value": ["A1", "A2"]},
//     {"name": "prefB", "value": ["B1"]}
//   ],
// conditions_met = A1 || A2 || B1
bool MatchUserPrefCriteria(const PrefService& pref_service,
                           const base::Value::List& criteria) {
  for (auto& criterion : criteria) {
    if (!criterion.is_dict()) {
      growth::RecordCampaignsManagerError(
          growth::CampaignsManagerError::kTargetingUserPrefParsingFail);
      CAMPAIGNS_LOG(ERROR) << "Fail to parse user pref targeting.";
      continue;
    }

    if (MatchUserPref(pref_service, criterion.GetDict().FindString("name"),
                      criterion.GetDict().FindList("value"))) {
      return true;
    }
  }
  return false;
}

// Match the user preference. The structure looks like:
// "userPrefs": [ // These two conditions are logic AND.
//   [ // These criteria are logic OR.
//     {"name": "prefA", "value": ["A1", "A2"]},
//     {"name": "prefB", "value": ["B1"]}
//   ],
//   [
//     {"name": "prefC", "value": ["C1"]}
//   ]
// ]
// conditions_met = (A1 || A2 || B1) && C1;
bool MatchUserPrefs(const PrefService* pref_service,
                    const base::Value::List* user_pref_targetings) {
  if (!user_pref_targetings || user_pref_targetings->empty()) {
    return true;
  }

  if (!pref_service) {
    RecordCampaignsManagerError(
        CampaignsManagerError::kUserPrefUnavailableAtMatching);
    CAMPAIGNS_LOG(ERROR)
        << "Matching user pref before user pref service is available";
    return false;
  }

  // If all conditions are matched, the campaign will be selected.
  for (auto& targeting : *user_pref_targetings) {
    if (!targeting.is_list()) {
      growth::RecordCampaignsManagerError(
          growth::CampaignsManagerError::kTargetingUserPrefParsingFail);
      CAMPAIGNS_LOG(ERROR) << "Invalid user pref targeting set.";
      return false;
    }
    if (!MatchUserPrefCriteria(*pref_service, targeting.GetList())) {
      CAMPAIGNS_LOG(DEBUG) << "UserPref is NOT matched.";
      return false;
    }
  }
  return true;
}

bool MatchVersion(const base::Version& current_version,
                  std::optional<base::Version> min_version,
                  std::optional<base::Version> max_version) {
  if (!current_version.IsValid()) {
    // Not match if current version is invalid.
    return false;
  }

  if (min_version && min_version->CompareTo(current_version) == 1) {
    return false;
  }

  if (max_version && max_version->CompareTo(current_version) == -1) {
    return false;
  }

  return true;
}

bool IsCampaignValid(const Campaign* campaign) {
  if (!GetCampaignId(campaign)) {
    CAMPAIGNS_LOG(ERROR) << "Invalid campaign: missing campaign ID.";
    RecordCampaignsManagerError(CampaignsManagerError::kMissingCampaignId);
    return false;
  }

  return true;
}

std::map<std::string, std::string> CreateBasicConditionParams() {
  std::map<std::string, std::string> conditions_params;
  // `event_used` and `event_trigger` are required for feature_engagement
  // config, although they are not used in campaign matching.
  static constexpr char kTemplate[] =
      "name:ChromeOSAshGrowthCampaigns_Event%s;comparator:any;window:1;storage:"
      "1";
  conditions_params["event_used"] = base::StringPrintf(kTemplate, "Used");
  conditions_params["event_trigger"] = base::StringPrintf(kTemplate, "Trigger");

  return conditions_params;
}

}  // namespace

CampaignsMatcher::CampaignsMatcher(CampaignsManagerClient* client,
                                   PrefService* local_state)
    : client_(client), local_state_(local_state) {}
CampaignsMatcher::~CampaignsMatcher() = default;

void CampaignsMatcher::FilterAndSetCampaigns(CampaignsPerSlot* campaigns) {
  // Filter campaigns that doesn't pass pre-match.
  for (int slot = 0; slot <= static_cast<int>(Slot::kMaxValue); slot++) {
    auto* targeted_campaigns =
        GetMutableCampaignsBySlot(campaigns, static_cast<Slot>(slot));
    if (!targeted_campaigns) {
      continue;
    }

    auto campaign_iter = targeted_campaigns->begin();
    while (campaign_iter != targeted_campaigns->end()) {
      if (IsCampaignMatched(campaign_iter->GetIfDict(),
                            /*is_prematch=*/true)) {
        ++campaign_iter;
      } else {
        campaign_iter = targeted_campaigns->erase(campaign_iter);
      }
    }
  }

  campaigns_ = campaigns;
}

void CampaignsMatcher::SetOpenedApp(std::string app_id) {
  opened_app_id_ = std::move(app_id);
}

void CampaignsMatcher::SetActiveUrl(const GURL& url) {
  active_url_ = url;
}

void CampaignsMatcher::SetOobeCompleteTime(base::Time time) {
  oobe_compelete_time_ = time;
}

void CampaignsMatcher::SetIsUserOwner(bool is_user_owner) {
  is_user_owner_ = is_user_owner;
}

void CampaignsMatcher::SetTrigger(const Trigger&& trigger) {
  trigger_ = std::move(trigger);
}

void CampaignsMatcher::SetPrefs(PrefService* prefs) {
  prefs_ = prefs;
}

const Campaign* CampaignsMatcher::GetCampaignBySlot(Slot slot) const {
  auto* targeted_campaigns = GetCampaignsBySlot(campaigns_, slot);
  if (!targeted_campaigns) {
    return nullptr;
  }

  for (auto& campaign_value : *targeted_campaigns) {
    const auto* campaign = campaign_value.GetIfDict();
    if (IsCampaignMatched(campaign, /*is_prematch=*/false)) {
      return campaign;
    }
  }

  return nullptr;
}

void CampaignsMatcher::SetMantaCapabilityForTesting(signin::Tribool value) {
  manta_capability_for_testing_ = value;
}

bool CampaignsMatcher::IsCampaignMatched(const Campaign* campaign,
                                         bool is_prematch) const {
  if (!campaign || !IsCampaignValid(campaign)) {
    CAMPAIGNS_LOG(ERROR) << "Invalid campaign.";
    RecordCampaignsManagerError(CampaignsManagerError::kInvalidCampaign);
    return false;
  }

  const auto* targetings = GetTargetings(campaign);

  const auto campaign_id = GetCampaignId(campaign).value();

  CAMPAIGNS_LOG(DEBUG) << "Evaluating campaign: " << campaign_id
                       << ". In prematch: " << ToString(is_prematch);

  const bool is_matched = Matched(targetings, campaign_id,
                                  GetCampaignGroupId(campaign), is_prematch);

  CAMPAIGNS_LOG(DEBUG) << "Campaign: " << campaign_id
                       << " is matched: " << ToString(is_matched)
                       << " In prematch: " << ToString(is_prematch);
  return is_matched;
}

bool CampaignsMatcher::MatchDemoModeTier(
    const DemoModeTargeting& targeting) const {
  const auto is_cloud_gaming = targeting.TargetCloudGamingDevice();
  if (is_cloud_gaming.has_value()) {
    if (is_cloud_gaming != client_->IsCloudGamingDevice()) {
      return false;
    }
  }

  const auto is_feature_aware_device = targeting.TargetFeatureAwareDevice();
  if (is_feature_aware_device.has_value()) {
    if (is_feature_aware_device != client_->IsFeatureAwareDevice()) {
      return false;
    }
  }
  return true;
}

bool CampaignsMatcher::MatchRetailers(
    const base::Value::List* retailers) const {
  if (!retailers) {
    return true;
  }

  base::Value::List canonicalized_retailers;
  for (auto& retailer : *retailers) {
    if (retailer.is_string()) {
      canonicalized_retailers.Append(
          ash::demo_mode::CanonicalizeDimension(retailer.GetString()));
    }
  }

  return MatchPref(&canonicalized_retailers, ash::prefs::kDemoModeRetailerId,
                   local_state_);
}

bool CampaignsMatcher::MatchDemoModeAppVersion(
    const DemoModeTargeting& targeting) const {
  return MatchVersion(client_->GetDemoModeAppVersion(),
                      targeting.GetAppMinVersion(),
                      targeting.GetAppMaxVersion());
}

bool CampaignsMatcher::MaybeMatchDemoModeTargeting(
    const DemoModeTargeting& targeting) const {
  if (!targeting.IsValid()) {
    // Campaigns matched if there is no demo mode targeting.
    return true;
  }

  if (!client_->IsDeviceInDemoMode()) {
    // Return early if it is not in demo mode while the campaign is targeting
    // demo mode.
    return false;
  }

  if (!MatchDemoModeAppVersion(targeting)) {
    return false;
  }

  if (!MatchDemoModeTier(targeting)) {
    return false;
  }

  return MatchRetailers(targeting.GetRetailers()) &&
         MatchPref(targeting.GetStoreIds(), ash::prefs::kDemoModeStoreId,
                   local_state_) &&
         MatchPref(targeting.GetCountries(), ash::prefs::kDemoModeCountry,
                   local_state_);
}

bool CampaignsMatcher::MatchMilestone(const DeviceTargeting& targeting) const {
  const auto milestone = GetMilestone();
  auto min_milestone = targeting.GetMinMilestone();
  if (min_milestone && milestone < min_milestone) {
    return false;
  }

  auto max_milestone = targeting.GetMaxMilestone();
  if (max_milestone && milestone > max_milestone) {
    return false;
  }

  return true;
}

bool CampaignsMatcher::MatchMilestoneVersion(
    const DeviceTargeting& targeting) const {
  return MatchVersion(GetVersion(), targeting.GetMinVersion(),
                      targeting.GetMaxVersion());
}

bool CampaignsMatcher::MatchDeviceTargeting(
    const DeviceTargeting& targeting) const {
  if (!targeting.IsValid()) {
    // Campaigns matched if there is no device targeting.
    return true;
  }

  auto target_feature_aware_device = targeting.GetFeatureAwareDevice();
  if (target_feature_aware_device &&
      target_feature_aware_device.value() !=
          ash::features::IsFeatureManagementGrowthFrameworkEnabled()) {
    return false;
  }

  const auto* targeting_locales = targeting.GetLocales();
  if (targeting_locales &&
      !Contains(*targeting_locales, client_->GetApplicationLocale())) {
    return false;
  }

  const auto* user_locales = targeting.GetUserLocales();
  if (user_locales && !Contains(*user_locales, client_->GetUserLocale())) {
    return false;
  }

  const auto registered_time_targeting = targeting.GetRegisteredTime();
  if (!MatchRegisteredTime(registered_time_targeting)) {
    return false;
  }

  const auto device_age_in_hours = targeting.GetDeviceAge();
  if (!MatchDeviceAge(device_age_in_hours)) {
    return false;
  }

  const auto* included_countries = targeting.GetIncludedCountries();
  if (included_countries &&
      !Contains(*included_countries, client_->GetCountryCode())) {
    return false;
  }

  const auto* excluded_countries = targeting.GetExcludedCountries();
  if (excluded_countries &&
      Contains(*excluded_countries, client_->GetCountryCode())) {
    return false;
  }

  return MatchMilestone(targeting) && MatchMilestoneVersion(targeting);
}

bool CampaignsMatcher::MatchRegisteredTime(
    const std::unique_ptr<TimeWindowTargeting>& registered_time_targeting)
    const {
  if (!registered_time_targeting) {
    // Match campaign if there is no registered date targeting.
    return true;
  }

  // TODO: b/333458177 - The `oobe_complete_time_` is not available when testing
  // in x11 emulator. Add support make it testable in x11 emulator.
  return MatchTimeWindow(*registered_time_targeting, oobe_compelete_time_);
}

bool CampaignsMatcher::MatchDeviceAge(
    const std::unique_ptr<NumberRangeTargeting>& device_age_in_hours) const {
  if (!device_age_in_hours) {
    // Match campaign if there is no device age targeting.
    return true;
  }

  // TODO: b/333458177 - The `oobe_complete_time_` is not available when testing
  // in x11 emulator. Add support make it testable in x11 emulator.
  auto start_time = base::Time::Min();
  const auto device_age_start = device_age_in_hours->GetStart();
  if (device_age_start) {
    start_time = oobe_compelete_time_ + base::Hours(device_age_start.value());
  }

  auto end_time = base::Time::Max();
  const auto device_age_end = device_age_in_hours->GetEnd();
  if (device_age_end) {
    end_time = oobe_compelete_time_ + base::Hours(device_age_end.value());
  }

  return MatchTimeWindow(start_time, end_time,
                         /*target=*/base::Time::Now());
}

bool CampaignsMatcher::MatchTriggerTargeting(
    const std::vector<std::unique_ptr<TriggerTargeting>>& trigger_targetings)
    const {
  if (trigger_targetings.empty()) {
    // Campaigns matched if `trigger_targetings` is empty.
    return true;
  }

  for (const auto& trigger : trigger_targetings) {
    auto trigger_type = trigger->GetTriggerType();
    if (!trigger_type) {
      // Ignore if trigger type is missing from the targeting.
      // TODO: b/341374525 - Record the error when the trigger type is missing.
      continue;
    }

    // TODO: b/330931877 - Add bounds check for casting to enum from value in
    // campaign.
    if (trigger_.type != static_cast<TriggerType>(trigger_type.value())) {
      continue;
    }

    // Only `kEvent` trigger needs to check event name, so other trigger type
    // is matched at this point.
    if (trigger_.type != TriggerType::kEvent) {
      return true;
    }

    const base::Value::List* trigger_events = trigger->GetTriggerEvents();
    if (!trigger_events) {
      // If the trigger type is `kEvent`, but the `trigger_events` is not valid,
      // does not match.
      // TODO: b/341164013 - Add new specific error type for this case.
      RecordCampaignsManagerError(CampaignsManagerError::kInvalidTrigger);
      CAMPAIGNS_LOG(ERROR)
          << "Invalid trigger events, requires a list of strings.";
      continue;
    }

    if (HasOverlapEntries(*trigger_events, trigger_.events)) {
      return true;
    }
  }

  CAMPAIGNS_LOG(DEBUG) << "TriggerTargeting is NOT matched.";
  return false;
}

bool CampaignsMatcher::MatchOpenedApp(
    const std::vector<std::unique_ptr<AppTargeting>>& apps_opened_targeting)
    const {
  if (apps_opened_targeting.empty()) {
    // Campaigns matched if apps opened targeting is empty.
    return true;
  }

  for (const auto& app : apps_opened_targeting) {
    auto* app_id = app->GetAppId();

    if (!app_id) {
      // Ignore if app id is missing from the targeting.
      continue;
    }

    if (*app_id == opened_app_id_) {
      return true;
    }
  }

  CAMPAIGNS_LOG(DEBUG) << "OpenedApp is NOT matched.";
  return false;
}

bool CampaignsMatcher::ReachCap(base::cstring_view campaign_type,
                                int id,
                                base::cstring_view event_type,
                                std::optional<int> cap) const {
  if (!cap) {
    // There is no cap, return false.
    return false;
  }

  std::map<std::string, std::string> conditions_params =
      CreateBasicConditionParams();
  // Event can be put in any key starting with `event_`.
  // Please see `components/feature_engagement/README.md#featureconfig`.
  conditions_params[kEventKey] = base::StringPrintf(
      "name:ChromeOSAshGrowthCampaigns_%s%d_%s;comparator:<%d;window:3650;"
      "storage:3650",
      campaign_type.c_str(), id, event_type.c_str(), cap.value());

  const bool reach_cap = !client_->WouldTriggerHelpUI(conditions_params);
  if (reach_cap) {
    CAMPAIGNS_LOG(DEBUG) << "Events are NOT matched. Campaign: " << id
                         << " reached cap for " << campaign_type << " "
                         << event_type;
  }
  return reach_cap;
}

bool CampaignsMatcher::MatchActiveUrlRegexes(
    const std::vector<std::string>& active_url_regrexes) const {
  if (active_url_regrexes.empty()) {
    // Campaigns matched if active URL targeting is empty.
    return true;
  }

  if (active_url_.is_empty()) {
    // Campaigns matched if no active URL is set. Active URL is used for
    // targeting web app and PWA. When active URL is empty, it is likely not
    // triggered by opening web app or PWA. In this case, defer to other
    // targeting to match campaign.
    // An example is G1 nudge is triggered by a group of app opened (PWA, Web
    // App and ARC app), the active URL targeting is used for PWA and Web App
    // while doesn't apply for ARC app.
    return true;
  }

  for (const auto& url_regrex : active_url_regrexes) {
    if (RE2::FullMatch(active_url_.spec(), url_regrex)) {
      return true;
    }
  }

  CAMPAIGNS_LOG(DEBUG) << "ActiveUrlRegexes is NOT matched.";
  return false;
}

bool CampaignsMatcher::MatchHotseatAppIcon(
    std::unique_ptr<AppTargeting> app) const {
  if (!app) {
    return true;
  }

  auto* app_id = app->GetAppId();
  if (!app_id) {
    // Ignore if app id is missing from the targeting.
    return true;
  }
  return client_->IsAppIconOnShelf(*app_id);
}

bool CampaignsMatcher::MatchEvents(std::unique_ptr<EventsTargeting> config,
                                   int campaign_id,
                                   std::optional<int> group_id) const {
  if (!config) {
    // Campaign is matched if there is no events targeting.
    return true;
  }

  // Check group impression cap and dismissal cap.
  if (group_id && (ReachCap("Group", group_id.value(), "Impression",
                            config->GetGroupImpressionCap()) ||
                   ReachCap("Group", group_id.value(), "Dismissed",
                            config->GetGroupDismissalCap()))) {
    // Reached group impression cap or dismissal cap.
    return false;
  }

  // Check campaign impression cap and dismissal cap.
  if (ReachCap("Campaign", campaign_id, "Impression",
               config->GetImpressionCap()) ||
      ReachCap("Campaign", campaign_id, "Dismissed",
               config->GetDismissalCap())) {
    return false;
  }

  std::map<std::string, std::string> conditions_params =
      CreateBasicConditionParams();
  // Here is to handle custom events targeting conditions.
  // The outer loop is AND logic and the inner loop is OR logic.
  const base::Value::List* conditions = config->GetEventsConditions();
  if (!conditions) {
    // Campaign is matched if there is no custom events targeting conditions.
    return true;
  }

  for (const auto& condition : *conditions) {
    if (!condition.is_list()) {
      RecordCampaignsManagerError(
          CampaignsManagerError::kInvalidEventTargetingCondition);
      CAMPAIGNS_LOG(ERROR) << "Invalid events targeting conditions.";
      return false;
    }

    bool any_event_matched = false;
    for (const auto& param : condition.GetList()) {
      if (!param.is_string()) {
        RecordCampaignsManagerError(
            CampaignsManagerError::kInvalidEventTargetingConditionParam);
        CAMPAIGNS_LOG(ERROR) << "Invalid events targeting condition.";
        return false;
      }

      std::string param_str = param.GetString();
      conditions_params[kEventKey] = param_str;
      if (client_->WouldTriggerHelpUI(conditions_params)) {
        any_event_matched = true;
        // Can break the loop if any condition is met.
        break;
      }
    }

    if (!any_event_matched) {
      // Can return if no condition is met.
      CAMPAIGNS_LOG(DEBUG)
          << "Events are NOT matched. Not meet custom events conditions.";
      return false;
    }
  }

  return true;
}

// Returns true if the users' minor mode status (e.g. under age of 18 or not)
// matches the `minor_user_targeting` capaiblity. The minor mode status is read
// from account capabilities. We assume user is in minor mode if capability
// value is unknown.
bool CampaignsMatcher::MatchMinorUser(
    std::optional<bool> minor_user_targeting) const {
  if (!minor_user_targeting) {
    // Campaign matched if it does no include minor targeting.
    return true;
  }

  signin::Tribool capability;
  if (manta_capability_for_testing_) {
    capability = manta_capability_for_testing_.value();
  } else {
    auto* identity_manager = client_->GetIdentityManager();
    if (!identity_manager) {
      // Identity manager is not available (e.g:guest mode). In that case,
      // a campaign with minor user targeting shouldn't be triggered.
      CAMPAIGNS_LOG(ERROR) << "IdentityManager is null.";
      return false;
    }
    std::string gaia_id = user_manager::UserManager::Get()
                              ->GetActiveUser()
                              ->GetAccountId()
                              .GetGaiaId();
    const AccountInfo account_info =
        identity_manager->FindExtendedAccountInfoByGaiaId(gaia_id);
    // TODO: b/333896450 - find a better signal for minor mode.
    capability = account_info.capabilities.can_use_manta_service();
  }

  if (capability == signin::Tribool::kUnknown) {
    // Records metrics when minor user state is unknown. This could be caused
    // by the delay of getting minor users state. We would like to log metric
    // to understand the impact while matching on the conservative side by
    // considering unknown as minor users.
    RecordCampaignsManagerError(CampaignsManagerError::kUnknownMinorUserState);
    return false;
  }

  bool isMinor = capability == signin::Tribool::kFalse;
  return isMinor == minor_user_targeting.value();
}

bool CampaignsMatcher::MatchOwner(std::optional<bool> is_owner) const {
  if (!is_owner) {
    // Campaigns matched if there is no owner targeting.
    return true;
  }

  return is_owner.value() == is_user_owner_;
}

bool CampaignsMatcher::MatchSessionTargeting(
    const SessionTargeting& targeting) const {
  if (!targeting.IsValid()) {
    // Campaigns matched if there is no session targeting.
    return true;
  }

  bool is_matched = false;
  is_matched = MatchExperimentTags(targeting.GetExperimentTags(),
                                   targeting.GetFeature());
  if (!is_matched) {
    CAMPAIGNS_LOG(DEBUG) << " ExperimentTags targeting is NOT matched.";
    return false;
  }

  is_matched = MatchMinorUser(targeting.GetMinorUser());
  if (!is_matched) {
    CAMPAIGNS_LOG(DEBUG) << "Please check MinorUser targeting.";
    return false;
  }

  is_matched = MatchOwner(targeting.GetIsOwner());
  if (!is_matched) {
    CAMPAIGNS_LOG(DEBUG) << "Owner targeting is NOT matched.";
    return false;
  }

  return true;
}

bool CampaignsMatcher::MatchRuntimeTargeting(
    const RuntimeTargeting& targeting,
    int campaign_id,
    std::optional<int> group_id) const {
  if (!targeting.IsValid()) {
    // Campaigns matched if there is no runtime targeting.
    return true;
  }

  bool is_matched = false;
  is_matched = MatchSchedulings(targeting.GetSchedulings());
  if (!is_matched) {
    CAMPAIGNS_LOG(DEBUG) << "Campaign: " << campaign_id
                         << " Schedulings targeting is NOT matched.";
    return false;
  }

  is_matched = MatchHotseatAppIcon(targeting.GetHotseatAppIcon());
  if (!is_matched) {
    CAMPAIGNS_LOG(DEBUG) << "Campaign: " << campaign_id
                         << " Hotseat app icon targeting is NOT matched.";
    return false;
  }

  is_matched = MatchOpenedApp(targeting.GetAppsOpened());
  if (!is_matched) {
    CAMPAIGNS_LOG(DEBUG) << "Campaign: " << campaign_id
                         << " Trigger targeting is NOT matched.";
    return false;
  }

  is_matched = MatchActiveUrlRegexes(targeting.GetActiveUrlRegexes());
  if (!is_matched) {
    CAMPAIGNS_LOG(DEBUG) << "Campaign: " << campaign_id
                         << " ActiveUrlRegexes targeting is NOT matched.";
    return false;
  }

  is_matched = MatchEvents(targeting.GetEventsConfig(), campaign_id, group_id);
  if (!is_matched) {
    CAMPAIGNS_LOG(DEBUG) << "Campaign: " << campaign_id
                         << " Events targeting is NOT matched.";
    return false;
  }

  is_matched = MatchUserPrefs(prefs_, targeting.GetUserPrefTargetings());
  if (!is_matched) {
    CAMPAIGNS_LOG(DEBUG) << "Campaign: " << campaign_id
                         << " UserPrefs targeting is NOT matched.";
    return false;
  }

  return true;
}

bool CampaignsMatcher::Matched(const Targeting* targeting,
                               int campaign_id,
                               std::optional<int> group_id,
                               bool is_prematch) const {
  if (!targeting) {
    // Targeting is invalid. Skip the current campaign.
    CAMPAIGNS_LOG(ERROR) << "Invalid targeting.";
    RecordCampaignsManagerError(CampaignsManagerError::kInvalidTargeting);
    return false;
  }

  if (is_prematch) {
    // TOOD(369188855): Re-enable demo mode prematch.
    return MatchDeviceTargeting(DeviceTargeting(targeting));
  }

  bool is_matched = MaybeMatchDemoModeTargeting(DemoModeTargeting(targeting));
  if (!is_matched) {
    CAMPAIGNS_LOG(DEBUG) << "Campaign: " << campaign_id
                         << " Demo Mode targeting is NOT matched.";
    return false;
  }

  // Skip other matchings if the trigger type doesn't match, which contributes
  // to the majority of the mismatch as we added event based triggering.
  RuntimeTargeting runtime_targeting = RuntimeTargeting(targeting);
  if (runtime_targeting.IsValid()) {
    is_matched = MatchTriggerTargeting(runtime_targeting.GetTriggers());
    if (!is_matched) {
      CAMPAIGNS_LOG(DEBUG) << "Campaign: " << campaign_id
                           << " Trigger targeting is NOT matched.";
      return false;
    }
  }

  is_matched = MatchSessionTargeting(SessionTargeting(targeting));
  if (!is_matched) {
    CAMPAIGNS_LOG(DEBUG) << "Campaign: " << campaign_id
                         << " Session targeting is NOT matched.";
    return false;
  }

  is_matched =
      MatchRuntimeTargeting(RuntimeTargeting(targeting), campaign_id, group_id);
  if (!is_matched) {
    CAMPAIGNS_LOG(DEBUG) << "Campaign: " << campaign_id
                         << " Runtime targeting is NOT matched.";
    return false;
  }

  return true;
}

bool CampaignsMatcher::Matched(const Targetings* targetings,
                               int campaign_id,
                               std::optional<int> group_id,
                               bool is_prematch) const {
  if (!targetings || targetings->empty()) {
    return true;
  }

  for (const auto& targeting : *targetings) {
    if (Matched(targeting.GetIfDict(), campaign_id, group_id, is_prematch)) {
      return true;
    }
  }

  return false;
}

}  // namespace growth
