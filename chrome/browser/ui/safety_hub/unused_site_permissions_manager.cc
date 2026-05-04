// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/safety_hub/unused_site_permissions_manager.h"

#include <memory>
#include <optional>
#include <utility>
#include <vector>

#include "base/auto_reset.h"
#include "base/check.h"
#include "base/check_op.h"
#include "base/containers/flat_map.h"
#include "base/feature_list.h"
#include "base/logging.h"
#include "base/metrics/histogram_functions.h"
#include "base/metrics/histogram_macros.h"
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
#include "components/content_settings/core/browser/permission_settings_info.h"
#include "components/content_settings/core/browser/permission_settings_registry.h"
#include "components/content_settings/core/browser/website_settings_registry.h"
#include "components/content_settings/core/common/content_settings.h"
#include "components/content_settings/core/common/content_settings_constraints.h"
#include "components/content_settings/core/common/content_settings_pattern.h"
#include "components/content_settings/core/common/content_settings_types.h"
#include "components/content_settings/core/common/content_settings_utils.h"
#include "components/content_settings/core/common/features.h"
#include "components/permissions/constants.h"
#include "components/permissions/features.h"
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
namespace permissions_features = ::permissions;

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

bool IsPermissionSetting(ContentSettingsType type) {
  auto* permission_setting_registry =
      content_settings::PermissionSettingsRegistry::GetInstance();
  return permission_setting_registry->Get(type);
}

base::ListValue ConvertContentSettingsIntValuesToString(
    base::ListValue content_settings_values_list,
    bool* successful_migration) {
  base::ListValue string_value_list;
  for (base::Value& setting_value : content_settings_values_list) {
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
      string_value_list.Append(std::move(setting_value));
    }
  }
  return string_value_list;
}

}  // namespace

// static
std::unique_ptr<SafetyHubResult>
UnusedSitePermissionsManager::UpdateOnBackgroundThread(
    base::Clock* clock,
    const scoped_refptr<HostContentSettingsMap> hcsm,
    bool revocation_backfill_completed) {
  auto result = std::make_unique<RevokedPermissionsResult>();

  const bool revocation_backfill_enabled = base::FeatureList::IsEnabled(
      permissions::features::
          kSafetyHubUnusedPermissionRevocationForAllSurfaces);
  // Pass the flag to UI thread to maintain consistency throughout the session.
  result->SetRevocationBackfillEnabled(revocation_backfill_enabled);
  if (revocation_backfill_enabled) {
    // Record whether the backfill was already completed for the user or not.
    UMA_HISTOGRAM_BOOLEAN(
        "Settings.SafetyHub.UnusedSitePermissionsModule.Backfill."
        "CompletionStatus",
        revocation_backfill_completed);

    if (!revocation_backfill_completed) {
      // Record the attempt to run the backfill code.
      UMA_HISTOGRAM_BOOLEAN(
          "Settings.SafetyHub.UnusedSitePermissionsModule.Backfill.RunStatus",
          false /*STARTED*/);
    }
  }

  UnusedSitePermissionsManager::UnusedPermissionMap recently_unused;
  UnusedSitePermissionsManager::UntimestampedPermissionList
      untimestamped_permissions;

  const base::Time threshold =
      clock->Now() - content_settings::GetCoarseVisitedTimePrecision();
  auto* website_setting_registry =
      content_settings::WebsiteSettingsRegistry::GetInstance();
  for (const content_settings::WebsiteSettingsInfo* info :
       *website_setting_registry) {
    ContentSettingsType type = info->type();
    if (!IsPermissionSetting(type)) {
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
      // Skip permissions that are explicitly excluded from autorevocation by
      // user.
      if (setting.metadata.autorevocation_bypassed_by_user()) {
        continue;
      }
      if (setting.metadata.last_visited() != base::Time()) {
        if (setting.metadata.last_visited() < threshold) {
          // Converting a primary pattern to an origin is normally an
          // anti-pattern but here it is ok since the primary pattern belongs to
          // a single origin. Therefore, it has a fully defined URL+scheme+port
          // which makes converting primary pattern to origin successful.
          url::Origin origin =
              ConvertPrimaryPatternToOrigin(setting.primary_pattern);
          recently_unused[origin.Serialize()].push_back(
              {type, std::move(setting)});
        }
        // TODO(crbug.com/40267370): Clean-up after the backfill is done.
      } else {
        // Track untimestamped permissions if the backfill was not completed
        // yet.
        if (revocation_backfill_enabled && !revocation_backfill_completed) {
          untimestamped_permissions.push_back(
              {type, setting.primary_pattern, setting.secondary_pattern});
        }
      }
    }
  }

  if (revocation_backfill_enabled && !revocation_backfill_completed &&
      !untimestamped_permissions.empty()) {
    result->SetUntimestampedPermissions(untimestamped_permissions);
  }

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
  auto* interim_result = static_cast<RevokedPermissionsResult*>(result.get());

  // TODO(crbug.com/40267370): Clean-up after the backfill is done.
  MaybePerformLastVisitedBackfill(interim_result);

  // Set this to true to prevent
  // `UnusedSitePermissionsManager::OnContentSettingChanged` from removing
  // revoked setting values during auto-revocation.
  base::AutoReset<bool> is_unused_site_revocation_running(
      &is_unused_site_revocation_running_, true);

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

    base::flat_map<ContentSettingsType, base::Value> revoked_permissions;
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
        revoked_permissions.insert(
            std::make_pair(entry.type, entry.source.setting_value.Clone()));
        CHECK(IsPermissionSetting(entry.type));
        hcsm()->SetPermissionSettingCustomScope(entry.source.primary_pattern,
                                                entry.source.secondary_pattern,
                                                entry.type, std::nullopt);
        unused_site_permissions.erase(permission_itr++);
      } else {
        permission_itr++;
      }
    }

    // Store revoked permissions on HCSM.
    StorePermissionInUnusedSitePermissionSetting(std::move(revoked_permissions),
                                                 std::nullopt, primary_pattern,
                                                 secondary_pattern);

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
      // This should only be updated for settings that are already tracked.
      if (it->source.metadata.last_visited() != base::Time()) {
        hcsm()->UpdateLastVisitedTime(it->source.primary_pattern,
                                      it->source.secondary_pattern, it->type);
        site_permissions.erase(it++);
      }
    } else {
      it++;
    }
  }
  // Remove origin entry if all permissions were updated.
  if (site_permissions.empty()) {
    recently_unused_permissions_.erase(origin_entry);
  }
}

