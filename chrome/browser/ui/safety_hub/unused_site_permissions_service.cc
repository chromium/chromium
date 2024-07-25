// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/safety_hub/unused_site_permissions_service.h"

#include <memory>
#include <string>

#include "base/check.h"
#include "base/feature_list.h"
#include "base/functional/bind.h"
#include "base/functional/callback_helpers.h"
#include "base/json/values_util.h"
#include "base/memory/scoped_refptr.h"
#include "base/memory/weak_ptr.h"
#include "base/metrics/field_trial_params.h"
#include "base/metrics/histogram_functions.h"
#include "base/run_loop.h"
#include "base/strings/string_number_conversions.h"
#include "base/task/task_traits.h"
#include "base/task/thread_pool.h"
#include "base/time/clock.h"
#include "base/time/default_clock.h"
#include "base/time/time.h"
#include "base/values.h"
#include "build/build_config.h"
#include "chrome/app/chrome_command_ids.h"
#include "chrome/browser/browser_process.h"
#include "chrome/browser/safe_browsing/safe_browsing_service.h"
#include "chrome/browser/ui/safety_hub/safety_hub_prefs.h"
#include "chrome/browser/ui/safety_hub/safety_hub_service.h"
#include "chrome/browser/ui/safety_hub/safety_hub_util.h"
#include "chrome/common/chrome_features.h"
#include "chrome/grit/generated_resources.h"
#include "components/content_settings/core/browser/content_settings_info.h"
#include "components/content_settings/core/browser/content_settings_registry.h"
#include "components/content_settings/core/browser/content_settings_uma_util.h"
#include "components/content_settings/core/browser/content_settings_utils.h"
#include "components/content_settings/core/browser/host_content_settings_map.h"
#include "components/content_settings/core/browser/website_settings_info.h"
#include "components/content_settings/core/browser/website_settings_registry.h"
#include "components/content_settings/core/common/content_settings.h"
#include "components/content_settings/core/common/content_settings_constraints.h"
#include "components/content_settings/core/common/content_settings_pattern.h"
#include "components/content_settings/core/common/content_settings_types.h"
#include "components/content_settings/core/common/content_settings_utils.h"
#include "components/content_settings/core/common/features.h"
#include "components/permissions/constants.h"
#include "components/permissions/permission_uma_util.h"
#include "components/permissions/pref_names.h"
#include "components/prefs/pref_change_registrar.h"
#include "components/prefs/pref_service.h"
#include "components/safe_browsing/core/common/features.h"
#include "components/safe_browsing/core/common/safe_browsing_prefs.h"
#include "content/public/browser/browser_context.h"
#include "content/public/browser/browser_thread.h"
#include "ui/base/l10n/l10n_util.h"
#include "url/gurl.h"
#include "url/origin.h"

constexpr base::TimeDelta kRevocationThresholdNoDelayForTesting = base::Days(0);
constexpr base::TimeDelta kRevocationThresholdWithDelayForTesting =
    base::Minutes(5);

namespace {

constexpr char kUnknownContentSettingsType[] = "unknown";

// Reflects the maximum number of days between a permissions being revoked and
// the time when the user regrants the permission through the unused site
// permission module of Safete Check. The maximum number of days is determined
// by `kRevocationCleanUpThreshold`.
size_t kAllowAgainMetricsExclusiveMaxCount = 31;

// Using a single bucket per day, following the value of
// |kAllowAgainMetricsExclusiveMaxCount|.
size_t kAllowAgainMetricsBuckets = 31;

base::TimeDelta GetRevocationThreshold() {
  // TODO(crbug.com/40250875): Clean up no delay revocation after the feature is
  // ready. Today, no delay revocation is necessary to enable manual testing.
  if (content_settings::features::kSafetyCheckUnusedSitePermissionsNoDelay
          .Get()) {
    return kRevocationThresholdNoDelayForTesting;
  } else if (content_settings::features::
                 kSafetyCheckUnusedSitePermissionsWithDelay.Get()) {
    return kRevocationThresholdWithDelayForTesting;
  }
  return content_settings::features::
      kSafetyCheckUnusedSitePermissionsRevocationThreshold.Get();
}

bool IsContentSetting(ContentSettingsType type) {
  auto* content_setting_registry =
      content_settings::ContentSettingsRegistry::GetInstance();
  return content_setting_registry->Get(type);
}

bool IsWebsiteSetting(ContentSettingsType type) {
  auto* website_setting_registry =
      content_settings::WebsiteSettingsRegistry::GetInstance();
  return website_setting_registry->Get(type);
}

bool IsChooserPermissionSupported() {
  return base::FeatureList::IsEnabled(
      content_settings::features::
          kSafetyCheckUnusedSitePermissionsForSupportedChooserPermissions);
}

base::Value::List ConvertContentSettingsIntValuesToString(
    const base::Value::List& content_settings_values_list,
    bool* successful_migration) {
  base::Value::List string_value_list;
  for (const base::Value& setting_value : content_settings_values_list) {
    if (setting_value.is_int()) {
      int setting_int = setting_value.GetInt();
      auto setting_name =
          UnusedSitePermissionsService::ConvertContentSettingsTypeToKey(
              static_cast<ContentSettingsType>(setting_int));
      if (setting_name == kUnknownContentSettingsType) {
        *successful_migration = false;
        string_value_list.Append(setting_value.GetInt());
      } else {
        string_value_list.Append(setting_name);
      }
    } else {
      DCHECK(setting_value.is_string());
      // Store string group name values.
      string_value_list.Append(setting_value.GetString());
    }
  }
  return string_value_list;
}

base::Value::Dict ConvertChooserContentSettingsIntValuesToString(
    const base::Value::Dict& chooser_content_settings_values_dict) {
  base::Value::Dict string_keyed_dict;
  for (const auto [key, value] : chooser_content_settings_values_dict) {
    int number = -1;
    base::StringToInt(key, &number);
    // If number conversion fails it returns 0 which is not a chooser permission
    // enum value so it will not clash with the conversion.
    if (number == 0) {
      // Store string keyed values as is.
      string_keyed_dict.Set(key, value.GetDict().Clone());
    } else {
      string_keyed_dict.Set(
          UnusedSitePermissionsService::ConvertContentSettingsTypeToKey(
              static_cast<ContentSettingsType>(number)),
          value.GetDict().Clone());
    }
  }
  return string_keyed_dict;
}

}  // namespace

