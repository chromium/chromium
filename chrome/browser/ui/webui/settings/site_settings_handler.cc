// Copyright 2016 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/webui/settings/site_settings_handler.h"

#include <memory>
#include <set>
#include <string_view>
#include <utility>
#include <vector>

#include "base/barrier_closure.h"
#include "base/check_deref.h"
#include "base/containers/contains.h"
#include "base/containers/flat_set.h"
#include "base/containers/to_value_list.h"
#include "base/feature_list.h"
#include "base/functional/bind.h"
#include "base/functional/callback_helpers.h"
#include "base/functional/overloaded.h"
#include "base/i18n/message_formatter.h"
#include "base/i18n/number_formatting.h"
#include "base/json/values_util.h"
#include "base/metrics/histogram_functions.h"
#include "base/metrics/histogram_macros.h"
#include "base/metrics/user_metrics.h"
#include "base/no_destructor.h"
#include "base/ranges/algorithm.h"
#include "base/strings/string_util.h"
#include "base/strings/utf_string_conversions.h"
#include "base/values.h"
#include "build/chromeos_buildflags.h"
#include "chrome/browser/apps/app_service/app_service_proxy.h"
#include "chrome/browser/apps/app_service/app_service_proxy_factory.h"
#include "chrome/browser/bluetooth/bluetooth_chooser_context_factory.h"
#include "chrome/browser/browsing_data/chrome_browsing_data_model_delegate.h"
#include "chrome/browser/browsing_topics/browsing_topics_service_factory.h"
#include "chrome/browser/content_settings/chrome_content_settings_utils.h"
#include "chrome/browser/content_settings/host_content_settings_map_factory.h"
#include "chrome/browser/file_system_access/chrome_file_system_access_permission_context.h"
#include "chrome/browser/file_system_access/file_system_access_features.h"
#include "chrome/browser/file_system_access/file_system_access_permission_context_factory.h"
#include "chrome/browser/hid/hid_chooser_context.h"
#include "chrome/browser/hid/hid_chooser_context_factory.h"
#include "chrome/browser/media/unified_autoplay_config.h"
#include "chrome/browser/permissions/permission_decision_auto_blocker_factory.h"
#include "chrome/browser/permissions/system/system_permission_settings.h"
#include "chrome/browser/privacy_sandbox/privacy_sandbox_service.h"
#include "chrome/browser/privacy_sandbox/privacy_sandbox_service_factory.h"
#include "chrome/browser/serial/serial_chooser_context.h"
#include "chrome/browser/serial/serial_chooser_context_factory.h"
#if BUILDFLAG(IS_CHROMEOS)
#include "chrome/browser/smart_card/smart_card_permission_context.h"
#include "chrome/browser/smart_card/smart_card_permission_context_factory.h"
#endif
#include "chrome/browser/ui/browser.h"
#include "chrome/browser/ui/browser_list.h"
#include "chrome/browser/ui/page_info/page_info_infobar_delegate.h"
#include "chrome/browser/ui/safety_hub/notification_permission_review_service_factory.h"
#include "chrome/browser/ui/tabs/tab_strip_model.h"
#include "chrome/browser/ui/url_identity.h"
#include "chrome/browser/ui/webui/settings/recent_site_settings_helper.h"
#include "chrome/browser/ui/webui/settings/site_settings_helper.h"
#include "chrome/browser/usb/usb_chooser_context.h"
#include "chrome/browser/usb/usb_chooser_context_factory.h"
#include "chrome/browser/web_applications/isolated_web_apps/isolated_web_app_url_info.h"
#include "chrome/browser/web_applications/web_app_provider.h"
#include "chrome/browser/web_applications/web_app_registrar.h"
#include "chrome/common/chrome_features.h"
#include "chrome/common/extensions/manifest_handlers/app_launch_info.h"
#include "chrome/common/pref_names.h"
#include "chrome/common/url_constants.h"
#include "chrome/grit/generated_resources.h"
#include "components/browsing_data/content/browsing_data_model.h"
#include "components/browsing_topics/browsing_topics_service.h"
#include "components/content_settings/core/browser/cookie_settings.h"
#include "components/content_settings/core/browser/website_settings_info.h"
#include "components/content_settings/core/browser/website_settings_registry.h"
#include "components/content_settings/core/common/content_settings.h"
#include "components/content_settings/core/common/content_settings_types.h"
#include "components/content_settings/core/common/content_settings_utils.h"
#include "components/content_settings/core/common/pref_names.h"
#include "components/crx_file/id_util.h"
#include "components/infobars/content/content_infobar_manager.h"
#include "components/permissions/contexts/bluetooth_chooser_context.h"
#include "components/permissions/object_permission_context_base.h"
#include "components/permissions/permission_decision_auto_blocker.h"
#include "components/permissions/permission_uma_util.h"
#include "components/permissions/permission_util.h"
#include "components/prefs/pref_change_registrar.h"
#include "components/prefs/pref_service.h"
#include "components/privacy_sandbox/privacy_sandbox_features.h"
#include "components/services/app_service/public/cpp/app_registry_cache.h"
#include "components/services/app_service/public/cpp/app_types.h"
#include "components/services/app_service/public/cpp/app_update.h"
#include "components/site_engagement/content/site_engagement_service.h"
#include "components/strings/grit/components_strings.h"
#include "content/public/browser/browser_thread.h"
#include "content/public/browser/browsing_data_filter_builder.h"
#include "content/public/browser/browsing_data_remover.h"
#include "content/public/browser/storage_partition.h"
#include "content/public/browser/storage_partition_config.h"
#include "content/public/browser/web_contents.h"
#include "content/public/browser/web_ui.h"
#include "content/public/common/content_features.h"
#include "content/public/common/url_constants.h"
#include "extensions/browser/extension_registry.h"
#include "extensions/common/permissions/api_permission.h"
#include "extensions/common/permissions/permissions_data.h"
#include "services/network/public/cpp/is_potentially_trustworthy.h"
#include "storage/common/file_system/file_system_util.h"
#include "third_party/abseil-cpp/absl/types/variant.h"
#include "third_party/blink/public/common/features.h"
#include "third_party/blink/public/common/page/page_zoom.h"
#include "ui/base/l10n/l10n_util.h"
#include "ui/base/text/bytes_formatting.h"
#include "url/gurl.h"
#include "url/origin.h"
#include "url/url_constants.h"

#if BUILDFLAG(IS_CHROMEOS_ASH)
#include "components/user_manager/user_manager.h"
#endif

#if BUILDFLAG(IS_WIN)
#include "chrome/browser/media/cdm_document_service_impl.h"
#endif

#if BUILDFLAG(ENABLE_EXTENSIONS)
#include "extensions/common/constants.h"
#endif

using extensions::mojom::APIPermissionID;