base::flat_map<ContentSettingsType, base::Value>
UnusedSitePermissionsManager::ExtractRevokedPermissions(
    base::Value stored_value) {
  base::flat_map<ContentSettingsType, base::Value> permissions;
  if (!stored_value.is_dict()) {
    return permissions;
  }
  base::ListValue* revoked_list =
      stored_value.GetDict().EnsureList(permissions::kRevokedKey);

  // Revoked unused permissions are stored as element in a list. Simple content
  // settings are just encoded as strings (using
  // ConvertContentSettingTypeToKey()). PermissionSettings which are not
  // ContentSettings are stored as a dict {kRevokerPermissionType:
  // ConvertContentSettingTypeToKey(), kRevokerPermissionSettingValue:
  // revoked_value} containing also the original revoked_value, so that they can
  // be regranted (while for simple content settings the original revoked value
  // must just be CONTENT_SETTING_ALLOW.
  for (base::Value& revoked_value : *revoked_list) {
    const std::string* type_string = nullptr;
    base::Value allow_value = base::Value(CONTENT_SETTING_ALLOW);
    base::Value* value;
    if (revoked_value.is_string()) {
      type_string = &revoked_value.GetString();
      value = &allow_value;
    } else if (revoked_value.is_dict()) {
      type_string = revoked_value.GetDict().FindString(
          permissions::kRevokedPermissionType);
      value = revoked_value.GetDict().Find(
          permissions::kRevokedPermissionSettingValue);
    } else {
      continue;
    }
    if (!type_string || !value) {
      continue;
    }
    if (!content_settings::WebsiteSettingsRegistry::GetInstance()->GetByName(
            *type_string)) {
      continue;
    }
    ContentSettingsType type =
        UnusedSitePermissionsManager::ConvertKeyToContentSettingsType(
            *type_string);
    permissions.insert(std::make_pair(type, std::move(*value)));
  }
  return permissions;
}

