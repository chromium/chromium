// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/webui/settings/site_settings_handler.h"

#include <algorithm>
#include <utility>
#include <vector>

#include "base/barrier_closure.h"
#include "base/bind.h"
#include "base/i18n/number_formatting.h"
#include "base/macros.h"
#include "base/metrics/histogram_macros.h"
#include "base/metrics/user_metrics.h"
#include "base/stl_util.h"
#include "base/strings/utf_string_conversions.h"
#include "base/values.h"
#include "chrome/browser/content_settings/host_content_settings_map_factory.h"
#include "chrome/browser/content_settings/web_site_settings_uma_util.h"
#include "chrome/browser/engagement/site_engagement_service.h"
#include "chrome/browser/infobars/infobar_service.h"
#include "chrome/browser/media/unified_autoplay_config.h"
#include "chrome/browser/permissions/chooser_context_base.h"
#include "chrome/browser/permissions/permission_decision_auto_blocker.h"
#include "chrome/browser/permissions/permission_manager.h"
#include "chrome/browser/permissions/permission_uma_util.h"
#include "chrome/browser/permissions/permission_util.h"
#include "chrome/browser/serial/serial_chooser_context.h"
#include "chrome/browser/serial/serial_chooser_context_factory.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/browser/ui/browser_list.h"
#include "chrome/browser/ui/page_info/page_info_infobar_delegate.h"
#include "chrome/browser/ui/tabs/tab_strip_model.h"
#include "chrome/browser/ui/webui/site_settings_helper.h"
#include "chrome/browser/usb/usb_chooser_context.h"
#include "chrome/browser/usb/usb_chooser_context_factory.h"
#include "chrome/browser/web_applications/web_app_provider.h"
#include "chrome/browser/web_applications/web_app_registrar.h"
#include "chrome/common/extensions/manifest_handlers/app_launch_info.h"
#include "chrome/common/pref_names.h"
#include "chrome/grit/generated_resources.h"
#include "components/content_settings/core/browser/website_settings_registry.h"
#include "components/content_settings/core/common/content_settings_types.h"
#include "components/content_settings/core/common/content_settings_utils.h"
#include "components/crx_file/id_util.h"
#include "components/prefs/pref_change_registrar.h"
#include "components/prefs/pref_service.h"
#include "content/public/browser/browser_thread.h"
#include "content/public/browser/web_contents.h"
#include "content/public/browser/web_ui.h"
#include "content/public/common/origin_util.h"
#include "content/public/common/url_constants.h"
#include "extensions/browser/extension_registry.h"
#include "extensions/common/permissions/api_permission.h"
#include "extensions/common/permissions/permissions_data.h"
#include "third_party/blink/public/common/page/page_zoom.h"
#include "ui/base/l10n/l10n_util.h"
#include "ui/base/text/bytes_formatting.h"

#if defined(OS_CHROMEOS)
#include "components/user_manager/user_manager.h"
#endif

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
constexpr char kZoom[] = "zoom";
// Placeholder value for ETLD+1 until a valid origin is added. If an ETLD+1
// only has placeholder, then create an ETLD+1 origin.
constexpr char kPlaceholder[] = "placeholder";

// These values are persisted to logs. Entries should not be renumbered and
// numeric values should never be reused.
enum class AllSitesAction {
  kLoadPage = 0,
  kResetPermissions = 1,
  kClearData = 2,
  kEnterSiteDetails = 3,
  kMaxValue = kEnterSiteDetails,
};

// Return an appropriate API Permission ID for the given string name.
extensions::APIPermission::APIPermission::ID APIPermissionFromGroupName(
    std::string type) {
  // Once there are more than two groups to consider, this should be changed to
  // something better than if's.

  if (site_settings::ContentSettingsTypeFromGroupName(type) ==
      ContentSettingsType::GEOLOCATION)
    return extensions::APIPermission::APIPermission::kGeolocation;

  if (site_settings::ContentSettingsTypeFromGroupName(type) ==
      ContentSettingsType::NOTIFICATIONS)
    return extensions::APIPermission::APIPermission::kNotifications;

  return extensions::APIPermission::APIPermission::kInvalid;
}

