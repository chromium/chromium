// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/safety_hub/unused_site_permissions_manager.h"

#include <memory>
#include <optional>
#include <set>
#include <utility>
#include <vector>

#include "base/check.h"
#include "base/check_op.h"
#include "base/feature_list.h"
#include "base/logging.h"
#include "base/metrics/histogram_functions.h"
#include "base/strings/string_number_conversions.h"
#include "base/time/clock.h"
#include "base/time/default_clock.h"
#include "base/time/time.h"
#include "base/values.h"
#include "chrome/browser/ui/safety_hub/revoked_permissions_result.h"
#include "chrome/browser/ui/safety_hub/safety_hub_prefs.h"
#include "chrome/browser/ui/safety_hub/safety_hub_result.h"
#include "chrome/browser/ui/safety_hub/safety_hub_util.h"
#include "components/content_settings/core/browser/content_settings_info.h"
#include "components/content_settings/core/browser/content_settings_registry.h"
#include "components/content_settings/core/browser/content_settings_uma_util.h"
#include "components/content_settings/core/browser/content_settings_utils.h"
#include "components/content_settings/core/browser/website_settings_registry.h"
#include "components/content_settings/core/common/content_settings.h"
#include "components/content_settings/core/common/content_settings_constraints.h"
#include "components/content_settings/core/common/content_settings_pattern.h"
#include "components/content_settings/core/common/content_settings_types.h"
#include "components/content_settings/core/common/features.h"
#include "components/permissions/constants.h"
#include "components/permissions/permission_uma_util.h"
#include "components/prefs/pref_change_registrar.h"
#include "components/prefs/pref_service.h"
#include "components/safety_check/safety_check.h"
#include "content/public/browser/browser_context.h"
#include "content/public/browser/browser_thread.h"
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
constexpr size_t kAllowAgainMetricsExclusiveMaxCount = 31;

// Using a single bucket per day, following the value of
// `kAllowAgainMetricsExclusiveMaxCount`.
constexpr size_t kAllowAgainMetricsBuckets = 31;

// Determines the time interval after which sites are considered to be unused
// and their permissions will be revoked.
const base::TimeDelta kUnusedSitePermissionsRevocationThreshold =
    base::Days(60);

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
  return kUnusedSitePermissionsRevocationThreshold;
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

const std::set<ContentSettingsType> GetRevokedUnusedSitePermissionTypes(
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

base::Value::List ConvertContentSettingsIntValuesToString(
    const base::Value::List& content_settings_values_list,
    bool* successful_migration) {
  base::Value::List string_value_list;
  for (const base::Value& setting_value : content_settings_values_list) {
    if (setting_value.is_int()) {
      int setting_int = setting_value.GetInt();
      auto setting_name =
          UnusedSitePermissionsManager::ConvertContentSettingsTypeToKey(
              static_cast<ContentSettingsType>(setting_int));
      if (setting_name == kUnknownContentSettingsType) {
        *successful_migration = false;
        string_value_list.Append(setting_value.GetInt());
      } else {
        string_value_list.Append(setting_name);
      }
    } else {
      CHECK(setting_value.is_string());
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
          UnusedSitePermissionsManager::ConvertContentSettingsTypeToKey(
              static_cast<ContentSettingsType>(number)),
          value.GetDict().Clone());
    }
  }
  return string_keyed_dict;
}

}  // namespace

// static
std::unique_ptr<SafetyHubResult>
UnusedSitePermissionsManager::UpdateOnBackgroundThread(
    base::Clock* clock,
    const scoped_refptr<HostContentSettingsMap> hcsm) {
  UnusedSitePermissionsManager::UnusedPermissionMap recently_unused;
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

  auto result = std::make_unique<RevokedPermissionsResult>();
  result->SetRecentlyUnusedPermissions(recently_unused);
  return std::move(result);
}