// static
std::string UnusedSitePermissionsService::ConvertContentSettingsTypeToKey(
    ContentSettingsType type) {
  auto* website_setting_registry =
      content_settings::WebsiteSettingsRegistry::GetInstance();
  DCHECK(website_setting_registry);

  auto* website_settings_info = website_setting_registry->Get(type);
  if (!website_settings_info) {
    auto integer_type = static_cast<int32_t>(type);
    DVLOG(1) << "Couldn't retrieve website settings info entry from the "
                "registry for type: "
             << integer_type;
    base::UmaHistogramSparse(
        "Settings.SafetyCheck.UnusedSitePermissionsMigrationFail",
        integer_type);
    return kUnknownContentSettingsType;
  }

  return website_settings_info->name();
}

// static
ContentSettingsType
UnusedSitePermissionsService::ConvertKeyToContentSettingsType(
    const std::string& key) {
  auto* website_setting_registry =
      content_settings::WebsiteSettingsRegistry::GetInstance();
  return website_setting_registry->GetByName(key)->type();
}

// static
url::Origin UnusedSitePermissionsService::ConvertPrimaryPatternToOrigin(
    const ContentSettingsPattern& primary_pattern) {
  GURL origin_url = GURL(primary_pattern.ToString());
  CHECK(origin_url.is_valid());

  return url::Origin::Create(origin_url);
}

base::TimeDelta UnusedSitePermissionsService::GetRepeatedUpdateInterval() {
  return content_settings::features::
      kSafetyCheckUnusedSitePermissionsRepeatedUpdateInterval.Get();
}

UnusedSitePermissionsService::TabHelper::TabHelper(
    content::WebContents* web_contents,
    UnusedSitePermissionsService* unused_site_permission_service)
    : content::WebContentsObserver(web_contents),
      content::WebContentsUserData<TabHelper>(*web_contents),
      unused_site_permission_service_(
          unused_site_permission_service->AsWeakPtr()) {}

UnusedSitePermissionsService::TabHelper::~TabHelper() = default;

PermissionsData::PermissionsData() = default;

PermissionsData::~PermissionsData() = default;

PermissionsData::PermissionsData(const PermissionsData& other)
    : primary_pattern(other.primary_pattern),
      permission_types(other.permission_types),
      constraints(other.constraints),
      abusive_revocation_constraints(other.abusive_revocation_constraints) {
  chooser_permissions_data = other.chooser_permissions_data.Clone();
}

UnusedSitePermissionsService::UnusedSitePermissionsResult::
    UnusedSitePermissionsResult() = default;
UnusedSitePermissionsService::UnusedSitePermissionsResult::
    ~UnusedSitePermissionsResult() = default;

UnusedSitePermissionsService::UnusedSitePermissionsResult::
    UnusedSitePermissionsResult(const UnusedSitePermissionsResult&) = default;

std::unique_ptr<SafetyHubService::Result>
UnusedSitePermissionsService::UnusedSitePermissionsResult::Clone() const {
  return std::make_unique<UnusedSitePermissionsResult>(*this);
}

void UnusedSitePermissionsService::UnusedSitePermissionsResult::
    AddRevokedPermission(const PermissionsData& permissions_data) {
  revoked_permissions_.push_back(std::move(permissions_data));
}

std::list<PermissionsData> UnusedSitePermissionsService::
    UnusedSitePermissionsResult::GetRevokedPermissions() {
  std::list<PermissionsData> result(revoked_permissions_);
  return result;
}

std::set<ContentSettingsPattern>
UnusedSitePermissionsService::UnusedSitePermissionsResult::GetRevokedOrigins()
    const {
  std::set<ContentSettingsPattern> origins;
  for (auto permission : revoked_permissions_) {
    origins.insert(permission.primary_pattern);
  }
  return origins;
}

base::Value::Dict
UnusedSitePermissionsService::UnusedSitePermissionsResult::ToDictValue() const {
  base::Value::Dict result = BaseToDictValue();
  base::Value::List revoked_origins;
  for (auto permission : revoked_permissions_) {
    revoked_origins.Append(permission.primary_pattern.ToString());
  }
  result.Set(kUnusedSitePermissionsResultKey, std::move(revoked_origins));
  return result;
}

bool UnusedSitePermissionsService::UnusedSitePermissionsResult::
    IsTriggerForMenuNotification() const {
  // A menu notification should be shown when there is at least one permission
  // that was revoked.
  return !GetRevokedOrigins().empty();
}

bool UnusedSitePermissionsService::UnusedSitePermissionsResult::
    WarrantsNewMenuNotification(
        const base::Value::Dict& previous_result_dict) const {
  std::set<ContentSettingsPattern> old_origins;
  for (const base::Value& origin_val :
       *previous_result_dict.FindList(kUnusedSitePermissionsResultKey)) {
    // Before crrev.com/c/5000387, the revoked permissions were stored in a dict
    // that looked as follows:
    // {
    //    "origin": "site.com",
    //    "permissionTypes": [...permissions],
    //    "expiration": TimeValue
    // }
    // After this CL, the list was updated to a list of strings representing
    // the origins. To maintain backwards compatibility, support these
    // old values for now. This check can be deleted in the future.
    const std::string* origin_str{};
    if (origin_val.is_dict()) {
      const base::Value::Dict& revoked_permission = origin_val.GetDict();
      origin_str = revoked_permission.FindString(kSafetyHubOriginKey);
    } else if (origin_val.is_string()) {
      origin_str = &origin_val.GetString();
    } else {
      NOTREACHED_IN_MIGRATION();
    }
    ContentSettingsPattern origin =
        ContentSettingsPattern::FromString(*origin_str);
    old_origins.insert(origin);
  }

  std::set<ContentSettingsPattern> new_origins = GetRevokedOrigins();
  return !base::ranges::includes(old_origins, new_origins);
}