namespace settings {

namespace {

using GroupingKey = SiteSettingsHandler::GroupingKey;

// Keys of the dictionary returned by HandleIsPatternValidForType.
constexpr char kIsValidKey[] = "isValid";
constexpr char kReasonKey[] = "reason";

constexpr char kEffectiveTopLevelDomainPlus1Name[] = "etldPlus1";
constexpr char kGroupingKey[] = "groupingKey";
constexpr char kGroupingKeyEtldPrefix[] = "etld:";
constexpr char kGroupingKeyOriginPrefix[] = "origin:";
constexpr char kOriginList[] = "origins";
constexpr char kNumCookies[] = "numCookies";
constexpr char kHasPermissionSettings[] = "hasPermissionSettings";
constexpr char kHasInstalledPWA[] = "hasInstalledPWA";
constexpr char kIsInstalled[] = "isInstalled";
constexpr char kRwsOwner[] = "rwsOwner";
constexpr char kRwsNumMembers[] = "rwsNumMembers";
constexpr char kRwsEnterpriseManaged[] = "rwsEnterpriseManaged";
constexpr char kZoom[] = "zoom";

constexpr uint16_t kHttpsDefaultPort = 443;

// Content types for chooser data.
constexpr ContentSettingsType kChooserDataContentSettingsTypes[] = {
    ContentSettingsType::BLUETOOTH_CHOOSER_DATA,
    ContentSettingsType::HID_CHOOSER_DATA,
    ContentSettingsType::SERIAL_CHOOSER_DATA,
    ContentSettingsType::USB_CHOOSER_DATA,
};

// Content types related to the double-patterned storage access settings.
// Entries in this array will be included in the results from HandleGetAllSites
// in addition to all the single-patterned settings.
constexpr ContentSettingsType kStorageAccessSettingsTypes[] = {
    ContentSettingsType::STORAGE_ACCESS,
    ContentSettingsType::TOP_LEVEL_STORAGE_ACCESS,
};

// These values are persisted to logs. Entries should not be renumbered and
// numeric values should never be reused.
enum class AllSitesAction2 {
  kLoadPage = 0,
  kResetSiteGroupPermissions = 1,
  kResetOriginPermissions = 2,
  kClearAllData = 3,
  kClearSiteGroupData = 4,
  kClearOriginData = 5,
  kEnterSiteDetails = 6,
  kRemoveSiteGroup = 7,
  kRemoveOrigin = 8,
  kRemoveOriginPartitioned = 9,
  kFilterByFpsOwner = 10,
  kDeleteForEntireFps = 11,
  kMaxValue = kDeleteForEntireFps,
};

// Return an appropriate API Permission ID for the given string name.
APIPermissionID APIPermissionFromGroupName(std::string type) {
  // Once there are more than two groups to consider, this should be changed to
  // something better than if's.

  if (site_settings::ContentSettingsTypeFromGroupName(type) ==
      ContentSettingsType::GEOLOCATION) {
    return APIPermissionID::kGeolocation;
  }

  if (site_settings::ContentSettingsTypeFromGroupName(type) ==
      ContentSettingsType::NOTIFICATIONS) {
    return APIPermissionID::kNotifications;
  }

  return APIPermissionID::kInvalid;
}

// Asks the |profile| for hosted apps which have the |permission| set, and
// adds their web extent and launch URL to the |exceptions| list.
void AddExceptionsGrantedByHostedApps(content::BrowserContext* context,
                                      APIPermissionID permission,
                                      base::Value::List* exceptions) {
  const extensions::ExtensionSet& extensions =
      extensions::ExtensionRegistry::Get(context)->enabled_extensions();
  for (extensions::ExtensionSet::const_iterator extension = extensions.begin();
       extension != extensions.end(); ++extension) {
    if (!(*extension)->is_hosted_app() ||
        !(*extension)->permissions_data()->HasAPIPermission(permission)) {
      continue;
    }

    const extensions::URLPatternSet& web_extent = (*extension)->web_extent();
    // Add patterns from web extent.
    for (const auto& pattern : web_extent) {
      std::string url_pattern = pattern.GetAsString();
      site_settings::AddExceptionForHostedApp(url_pattern, *extension->get(),
                                              exceptions);
    }
    // Retrieve the launch URL.
    GURL launch_url =
        extensions::AppLaunchInfo::GetLaunchWebURL(extension->get());
    // Skip adding the launch URL if it is part of the web extent.
    if (web_extent.MatchesURL(launch_url))
      continue;
    site_settings::AddExceptionForHostedApp(launch_url.spec(),
                                            *extension->get(), exceptions);
  }
}

base::flat_set<url::Origin> GetInstalledAppOrigins(Profile* profile) {
  if (!apps::AppServiceProxyFactory::IsAppServiceAvailableForProfile(profile))
    return base::flat_set<url::Origin>();

  std::vector<url::Origin> origins;
  apps::AppServiceProxyFactory::GetForProfile(profile)
      ->AppRegistryCache()
      .ForEachApp([&origins](const apps::AppUpdate& update) {
        if (update.AppType() == apps::AppType::kWeb ||
            update.AppType() == apps::AppType::kSystemWeb) {
          // For web apps, |PublisherId()| is set to the start URL.
          const GURL start_url(update.PublisherId());
          DCHECK(start_url.is_valid());
          origins.push_back(url::Origin::Create(start_url));
        }
      });
  return base::flat_set<url::Origin>(std::move(origins));
}

// Placeholder opaque origin until a valid origin is added. If a group only has
// the placeholder origin, then replace placeholder with the GroupingKey.
const url::Origin& GetPlaceholderOrigin() {
  static auto placeholder_origin = base::NoDestructor(url::Origin());
  return *placeholder_origin;
}

// Inserts |origin| into |site_group_map|, creating a new group if necessary.
// Origins are grouped by their GroupingKey. If |origin| is HTTP/HTTPS, the
// GroupingKey will be |partition_etld_plus1| or |origin|'s eTLD+1. The
// GroupingKey for other schemes will be |origin|.
// There are three cases:
// 1. The GroupingKey is not yet in |site_group_map|. We add the GroupingKey
//    to |site_group_map|. If |origin| is an eTLD+1 cookie origin,
//    put a placeholder origin in the new group.
// 2. The GroupingKey is in |site_group_map| and |origin| is an eTLD+1 cookie
//    origin. This means case 1 has already happened and nothing more needs to
//    be done.
// 3. The GroupingKey is in |site_group_map| and |origin| is NOT an eTLD+1
//    cookie origin. For a cookie origin, if a https origin with same host and
//    partitioned status exists, nothing more needs to be done.
// In case 3, we try to add |origin| to the set of origins for the GroupingKey.
// If an existing origin is a placeholder, delete it, because the placeholder
// is no longer needed.
void InsertOriginIntoGroup(
    SiteSettingsHandler::AllSitesMap* site_group_map,
    const url::Origin& origin,
    bool is_origin_with_cookies = false,
    std::optional<GroupingKey> partition_grouping_key = std::nullopt) {
  const url::Origin& placeholder_origin = GetPlaceholderOrigin();
  bool is_partitioned = partition_grouping_key.has_value();
  GroupingKey grouping_key = partition_grouping_key.has_value()
                                 ? partition_grouping_key.value()
                                 : GroupingKey::Create(origin);
  auto group = site_group_map->find(grouping_key);
  bool is_etld_plus1_cookie_origin =
      is_origin_with_cookies && origin.GetURL().SchemeIsHTTPOrHTTPS() &&
      grouping_key.GetEtldPlusOne() == origin.host();

  // Case 1:
  if (group == site_group_map->end()) {
    url::Origin new_origin =
        is_etld_plus1_cookie_origin ? placeholder_origin : origin;
    site_group_map->emplace(
        grouping_key,
        std::set<std::pair<url::Origin, bool>>({{new_origin, is_partitioned}}));
    return;
  }
  // Case 2:
  if (is_etld_plus1_cookie_origin && !is_partitioned) {
    return;
  }
  // Case 3:
  if (is_origin_with_cookies && origin.scheme() == url::kHttpScheme) {
    // Cookies ignore schemes, so try and see if a https schemed version
    // already exists in the origin list, if not, then add the http schemed
    // version into the map.
    auto https_origin = url::Origin::CreateFromNormalizedTuple(
        url::kHttpsScheme, origin.host(), kHttpsDefaultPort);
    if (group->second.find({https_origin, is_partitioned}) !=
        group->second.end()) {
      return;
    }
  }
  group->second.insert({origin, is_partitioned});
  // Find the placeholder with unpartitioned state as it's no longer needed.
  auto placeholder =
      group->second.find({placeholder_origin, /*is_partitioned=*/false});
  if (placeholder != group->second.end()) {
    group->second.erase(placeholder);
  }
}

// Update the storage data in |origin_size_map|.
void UpdateDataForOrigin(const url::Origin& origin,
                         const int64_t size,
                         std::map<url::Origin, int64_t>* origin_size_map) {
  if (size > 0) {
    (*origin_size_map)[origin] += size;
  }
}

// Converts |etld_plus1| into an origin representation by adding HTTP(S) scheme.
url::Origin ConvertEtldToOrigin(const std::string etld_plus1, bool secure) {
  if (secure) {
    return url::Origin::CreateFromNormalizedTuple(url::kHttpsScheme, etld_plus1,
                                                  kHttpsDefaultPort);
  }
  return url::Origin::CreateFromNormalizedTuple(url::kHttpScheme, etld_plus1,
                                                80);
}

bool IsPatternValidForType(const std::string& pattern_string,
                           ContentSettingsType content_type,
                           Profile* profile,
                           std::string* out_error) {
  ContentSettingsPattern pattern =
      ContentSettingsPattern::FromString(pattern_string);

  HostContentSettingsMap* map =
      HostContentSettingsMapFactory::GetForProfile(profile);

  // Don't allow patterns for WebUI schemes, even though it's a valid pattern.
  // WebUI permissions are controlled by ContentSettingsRegistry
  // AllowlistedSchemes and WebUIAllowlist. Users shouldn't be able to grant
  // extra permissions or revoke existing permissions.
  if (pattern.GetScheme() == ContentSettingsPattern::SCHEME_CHROME ||
      pattern.GetScheme() == ContentSettingsPattern::SCHEME_CHROMEUNTRUSTED ||
      pattern.GetScheme() == ContentSettingsPattern::SCHEME_DEVTOOLS ||
      pattern.GetScheme() == ContentSettingsPattern::SCHEME_CHROMESEARCH) {
    *out_error = l10n_util::GetStringUTF8(IDS_SETTINGS_NOT_VALID_WEB_ADDRESS);
    return false;
  }

  // Don't allow an input of '*', even though it's a valid pattern. This
  // changes the default setting.
  if (!pattern.IsValid() || pattern == ContentSettingsPattern::Wildcard()) {
    *out_error = l10n_util::GetStringUTF8(IDS_SETTINGS_NOT_VALID_WEB_ADDRESS);
    return false;
  }

  // Check if a setting can be set for this url and setting type, and if not,
  // return false with a string saying why.
  GURL url(pattern_string);
  if (url.is_valid() && map->IsRestrictedToSecureOrigins(content_type) &&
      !network::IsUrlPotentiallyTrustworthy(url)) {
    *out_error = l10n_util::GetStringUTF8(
        IDS_SETTINGS_NOT_VALID_WEB_ADDRESS_FOR_CONTENT_TYPE);
    return false;
  }

  // The pattern is valid.
  return true;
}

void UpdateDataFromModel(
    SiteSettingsHandler::AllSitesMap* all_sites_map,
    std::map<url::Origin, int64_t>* origin_size_map,
    const url::Origin& origin,
    int64_t size,
    std::optional<GroupingKey> partition_grouping_key = std::nullopt) {
  UpdateDataForOrigin(origin, size, origin_size_map);
  InsertOriginIntoGroup(all_sites_map, origin,
                        /*is_origin_with_cookies=*/false,
                        partition_grouping_key);
}

void LogAllSitesAction(AllSitesAction2 action) {
  UMA_HISTOGRAM_ENUMERATION("WebsiteSettings.AllSitesAction2", action);
}

// Returns the registrable domain (eTLD+1) for the `host`. If it doesn't exist,
// returns the host.
std::string GetEtldPlusOneForHost(const std::string& host) {
  auto eltd_plus_one = net::registry_controlled_domains::GetDomainAndRegistry(
      host, net::registry_controlled_domains::INCLUDE_PRIVATE_REGISTRIES);
  return eltd_plus_one.empty() ? host : eltd_plus_one;
}

// Returns the registrable domain (eTLD+1) for the `origin`. If it doesn't
// exist, returns the host.
std::string GetEtldPlusOne(const url::Origin& origin) {
  return GetEtldPlusOneForHost(origin.host());
}

// Converts |etld_plus1| into an HTTPS SchemefulSite.
net::SchemefulSite ConvertEtldToSchemefulSite(const std::string etld_plus1) {
  return net::SchemefulSite(GURL(std::string(url::kHttpsScheme) +
                                 url::kStandardSchemeSeparator + etld_plus1 +
                                 "/"));
}

// Iterates over data owners in `browsing_data_model` which contains all sites
// that have storage set and uses them to retrieve related website set
// membership information. Returns a map of site eTLD+1 matched with their RWS
// owner and count of related website set members.
std::map<std::string, std::pair<std::string, int>> GetRwsMap(
    PrivacySandboxService* privacy_sandbox_service,
    BrowsingDataModel* browsing_data_model) {
  // Used to count unique eTLD+1 owned by an RWS owner.
  std::map<std::string, std::set<std::string>> rws_owner_to_members;

  // Count members by unique eTLD+1 for each related website set.
  if (browsing_data_model) {
    for (const auto& entry : *browsing_data_model) {
      std::string etld_plus1 = GetEtldPlusOneForHost(
          BrowsingDataModel::GetHost(entry.data_owner.get()));
      auto schemeful_site = ConvertEtldToSchemefulSite(etld_plus1);
      auto rws_owner = privacy_sandbox_service->GetFirstPartySetOwner(
          schemeful_site.GetURL());
      if (rws_owner.has_value()) {
        rws_owner_to_members[rws_owner->GetURL().host()].insert(etld_plus1);
      }
    }
  }

  // site eTLD+1 : {owner site eTLD+1, # of sites in that related website set}
  std::map<std::string, std::pair<std::string, int>> rws_map;
  for (auto rws : rws_owner_to_members) {
    // Set rws owner and count of members for each eTLD+1
    for (auto member : rws.second) {
      rws_map[member] = {rws.first, rws.second.size()};
    }
  }

  return rws_map;
}

// Resolves |origin| to the correct value for its site group if it is a
// placeholder origin.
url::Origin ResolveOriginInSiteGroup(const GroupingKey& grouping_key,
                                     const url::Origin& origin) {
  if (origin != GetPlaceholderOrigin()) {
    return origin;
  }
  if (auto etld_plus1 = grouping_key.GetEtldPlusOne(); etld_plus1.has_value()) {
    return ConvertEtldToOrigin(*etld_plus1, /*secure=*/false);
  }
  return *grouping_key.GetOrigin();
}

// Converts a given |site_group_map| to a list of base::Value::Dicts, adding
// the site engagement score for each origin.
void ConvertSiteGroupMapToList(
    const SiteSettingsHandler::AllSitesMap& site_group_map,
    const std::set<url::Origin>& origin_permission_set,
    base::Value::List* list_value,
    Profile* profile,
    BrowsingDataModel* browsing_data_model) {
  DCHECK(profile);
  auto* privacy_sandbox_service =
      PrivacySandboxServiceFactory::GetForProfile(profile);
  auto rws_map = GetRwsMap(privacy_sandbox_service, browsing_data_model);
  base::flat_set<url::Origin> installed_origins =
      GetInstalledAppOrigins(profile);
  site_engagement::SiteEngagementService* engagement_service =
      site_engagement::SiteEngagementService::Get(profile);
  for (const auto& entry : site_group_map) {
    base::Value::Dict site_group;
    const GroupingKey& grouping_key = entry.first;
    site_group.Set(kGroupingKey, grouping_key.Serialize());

    // eTLD+1 is the effective top level domain + 1.
    std::optional<std::string> etld_plus1 = grouping_key.GetEtldPlusOne();
    std::optional<url::Origin> group_origin = grouping_key.GetOrigin();
    CHECK(etld_plus1 || group_origin);
    site_group.Set(site_settings::kDisplayName,
                   etld_plus1.has_value()
                       ? *etld_plus1
                       : site_settings::GetDisplayNameForGURL(
                             profile, group_origin->GetURL(),
                             /*hostname_only=*/false));
    if (etld_plus1.has_value()) {
      site_group.Set(kEffectiveTopLevelDomainPlus1Name, *etld_plus1);
    }

    bool has_installed_pwa = false;
    base::Value::List origin_list;
    for (const auto& origin_is_partitioned : entry.second) {
      const url::Origin& origin = origin_is_partitioned.first;
      bool is_partitioned = origin_is_partitioned.second;
      base::Value::Dict origin_object;
      // If origin is placeholder, use the grouping key for the origin.
      origin_object.Set(
          "origin",
          ResolveOriginInSiteGroup(grouping_key, origin).GetURL().spec());
      origin_object.Set("isPartitioned", is_partitioned);
      origin_object.Set("engagement",
                        engagement_service->GetScore(origin.GetURL()));
      origin_object.Set("usage", 0.0);
      origin_object.Set(kNumCookies, 0);

      bool is_installed = installed_origins.contains(origin);
      if (is_installed)
        has_installed_pwa = true;
      origin_object.Set(kIsInstalled, is_installed);

      origin_object.Set(kHasPermissionSettings,
                        base::Contains(origin_permission_set, origin));
      origin_list.Append(std::move(origin_object));
    }
    site_group.Set(kHasInstalledPWA, has_installed_pwa);
    site_group.Set(kNumCookies, 0);
    site_group.Set(kOriginList, std::move(origin_list));
    if (etld_plus1.has_value() && rws_map.count(*etld_plus1)) {
      site_group.Set(kRwsOwner, rws_map[*etld_plus1].first);
      site_group.Set(kRwsNumMembers, rws_map[*etld_plus1].second);
      auto schemeful_site = ConvertEtldToSchemefulSite(*etld_plus1);
      site_group.Set(kRwsEnterpriseManaged,
                     privacy_sandbox_service->IsPartOfManagedFirstPartySet(
                         schemeful_site));
    }
    list_value->Append(std::move(site_group));
  }
}

base::Value::Dict CreateZoomLevelException(
    const std::string& host_or_spec,
    const std::string& origin_for_favicon,
    const std::string& display_name,
    double zoom) {
  base::Value::Dict exception;
  exception.Set(site_settings::kHostOrSpec, host_or_spec);
  exception.Set(site_settings::kOriginForFavicon, origin_for_favicon);
  exception.Set(site_settings::kDisplayName, display_name);

  // Calculate the zoom percent from the factor. Round up to the nearest
  // whole number.
  int zoom_percent =
      static_cast<int>(blink::ZoomLevelToZoomFactor(zoom) * 100 + 0.5);
  exception.Set(kZoom, base::FormatPercent(zoom_percent));
  return exception;
}

}  // namespace

// static
GroupingKey GroupingKey::Create(const url::Origin& origin) {
  if (origin.GetURL().SchemeIsHTTPOrHTTPS()) {
    return GroupingKey(settings::GetEtldPlusOne(origin));
  }
  return GroupingKey(origin);
}

// static
GroupingKey GroupingKey::CreateFromEtldPlus1(const std::string& etld_plus1) {
  return GroupingKey(etld_plus1);
}

// static
GroupingKey GroupingKey::Deserialize(const std::string& serialized) {
  if (base::StartsWith(serialized, kGroupingKeyEtldPrefix)) {
    return GroupingKey::CreateFromEtldPlus1(
        serialized.substr(sizeof(kGroupingKeyEtldPrefix) - 1));
  }
  CHECK(base::StartsWith(serialized, kGroupingKeyOriginPrefix));
  GURL url(serialized.substr(sizeof(kGroupingKeyOriginPrefix) - 1));
  return GroupingKey::Create(url::Origin::Create(url));
}

GroupingKey::GroupingKey(const absl::variant<std::string, url::Origin>& value)
    : value_(value) {}

GroupingKey::GroupingKey(const GroupingKey& other) = default;
GroupingKey& GroupingKey::operator=(const GroupingKey& other) = default;
GroupingKey::~GroupingKey() = default;

std::string GroupingKey::Serialize() const {
  return absl::visit(base::Overloaded{[](const std::string& etld_plus1) {
                                        return kGroupingKeyEtldPrefix +
                                               etld_plus1;
                                      },
                                      [](const url::Origin& origin) {
                                        return kGroupingKeyOriginPrefix +
                                               origin.GetURL().spec();
                                      }},
                     value_);
}

std::optional<std::string> GroupingKey::GetEtldPlusOne() const {
  if (absl::holds_alternative<std::string>(value_)) {
    return absl::get<std::string>(value_);
  }
  return std::nullopt;
}

std::optional<url::Origin> GroupingKey::GetOrigin() const {
  if (absl::holds_alternative<url::Origin>(value_)) {
    return absl::get<url::Origin>(value_);
  }
  return std::nullopt;
}

url::Origin GroupingKey::ToOrigin() const {
  return absl::visit(
      base::Overloaded{[](const std::string& etld_plus1) {
                         return ConvertEtldToOrigin(etld_plus1,
                                                    /*secure=*/false);
                       },
                       [](const url::Origin& origin) { return origin; }},
      value_);
}

bool GroupingKey::operator<(const GroupingKey& other) const {
  // To keep extensions and Isolated Web Apps grouped together, convert the
  // GroupingKeys to an origin, putting all eTLD+1 keys under the same scheme
  // (HTTPS), and sort based on this origin.
  return ToOrigin() < other.ToOrigin();
}

SiteSettingsHandler::SiteSettingsHandler(Profile* profile)
    : profile_(profile) {}

SiteSettingsHandler::~SiteSettingsHandler() = default;

void SiteSettingsHandler::RegisterMessages() {
  web_ui()->RegisterMessageCallback(
      "fetchUsageTotal",
      base::BindRepeating(&SiteSettingsHandler::HandleFetchUsageTotal,
                          base::Unretained(this)));
  web_ui()->RegisterMessageCallback(
      "getRwsMembershipLabel",
      base::BindRepeating(&SiteSettingsHandler::HandleGetRwsMembershipLabel,
                          base::Unretained(this)));
  web_ui()->RegisterMessageCallback(
      "clearUnpartitionedUsage",
      base::BindRepeating(&SiteSettingsHandler::HandleClearUnpartitionedUsage,
                          base::Unretained(this)));
  web_ui()->RegisterMessageCallback(
      "clearPartitionedUsage",
      base::BindRepeating(&SiteSettingsHandler::HandleClearPartitionedUsage,
                          base::Unretained(this)));
  web_ui()->RegisterMessageCallback(
      "setDefaultValueForContentType",
      base::BindRepeating(
          &SiteSettingsHandler::HandleSetDefaultValueForContentType,
          base::Unretained(this)));
  web_ui()->RegisterMessageCallback(
      "getDefaultValueForContentType",
      base::BindRepeating(
          &SiteSettingsHandler::HandleGetDefaultValueForContentType,
          base::Unretained(this)));
  web_ui()->RegisterMessageCallback(
      "getAllSites",
      base::BindRepeating(&SiteSettingsHandler::HandleGetAllSites,
                          base::Unretained(this)));
  web_ui()->RegisterMessageCallback(
      "getCategoryList",
      base::BindRepeating(&SiteSettingsHandler::HandleGetCategoryList,
                          base::Unretained(this)));
  web_ui()->RegisterMessageCallback(
      "getRecentSitePermissions",
      base::BindRepeating(&SiteSettingsHandler::HandleGetRecentSitePermissions,
                          base::Unretained(this)));
  web_ui()->RegisterMessageCallback(
      "getFormattedBytes",
      base::BindRepeating(&SiteSettingsHandler::HandleGetFormattedBytes,
                          base::Unretained(this)));
  web_ui()->RegisterMessageCallback(
      "getExceptionList",
      base::BindRepeating(&SiteSettingsHandler::HandleGetExceptionList,
                          base::Unretained(this)));
  web_ui()->RegisterMessageCallback(
      "getStorageAccessExceptionList",
      base::BindRepeating(
          &SiteSettingsHandler::HandleGetStorageAccessExceptionList,
          base::Unretained(this)));
  web_ui()->RegisterMessageCallback(
      "getFileSystemGrants",
      base::BindRepeating(&SiteSettingsHandler::HandleGetFileSystemGrants,
                          base::Unretained(this)));
  web_ui()->RegisterMessageCallback(
      "revokeFileSystemGrant",
      base::BindRepeating(&SiteSettingsHandler::HandleRevokeFileSystemGrant,
                          base::Unretained(this)));
  web_ui()->RegisterMessageCallback(
      "revokeFileSystemGrants",
      base::BindRepeating(&SiteSettingsHandler::HandleRevokeFileSystemGrants,
                          base::Unretained(this)));
  web_ui()->RegisterMessageCallback(
      "getSmartCardReaderGrants",
      base::BindRepeating(&SiteSettingsHandler::HandleGetSmartCardReaderGrants,
                          base::Unretained(this)));
  web_ui()->RegisterMessageCallback(
      "revokeAllSmartCardReadersGrants",
      base::BindRepeating(
          &SiteSettingsHandler::HandleRevokeAllSmartCardReaderGrants,
          base::Unretained(this)));
  web_ui()->RegisterMessageCallback(
      "revokeSmartCardReaderGrant",
      base::BindRepeating(
          &SiteSettingsHandler::HandleRevokeSmartCardReaderGrant,
          base::Unretained(this)));
  web_ui()->RegisterMessageCallback(
      "getChooserExceptionList",
      base::BindRepeating(&SiteSettingsHandler::HandleGetChooserExceptionList,
                          base::Unretained(this)));
  web_ui()->RegisterMessageCallback(
      "getOriginPermissions",
      base::BindRepeating(&SiteSettingsHandler::HandleGetOriginPermissions,
                          base::Unretained(this)));
  web_ui()->RegisterMessageCallback(
      "setOriginPermissions",
      base::BindRepeating(&SiteSettingsHandler::HandleSetOriginPermissions,
                          base::Unretained(this)));
  web_ui()->RegisterMessageCallback(
      "resetCategoryPermissionForPattern",
      base::BindRepeating(
          &SiteSettingsHandler::HandleResetCategoryPermissionForPattern,
          base::Unretained(this)));
  web_ui()->RegisterMessageCallback(
      "setCategoryPermissionForPattern",
      base::BindRepeating(
          &SiteSettingsHandler::HandleSetCategoryPermissionForPattern,
          base::Unretained(this)));
  web_ui()->RegisterMessageCallback(
      "resetChooserExceptionForSite",
      base::BindRepeating(
          &SiteSettingsHandler::HandleResetChooserExceptionForSite,
          base::Unretained(this)));
  web_ui()->RegisterMessageCallback(
      "isOriginValid",
      base::BindRepeating(&SiteSettingsHandler::HandleIsOriginValid,
                          base::Unretained(this)));
  web_ui()->RegisterMessageCallback(
      "isPatternValidForType",
      base::BindRepeating(&SiteSettingsHandler::HandleIsPatternValidForType,
                          base::Unretained(this)));
  web_ui()->RegisterMessageCallback(
      "updateIncognitoStatus",
      base::BindRepeating(&SiteSettingsHandler::HandleUpdateIncognitoStatus,
                          base::Unretained(this)));
  web_ui()->RegisterMessageCallback(
      "fetchZoomLevels",
      base::BindRepeating(&SiteSettingsHandler::HandleFetchZoomLevels,
                          base::Unretained(this)));
  web_ui()->RegisterMessageCallback(
      "removeZoomLevel",
      base::BindRepeating(&SiteSettingsHandler::HandleRemoveZoomLevel,
                          base::Unretained(this)));
  web_ui()->RegisterMessageCallback(
      "setBlockAutoplayEnabled",
      base::BindRepeating(&SiteSettingsHandler::HandleSetBlockAutoplayEnabled,
                          base::Unretained(this)));
  web_ui()->RegisterMessageCallback(
      "fetchBlockAutoplayStatus",
      base::BindRepeating(&SiteSettingsHandler::HandleFetchBlockAutoplayStatus,
                          base::Unretained(this)));
  web_ui()->RegisterMessageCallback(
      "clearSiteGroupDataAndCookies",
      base::BindRepeating(
          &SiteSettingsHandler::HandleClearSiteGroupDataAndCookies,
          base::Unretained(this)));
  web_ui()->RegisterMessageCallback(
      "recordAction",
      base::BindRepeating(&SiteSettingsHandler::HandleRecordAction,
                          base::Unretained(this)));
  web_ui()->RegisterMessageCallback(
      "getNumCookiesString",
      base::BindRepeating(&SiteSettingsHandler::HandleGetNumCookiesString,
                          base::Unretained(this)));
  web_ui()->RegisterMessageCallback(
      "getSystemDeniedPermissions",
      base::BindRepeating(
          &SiteSettingsHandler::HandleGetSystemDeniedPermissions,
          base::Unretained(this)));
  web_ui()->RegisterMessageCallback(
      "openSystemPermissionSettings",
      base::BindRepeating(
          &SiteSettingsHandler::HandleOpenSystemPermissionSettings,
          base::Unretained(this)));
}

void SiteSettingsHandler::OnJavascriptAllowed() {
  ObserveSourcesForProfile(profile_);
  if (profile_->HasPrimaryOTRProfile()) {
    auto* primary_otr_profile =
        profile_->GetPrimaryOTRProfile(/*create_if_needed=*/true);
    // Avoid duplicate observation.
    if (primary_otr_profile != profile_)
      ObserveSourcesForProfile(primary_otr_profile);
  }

  // Listen for zoom changes in the default StoragePartition and the primary
  // StoragePartition of all installed Isolated Web Apps.
  auto zoom_changed_callback = base::BindRepeating(
      &SiteSettingsHandler::OnZoomLevelChanged, base::Unretained(this));
  host_zoom_map_subscriptions_.push_back(
      content::HostZoomMap::GetDefaultForBrowserContext(profile_)
          ->AddZoomLevelChangedCallback(zoom_changed_callback));
  for (const web_app::IsolatedWebAppUrlInfo& iwa_url_info :
       site_settings::GetInstalledIsolatedWebApps(profile_)) {
    content::StoragePartition* iwa_storage_partition =
        profile_->GetStoragePartition(
            iwa_url_info.storage_partition_config(profile_));
    host_zoom_map_subscriptions_.push_back(
        content::HostZoomMap::GetForStoragePartition(iwa_storage_partition)
            ->AddZoomLevelChangedCallback(zoom_changed_callback));
  }

  pref_change_registrar_ = std::make_unique<PrefChangeRegistrar>();
  pref_change_registrar_->Init(profile_->GetPrefs());

  // If the block autoplay pref changes send the new status.
  pref_change_registrar_->Add(
      prefs::kBlockAutoplayEnabled,
      base::BindRepeating(&SiteSettingsHandler::SendBlockAutoplayStatus,
                          base::Unretained(this)));

  // Setup observation of system permissions.
  system_permission_settings_observation_ = system_permission_settings::Observe(
      base::BindRepeating(&SiteSettingsHandler::OnSystemPermissionChanged,
                          base::Unretained(this)));
}

void SiteSettingsHandler::OnJavascriptDisallowed() {
  system_permission_settings_observation_.reset();
  observations_.RemoveAllObservations();
  chooser_observations_.RemoveAllObservations();
  host_zoom_map_subscriptions_.clear();
  pref_change_registrar_->Remove(prefs::kBlockAutoplayEnabled);
  observed_profiles_.RemoveAllObservations();
}

void SiteSettingsHandler::OnGetUsageInfo() {
  DCHECK_CURRENTLY_ON(content::BrowserThread::UI);
  // Site Details Page does not display the number of cookies for the origin.
  int64_t size = 0;
  std::string usage_string;
  std::string cookie_string;
  std::string rws_string;
  bool rwsPolicy = false;
  // TODO(crbug.com/40256547): Ensure the key uniquely identifies the owner of
  // the browsing data (hostname is insufficient) in the BrowsingDataModel.
  int num_cookies = 0;
  auto usage_origin = url::Origin::Create(GURL(usage_origin_));
  for (const BrowsingDataModel::BrowsingDataEntryView& entry :
       *browsing_data_model_) {
    if (!entry.Matches(usage_origin)) {
      continue;
    }
    size += entry.data_details->storage_size;
    // Display only first party cookies.
    if (!entry.GetThirdPartyPartitioningSite().has_value()) {
      num_cookies += entry.data_details->cookie_count;
    }
  }

  if (num_cookies > 0) {
    cookie_string = base::UTF16ToUTF8(l10n_util::GetPluralStringFUTF16(
        IDS_SETTINGS_SITE_SETTINGS_NUM_COOKIES, num_cookies));
  }

  if (size > 0) {
    usage_string = base::UTF16ToUTF8(ui::FormatBytes(size));
  }

  auto* privacy_sandbox_service =
      PrivacySandboxServiceFactory::GetForProfile(profile_);
  auto rws_map = GetRwsMap(privacy_sandbox_service, browsing_data_model_.get());
  auto etld_plus1 = GetEtldPlusOne(usage_origin);
  if (rws_map.count(etld_plus1)) {
    rws_string =
        base::UTF16ToUTF8(base::i18n::MessageFormatter::FormatWithNamedArgs(
            l10n_util::GetStringUTF16(
                IDS_SETTINGS_SITE_SETTINGS_RELATED_WEBSITE_SETS_MEMBERSHIP_LABEL),
            "MEMBERS", static_cast<int>(rws_map[etld_plus1].second),
            "RWS_OWNER", rws_map[etld_plus1].first));
    rwsPolicy = privacy_sandbox_service->IsPartOfManagedFirstPartySet(
        ConvertEtldToSchemefulSite(etld_plus1));
  }

  FireWebUIListener("usage-total-changed", base::Value(usage_origin_),
                    base::Value(usage_string), base::Value(cookie_string),
                    base::Value(rws_string), base::Value(rwsPolicy));
}

void SiteSettingsHandler::BrowsingDataModelCreated(
    std::unique_ptr<BrowsingDataModel> model) {
  browsing_data_model_ = std::move(model);
  ServicePendingRequests();
}

void SiteSettingsHandler::OnContentSettingChanged(
    const ContentSettingsPattern& primary_pattern,
    const ContentSettingsPattern& secondary_pattern,
    ContentSettingsType content_type) {
  if (!site_settings::HasRegisteredGroupName(content_type))
    return;

  if (primary_pattern.MatchesAllHosts() &&
      secondary_pattern.MatchesAllHosts()) {
    FireWebUIListener("contentSettingCategoryChanged",
                      base::Value(site_settings::ContentSettingsTypeToGroupName(
                          content_type)));
  } else {
    FireWebUIListener(
        "contentSettingSitePermissionChanged",
        base::Value(
            site_settings::ContentSettingsTypeToGroupName(content_type)),
        base::Value(primary_pattern.ToString()),
        base::Value(secondary_pattern == ContentSettingsPattern::Wildcard()
                        ? ""
                        : secondary_pattern.ToString()));
  }

  // If the default sound content setting changed then we should send block
  // autoplay status.
  if (primary_pattern.MatchesAllHosts() &&
      secondary_pattern.MatchesAllHosts() &&
      content_type == ContentSettingsType::SOUND) {
    SendBlockAutoplayStatus();
  }
}

void SiteSettingsHandler::OnOffTheRecordProfileCreated(
    Profile* off_the_record) {
  FireWebUIListener("onIncognitoStatusChanged", base::Value(true));
  ObserveSourcesForProfile(off_the_record);
}

void SiteSettingsHandler::OnProfileWillBeDestroyed(Profile* profile) {
  if (profile->IsOffTheRecord())
    FireWebUIListener("onIncognitoStatusChanged", base::Value(false));
  StopObservingSourcesForProfile(profile);
}

void SiteSettingsHandler::OnObjectPermissionChanged(
    std::optional<ContentSettingsType> guard_content_settings_type,
    ContentSettingsType data_content_settings_type) {
  if (!guard_content_settings_type ||
      !site_settings::HasRegisteredGroupName(*guard_content_settings_type) ||
      !site_settings::HasRegisteredGroupName(data_content_settings_type)) {
    return;
  }

  FireWebUIListener("contentSettingChooserPermissionChanged",
                    base::Value(site_settings::ContentSettingsTypeToGroupName(
                        *guard_content_settings_type)),
                    base::Value(site_settings::ContentSettingsTypeToGroupName(
                        data_content_settings_type)));
}

void SiteSettingsHandler::OnZoomLevelChanged(
    const content::HostZoomMap::ZoomLevelChange& change) {
  SendZoomLevels();
}

void SiteSettingsHandler::OnSystemPermissionChanged(
    ContentSettingsType content_type,
    bool is_blocked) {
  FireWebUIListener("osGlobalPermissionChanged", GetSystemDeniedPermissions());
}

void SiteSettingsHandler::HandleFetchUsageTotal(const base::Value::List& args) {
  AllowJavascript();
  CHECK_EQ(1U, args.size());
  usage_origin_ = args[0].GetString();

  update_site_details_ = true;
  RebuildModel();
}

void SiteSettingsHandler::HandleGetRwsMembershipLabel(
    const base::Value::List& args) {
  AllowJavascript();
  CHECK_EQ(3U, args.size());

  std::string callback_id = args[0].GetString();
  int num_members = args[1].GetInt();
  std::string rws_owner = args[2].GetString();

  const std::string label =
      base::UTF16ToUTF8(base::i18n::MessageFormatter::FormatWithNamedArgs(
          l10n_util::GetStringUTF16(
              IDS_SETTINGS_SITE_SETTINGS_RELATED_WEBSITE_SETS_MEMBERSHIP_LABEL),
          "MEMBERS", static_cast<int>(num_members), "RWS_OWNER", rws_owner));

  ResolveJavascriptCallback(base::Value(callback_id), base::Value(label));
}

void SiteSettingsHandler::HandleClearUnpartitionedUsage(
    const base::Value::List& args) {
  CHECK_EQ(1U, args.size());
  auto origin = url::Origin::Create(GURL(args[0].GetString()));
  if (origin.opaque())
    return;
  AllowJavascript();

  // TODO(crbug.com/40240175) - Permission info loading before storage info
  // can result in an interleaving of actions that means this pointer is
  // null (as it hasn't loaded yet, but the user can delete an entry which has
  // been created by permission info).
  if (browsing_data_model_) {
    // The provided origin may be an IWA, or a regular site.
    if (origin.GetURL().SchemeIsHTTPOrHTTPS()) {
      browsing_data_model_->RemoveUnpartitionedBrowsingData(origin.host(),
                                                            base::DoNothing());
    } else {
      browsing_data_model_->RemoveUnpartitionedBrowsingData(origin,
                                                            base::DoNothing());
    }
  }

  // The scheme for some sites detail page is http on
  // chrome://settings/content/all. Cookies or site data might not cleared if
  // the existing cookie scheme was https when users click the site detail link
  // to clear data. Hence, we need only additionally clear the HTTPS version if
  // an origin scheme is HTTP.
  std::vector<url::Origin> affected_origins = {origin};
  if (origin.GetURL().SchemeIs(url::kHttpScheme)) {
    GURL https_url = origin.GetURL();
    GURL::Replacements replacements;
    replacements.SetSchemeStr(url::kHttpsScheme);
    https_url = https_url.ReplaceComponents(replacements);
    auto https_origin = url::Origin::Create(https_url);
    affected_origins.emplace_back(https_origin);
  }

  RemoveNonModelData(affected_origins);
}

void SiteSettingsHandler::HandleClearPartitionedUsage(
    const base::Value::List& args) {
  CHECK_EQ(2U, args.size());
  auto origin = url::Origin::Create(GURL(args[0].GetString()));
  auto grouping_key = GroupingKey::Deserialize(args[1].GetString());

  // The group key should always be an eTLD+1 because there aren't any
  // partitioned entries for IWAs (which have a non-eTLD+1 grouping key).
  std::optional<std::string> group_etld_plus1 = grouping_key.GetEtldPlusOne();
  DCHECK(group_etld_plus1);

  net::SchemefulSite https_top_level_site(
      ConvertEtldToOrigin(*group_etld_plus1, true));
  browsing_data_model_->RemovePartitionedBrowsingData(
      origin.host(), https_top_level_site, base::DoNothing());

  net::SchemefulSite http_top_level_site =
      net::SchemefulSite(ConvertEtldToOrigin(*group_etld_plus1, false));
  browsing_data_model_->RemovePartitionedBrowsingData(
      origin.host(), http_top_level_site, base::DoNothing());
}

void SiteSettingsHandler::HandleSetDefaultValueForContentType(
    const base::Value::List& args) {
  CHECK_EQ(2U, args.size());
  const std::string& content_type = args[0].GetString();
  const std::string& setting = args[1].GetString();
  ContentSetting default_setting;
  CHECK(content_settings::ContentSettingFromString(setting, &default_setting));
  ContentSettingsType type =
      site_settings::ContentSettingsTypeFromGroupName(content_type);

  Profile* profile = profile_;
#if BUILDFLAG(IS_CHROMEOS_ASH)
  // ChromeOS special case: in Guest mode, settings are opened in Incognito
  // mode so we need the original profile to actually modify settings.
  if (user_manager::UserManager::Get()->IsLoggedInAsGuest())
    profile = profile->GetOriginalProfile();
#endif
  HostContentSettingsMap* map =
      HostContentSettingsMapFactory::GetForProfile(profile);
  ContentSetting previous_setting =
      map->GetDefaultContentSetting(type, nullptr);
  map->SetDefaultContentSetting(type, default_setting);

  if (type == ContentSettingsType::SOUND &&
      previous_setting != default_setting) {
    if (default_setting == CONTENT_SETTING_BLOCK) {
      base::RecordAction(
          base::UserMetricsAction("SoundContentSetting.MuteBy.DefaultSwitch"));
    } else {
      base::RecordAction(base::UserMetricsAction(
          "SoundContentSetting.UnmuteBy.DefaultSwitch"));
    }
  }
}

void SiteSettingsHandler::HandleGetDefaultValueForContentType(
    const base::Value::List& args) {
  AllowJavascript();

  CHECK_EQ(2U, args.size());
  const base::Value& callback_id = args[0];
  const std::string& type = args[1].GetString();

  ContentSettingsType content_type =
      site_settings::ContentSettingsTypeFromGroupName(type);
  HostContentSettingsMap* map =
      HostContentSettingsMapFactory::GetForProfile(profile_);

  base::Value::Dict category;
  site_settings::GetContentCategorySetting(map, content_type, &category);
  ResolveJavascriptCallback(callback_id, category);
}

void SiteSettingsHandler::HandleGetAllSites(const base::Value::List& args) {
  AllowJavascript();

  CHECK_EQ(1U, args.size());
  std::string callback_id = args[0].GetString();

  all_sites_map_.clear();
  origin_permission_set_.clear();

  // Incognito contains incognito content settings plus non-incognito content
  // settings. Thus if it exists, just get exceptions for the incognito profile.
  Profile* profile = profile_;
  if (profile_->HasPrimaryOTRProfile() &&
      profile_->GetPrimaryOTRProfile(/*create_if_needed=*/true) != profile_) {
    profile = profile_->GetPrimaryOTRProfile(/*create_if_needed=*/true);
  }
  DCHECK(profile);
  HostContentSettingsMap* map =
      HostContentSettingsMapFactory::GetForProfile(profile);

  std::vector<ContentSettingsType> content_types =
      site_settings::GetVisiblePermissionCategories();
  // Make sure to include cookies, because All Sites handles data storage
  // cookies as well as regular ContentSettingsTypes.
  content_types.push_back(ContentSettingsType::COOKIES);

  // Retrieve a list of embargoed settings to check separately. This ensures
  // that only settings included in |content_types| will be listed in all sites.
  auto* autoblocker =
      PermissionDecisionAutoBlockerFactory::GetForProfile(profile_);
  for (auto& url : autoblocker->GetEmbargoedOrigins(content_types)) {
    // Add |url| to the set if there are any embargo settings.
    auto origin = url::Origin::Create(url);
    InsertOriginIntoGroup(&all_sites_map_, origin);
    origin_permission_set_.insert(origin);
  }

  // Get permission exceptions which apply to a single site
  for (auto content_type : content_types) {
    auto exceptions = site_settings::GetSingleOriginExceptionsForContentType(
        map, content_type);
    for (const auto& e : exceptions) {
      auto origin = url::Origin::Create(GURL(e.primary_pattern.ToString()));
      InsertOriginIntoGroup(&all_sites_map_, origin);
      origin_permission_set_.insert(origin);
    }
  }

  // Include any storage access permissions; list the primary (embedded) site
  // using a representative URL.
  for (auto content_type : kStorageAccessSettingsTypes) {
    auto exceptions = map->GetSettingsForOneType(content_type);
    for (const auto& e : exceptions) {
      if (e.primary_pattern != ContentSettingsPattern::Wildcard()) {
        auto origin =
            url::Origin::Create(e.primary_pattern.ToRepresentativeUrl());
        InsertOriginIntoGroup(&all_sites_map_, origin);
        origin_permission_set_.insert(origin);
      }
    }
  }

  // Get device chooser permission exceptions.
  for (auto content_type : kChooserDataContentSettingsTypes) {
    std::string_view group_name =
        site_settings::ContentSettingsTypeToGroupName(content_type);
    DCHECK(!group_name.empty());
    const site_settings::ChooserTypeNameEntry* chooser_type =
        site_settings::ChooserTypeFromGroupName(group_name);
    DCHECK(chooser_type);
    base::Value::List exceptions =
        site_settings::GetChooserExceptionListFromProfile(profile_,
                                                          *chooser_type);
    for (const base::Value& exception : exceptions) {
      const base::Value::List* sites =
          exception.GetDict().FindList(site_settings::kSites);
      DCHECK(sites);
      for (const base::Value& site : *sites) {
        const std::string* origin_string =
            site.GetDict().FindString(site_settings::kOrigin);
        DCHECK(origin_string);
        auto origin = url::Origin::Create(GURL(*origin_string));
        InsertOriginIntoGroup(&all_sites_map_, origin);
        origin_permission_set_.insert(origin);
      }
    }
  }

  // Recreate the model to refresh the usage information.
  // This happens in the background and will send usage data to the page.
  send_sites_list_ = true;
  RebuildModel();

  base::Value::List result;

  // Respond with currently available data.
  ConvertSiteGroupMapToList(all_sites_map_, origin_permission_set_, &result,
                            profile, browsing_data_model_.get());

  LogAllSitesAction(AllSitesAction2::kLoadPage);

  ResolveJavascriptCallback(base::Value(callback_id), result);
}

void SiteSettingsHandler::HandleGetCategoryList(const base::Value::List& args) {
  AllowJavascript();

  CHECK_EQ(2U, args.size());
  std::string callback_id = args[0].GetString();
  const std::string& origin_string = args[1].GetString();

  base::Value::List result;
  for (ContentSettingsType content_type :
       site_settings::GetVisiblePermissionCategories(origin_string, profile_)) {
    result.Append(site_settings::ContentSettingsTypeToGroupName(content_type));
  }

  ResolveJavascriptCallback(base::Value(callback_id), result);
}

void SiteSettingsHandler::HandleGetRecentSitePermissions(
    const base::Value::List& args) {
  AllowJavascript();

  CHECK_EQ(2U, args.size());
  std::string callback_id = args[0].GetString();
  size_t max_sources = base::checked_cast<size_t>(args[1].GetInt());

  const std::vector<ContentSettingsType>& content_types =
      site_settings::GetVisiblePermissionCategories();
  auto recent_site_permissions = site_settings::GetRecentSitePermissions(
      profile_, content_types, max_sources);

  // Convert groups of TimestampedPermissions for consumption by JS
  base::Value::List result;
  for (const auto& site_permissions : recent_site_permissions) {
    DCHECK(!site_permissions.settings.empty());
    base::Value::Dict recent_site;
    recent_site.Set(site_settings::kOrigin, site_permissions.origin.spec());
    recent_site.Set(site_settings::kDisplayName, site_permissions.display_name);
    recent_site.Set(site_settings::kIncognito, site_permissions.incognito);

    base::Value::List permissions_list;
    for (const auto& p : site_permissions.settings) {
      base::Value::Dict recent_permission;
      recent_permission.Set(
          site_settings::kType,

          site_settings::ContentSettingsTypeToGroupName(p.content_type));
      recent_permission.Set(
          site_settings::kSetting,

          content_settings::ContentSettingToString(p.content_setting));
      recent_permission.Set(
          site_settings::kSource,

          site_settings::SiteSettingSourceToString(p.setting_source));
      permissions_list.Append(std::move(recent_permission));
    }
    recent_site.Set(site_settings::kRecentPermissions,
                    std::move(permissions_list));
    result.Append(std::move(recent_site));
  }
  ResolveJavascriptCallback(base::Value(callback_id), result);
}

base::Value::List SiteSettingsHandler::PopulateCookiesAndUsageData(
    Profile* profile) {
  std::map<url::Origin, int64_t> origin_size_map;
  std::map<std::pair<std::string, std::optional<std::string>>, int>
      host_cookie_map;
  base::Value::List list_value;

  GetOriginStorage(&all_sites_map_, &origin_size_map);
  GetHostCookies(&all_sites_map_, &host_cookie_map);
  ConvertSiteGroupMapToList(all_sites_map_, origin_permission_set_, &list_value,
                            profile, browsing_data_model_.get());

  // Merge the origin usage and cookies number into |list_value|.
  for (base::Value& item : list_value) {
    base::Value::Dict& site_group = item.GetDict();
    base::Value::List& origin_list = *site_group.FindList(kOriginList);
    int cookie_num = 0;
    auto grouping_key =
        GroupingKey::Deserialize(*site_group.FindString(kGroupingKey));
    // Add the number of eTLD+1 scoped cookies.
    std::optional<std::string> etld_plus1 = grouping_key.GetEtldPlusOne();
    if (etld_plus1.has_value()) {
      const auto& etld_plus1_cookie_num_it =
          std::as_const(host_cookie_map).find({*etld_plus1, std::nullopt});
      if (etld_plus1_cookie_num_it != host_cookie_map.end()) {
        cookie_num += etld_plus1_cookie_num_it->second;
      }
    }
    // Iterate over the origins for the group, and set their usage and cookie
    // numbers.
    for (base::Value& value : origin_list) {
      base::Value::Dict& origin_info = value.GetDict();
      auto origin =
          url::Origin::Create(GURL(*origin_info.FindString("origin")));
      bool is_partitioned =
          origin_info.FindBool("isPartitioned").value_or(false);

      const auto& size_info_it = origin_size_map.find(origin);
      if (size_info_it != origin_size_map.end()) {
        origin_info.Set("usage", static_cast<double>(size_info_it->second));
      }

      const auto& host_cookie_num_it = host_cookie_map.find(
          {origin.host(), (is_partitioned ? etld_plus1 : std::nullopt)});
      if (host_cookie_num_it != host_cookie_map.end()) {
        origin_info.Set(kNumCookies, host_cookie_num_it->second);
        // Add cookies numbers for origins that aren't an eTLD+1.
        if (origin.host() != etld_plus1 || is_partitioned) {
          cookie_num += host_cookie_num_it->second;
        }
      }
    }
    site_group.Set(kNumCookies, cookie_num);
  }
  return list_value;
}

void SiteSettingsHandler::OnStorageFetched() {
  AllowJavascript();
  FireWebUIListener("onStorageListFetched",
                    PopulateCookiesAndUsageData(profile_));
}

void SiteSettingsHandler::HandleGetFormattedBytes(
    const base::Value::List& args) {
  AllowJavascript();
  CHECK_EQ(2U, args.size());
  int64_t num_bytes = static_cast<int64_t>(args[1].GetDouble());
  ResolveJavascriptCallback(/*callback_id=*/args[0],
                            base::Value(ui::FormatBytes(num_bytes)));
}

void SiteSettingsHandler::HandleGetExceptionList(
    const base::Value::List& args) {
  AllowJavascript();

  CHECK_EQ(2U, args.size());
  const base::Value& callback_id = args[0];
  const std::string& type = args[1].GetString();
  ContentSettingsType content_type =
      site_settings::ContentSettingsTypeFromGroupName(type);

  base::Value::List exceptions;

  AddExceptionsGrantedByHostedApps(profile_, APIPermissionFromGroupName(type),
                                   &exceptions);
  site_settings::GetExceptionsForContentType(content_type, profile_, web_ui(),
                                             /*incognito=*/false, &exceptions);

  Profile* incognito =
      profile_->HasPrimaryOTRProfile()
          ? profile_->GetPrimaryOTRProfile(/*create_if_needed=*/true)
          : nullptr;
  // On Chrome OS in Guest mode the incognito profile is the primary profile,
  // so do not fetch an extra copy of the same exceptions.
  if (incognito && incognito != profile_) {
    site_settings::GetExceptionsForContentType(content_type, incognito,
                                               web_ui(),
                                               /*incognito=*/true, &exceptions);
  }

  ResolveJavascriptCallback(callback_id, exceptions);
}

void SiteSettingsHandler::HandleGetStorageAccessExceptionList(
    const base::Value::List& args) {
  AllowJavascript();

  CHECK_EQ(2U, args.size());
  const base::Value& callback_id = args[0];

  ContentSetting setting;
  CHECK(content_settings::ContentSettingFromString(args[1].GetString(),
                                                   &setting));

  Profile* incognito_ =
      profile_->HasPrimaryOTRProfile()
          ? profile_->GetPrimaryOTRProfile(/*create_if_needed=*/true)
          : nullptr;

  // On Chrome OS in Guest mode the incognito profile is the primary profile,
  // so do not fetch an extra copy of the same exceptions.
  if (incognito_ && incognito_ == profile_) {
    incognito_ = nullptr;
  }

  base::Value::List exceptions;
  site_settings::GetStorageAccessExceptions(setting, profile_, incognito_,
                                            web_ui(), &exceptions);

  ResolveJavascriptCallback(callback_id, exceptions);
}

void SiteSettingsHandler::HandleGetChooserExceptionList(
    const base::Value::List& args) {
  AllowJavascript();

  CHECK_EQ(2U, args.size());
  const base::Value& callback_id = args[0];
  const std::string& type = args[1].GetString();
  const site_settings::ChooserTypeNameEntry* chooser_type =
      site_settings::ChooserTypeFromGroupName(type);
  CHECK(chooser_type);

  base::Value::List exceptions =
      site_settings::GetChooserExceptionListFromProfile(profile_,
                                                        *chooser_type);
  ResolveJavascriptCallback(callback_id, exceptions);
}

void SiteSettingsHandler::HandleGetOriginPermissions(
    const base::Value::List& args) {
  AllowJavascript();

  CHECK_EQ(3U, args.size());
  const base::Value& callback_id = args[0];
  std::string origin = args[1].GetString();
  const base::Value::List& types = args[2].GetList();

  // Note: Invalid URLs will just result in default settings being shown.
  const GURL origin_url(origin);
  base::Value::List exceptions;
  for (const auto& type_val : types) {
    std::string type;
    DCHECK(type_val.is_string());
    const std::string* maybe_type = type_val.GetIfString();
    if (maybe_type)
      type = *maybe_type;
    ContentSettingsType content_type =
        site_settings::ContentSettingsTypeFromGroupName(type);
    CHECK(content_type != ContentSettingsType::DEFAULT)
        << type << " is not expected to have a UI representation.";
    HostContentSettingsMap* map =
        HostContentSettingsMapFactory::GetForProfile(profile_);

    site_settings::SiteSettingSource source;
    ContentSetting content_setting = site_settings::GetContentSettingForOrigin(
        profile_, map, origin_url, content_type, &source);
    std::string content_setting_string =
        content_settings::ContentSettingToString(content_setting);

    base::Value::Dict raw_site_exception;
    raw_site_exception.Set(site_settings::kEmbeddingOrigin, origin);
    raw_site_exception.Set(site_settings::kIncognito,
                           profile_->IsOffTheRecord());
    raw_site_exception.Set(site_settings::kOrigin, origin);
    raw_site_exception.Set(site_settings::kSetting, content_setting_string);
    raw_site_exception.Set(site_settings::kSource,
                           site_settings::SiteSettingSourceToString(source));

    UrlIdentity identity = site_settings::GetUrlIdentityForGURL(
        profile_, origin_url, /*hostname_only=*/false);
    std::string display_name;
    if (identity.type == UrlIdentity::Type::kChromeExtension ||
        identity.type == UrlIdentity::Type::kIsolatedWebApp) {
      // Append " (ID: <id>)" to extensions and IWA names as the user could have
      // multiple extensions/IWAs installed with the same name.
      display_name = l10n_util::GetStringFUTF8(
          IDS_SETTINGS_EXTENSION_OR_APP_DISPLAY_NAME, identity.name,
          base::UTF8ToUTF16(origin_url.host_piece()));
    } else {
      display_name = base::UTF16ToUTF8(identity.name);
    }
    raw_site_exception.Set(site_settings::kDisplayName, display_name);

    exceptions.Append(std::move(raw_site_exception));
  }

  ResolveJavascriptCallback(callback_id, exceptions);
}

void SiteSettingsHandler::HandleGetFileSystemGrants(
    const base::Value::List& args) {
  CHECK_EQ(1U, args.size());
  AllowJavascript();

  const base::Value& callback_id = args[0];
  base::Value::List grants = PopulateFileSystemGrantData();

  ResolveJavascriptCallback(callback_id, grants);
}

void SiteSettingsHandler::HandleRevokeFileSystemGrant(
    const base::Value::List& args) {
  DCHECK(base::FeatureList::IsEnabled(
      features::kFileSystemAccessPersistentPermissions));
  CHECK_EQ(2U, args.size());
  AllowJavascript();

  auto url = GURL(args[0].GetString());
  DCHECK(url.is_valid());
  const url::Origin& origin = url::Origin::Create(url);

  const base::FilePath& file_path =
      storage::StringToFilePath(args[1].GetString());

  ChromeFileSystemAccessPermissionContext* permission_context =
      FileSystemAccessPermissionContextFactory::GetForProfile(profile_);

  permission_context->RevokeGrant(origin, file_path);
}

void SiteSettingsHandler::HandleRevokeFileSystemGrants(
    const base::Value::List& args) {
  DCHECK(base::FeatureList::IsEnabled(
      features::kFileSystemAccessPersistentPermissions));

  CHECK_EQ(1U, args.size());
  AllowJavascript();

  auto url = GURL(args[0].GetString());
  DCHECK(url.is_valid());
  const url::Origin& origin = url::Origin::Create(url);

  ChromeFileSystemAccessPermissionContext* permission_context =
      FileSystemAccessPermissionContextFactory::GetForProfile(profile_);

  permission_context->RevokeGrants(origin);
}

void SiteSettingsHandler::HandleGetSmartCardReaderGrants(
    const base::Value::List& args) {
  DCHECK(base::FeatureList::IsEnabled(blink::features::kSmartCard));

  CHECK_EQ(1U, args.size());
  AllowJavascript();

  const base::Value& callback_id = args[0];
  base::Value::List reader_names;
#if BUILDFLAG(IS_CHROMEOS)
  SmartCardPermissionContext& permission_context =
      SmartCardPermissionContextFactory::GetForProfile(*profile_);

  reader_names = base::ToValueList(
      permission_context.GetPersistentReaderGrants(),
      [](const SmartCardPermissionContext::ReaderGrants& reader_grant) {
        return base::Value::Dict()
            .Set(site_settings::kReaderName, reader_grant.reader_name)
            .Set(site_settings::kOrigins,
                 base::ToValueList(reader_grant.origins,
                                   &url::Origin::Serialize));
      });
#endif
  ResolveJavascriptCallback(callback_id, reader_names);
}

void SiteSettingsHandler::HandleRevokeAllSmartCardReaderGrants(
    const base::Value::List& args) {
  DCHECK(base::FeatureList::IsEnabled(blink::features::kSmartCard));

  CHECK(args.empty());
  AllowJavascript();
#if BUILDFLAG(IS_CHROMEOS)
  SmartCardPermissionContext& permission_context =
      SmartCardPermissionContextFactory::GetForProfile(*profile_);

  permission_context.RevokeAllPermissions();
#endif
}

void SiteSettingsHandler::HandleRevokeSmartCardReaderGrant(
    const base::Value::List& args) {
  DCHECK(base::FeatureList::IsEnabled(blink::features::kSmartCard));

  CHECK_EQ(2U, args.size());
  AllowJavascript();
#if BUILDFLAG(IS_CHROMEOS)

  auto reader_name = args[0].GetString();
  auto url = GURL(args[1].GetString());
  DCHECK(url.is_valid());
  const url::Origin& origin = url::Origin::Create(url);

  SmartCardPermissionContext& permission_context =
      SmartCardPermissionContextFactory::GetForProfile(*profile_);
  permission_context.RevokePersistentPermission(reader_name, origin);
#endif
}

void SiteSettingsHandler::HandleSetOriginPermissions(
    const base::Value::List& args) {
  CHECK_EQ(3U, args.size());
  const std::string& origin_string = args[0].GetString();
  const std::string* type_string = args[1].GetIfString();
  std::string value = args[2].GetString();

  const GURL origin(origin_string);
  if (!origin.is_valid())
    return;

  ContentSetting setting;
  CHECK(content_settings::ContentSettingFromString(value, &setting));
  std::vector<ContentSettingsType> types;
  std::vector<ContentSettingsPattern> additional_patterns_for_infobar;
  if (type_string) {
    ContentSettingsType content_type =
        site_settings::ContentSettingsTypeFromGroupName(*type_string);
    CHECK(content_type != ContentSettingsType::DEFAULT)
        << *type_string << " is not expected to have a UI representation.";
    types.push_back(content_type);
  } else {
    // Clear device chooser data permission exceptions.
    if (setting == CONTENT_SETTING_DEFAULT) {
      for (auto content_type : kChooserDataContentSettingsTypes) {
        std::string_view group_name =
            site_settings::ContentSettingsTypeToGroupName(content_type);
        DCHECK(!group_name.empty());
        const site_settings::ChooserTypeNameEntry* chooser_type =
            site_settings::ChooserTypeFromGroupName(group_name);
        DCHECK(chooser_type);

        // The BluetoothChooserContext is only available when the
        // WebBluetoothNewPermissionsBackend flag is enabled.
        // TODO(crbug.com/40458188): Remove the nullptr check when it is enabled
        // by default.
        permissions::ObjectPermissionContextBase* chooser_context =
            chooser_type->get_context(profile_);
        if (!chooser_context) {
          continue;
        }

        auto objects = chooser_context->GetAllGrantedObjects();
        for (const auto& object : objects) {
          if (origin == object->origin) {
            chooser_context->RevokeObjectPermission(url::Origin::Create(origin),
                                                    object->value);
          }
        }
      }
    }

    types = site_settings::GetVisiblePermissionCategories();
  }

  HostContentSettingsMap* map =
      HostContentSettingsMapFactory::GetForProfile(profile_);
  for (ContentSettingsType content_type : types) {
    permissions::PermissionUmaUtil::ScopedRevocationReporter
        scoped_revocation_reporter(
            profile_, origin, origin, content_type,
            permissions::PermissionSourceUI::SITE_SETTINGS);

    // Clear any existing embargo status if the new setting isn't block.
    if (setting != CONTENT_SETTING_BLOCK) {
      PermissionDecisionAutoBlockerFactory::GetForProfile(profile_)
          ->RemoveEmbargoAndResetCounts(origin, content_type);
    }
    map->SetContentSettingDefaultScope(origin, origin, content_type, setting);

    const content_settings::WebsiteSettingsInfo::ScopingType scoping_type =
        content_settings::WebsiteSettingsRegistry::GetInstance()
            ->Get(content_type)
            ->scoping_type();

    // TODO(crbug.com/356170740) Refactor to eliminate the need for listing
    // content settings in here.
    if (setting == CONTENT_SETTING_DEFAULT &&
        (scoping_type == content_settings::WebsiteSettingsInfo::
                             REQUESTING_AND_TOP_SCHEMEFUL_SITE_SCOPE ||
         scoping_type == content_settings::WebsiteSettingsInfo::
                             REQUESTING_ORIGIN_AND_TOP_SCHEMEFUL_SITE_SCOPE)) {
      // In order to correctly set the reload infobanner on pages that have
      // embedded content from the origin being reset, we need to keep track of
      // any associated secondary patterns before we change anything.
      for (const auto& content_setting_pattern :
           map->GetSettingsForOneType(content_type)) {
        if (content_setting_pattern.primary_pattern.Matches(origin) &&
            content_setting_pattern.secondary_pattern !=
                ContentSettingsPattern::Wildcard()) {
          // Including the primary pattern isn't necessary since that matches
          // the origin we were called with, and is already handled by the
          // general-case logic.
          additional_patterns_for_infobar.push_back(
              content_setting_pattern.secondary_pattern);
        }
      }

      // The user probably expects that clearing double-keyed permissions will
      // clear the permissions in both directions. They may clear siteA's
      // permissions in order to prevent siteA embedded in siteB from accessing
      // its cookies, but they may also be aware that siteB is embedding siteA
      // and wish to clear the association via siteB's site details listing.
      map->ClearSettingsForOneTypeWithPredicate(
          content_type, [&](const ContentSettingPatternSource& pattern_source) {
            return pattern_source.primary_pattern.Matches(origin) ||
                   pattern_source.secondary_pattern.Matches(origin);
          });
    }

    if (content_type == ContentSettingsType::SOUND) {
      ContentSetting default_setting =
          map->GetDefaultContentSetting(ContentSettingsType::SOUND, nullptr);
      bool mute = (setting == CONTENT_SETTING_BLOCK) ||
                  (setting == CONTENT_SETTING_DEFAULT &&
                   default_setting == CONTENT_SETTING_BLOCK);
      if (mute) {
        base::RecordAction(
            base::UserMetricsAction("SoundContentSetting.MuteBy.SiteSettings"));
      } else {
        base::RecordAction(base::UserMetricsAction(
            "SoundContentSetting.UnmuteBy.SiteSettings"));
      }
    }

    permissions::PermissionUmaUtil::RecordPermissionRegrantForUnusedSites(
        origin, content_type, permissions::PermissionSourceUI::SITE_SETTINGS,
        profile_, base::Time::Now());
  }

  // Show an infobar reminding the user to reload tabs where their site
  // permissions have been updated.
  // Info bar should only be shown on pages with the same origin and
  // on the same profile, or on any pages where changes to a double-keyed
  // setting occurred.
  for (Browser* it : *BrowserList::GetInstance()) {
    TabStripModel* tab_strip = it->tab_strip_model();
    for (int i = 0; i < tab_strip->count(); ++i) {
      content::WebContents* web_contents = tab_strip->GetWebContentsAt(i);
      GURL tab_url = web_contents->GetLastCommittedURL();
      const bool tab_is_same_origin = url::IsSameOriginWith(origin, tab_url);
      const bool tab_might_embed_origin = base::ranges::any_of(
          additional_patterns_for_infobar, [&](const auto& additional_pattern) {
            return additional_pattern.Matches(tab_url);
          });

      if ((tab_is_same_origin || tab_might_embed_origin) &&
          it->profile()->GetOriginalProfile() ==
              profile_->GetOriginalProfile()) {
        infobars::ContentInfoBarManager* infobar_manager =
            infobars::ContentInfoBarManager::FromWebContents(web_contents);
        PageInfoInfoBarDelegate::Create(infobar_manager);
      }
    }
  }
}

void SiteSettingsHandler::HandleResetCategoryPermissionForPattern(
    const base::Value::List& args) {
  CHECK_EQ(4U, args.size());
  const std::string& primary_pattern_string = args[0].GetString();
  const std::string& secondary_pattern_string = args[1].GetString();
  const std::string& type = args[2].GetString();
  const bool& incognito = args[3].GetBool();

  ContentSettingsType content_type =
      site_settings::ContentSettingsTypeFromGroupName(type);
  CHECK(content_type != ContentSettingsType::DEFAULT)
      << type << " is not expected to have a UI representation.";

  Profile* profile = nullptr;
  if (incognito) {
    if (!profile_->HasPrimaryOTRProfile())
      return;
    profile = profile_->GetPrimaryOTRProfile(/*create_if_needed=*/true);
  } else {
    profile = profile_;
  }

  HostContentSettingsMap* map =
      HostContentSettingsMapFactory::GetForProfile(profile);

  ContentSettingsPattern primary_pattern =
      ContentSettingsPattern::FromString(primary_pattern_string);
  ContentSettingsPattern secondary_pattern =
      secondary_pattern_string.empty()
          ? ContentSettingsPattern::Wildcard()
          : ContentSettingsPattern::FromString(secondary_pattern_string);
  permissions::PermissionUmaUtil::ScopedRevocationReporter
      scoped_revocation_reporter(
          profile, primary_pattern, secondary_pattern, content_type,
          permissions::PermissionSourceUI::SITE_SETTINGS);

  map->SetContentSettingCustomScope(primary_pattern, secondary_pattern,
                                    content_type, CONTENT_SETTING_DEFAULT);

  if (content_type == ContentSettingsType::SOUND) {
    ContentSetting default_setting =
        map->GetDefaultContentSetting(ContentSettingsType::SOUND, nullptr);
    if (default_setting == CONTENT_SETTING_BLOCK) {
      base::RecordAction(base::UserMetricsAction(
          "SoundContentSetting.MuteBy.PatternException"));
    } else {
      base::RecordAction(base::UserMetricsAction(
          "SoundContentSetting.UnmuteBy.PatternException"));
    }
  }

  // End embargo if currently active.
  auto url = GURL(primary_pattern_string);
  if (url.is_valid()) {
    PermissionDecisionAutoBlockerFactory::GetForProfile(profile)
        ->RemoveEmbargoAndResetCounts(url, content_type);
  }

  if (content_type == ContentSettingsType::NOTIFICATIONS) {
    SendNotificationPermissionReviewList();
  }

  if (content_type == ContentSettingsType::COOKIES &&
      primary_pattern.MatchesAllHosts() &&
      !secondary_pattern.MatchesAllHosts()) {
    // Remove TP exceptions along with 3PC exceptions if we are not showing
    // them explicitly in settings but are supporting adding/removing via UB.
    // TODO(https://b/333527273): Remove post-3PCD launch.
    if (base::FeatureList::IsEnabled(
            privacy_sandbox::kTrackingProtectionContentSettingUbControl) &&
        !base::FeatureList::IsEnabled(
            privacy_sandbox::kTrackingProtectionContentSettingInSettings)) {
      map->SetContentSettingCustomScope(
          ContentSettingsPattern::Wildcard(), secondary_pattern,
          ContentSettingsType::TRACKING_PROTECTION, CONTENT_SETTING_DEFAULT);
    }
    base::RecordAction(base::UserMetricsAction(
        "ThirdPartyCookies.SettingsSiteException.Removed"));
  }

  if (content_type == ContentSettingsType::TRACKING_PROTECTION) {
    base::RecordAction(base::UserMetricsAction(
        "Settings.TrackingProtection.SiteExceptionRemoved"));
  }
}

void SiteSettingsHandler::HandleSetCategoryPermissionForPattern(
    const base::Value::List& args) {
  CHECK_EQ(5U, args.size());
  const std::string& primary_pattern_string = args[0].GetString();
  const std::string& secondary_pattern_string = args[1].GetString();
  const std::string& type = args[2].GetString();
  const std::string& value = args[3].GetString();
  const bool& incognito = args[4].GetBool();

  ContentSettingsType content_type =
      site_settings::ContentSettingsTypeFromGroupName(type);
  CHECK(content_type != ContentSettingsType::DEFAULT)
      << type << " is not expected to have a UI representation.";
  ContentSetting setting;
  CHECK(content_settings::ContentSettingFromString(value, &setting));

  Profile* target_profile = nullptr;
  if (incognito) {
    if (!profile_->HasPrimaryOTRProfile())
      return;
    target_profile = profile_->GetPrimaryOTRProfile(/*create_if_needed=*/true);
  } else {
    target_profile = profile_;
  }

  HostContentSettingsMap* map =
      HostContentSettingsMapFactory::GetForProfile(target_profile);

  ContentSettingsPattern primary_pattern =
      ContentSettingsPattern::FromString(primary_pattern_string);
  ContentSettingsPattern secondary_pattern =
      secondary_pattern_string.empty()
          ? ContentSettingsPattern::Wildcard()
          : ContentSettingsPattern::FromString(secondary_pattern_string);

  // Clear any existing embargo status if the new setting isn't block.
  if (setting != CONTENT_SETTING_BLOCK) {
    GURL url(primary_pattern.ToString());
    if (url.is_valid()) {
      PermissionDecisionAutoBlockerFactory::GetForProfile(target_profile)
          ->RemoveEmbargoAndResetCounts(url, content_type);
    }
  }

  permissions::PermissionUmaUtil::ScopedRevocationReporter
      scoped_revocation_reporter(
          target_profile, primary_pattern, secondary_pattern, content_type,
          permissions::PermissionSourceUI::SITE_SETTINGS);

  map->SetContentSettingCustomScope(primary_pattern, secondary_pattern,
                                    content_type, setting);

  if (content_type == ContentSettingsType::SOUND) {
    ContentSetting default_setting =
        map->GetDefaultContentSetting(ContentSettingsType::SOUND, nullptr);
    bool mute = (setting == CONTENT_SETTING_BLOCK) ||
                (setting == CONTENT_SETTING_DEFAULT &&
                 default_setting == CONTENT_SETTING_BLOCK);
    if (mute) {
      base::RecordAction(base::UserMetricsAction(
          "SoundContentSetting.MuteBy.PatternException"));
    } else {
      base::RecordAction(base::UserMetricsAction(
          "SoundContentSetting.UnmuteBy.PatternException"));
    }
  }

  if (content_type == ContentSettingsType::NOTIFICATIONS) {
    SendNotificationPermissionReviewList();
  }

  if (content_type == ContentSettingsType::COOKIES &&
      primary_pattern.MatchesAllHosts() &&
      !secondary_pattern.MatchesAllHosts()) {
    base::RecordAction(base::UserMetricsAction(
        "ThirdPartyCookies.SettingsSiteException.Added"));
  }

  if (content_type == ContentSettingsType::TRACKING_PROTECTION) {
    base::RecordAction(base::UserMetricsAction(
        "Settings.TrackingProtection.SiteExceptionAdded"));
  }
}

void SiteSettingsHandler::HandleResetChooserExceptionForSite(
    const base::Value::List& args) {
  CHECK_EQ(3U, args.size());

  const std::string& chooser_type_str = args[0].GetString();
  const site_settings::ChooserTypeNameEntry* chooser_type =
      site_settings::ChooserTypeFromGroupName(chooser_type_str);
  CHECK(chooser_type);

  auto origin_url = GURL(args[1].GetString());
  CHECK(origin_url.is_valid());
  auto origin = url::Origin::Create(origin_url);

  permissions::ObjectPermissionContextBase* chooser_context =
      chooser_type->get_context(profile_);
  chooser_context->RevokeObjectPermission(origin, args[2].GetDict());
}

void SiteSettingsHandler::HandleIsOriginValid(const base::Value::List& args) {
  AllowJavascript();
  CHECK_EQ(2U, args.size());
  const base::Value& callback_id = args[0];
  const std::string& origin_string = args[1].GetString();

  ResolveJavascriptCallback(callback_id,
                            base::Value(GURL(origin_string).is_valid()));
}

void SiteSettingsHandler::HandleIsPatternValidForType(
    const base::Value::List& args) {
  AllowJavascript();
  CHECK_EQ(3U, args.size());
  const base::Value& callback_id = args[0];
  const std::string& pattern_string = args[1].GetString();
  const std::string& type_string = args[2].GetString();

  ContentSettingsType content_type =
      site_settings::ContentSettingsTypeFromGroupName(type_string);
  CHECK(content_type != ContentSettingsType::DEFAULT)
      << type_string << " is not expected to have a UI representation.";

  std::string reason;
  bool is_valid =
      IsPatternValidForType(pattern_string, content_type, profile_, &reason);

  base::Value::Dict return_value;
  return_value.Set(kIsValidKey, base::Value(is_valid));
  return_value.Set(kReasonKey, base::Value(std::move(reason)));
  ResolveJavascriptCallback(callback_id, return_value);
}

void SiteSettingsHandler::HandleUpdateIncognitoStatus(
    const base::Value::List& args) {
  AllowJavascript();
  FireWebUIListener("onIncognitoStatusChanged",
                    base::Value(profile_->HasPrimaryOTRProfile()));
}

void SiteSettingsHandler::HandleFetchZoomLevels(const base::Value::List& args) {
  AllowJavascript();
  SendZoomLevels();
}

void SiteSettingsHandler::SendZoomLevels() {
  if (!IsJavascriptAllowed())
    return;

  base::Value::List zoom_levels_exceptions;

  // Show any non-default Isolated Web App zoom levels at the top of the page.
  auto* web_app_provider = web_app::WebAppProvider::GetForWebApps(profile_);
  if (web_app_provider) {
    const web_app::WebAppRegistrar& registrar =
        web_app_provider->registrar_unsafe();
    for (const web_app::IsolatedWebAppUrlInfo& iwa_url_info :
         site_settings::GetInstalledIsolatedWebApps(profile_)) {
      content::StoragePartition* iwa_storage_partition =
          profile_->GetStoragePartition(
              iwa_url_info.storage_partition_config(profile_));
      auto* host_zoom_map =
          content::HostZoomMap::GetForStoragePartition(iwa_storage_partition);
      double iwa_zoom = host_zoom_map->GetZoomLevelForHostAndScheme(
          chrome::kIsolatedAppScheme, iwa_url_info.origin().host());
      if (iwa_zoom == host_zoom_map->GetDefaultZoomLevel()) {
        continue;
      }

      zoom_levels_exceptions.Append(CreateZoomLevelException(
          iwa_url_info.origin().Serialize(), iwa_url_info.origin().Serialize(),
          registrar.GetAppShortName(iwa_url_info.app_id()), iwa_zoom));
    }

    // Sort by app name.
    std::sort(zoom_levels_exceptions.begin(), zoom_levels_exceptions.end(),
              [](const base::Value& a, const base::Value& b) {
                const std::string& name_a =
                    *a.GetDict().FindString(site_settings::kDisplayName);
                const std::string& name_b =
                    *b.GetDict().FindString(site_settings::kDisplayName);
                return name_a < name_b;
              });
  }

  content::HostZoomMap* host_zoom_map =
      content::HostZoomMap::GetDefaultForBrowserContext(profile_);
  content::HostZoomMap::ZoomLevelVector zoom_levels(
      host_zoom_map->GetAllZoomLevels());

  const auto* extension_registry = extensions::ExtensionRegistry::Get(profile_);

  // Sort ZoomLevelChanges by host and scheme
  // (a.com < http://a.com < https://a.com < b.com).
  std::sort(zoom_levels.begin(), zoom_levels.end(),
            [](const content::HostZoomMap::ZoomLevelChange& a,
               const content::HostZoomMap::ZoomLevelChange& b) {
              return a.host == b.host ? a.scheme < b.scheme : a.host < b.host;
            });
  GURL unreachable_web_data_url(content::kUnreachableWebDataURL);
  for (const auto& zoom_level : zoom_levels) {
    base::Value::Dict exception;
    switch (zoom_level.mode) {
      case content::HostZoomMap::ZOOM_CHANGED_FOR_HOST: {
        std::string host_or_spec = zoom_level.host;
        std::string origin_for_favicon = host_or_spec;
        std::string display_name = host_or_spec;

        if (host_or_spec == unreachable_web_data_url.host()) {
          display_name =
              l10n_util::GetStringUTF8(IDS_ZOOMLEVELS_CHROME_ERROR_PAGES_LABEL);
        }

        // As an optimization, only check hosts that could be an extension.
        if (crx_file::id_util::IdIsValid(host_or_spec)) {
          // Look up the host as an extension, if found then it is an extension.
          const extensions::Extension* extension =
              extension_registry->GetExtensionById(
                  host_or_spec, extensions::ExtensionRegistry::EVERYTHING);
          if (extension) {
            origin_for_favicon = extension->url().spec();
            display_name = extension->name();
          }
        }

        zoom_levels_exceptions.Append(
            CreateZoomLevelException(host_or_spec, origin_for_favicon,
                                     display_name, zoom_level.zoom_level));
        break;
      }
      case content::HostZoomMap::ZOOM_CHANGED_FOR_SCHEME_AND_HOST:
        // These are not stored in preferences and get cleared on next browser
        // start. Therefore, we don't care for them.
        continue;
      case content::HostZoomMap::ZOOM_CHANGED_TEMPORARY_ZOOM:
        NOTREACHED_IN_MIGRATION();
    }
  }

  FireWebUIListener("onZoomLevelsChanged", zoom_levels_exceptions);
}

void SiteSettingsHandler::HandleRemoveZoomLevel(const base::Value::List& args) {
  CHECK_EQ(1U, args.size());

  std::string host_or_spec = args[0].GetString();

  GURL url(host_or_spec);
  if (url.is_valid() && url.scheme() == chrome::kIsolatedAppScheme) {
    base::expected<web_app::IsolatedWebAppUrlInfo, std::string> iwa_url_info =
        web_app::IsolatedWebAppUrlInfo::Create(url);
    if (!iwa_url_info.has_value()) {
      return;
    }
    content::StoragePartition* iwa_storage_partition =
        profile_->GetStoragePartition(
            iwa_url_info->storage_partition_config(profile_));
    auto* host_zoom_map =
        content::HostZoomMap::GetForStoragePartition(iwa_storage_partition);
    double default_level = host_zoom_map->GetDefaultZoomLevel();
    host_zoom_map->SetZoomLevelForHost(url.host(), default_level);
    return;
  }

  content::HostZoomMap* host_zoom_map =
      content::HostZoomMap::GetDefaultForBrowserContext(profile_);
  double default_level = host_zoom_map->GetDefaultZoomLevel();
  host_zoom_map->SetZoomLevelForHost(host_or_spec, default_level);
}

void SiteSettingsHandler::HandleFetchBlockAutoplayStatus(
    const base::Value::List& args) {
  AllowJavascript();
  SendBlockAutoplayStatus();
}

void SiteSettingsHandler::SendBlockAutoplayStatus() {
  if (!IsJavascriptAllowed())
    return;

  base::Value::Dict status;

  // Whether the block autoplay toggle should be checked.
  base::Value::Dict pref;
  pref.Set("value",

           UnifiedAutoplayConfig::ShouldBlockAutoplay(profile_) &&
               UnifiedAutoplayConfig::IsBlockAutoplayUserModifiable(profile_));
  status.Set("pref", std::move(pref));

  // Whether the block autoplay toggle should be enabled.
  status.Set("enabled",
             UnifiedAutoplayConfig::IsBlockAutoplayUserModifiable(profile_));

  FireWebUIListener("onBlockAutoplayStatusChanged", status);
}

void SiteSettingsHandler::HandleSetBlockAutoplayEnabled(
    const base::Value::List& args) {
  AllowJavascript();

  if (!UnifiedAutoplayConfig::IsBlockAutoplayUserModifiable(profile_))
    return;

  CHECK_EQ(1U, args.size());
  CHECK(args[0].is_bool());
  bool value = args[0].GetBool();

  profile_->GetPrefs()->SetBoolean(prefs::kBlockAutoplayEnabled, value);
}

void SiteSettingsHandler::RebuildModel() {
  // The handler services two requests async once models have been built.
  DCHECK(update_site_details_ || send_sites_list_);

  // Tests will directly fire the appropriate service method.
  if (model_set_for_testing_) {
    return;
  }

  // Reset any existing models.
  // TODO(crbug.com/40240175) The implicit semantics of the handler require the
  // models to be reset every time, but this is not required for all operations.
  // A stronger call ordering enforcement, or stronger guarantees around when
  // the models exist, could remove the requirement for this.
  browsing_data_model_.reset();

  BrowsingDataModel::BuildFromDisk(
      profile_, ChromeBrowsingDataModelDelegate::CreateForProfile(profile_),
      base::BindOnce(&SiteSettingsHandler::BrowsingDataModelCreated,
                     weak_ptr_factory_.GetWeakPtr()));
}

void SiteSettingsHandler::ServicePendingRequests() {
  if (!IsJavascriptAllowed())
    return;

  if (send_sites_list_)
    OnStorageFetched();
  if (update_site_details_)
    OnGetUsageInfo();

  send_sites_list_ = false;
  update_site_details_ = false;
}

void SiteSettingsHandler::ObserveSourcesForProfile(Profile* profile) {
  auto* map = HostContentSettingsMapFactory::GetForProfile(profile);
  if (!observations_.IsObservingSource(map))
    observations_.AddObservation(map);

  auto* usb_context = UsbChooserContextFactory::GetForProfile(profile);
  if (!chooser_observations_.IsObservingSource(usb_context))
    chooser_observations_.AddObservation(usb_context);

  auto* serial_context = SerialChooserContextFactory::GetForProfile(profile);
  if (!chooser_observations_.IsObservingSource(serial_context))
    chooser_observations_.AddObservation(serial_context);

  auto* hid_context = HidChooserContextFactory::GetForProfile(profile);
  if (!chooser_observations_.IsObservingSource(hid_context))
    chooser_observations_.AddObservation(hid_context);

  if (base::FeatureList::IsEnabled(
          features::kWebBluetoothNewPermissionsBackend)) {
    auto* bluetooth_context =
        BluetoothChooserContextFactory::GetForProfile(profile);
    if (!chooser_observations_.IsObservingSource(bluetooth_context))
      chooser_observations_.AddObservation(bluetooth_context);
  }

  if (base::FeatureList::IsEnabled(
          features::kFileSystemAccessPersistentPermissions)) {
    auto* file_system_access_permission_context =
        FileSystemAccessPermissionContextFactory::GetForProfile(profile);
    if (!chooser_observations_.IsObservingSource(
            file_system_access_permission_context)) {
      chooser_observations_.AddObservation(
          file_system_access_permission_context);
    }
  }

#if BUILDFLAG(IS_CHROMEOS)
  if (base::FeatureList::IsEnabled(blink::features::kSmartCard)) {
    auto& smart_card_context =
        SmartCardPermissionContextFactory::GetForProfile(*profile);
    if (!chooser_observations_.IsObservingSource(&smart_card_context)) {
      chooser_observations_.AddObservation(&smart_card_context);
    }
  }
#endif
  observed_profiles_.AddObservation(profile);
}

void SiteSettingsHandler::StopObservingSourcesForProfile(Profile* profile) {
  auto* map = HostContentSettingsMapFactory::GetForProfile(profile);
  if (observations_.IsObservingSource(map))
    observations_.RemoveObservation(map);

  auto* usb_context = UsbChooserContextFactory::GetForProfile(profile);
  if (chooser_observations_.IsObservingSource(usb_context))
    chooser_observations_.RemoveObservation(usb_context);

  auto* serial_context = SerialChooserContextFactory::GetForProfile(profile);
  if (chooser_observations_.IsObservingSource(serial_context))
    chooser_observations_.RemoveObservation(serial_context);

  auto* hid_context = HidChooserContextFactory::GetForProfile(profile);
  if (chooser_observations_.IsObservingSource(hid_context))
    chooser_observations_.RemoveObservation(hid_context);

  if (base::FeatureList::IsEnabled(
          features::kWebBluetoothNewPermissionsBackend)) {
    auto* bluetooth_context =
        BluetoothChooserContextFactory::GetForProfile(profile);
    if (chooser_observations_.IsObservingSource(bluetooth_context))
      chooser_observations_.RemoveObservation(bluetooth_context);
  }

  if (base::FeatureList::IsEnabled(
          features::kFileSystemAccessPersistentPermissions)) {
    auto* file_system_access_permission_context =
        FileSystemAccessPermissionContextFactory::GetForProfile(profile);
    if (chooser_observations_.IsObservingSource(
            file_system_access_permission_context)) {
      chooser_observations_.RemoveObservation(
          file_system_access_permission_context);
    }
  }

  observed_profiles_.RemoveObservation(profile);
}

void SiteSettingsHandler::GetOriginStorage(
    AllSitesMap* all_sites_map,
    std::map<url::Origin, int64_t>* origin_size_map) {
  for (const auto& entry : *browsing_data_model_) {
    if (entry.data_details->storage_size == 0)
      continue;

    url::Origin origin =
        BrowsingDataModel::GetOriginForDataKey(entry.data_key.get());

    // If the storage is partitioned on a third party we need to ensure the
    // grouping key matches the top-site and doesn't default to the origin
    // in the UI.
    std::optional<GroupingKey> partition_grouping_key = std::nullopt;
    auto third_party_partitioning_site = entry.GetThirdPartyPartitioningSite();
    if (third_party_partitioning_site) {
      partition_grouping_key = GroupingKey::Create(url::Origin::Create(
          GURL(third_party_partitioning_site->Serialize())));
    }
    UpdateDataFromModel(all_sites_map, origin_size_map, origin,
                        entry.data_details->storage_size,
                        partition_grouping_key);
  }
}

void SiteSettingsHandler::GetHostCookies(
    AllSitesMap* all_sites_map,
    std::map<std::pair<std::string, std::optional<std::string>>, int>*
        host_cookie_map) {
  for (const auto& [owner, key, details] : *browsing_data_model_) {
    const net::CanonicalCookie* cookie =
        absl::get_if<net::CanonicalCookie>(&key.get());
    // Skip data keys that don't have cookies.
    if (!cookie) {
      continue;
    }
    std::optional<std::string> partition_etld_plus1 = std::nullopt;
    std::optional<GroupingKey> partition_grouping_key = std::nullopt;
    if (cookie->IsPartitioned()) {
      partition_etld_plus1 = cookie->PartitionKey()->site().GetURL().host();
      partition_grouping_key =
          GroupingKey::CreateFromEtldPlus1(*partition_etld_plus1);
    }

    const auto owner_host = BrowsingDataModel::GetHost(owner.get());
    const auto origin = BrowsingDataModel::GetOriginForDataKey(*cookie);
    InsertOriginIntoGroup(all_sites_map, origin,
                          /*is_origin_with_cookies=*/true,
                          partition_grouping_key);
    (*host_cookie_map)[{owner_host, partition_etld_plus1}]++;
  }
}

void SiteSettingsHandler::HandleClearSiteGroupDataAndCookies(
    const base::Value::List& args) {
  CHECK_EQ(1U, args.size());
  auto grouping_key = GroupingKey::Deserialize(args[0].GetString());
  net::SchemefulSite https_top_level_site;
  net::SchemefulSite http_top_level_site;
  if (std::optional<std::string> etld_plus_one =
          grouping_key.GetEtldPlusOne()) {
    https_top_level_site =
        net::SchemefulSite(ConvertEtldToOrigin(*etld_plus_one, true));
    http_top_level_site =
        net::SchemefulSite(ConvertEtldToOrigin(*etld_plus_one, false));
  } else if (std::optional<url::Origin> origin = grouping_key.GetOrigin()) {
    https_top_level_site = net::SchemefulSite(*origin);
    http_top_level_site = net::SchemefulSite(*origin);
  }

  AllowJavascript();
  // Retrieve all of the origin entries grouped under this group.
  std::vector<url::Origin> affected_origins;
  for (const auto& origin_is_partitioned : all_sites_map_[grouping_key]) {
    if (origin_is_partitioned.second) {
      // Ensure that if the entry is partitioned, the partitioning site has
      // been set.
      CHECK(https_top_level_site.GetURL().is_valid());
      CHECK(http_top_level_site.GetURL().is_valid());

      // Partitioned entries must be removed from the browsing data model.
      if (browsing_data_model_) {
        browsing_data_model_->RemovePartitionedBrowsingData(
            origin_is_partitioned.first.host(), https_top_level_site,
            base::DoNothing());
        browsing_data_model_->RemovePartitionedBrowsingData(
            origin_is_partitioned.first.host(), http_top_level_site,
            base::DoNothing());
      }
    } else {
      affected_origins.emplace_back(
          // A placeholder origin may have been created, in this case the
          // grouping key itself should be used as the origin, the same as it
          // would have been for display.
          ResolveOriginInSiteGroup(grouping_key, origin_is_partitioned.first));
    }
  }

  // Cookies may have associated with the entry for the grouping url itself.
  // As per the logic in InsertOriginIntoGroup, this will only occur
  // if the existing entry was https, otherwise a new http entry would be
  // created for the placeholder. Hence, we need only additionally include the
  // HTTPS version of the eTLD+1 as an origin.
  if (auto etld_plus1 = grouping_key.GetEtldPlusOne(); etld_plus1.has_value()) {
    affected_origins.emplace_back(
        ConvertEtldToOrigin(*etld_plus1, /*secure=*/true));
  }

  if (browsing_data_model_) {
    for (const auto& origin : affected_origins) {
      if (origin.GetURL().SchemeIsHTTPOrHTTPS()) {
        browsing_data_model_->RemoveUnpartitionedBrowsingData(
            origin.host(), base::DoNothing());
      } else {
        browsing_data_model_->RemoveUnpartitionedBrowsingData(
            origin, base::DoNothing());
      }
    }
  }

  RemoveNonModelData(affected_origins);
}

void SiteSettingsHandler::HandleRecordAction(const base::Value::List& args) {
  const auto& list = args;
  CHECK_EQ(1U, list.size());
  int action = list[0].GetInt();
  DCHECK_LE(action, static_cast<int>(AllSitesAction2::kMaxValue));
  DCHECK_GE(action, static_cast<int>(AllSitesAction2::kLoadPage));

  LogAllSitesAction(static_cast<AllSitesAction2>(action));
}

void SiteSettingsHandler::HandleGetNumCookiesString(
    const base::Value::List& args) {
  CHECK_EQ(2U, args.size());
  const std::string callback_id = args[0].GetString();
  int num_cookies = args[1].GetInt();

  AllowJavascript();
  const std::u16string string =
      num_cookies > 0 ? l10n_util::GetPluralStringFUTF16(
                            IDS_SETTINGS_SITE_SETTINGS_NUM_COOKIES, num_cookies)
                      : std::u16string();

  ResolveJavascriptCallback(base::Value(callback_id), base::Value(string));
}

void SiteSettingsHandler::HandleGetSystemDeniedPermissions(
    const base::Value::List& args) {
  CHECK_EQ(1U, args.size());
  const std::string callback_id = args[0].GetString();

  AllowJavascript();
  ResolveJavascriptCallback(base::Value(callback_id),
                            GetSystemDeniedPermissions());
}

void SiteSettingsHandler::HandleOpenSystemPermissionSettings(
    const base::Value::List& args) {
  CHECK_EQ(1U, args.size());
  const ContentSettingsType permission_type =
      site_settings::ContentSettingsTypeFromGroupName(args[0].GetString());

  content::WebContents* web_contents = CHECK_DEREF(web_ui()).GetWebContents();
  system_permission_settings::OpenSystemSettings(web_contents, permission_type);
}

void SiteSettingsHandler::RemoveNonModelData(
    const std::vector<url::Origin>& origins) {
  if (origins.empty()) {
    return;
  }

  // TODO(crbug.com/40204789): Remove client hint information, which cannot be
  // associated with Cookie node information as the scheme in the cookie node
  // may not match due to HTTP / HTTPS distinction issues.
  for (const auto& origin : origins) {
    HostContentSettingsMapFactory::GetForProfile(profile_)
        ->SetWebsiteSettingDefaultScope(origin.GetURL(), GURL(),
                                        ContentSettingsType::CLIENT_HINTS,
                                        base::Value());
    // Once user clears site setting data for `origins`, all corresponding
    // reduced accept language stored in the setting map should also be cleaned.
    HostContentSettingsMapFactory::GetForProfile(profile_)
        ->SetWebsiteSettingDefaultScope(
            origin.GetURL(), GURL(),
            ContentSettingsType::REDUCED_ACCEPT_LANGUAGE, base::Value());
    // Once user clears site setting data for `origins`, the Durable storage bit
    // should also be reset.
    // TODO(crbug.com/40287777): This should be replaced when integrated with
    // the BrowserDataModel.
    HostContentSettingsMapFactory::GetForProfile(profile_)
        ->SetWebsiteSettingDefaultScope(origin.GetURL(), GURL(),
                                        ContentSettingsType::DURABLE_STORAGE,
                                        base::Value());
  }

#if BUILDFLAG(IS_WIN)
  // Removes any Media License Data associated with the origin that is not
  // stored in quota nodes. This should only be on Windows as ChromeOS does
  // not support removing Media License Data per origin, and
  // site_settings_handler.cc does not handle Android site specific code.
  // The code for Android site specific code is located in
  // components/browser_ui/site_settings/android/website_preference_bridge.cc
  // TODO(b/248311157) - When CrOS supports the ability to delete platform
  // keys by domain, implement the CrOS specific logic regarding clearing site
  // specific media license data.
  // TODO(b/248311157) - When the migration to BrowsingDataModel is finished,
  // remove this and integrate the media license data removal steps there.
  auto filter_builder = content::BrowsingDataFilterBuilder::Create(
      content::BrowsingDataFilterBuilder::Mode::kDelete);

  for (const auto& origin : origins)
    filter_builder->AddOrigin(origin);

  CdmDocumentServiceImpl::ClearCdmData(
      profile_, base::Time::Min(), base::Time::Max(),
      filter_builder->BuildUrlFilter(), base::DoNothing());
#endif  // BUILDFLAG(IS_WIN)
}

void SiteSettingsHandler::SetModelForTesting(
    std::unique_ptr<BrowsingDataModel> browsing_data_model) {
  browsing_data_model_ = std::move(browsing_data_model);
  model_set_for_testing_ = true;
}

void SiteSettingsHandler::ClearAllSitesMapForTesting() {
  all_sites_map_.clear();
}

BrowsingDataModel* SiteSettingsHandler::GetBrowsingDataModelForTesting() {
  return browsing_data_model_.get();
}

// Dictionary keys for an individual `FileSystemPermissionGrant`.
// Schema (per grant):
// {
//     "origin" : <string>;
//     "filePath" : <string>;
//     "displayName" : <string>;
//     "isDirectory" : <bool>;
// }

// Dictionary keys for an individual permission grant in
// the returned `grants` List.
// Schema (per origin):
// [
//  ...
//   {
//     "origin" : <string>;
//     "viewGrants" : FileSystemPermissionGrant[];
//     "editGrants" : FileSystemPermissionGrant[];
//   }
//  ...
// ]
base::Value::List SiteSettingsHandler::PopulateFileSystemGrantData() {
  base::Value::List grants;

  if (!base::FeatureList::IsEnabled(
          features::kFileSystemAccessPersistentPermissions)) {
    return grants;
  }

  ChromeFileSystemAccessPermissionContext* permission_context =
      FileSystemAccessPermissionContextFactory::GetForProfile(profile_);
  std::set<url::Origin> origins_with_grants =
      permission_context->GetOriginsWithGrants();

  for (auto& origin : origins_with_grants) {
    ChromeFileSystemAccessPermissionContext::Grants grantObj =
        permission_context->ConvertObjectsToGrants(
            permission_context->GetGrantedObjects(origin));
    if (grantObj.file_read_grants.empty() &&
        grantObj.file_write_grants.empty() &&
        grantObj.directory_read_grants.empty() &&
        grantObj.directory_write_grants.empty()) {
      continue;
    }

    base::Value::Dict origin_file_system_permission_grants;
    base::Value::List view_grants;
    base::Value::List edit_grants;
    std::vector<std::string> directory_edit_grants_file_paths;
    std::vector<std::string> file_edit_grants_file_paths;

    std::string origin_string = origin.GetURL().spec();
    origin_file_system_permission_grants.Set(site_settings::kOrigin,
                                             origin_string);

    // Populate the `file_system_permission_grant` object with allowed
    // permissions.
    for (auto& file_path : grantObj.directory_write_grants) {
      base::Value::Dict directory_write_grant;
      const std::string file_path_string =
          FilePathToValue(file_path).GetString();
      directory_write_grant.Set(site_settings::kOrigin, origin_string);
      directory_write_grant.Set(site_settings::kFileSystemFilePath,
                                file_path_string);
      directory_write_grant.Set(site_settings::kDisplayName, file_path_string);
      directory_write_grant.Set(site_settings::kFileSystemIsDirectory, true);
      directory_edit_grants_file_paths.push_back(file_path_string);
      edit_grants.Append(std::move(directory_write_grant));
    }

    for (auto& file_path : grantObj.directory_read_grants) {
      const std::string file_path_string =
          FilePathToValue(file_path).GetString();
      if (base::Contains(directory_edit_grants_file_paths, file_path_string)) {
        continue;
      }
      base::Value::Dict directory_read_grant;
      directory_read_grant.Set(site_settings::kOrigin, origin_string);
      directory_read_grant.Set(site_settings::kFileSystemFilePath,
                               file_path_string);
      directory_read_grant.Set(site_settings::kDisplayName, file_path_string);
      directory_read_grant.Set(site_settings::kFileSystemIsDirectory, true);
      view_grants.Append(std::move(directory_read_grant));
    }

    for (auto& file_path : grantObj.file_write_grants) {
      base::Value::Dict file_write_grant;
      const std::string file_path_string =
          FilePathToValue(file_path).GetString();
      file_write_grant.Set(site_settings::kOrigin, origin_string);
      file_write_grant.Set(site_settings::kFileSystemFilePath,
                           file_path_string);
      file_write_grant.Set(site_settings::kDisplayName, file_path_string);
      file_write_grant.Set(site_settings::kFileSystemIsDirectory, false);
      file_edit_grants_file_paths.push_back(file_path_string);
      edit_grants.Append(std::move(file_write_grant));
    }

    for (auto& file_path : grantObj.file_read_grants) {
      const std::string file_path_string =
          FilePathToValue(file_path).GetString();
      if (base::Contains(file_edit_grants_file_paths, file_path_string)) {
        continue;
      }
      base::Value::Dict file_read_grant;
      file_read_grant.Set(site_settings::kOrigin, origin_string);
      file_read_grant.Set(site_settings::kFileSystemFilePath, file_path_string);
      file_read_grant.Set(site_settings::kDisplayName, file_path_string);
      file_read_grant.Set(site_settings::kFileSystemIsDirectory, false);
      view_grants.Append((base::Value(std::move(file_read_grant))));
    }
    origin_file_system_permission_grants.Set("viewGrants",
                                             std::move(view_grants));
    origin_file_system_permission_grants.Set("editGrants",
                                             std::move(edit_grants));
    grants.Append(std::move(origin_file_system_permission_grants));
  }
  return grants;
}

void SiteSettingsHandler::SendNotificationPermissionReviewList() {
  NotificationPermissionsReviewService* service =
      NotificationPermissionsReviewServiceFactory::GetForProfile(profile_);
  CHECK(service);
  // Notify observers that the permission review list could have changed. Note
  // that the list is not guaranteed to have changed. In places where
  // determining whether the list has changed is cause for performance concerns,
  // an unchanged list may be sent. This is the case for
  // HandleResetCategoryPermissionForPattern and
  // HandleSetCategoryPermissionForPattern.
  FireWebUIListener(
      site_settings::kNotificationPermissionsReviewListMaybeChangedEvent,
      service->PopulateNotificationPermissionReviewData());
}

base::Value SiteSettingsHandler::GetSystemDeniedPermissions() {
  base::Value::List blocked_permissions;

#if BUILDFLAG(IS_CHROMEOS)
  // This is used to display warning messages in the UI in case that
  // geolocation, microphone or camera are disabled at the system level. At the
  // moment this functionality is only targeting CrOS.
  if (system_permission_settings::IsDenied(
          ContentSettingsType::MEDIASTREAM_CAMERA)) {
    blocked_permissions.Append(site_settings::ContentSettingsTypeToGroupName(
        ContentSettingsType::MEDIASTREAM_CAMERA));
  }
  if (system_permission_settings::IsDenied(
          ContentSettingsType::MEDIASTREAM_MIC)) {
    blocked_permissions.Append(site_settings::ContentSettingsTypeToGroupName(
        ContentSettingsType::MEDIASTREAM_MIC));
  }
  if (system_permission_settings::IsDenied(ContentSettingsType::GEOLOCATION)) {
    blocked_permissions.Append(site_settings::ContentSettingsTypeToGroupName(
        ContentSettingsType::GEOLOCATION));
  }
#endif

  return base::Value(std::move(blocked_permissions));
}

}  // namespace settings
