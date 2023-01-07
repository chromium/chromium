// Copyright 2016 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/permissions/permission_decision_auto_blocker.h"

#include <algorithm>
#include <memory>
#include <string>
#include <utility>

#include "base/cxx17_backports.h"
#include "base/feature_list.h"
#include "base/metrics/field_trial_params.h"
#include "base/strings/string_number_conversions.h"
#include "base/values.h"
#include "components/content_settings/core/browser/host_content_settings_map.h"
#include "components/keyed_service/content/browser_context_dependency_manager.h"
#include "components/permissions/features.h"
#include "components/permissions/permission_util.h"
#include "url/gurl.h"

namespace permissions {
namespace {

constexpr int kDefaultDismissalsBeforeBlock = 3;
constexpr int kDefaultIgnoresBeforeBlock = 4;
constexpr int kDefaultDismissalsBeforeBlockWithQuietUi = 1;
constexpr int kDefaultIgnoresBeforeBlockWithQuietUi = 2;
constexpr int kDefaultEmbargoDays = 7;

// The number of times that users may explicitly dismiss a
// FEDERATED_IDENTITY_API permission prompt from an origin before it is
// automatically blocked.
constexpr int kFederatedIdentityApiDismissalsBeforeBlock = 1;

// The durations that an origin will stay under embargo for the
// FEDERATED_IDENTITY_API permission due to the user explicitly dismissing the
// permission prompt.
constexpr base::TimeDelta kFederatedIdentityApiEmbargoDurationDismiss[] = {
    base::Hours(2) /* 1st dismissal */, base::Days(1) /* 2nd dismissal */,
    base::Days(7), base::Days(28)};

// The number of times that users may explicitly dismiss a permission prompt
// from an origin before it is automatically blocked.
int g_dismissals_before_block = kDefaultDismissalsBeforeBlock;

// The number of times that users may ignore a permission prompt from an origin
// before it is automatically blocked.
int g_ignores_before_block = kDefaultIgnoresBeforeBlock;

// The number of times that users may dismiss a permission prompt that uses the
// quiet UI from an origin before it is automatically blocked.
int g_dismissals_before_block_with_quiet_ui =
    kDefaultDismissalsBeforeBlockWithQuietUi;

// The number of times that users may ignore a permission prompt that uses the
// quiet UI from an origin before it is automatically blocked.
int g_ignores_before_block_with_quiet_ui =
    kDefaultIgnoresBeforeBlockWithQuietUi;

// The number of days that an origin will stay under embargo for a requested
// permission due to repeated dismissals.
int g_dismissal_embargo_days = kDefaultEmbargoDays;

// The number of days that an origin will stay under embargo for a requested
// permission due to repeated ignores.
int g_ignore_embargo_days = kDefaultEmbargoDays;

std::string GetStringForContentType(ContentSettingsType content_type) {
  if (content_type == ContentSettingsType::FEDERATED_IDENTITY_API)
    return "FederatedIdentityApi";
  return PermissionUtil::GetPermissionString(content_type);
}

std::unique_ptr<base::Value> GetOriginAutoBlockerData(
    HostContentSettingsMap* settings,
    const GURL& origin_url) {
  base::Value website_setting = settings->GetWebsiteSetting(
      origin_url, GURL(), ContentSettingsType::PERMISSION_AUTOBLOCKER_DATA,
      nullptr);
  if (!website_setting.is_dict())
    return std::make_unique<base::Value>(base::Value::Type::DICTIONARY);

  return base::Value::ToUniquePtrValue(std::move(website_setting));
}

base::Value* GetOrCreatePermissionDict(base::Value* origin_dict,
                                       const std::string& permission) {
  base::Value* permission_dict =
      origin_dict->FindKeyOfType(permission, base::Value::Type::DICTIONARY);
  if (permission_dict)
    return permission_dict;
  return origin_dict->SetKey(permission,
                             base::Value(base::Value::Type::DICTIONARY));
}

int RecordActionInWebsiteSettings(const GURL& url,
                                  ContentSettingsType permission,
                                  const char* key,
                                  HostContentSettingsMap* settings_map) {
  std::unique_ptr<base::Value> dict =
      GetOriginAutoBlockerData(settings_map, url);

  base::Value* permission_dict = GetOrCreatePermissionDict(
      dict.get(), GetStringForContentType(permission));

  base::Value* value =
      permission_dict->FindKeyOfType(key, base::Value::Type::INTEGER);
  int current_count = value ? value->GetInt() : 0;
  permission_dict->SetKey(key, base::Value(++current_count));

  settings_map->SetWebsiteSettingDefaultScope(
      url, GURL(), ContentSettingsType::PERMISSION_AUTOBLOCKER_DATA,
      base::Value::FromUniquePtrValue(std::move(dict)));

  return current_count;
}

int GetActionCount(const GURL& url,
                   ContentSettingsType permission,
                   const char* key,
                   HostContentSettingsMap* settings_map) {
  std::unique_ptr<base::Value> dict =
      GetOriginAutoBlockerData(settings_map, url);
  base::Value* permission_dict = GetOrCreatePermissionDict(
      dict.get(), GetStringForContentType(permission));

  base::Value* value =
      permission_dict->FindKeyOfType(key, base::Value::Type::INTEGER);
  return value ? value->GetInt() : 0;
}

// Returns the number of times that users may explicitly dismiss a permission
// prompt for an origin for the passed-in |permission| before it is
// automatically blocked.
int GetDismissalsBeforeBlockForContentSettingsType(
    ContentSettingsType permission) {
  return (permission == ContentSettingsType::FEDERATED_IDENTITY_API)
             ? kFederatedIdentityApiDismissalsBeforeBlock
             : g_dismissals_before_block;
}

// The duration that an origin will stay under embargo for the passed-in
// |permission| due to the user explicitly dismissing the permission prompt.
base::TimeDelta GetEmbargoDurationForContentSettingsType(
    ContentSettingsType permission,
    int dismiss_count) {
  if (permission == ContentSettingsType::FEDERATED_IDENTITY_API) {
    int duration_index = base::clamp(
        dismiss_count - 1, 0,
        static_cast<int>(
            std::size(kFederatedIdentityApiEmbargoDurationDismiss) - 1));
    return kFederatedIdentityApiEmbargoDurationDismiss[duration_index];
  }
  return base::Days(g_dismissal_embargo_days);
}

base::Time GetEmbargoStartTime(base::Value* permission_dict,
                               const base::Feature& feature,
                               const char* key) {
  base::Value* found =
      permission_dict->FindKeyOfType(key, base::Value::Type::DOUBLE);
  if (found && base::FeatureList::IsEnabled(feature)) {
    return base::Time::FromDeltaSinceWindowsEpoch(
        base::Microseconds(found->GetDouble()));
  }
  return base::Time();
}

bool IsUnderEmbargo(base::Value* permission_dict,
                    const base::Feature& feature,
                    const char* key,
                    base::Time current_time,
                    base::TimeDelta offset) {
  base::Value* found =
      permission_dict->FindKeyOfType(key, base::Value::Type::DOUBLE);
  if (found && base::FeatureList::IsEnabled(feature) &&
      current_time <
          base::Time::FromInternalValue(found->GetDouble()) + offset) {
    return true;
  }

  return false;
}

void UpdateValueFromVariation(const std::string& variation_value,
                              int* value_store,
                              const int default_value) {
  int tmp_value = -1;
  if (base::StringToInt(variation_value, &tmp_value) && tmp_value > 0)
    *value_store = tmp_value;
  else
    *value_store = default_value;
}

}  // namespace

// static
const char PermissionDecisionAutoBlocker::kPromptDismissCountKey[] =
    "dismiss_count";

// static
const char PermissionDecisionAutoBlocker::kPromptIgnoreCountKey[] =
    "ignore_count";

// static
const char PermissionDecisionAutoBlocker::kPromptDismissCountWithQuietUiKey[] =
    "dismiss_count_quiet_ui";

// static
const char PermissionDecisionAutoBlocker::kPromptIgnoreCountWithQuietUiKey[] =
    "ignore_count_quiet_ui";

// static
const char PermissionDecisionAutoBlocker::kPermissionDismissalEmbargoKey[] =
    "dismissal_embargo_days";

// static
const char PermissionDecisionAutoBlocker::kPermissionIgnoreEmbargoKey[] =
    "ignore_embargo_days";

// static
bool PermissionDecisionAutoBlocker::IsEnabledForContentSetting(
    ContentSettingsType content_setting) {
  return PermissionUtil::IsPermission(content_setting) ||
         content_setting == ContentSettingsType::FEDERATED_IDENTITY_API;
}

// static
absl::optional<PermissionResult>
PermissionDecisionAutoBlocker::GetEmbargoResult(
    HostContentSettingsMap* settings_map,
    const GURL& request_origin,
    ContentSettingsType permission,
    base::Time current_time) {
  DCHECK(settings_map);
  DCHECK(IsEnabledForContentSetting(permission));

  std::unique_ptr<base::Value> dict =
      GetOriginAutoBlockerData(settings_map, request_origin);
  base::Value* permission_dict = GetOrCreatePermissionDict(
      dict.get(), GetStringForContentType(permission));

  int dismiss_count = GetActionCount(request_origin, permission,
                                     kPromptDismissCountKey, settings_map);
  if (IsUnderEmbargo(permission_dict, features::kBlockPromptsIfDismissedOften,
                     kPermissionDismissalEmbargoKey, current_time,
                     GetEmbargoDurationForContentSettingsType(permission,
                                                              dismiss_count))) {
    return PermissionResult(CONTENT_SETTING_BLOCK,
                            PermissionStatusSource::MULTIPLE_DISMISSALS);
  }

  if (IsUnderEmbargo(permission_dict, features::kBlockPromptsIfIgnoredOften,
                     kPermissionIgnoreEmbargoKey, current_time,
                     base::Days(g_ignore_embargo_days))) {
    return PermissionResult(CONTENT_SETTING_BLOCK,
                            PermissionStatusSource::MULTIPLE_IGNORES);
  }

  return absl::nullopt;
}

// static
void PermissionDecisionAutoBlocker::UpdateFromVariations() {
  std::string dismissals_before_block_value =
      base::GetFieldTrialParamValueByFeature(
          features::kBlockPromptsIfDismissedOften, kPromptDismissCountKey);
  std::string ignores_before_block_value =
      base::GetFieldTrialParamValueByFeature(
          features::kBlockPromptsIfIgnoredOften, kPromptIgnoreCountKey);
  std::string dismissals_before_block_value_with_quiet_ui =
      base::GetFieldTrialParamValueByFeature(
          features::kBlockPromptsIfDismissedOften,
          kPromptDismissCountWithQuietUiKey);
  std::string ignores_before_block_value_with_quiet_ui =
      base::GetFieldTrialParamValueByFeature(
          features::kBlockPromptsIfIgnoredOften,
          kPromptIgnoreCountWithQuietUiKey);
  std::string dismissal_embargo_days_value =
      base::GetFieldTrialParamValueByFeature(
          features::kBlockPromptsIfDismissedOften,
          kPermissionDismissalEmbargoKey);
  std::string ignore_embargo_days_value =
      base::GetFieldTrialParamValueByFeature(
          features::kBlockPromptsIfIgnoredOften, kPermissionIgnoreEmbargoKey);

  // If converting the value fails, revert to the original value.
  UpdateValueFromVariation(dismissals_before_block_value,
                           &g_dismissals_before_block,
                           kDefaultDismissalsBeforeBlock);
  UpdateValueFromVariation(ignores_before_block_value, &g_ignores_before_block,
                           kDefaultIgnoresBeforeBlock);
  UpdateValueFromVariation(dismissals_before_block_value_with_quiet_ui,
                           &g_dismissals_before_block_with_quiet_ui,
                           kDefaultDismissalsBeforeBlockWithQuietUi);
  UpdateValueFromVariation(ignores_before_block_value_with_quiet_ui,
                           &g_ignores_before_block_with_quiet_ui,
                           kDefaultIgnoresBeforeBlockWithQuietUi);
  UpdateValueFromVariation(dismissal_embargo_days_value,
                           &g_dismissal_embargo_days, kDefaultEmbargoDays);
  UpdateValueFromVariation(ignore_embargo_days_value, &g_ignore_embargo_days,
                           kDefaultEmbargoDays);
}

bool PermissionDecisionAutoBlocker::IsEmbargoed(
    const GURL& request_origin,
    ContentSettingsType permission) {
  return GetEmbargoResult(request_origin, permission).has_value();
}

absl::optional<PermissionResult>
PermissionDecisionAutoBlocker::GetEmbargoResult(
    const GURL& request_origin,
    ContentSettingsType permission) {
  return GetEmbargoResult(settings_map_, request_origin, permission,
                          clock_->Now());
}

base::Time PermissionDecisionAutoBlocker::GetEmbargoStartTime(
    const GURL& request_origin,
    ContentSettingsType permission) {
  DCHECK(settings_map_);
  std::unique_ptr<base::Value> dict =
      GetOriginAutoBlockerData(settings_map_, request_origin);
  base::Value* permission_dict = GetOrCreatePermissionDict(
      dict.get(), GetStringForContentType(permission));

  // A permission may have a record for both dismisal and ignore, return the
  // most recent. A permission will only actually be under one embargo, but
  // the record of embargo start will persist until explicitly deleted
  base::Time dismissal_start_time = permissions::GetEmbargoStartTime(
      permission_dict, features::kBlockPromptsIfDismissedOften,
      kPermissionDismissalEmbargoKey);
  base::Time ignore_start_time = permissions::GetEmbargoStartTime(
      permission_dict, features::kBlockPromptsIfIgnoredOften,
      kPermissionIgnoreEmbargoKey);

  return dismissal_start_time > ignore_start_time ? dismissal_start_time
                                                  : ignore_start_time;
}

std::set<GURL> PermissionDecisionAutoBlocker::GetEmbargoedOrigins(
    ContentSettingsType content_type) {
  return GetEmbargoedOrigins(std::vector<ContentSettingsType>{content_type});
}

std::set<GURL> PermissionDecisionAutoBlocker::GetEmbargoedOrigins(
    std::vector<ContentSettingsType> content_types) {
  DCHECK(settings_map_);

  std::vector<ContentSettingsType> filtered_content_types;
  for (ContentSettingsType content_type : content_types) {
    if (IsEnabledForContentSetting(content_type))
      filtered_content_types.emplace_back(content_type);
  }
  if (filtered_content_types.empty())
    return std::set<GURL>();

  ContentSettingsForOneType embargo_settings;
  settings_map_->GetSettingsForOneType(
      ContentSettingsType::PERMISSION_AUTOBLOCKER_DATA, &embargo_settings);
  std::set<GURL> origins;
  for (const auto& e : embargo_settings) {
    for (auto content_type : filtered_content_types) {
      const GURL url(e.primary_pattern.ToString());
      if (IsEmbargoed(url, content_type)) {
        origins.insert(url);
        break;
      }
    }
  }
  return origins;
}

int PermissionDecisionAutoBlocker::GetDismissCount(
    const GURL& url,
    ContentSettingsType permission) {
  return GetActionCount(url, permission, kPromptDismissCountKey, settings_map_);
}

int PermissionDecisionAutoBlocker::GetIgnoreCount(
    const GURL& url,
    ContentSettingsType permission) {
  return GetActionCount(url, permission, kPromptIgnoreCountKey, settings_map_);
}

bool PermissionDecisionAutoBlocker::RecordDismissAndEmbargo(
    const GURL& url,
    ContentSettingsType permission,
    bool dismissed_prompt_was_quiet) {
  int current_dismissal_count = RecordActionInWebsiteSettings(
      url, permission, kPromptDismissCountKey, settings_map_);

  int current_dismissal_count_with_quiet_ui =
      dismissed_prompt_was_quiet
          ? RecordActionInWebsiteSettings(url, permission,
                                          kPromptDismissCountWithQuietUiKey,
                                          settings_map_)
          : -1;

  // TODO(dominickn): ideally we would have a method
  // PermissionContextBase::ShouldEmbargoAfterRepeatedDismissals() to specify
  // if a permission is opted in. This is difficult right now because:
  // 1. PermissionQueueController needs to call this method at a point where it
  //    does not have a PermissionContextBase available
  // 2. Not calling RecordDismissAndEmbargo means no repeated dismissal metrics
  //    are recorded
  if (base::FeatureList::IsEnabled(features::kBlockPromptsIfDismissedOften)) {
    if (current_dismissal_count >=
        GetDismissalsBeforeBlockForContentSettingsType(permission)) {
      PlaceUnderEmbargo(url, permission, kPermissionDismissalEmbargoKey);
      return true;
    }

    if (current_dismissal_count_with_quiet_ui >=
        g_dismissals_before_block_with_quiet_ui) {
      DCHECK(permission == ContentSettingsType::NOTIFICATIONS ||
             permission == ContentSettingsType::GEOLOCATION);
      PlaceUnderEmbargo(url, permission, kPermissionDismissalEmbargoKey);
      return true;
    }
  }
  return false;
}

bool PermissionDecisionAutoBlocker::RecordIgnoreAndEmbargo(
    const GURL& url,
    ContentSettingsType permission,
    bool ignored_prompt_was_quiet) {
  int current_ignore_count = RecordActionInWebsiteSettings(
      url, permission, kPromptIgnoreCountKey, settings_map_);

  int current_ignore_count_with_quiet_ui =
      ignored_prompt_was_quiet
          ? RecordActionInWebsiteSettings(url, permission,
                                          kPromptIgnoreCountWithQuietUiKey,
                                          settings_map_)
          : -1;

  if (base::FeatureList::IsEnabled(features::kBlockPromptsIfIgnoredOften)) {
    if (current_ignore_count >= g_ignores_before_block) {
      PlaceUnderEmbargo(url, permission, kPermissionIgnoreEmbargoKey);
      return true;
    }

    if (current_ignore_count_with_quiet_ui >=
        g_ignores_before_block_with_quiet_ui) {
      DCHECK(permission == ContentSettingsType::NOTIFICATIONS ||
             permission == ContentSettingsType::GEOLOCATION);
      PlaceUnderEmbargo(url, permission, kPermissionIgnoreEmbargoKey);
      return true;
    }
  }

  return false;
}

void PermissionDecisionAutoBlocker::RemoveEmbargoAndResetCounts(
    const GURL& url,
    ContentSettingsType permission) {
  if (!IsEnabledForContentSetting(permission))
    return;

  std::unique_ptr<base::Value> dict =
      GetOriginAutoBlockerData(settings_map_, url);

  dict->RemoveKey(GetStringForContentType(permission));

  settings_map_->SetWebsiteSettingDefaultScope(
      url, GURL(), ContentSettingsType::PERMISSION_AUTOBLOCKER_DATA,
      base::Value::FromUniquePtrValue(std::move(dict)));
}

void PermissionDecisionAutoBlocker::RemoveEmbargoAndResetCounts(
    base::RepeatingCallback<bool(const GURL& url)> filter) {
  std::unique_ptr<ContentSettingsForOneType> settings(
      new ContentSettingsForOneType);
  settings_map_->GetSettingsForOneType(
      ContentSettingsType::PERMISSION_AUTOBLOCKER_DATA, settings.get());

  for (const auto& site : *settings) {
    GURL origin(site.primary_pattern.ToString());

    if (origin.is_valid() && filter.Run(origin)) {
      settings_map_->SetWebsiteSettingDefaultScope(
          origin, GURL(), ContentSettingsType::PERMISSION_AUTOBLOCKER_DATA,
          base::Value());
    }
  }
}

void PermissionDecisionAutoBlocker::AddObserver(Observer* obs) {
  observers_.AddObserver(obs);
}

void PermissionDecisionAutoBlocker::RemoveObserver(Observer* obs) {
  observers_.RemoveObserver(obs);
}

// static
const char*
PermissionDecisionAutoBlocker::GetPromptDismissCountKeyForTesting() {
  return kPromptDismissCountKey;
}

PermissionDecisionAutoBlocker::PermissionDecisionAutoBlocker(
    HostContentSettingsMap* settings_map)
    : settings_map_(settings_map), clock_(base::DefaultClock::GetInstance()) {}

PermissionDecisionAutoBlocker::~PermissionDecisionAutoBlocker() {}

void PermissionDecisionAutoBlocker::PlaceUnderEmbargo(
    const GURL& request_origin,
    ContentSettingsType permission,
    const char* key) {
  std::unique_ptr<base::Value> dict =
      GetOriginAutoBlockerData(settings_map_, request_origin);
  base::Value* permission_dict = GetOrCreatePermissionDict(
      dict.get(), GetStringForContentType(permission));
  permission_dict->SetKey(
      key, base::Value(static_cast<double>(clock_->Now().ToInternalValue())));
  settings_map_->SetWebsiteSettingDefaultScope(
      request_origin, GURL(), ContentSettingsType::PERMISSION_AUTOBLOCKER_DATA,
      base::Value::FromUniquePtrValue(std::move(dict)));
  NotifyEmbargoStarted(request_origin, permission);
}

void PermissionDecisionAutoBlocker::NotifyEmbargoStarted(
    const GURL& origin,
    ContentSettingsType content_setting) {
  for (Observer& obs : observers_)
    obs.OnEmbargoStarted(origin, content_setting);
}

void PermissionDecisionAutoBlocker::SetClockForTesting(base::Clock* clock) {
  clock_ = clock;
}

}  // namespace permissions