std::u16string UnusedSitePermissionsService::UnusedSitePermissionsResult::
    GetNotificationString() const {
  if (revoked_permissions_.empty()) {
    return std::u16string();
  }
  return l10n_util::GetPluralStringFUTF16(
      base::FeatureList::IsEnabled(
          safe_browsing::kSafetyHubAbusiveNotificationRevocation)
          ? IDS_SETTINGS_SAFETY_HUB_REVOKED_PERMISSIONS_MENU_NOTIFICATION
          : IDS_SETTINGS_SAFETY_HUB_UNUSED_SITE_PERMISSIONS_MENU_NOTIFICATION,
      revoked_permissions_.size());
}

int UnusedSitePermissionsService::UnusedSitePermissionsResult::
    GetNotificationCommandId() const {
  return IDC_OPEN_SAFETY_HUB;
}

void UnusedSitePermissionsService::TabHelper::PrimaryPageChanged(
    content::Page& page) {
  if (unused_site_permission_service_) {
    unused_site_permission_service_->OnPageVisited(
        page.GetMainDocument().GetLastCommittedOrigin());
  }
}

WEB_CONTENTS_USER_DATA_KEY_IMPL(UnusedSitePermissionsService::TabHelper);

UnusedSitePermissionsService::UnusedSitePermissionsService(
    content::BrowserContext* browser_context,
    PrefService* prefs)
    : browser_context_(browser_context),
      clock_(base::DefaultClock::GetInstance()) {
  DCHECK_CURRENTLY_ON(content::BrowserThread::UI);
  DCHECK(browser_context_);

  content_settings_observation_.Observe(hcsm());
  pref_change_registrar_ = std::make_unique<PrefChangeRegistrar>();
  pref_change_registrar_->Init(prefs);

  if (base::FeatureList::IsEnabled(features::kSafetyHub)) {
    pref_change_registrar_->Add(
        safety_hub_prefs::kUnusedSitePermissionsRevocationEnabled,
        base::BindRepeating(&UnusedSitePermissionsService::
                                OnPermissionsAutorevocationControlChanged,
                            base::Unretained(this)));
  }

  if (base::FeatureList::IsEnabled(
          safe_browsing::kSafetyHubAbusiveNotificationRevocation)) {
    abusive_notification_manager_ =
        std::make_unique<AbusiveNotificationPermissionsManager>(
            g_browser_process->safe_browsing_service()
                ? g_browser_process->safe_browsing_service()->database_manager()
                : nullptr,
            hcsm());

    pref_change_registrar_->Add(
        prefs::kSafeBrowsingEnabled,
        base::BindRepeating(&UnusedSitePermissionsService::
                                OnPermissionsAutorevocationControlChanged,
                            base::Unretained(this)));
  }

  bool migration_completed = pref_change_registrar_->prefs()->GetBoolean(
      safety_hub_prefs::kUnusedSitePermissionsRevocationMigrationCompleted);
  if (!migration_completed) {
    // Convert all integer permission values to string, if there is any
    // permission represented by integer stored in disk.
    // TODO(crbug.com/41495119): Clean up this migration after some milestones.
    UpdateIntegerValuesToGroupName();
  }

  InitializeLatestResult();

  if (IsUnusedSiteAutoRevocationEnabled() ||
      IsAbusiveNotificationAutoRevocationEnabled()) {
    StartRepeatedUpdates();
  }
}

UnusedSitePermissionsService::~UnusedSitePermissionsService() = default;

std::unique_ptr<SafetyHubService::Result>
UnusedSitePermissionsService::InitializeLatestResultImpl() {
  return GetRevokedPermissions();
}

void UnusedSitePermissionsService::OnContentSettingChanged(
    const ContentSettingsPattern& primary_pattern,
    const ContentSettingsPattern& secondary_pattern,
    ContentSettingsTypeSet content_type_set) {
  // When permissions change for a pattern it is either (1) through resetting
  // permissions, e.g. in page info or site settings, (2) user modifying
  // permissions manually, (3) through auto-revocation of unused site
  // permissions that this module performs, or (4) through auto-revocation of
  // abusive notifications that this module performs. In (1) and (2) the
  // pattern should no longer be shown to the user.
  bool should_clean_revoked_permission_data = true;

  // If `content_type_set` contains all types, all permissions are changed at
  // once and all revoked permissions should be cleaned up. Since `GetType()`
  // can only be called when `ContainsAllTypes()` is false, skip the switch in
  // this case.
  if (!content_type_set.ContainsAllTypes()) {
    switch (content_type_set.GetType()) {
      case ContentSettingsType::REVOKED_UNUSED_SITE_PERMISSIONS:
      case ContentSettingsType::REVOKED_ABUSIVE_NOTIFICATION_PERMISSIONS:
        // If content setting is changed because of auto revocation, the revoked
        // permission data should not be deleted.
        should_clean_revoked_permission_data = false;
        break;
      default:
        // If the permission is changed by users, then clean up the revoked
        // permissions data. However if the permission is changed because of
        // Safety Hub revocation, then the revoked permission data should not be
        // revoked.
        bool is_abusive_revocation_running =
            IsAbusiveNotificationAutoRevocationEnabled()
                ? abusive_notification_manager_->IsRevocationRunning()
                : false;
        should_clean_revoked_permission_data =
            !is_unused_site_revocation_running &&
            !is_abusive_revocation_running;
        break;
    }
  }

  if (should_clean_revoked_permission_data) {
    DeletePatternFromRevokedPermissionList(primary_pattern, secondary_pattern);
    if (IsAbusiveNotificationAutoRevocationEnabled()) {
      abusive_notification_manager_
          ->DeletePatternFromRevokedAbusiveNotificationList(primary_pattern,
                                                            secondary_pattern);
    }
  }
}