// static
std::string UnusedSitePermissionsManager::ConvertContentSettingsTypeToKey(
    ContentSettingsType type) {
  auto* website_setting_registry =
      content_settings::WebsiteSettingsRegistry::GetInstance();
  CHECK(website_setting_registry);

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
UnusedSitePermissionsManager::ConvertKeyToContentSettingsType(
    const std::string& key) {
  auto* website_setting_registry =
      content_settings::WebsiteSettingsRegistry::GetInstance();
  return website_setting_registry->GetByName(key)->type();
}

// static
url::Origin UnusedSitePermissionsManager::ConvertPrimaryPatternToOrigin(
    const ContentSettingsPattern& primary_pattern) {
  GURL origin_url = GURL(primary_pattern.ToString());
  CHECK(origin_url.is_valid());

  return url::Origin::Create(origin_url);
}

UnusedSitePermissionsManager::UnusedSitePermissionsManager(
    content::BrowserContext* browser_context,
    PrefService* prefs)
    : browser_context_(browser_context),
      clock_(base::DefaultClock::GetInstance()) {
  CHECK_CURRENTLY_ON(content::BrowserThread::UI);
  CHECK(browser_context_);

  pref_change_registrar_ = std::make_unique<PrefChangeRegistrar>();
  pref_change_registrar_->Init(prefs);

  bool migration_completed = pref_change_registrar_->prefs()->GetBoolean(
      safety_hub_prefs::kUnusedSitePermissionsRevocationMigrationCompleted);
  if (!migration_completed) {
    // Convert all integer permission values to string, if there is any
    // permission represented by integer stored in disk.
    // TODO(crbug.com/415227458): Clean up this migration after some milestones.
    UpdateIntegerValuesToGroupName();
  }
}

UnusedSitePermissionsManager::~UnusedSitePermissionsManager() = default;

void UnusedSitePermissionsManager::RevokeUnusedPermissions(
    std::unique_ptr<SafetyHubResult> result) {
  // Set this to true to prevent
  // `UnusedSitePermissionsManager::OnContentSettingChanged` from removing
  // revoked setting values during auto-revocation.
  is_unused_site_revocation_running_ = true;

  auto* interim_result = static_cast<RevokedPermissionsResult*>(result.get());
  recently_unused_permissions_ = interim_result->GetRecentlyUnusedPermissions();

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
      if (!content_settings::CanBeAutoRevokedAsUnusedPermission(
              /*type=*/entry.type, /*value=*/entry.source.setting_value)) {
        permission_itr++;
        continue;
      }

      CHECK_EQ(entry.source.primary_pattern, primary_pattern);
      CHECK(entry.source.secondary_pattern ==
                ContentSettingsPattern::Wildcard() ||
            entry.source.secondary_pattern == entry.source.primary_pattern);

      // Reset the permission to default if the site is visited before
      // threshold. Also, the secondary pattern should be wildcard.
      CHECK_NE(entry.source.metadata.last_visited(), base::Time());
      CHECK(entry.type != ContentSettingsType::NOTIFICATIONS);
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
          NOTREACHED()
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
      StorePermissionInUnusedSitePermissionSetting(
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
  is_unused_site_revocation_running_ = false;
}

bool UnusedSitePermissionsManager::IsRevocationRunning() {
  return is_unused_site_revocation_running_;
}

// Called by TabHelper when a URL was visited.
void UnusedSitePermissionsManager::OnPageVisited(const url::Origin& origin) {
  CHECK_CURRENTLY_ON(content::BrowserThread::UI);
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

void UnusedSitePermissionsManager::RegrantPermissionsForOrigin(
    const url::Origin& origin) {
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
  is_unused_site_revocation_running_ = true;

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
      CHECK(chooser_permissions_data);
      // Chooser permissions are WebsiteSettingsRegistry-based, so it is
      // re-granted by restoring the previously revoked Website Setting value.
      auto* revoked_value = chooser_permissions_data->FindDict(
          ConvertContentSettingsTypeToKey(type));
      CHECK(revoked_value);
      hcsm()->SetWebsiteSettingCustomScope(
          info.primary_pattern, info.secondary_pattern, type,
          base::Value(std::move(*revoked_value)));
    } else {
      NOTREACHED() << "Unable to find ContentSettingsType in neither "
                   << "ContentSettingsRegistry nor WebsiteSettingsRegistry: "
                   << ConvertContentSettingsTypeToKey(type);
    }
  }

  // Set this back to false, so that `OnContentSettingChanged` can cleanup
  // revoked settings if necessary.
  is_unused_site_revocation_running_ = false;

  // Ignore origin from future auto-revocations.
  IgnoreOriginForAutoRevocation(origin);

  // Remove origin from revoked permissions list.
  DeletePatternFromRevokedUnusedSitePermissionList(info.primary_pattern,
                                                   info.secondary_pattern);

  // Record the days elapsed from auto-revocation to regrant.
  base::Time revoked_time =
      info.metadata.expiration() -
      safety_check::GetUnusedSitePermissionsRevocationCleanUpThreshold();
  base::UmaHistogramCustomCounts(
      "Settings.SafetyCheck.UnusedSitePermissionsAllowAgainDays",
      (clock_->Now() - revoked_time).InDays(), 0,
      kAllowAgainMetricsExclusiveMaxCount, kAllowAgainMetricsBuckets);
}

void UnusedSitePermissionsManager::UndoRegrantPermissionsForOrigin(
    const PermissionsData& permissions_data) {
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
  is_unused_site_revocation_running_ = true;
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
      NOTREACHED() << "Unable to find ContentSettingsType in neither "
                   << "ContentSettingsRegistry nor WebsiteSettingsRegistry: "
                   << ConvertContentSettingsTypeToKey(permission);
    }
  }
  // Set this back to false, so that `OnContentSettingChanged` can cleanup
  // revoked settings if necessary.
  is_unused_site_revocation_running_ = false;

  StorePermissionInUnusedSitePermissionSetting(
      unused_site_permission_types, permissions_data.chooser_permissions_data,
      permissions_data.constraints.Clone(), permissions_data.primary_pattern,
      ContentSettingsPattern::Wildcard());
}