void UnusedSitePermissionsManager::RegrantPermissionsForOrigin(
    const url::Origin& origin) {
  content_settings::SettingInfo info;
  base::Value stored_value(hcsm()->GetWebsiteSetting(
      origin.GetURL(), origin.GetURL(),
      ContentSettingsType::REVOKED_UNUSED_SITE_PERMISSIONS, &info));

  base::flat_map<ContentSettingsType, base::Value> revoked_permissions =
      ExtractRevokedPermissions(std::move(stored_value));

  // Set this to true to prevent `OnContentSettingChanged` from removing
  // revoked setting values, since this is set with specific values below.
  base::AutoReset<bool> is_unused_site_revocation_running(
      &is_unused_site_revocation_running_, true);

  for (auto&& it : revoked_permissions) {
    // Seamlessly handle the transition GEOLOCATION -> GEOLOCATION_WITH_OPTIONS.
    // TODO(https://crbug.com/441689815): Remove this code once approximate
    // geolocation launches, the transition is completed and
    // ContentSettingsType::GEOLOCATION is removed.
    ContentSettingsType type = it.first;
    base::Value value = std::move(it.second);
    if (type == ContentSettingsType::GEOLOCATION) {
      type = content_settings::GeolocationContentSettingsType();
    }

    const content_settings::PermissionSettingsInfo* permission_settings_info =
        content_settings::PermissionSettingsRegistry::GetInstance()->Get(type);
    if (permission_settings_info) {
      std::optional<PermissionSetting> permission_setting =
          permission_settings_info->delegate().FromValue(value);
      if (!permission_setting) {
        // If we were unable to find the revoked permission setting, restore to
        // a default allow value.
        permission_setting =
            permission_settings_info->delegate().ToPermissionSetting(
                ContentSetting::CONTENT_SETTING_ALLOW);
      }
      hcsm()->SetPermissionSettingCustomScope(info.primary_pattern,
                                              info.secondary_pattern, type,
                                              permission_setting);
    }
  }

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
  // Set this to true to prevent `OnContentSettingChanged` from removing
  // revoked setting values, since this is set with specific values below.
  base::AutoReset<bool> is_unused_site_revocation_running(
      &is_unused_site_revocation_running_, true);
  base::flat_map<ContentSettingsType, base::Value> unused_permissions;
  for (const auto& [permission, value] : permissions_data.permissions) {
    // Ignore notifications since they are already handled as revoked abusive or
    // disruptive notifications.
    if (permission == ContentSettingsType::NOTIFICATIONS ||
        !IsPermissionSetting(permission)) {
      continue;
    }
    unused_permissions.insert(std::make_pair(permission, value.Clone()));
    // Setting a new value to a permission will set
    // `bypassed_autorevocation_by_user` to default (false) value so no need
    // to reset it separately.
    hcsm()->SetPermissionSettingCustomScope(permissions_data.primary_pattern,
                                            ContentSettingsPattern::Wildcard(),
                                            permission, std::nullopt);
  }

  StorePermissionInUnusedSitePermissionSetting(
      std::move(unused_permissions), permissions_data.constraints.Clone(),
      permissions_data.primary_pattern, ContentSettingsPattern::Wildcard());
}

void UnusedSitePermissionsManager::
    DeletePatternFromRevokedUnusedSitePermissionList(
        const ContentSettingsPattern& primary_pattern,
        const ContentSettingsPattern& secondary_pattern) {
  hcsm()->SetWebsiteSettingCustomScope(
      primary_pattern, secondary_pattern,
      ContentSettingsType::REVOKED_UNUSED_SITE_PERMISSIONS, {});
}

namespace {

void FilterUnusedPermissions(
    base::flat_map<ContentSettingsType, base::Value>& permissions) {
  base::EraseIf(permissions, [](const auto& it) {
    const auto& [type, value] = it;
    return type == ContentSettingsType::NOTIFICATIONS ||
           !IsPermissionSetting(type);
  });
}

}  // namespace