void UnusedSitePermissionsService::Shutdown() {
  content_settings_observation_.Reset();
}

void UnusedSitePermissionsService::RegrantPermissionsForOrigin(
    const url::Origin& origin) {
  if (IsAbusiveNotificationAutoRevocationEnabled()) {
    abusive_notification_manager_->RegrantPermissionForOriginIfNecessary(
        origin.GetURL());
  }

  content_settings::SettingInfo info;
  base::Value stored_value(hcsm()->GetWebsiteSetting(
      origin.GetURL(), origin.GetURL(),
      ContentSettingsType::REVOKED_UNUSED_SITE_PERMISSIONS, &info));

  if (!stored_value.is_dict()) {
    return;
  }

  base::Value::List* permission_type_list =
      stored_value.GetDict().FindList(permissions::kRevokedKey);
  CHECK(permission_type_list);
  // Set this to true to prevent `OnContentSettingChanged` from removing
  // revoked setting values, since this is set with specific values below.
  is_unused_site_revocation_running = true;
  for (auto& permission_type : *permission_type_list) {
    // Look up ContentSettingsRegistry to see if type is content setting
    // or website setting.
    ContentSettingsType type =
        ConvertKeyToContentSettingsType(permission_type.GetString());
    if (IsContentSetting(type)) {
      // ContentSettingsRegistry-based permissions with ALLOW value were
      // revoked; re-grant them by setting ALLOW again.
      hcsm()->SetContentSettingCustomScope(
          info.primary_pattern, info.secondary_pattern, type,
          ContentSetting::CONTENT_SETTING_ALLOW);
    } else if (IsChooserPermissionSupported() && IsWebsiteSetting(type)) {
      auto* chooser_permissions_data = stored_value.GetDict().FindDict(
          permissions::kRevokedChooserPermissionsKey);
      // There should be always data attached for a revoked chooser permission.
      DCHECK(chooser_permissions_data);
      // Chooser permissions are WebsiteSettingsRegistry-based, so it is
      // re-granted by restoring the previously revoked Website Setting value.
      auto* revoked_value = chooser_permissions_data->FindDict(
          ConvertContentSettingsTypeToKey(type));
      DCHECK(revoked_value);
      hcsm()->SetWebsiteSettingCustomScope(
          info.primary_pattern, info.secondary_pattern, type,
          base::Value(std::move(*revoked_value)));
    } else {
      NOTREACHED_IN_MIGRATION()
          << "Unable to find ContentSettingsType in neither "
          << "ContentSettingsRegistry nor WebsiteSettingsRegistry: "
          << ConvertContentSettingsTypeToKey(type);
    }
  }
  // Set this back to false, so that `OnContentSettingChanged` can cleanup
  // revoked settings if necessary.
  is_unused_site_revocation_running = false;

  // Ignore origin from future auto-revocations.
  IgnoreOriginForAutoRevocation(origin);

  // Remove origin from revoked permissions list.
  DeletePatternFromRevokedPermissionList(info.primary_pattern,
                                         info.secondary_pattern);

  // Record the days elapsed from auto-revocation to regrant.
  base::Time revoked_time =
      info.metadata.expiration() -
      content_settings::features::
          kSafetyCheckUnusedSitePermissionsRevocationCleanUpThreshold.Get();
  base::UmaHistogramCustomCounts(
      "Settings.SafetyCheck.UnusedSitePermissionsAllowAgainDays",
      (clock_->Now() - revoked_time).InDays(), 0,
      kAllowAgainMetricsExclusiveMaxCount, kAllowAgainMetricsBuckets);
}

void UnusedSitePermissionsService::UndoRegrantPermissionsForOrigin(
    const PermissionsData& permissions_data) {
  if (IsAbusiveNotificationAutoRevocationEnabled()) {
    abusive_notification_manager_->UndoRegrantPermissionForOriginIfNecessary(
        GURL(permissions_data.primary_pattern.ToString()),
        permissions_data.permission_types,
        permissions_data.abusive_revocation_constraints);
  }

  // If `permissions_data` had abusive notifications revoked, remove the
  // `NOTIFICATIONS` setting from the list of permission types to handle below,
  // since these were already handled. If there are no unused site permissions
  // to handle below, then return. Otherwise, handle them below.
  const std::set<ContentSettingsType> unused_site_permission_types =
      GetRevokedUnusedSitePermissionTypes(permissions_data.permission_types);
  if (unused_site_permission_types.empty()) {
    return;
  }

  // Set this to true to prevent `OnContentSettingChanged` from removing
  // revoked setting values, since this is set with specific values below.
  is_unused_site_revocation_running = true;
  for (const auto& permission : unused_site_permission_types) {
    if (IsContentSetting(permission)) {
      hcsm()->SetContentSettingCustomScope(
          permissions_data.primary_pattern, ContentSettingsPattern::Wildcard(),
          permission, ContentSetting::CONTENT_SETTING_DEFAULT);
    } else if (IsChooserPermissionSupported() && IsWebsiteSetting(permission)) {
      hcsm()->SetWebsiteSettingDefaultScope(
          permissions_data.primary_pattern.ToRepresentativeUrl(), GURL(),
          permission, base::Value());
    } else {
      NOTREACHED_IN_MIGRATION()
          << "Unable to find ContentSettingsType in neither "
          << "ContentSettingsRegistry nor WebsiteSettingsRegistry: "
          << ConvertContentSettingsTypeToKey(permission);
    }
  }
  // Set this back to false, so that `OnContentSettingChanged` can cleanup
  // revoked settings if necessary.
  is_unused_site_revocation_running = false;

  StorePermissionInRevokedPermissionSetting(
      unused_site_permission_types, permissions_data.chooser_permissions_data,
      permissions_data.constraints, permissions_data.primary_pattern,
      ContentSettingsPattern::Wildcard());
}

