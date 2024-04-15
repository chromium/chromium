// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/webui/settings/recent_site_settings_helper.h"

#include <vector>

#include "chrome/browser/content_settings/host_content_settings_map_factory.h"
#include "chrome/browser/permissions/permission_decision_auto_blocker_factory.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/ui/webui/settings/site_settings_helper.h"
#include "components/content_settings/core/common/content_settings_utils.h"
#include "components/permissions/permission_decision_auto_blocker.h"

namespace site_settings {

namespace {

// The priority for surfacing a permission to the user.
constexpr int GetPriorityForType(ContentSettingsType type) {
  switch (type) {
    case ContentSettingsType::GEOLOCATION:
      return 0;
    case ContentSettingsType::MEDIASTREAM_CAMERA:
      return 1;
    case ContentSettingsType::MEDIASTREAM_MIC:
      return 2;
    case ContentSettingsType::NOTIFICATIONS:
      return 3;
    case ContentSettingsType::BACKGROUND_SYNC:
      return 4;
    default:
      // Every other |content_type| is considered of lower but equal priority
      return 5;
  }
}

// Return the most recent timestamp from the settings contained in a
// RecentSitePermissions struct. Returns base::Time() if no settings exist.
base::Time GetMostRecentTimestamp(const RecentSitePermissions& x) {
  auto most_recent = base::Time();
  for (const auto& setting : x.settings) {
    if (setting.timestamp > most_recent)
      most_recent = setting.timestamp;
  }
  return most_recent;
}

// Return a map keyed on URLs of all TimestampedSettings which currently apply
// to the provided profile for the provided content types.
std::map<GURL, std::vector<TimestampedSetting>> GetAllSettingsForProfile(
    Profile* profile,
    std::vector<ContentSettingsType> content_types) {
  auto* auto_blocker =
      PermissionDecisionAutoBlockerFactory::GetForProfile(profile);
  HostContentSettingsMap* content_settings_map =
      HostContentSettingsMapFactory::GetForProfile(profile);
  std::map<GURL, std::vector<TimestampedSetting>> results;
  for (auto content_type : content_types) {
    auto exceptions_for_type =
        site_settings::GetSingleOriginExceptionsForContentType(
            content_settings_map, content_type);
    for (const auto& e : exceptions_for_type) {
      auto last_modified = e.metadata.last_modified();
      if (last_modified.is_null()) {
        continue;
      }
      GURL origin =
          GURL(e.primary_pattern.ToString()).DeprecatedGetOriginAsURL();
      results[origin].emplace_back(
          last_modified, content_type,
          content_settings::ValueToContentSetting(e.setting_value),
          site_settings::SiteSettingSource::kPreference);
    }

    // Get sites under embargo.
    auto embargoed_sites = auto_blocker->GetEmbargoedOrigins(content_type);
    for (auto& url : embargoed_sites) {
      auto last_modified =
          PermissionDecisionAutoBlockerFactory::GetForProfile(profile)
              ->GetEmbargoStartTime(url, content_type);
      results[url.DeprecatedGetOriginAsURL()].emplace_back(
          last_modified, content_type, ContentSetting::CONTENT_SETTING_BLOCK,
          site_settings::SiteSettingSource::kEmbargo);
    }
  }

  // Keep only the first entry for any content type. This will be the enforced
  // user set permission appropriate for the profile.
  for (auto& source_settings : results) {
    std::stable_sort(
        source_settings.second.begin(), source_settings.second.end(),
        [](const TimestampedSetting& x, const TimestampedSetting& y) {
          return x.content_type < y.content_type;
        });
    source_settings.second.erase(
        std::unique(
            source_settings.second.begin(), source_settings.second.end(),
            [](const TimestampedSetting& x, const TimestampedSetting& y) {
              return x.content_type == y.content_type;
            }),
        source_settings.second.end());
  }
  return results;
}

}  // namespace

TimestampedSetting::TimestampedSetting()
    : timestamp(base::Time()),
      content_type(ContentSettingsType::DEFAULT),
      content_setting(ContentSetting::CONTENT_SETTING_DEFAULT),
      setting_source(site_settings::SiteSettingSource::kDefault) {}
TimestampedSetting::TimestampedSetting(const TimestampedSetting& other) =
    default;
TimestampedSetting::TimestampedSetting(
    base::Time timestamp,
    ContentSettingsType content_type,
    ContentSetting content_setting,
    site_settings::SiteSettingSource setting_source)
    : timestamp(timestamp),
      content_type(content_type),
      content_setting(content_setting),
      setting_source(setting_source) {}
TimestampedSetting::~TimestampedSetting() = default;

RecentSitePermissions::RecentSitePermissions()
    : origin(GURL()),
      incognito(false),
      settings(std::vector<TimestampedSetting>()) {}
RecentSitePermissions::RecentSitePermissions(
    const RecentSitePermissions& other) = default;
RecentSitePermissions::RecentSitePermissions(RecentSitePermissions&& other) =
    default;
RecentSitePermissions::RecentSitePermissions(
    GURL origin,
    const std::string& display_name,
    bool incognito,
    std::vector<TimestampedSetting> settings)
    : origin(origin),
      display_name(display_name),
      incognito(incognito),
      settings(settings) {}
RecentSitePermissions::~RecentSitePermissions() = default;

std::vector<RecentSitePermissions> GetRecentSitePermissions(
    Profile* profile,
    std::vector<ContentSettingsType> content_types,
    size_t max_sources) {
  std::map<GURL, std::vector<TimestampedSetting>> regular_settings =
      GetAllSettingsForProfile(profile, content_types);
  std::map<GURL, std::vector<TimestampedSetting>> incognito_settings;

  if (profile->HasPrimaryOTRProfile()) {
    incognito_settings = GetAllSettingsForProfile(
        profile->GetPrimaryOTRProfile(/*create_if_needed=*/true),
        content_types);

    // Remove all permission entries in the incognito map which also have
    // an entry in the regular settings. This may result in an empty setting
    // vector, in which case we remove the map entry.
    // TODO(crbug.com/40117710): Make determining actual source of
    // active permission simpler.
    for (auto incognito_iter = incognito_settings.begin();
         incognito_iter != incognito_settings.end();) {
      auto regular_iter = regular_settings.find(incognito_iter->first);
      if (regular_iter == regular_settings.end()) {
        ++incognito_iter;
        continue;
      }

      // Define an arbitrary ordering on TimestampedSetting's so we can sort
      // and make use of set difference
      auto arbitrary_strict_weak_ordering = [](const TimestampedSetting& x,
                                               const TimestampedSetting& y) {
        return std::tie(x.timestamp, x.content_type, x.content_setting,
                        x.setting_source) <
               std::tie(y.timestamp, y.content_type, y.content_setting,
                        y.setting_source);
      };

      std::vector<TimestampedSetting> incognito_only_settings;
      std::sort(regular_iter->second.begin(), regular_iter->second.end(),
                arbitrary_strict_weak_ordering);
      std::sort(incognito_iter->second.begin(), incognito_iter->second.end(),
                arbitrary_strict_weak_ordering);
      std::set_difference(
          incognito_iter->second.begin(), incognito_iter->second.end(),
          regular_iter->second.begin(), regular_iter->second.end(),
          std::back_inserter(incognito_only_settings),
          arbitrary_strict_weak_ordering);

      if (incognito_only_settings.empty()) {
        incognito_iter = incognito_settings.erase(incognito_iter);
      } else {
        incognito_iter->second = incognito_only_settings;
        ++incognito_iter;
      }
    }
  }

  // Combine incognito and regular permissions and sort based on the most
  // recent setting for each source.
  std::vector<RecentSitePermissions> all_site_permissions;
  for (auto& url_settings_pair : regular_settings) {
    all_site_permissions.emplace_back(
        url_settings_pair.first,
        site_settings::GetDisplayNameForGURL(profile, url_settings_pair.first,
                                             /*hostname_only=*/true),
        /*incognito=*/false, std::move(url_settings_pair.second));
  }
  for (auto& url_settings_pair : incognito_settings) {
    all_site_permissions.emplace_back(
        url_settings_pair.first,
        site_settings::GetDisplayNameForGURL(profile, url_settings_pair.first,
                                             /*hostname_only=*/true),
        /*incognito=*/true, std::move(url_settings_pair.second));
  }
  std::sort(all_site_permissions.begin(), all_site_permissions.end(),
            [](const RecentSitePermissions& x, const RecentSitePermissions& y) {
              return GetMostRecentTimestamp(x) > GetMostRecentTimestamp(y);
            });

  // Sort source permissions based on their priority for surfacing to the user
  for (auto& site_permissions : all_site_permissions) {
    std::sort(site_permissions.settings.begin(),
              site_permissions.settings.end(),
              [](const TimestampedSetting& x, const TimestampedSetting& y) {
                if (GetPriorityForType(x.content_type) !=
                    GetPriorityForType(y.content_type)) {
                  return GetPriorityForType(x.content_type) <
                         GetPriorityForType(y.content_type);
                }
                return x.timestamp > y.timestamp;
              });
  }

  if (all_site_permissions.size() <= max_sources) {
    return all_site_permissions;
  }

  // We only want to surface the recent permissions for sites. Explicitly no
  // settings older than the newest setting for the (max_sources + 1)th source.
  auto min_timestamp =
      GetMostRecentTimestamp(all_site_permissions[max_sources]);
  all_site_permissions.erase(all_site_permissions.begin() + max_sources,
                             all_site_permissions.end());

  for (auto& site_permissions : all_site_permissions) {
    std::erase_if(site_permissions.settings,
                  [min_timestamp](const TimestampedSetting& x) {
                    return x.timestamp < min_timestamp;
                  });
  }
  return all_site_permissions;
}

}  // namespace site_settings
