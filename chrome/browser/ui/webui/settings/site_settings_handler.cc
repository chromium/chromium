// Copyright 2016 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/webui/settings/site_settings_handler.h"

#include <set>
#include <utility>
#include <vector>

#include "base/barrier_closure.h"
#include "base/containers/contains.h"
#include "base/containers/flat_set.h"
#include "base/feature_list.h"
#include "base/functional/bind.h"
#include "base/functional/callback_helpers.h"
#include "base/i18n/message_formatter.h"
#include "base/i18n/number_formatting.h"
#include "base/json/values_util.h"
#include "base/metrics/histogram_functions.h"
#include "base/metrics/histogram_macros.h"
#include "base/metrics/user_metrics.h"
#include "base/ranges/algorithm.h"
#include "base/strings/utf_string_conversions.h"
#include "base/values.h"
#include "build/chromeos_buildflags.h"
#include "chrome/browser/apps/app_service/app_service_proxy.h"
#include "chrome/browser/apps/app_service/app_service_proxy_factory.h"
#include "chrome/browser/bluetooth/bluetooth_chooser_context_factory.h"
#include "chrome/browser/browsing_data/access_context_audit_service_factory.h"
#include "chrome/browser/browsing_data/cookies_tree_model.h"
#include "chrome/browser/browsing_topics/browsing_topics_service_factory.h"
#include "chrome/browser/content_settings/chrome_content_settings_utils.h"
#include "chrome/browser/content_settings/host_content_settings_map_factory.h"
#include "chrome/browser/file_system_access/chrome_file_system_access_permission_context.h"
#include "chrome/browser/file_system_access/file_system_access_permission_context_factory.h"
#include "chrome/browser/hid/hid_chooser_context.h"
#include "chrome/browser/hid/hid_chooser_context_factory.h"
#include "chrome/browser/media/unified_autoplay_config.h"
#include "chrome/browser/permissions/notification_permission_review_service_factory.h"
#include "chrome/browser/permissions/permission_decision_auto_blocker_factory.h"
#include "chrome/browser/privacy_sandbox/privacy_sandbox_service.h"
#include "chrome/browser/privacy_sandbox/privacy_sandbox_service_factory.h"
#include "chrome/browser/serial/serial_chooser_context.h"
#include "chrome/browser/serial/serial_chooser_context_factory.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/browser/ui/browser_list.h"
#include "chrome/browser/ui/page_info/page_info_infobar_delegate.h"
#include "chrome/browser/ui/tabs/tab_strip_model.h"
#include "chrome/browser/ui/webui/settings/recent_site_settings_helper.h"
#include "chrome/browser/ui/webui/settings/site_settings_helper.h"
#include "chrome/browser/usb/usb_chooser_context.h"
#include "chrome/browser/usb/usb_chooser_context_factory.h"
#include "chrome/common/chrome_features.h"
#include "chrome/common/extensions/manifest_handlers/app_launch_info.h"
#include "chrome/common/pref_names.h"
#include "chrome/grit/generated_resources.h"
#include "components/browsing_data/content/browsing_data_model.h"
#include "components/browsing_topics/browsing_topics_service.h"
#include "components/content_settings/core/browser/cookie_settings.h"
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
#include "components/services/app_service/public/cpp/app_registry_cache.h"
#include "components/services/app_service/public/cpp/app_types.h"
#include "components/services/app_service/public/cpp/app_update.h"
#include "components/site_engagement/content/site_engagement_service.h"
#include "content/public/browser/browser_thread.h"
#include "content/public/browser/browsing_data_filter_builder.h"
#include "content/public/browser/browsing_data_remover.h"
#include "content/public/browser/storage_partition.h"
#include "content/public/browser/web_contents.h"
#include "content/public/browser/web_ui.h"
#include "content/public/common/content_features.h"
#include "content/public/common/url_constants.h"
#include "extensions/browser/extension_registry.h"
#include "extensions/common/permissions/api_permission.h"
#include "extensions/common/permissions/permissions_data.h"
#include "services/network/public/cpp/is_potentially_trustworthy.h"
#include "storage/common/file_system/file_system_util.h"
#include "third_party/blink/public/common/page/page_zoom.h"
#include "ui/base/l10n/l10n_util.h"
#include "ui/base/text/bytes_formatting.h"
#include "url/gurl.h"
#include "url/origin.h"

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

// Keys of the dictionary returned by HandleIsPatternValidForType.
constexpr char kIsValidKey[] = "isValid";
constexpr char kReasonKey[] = "reason";

constexpr char kEffectiveTopLevelDomainPlus1Name[] = "etldPlus1";
constexpr char kOriginList[] = "origins";
constexpr char kNumCookies[] = "numCookies";
constexpr char kHasPermissionSettings[] = "hasPermissionSettings";
constexpr char kHasInstalledPWA[] = "hasInstalledPWA";
constexpr char kIsInstalled[] = "isInstalled";
constexpr char kFpsOwner[] = "fpsOwner";
constexpr char kFpsNumMembers[] = "fpsNumMembers";
constexpr char kFpsEnterpriseManaged[] = "fpsEnterpriseManaged";
constexpr char kZoom[] = "zoom";
// Placeholder value for ETLD+1 until a valid origin is added. If an ETLD+1
// only has placeholder, then create an ETLD+1 origin.
constexpr char kPlaceholder[] = "placeholder";