void UnusedSitePermissionsService::ClearRevokedPermissionsList() {
  if (IsAbusiveNotificationAutoRevocationEnabled()) {
    abusive_notification_manager_->ClearRevokedPermissionsList();
  }

  for (const auto& revoked_permissions : hcsm()->GetSettingsForOneType(
           ContentSettingsType::REVOKED_UNUSED_SITE_PERMISSIONS)) {
    DeletePatternFromRevokedPermissionList(
        revoked_permissions.primary_pattern,
        revoked_permissions.secondary_pattern);
  }
}

void UnusedSitePermissionsService::IgnoreOriginForAutoRevocation(
    const url::Origin& origin) {
  auto* registry = content_settings::ContentSettingsRegistry::GetInstance();

  for (const content_settings::ContentSettingsInfo* info : *registry) {
    ContentSettingsType type = info->website_settings_info()->type();

    for (const auto& setting : hcsm()->GetSettingsForOneType(type)) {
      if (setting.metadata.last_visited() != base::Time() &&
          setting.primary_pattern.MatchesSingleOrigin() &&
          setting.primary_pattern.Matches(origin.GetURL())) {
        hcsm()->ResetLastVisitedTime(setting.primary_pattern,
                                     setting.secondary_pattern, type);
        break;
      }
    }
  }
}

// Called by TabHelper when a URL was visited.
void UnusedSitePermissionsService::OnPageVisited(const url::Origin& origin) {
  DCHECK_CURRENTLY_ON(content::BrowserThread::UI);
  // Check if this origin has unused permissions.
  auto origin_entry = recently_unused_permissions_.find(origin.Serialize());
  if (origin_entry == recently_unused_permissions_.end()) {
    return;
  }

  // See which permissions of the origin actually match the URL and update them.
  auto& site_permissions = origin_entry->second;
  for (auto it = site_permissions.begin(); it != site_permissions.end();) {
    if (it->source.primary_pattern.Matches(origin.GetURL())) {
      hcsm()->UpdateLastVisitedTime(it->source.primary_pattern,
                                    it->source.secondary_pattern, it->type);
      site_permissions.erase(it++);
    } else {
      it++;
    }
  }
  // Remove origin entry if all permissions were updated.
  if (site_permissions.empty()) {
    recently_unused_permissions_.erase(origin_entry);
  }
}

void UnusedSitePermissionsService::DeletePatternFromRevokedPermissionList(
    const ContentSettingsPattern& primary_pattern,
    const ContentSettingsPattern& secondary_pattern) {
  hcsm()->SetWebsiteSettingCustomScope(
      primary_pattern, secondary_pattern,
      ContentSettingsType::REVOKED_UNUSED_SITE_PERMISSIONS, {});
}

base::OnceCallback<std::unique_ptr<SafetyHubService::Result>()>
UnusedSitePermissionsService::GetBackgroundTask() {
  return base::BindOnce(&UnusedSitePermissionsService::UpdateOnBackgroundThread,
                        clock_, base::WrapRefCounted(hcsm()));
}

std::unique_ptr<SafetyHubService::Result>
UnusedSitePermissionsService::UpdateOnBackgroundThread(
    base::Clock* clock,
    const scoped_refptr<HostContentSettingsMap> hcsm) {
  UnusedSitePermissionsService::UnusedPermissionMap recently_unused;
  const base::Time threshold =
      clock->Now() - content_settings::GetCoarseVisitedTimePrecision();
  auto* website_setting_registry =
      content_settings::WebsiteSettingsRegistry::GetInstance();
  for (const content_settings::WebsiteSettingsInfo* info :
       *website_setting_registry) {
    ContentSettingsType type = info->type();
    if (!IsContentSetting(type) && IsWebsiteSetting(type) &&
        !IsChooserPermissionSupported()) {
      continue;
    }
    if (!content_settings::CanTrackLastVisit(type)) {
      continue;
    }
    ContentSettingsForOneType settings = hcsm->GetSettingsForOneType(type);
    for (const auto& setting : settings) {
      // Skip wildcard patterns that don't belong to a single origin. These
      // shouldn't track visit timestamps.
      if (!setting.primary_pattern.MatchesSingleOrigin()) {
        continue;
      }
      if (setting.metadata.last_visited() != base::Time() &&
          setting.metadata.last_visited() < threshold) {
        // Converting a primary pattern to an origin is normally an anti-pattern
        // but here it is ok since the primary pattern belongs to a single
        // origin. Therefore, it has a fully defined URL+scheme+port which makes
        // converting primary pattern to origin successful.
        url::Origin origin =
            ConvertPrimaryPatternToOrigin(setting.primary_pattern);
        recently_unused[origin.Serialize()].push_back(
            {type, std::move(setting)});
      }
    }
  }

  auto result = std::make_unique<
      UnusedSitePermissionsService::UnusedSitePermissionsResult>();
  result->SetRecentlyUnusedPermissions(recently_unused);
  return std::move(result);
}

std::unique_ptr<SafetyHubService::Result>
UnusedSitePermissionsService::UpdateOnUIThread(
    std::unique_ptr<SafetyHubService::Result> result) {
  auto* interim_result =
      static_cast<UnusedSitePermissionsService::UnusedSitePermissionsResult*>(
          result.get());
  recently_unused_permissions_ = interim_result->GetRecentlyUnusedPermissions();
  RevokeUnusedPermissions();
  // TODO(crbug.com/40250875): Clean up these checks.
  if (IsAbusiveNotificationAutoRevocationEnabled()) {
    abusive_notification_manager_->CheckNotificationPermissionOrigins();
  }
  return GetRevokedPermissions();
}