// Asks the |profile| for hosted apps which have the |permission| set, and
// adds their web extent and launch URL to the |exceptions| list.
void AddExceptionsGrantedByHostedApps(
    content::BrowserContext* context,
    extensions::APIPermission::APIPermission::ID permission,
    base::ListValue* exceptions) {
  const extensions::ExtensionSet& extensions =
      extensions::ExtensionRegistry::Get(context)->enabled_extensions();
  for (extensions::ExtensionSet::const_iterator extension = extensions.begin();
       extension != extensions.end(); ++extension) {
    if (!(*extension)->is_hosted_app() ||
        !(*extension)->permissions_data()->HasAPIPermission(permission))
      continue;

    const extensions::URLPatternSet& web_extent = (*extension)->web_extent();
    // Add patterns from web extent.
    for (auto pattern = web_extent.begin(); pattern != web_extent.end();
         ++pattern) {
      std::string url_pattern = pattern->GetAsString();
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

base::flat_set<web_app::AppId> GetInstalledApps(
    Profile* profile,
    web_app::AppRegistrar& registrar) {
  auto apps = registrar.GetAppIds();
  base::flat_set<std::string> installed;
  for (auto app : apps) {
    base::Optional<GURL> scope = registrar.GetAppScope(app);
    if (scope.has_value())
      installed.insert(scope.value().GetOrigin().spec());
  }
  return installed;
}

// Whether |pattern| applies to a single origin.
bool PatternAppliesToSingleOrigin(const ContentSettingPatternSource& pattern) {
  const GURL url(pattern.primary_pattern.ToString());
  // Default settings and other patterns apply to multiple origins.
  if (url::Origin::Create(url).opaque())
    return false;
  // Embedded content settings only when |url| is embedded in another origin, so
  // ignore non-wildcard secondary patterns that are different to the primary.
  if (pattern.primary_pattern != pattern.secondary_pattern &&
      pattern.secondary_pattern != ContentSettingsPattern::Wildcard()) {
    return false;
  }
  return true;
}

// Groups |url| into sets of eTLD+1s in |site_group_map|, assuming |url| is an
// origin.
// There are three cases:
// 1. The ETLD+1 of |url| is not yet in |site_group_map|. We add the ETLD+1
//    to |site_group_map|. If the |url| is an ETLD+1 cookie origin, put a
//    placeholder origin for the ETLD+1.
// 2. The ETLD+1 of |url| is in |site_group_map|, and is equal to host of
//    |url|. This means case 1 has already happened and nothing more needs to
//    be done.
// 3. The ETLD+1 of |url| is in |site_group_map| and is different to host of
//    |url|. For a cookies url, if a https origin with same host exists,
//    nothing more needs to be done.
// In case 3, we try to add |url| to the set of origins for the ETLD+1. If an
// existing origin is a placeholder, delete it, because the placeholder is no
// longer needed.
void CreateOrAppendSiteGroupEntry(
    std::map<std::string, std::set<std::string>>* site_group_map,
    const GURL& url,
    bool url_is_origin_with_cookies = false) {
  std::string etld_plus1_string =
      net::registry_controlled_domains::GetDomainAndRegistry(
          url, net::registry_controlled_domains::INCLUDE_PRIVATE_REGISTRIES);
  auto entry = site_group_map->find(etld_plus1_string);
  bool etld_plus1_cookie_url =
      url_is_origin_with_cookies && url.host() == etld_plus1_string;

  if (entry == site_group_map->end()) {
    // Case 1:
    std::string origin = etld_plus1_cookie_url ? kPlaceholder : url.spec();
    site_group_map->emplace(etld_plus1_string, std::set<std::string>({origin}));
    return;
  }
  // Case 2:
  if (etld_plus1_cookie_url)
    return;
  // Case 3:
  if (url_is_origin_with_cookies) {
    // Cookies ignore schemes, so try and see if a https schemed version
    // already exists in the origin list, if not, then add the http schemed
    // version into the map.
    std::string https_url = std::string(url::kHttpsScheme) +
                            url::kStandardSchemeSeparator + url.host() + "/";
    if (entry->second.find(https_url) != entry->second.end())
      return;
  }
  entry->second.insert(url.spec());
  auto placeholder = entry->second.find(kPlaceholder);
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

// Converts a given |site_group_map| to a list of base::DictionaryValues, adding
// the site engagement score for each origin.
void ConvertSiteGroupMapToListValue(
    const std::map<std::string, std::set<std::string>>& site_group_map,
    const std::set<std::string>& origin_permission_set,
    base::Value* list_value,
    Profile* profile,
    web_app::AppRegistrar& registrar) {
  DCHECK_EQ(base::Value::Type::LIST, list_value->type());
  DCHECK(profile);
  base::flat_set<web_app::AppId> installed_apps =
      GetInstalledApps(profile, registrar);
  SiteEngagementService* engagement_service =
      SiteEngagementService::Get(profile);
  for (const auto& entry : site_group_map) {
    // eTLD+1 is the effective top level domain + 1.
    base::Value site_group(base::Value::Type::DICTIONARY);
    site_group.SetKey(kEffectiveTopLevelDomainPlus1Name,
                      base::Value(entry.first));
    bool has_installed_pwa = false;
    base::Value origin_list(base::Value::Type::LIST);
    for (const std::string& origin : entry.second) {
      base::Value origin_object(base::Value::Type::DICTIONARY);
      // If origin is placeholder, create a http ETLD+1 origin for it.
      if (origin == kPlaceholder) {
        origin_object.SetKey(
            "origin",
            base::Value(std::string(url::kHttpScheme) +
                        url::kStandardSchemeSeparator + entry.first + "/"));
      } else {
        origin_object.SetKey("origin", base::Value(origin));
      }
      origin_object.SetKey(
          "engagement",
          base::Value(engagement_service->GetScore(GURL(origin))));
      origin_object.SetKey("usage", base::Value(0));
      origin_object.SetKey(kNumCookies, base::Value(0));

      bool is_installed = installed_apps.contains(origin);
      if (is_installed)
        has_installed_pwa = true;
      origin_object.SetKey(kIsInstalled, base::Value(is_installed));

      origin_object.SetKey(
          kHasPermissionSettings,
          base::Value(base::Contains(origin_permission_set, origin)));
      origin_list.Append(std::move(origin_object));
    }
    site_group.SetKey(kHasInstalledPWA, base::Value(has_installed_pwa));
    site_group.SetKey(kNumCookies, base::Value(0));
    site_group.SetKey(kOriginList, std::move(origin_list));
    list_value->Append(std::move(site_group));
  }
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
      !content::IsOriginSecure(url)) {
    *out_error = l10n_util::GetStringUTF8(
        IDS_SETTINGS_NOT_VALID_WEB_ADDRESS_FOR_CONTENT_TYPE);
    return false;
  }

  // The pattern is valid.
  return true;
}

void UpdateDataFromCookiesTree(
    std::map<std::string, std::set<std::string>>* all_sites_map,
    std::map<std::string, int64_t>* origin_size_map,
    const GURL& origin,
    int64_t size) {
  UpdateDataForOrigin(origin, size, origin_size_map);
  CreateOrAppendSiteGroupEntry(all_sites_map, origin);
}

void LogAllSitesAction(AllSitesAction action) {
  UMA_HISTOGRAM_ENUMERATION("WebsiteSettings.AllSitesAction", action);
}

}  // namespace

SiteSettingsHandler::SiteSettingsHandler(Profile* profile,
                                         web_app::AppRegistrar& app_registrar)
    : profile_(profile), app_registrar_(app_registrar) {}

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
      "clearUsage", base::BindRepeating(&SiteSettingsHandler::HandleClearUsage,
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
      "getFormattedBytes",
      base::BindRepeating(&SiteSettingsHandler::HandleGetFormattedBytes,
                          base::Unretained(this)));
  web_ui()->RegisterMessageCallback(
      "getExceptionList",
      base::BindRepeating(&SiteSettingsHandler::HandleGetExceptionList,
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
      "clearFlashPref",
      base::BindRepeating(&SiteSettingsHandler::HandleClearFlashPref,
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
      "clearEtldPlus1DataAndCookies",
      base::BindRepeating(
          &SiteSettingsHandler::HandleClearEtldPlus1DataAndCookies,
          base::Unretained(this)));
  web_ui()->RegisterMessageCallback(
      "recordAction",
      base::BindRepeating(&SiteSettingsHandler::HandleRecordAction,
                          base::Unretained(this)));
}

void SiteSettingsHandler::OnJavascriptAllowed() {
  ObserveSourcesForProfile(profile_);
  if (profile_->HasOffTheRecordProfile())
    ObserveSourcesForProfile(profile_->GetOffTheRecordProfile());

  // Here we only subscribe to the HostZoomMap for the default storage partition
  // since we don't allow the user to manage the zoom levels for apps.
  // We're only interested in zoom-levels that are persisted, since the user
  // is given the opportunity to view/delete these in the content-settings page.
  host_zoom_map_subscription_ =
      content::HostZoomMap::GetDefaultForBrowserContext(profile_)
          ->AddZoomLevelChangedCallback(
              base::Bind(&SiteSettingsHandler::OnZoomLevelChanged,
                         base::Unretained(this)));

  pref_change_registrar_ = std::make_unique<PrefChangeRegistrar>();
  pref_change_registrar_->Init(profile_->GetPrefs());

  // If the block autoplay pref changes send the new status.
  pref_change_registrar_->Add(
      prefs::kBlockAutoplayEnabled,
      base::Bind(&SiteSettingsHandler::SendBlockAutoplayStatus,
                 base::Unretained(this)));

#if defined(OS_CHROMEOS)
  pref_change_registrar_->Add(
      prefs::kEnableDRM,
      base::Bind(&SiteSettingsHandler::OnPrefEnableDrmChanged,
                 base::Unretained(this)));
#endif
}

void SiteSettingsHandler::OnJavascriptDisallowed() {
  observer_.RemoveAll();
  chooser_observer_.RemoveAll();
  host_zoom_map_subscription_.reset();
  pref_change_registrar_->Remove(prefs::kBlockAutoplayEnabled);
#if defined(OS_CHROMEOS)
  pref_change_registrar_->Remove(prefs::kEnableDRM);
#endif
  observed_profiles_.RemoveAll();
}

void SiteSettingsHandler::OnGetUsageInfo() {
  DCHECK_CURRENTLY_ON(content::BrowserThread::UI);
  // Site Details Page does not display the number of cookies for the origin.
  const CookieTreeNode* root = cookies_tree_model_->GetRoot();
  std::string usage_string = "";
  std::string cookie_string = "";
  for (const auto& site : root->children()) {
    std::string title = base::UTF16ToUTF8(site->GetTitle());
    if (title != usage_host_)
      continue;
    int64_t size = site->InclusiveSize();
    if (size != 0)
      usage_string = base::UTF16ToUTF8(ui::FormatBytes(size));
    int num_cookies = site->NumberOfCookies();
    if (num_cookies != 0) {
      cookie_string = base::UTF16ToUTF8(l10n_util::GetPluralStringFUTF16(
          IDS_SETTINGS_SITE_SETTINGS_NUM_COOKIES, num_cookies));
    }
    break;
  }
  CallJavascriptFunction("settings.WebsiteUsagePrivateApi.returnUsageTotal",
                         base::Value(usage_host_), base::Value(usage_string),
                         base::Value(cookie_string));
}

#if defined(OS_CHROMEOS)
void SiteSettingsHandler::OnPrefEnableDrmChanged() {
  FireWebUIListener("prefEnableDrmChanged");
}
#endif

void SiteSettingsHandler::OnContentSettingChanged(
    const ContentSettingsPattern& primary_pattern,
    const ContentSettingsPattern& secondary_pattern,
    ContentSettingsType content_type,
    const std::string& resource_identifier) {
  if (!site_settings::HasRegisteredGroupName(content_type))
    return;

  if (primary_pattern.ToString().empty()) {
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
  if (primary_pattern == ContentSettingsPattern() &&
      secondary_pattern == ContentSettingsPattern() &&
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

void SiteSettingsHandler::OnChooserObjectPermissionChanged(
    ContentSettingsType guard_content_settings_type,
    ContentSettingsType data_content_settings_type) {
  if (!site_settings::HasRegisteredGroupName(guard_content_settings_type) ||
      !site_settings::HasRegisteredGroupName(data_content_settings_type)) {
    return;
  }

  FireWebUIListener("contentSettingChooserPermissionChanged",
                    base::Value(site_settings::ContentSettingsTypeToGroupName(
                        guard_content_settings_type)),
                    base::Value(site_settings::ContentSettingsTypeToGroupName(
                        data_content_settings_type)));
}

void SiteSettingsHandler::OnZoomLevelChanged(
    const content::HostZoomMap::ZoomLevelChange& change) {
  SendZoomLevels();
}

void SiteSettingsHandler::HandleFetchUsageTotal(const base::ListValue* args) {
  AllowJavascript();
  CHECK_EQ(1U, args->GetSize());
  std::string host;
  CHECK(args->GetString(0, &host));
  usage_host_ = host;

  update_site_details_ = true;
  if (cookies_tree_model_ && !send_sites_list_) {
    cookies_tree_model_->RemoveCookiesTreeObserver(this);
    cookies_tree_model_.reset();
  }
  EnsureCookiesTreeModelCreated();
}

void SiteSettingsHandler::HandleClearUsage(const base::ListValue* args) {
  CHECK_EQ(1U, args->GetSize());
  std::string origin;
  CHECK(args->GetString(0, &origin));
  GURL url(origin);
  if (!url.is_valid())
    return;
  AllowJavascript();
  for (const auto& node : cookies_tree_model_->GetRoot()->children()) {
    if (origin == node->GetDetailedInfo().origin.GetURL().spec()) {
      cookies_tree_model_->DeleteCookieNode(node.get());
      return;
    }
  }
}

void SiteSettingsHandler::HandleSetDefaultValueForContentType(
    const base::ListValue* args) {
  CHECK_EQ(2U, args->GetSize());
  std::string content_type;
  CHECK(args->GetString(0, &content_type));
  std::string setting;
  CHECK(args->GetString(1, &setting));
  ContentSetting default_setting;
  CHECK(content_settings::ContentSettingFromString(setting, &default_setting));
  ContentSettingsType type =
      site_settings::ContentSettingsTypeFromGroupName(content_type);

  Profile* profile = profile_;
#if defined(OS_CHROMEOS)
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
    const base::ListValue* args) {
  AllowJavascript();

  CHECK_EQ(2U, args->GetSize());
  const base::Value* callback_id;
  CHECK(args->Get(0, &callback_id));
  std::string type;
  CHECK(args->GetString(1, &type));

  ContentSettingsType content_type =
      site_settings::ContentSettingsTypeFromGroupName(type);
  HostContentSettingsMap* map =
      HostContentSettingsMapFactory::GetForProfile(profile_);

  base::DictionaryValue category;
  site_settings::GetContentCategorySetting(map, content_type, &category);
  ResolveJavascriptCallback(*callback_id, category);
}

void SiteSettingsHandler::HandleGetAllSites(const base::ListValue* args) {
  AllowJavascript();

  CHECK_EQ(2U, args->GetSize());
  const base::Value* callback_id;
  CHECK(args->Get(0, &callback_id));
  const base::ListValue* types;
  CHECK(args->GetList(1, &types));

  all_sites_map_.clear();
  origin_permission_set_.clear();
  // Convert |types| to a list of ContentSettingsTypes.
  std::vector<ContentSettingsType> content_types;
  for (size_t i = 0; i < types->GetSize(); ++i) {
    std::string type;
    types->GetString(i, &type);
    content_types.push_back(
        site_settings::ContentSettingsTypeFromGroupName(type));
  }

  // Incognito contains incognito content settings plus non-incognito content
  // settings. Thus if it exists, just get exceptions for the incognito profile.
  Profile* profile = profile_;
  if (profile_->HasOffTheRecordProfile() &&
      profile_->GetOffTheRecordProfile() != profile_) {
    profile = profile_->GetOffTheRecordProfile();
  }
  DCHECK(profile);
  HostContentSettingsMap* map =
      HostContentSettingsMapFactory::GetForProfile(profile);

  // Retrieve a list of embargoed settings to check separately. This ensures
  // that only settings included in |content_types| will be listed in all sites.
  ContentSettingsForOneType embargo_settings;
  map->GetSettingsForOneType(ContentSettingsType::PERMISSION_AUTOBLOCKER_DATA,
                             std::string(), &embargo_settings);
  PermissionManager* permission_manager = PermissionManager::Get(profile);
  for (const ContentSettingPatternSource& e : embargo_settings) {
    for (ContentSettingsType content_type : content_types) {
      if (PermissionUtil::IsPermission(content_type)) {
        const GURL url(e.primary_pattern.ToString());
        // Add |url| to the set if there are any embargo settings.
        PermissionResult result =
            permission_manager->GetPermissionStatus(content_type, url, url);
        if (result.source == PermissionStatusSource::MULTIPLE_DISMISSALS ||
            result.source == PermissionStatusSource::MULTIPLE_IGNORES) {
          CreateOrAppendSiteGroupEntry(&all_sites_map_, url);
          origin_permission_set_.insert(url.spec());
          break;
        }
      }
    }
  }

  // Convert |types| to a list of ContentSettingsTypes.
  for (ContentSettingsType content_type : content_types) {
    ContentSettingsForOneType entries;
    map->GetSettingsForOneType(content_type, std::string(), &entries);
    for (const ContentSettingPatternSource& e : entries) {
      if (PatternAppliesToSingleOrigin(e)) {
        CreateOrAppendSiteGroupEntry(&all_sites_map_,
                                     GURL(e.primary_pattern.ToString()));
        origin_permission_set_.insert(
            GURL(e.primary_pattern.ToString()).spec());
      }
    }
  }

  // Recreate the cookies tree model to refresh the usage information.
  // This happens in the background and will call TreeModelEndBatch() when
  // finished. At that point we send usage data to the page.
  if (cookies_tree_model_)
    cookies_tree_model_->RemoveCookiesTreeObserver(this);
  cookies_tree_model_.reset();
  EnsureCookiesTreeModelCreated();

  base::Value result(base::Value::Type::LIST);

  // Respond with currently available data.
  ConvertSiteGroupMapToListValue(all_sites_map_, origin_permission_set_,
                                 &result, profile, app_registrar_);

  LogAllSitesAction(AllSitesAction::kLoadPage);

  send_sites_list_ = true;

  ResolveJavascriptCallback(*callback_id, result);
}

base::Value SiteSettingsHandler::PopulateCookiesAndUsageData(Profile* profile) {
  std::map<std::string, int64_t> origin_size_map;
  std::map<std::string, int> origin_cookie_map;
  base::Value list_value(base::Value::Type::LIST);

  GetOriginStorage(&all_sites_map_, &origin_size_map);
  GetOriginCookies(&all_sites_map_, &origin_cookie_map);
  ConvertSiteGroupMapToListValue(all_sites_map_, origin_permission_set_,
                                 &list_value, profile, app_registrar_);

  // Merge the origin usage and cookies number into |list_value|.
  for (base::Value& site_group : list_value.GetList()) {
    base::Value* origin_list = site_group.FindKey(kOriginList);
    int cookie_num = 0;
    const std::string& etld_plus1 =
        site_group.FindKey(kEffectiveTopLevelDomainPlus1Name)->GetString();
    const auto& etld_plus1_cookie_num_it = origin_cookie_map.find(etld_plus1);
    // Add the number of eTLD+1 scoped cookies.
    if (etld_plus1_cookie_num_it != origin_cookie_map.end())
      cookie_num = etld_plus1_cookie_num_it->second;
    // Iterate over the origins for the ETLD+1, and set their usage and cookie
    // numbers.
    for (base::Value& origin_info : origin_list->GetList()) {
      const std::string& origin = origin_info.FindKey("origin")->GetString();
      const auto& size_info_it = origin_size_map.find(origin);
      if (size_info_it != origin_size_map.end())
        origin_info.SetKey("usage", base::Value(double(size_info_it->second)));
      const auto& origin_cookie_num_it =
          origin_cookie_map.find(GURL(origin).host());
      if (origin_cookie_num_it != origin_cookie_map.end()) {
        origin_info.SetKey(kNumCookies,
                           base::Value(origin_cookie_num_it->second));
        // Add cookies numbers for origins that isn't an eTLD+1.
        if (GURL(origin).host() != etld_plus1)
          cookie_num += origin_cookie_num_it->second;
      }
    }
    site_group.SetKey(kNumCookies, base::Value(cookie_num));
  }
  return list_value;
}

void SiteSettingsHandler::OnStorageFetched() {
  AllowJavascript();
  FireWebUIListener("onStorageListFetched",
                    PopulateCookiesAndUsageData(profile_));
}

void SiteSettingsHandler::HandleGetFormattedBytes(const base::ListValue* args) {
  AllowJavascript();

  CHECK_EQ(2U, args->GetSize());
  const base::Value* callback_id;
  CHECK(args->Get(0, &callback_id));
  double num_bytes;
  CHECK(args->GetDouble(1, &num_bytes));

  const base::string16 string = ui::FormatBytes(int64_t(num_bytes));
  ResolveJavascriptCallback(*callback_id, base::Value(string));
}

void SiteSettingsHandler::HandleGetExceptionList(const base::ListValue* args) {
  AllowJavascript();

  CHECK_EQ(2U, args->GetSize());
  const base::Value* callback_id;
  CHECK(args->Get(0, &callback_id));
  std::string type;
  CHECK(args->GetString(1, &type));
  ContentSettingsType content_type =
      site_settings::ContentSettingsTypeFromGroupName(type);

  std::unique_ptr<base::ListValue> exceptions(new base::ListValue);

  HostContentSettingsMap* map =
      HostContentSettingsMapFactory::GetForProfile(profile_);
  const auto* extension_registry = extensions::ExtensionRegistry::Get(profile_);
  AddExceptionsGrantedByHostedApps(profile_, APIPermissionFromGroupName(type),
                                   exceptions.get());
  site_settings::GetExceptionsFromHostContentSettingsMap(
      map, content_type, extension_registry, web_ui(), /*incognito=*/false,
      /*filter=*/nullptr, exceptions.get());

  Profile* incognito = profile_->HasOffTheRecordProfile()
                           ? profile_->GetOffTheRecordProfile()
                           : nullptr;
  // On Chrome OS in Guest mode the incognito profile is the primary profile,
  // so do not fetch an extra copy of the same exceptions.
  if (incognito && incognito != profile_) {
    map = HostContentSettingsMapFactory::GetForProfile(incognito);
    extension_registry = extensions::ExtensionRegistry::Get(incognito);
    site_settings::GetExceptionsFromHostContentSettingsMap(
        map, content_type, extension_registry, web_ui(), /*incognito=*/true,
        /*filter=*/nullptr, exceptions.get());
  }

  ResolveJavascriptCallback(*callback_id, *exceptions.get());
}

void SiteSettingsHandler::HandleGetChooserExceptionList(
    const base::ListValue* args) {
  AllowJavascript();

  CHECK_EQ(2U, args->GetSize());
  const base::Value* callback_id;
  CHECK(args->Get(0, &callback_id));
  std::string type;
  CHECK(args->GetString(1, &type));
  const site_settings::ChooserTypeNameEntry* chooser_type =
      site_settings::ChooserTypeFromGroupName(type);
  CHECK(chooser_type);

  base::Value exceptions = site_settings::GetChooserExceptionListFromProfile(
      profile_, *chooser_type);
  ResolveJavascriptCallback(*callback_id, std::move(exceptions));
}

void SiteSettingsHandler::HandleGetOriginPermissions(
    const base::ListValue* args) {
  AllowJavascript();

  CHECK_EQ(3U, args->GetSize());
  const base::Value* callback_id;
  CHECK(args->Get(0, &callback_id));
  std::string origin;
  CHECK(args->GetString(1, &origin));
  const base::ListValue* types;
  CHECK(args->GetList(2, &types));

  // Note: Invalid URLs will just result in default settings being shown.
  const GURL origin_url(origin);
  auto exceptions = std::make_unique<base::ListValue>();
  for (size_t i = 0; i < types->GetSize(); ++i) {
    std::string type;
    types->GetString(i, &type);
    ContentSettingsType content_type =
        site_settings::ContentSettingsTypeFromGroupName(type);
    HostContentSettingsMap* map =
        HostContentSettingsMapFactory::GetForProfile(profile_);
    const auto* extension_registry =
        extensions::ExtensionRegistry::Get(profile_);

    std::string source_string, display_name;
    ContentSetting content_setting = site_settings::GetContentSettingForOrigin(
        profile_, map, origin_url, content_type, &source_string,
        extension_registry, &display_name);
    std::string content_setting_string =
        content_settings::ContentSettingToString(content_setting);

    auto raw_site_exception = std::make_unique<base::DictionaryValue>();
    raw_site_exception->SetString(site_settings::kEmbeddingOrigin, origin);
    raw_site_exception->SetBoolean(site_settings::kIncognito,
                                   profile_->IsOffTheRecord());
    raw_site_exception->SetString(site_settings::kOrigin, origin);
    raw_site_exception->SetString(site_settings::kDisplayName, display_name);
    raw_site_exception->SetString(site_settings::kSetting,
                                  content_setting_string);
    raw_site_exception->SetString(site_settings::kSource, source_string);
    exceptions->Append(std::move(raw_site_exception));
  }

  ResolveJavascriptCallback(*callback_id, *exceptions);
}

void SiteSettingsHandler::HandleSetOriginPermissions(
    const base::ListValue* args) {
  CHECK_EQ(3U, args->GetSize());
  std::string origin_string;
  CHECK(args->GetString(0, &origin_string));
  const base::ListValue* types;
  CHECK(args->GetList(1, &types));
  std::string value;
  CHECK(args->GetString(2, &value));

  const GURL origin(origin_string);
  if (!origin.is_valid())
    return;

  ContentSetting setting;
  CHECK(content_settings::ContentSettingFromString(value, &setting));
  for (size_t i = 0; i < types->GetSize(); ++i) {
    std::string type;
    types->GetString(i, &type);

    ContentSettingsType content_type =
        site_settings::ContentSettingsTypeFromGroupName(type);
    HostContentSettingsMap* map =
        HostContentSettingsMapFactory::GetForProfile(profile_);

    PermissionUtil::ScopedRevocationReporter scoped_revocation_reporter(
        profile_, origin, origin, content_type,
        PermissionSourceUI::SITE_SETTINGS);

    // Clear any existing embargo status if the new setting isn't block.
    if (setting != CONTENT_SETTING_BLOCK) {
      PermissionDecisionAutoBlocker::GetForProfile(profile_)
          ->RemoveEmbargoByUrl(origin, content_type);
    }
    map->SetContentSettingDefaultScope(origin, origin, content_type,
                                       std::string(), setting);
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
    WebSiteSettingsUmaUtil::LogPermissionChange(content_type, setting);
  }

  // Show an infobar reminding the user to reload tabs where their site
  // permissions have been updated.
  for (auto* it : *BrowserList::GetInstance()) {
    TabStripModel* tab_strip = it->tab_strip_model();
    for (int i = 0; i < tab_strip->count(); ++i) {
      content::WebContents* web_contents = tab_strip->GetWebContentsAt(i);
      GURL tab_url = web_contents->GetLastCommittedURL();
      if (url::IsSameOriginWith(origin, tab_url)) {
        InfoBarService* infobar_service =
            InfoBarService::FromWebContents(web_contents);
        PageInfoInfoBarDelegate::Create(infobar_service);
      }
    }
  }
}

void SiteSettingsHandler::HandleClearFlashPref(const base::ListValue* args) {
  CHECK_EQ(1U, args->GetSize());
  std::string origin_string;
  CHECK(args->GetString(0, &origin_string));

  HostContentSettingsMap* map =
      HostContentSettingsMapFactory::GetForProfile(profile_);
  const GURL origin(origin_string);
  map->SetWebsiteSettingDefaultScope(origin, origin,
                                     ContentSettingsType::PLUGINS_DATA,
                                     std::string(), nullptr);
}

void SiteSettingsHandler::HandleResetCategoryPermissionForPattern(
    const base::ListValue* args) {
  CHECK_EQ(4U, args->GetSize());
  std::string primary_pattern_string;
  CHECK(args->GetString(0, &primary_pattern_string));
  std::string secondary_pattern_string;
  CHECK(args->GetString(1, &secondary_pattern_string));
  std::string type;
  CHECK(args->GetString(2, &type));
  bool incognito;
  CHECK(args->GetBoolean(3, &incognito));

  ContentSettingsType content_type =
      site_settings::ContentSettingsTypeFromGroupName(type);

  Profile* profile = nullptr;
  if (incognito) {
    if (!profile_->HasOffTheRecordProfile())
      return;
    profile = profile_->GetOffTheRecordProfile();
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
  PermissionUtil::ScopedRevocationReporter scoped_revocation_reporter(
      profile, primary_pattern, secondary_pattern, content_type,
      PermissionSourceUI::SITE_SETTINGS);

  map->SetContentSettingCustomScope(primary_pattern, secondary_pattern,
                                    content_type, "", CONTENT_SETTING_DEFAULT);

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
  WebSiteSettingsUmaUtil::LogPermissionChange(
      content_type, ContentSetting::CONTENT_SETTING_DEFAULT);
}

void SiteSettingsHandler::HandleSetCategoryPermissionForPattern(
    const base::ListValue* args) {
  CHECK_EQ(5U, args->GetSize());
  std::string primary_pattern_string;
  CHECK(args->GetString(0, &primary_pattern_string));
  std::string secondary_pattern_string;
  CHECK(args->GetString(1, &secondary_pattern_string));
  std::string type;
  CHECK(args->GetString(2, &type));
  std::string value;
  CHECK(args->GetString(3, &value));
  bool incognito;
  CHECK(args->GetBoolean(4, &incognito));

  ContentSettingsType content_type =
      site_settings::ContentSettingsTypeFromGroupName(type);
  ContentSetting setting;
  CHECK(content_settings::ContentSettingFromString(value, &setting));

  Profile* profile = nullptr;
  if (incognito) {
    if (!profile_->HasOffTheRecordProfile())
      return;
    profile = profile_->GetOffTheRecordProfile();
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

  PermissionUtil::ScopedRevocationReporter scoped_revocation_reporter(
      profile, primary_pattern, secondary_pattern, content_type,
      PermissionSourceUI::SITE_SETTINGS);

  map->SetContentSettingCustomScope(primary_pattern, secondary_pattern,
                                    content_type, "", setting);

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
  WebSiteSettingsUmaUtil::LogPermissionChange(content_type, setting);
}

void SiteSettingsHandler::HandleResetChooserExceptionForSite(
    const base::ListValue* args) {
  CHECK_EQ(4U, args->GetSize());

  std::string chooser_type_str;
  CHECK(args->GetString(0, &chooser_type_str));
  const site_settings::ChooserTypeNameEntry* chooser_type =
      site_settings::ChooserTypeFromGroupName(chooser_type_str);
  CHECK(chooser_type);

  std::string origin_str;
  CHECK(args->GetString(1, &origin_str));
  GURL requesting_origin(origin_str);
  CHECK(requesting_origin.is_valid());

  std::string embedding_origin_str;
  CHECK(args->GetString(2, &embedding_origin_str));
  GURL embedding_origin(embedding_origin_str);
  CHECK(embedding_origin.is_valid());

  ChooserContextBase* chooser_context = chooser_type->get_context(profile_);
  chooser_context->RevokeObjectPermission(
      url::Origin::Create(requesting_origin),
      url::Origin::Create(embedding_origin), args->GetList()[3]);
}

void SiteSettingsHandler::HandleIsOriginValid(const base::ListValue* args) {
  AllowJavascript();
  CHECK_EQ(2U, args->GetSize());
  const base::Value* callback_id;
  CHECK(args->Get(0, &callback_id));
  std::string origin_string;
  CHECK(args->GetString(1, &origin_string));

  ResolveJavascriptCallback(*callback_id,
                            base::Value(GURL(origin_string).is_valid()));
}

void SiteSettingsHandler::HandleIsPatternValidForType(
    const base::ListValue* args) {
  AllowJavascript();
  CHECK_EQ(3U, args->GetSize());
  const base::Value* callback_id;
  CHECK(args->Get(0, &callback_id));
  std::string pattern_string;
  CHECK(args->GetString(1, &pattern_string));
  std::string type;
  CHECK(args->GetString(2, &type));

  std::string reason = "";
  bool is_valid =
      IsPatternValidForType(pattern_string, type, profile_, &reason);

  base::Value return_value(base::Value::Type::DICTIONARY);
  return_value.SetKey(kIsValidKey, base::Value(is_valid));
  return_value.SetKey(kReasonKey, base::Value(std::move(reason)));
  ResolveJavascriptCallback(*callback_id, return_value);
}

void SiteSettingsHandler::HandleUpdateIncognitoStatus(
    const base::ListValue* args) {
  AllowJavascript();
  FireWebUIListener("onIncognitoStatusChanged",
                    base::Value(profile_->HasOffTheRecordProfile()));
}

void SiteSettingsHandler::HandleFetchZoomLevels(const base::ListValue* args) {
  AllowJavascript();
  SendZoomLevels();
}

void SiteSettingsHandler::SendZoomLevels() {
  if (!IsJavascriptAllowed())
    return;

  base::ListValue zoom_levels_exceptions;

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
    std::unique_ptr<base::DictionaryValue> exception(new base::DictionaryValue);
    switch (zoom_level.mode) {
      case content::HostZoomMap::ZOOM_CHANGED_FOR_HOST: {
        std::string host = zoom_level.host;
        if (host == content::kUnreachableWebDataURL) {
          host =
              l10n_util::GetStringUTF8(IDS_ZOOMLEVELS_CHROME_ERROR_PAGES_LABEL);
        }
        exception->SetString(site_settings::kOrigin, host);

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
        exception->SetString(site_settings::kDisplayName, display_name);
        exception->SetString(site_settings::kOriginForFavicon,
                             origin_for_favicon);
        break;
      }
      case content::HostZoomMap::ZOOM_CHANGED_FOR_SCHEME_AND_HOST:
        // These are not stored in preferences and get cleared on next browser
        // start. Therefore, we don't care for them.
        continue;
      case content::HostZoomMap::PAGE_SCALE_IS_ONE_CHANGED:
        continue;
      case content::HostZoomMap::ZOOM_CHANGED_TEMPORARY_ZOOM:
        NOTREACHED();
    }

    std::string setting_string =
        content_settings::ContentSettingToString(CONTENT_SETTING_DEFAULT);
    DCHECK(!setting_string.empty());

    exception->SetString(site_settings::kSetting, setting_string);

    // Calculate the zoom percent from the factor. Round up to the nearest whole
    // number.
    int zoom_percent = static_cast<int>(
        blink::PageZoomLevelToZoomFactor(zoom_level.zoom_level) * 100 + 0.5);
    exception->SetString(kZoom, base::FormatPercent(zoom_percent));
    exception->SetString(site_settings::kSource,
                         site_settings::SiteSettingSourceToString(
                             site_settings::SiteSettingSource::kPreference));
    // Append the new entry to the list and map.
    zoom_levels_exceptions.Append(std::move(exception));
  }

  FireWebUIListener("onZoomLevelsChanged", zoom_levels_exceptions);
}

void SiteSettingsHandler::HandleRemoveZoomLevel(const base::ListValue* args) {
  CHECK_EQ(1U, args->GetSize());

  std::string origin;
  CHECK(args->GetString(0, &origin));

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
    const base::ListValue* args) {
  AllowJavascript();
  SendBlockAutoplayStatus();
}

void SiteSettingsHandler::SendBlockAutoplayStatus() {
  if (!IsJavascriptAllowed())
    return;

  base::DictionaryValue status;

  // Whether the block autoplay toggle should be checked.
  base::DictionaryValue pref;
  pref.SetKey(
      "value",
      base::Value(
          UnifiedAutoplayConfig::ShouldBlockAutoplay(profile_) &&
          UnifiedAutoplayConfig::IsBlockAutoplayUserModifiable(profile_)));
  status.SetKey("pref", std::move(pref));

  // Whether the block autoplay toggle should be enabled.
  status.SetKey(
      "enabled",
      base::Value(
          UnifiedAutoplayConfig::IsBlockAutoplayUserModifiable(profile_)));

  FireWebUIListener("onBlockAutoplayStatusChanged", status);
}

void SiteSettingsHandler::HandleSetBlockAutoplayEnabled(
    const base::ListValue* args) {
  AllowJavascript();

  if (!UnifiedAutoplayConfig::IsBlockAutoplayUserModifiable(profile_))
    return;

  CHECK_EQ(1U, args->GetSize());
  bool value;
  CHECK(args->GetBoolean(0, &value));

  profile_->GetPrefs()->SetBoolean(prefs::kBlockAutoplayEnabled, value);
}

void SiteSettingsHandler::EnsureCookiesTreeModelCreated() {
  if (cookies_tree_model_)
    return;
  cookies_tree_model_ = CookiesTreeModel::CreateForProfile(profile_);
  cookies_tree_model_->AddCookiesTreeObserver(this);
}

void SiteSettingsHandler::ObserveSourcesForProfile(Profile* profile) {
  auto* map = HostContentSettingsMapFactory::GetForProfile(profile);
  if (!observer_.IsObserving(map))
    observer_.Add(map);

  auto* usb_context = UsbChooserContextFactory::GetForProfile(profile);
  if (!chooser_observer_.IsObserving(usb_context))
    chooser_observer_.Add(usb_context);

  auto* serial_context = SerialChooserContextFactory::GetForProfile(profile);
  if (!chooser_observer_.IsObserving(serial_context))
    chooser_observer_.Add(serial_context);

  observed_profiles_.Add(profile);
}

void SiteSettingsHandler::StopObservingSourcesForProfile(Profile* profile) {
  auto* map = HostContentSettingsMapFactory::GetForProfile(profile);
  if (observer_.IsObserving(map))
    observer_.Remove(map);

  auto* usb_context = UsbChooserContextFactory::GetForProfile(profile);
  if (chooser_observer_.IsObserving(usb_context))
    chooser_observer_.Remove(usb_context);

  auto* serial_context = SerialChooserContextFactory::GetForProfile(profile);
  if (chooser_observer_.IsObserving(serial_context))
    chooser_observer_.Remove(serial_context);

  observed_profiles_.Remove(profile);
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

void SiteSettingsHandler::TreeModelEndBatch(CookiesTreeModel* model) {
  // The WebUI may have shut down before we get the data.
  if (!IsJavascriptAllowed())
    return;
  if (send_sites_list_)
    OnStorageFetched();
  if (update_site_details_)
    OnGetUsageInfo();
  send_sites_list_ = false;
  update_site_details_ = false;
}

void SiteSettingsHandler::GetOriginStorage(
    std::map<std::string, std::set<std::string>>* all_sites_map,
    std::map<std::string, int64_t>* origin_size_map) {
  CHECK(cookies_tree_model_.get());

  for (const auto& site : cookies_tree_model_->GetRoot()->children()) {
    int64_t size = site->InclusiveSize();
    if (size == 0)
      continue;
    UpdateDataFromCookiesTree(all_sites_map, origin_size_map,
                              site->GetDetailedInfo().origin.GetURL(), size);
  }
}

void SiteSettingsHandler::GetOriginCookies(
    std::map<std::string, std::set<std::string>>* all_sites_map,
    std::map<std::string, int>* origin_cookie_map) {
  CHECK(cookies_tree_model_.get());
  // Get sites that don't have data but have cookies.
  for (const auto& site : cookies_tree_model_->GetRoot()->children()) {
    GURL url = site->GetDetailedInfo().origin.GetURL();
    (*origin_cookie_map)[url.host()] = site->NumberOfCookies();
    CreateOrAppendSiteGroupEntry(all_sites_map, url,
                                 /*url_is_origin_with_cookies = */ true);
  }
}

void SiteSettingsHandler::HandleClearEtldPlus1DataAndCookies(
    const base::ListValue* args) {
  CHECK_EQ(1U, args->GetSize());
  std::string etld_plus1_string;
  CHECK(args->GetString(0, &etld_plus1_string));

  AllowJavascript();
  CookieTreeNode* parent = cookies_tree_model_->GetRoot();

  // Find all the nodes that contain the given etld+1.
  std::vector<CookieTreeNode*> nodes_to_delete;
  for (const auto& node : parent->children()) {
    std::string cookie_node_etld_plus1 =
        net::registry_controlled_domains::GetDomainAndRegistry(
            base::UTF16ToUTF8(node->GetTitle()),
            net::registry_controlled_domains::INCLUDE_PRIVATE_REGISTRIES);
    if (etld_plus1_string == cookie_node_etld_plus1)
      nodes_to_delete.push_back(node.get());
  }
  for (auto* node : nodes_to_delete)
    cookies_tree_model_->DeleteCookieNode(node);

  LogAllSitesAction(AllSitesAction::kClearData);
}

void SiteSettingsHandler::HandleRecordAction(const base::ListValue* args) {
  CHECK_EQ(1U, args->GetSize());
  int action;
  CHECK(args->GetInteger(0, &action));
  DCHECK_LE(action, static_cast<int>(AllSitesAction::kMaxValue));
  DCHECK_GE(action, static_cast<int>(AllSitesAction::kLoadPage));

  LogAllSitesAction(static_cast<AllSitesAction>(action));
}

void SiteSettingsHandler::SetCookiesTreeModelForTesting(
    std::unique_ptr<CookiesTreeModel> cookies_tree_model) {
  cookies_tree_model_ = std::move(cookies_tree_model);
}

void SiteSettingsHandler::ClearAllSitesMapForTesting() {
  all_sites_map_.clear();
}
}  // namespace settings