void UnusedSitePermissionsManager::
    DeletePatternFromRevokedUnusedSitePermissionList(
        const ContentSettingsPattern& primary_pattern,
        const ContentSettingsPattern& secondary_pattern) {
  hcsm()->SetWebsiteSettingCustomScope(
      primary_pattern, secondary_pattern,
      ContentSettingsType::REVOKED_UNUSED_SITE_PERMISSIONS, {});
}

void UnusedSitePermissionsManager::StorePermissionInUnusedSitePermissionSetting(
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
  CHECK(url.is_valid());
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
    CHECK(IsContentSetting(permission) || !IsChooserPermissionSupported() ||
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
      CHECK(permissions.contains(ConvertKeyToContentSettingsType(data.first)));
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

void UnusedSitePermissionsManager::SetClockForTesting(base::Clock* clock) {
  clock_ = clock;
}

std::vector<ContentSettingEntry>
UnusedSitePermissionsManager::GetTrackedUnusedPermissionsForTesting() {
  std::vector<ContentSettingEntry> result;
  for (const auto& list : recently_unused_permissions_) {
    for (const auto& entry : list.second) {
      result.push_back(entry);
    }
  }
  return result;
}

void UnusedSitePermissionsManager::IgnoreOriginForAutoRevocation(
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

void UnusedSitePermissionsManager::UpdateIntegerValuesToGroupName() {
  ContentSettingsForOneType settings = hcsm()->GetSettingsForOneType(
      ContentSettingsType::REVOKED_UNUSED_SITE_PERMISSIONS);

  bool successful_migration = true;
  for (const auto& revoked_permissions : settings) {
    const base::Value& stored_value = revoked_permissions.setting_value;
    CHECK(stored_value.is_dict());
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