std::unique_ptr<UnusedSitePermissionsService::UnusedSitePermissionsResult>
UnusedSitePermissionsService::GetRevokedPermissions() {
  ContentSettingsForOneType settings = hcsm()->GetSettingsForOneType(
      ContentSettingsType::REVOKED_UNUSED_SITE_PERMISSIONS);
  auto result = std::make_unique<
      UnusedSitePermissionsService::UnusedSitePermissionsResult>();

  for (const auto& revoked_permissions : settings) {
    PermissionsData permissions_data;
    permissions_data.primary_pattern = revoked_permissions.primary_pattern;
    const base::Value& stored_value = revoked_permissions.setting_value;
    DCHECK(stored_value.is_dict());

    const base::Value::List* type_list =
        stored_value.GetDict().FindList(permissions::kRevokedKey);
    CHECK(type_list);
    for (base::Value& type_value : type_list->Clone()) {
      // To avoid crashes for unknown types skip integer values.
      if (type_value.is_int()) {
        continue;
      }

      ContentSettingsType type =
          ConvertKeyToContentSettingsType(type_value.GetString());
      permissions_data.permission_types.insert(type);
    }

    permissions_data.constraints = content_settings::ContentSettingConstraints(
        revoked_permissions.metadata.expiration() -
        revoked_permissions.metadata.lifetime());
    permissions_data.constraints.set_lifetime(
        revoked_permissions.metadata.lifetime());

    auto* chooser_permissions_data_dict = stored_value.GetDict().FindDict(
        permissions::kRevokedChooserPermissionsKey);
    if (chooser_permissions_data_dict) {
      permissions_data.chooser_permissions_data =
          chooser_permissions_data_dict->Clone();
    }

    // If the origin has a revoked abusive notification, add `NOTIFICATIONS` to
    // the list of revoked permissions.
    if (safety_hub_util::IsUrlRevokedAbusiveNotification(
            hcsm(), GURL(revoked_permissions.primary_pattern.ToString()))) {
      DCHECK(IsAbusiveNotificationAutoRevocationEnabled());
      permissions_data.permission_types.insert(
          static_cast<ContentSettingsType>(ContentSettingsType::NOTIFICATIONS));

      // Add a new constraint for abusive notification revocations to expire.
      permissions_data.abusive_revocation_constraints =
          content_settings::ContentSettingConstraints(
              revoked_permissions.metadata.expiration() -
              revoked_permissions.metadata.lifetime());
      permissions_data.abusive_revocation_constraints.set_lifetime(
          revoked_permissions.metadata.lifetime());
    }

    result->AddRevokedPermission(permissions_data);
  }

  ContentSettingsForOneType revoked_abusive_notification_settings =
      safety_hub_util::GetRevokedAbusiveNotificationPermissions(hcsm());
  for (const auto& revoked_abusive_notification_permission :
       revoked_abusive_notification_settings) {
    // Skip origins with revoked unused site permissions, since these were
    // handled above.
    if (safety_hub_util::IsUrlRevokedUnusedSite(
            hcsm(), GURL(revoked_abusive_notification_permission.primary_pattern
                             .ToString()))) {
      continue;
    }
    PermissionsData permissions_data;
    permissions_data.primary_pattern =
        revoked_abusive_notification_permission.primary_pattern;
    permissions_data.permission_types.insert(
        static_cast<ContentSettingsType>(ContentSettingsType::NOTIFICATIONS));

    // Add `abusive_revocation_constraints`.
    permissions_data.abusive_revocation_constraints =
        content_settings::ContentSettingConstraints(
            revoked_abusive_notification_permission.metadata.expiration() -
            revoked_abusive_notification_permission.metadata.lifetime());
    permissions_data.abusive_revocation_constraints.set_lifetime(
        revoked_abusive_notification_permission.metadata.lifetime());

    result->AddRevokedPermission(permissions_data);
  }

  return result;
}