// Content types for chooser data.
constexpr ContentSettingsType kChooserDataContentSettingsTypes[] = {
    ContentSettingsType::BLUETOOTH_CHOOSER_DATA,
    ContentSettingsType::HID_CHOOSER_DATA,
    ContentSettingsType::SERIAL_CHOOSER_DATA,
    ContentSettingsType::USB_CHOOSER_DATA,
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

base::flat_set<std::string> GetInstalledAppOrigins(Profile* profile) {
  if (!apps::AppServiceProxyFactory::IsAppServiceAvailableForProfile(profile))
    return base::flat_set<std::string>();

  std::vector<std::string> origins;
  apps::AppServiceProxyFactory::GetForProfile(profile)
      ->AppRegistryCache()
      .ForEachApp([&origins](const apps::AppUpdate& update) {
        if (update.AppType() == apps::AppType::kWeb ||
            update.AppType() == apps::AppType::kSystemWeb) {
          // For web apps, |PublisherId()| is set to the start URL.
          const GURL start_url(update.PublisherId());
          DCHECK(start_url.is_valid());
          origins.push_back(start_url.DeprecatedGetOriginAsURL().spec());
        }
      });
  return base::flat_set<std::string>(std::move(origins));
}

// Groups |url| into sets of eTLD+1s in |site_group_map|, assuming |url| is an
// origin. The effective eTLD+1 is |partition_etld_plus1| is set, otherwise it
// is the eTLD+1 of |url|.
// There are three cases:
// 1. The effective eLTD+1 is not yet in |site_group_map|. We add the ETLD+1
//    to |site_group_map|. If the |url| is an effective ETLD+1 cookie origin,
//    put a placeholder origin for the ETLD+1.
// 2. The effective ETLD+1 is in |site_group_map|, and is equal to host of
//    |url|. This means case 1 has already happened and nothing more needs to
//    be done.
// 3. The effective ETLD+1 is in |site_group_map| and is different to host of
//    |url|. For a cookies url, if a https origin with same host and partitioned
//    status exists, nothing more needs to be done.
// In case 3, we try to add |url| to the set of origins for the ETLD+1. If an
// existing origin is a placeholder, delete it, because the placeholder is no
// longer needed.
void CreateOrAppendSiteGroupEntry(
    std::map<std::string, std::set<std::pair<std::string, bool>>>*
        site_group_map,
    const GURL& url,
    bool url_is_origin_with_cookies = false,
    absl::optional<std::string> partition_etld_plus1 = absl::nullopt) {
#if BUILDFLAG(ENABLE_EXTENSIONS)
  // As extension doesn't have ETLD+1, handle it in a different logic.
  if (url.SchemeIs(extensions::kExtensionScheme)) {
    // |url| for an extension should always be in the format of
    // "chrome-extension://<extension_id>" and it will be a single origin site
    // group. So insert single origin site group to |site_group_map| if it
    // doesn't exist.
    if (site_group_map->find(url.spec()) == site_group_map->end()) {
      site_group_map->emplace(url.spec(),
                              std::set<std::pair<std::string, bool>>(
                                  {{url.spec(), /*is_partitioned=*/false}}));
    }
    return;
  }
#endif

  bool is_partitioned = partition_etld_plus1.has_value();
  std::string effective_etld_plus1_string =
      is_partitioned
          ? partition_etld_plus1.value()
          : net::registry_controlled_domains::GetDomainAndRegistry(
                url,
                net::registry_controlled_domains::INCLUDE_PRIVATE_REGISTRIES);
  auto entry = site_group_map->find(effective_etld_plus1_string);
  bool etld_plus1_cookie_url =
      url_is_origin_with_cookies && url.host() == effective_etld_plus1_string;

  if (entry == site_group_map->end()) {
    // Case 1:
    std::string origin = etld_plus1_cookie_url ? kPlaceholder : url.spec();
    site_group_map->emplace(
        effective_etld_plus1_string,
        std::set<std::pair<std::string, bool>>({{origin, is_partitioned}}));
    return;
  }
  // Case 2:
  if (etld_plus1_cookie_url && !is_partitioned) {
    return;
  }
  // Case 3:
  if (url_is_origin_with_cookies) {
    // Cookies ignore schemes, so try and see if a https schemed version
    // already exists in the origin list, if not, then add the http schemed
    // version into the map.
    std::string https_url = std::string(url::kHttpsScheme) +
                            url::kStandardSchemeSeparator + url.host() + "/";
    if (entry->second.find({https_url, is_partitioned}) !=
        entry->second.end()) {
      return;
    }
  }
  entry->second.insert({url.spec(), is_partitioned});
  auto placeholder = entry->second.find({kPlaceholder, is_partitioned});
  if (placeholder != entry->second.end())
    entry->second.erase(placeholder);
}

// Update the storage data in |origin_size_map|.
void UpdateDataForOrigin(const GURL& url,
                         const int64_t size,
                         std::map<std::string, int64_t>* origin_size_map) {
  if (size > 0)
    (*origin_size_map)[url.spec()] += size;
}

// Converts |etld_plus1| into an origin representation by adding HTTP scheme.
std::string ConvertEtldToOrigin(const std::string etld_plus1, bool secure) {
  return std::string(secure ? url::kHttpsScheme : url::kHttpScheme) +
         url::kStandardSchemeSeparator + etld_plus1 + "/";
}

bool IsPatternValidForType(const std::string& pattern_string,
                           const std::string& type,
                           Profile* profile,
                           std::string* out_error) {
  ContentSettingsType content_type =
      site_settings::ContentSettingsTypeFromGroupName(type);

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
    std::map<std::string, std::set<std::pair<std::string, bool>>>*
        all_sites_map,
    std::map<std::string, int64_t>* origin_size_map,
    const GURL& origin,
    int64_t size) {
  UpdateDataForOrigin(origin, size, origin_size_map);
  CreateOrAppendSiteGroupEntry(all_sites_map, origin);
}

void LogAllSitesAction(AllSitesAction2 action) {
  UMA_HISTOGRAM_ENUMERATION("WebsiteSettings.AllSitesAction2", action);
}

int GetNumCookieExceptionsOfTypes(HostContentSettingsMap* map,
                                  const std::set<ContentSetting> types) {
  ContentSettingsForOneType output;
  map->GetSettingsForOneType(ContentSettingsType::COOKIES, &output);
  return base::ranges::count_if(
      output, [types](const ContentSettingPatternSource setting) {
        return types.count(
            content_settings::ValueToContentSetting(setting.setting_value));
      });
}

std::string GetCookieSettingDescription(Profile* profile) {
  HostContentSettingsMap* map =
      HostContentSettingsMapFactory::GetForProfile(profile);
  auto content_setting =
      map->GetDefaultContentSetting(ContentSettingsType::COOKIES, nullptr);

  auto control_mode = static_cast<content_settings::CookieControlsMode>(
      profile->GetPrefs()->GetInteger(prefs::kCookieControlsMode));

  // Determine what the effective cookie setting is. These conditions are not
  // mutually exclusive and rely on ordering.
  if (content_setting == ContentSetting::CONTENT_SETTING_BLOCK) {
    return l10n_util::GetPluralStringFUTF8(
        IDS_SETTINGS_SITE_SETTINGS_COOKIES_BLOCK,
        GetNumCookieExceptionsOfTypes(
            map, {ContentSetting::CONTENT_SETTING_ALLOW,
                  ContentSetting::CONTENT_SETTING_SESSION_ONLY}));
  }
  switch (control_mode) {
    case content_settings::CookieControlsMode::kBlockThirdParty:
      return l10n_util::GetStringUTF8(
          IDS_SETTINGS_SITE_SETTINGS_COOKIES_BLOCK_THIRD_PARTY);
    case content_settings::CookieControlsMode::kIncognitoOnly:
      return l10n_util::GetStringUTF8(
          IDS_SETTINGS_SITE_SETTINGS_COOKIES_BLOCK_THIRD_PARTY_INCOGNITO);
    case content_settings::CookieControlsMode::kOff:
      // We do not make a distinction between allow and clear on exit.
      return l10n_util::GetPluralStringFUTF8(
          IDS_SETTINGS_SITE_SETTINGS_COOKIES_ALLOW,
          GetNumCookieExceptionsOfTypes(
              map, {ContentSetting::CONTENT_SETTING_BLOCK}));
  }
  NOTREACHED();
}

// Removes all nodes from |model| which match |origin| and |etld_plus1|. At
// least one of |origin| or |etld_plus1| must be set. If only |origin| is set,
// then unpartitioned storage for that origin is removed. If only |etld_plus1|
// is set, then any unpartitioned storage which matches that etld + 1, or
// partitioned storage where it is the partitioning site, is removed. If both
// |origin| and |etld_plus1| is set, then only storage for |origin| partitioned
// by |etld_plus1| is removed.
void RemoveMatchingNodes(CookiesTreeModel* model,
                         absl::optional<std::string> origin,
                         absl::optional<std::string> etld_plus1) {
  DCHECK(origin || etld_plus1);
  std::vector<CookieTreeNode*> nodes_to_delete;

  for (const auto& host_node : model->GetRoot()->children()) {
    bool origin_matches =
        origin &&
        *origin == host_node->GetDetailedInfo().origin.GetURL().spec();

    if (origin && !origin_matches) {
      // If the origin is set, host nodes which do not match that origin cannot
      // contain storage targeted for removal.
      continue;
    }

    std::string host_node_etld_plus1 =
        net::registry_controlled_domains::GetDomainAndRegistry(
            base::UTF16ToUTF8(host_node->GetTitle()),
            net::registry_controlled_domains::INCLUDE_PRIVATE_REGISTRIES);

    bool etld_plus1_matches = etld_plus1 && *etld_plus1 == host_node_etld_plus1;

    for (const auto& storage_type_node : host_node->children()) {
      if (storage_type_node->GetDetailedInfo().node_type !=
          CookieTreeNode::DetailedInfo::TYPE_COOKIES) {
        // Non cookie storage cannot (currently) be partitioned.
        if (origin && etld_plus1) {
          continue;
        }

        if (origin_matches || etld_plus1_matches) {
          nodes_to_delete.push_back(storage_type_node.get());
          continue;
        }
      } else {
        // Every cookie must be inspected to confirm partition state.
        // TODO(crbug.com/1271155): This is slow, and should be addressed when
        // the CookiesTreeModel is deprecated.
        for (const auto& cookie_node : storage_type_node->children()) {
          const auto& cookie = cookie_node->GetDetailedInfo().cookie;
          if (!cookie->IsPartitioned() &&
              (origin_matches || etld_plus1_matches) &&
              (!origin || !etld_plus1)) {
            nodes_to_delete.push_back(cookie_node.get());
            continue;
          }
          if (cookie->IsPartitioned()) {
            const auto& partition_site =
                cookie->PartitionKey()->site().GetURL().host();

            // If an origin has been set, it must match the origin of the
            // current node, which means it can be ignored.
            DCHECK(!origin || origin_matches);

            if (etld_plus1 && partition_site == *etld_plus1) {
              nodes_to_delete.push_back(cookie_node.get());
            }
          }
        }
      }
    }
  }

  for (auto* node : nodes_to_delete)
    model->DeleteCookieNode(node);
}

// Returns the registable domain (eTLD+1) for the `origin`. If it doesn't exist,
// returns the host.
std::string GetEtldPlusOne(const url::Origin& origin) {
  auto eltd_plus_one = net::registry_controlled_domains::GetDomainAndRegistry(
      origin, net::registry_controlled_domains::INCLUDE_PRIVATE_REGISTRIES);
  return eltd_plus_one.empty() ? origin.host() : eltd_plus_one;
}

// Converts |etld_plus1| into an HTTPS SchemefulSite.
net::SchemefulSite ConvertEtldToSchemefulSite(const std::string etld_plus1) {
  return net::SchemefulSite(GURL(std::string(url::kHttpsScheme) +
                                 url::kStandardSchemeSeparator + etld_plus1 +
                                 "/"));
}

// Iterates over host nodes in `tree_model` which contains all sites that have
// storage set and uses them to retrieve first party set membership information.
// Returns a map of site eTLD+1 matched with their FPS owner and count of first
// party set members.
std::map<std::string, std::pair<std::string, int>> GetFpsMap(
    PrivacySandboxService* privacy_sandbox_service,
    CookiesTreeModel* tree_model) {
  // Used to count unique eTLD+1 owned by a FPS owner.
  std::map<std::string, std::set<std::string>> fps_owner_to_members;

  // Count members by unique eTLD+1 for each first party set.
  for (const auto& host_node : tree_model->GetRoot()->children()) {
    std::string etld_plus1 =
        GetEtldPlusOne(host_node->GetDetailedInfo().origin);
    auto schemeful_site = ConvertEtldToSchemefulSite(etld_plus1);
    auto fps_owner =
        privacy_sandbox_service->GetFirstPartySetOwner(schemeful_site.GetURL());
    if (fps_owner.has_value()) {
      fps_owner_to_members[fps_owner->GetURL().host()].insert(etld_plus1);
    }
  }

  // site eTLD+1 : {owner site eTLD+1, # of sites in that first party set}
  std::map<std::string, std::pair<std::string, int>> fps_map;
  for (auto fps : fps_owner_to_members) {
    // Set fps owner and count of members for each eTLD+1
    for (auto member : fps.second) {
      fps_map[member] = {fps.first, fps.second.size()};
    }
  }

  return fps_map;
}

// Converts a given |site_group_map| to a list of base::Value::Dicts, adding
// the site engagement score for each origin.
void ConvertSiteGroupMapToList(
    const std::map<std::string, std::set<std::pair<std::string, bool>>>&
        site_group_map,
    const std::set<std::string>& origin_permission_set,
    base::Value::List* list_value,
    Profile* profile,
    CookiesTreeModel* tree_model) {
  DCHECK(profile);
  auto* privacy_sandbox_service =
      PrivacySandboxServiceFactory::GetForProfile(profile);
  auto fps_map = GetFpsMap(privacy_sandbox_service, tree_model);
  base::flat_set<std::string> installed_origins =
      GetInstalledAppOrigins(profile);
  site_engagement::SiteEngagementService* engagement_service =
      site_engagement::SiteEngagementService::Get(profile);
  for (const auto& entry : site_group_map) {
    // eTLD+1 is the effective top level domain + 1.
    base::Value::Dict site_group;
    site_group.Set(kEffectiveTopLevelDomainPlus1Name, entry.first);

    // Isolated Web Apps or extension do not support sub domains, so the origins
    // set always contains only 1 entry.
    const GURL primary_origin_url(entry.second.begin()->first);
    absl::optional<std::string> isolated_web_app_name =
        site_settings::GetIsolatedWebAppName(profile, primary_origin_url);
    if (isolated_web_app_name.has_value()) {
      site_group.Set(site_settings::kIsolatedWebAppName,
                     isolated_web_app_name.value());
    }

    absl::optional<std::string> extension_name =
        site_settings::GetExtensionDisplayName(profile, primary_origin_url);
    if (extension_name.has_value() && !extension_name.value().empty()) {
      site_group.Set(site_settings::kExtensionName, extension_name.value());
    }

    bool has_installed_pwa = false;
    base::Value::List origin_list;
    for (const auto& origin_is_partitioned : entry.second) {
      const auto& origin = origin_is_partitioned.first;
      bool is_partitioned = origin_is_partitioned.second;
      base::Value::Dict origin_object;
      // If origin is placeholder, create a http ETLD+1 origin for it.
      if (origin == kPlaceholder) {
        origin_object.Set("origin",
                          ConvertEtldToOrigin(entry.first, /*secure=*/false));
      } else {
        origin_object.Set("origin", origin);
      }
      origin_object.Set("isPartitioned", is_partitioned);
      origin_object.Set("engagement",
                        engagement_service->GetScore(GURL(origin)));
      origin_object.Set("usage", 0);
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
    if (fps_map.count(entry.first)) {
      site_group.Set(kFpsOwner, fps_map[entry.first].first);
      site_group.Set(kFpsNumMembers, fps_map[entry.first].second);
      auto schemeful_site = ConvertEtldToSchemefulSite(entry.first);
      site_group.Set(kFpsEnterpriseManaged,
                     privacy_sandbox_service->IsPartOfManagedFirstPartySet(
                         schemeful_site));
    }
    list_value->Append(std::move(site_group));
  }
}

bool ShouldAddToNotificationPermissionReviewList(
    site_engagement::SiteEngagementService* service,
    GURL url,
    int notification_count) {
  // The notification permission should be added to the list if one of the
  // criteria below holds:
  // - Site engagement level is NONE OR MINIMAL and average daily notification
  // count is more than 0.
  // - Site engamment level is LOW and average daily notification count is
  // more than 3. Otherwise, the notification permission should not be added
  // to review list.
  double score = service->GetScore(url);
  int low_engagement_notification_limit =
      features::kSafetyCheckNotificationPermissionsLowEnagementLimit.Get();
  bool is_low_engagement =
      !site_engagement::SiteEngagementService::IsEngagementAtLeast(
          score, blink::mojom::EngagementLevel::MEDIUM) &&
      notification_count > low_engagement_notification_limit;
  int min_engagement_notification_limit =
      features::kSafetyCheckNotificationPermissionsMinEnagementLimit.Get();
  bool is_minimal_engagement =
      !site_engagement::SiteEngagementService::IsEngagementAtLeast(
          score, blink::mojom::EngagementLevel::LOW) &&
      notification_count > min_engagement_notification_limit;

  return is_minimal_engagement || is_low_engagement;
}

}  // namespace

SiteSettingsHandler::SiteSettingsHandler(Profile* profile)
    : profile_(profile) {}

SiteSettingsHandler::~SiteSettingsHandler() {
  if (cookies_tree_model_)
    cookies_tree_model_->RemoveCookiesTreeObserver(this);
}

void SiteSettingsHandler::RegisterMessages() {
  web_ui()->RegisterMessageCallback(
      "fetchUsageTotal",
      base::BindRepeating(&SiteSettingsHandler::HandleFetchUsageTotal,
                          base::Unretained(this)));
  web_ui()->RegisterMessageCallback(
      "getFpsMembershipLabel",
      base::BindRepeating(&SiteSettingsHandler::HandleGetFpsMembershipLabel,
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
      "getCookieSettingDescription",
      base::BindRepeating(
          &SiteSettingsHandler::HandleGetCookieSettingDescription,
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
      "getChooserExceptionList",
      base::BindRepeating(&SiteSettingsHandler::HandleGetChooserExceptionList,
                          base::Unretained(this)));
  web_ui()->RegisterMessageCallback(
      "getNotificationPermissionReview",
      base::BindRepeating(
          &SiteSettingsHandler::HandleGetNotificationPermissionReviewList,
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
      "ignoreNotificationPermissionReviewForOrigins",
      base::BindRepeating(
          &SiteSettingsHandler::
              HandleIgnoreOriginsForNotificationPermissionReview,
          base::Unretained(this)));
  web_ui()->RegisterMessageCallback(
      "resetNotificationPermissionForOrigins",
      base::BindRepeating(
          &SiteSettingsHandler::HandleResetNotificationPermissionForOrigins,
          base::Unretained(this)));
  web_ui()->RegisterMessageCallback(
      "blockNotificationPermissionForOrigins",
      base::BindRepeating(
          &SiteSettingsHandler::HandleBlockNotificationPermissionForOrigins,
          base::Unretained(this)));
  web_ui()->RegisterMessageCallback(
      "allowNotificationPermissionForOrigins",
      base::BindRepeating(
          &SiteSettingsHandler::HandleAllowNotificationPermissionForOrigins,
          base::Unretained(this)));
  web_ui()->RegisterMessageCallback(
      "undoIgnoreNotificationPermissionReviewForOrigins",
      base::BindRepeating(
          &SiteSettingsHandler::
              HandleUndoIgnoreOriginsForNotificationPermissionReview,
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
      "clearEtldPlus1DataAndCookies",
      base::BindRepeating(
          &SiteSettingsHandler::HandleClearEtldPlus1DataAndCookies,
          base::Unretained(this)));
  web_ui()->RegisterMessageCallback(
      "recordAction",
      base::BindRepeating(&SiteSettingsHandler::HandleRecordAction,
                          base::Unretained(this)));
  web_ui()->RegisterMessageCallback(
      "getNumCookiesString",
      base::BindRepeating(&SiteSettingsHandler::HandleGetNumCookiesString,
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

  // Here we only subscribe to the HostZoomMap for the default storage partition
  // since we don't allow the user to manage the zoom levels for apps.
  // We're only interested in zoom-levels that are persisted, since the user
  // is given the opportunity to view/delete these in the content-settings page.
  host_zoom_map_subscription_ =
      content::HostZoomMap::GetDefaultForBrowserContext(profile_)
          ->AddZoomLevelChangedCallback(
              base::BindRepeating(&SiteSettingsHandler::OnZoomLevelChanged,
                                  base::Unretained(this)));

  pref_change_registrar_ = std::make_unique<PrefChangeRegistrar>();
  pref_change_registrar_->Init(profile_->GetPrefs());

  // If the block autoplay pref changes send the new status.
  pref_change_registrar_->Add(
      prefs::kBlockAutoplayEnabled,
      base::BindRepeating(&SiteSettingsHandler::SendBlockAutoplayStatus,
                          base::Unretained(this)));

  // Listen for prefs that impact the effective cookie setting
  pref_change_registrar_->Add(
      prefs::kCookieControlsMode,
      base::BindRepeating(&SiteSettingsHandler::SendCookieSettingDescription,
                          base::Unretained(this)));
}

void SiteSettingsHandler::OnJavascriptDisallowed() {
  observations_.RemoveAllObservations();
  chooser_observations_.RemoveAllObservations();
  host_zoom_map_subscription_ = {};
  pref_change_registrar_->Remove(prefs::kBlockAutoplayEnabled);
  pref_change_registrar_->Remove(prefs::kCookieControlsMode);
  observed_profiles_.RemoveAllObservations();
}

void SiteSettingsHandler::OnGetUsageInfo() {
  DCHECK_CURRENTLY_ON(content::BrowserThread::UI);
  // Site Details Page does not display the number of cookies for the origin.
  const CookieTreeNode* root = cookies_tree_model_->GetRoot();
  std::string usage_string;
  std::string cookie_string;
  std::string fps_string;
  bool fpsPolicy = false;
  // Convert origin to hostname because CookieTreeNode use hostname as
  // |title|(key).
  // TODO(crbug.com/1415380): Ensure the key uniquely identifies the owner of
  // the browsing data (hostname is insufficient) in CookieTreeModel or the new
  // BrowsingDataModel.
  std::string usage_hostname = GURL(usage_origin_).host();
  for (const auto& site : root->children()) {
    std::string title = base::UTF16ToUTF8(site->GetTitle());
    if (title != usage_hostname) {
      continue;
    }
    int64_t size = site->InclusiveSize();
    if (size != 0)
      usage_string = base::UTF16ToUTF8(ui::FormatBytes(size));

    // Usage info only includes unpartitioned cookies, so each cookie must be
    // inspected.
    // TODO (crbug.com/1271155): This is slow, the replacement for the
    // CookiesTreeModel should improve this significantly.
    int num_cookies = 0;
    for (const auto& site_child : site->children()) {
      if (site_child->GetDetailedInfo().node_type !=
          CookieTreeNode::DetailedInfo::TYPE_COOKIES) {
        continue;
      }

      num_cookies += base::ranges::count_if(
          site_child->children(),
          [](const std::unique_ptr<CookieTreeNode>& cookie) {
            const auto& detailed_info = cookie->GetDetailedInfo();
            DCHECK(detailed_info.node_type ==
                   CookieTreeNode::DetailedInfo::TYPE_COOKIE);
            DCHECK(detailed_info.cookie);
            return !detailed_info.cookie->IsPartitioned();
          });
    }
    if (num_cookies != 0) {
      cookie_string = base::UTF16ToUTF8(l10n_util::GetPluralStringFUTF16(
          IDS_SETTINGS_SITE_SETTINGS_NUM_COOKIES, num_cookies));
    }

    auto* privacy_sandbox_service =
        PrivacySandboxServiceFactory::GetForProfile(profile_);
    auto fps_map =
        GetFpsMap(privacy_sandbox_service, cookies_tree_model_.get());
    auto etld_plus1 = GetEtldPlusOne(site->GetDetailedInfo().origin);
    if (fps_map.count(etld_plus1)) {
      fps_string =
          base::UTF16ToUTF8(base::i18n::MessageFormatter::FormatWithNamedArgs(
              l10n_util::GetStringUTF16(
                  IDS_SETTINGS_SITE_SETTINGS_FIRST_PARTY_SETS_MEMBERSHIP_LABEL),
              "MEMBERS", static_cast<int>(fps_map[etld_plus1].second),
              "FPS_OWNER", fps_map[etld_plus1].first));
      fpsPolicy = privacy_sandbox_service->IsPartOfManagedFirstPartySet(
          ConvertEtldToSchemefulSite(etld_plus1));
    }
    break;
  }
  FireWebUIListener("usage-total-changed", base::Value(usage_origin_),
                    base::Value(usage_string), base::Value(cookie_string),
                    base::Value(fps_string), base::Value(fpsPolicy));
}

void SiteSettingsHandler::BrowsingDataModelCreated(
    std::unique_ptr<BrowsingDataModel> model) {
  browsing_data_model_ = std::move(model);
  ModelBuilt();
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

  // If the default cookie setting changed we should update the effective
  // setting description.
  if (content_type == ContentSettingsType::COOKIES) {
    SendCookieSettingDescription();
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
    absl::optional<ContentSettingsType> guard_content_settings_type,
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

void SiteSettingsHandler::HandleFetchUsageTotal(const base::Value::List& args) {
  AllowJavascript();
  CHECK_EQ(1U, args.size());
  usage_origin_ = args[0].GetString();

  update_site_details_ = true;
  RebuildModels();
}

void SiteSettingsHandler::HandleGetFpsMembershipLabel(
    const base::Value::List& args) {
  AllowJavascript();
  CHECK_EQ(3U, args.size());

  std::string callback_id = args[0].GetString();
  int num_members = args[1].GetInt();
  std::string fps_owner = args[2].GetString();

  const std::string label =
      base::UTF16ToUTF8(base::i18n::MessageFormatter::FormatWithNamedArgs(
          l10n_util::GetStringUTF16(
              IDS_SETTINGS_SITE_SETTINGS_FIRST_PARTY_SETS_MEMBERSHIP_LABEL),
          "MEMBERS", static_cast<int>(num_members), "FPS_OWNER", fps_owner));

  ResolveJavascriptCallback(base::Value(callback_id), base::Value(label));
}

void SiteSettingsHandler::HandleClearUnpartitionedUsage(
    const base::Value::List& args) {
  CHECK_EQ(1U, args.size());
  const std::string& origin_string = args[0].GetString();
  auto origin = url::Origin::Create(GURL(origin_string));
  if (origin.opaque())
    return;
  AllowJavascript();

  // TODO(crbug.com/1368048): This code assumes that model pointers are valid.
  // This assumption requires specific front-end behavior which is not strictly
  // enforced.
  DCHECK(browsing_data_model_);
  DCHECK(cookies_tree_model_);

  RemoveMatchingNodes(cookies_tree_model_.get(), origin_string, absl::nullopt);

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

    // Also remove matching cookies node with HTTPS scheme if it exists to
    // avoid confusion when cookies already exist when refreshing clear site
    // data page. Notes: this also means HTTPS sites cookie will be cleared when
    // user clear HTTP scheme Cookie.
    RemoveMatchingNodes(cookies_tree_model_.get(), https_origin.GetURL().spec(),
                        absl::nullopt);
    affected_origins.emplace_back(https_origin);
  }

  RemoveNonTreeModelData(affected_origins);
}

void SiteSettingsHandler::HandleClearPartitionedUsage(
    const base::Value::List& args) {
  CHECK_EQ(2U, args.size());
  const std::string& origin = args[0].GetString();
  const std::string& etld_plus1 = args[1].GetString();

  RemoveMatchingNodes(cookies_tree_model_.get(), origin, etld_plus1);
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

  request_started_time_ = base::TimeTicks::Now();

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
    CreateOrAppendSiteGroupEntry(&all_sites_map_, url);
    origin_permission_set_.insert(url.spec());
  }

  // Get permission exceptions which apply to a single site
  for (auto content_type : content_types) {
    auto exceptions =
        site_settings::GetSiteExceptionsForContentType(map, content_type);
    for (const auto& e : exceptions) {
      GURL url = GURL(e.primary_pattern.ToString());
      CreateOrAppendSiteGroupEntry(&all_sites_map_, url);
      origin_permission_set_.insert(url.spec());
    }
  }

  // Get device chooser permission exceptions.
  for (auto content_type : kChooserDataContentSettingsTypes) {
    base::StringPiece group_name =
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
        const std::string* origin =
            site.GetDict().FindString(site_settings::kOrigin);
        DCHECK(origin);
        GURL url = GURL(*origin);
        CreateOrAppendSiteGroupEntry(&all_sites_map_, url);
        origin_permission_set_.insert(url.spec());
      }
    }
  }

  // Recreate the cookies tree model to refresh the usage information.
  // This happens in the background and will call TreeModelEndBatch() when
  // finished. At that point we send usage data to the page.
  send_sites_list_ = true;
  RebuildModels();

  base::Value::List result;

  // Respond with currently available data.
  ConvertSiteGroupMapToList(all_sites_map_, origin_permission_set_, &result,
                            profile, cookies_tree_model_.get());

  LogAllSitesAction(AllSitesAction2::kLoadPage);

  ResolveJavascriptCallback(base::Value(callback_id), result);
}

void SiteSettingsHandler::HandleGetCategoryList(const base::Value::List& args) {
  AllowJavascript();

  CHECK_EQ(2U, args.size());
  std::string callback_id = args[0].GetString();
  GURL origin(args[1].GetString());

  base::Value::List result;
  for (ContentSettingsType content_type :
       site_settings::GetVisiblePermissionCategories()) {
    result.Append(site_settings::ContentSettingsTypeToGroupName(content_type));
  }

  ResolveJavascriptCallback(base::Value(callback_id), result);
}

void SiteSettingsHandler::HandleGetCookieSettingDescription(
    const base::Value::List& args) {
  AllowJavascript();
  CHECK_EQ(1U, args.size());
  std::string callback_id = args[0].GetString();
  ResolveJavascriptCallback(base::Value(callback_id),
                            base::Value(GetCookieSettingDescription(profile_)));
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
    recent_site.Set(site_settings::kIncognito, site_permissions.incognito);
    if (site_permissions.isolated_web_app_name.has_value()) {
      recent_site.Set(site_settings::kIsolatedWebAppName,
                      site_permissions.isolated_web_app_name.value());
    }

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
  std::map<std::string, int64_t> origin_size_map;
  std::map<std::pair<std::string, absl::optional<std::string>>, int>
      origin_cookie_map;
  base::Value::List list_value;

  GetOriginStorage(&all_sites_map_, &origin_size_map);
  GetOriginCookies(&all_sites_map_, &origin_cookie_map);
  ConvertSiteGroupMapToList(all_sites_map_, origin_permission_set_, &list_value,
                            profile, cookies_tree_model_.get());

  // Merge the origin usage and cookies number into |list_value|.
  for (base::Value& item : list_value) {
    base::Value::Dict& site_group = item.GetDict();
    base::Value::List& origin_list = *site_group.FindList(kOriginList);
    int cookie_num = 0;
    const std::string& etld_plus1 =
        *site_group.FindString(kEffectiveTopLevelDomainPlus1Name);
    const auto& etld_plus1_cookie_num_it =
        origin_cookie_map.find({etld_plus1, absl::nullopt});
    // Add the number of eTLD+1 scoped cookies.
    if (etld_plus1_cookie_num_it != origin_cookie_map.end())
      cookie_num = etld_plus1_cookie_num_it->second;
    // Iterate over the origins for the ETLD+1, and set their usage and cookie
    // numbers.
    for (base::Value& value : origin_list) {
      base::Value::Dict& origin_info = value.GetDict();
      const std::string* origin = origin_info.FindString("origin");
      bool is_partitioned =
          origin_info.FindBool("isPartitioned").value_or(false);
      if (!is_partitioned) {
        // Only unpartitioned storage has a size.
        const auto& size_info_it = origin_size_map.find(*origin);
        if (size_info_it != origin_size_map.end())
          origin_info.Set("usage", static_cast<double>(size_info_it->second));
      }
      GURL origin_url(*origin);
      const auto& origin_cookie_num_it = origin_cookie_map.find(
          {origin_url.host(),
           (is_partitioned ? absl::optional<std::string>(etld_plus1)
                           : absl::nullopt)});
      if (origin_cookie_num_it != origin_cookie_map.end()) {
        origin_info.Set(kNumCookies, origin_cookie_num_it->second);
        // Add cookies numbers for origins that isn't an eTLD+1.
        if (origin_url.host() != etld_plus1 || is_partitioned)
          cookie_num += origin_cookie_num_it->second;
      }
    }
    site_group.Set(kNumCookies, cookie_num);
  }
  return list_value;
}

void SiteSettingsHandler::OnStorageFetched() {
  AllowJavascript();

  // Record how long does it take to fetch the storage and return complete
  // information to the UI.
  DCHECK(!request_started_time_.is_null());
  base::UmaHistogramTimes("WebsiteSettings.GetAllSitesLoadTime",
                          base::TimeTicks::Now() - request_started_time_);
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

  const auto* extension_registry = extensions::ExtensionRegistry::Get(profile_);
  AddExceptionsGrantedByHostedApps(profile_, APIPermissionFromGroupName(type),
                                   &exceptions);
  site_settings::GetExceptionsForContentType(content_type, profile_,
                                             extension_registry, web_ui(),
                                             /*incognito=*/false, &exceptions);

  Profile* incognito =
      profile_->HasPrimaryOTRProfile()
          ? profile_->GetPrimaryOTRProfile(/*create_if_needed=*/true)
          : nullptr;
  // On Chrome OS in Guest mode the incognito profile is the primary profile,
  // so do not fetch an extra copy of the same exceptions.
  if (incognito && incognito != profile_) {
    extension_registry = extensions::ExtensionRegistry::Get(incognito);
    site_settings::GetExceptionsForContentType(content_type, incognito,
                                               extension_registry, web_ui(),
                                               /*incognito=*/true, &exceptions);
  }

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
    HostContentSettingsMap* map =
        HostContentSettingsMapFactory::GetForProfile(profile_);

    std::string source_string, display_name;
    ContentSetting content_setting = site_settings::GetContentSettingForOrigin(
        profile_, map, origin_url, content_type, &source_string, &display_name);
    std::string content_setting_string =
        content_settings::ContentSettingToString(content_setting);

    base::Value::Dict raw_site_exception;
    raw_site_exception.Set(site_settings::kEmbeddingOrigin, origin);
    raw_site_exception.Set(site_settings::kIncognito,
                           profile_->IsOffTheRecord());
    raw_site_exception.Set(site_settings::kOrigin, origin);
    absl::optional<std::string> isolated_web_app_name =
        site_settings::GetIsolatedWebAppName(profile_, origin_url);
    if (isolated_web_app_name.has_value()) {
      raw_site_exception.Set(site_settings::kIsolatedWebAppName,
                             isolated_web_app_name.value());
    }
    absl::optional<std::string> extension_name =
        site_settings::GetExtensionDisplayName(profile_, origin_url);
    if (extension_name.has_value()) {
      raw_site_exception.Set(site_settings::kExtensionNameWithId,
                             l10n_util::GetStringFUTF8(
                                 IDS_SETTINGS_EXTENSION_DISPLAY_NAME,
                                 base::UTF8ToUTF16(extension_name.value()),
                                 base::UTF8ToUTF16(origin_url.host_piece())));
    }
    raw_site_exception.Set(site_settings::kDisplayName, display_name);
    raw_site_exception.Set(site_settings::kSetting, content_setting_string);
    raw_site_exception.Set(site_settings::kSource, source_string);

    exceptions.Append(std::move(raw_site_exception));
  }

  ResolveJavascriptCallback(callback_id, exceptions);
}

void SiteSettingsHandler::HandleGetNotificationPermissionReviewList(
    const base::Value::List& args) {
  AllowJavascript();

  const base::Value& callback_id = args[0];

  base::Value::List result = PopulateNotificationPermissionReviewData();

  ResolveJavascriptCallback(callback_id, base::Value(std::move(result)));
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
  // TODO(crbug.com/1373962): Remove feature flag check after persisted
  // permissions is fully launched.
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

  permission_context->RevokeGrant(
      origin, file_path,
      ChromeFileSystemAccessPermissionContext::PersistedPermissionOptions::
          kUpdatePersistedPermission);
}

void SiteSettingsHandler::HandleRevokeFileSystemGrants(
    const base::Value::List& args) {
  // TODO(crbug.com/1373962): Remove feature flag check after persisted
  // permissions is fully launched.
  DCHECK(base::FeatureList::IsEnabled(
      features::kFileSystemAccessPersistentPermissions));

  CHECK_EQ(1U, args.size());
  AllowJavascript();

  auto url = GURL(args[0].GetString());
  DCHECK(url.is_valid());
  const url::Origin& origin = url::Origin::Create(url);

  ChromeFileSystemAccessPermissionContext* permission_context =
      FileSystemAccessPermissionContextFactory::GetForProfile(profile_);

  permission_context->RevokeGrants(
      origin, ChromeFileSystemAccessPermissionContext::
                  PersistedPermissionOptions::kUpdatePersistedPermission);
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
  if (type_string) {
    types.push_back(
        site_settings::ContentSettingsTypeFromGroupName(*type_string));
  } else {
    // Clear device chooser data permission exceptions.
    if (setting == CONTENT_SETTING_DEFAULT) {
      for (auto content_type : kChooserDataContentSettingsTypes) {
        base::StringPiece group_name =
            site_settings::ContentSettingsTypeToGroupName(content_type);
        DCHECK(!group_name.empty());
        const site_settings::ChooserTypeNameEntry* chooser_type =
            site_settings::ChooserTypeFromGroupName(group_name);
        DCHECK(chooser_type);

        // The BluetoothChooserContext is only available when the
        // WebBluetoothNewPermissionsBackend flag is enabled.
        // TODO(crbug.com/589228): Remove the nullptr check when it is enabled
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
  }

  // Show an infobar reminding the user to reload tabs where their site
  // permissions have been updated.
  // Info bar should only be shown on pages with the same origin and
  // on the same profile
  for (auto* it : *BrowserList::GetInstance()) {
    TabStripModel* tab_strip = it->tab_strip_model();
    for (int i = 0; i < tab_strip->count(); ++i) {
      content::WebContents* web_contents = tab_strip->GetWebContentsAt(i);
      GURL tab_url = web_contents->GetLastCommittedURL();
      if (url::IsSameOriginWith(origin, tab_url) &&
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
}

void SiteSettingsHandler::HandleResetChooserExceptionForSite(
    const base::Value::List& args) {
  CHECK_EQ(3U, args.size());

  const std::string& chooser_type_str = args[0].GetString();
  const site_settings::ChooserTypeNameEntry* chooser_type =
      site_settings::ChooserTypeFromGroupName(chooser_type_str);
  CHECK(chooser_type);

  const std::string& origin_str = args[1].GetString();
  GURL origin(origin_str);
  CHECK(origin.is_valid());

  permissions::ObjectPermissionContextBase* chooser_context =
      chooser_type->get_context(profile_);
  chooser_context->RevokeObjectPermission(url::Origin::Create(origin), args[2]);
}

void SiteSettingsHandler::HandleIgnoreOriginsForNotificationPermissionReview(
    const base::Value::List& args) {
  CHECK_EQ(1U, args.size());
  const base::Value::List& origins = args[0].GetList();

  auto* service =
      NotificationPermissionsReviewServiceFactory::GetForProfile(profile_);
  DCHECK(service);

  for (const auto& origin : origins) {
    const ContentSettingsPattern primary_pattern =
        ContentSettingsPattern::FromString(origin.GetString());
    service->AddPatternToNotificationPermissionReviewBlocklist(
        primary_pattern, ContentSettingsPattern::Wildcard());
  }

  SendNotificationPermissionReviewList();
}

void SiteSettingsHandler::HandleResetNotificationPermissionForOrigins(
    const base::Value::List& args) {
  CHECK_EQ(1U, args.size());

  const base::Value::List& origins = args[0].GetList();

  HostContentSettingsMap* map =
      HostContentSettingsMapFactory::GetForProfile(profile_);

  for (const auto& origin : origins) {
    map->SetContentSettingCustomScope(
        ContentSettingsPattern::FromString(origin.GetString()),
        ContentSettingsPattern::Wildcard(), ContentSettingsType::NOTIFICATIONS,
        CONTENT_SETTING_DEFAULT);
  }

  SendNotificationPermissionReviewList();
}

void SiteSettingsHandler::HandleBlockNotificationPermissionForOrigins(
    const base::Value::List& args) {
  CHECK_EQ(1U, args.size());
  const base::Value::List& origins = args[0].GetList();

  HostContentSettingsMap* map =
      HostContentSettingsMapFactory::GetForProfile(profile_);
  for (const auto& origin : origins) {
    map->SetContentSettingCustomScope(
        ContentSettingsPattern::FromString(origin.GetString()),
        ContentSettingsPattern::Wildcard(), ContentSettingsType::NOTIFICATIONS,
        CONTENT_SETTING_BLOCK);
  }

  SendNotificationPermissionReviewList();
}

void SiteSettingsHandler::HandleAllowNotificationPermissionForOrigins(
    const base::Value::List& args) {
  CHECK_EQ(1U, args.size());
  const base::Value::List& origins = args[0].GetList();

  HostContentSettingsMap* map =
      HostContentSettingsMapFactory::GetForProfile(profile_);

  for (const auto& origin : origins) {
    map->SetContentSettingCustomScope(
        ContentSettingsPattern::FromString(origin.GetString()),
        ContentSettingsPattern::Wildcard(), ContentSettingsType::NOTIFICATIONS,
        CONTENT_SETTING_ALLOW);
  }

  SendNotificationPermissionReviewList();
}

void SiteSettingsHandler::
    HandleUndoIgnoreOriginsForNotificationPermissionReview(
        const base::Value::List& args) {
  CHECK_EQ(1U, args.size());
  const base::Value::List& origins = args[0].GetList();
  auto* service =
      NotificationPermissionsReviewServiceFactory::GetForProfile(profile_);
  DCHECK(service);

  for (const auto& origin : origins) {
    const ContentSettingsPattern& primary_pattern =
        ContentSettingsPattern::FromString(origin.GetString());
    service->RemovePatternFromNotificationPermissionReviewBlocklist(
        primary_pattern, ContentSettingsPattern::Wildcard());
  }
  SendNotificationPermissionReviewList();
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
  const std::string& type = args[2].GetString();

  std::string reason;
  bool is_valid =
      IsPatternValidForType(pattern_string, type, profile_, &reason);

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
  for (const auto& zoom_level : zoom_levels) {
    base::Value::Dict exception;
    switch (zoom_level.mode) {
      case content::HostZoomMap::ZOOM_CHANGED_FOR_HOST: {
        std::string host = zoom_level.host;
        if (host == content::kUnreachableWebDataURL) {
          host =
              l10n_util::GetStringUTF8(IDS_ZOOMLEVELS_CHROME_ERROR_PAGES_LABEL);
        }
        exception.Set(site_settings::kOrigin, host);

        std::string display_name = host;
        std::string origin_for_favicon = host;
        // As an optimization, only check hosts that could be an extension.
        if (crx_file::id_util::IdIsValid(host)) {
          // Look up the host as an extension, if found then it is an extension.
          const extensions::Extension* extension =
              extension_registry->GetExtensionById(
                  host, extensions::ExtensionRegistry::EVERYTHING);
          if (extension) {
            origin_for_favicon = extension->url().spec();
            display_name = extension->name();
          }
        }
        exception.Set(site_settings::kDisplayName, display_name);
        exception.Set(site_settings::kOriginForFavicon, origin_for_favicon);
        break;
      }
      case content::HostZoomMap::ZOOM_CHANGED_FOR_SCHEME_AND_HOST:
        // These are not stored in preferences and get cleared on next browser
        // start. Therefore, we don't care for them.
        continue;
      case content::HostZoomMap::ZOOM_CHANGED_TEMPORARY_ZOOM:
        NOTREACHED();
    }

    std::string setting_string =
        content_settings::ContentSettingToString(CONTENT_SETTING_DEFAULT);
    DCHECK(!setting_string.empty());

    exception.Set(site_settings::kSetting, setting_string);

    // Calculate the zoom percent from the factor. Round up to the nearest whole
    // number.
    int zoom_percent = static_cast<int>(
        blink::PageZoomLevelToZoomFactor(zoom_level.zoom_level) * 100 + 0.5);
    exception.Set(kZoom, base::FormatPercent(zoom_percent));
    exception.Set(site_settings::kSource,
                  site_settings::SiteSettingSourceToString(
                      site_settings::SiteSettingSource::kPreference));
    // Append the new entry to the list and map.
    zoom_levels_exceptions.Append(std::move(exception));
  }

  FireWebUIListener("onZoomLevelsChanged", zoom_levels_exceptions);
}

void SiteSettingsHandler::HandleRemoveZoomLevel(const base::Value::List& args) {
  CHECK_EQ(1U, args.size());

  std::string origin = args[0].GetString();

  if (origin ==
      l10n_util::GetStringUTF8(IDS_ZOOMLEVELS_CHROME_ERROR_PAGES_LABEL)) {
    origin = content::kUnreachableWebDataURL;
  }

  content::HostZoomMap* host_zoom_map;
  host_zoom_map = content::HostZoomMap::GetDefaultForBrowserContext(profile_);
  double default_level = host_zoom_map->GetDefaultZoomLevel();
  host_zoom_map->SetZoomLevelForHost(origin, default_level);
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

void SiteSettingsHandler::RebuildModels() {
  // The handler services two requests async once models have been built.
  DCHECK(update_site_details_ || send_sites_list_);

  // Tests will directly fire the appropriate service method.
  if (models_set_for_testing_)
    return;

  // Don't do anything if the models are already in the process of being built.
  // Requests will be serviced when the existing build process finishes.
  if (num_models_being_built_ > 0)
    return;

  // Reset any existing models.
  // TODO(crbug.com/1368048) The implicit semantics of the handler require the
  // models to be reset every time, but this is not required for all operations.
  // A stronger call ordering enforcement, or stronger guarantees around when
  // the models exist, could remove the requirement for this.
  cookies_tree_model_.reset();
  browsing_data_model_.reset();

  num_models_being_built_ = 2;

  content::StoragePartition* storage_partition =
      profile_->GetDefaultStoragePartition();
  BrowsingDataModel::BuildFromDisk(
      storage_partition,
      base::BindOnce(&SiteSettingsHandler::BrowsingDataModelCreated,
                     weak_ptr_factory_.GetWeakPtr()));

  cookies_tree_model_ = CookiesTreeModel::CreateForProfileDeprecated(profile_);
  cookies_tree_model_->AddCookiesTreeObserver(this);
}

void SiteSettingsHandler::ModelBuilt() {
  DCHECK(num_models_being_built_ > 0);
  num_models_being_built_--;

  if (num_models_being_built_ == 0)
    ServicePendingRequests();
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

  observed_profiles_.RemoveObservation(profile);
}

void SiteSettingsHandler::TreeNodesAdded(ui::TreeModel* model,
                                         ui::TreeModelNode* parent,
                                         size_t start,
                                         size_t count) {}

void SiteSettingsHandler::TreeNodesRemoved(ui::TreeModel* model,
                                           ui::TreeModelNode* parent,
                                           size_t start,
                                           size_t count) {}

void SiteSettingsHandler::TreeNodeChanged(ui::TreeModel* model,
                                          ui::TreeModelNode* node) {}

void SiteSettingsHandler::TreeModelEndBatchDeprecated(CookiesTreeModel* model) {
  ModelBuilt();
}

void SiteSettingsHandler::GetOriginStorage(
    std::map<std::string, std::set<std::pair<std::string, bool>>>*
        all_sites_map,
    std::map<std::string, int64_t>* origin_size_map) {
  CHECK(cookies_tree_model_.get());

  for (const auto& site : cookies_tree_model_->GetRoot()->children()) {
    int64_t size = site->InclusiveSize();
    if (size == 0)
      continue;
    UpdateDataFromModel(all_sites_map, origin_size_map,
                        site->GetDetailedInfo().origin.GetURL(), size);
  }

  for (const auto& entry : *browsing_data_model_) {
    if (entry.data_details->storage_size == 0)
      continue;

    // Convert the primary host to an HTTPS url to match expecations for this
    // code.
    GURL host_url(std::string(url::kHttpsScheme) +
                  url::kStandardSchemeSeparator + *entry.primary_host + "/");
    UpdateDataFromModel(all_sites_map, origin_size_map, host_url,
                        entry.data_details->storage_size);
  }
}

void SiteSettingsHandler::GetOriginCookies(
    std::map<std::string, std::set<std::pair<std::string, bool>>>*
        all_sites_map,
    std::map<std::pair<std::string, absl::optional<std::string>>, int>*
        origin_cookie_map) {
  CHECK(cookies_tree_model_.get());
  // Get sites that don't have data but have cookies.
  // TODO(crbug.com/1271155): Query the Browsing Data Model instead when cookie
  // information is available there.
  for (const auto& site : cookies_tree_model_->GetRoot()->children()) {
    GURL url = site->GetDetailedInfo().origin.GetURL();
    if (!site->NumberOfCookies())
      continue;

    // Each cookie will need to be inspected to see if it is partitioned, so it
    // may be associated with the appropriate eTLD+1.
    // TODO (crbug.com/1271155): This is slow, the replacement for the
    // CookiesTreeModel should improve this significantly.
    for (const auto& site_child : site->children()) {
      if (site_child->GetDetailedInfo().node_type !=
          CookieTreeNode::DetailedInfo::TYPE_COOKIES) {
        continue;
      }

      for (const auto& cookie : site_child->children()) {
        const auto& detailed_info = cookie->GetDetailedInfo();
        DCHECK(detailed_info.node_type ==
               CookieTreeNode::DetailedInfo::TYPE_COOKIE);
        DCHECK(detailed_info.cookie);

        absl::optional<std::string> associated_etld_plus1 =
            detailed_info.cookie->IsPartitioned()
                ? absl::optional<std::string>(
                      detailed_info.cookie->PartitionKey()
                          ->site()
                          .GetURL()
                          .host())
                : absl::nullopt;
        CreateOrAppendSiteGroupEntry(all_sites_map, url,
                                     /*url_is_origin_with_cookies = */ true,
                                     associated_etld_plus1);
        (*origin_cookie_map)[{url.host(), associated_etld_plus1}]++;
      }
    }
  }
}

void SiteSettingsHandler::HandleClearEtldPlus1DataAndCookies(
    const base::Value::List& args) {
  CHECK_EQ(1U, args.size());
  const std::string& etld_plus1 = args[0].GetString();

  AllowJavascript();
  RemoveMatchingNodes(cookies_tree_model_.get(), absl::nullopt, etld_plus1);

  // Retrieve all of the origin entries grouped under this eTLD + 1.
  std::vector<url::Origin> affected_origins;
  for (const auto& origin_is_partitioned : all_sites_map_[etld_plus1]) {
    // Ignore entries which are partitioned, as no non-cookie tree storage is
    // partitioned.
    if (origin_is_partitioned.second)
      continue;

    affected_origins.emplace_back(url::Origin::Create(
        // A placeholder origin may have been created, in this case the
        // eTLD+1 itself should be converted to an origin, the same as it would
        // have been for display.
        origin_is_partitioned.first == kPlaceholder
            ? GURL(ConvertEtldToOrigin(etld_plus1, /*secure=*/false))
            : GURL(origin_is_partitioned.first)));
  }

  // Cookies may have associated with the entry for the eTLD+1 itself.
  // As per the logic in CreateOrAppendSiteGroupEntry, this will only occur
  // if the existing entry was https, otherwise a new http entry would be
  // created for the placeholder. Hence, we need only additionally include the
  // HTTPS version of the eTLD+1 as an origin.
  std::string https_url = std::string(url::kHttpsScheme) +
                          url::kStandardSchemeSeparator + etld_plus1 + "/";
  affected_origins.emplace_back(url::Origin::Create(GURL(https_url)));

  RemoveNonTreeModelData(affected_origins);
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
  std::string callback_id;
  callback_id = args[0].GetString();
  int num_cookies = args[1].GetInt();

  AllowJavascript();
  const std::u16string string =
      num_cookies > 0 ? l10n_util::GetPluralStringFUTF16(
                            IDS_SETTINGS_SITE_SETTINGS_NUM_COOKIES, num_cookies)
                      : std::u16string();

  ResolveJavascriptCallback(base::Value(callback_id), base::Value(string));
}

void SiteSettingsHandler::RemoveNonTreeModelData(
    const std::vector<url::Origin>& origins) {
  // TODO(crbug.com/1268626): Remove client hint information, which cannot be
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
  }
  // Remove Privacy Sandbox API data.
  content::BrowsingDataRemover* remover = profile_->GetBrowsingDataRemover();
  std::unique_ptr<content::BrowsingDataFilterBuilder> filter =
      content::BrowsingDataFilterBuilder::Create(
          content::BrowsingDataFilterBuilder::Mode::kDelete);
  for (const auto& origin : origins) {
    filter->AddOrigin(origin);
  }
  remover->RemoveWithFilter(
      base::Time::Min(), base::Time::Max(),
      content::BrowsingDataRemover::DATA_TYPE_PRIVACY_SANDBOX &
          // Part of BrowsingDataModel:
          ~content::BrowsingDataRemover::DATA_TYPE_TRUST_TOKENS,
      content::BrowsingDataRemover::ORIGIN_TYPE_UNPROTECTED_WEB,
      std::move(filter));

  // Remove Privacy Sandbox API data not integrated with the
  // BrowsingDataRemover.
  if (auto* browsing_topics_service =
          browsing_topics::BrowsingTopicsServiceFactory::GetForProfile(
              profile_)) {
    for (const auto& origin : origins) {
      browsing_topics_service->ClearTopicsDataForOrigin(origin);
    }
  }

  // Remove any Browsing Data Model data associated with the origins host.
  // TODO(crbug.com/1271155) - When the browsing data model supports all storage
  // types, re-work this handler to work directly with primary hosts as defined
  // by the model.
  for (const auto& origin : origins)
    browsing_data_model_->RemoveBrowsingData(origin.host(), base::DoNothing());

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

void SiteSettingsHandler::SetModelsForTesting(
    std::unique_ptr<CookiesTreeModel> cookies_tree_model,
    std::unique_ptr<BrowsingDataModel> browsing_data_model) {
  request_started_time_ = base::TimeTicks::Now();
  cookies_tree_model_ = std::move(cookies_tree_model);
  browsing_data_model_ = std::move(browsing_data_model);
  models_set_for_testing_ = true;
}

void SiteSettingsHandler::ClearAllSitesMapForTesting() {
  all_sites_map_.clear();
}

void SiteSettingsHandler::SendCookieSettingDescription() {
  FireWebUIListener("cookieSettingDescriptionChanged",
                    base::Value(GetCookieSettingDescription(profile_)));
}

base::Value::List
SiteSettingsHandler::PopulateNotificationPermissionReviewData() {
  base::Value::List result;
  if (!base::FeatureList::IsEnabled(
          features::kSafetyCheckNotificationPermissions))
    return result;

  auto* service =
      NotificationPermissionsReviewServiceFactory::GetForProfile(profile_);
  if (!service)
    return result;

  auto notification_permissions = service->GetNotificationSiteListForReview();

  site_engagement::SiteEngagementService* engagement_service =
      site_engagement::SiteEngagementService::Get(profile_);

  // Sort notification permissions by their priority for surfacing to the user.
  auto notification_permission_ordering =
      [](const permissions::NotificationPermissions& left,
         const permissions::NotificationPermissions& right) {
        return left.notification_count > right.notification_count;
      };
  std::sort(notification_permissions.begin(), notification_permissions.end(),
            notification_permission_ordering);

  for (const auto& notification_permission : notification_permissions) {
    // Converting primary pattern to GURL should always be valid, since
    // Notification Permission Review list only contains single origins. Those
    // are filtered in
    // NotificationPermissionsReviewService::GetNotificationSiteListForReview.
    GURL url = GURL(notification_permission.primary_pattern.ToString());
    DCHECK(url.is_valid());
    if (!ShouldAddToNotificationPermissionReviewList(
            engagement_service, url,
            notification_permission.notification_count)) {
      continue;
    }

    base::Value::Dict permission;
    permission.Set(site_settings::kOrigin,
                   notification_permission.primary_pattern.ToString());

    std::string notification_info_string =
        base::UTF16ToUTF8(l10n_util::GetPluralStringFUTF16(
            IDS_SETTINGS_SAFETY_CHECK_REVIEW_NOTIFICATION_PERMISSIONS_COUNT_LABEL,
            notification_permission.notification_count));
    permission.Set(site_settings::kNotificationInfoString,
                   notification_info_string);
    result.Append(std::move(permission));
  }

  return result;
}

// Dictionary keys for an individual `FileSystemPermissionGrant`.
// Schema (per grant):
// {
//     "origin" : <string>;
//     "filePath" : <string>;
//     "isWritable" : <bool>;
//     "isDirectory" : <bool>;
// }

// Dictionary keys for an individual permission grant in
// the returned `grants` List.
// Note that while the `isWritable` and `isDirectory` values
// are implied by the names of the grant lists, the
// `FileSystemPermissionGrant` type contains these attributes
// in order to make the data more easily accessible from the UI code.
//
// Schema (per origin):
// [
//  ...
//   {
//     "origin" : <string>;
//     "directoryReadGrants" : FileSystemPermissionGrant[];
//     "directoryWriteGrants" : FileSystemPermissionGrant[];
//     "fileReadGrants" : FileSystemPermissionGrant[];
//     "fileWriteGrants" : FileSystemPermissionGrant[];
//   }
//  ...
// ]
base::Value::List SiteSettingsHandler::PopulateFileSystemGrantData() {
  base::Value::List grants;

  // TODO(crbug.com/1373962): Remove feature flag check after persisted
  // permissions is fully launched.
  if (!base::FeatureList::IsEnabled(
          features::kFileSystemAccessPersistentPermissions))
    return grants;
  ChromeFileSystemAccessPermissionContext* permission_context =
      FileSystemAccessPermissionContextFactory::GetForProfile(profile_);
  std::vector<url::Origin> origins_with_grants =
      permission_context->GetOriginsWithGrants();

  for (auto& origin : origins_with_grants) {
    ChromeFileSystemAccessPermissionContext::Grants grantObj =
        permission_context->GetPermissionGrants(origin);
    if (grantObj.file_read_grants.empty() &&
        grantObj.file_write_grants.empty() &&
        grantObj.directory_read_grants.empty() &&
        grantObj.directory_write_grants.empty()) {
      continue;
    }

    base::Value::Dict file_system_permission_grant;
    base::Value::List directory_read_grants;
    base::Value::List directory_write_grants;
    base::Value::List file_read_grants;
    base::Value::List file_write_grants;
    std::string origin_string = origin.GetURL().spec();
    file_system_permission_grant.Set(site_settings::kOrigin, origin_string);

    // Populate the `file_system_permission_grant` object with allowed
    // permissions.
    for (auto& file_path : grantObj.directory_write_grants) {
      base::Value::Dict directory_write_grant;
      directory_write_grant.Set(site_settings::kOrigin, origin_string);
      directory_write_grant.Set(site_settings::kFilePath,
                                FilePathToValue(file_path));
      directory_write_grant.Set(site_settings::kIsWritable, true);
      directory_write_grant.Set(site_settings::kIsDirectory, true);
      directory_write_grants.Append(std::move(directory_write_grant));
    }
    file_system_permission_grant.Set(site_settings::kDirectoryWriteGrants,
                                     std::move(directory_write_grants));

    for (auto& file_path : grantObj.directory_read_grants) {
      base::Value::Dict directory_read_grant;
      directory_read_grant.Set(site_settings::kOrigin, origin_string);
      directory_read_grant.Set(site_settings::kFilePath,
                               FilePathToValue(file_path));
      directory_read_grant.Set(site_settings::kIsWritable, false);
      directory_read_grant.Set(site_settings::kIsDirectory, true);
      directory_read_grants.Append(std::move(directory_read_grant));
    }
    file_system_permission_grant.Set(site_settings::kDirectoryReadGrants,
                                     std::move(directory_read_grants));

    for (auto& file_path : grantObj.file_write_grants) {
      base::Value::Dict file_write_grant;
      file_write_grant.Set(site_settings::kOrigin, origin_string);
      file_write_grant.Set(site_settings::kFilePath,
                           FilePathToValue(file_path));
      file_write_grant.Set(site_settings::kIsWritable, true);
      file_write_grant.Set(site_settings::kIsDirectory, false);
      file_write_grants.Append(std::move(file_write_grant));
    }
    file_system_permission_grant.Set(site_settings::kFileWriteGrants,
                                     std::move(file_write_grants));

    for (auto& file_path : grantObj.file_read_grants) {
      base::Value::Dict file_read_grant;
      file_read_grant.Set(site_settings::kOrigin, origin_string);
      file_read_grant.Set(site_settings::kFilePath, FilePathToValue(file_path));
      file_read_grant.Set(site_settings::kIsWritable, false);
      file_read_grant.Set(site_settings::kIsDirectory, false);
      file_read_grants.Append((base::Value(std::move(file_read_grant))));
    }
    file_system_permission_grant.Set(site_settings::kFileReadGrants,
                                     std::move(file_read_grants));
    grants.Append(std::move(file_system_permission_grant));
  }
  return grants;
}

void SiteSettingsHandler::SendNotificationPermissionReviewList() {
  if (!base::FeatureList::IsEnabled(
          features::kSafetyCheckNotificationPermissions)) {
    return;
  }
  // Notify observers that the permission review list could have changed. Note
  // that the list is not guaranteed to have changed. In places where
  // determining whether the list has changed is cause for performance concerns,
  // an unchanged list may be sent. This is the case for
  // HandleResetCategoryPermissionForPattern and
  // HandleSetCategoryPermissionForPattern.
  FireWebUIListener("notification-permission-review-list-maybe-changed",
                    PopulateNotificationPermissionReviewData());
}

}  // namespace settings