void UnusedSitePermissionsManager::StorePermissionInUnusedSitePermissionSetting(
    base::flat_map<ContentSettingsType, base::Value> permissions,
    const std::optional<content_settings::ContentSettingConstraints> constraint,
    const ContentSettingsPattern& primary_pattern,
    const ContentSettingsPattern& secondary_pattern) {
  FilterUnusedPermissions(permissions);
  if (permissions.empty()) {
    return;
  }
  GURL url = GURL(primary_pattern.ToString());
  // The url should be valid as it is checked that the pattern represents a
  // single origin.
  CHECK(url.is_valid());
  // Get the current value of the setting to append the recently revoked
  // permissions.
  base::Value cur_value(hcsm()->GetWebsiteSetting(
      url, url, ContentSettingsType::REVOKED_UNUSED_SITE_PERMISSIONS));

  base::DictValue dict =
      cur_value.is_dict() ? std::move(cur_value.GetDict()) : base::DictValue();
  base::ListValue* permission_list = dict.EnsureList(permissions::kRevokedKey);

  // Revoked unused permissions are stored as element in a list. Simple content
  // settings are just encoded as strings (using
  // ConvertContentSettingTypeToKey()). PermissionSettings which are not
  // ContentSettings are stored as a dict {kRevokerPermissionType:
  // ConvertContentSettingTypeToKey(), kRevokerPermissionSettingValue:
  // revoked_value} containing also the original revoked_value, so that they can
  // be regranted.
  for (auto&& [content_setting_type, setting_value] : permissions) {
    std::string content_setting_key =
        ConvertContentSettingsTypeToKey(content_setting_type);
    if (setting_value == base::Value(CONTENT_SETTING_ALLOW)) {
      permission_list->Append(content_setting_key);
    } else {
      base::DictValue item;
      item.Set(permissions::kRevokedPermissionType, content_setting_key);
      item.Set(permissions::kRevokedPermissionSettingValue,
               std::move(setting_value));
      permission_list->Append(std::move(item));
    }
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

UnusedSitePermissionsManager::UntimestampedPermissionList
UnusedSitePermissionsManager::GetUntimestampedPermissionsForTesting() {
  UntimestampedPermissionList result;
  for (const auto& permission : untimestamped_permissions_) {
    result.push_back(permission);
  }
  return result;
}

void UnusedSitePermissionsManager::IgnoreOriginForAutoRevocation(
    const url::Origin& origin) {
  auto* registry = content_settings::PermissionSettingsRegistry::GetInstance();

  for (const content_settings::PermissionSettingsInfo* info : *registry) {
    ContentSettingsType type = info->website_settings_info()->type();

    for (const auto& setting : hcsm()->GetSettingsForOneType(type)) {
      if (setting.primary_pattern.MatchesSingleOrigin() &&
          setting.primary_pattern.Matches(origin.GetURL())) {
        hcsm()->SetAutorevocationBypassedByUser(
            setting.primary_pattern, setting.secondary_pattern, type);
        break;
      }
    }
  }
}

void UnusedSitePermissionsManager::UpdateIntegerValuesToGroupName() {
  ContentSettingsForOneType settings = hcsm()->GetSettingsForOneType(
      ContentSettingsType::REVOKED_UNUSED_SITE_PERMISSIONS);

  bool successful_migration = true;
  for (auto&& revoked_permissions : settings) {
    base::Value& stored_value = revoked_permissions.setting_value;
    CHECK(stored_value.is_dict());
    base::Value updated_dict(stored_value.Clone());

    base::ListValue* permission_value_list =
        stored_value.GetDict().FindList(permissions::kRevokedKey);
    if (permission_value_list) {
      base::ListValue updated_permission_value_list =
          ConvertContentSettingsIntValuesToString(
              std::move(*permission_value_list), &successful_migration);
      updated_dict.GetDict().Set(permissions::kRevokedKey,
                                 std::move(updated_permission_value_list));
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

void UnusedSitePermissionsManager::MaybePerformLastVisitedBackfill(
    RevokedPermissionsResult* interim_result) {
  if (!interim_result->GetRevocationBackfillEnabled()) {
    return;
  }

  bool revocation_backfill_completed =
      pref_change_registrar_->prefs()->GetBoolean(
          safety_hub_prefs::kUnusedSitePermissionsRevocationBackfillCompleted);
  if (revocation_backfill_completed) {
    return;
  }

  // Backfill untimestamped permissions' `last_visited` with the (coarsed)
  // current date.
  untimestamped_permissions_ = interim_result->GetUntimestampedPermissions();
  for (const auto& permission : untimestamped_permissions_) {
    hcsm()->UpdateLastVisitedTime(permission.primary_pattern,
                                  permission.secondary_pattern,
                                  permission.type);
  }
  pref_change_registrar_->prefs()->SetBoolean(
      safety_hub_prefs::kUnusedSitePermissionsRevocationBackfillCompleted,
      true);

  // Record the successful completion of the backfill run.
  UMA_HISTOGRAM_BOOLEAN(
      "Settings.SafetyHub.UnusedSitePermissionsModule.Backfill.RunStatus",
      true /*COMPLETED*/);
  // Record the number of permissions backfilled during one run.
  base::UmaHistogramCounts10000(
      "Settings.SafetyHub.UnusedSitePermissionsModule.Backfill."
      "ListCountOnCompletion",
      untimestamped_permissions_.size());
}