void UnusedSitePermissionsService::RevokeUnusedPermissions() {
  if (!IsUnusedSiteAutoRevocationEnabled()) {
    return;
  }

  // Set this to true to prevent `OnContentSettingChanged` from removing
  // revoked setting values during auto-revocation.
  is_unused_site_revocation_running = true;

  base::Time threshold = clock_->Now() - GetRevocationThreshold();

  for (auto itr = recently_unused_permissions_.begin();
       itr != recently_unused_permissions_.end();) {
    std::list<ContentSettingEntry>& unused_site_permissions = itr->second;

    // All |primary_pattern|s are equal across list items, the same is true for
    // |secondary_pattern|s. This property is needed later and checked in the
    // loop.
    ContentSettingsPattern primary_pattern =
        unused_site_permissions.front().source.primary_pattern;
    ContentSettingsPattern secondary_pattern =
        unused_site_permissions.front().source.secondary_pattern;

    std::set<ContentSettingsType> revoked_permissions;
    base::Value::Dict chooser_permissions_data;
    for (auto permission_itr = unused_site_permissions.begin();
         permission_itr != unused_site_permissions.end();) {
      const ContentSettingEntry& entry = *permission_itr;
      // Check if the current permission can be auto revoked.
      if (!content_settings::CanBeAutoRevoked(entry.type,
                                              entry.source.setting_value)) {
        permission_itr++;
        continue;
      }

      DCHECK_EQ(entry.source.primary_pattern, primary_pattern);
      DCHECK(entry.source.secondary_pattern ==
                 ContentSettingsPattern::Wildcard() ||
             entry.source.secondary_pattern == entry.source.primary_pattern);

      // Reset the permission to default if the site is visited before
      // threshold. Also, the secondary pattern should be wildcard.
      DCHECK_NE(entry.source.metadata.last_visited(), base::Time());
      DCHECK(entry.type != ContentSettingsType::NOTIFICATIONS);
      if (entry.source.metadata.last_visited() < threshold &&
          entry.source.secondary_pattern ==
              ContentSettingsPattern::Wildcard()) {
        permissions::PermissionUmaUtil::ScopedRevocationReporter reporter(
            browser_context_.get(), entry.source.primary_pattern,
            entry.source.secondary_pattern, entry.type,
            permissions::PermissionSourceUI::SAFETY_HUB_AUTO_REVOCATION);
        // Record the number of permissions auto-revoked per permission type.
        content_settings_uma_util::RecordContentSettingsHistogram(
            "Settings.SafetyHub.UnusedSitePermissionsModule.AutoRevoked2",
            entry.type);
        revoked_permissions.insert(entry.type);
        if (IsContentSetting(entry.type)) {
          hcsm()->SetContentSettingCustomScope(
              entry.source.primary_pattern, entry.source.secondary_pattern,
              entry.type, ContentSetting::CONTENT_SETTING_DEFAULT);
        } else if (IsChooserPermissionSupported() &&
                   IsWebsiteSetting(entry.type)) {
          chooser_permissions_data.Set(
              ConvertContentSettingsTypeToKey(entry.type),
              entry.source.setting_value.Clone());
          hcsm()->SetWebsiteSettingCustomScope(entry.source.primary_pattern,
                                               entry.source.secondary_pattern,
                                               entry.type, base::Value());
        } else {
          NOTREACHED_IN_MIGRATION()
              << "Unable to find ContentSettingsType in neither "
              << "ContentSettingsRegistry nor WebsiteSettingsRegistry: "
              << ConvertContentSettingsTypeToKey(entry.type);
        }
        unused_site_permissions.erase(permission_itr++);
      } else {
        permission_itr++;
      }
    }

    // Store revoked permissions on HCSM.
    if (!revoked_permissions.empty()) {
      StorePermissionInRevokedPermissionSetting(
          revoked_permissions, chooser_permissions_data, std::nullopt,
          primary_pattern, secondary_pattern);
    }

    // Handle clean up of recently_unused_permissions_ map after revocation.
    if (unused_site_permissions.empty()) {
      // Since all unused permissions are revoked, the map should be cleared.
      recently_unused_permissions_.erase(itr++);
    } else {
      // Since there are some permissions that are not revoked, the tracked
      // unused permissions should be set to those permissions.
      // Note that, currently all permissions belong to a single domain will
      // revoked all together, since triggering permission prompt requires a
      // page visit. So the timestamp of all granted permissions of the origin
      // will be updated. However, this logic will prevent edge cases like
      // permission prompt stays open long time, also will provide support for
      // revoking permissions separately in the future.
      itr++;
    }
  }
  // Set this back to false, so that `OnContentSettingChanged` can cleanup
  // revoked settings if necessary.
  is_unused_site_revocation_running = false;
}

void UnusedSitePermissionsService::StorePermissionInRevokedPermissionSetting(
    const PermissionsData& permissions_data) {
  // The |secondary_pattern| for
  // |ContentSettingsType::REVOKED_UNUSED_SITE_PERMISSIONS| is always wildcard.
  StorePermissionInRevokedPermissionSetting(
      permissions_data.permission_types,
      permissions_data.chooser_permissions_data, permissions_data.constraints,
      permissions_data.primary_pattern, ContentSettingsPattern::Wildcard());
}

void UnusedSitePermissionsService::StorePermissionInRevokedPermissionSetting(
    const std::set<ContentSettingsType>& permissions,
    const base::Value::Dict& chooser_permissions_data,
    const std::optional<content_settings::ContentSettingConstraints> constraint,
    const ContentSettingsPattern& primary_pattern,
    const ContentSettingsPattern& secondary_pattern) {
  // This method only pertains to permissions other than `NOTIFICATIONS`, since
  // these permissions are not revoked for unused sites.
  const std::set<ContentSettingsType>& unused_site_permission_types =
      GetRevokedUnusedSitePermissionTypes(permissions);
  GURL url = GURL(primary_pattern.ToString());
  // The url should be valid as it is checked that the pattern represents a
  // single origin.
  DCHECK(url.is_valid());
  // Get the current value of the setting to append the recently revoked
  // permissions.
  base::Value cur_value(hcsm()->GetWebsiteSetting(
      url, url, ContentSettingsType::REVOKED_UNUSED_SITE_PERMISSIONS));

  base::Value::Dict dict = cur_value.is_dict() ? std::move(cur_value.GetDict())
                                               : base::Value::Dict();
  base::Value::List permission_type_list =
      dict.FindList(permissions::kRevokedKey)
          ? std::move(*dict.FindList(permissions::kRevokedKey))
          : base::Value::List();

  for (const auto& permission : unused_site_permission_types) {
    // Chooser permissions (not ContentSettingsRegistry-based) should have
    // corresponding data to be restored in `chooser_permissions_data`.
    DCHECK(IsContentSetting(permission) || !IsChooserPermissionSupported() ||
           chooser_permissions_data.contains(
               ConvertContentSettingsTypeToKey(permission)));
    permission_type_list.Append(ConvertContentSettingsTypeToKey(permission));
  }

  dict.Set(permissions::kRevokedKey,
           base::Value::List(std::move(permission_type_list)));

  if (IsChooserPermissionSupported() && !chooser_permissions_data.empty()) {
    base::Value::Dict existing_chooser_permissions_data =
        dict.FindDict(permissions::kRevokedChooserPermissionsKey)
            ? std::move(
                  *dict.FindDict(permissions::kRevokedChooserPermissionsKey))
            : base::Value::Dict();
    for (auto data : chooser_permissions_data) {
      // Chooser permissions data should have its permission type included in
      // `permissions` set.
      DCHECK(permissions.contains(ConvertKeyToContentSettingsType(data.first)));
      existing_chooser_permissions_data.Set(data.first, data.second.Clone());
    }
    dict.Set(permissions::kRevokedChooserPermissionsKey,
             base::Value::Dict(std::move(existing_chooser_permissions_data)));
  }

  content_settings::ContentSettingConstraints default_constraint(clock_->Now());
  default_constraint.set_lifetime(safety_hub_util::GetCleanUpThreshold());

  // Set website setting for the list of recently revoked permissions and
  // previously revoked permissions, if exists.
  hcsm()->SetWebsiteSettingCustomScope(
      primary_pattern, secondary_pattern,
      ContentSettingsType::REVOKED_UNUSED_SITE_PERMISSIONS,
      base::Value(std::move(dict)),
      constraint.has_value() ? constraint.value() : default_constraint);
}

void UnusedSitePermissionsService::OnPermissionsAutorevocationControlChanged() {
  // TODO(crbug.com/40250875): Clean up these checks.
  if (IsUnusedSiteAutoRevocationEnabled() ||
      IsAbusiveNotificationAutoRevocationEnabled()) {
    StartRepeatedUpdates();
  } else {
    StopTimer();
  }
}

std::vector<UnusedSitePermissionsService::ContentSettingEntry>
UnusedSitePermissionsService::GetTrackedUnusedPermissionsForTesting() {
  std::vector<ContentSettingEntry> result;
  for (const auto& list : recently_unused_permissions_) {
    for (const auto& entry : list.second) {
      result.push_back(entry);
    }
  }
  return result;
}

void UnusedSitePermissionsService::SetClockForTesting(base::Clock* clock) {
  clock_ = clock;
  if (IsAbusiveNotificationAutoRevocationEnabled()) {
    abusive_notification_manager_->SetClockForTesting(clock);  // IN-TEST
  }
}

base::WeakPtr<SafetyHubService> UnusedSitePermissionsService::GetAsWeakRef() {
  return weak_factory_.GetWeakPtr();
}

bool UnusedSitePermissionsService::IsUnusedSiteAutoRevocationEnabled() {
  // If kSafetyHub is disabled, then the auto-revocation directly depends on
  // kSafetyCheckUnusedSitePermissions.
  if (!base::FeatureList::IsEnabled(features::kSafetyHub)) {
    return base::FeatureList::IsEnabled(
        content_settings::features::kSafetyCheckUnusedSitePermissions);
  }
  return pref_change_registrar_->prefs()->GetBoolean(
      safety_hub_prefs::kUnusedSitePermissionsRevocationEnabled);
}

bool UnusedSitePermissionsService::
    IsAbusiveNotificationAutoRevocationEnabled() {
  return base::FeatureList::IsEnabled(
             safe_browsing::kSafetyHubAbusiveNotificationRevocation) &&
         safe_browsing::IsSafeBrowsingEnabled(*pref_change_registrar_->prefs());
}

const std::set<ContentSettingsType>
UnusedSitePermissionsService::GetRevokedUnusedSitePermissionTypes(
    const std::set<ContentSettingsType> permissions) {
  std::set<ContentSettingsType>
      permissions_without_revoked_abusive_notification_manager = permissions;
  if (permissions_without_revoked_abusive_notification_manager.contains(
          ContentSettingsType::NOTIFICATIONS)) {
    permissions_without_revoked_abusive_notification_manager.erase(
        ContentSettingsType::NOTIFICATIONS);
  }
  return permissions_without_revoked_abusive_notification_manager;
}

void UnusedSitePermissionsService::UpdateIntegerValuesToGroupName() {
  ContentSettingsForOneType settings = hcsm()->GetSettingsForOneType(
      ContentSettingsType::REVOKED_UNUSED_SITE_PERMISSIONS);

  bool successful_migration = true;
  for (const auto& revoked_permissions : settings) {
    const base::Value& stored_value = revoked_permissions.setting_value;
    DCHECK(stored_value.is_dict());
    base::Value updated_dict(stored_value.Clone());

    const base::Value::List* permission_value_list =
        stored_value.GetDict().FindList(permissions::kRevokedKey);
    if (permission_value_list) {
      base::Value::List updated_permission_value_list =
          ConvertContentSettingsIntValuesToString(
              permission_value_list->Clone(), &successful_migration);
      updated_dict.GetDict().Set(permissions::kRevokedKey,
                                 std::move(updated_permission_value_list));
    }

    const base::Value::Dict* chooser_permission_value_dict =
        stored_value.GetDict().FindDict(
            permissions::kRevokedChooserPermissionsKey);
    if (chooser_permission_value_dict) {
      base::Value::Dict updated_chooser_permission_value_dict =
          ConvertChooserContentSettingsIntValuesToString(
              chooser_permission_value_dict->Clone());
      updated_dict.GetDict().Set(
          permissions::kRevokedChooserPermissionsKey,
          std::move(updated_chooser_permission_value_dict));
    }

    // Create a new constraint with the old creation time of the original
    // exception.
    base::Time creation_time = revoked_permissions.metadata.expiration() -
                               revoked_permissions.metadata.lifetime();
    content_settings::ContentSettingConstraints constraints(creation_time);
    constraints.set_lifetime(revoked_permissions.metadata.lifetime());

    hcsm()->SetWebsiteSettingCustomScope(
        revoked_permissions.primary_pattern,
        revoked_permissions.secondary_pattern,
        ContentSettingsType::REVOKED_UNUSED_SITE_PERMISSIONS,
        std::move(updated_dict), constraints);
  }

  if (successful_migration) {
    pref_change_registrar_->prefs()->SetBoolean(
        safety_hub_prefs::kUnusedSitePermissionsRevocationMigrationCompleted,
        true);
  }
}
