// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/webui/settings/site_settings_handler.h"

#include <algorithm>
#include <map>
#include <memory>
#include <set>
#include <string>
#include <utility>

#include "base/barrier_closure.h"
#include "base/bind.h"
#include "base/i18n/number_formatting.h"
#include "base/macros.h"
#include "base/metrics/user_metrics.h"
#include "base/values.h"
#include "chrome/browser/chrome_notification_types.h"
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
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/browser/ui/browser_list.h"
#include "chrome/browser/ui/page_info/page_info_infobar_delegate.h"
#include "chrome/browser/ui/tabs/tab_strip_model.h"
#include "chrome/browser/ui/webui/site_settings_helper.h"
#include "chrome/common/extensions/manifest_handlers/app_launch_info.h"
#include "chrome/common/pref_names.h"
#include "chrome/grit/generated_resources.h"
#include "components/content_settings/core/browser/host_content_settings_map.h"
#include "components/content_settings/core/common/content_settings_types.h"
#include "components/content_settings/core/common/content_settings_utils.h"
#include "components/crx_file/id_util.h"
#include "components/prefs/pref_change_registrar.h"
#include "components/prefs/pref_service.h"
#include "content/public/browser/browser_thread.h"
#include "content/public/browser/notification_service.h"
#include "content/public/browser/web_contents.h"
#include "content/public/browser/web_ui.h"
#include "content/public/common/page_zoom.h"
#include "content/public/common/url_constants.h"
#include "extensions/browser/extension_registry.h"
#include "extensions/common/permissions/api_permission.h"
#include "extensions/common/permissions/permissions_data.h"
#include "storage/browser/quota/quota_manager.h"
#include "third_party/blink/public/mojom/quota/quota_types.mojom.h"
#include "ui/base/l10n/l10n_util.h"
#include "ui/base/text/bytes_formatting.h"

#if defined(OS_CHROMEOS)
#include "components/user_manager/user_manager.h"
#endif

namespace settings {

namespace {

constexpr char kEffectiveTopLevelDomainPlus1Name[] = "etldPlus1";
constexpr char kOriginList[] = "origins";
constexpr char kNumCookies[] = "numCookies";
constexpr char kZoom[] = "zoom";

// Return an appropriate API Permission ID for the given string name.
extensions::APIPermission::APIPermission::ID APIPermissionFromGroupName(
    std::string type) {
  // Once there are more than two groups to consider, this should be changed to
  // something better than if's.

  if (site_settings::ContentSettingsTypeFromGroupName(type) ==
      CONTENT_SETTINGS_TYPE_GEOLOCATION)
    return extensions::APIPermission::APIPermission::kGeolocation;

  if (site_settings::ContentSettingsTypeFromGroupName(type) ==
      CONTENT_SETTINGS_TYPE_NOTIFICATIONS)
    return extensions::APIPermission::APIPermission::kNotifications;

  return extensions::APIPermission::APIPermission::kInvalid;
}

// Asks the |profile| for hosted apps which have the |permission| set, and
// adds their web extent and launch URL to the |exceptions| list.
void AddExceptionsGrantedByHostedApps(content::BrowserContext* context,
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
      site_settings::AddExceptionForHostedApp(
          url_pattern, *extension->get(), exceptions);
    }
    // Retrieve the launch URL.
    GURL launch_url =
        extensions::AppLaunchInfo::GetLaunchWebURL(extension->get());
    // Skip adding the launch URL if it is part of the web extent.
    if (web_extent.MatchesURL(launch_url))
      continue;
    site_settings::AddExceptionForHostedApp(
        launch_url.spec(), *extension->get(), exceptions);
  }
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
void CreateOrAppendSiteGroupEntry(
    std::map<std::string, std::set<std::string>>* site_group_map,
    const GURL& url) {
  std::string etld_plus1_string =
      net::registry_controlled_domains::GetDomainAndRegistry(
          url, net::registry_controlled_domains::INCLUDE_PRIVATE_REGISTRIES);
  auto entry = site_group_map->find(etld_plus1_string);
  if (entry == site_group_map->end()) {
    site_group_map->emplace(etld_plus1_string,
                            std::set<std::string>({url.spec()}));
  } else {
    entry->second.insert(url.spec());
  }
}

// Converts a given |site_group_map| to a list of base::DictionaryValues, adding
// the site engagement score for each origin.
void ConvertSiteGroupMapToListValue(
    const std::map<std::string, std::set<std::string>>& site_group_map,
    base::Value* list_value,
    Profile* profile) {
  DCHECK_EQ(base::Value::Type::LIST, list_value->type());
  DCHECK(profile);
  SiteEngagementService* engagement_service =
      SiteEngagementService::Get(profile);
  for (const auto& entry : site_group_map) {
    // eTLD+1 is the effective top level domain + 1.
    base::Value site_group(base::Value::Type::DICTIONARY);
    site_group.SetKey(kEffectiveTopLevelDomainPlus1Name,
                      base::Value(entry.first));
    base::Value origin_list(base::Value::Type::LIST);
    for (const std::string& origin : entry.second) {
      base::Value origin_object(base::Value::Type::DICTIONARY);
      origin_object.SetKey("origin", base::Value(origin));
      origin_object.SetKey(
          "engagement",
          base::Value(engagement_service->GetScore(GURL(origin))));
      origin_object.SetKey("usage", base::Value(0));
      origin_list.GetList().emplace_back(std::move(origin_object));
    }
    site_group.SetKey(kNumCookies, base::Value(0));
    site_group.SetKey(kOriginList, std::move(origin_list));
    list_value->GetList().push_back(std::move(site_group));
  }
}

}  // namespace

SiteSettingsHandler::SiteSettingsHandler(Profile* profile)
    : profile_(profile),
      observer_(this),
      pref_change_registrar_(nullptr),
      local_storage_helper_(nullptr) {}

SiteSettingsHandler::~SiteSettingsHandler() {
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
      "fetchUsbDevices",
      base::BindRepeating(&SiteSettingsHandler::HandleFetchUsbDevices,
                          base::Unretained(this)));
  web_ui()->RegisterMessageCallback(
      "removeUsbDevice",
      base::BindRepeating(&SiteSettingsHandler::HandleRemoveUsbDevice,
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
      "isOriginValid",
      base::BindRepeating(&SiteSettingsHandler::HandleIsOriginValid,
                          base::Unretained(this)));
  web_ui()->RegisterMessageCallback(
      "isPatternValid",
      base::BindRepeating(&SiteSettingsHandler::HandleIsPatternValid,
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
}

void SiteSettingsHandler::OnJavascriptAllowed() {
  observer_.Add(HostContentSettingsMapFactory::GetForProfile(profile_));
  if (profile_->HasOffTheRecordProfile()) {
    auto* map = HostContentSettingsMapFactory::GetForProfile(
        profile_->GetOffTheRecordProfile());
    if (!observer_.IsObserving(map))
      observer_.Add(map);
  }

  notification_registrar_.Add(
      this, chrome::NOTIFICATION_PROFILE_CREATED,
      content::NotificationService::AllSources());
  notification_registrar_.Add(
      this, chrome::NOTIFICATION_PROFILE_DESTROYED,
      content::NotificationService::AllSources());

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
  notification_registrar_.RemoveAll();
  host_zoom_map_subscription_.reset();
  pref_change_registrar_->Remove(prefs::kBlockAutoplayEnabled);
#if defined(OS_CHROMEOS)
  pref_change_registrar_->Remove(prefs::kEnableDRM);
#endif
}

void SiteSettingsHandler::OnGetUsageInfo(
    const storage::UsageInfoEntries& entries) {
  DCHECK_CURRENTLY_ON(content::BrowserThread::UI);

  for (const auto& entry : entries) {
    if (entry.usage <= 0) continue;
    if (entry.host == usage_host_) {
      CallJavascriptFunction("settings.WebsiteUsagePrivateApi.returnUsageTotal",
                             base::Value(entry.host),
                             base::Value(ui::FormatBytes(entry.usage)),
                             base::Value(static_cast<int>(entry.type)));
      return;
    }
  }
}

void SiteSettingsHandler::OnStorageCleared(base::OnceClosure callback,
                                           blink::mojom::QuotaStatusCode code) {
  if (code == blink::mojom::QuotaStatusCode::kOk) {
    std::move(callback).Run();
  }
}

void SiteSettingsHandler::OnUsageCleared() {
  CallJavascriptFunction("settings.WebsiteUsagePrivateApi.onUsageCleared",
                         base::Value(clearing_origin_));
}

#if defined(OS_CHROMEOS)
void SiteSettingsHandler::OnPrefEnableDrmChanged() {
  CallJavascriptFunction("cr.webUIListenerCallback",
                         base::Value("prefEnableDrmChanged"));
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
    CallJavascriptFunction(
        "cr.webUIListenerCallback",
        base::Value("contentSettingCategoryChanged"),
        base::Value(
            site_settings::ContentSettingsTypeToGroupName(content_type)));
  } else {
    CallJavascriptFunction(
        "cr.webUIListenerCallback",
        base::Value("contentSettingSitePermissionChanged"),
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
      content_type == CONTENT_SETTINGS_TYPE_SOUND) {
    SendBlockAutoplayStatus();
  }
}

void SiteSettingsHandler::Observe(
    int type,
    const content::NotificationSource& source,
    const content::NotificationDetails& details) {
  switch (type) {
    case chrome::NOTIFICATION_PROFILE_DESTROYED: {
      Profile* profile = content::Source<Profile>(source).ptr();
      if (!profile_->IsSameProfile(profile))
        break;
      SendIncognitoStatus(profile, /*was_destroyed=*/ true);

      HostContentSettingsMap* settings_map =
          HostContentSettingsMapFactory::GetForProfile(profile);
      if (profile->IsOffTheRecord() &&
          observer_.IsObserving(settings_map)) {
        observer_.Remove(settings_map);
      }

      break;
    }

    case chrome::NOTIFICATION_PROFILE_CREATED: {
      Profile* profile = content::Source<Profile>(source).ptr();
      if (!profile_->IsSameProfile(profile))
        break;
      SendIncognitoStatus(profile, /*was_destroyed=*/ false);

      observer_.Add(HostContentSettingsMapFactory::GetForProfile(profile));
      break;
    }
  }
}

void SiteSettingsHandler::OnZoomLevelChanged(
    const content::HostZoomMap::ZoomLevelChange& change) {
  SendZoomLevels();
}

void SiteSettingsHandler::HandleFetchUsageTotal(
    const base::ListValue* args) {
  AllowJavascript();

  CHECK_EQ(1U, args->GetSize());
  std::string host;
  CHECK(args->GetString(0, &host));
  usage_host_ = host;

  scoped_refptr<StorageInfoFetcher> storage_info_fetcher
      = new StorageInfoFetcher(profile_);
  storage_info_fetcher->FetchStorageInfo(
      base::Bind(&SiteSettingsHandler::OnGetUsageInfo, base::Unretained(this)));
}

void SiteSettingsHandler::HandleClearUsage(
    const base::ListValue* args) {
  CHECK_EQ(2U, args->GetSize());
  std::string origin;
  CHECK(args->GetString(0, &origin));
  double storage_type;
  CHECK(args->GetDouble(1, &storage_type));

  GURL url(origin);
  if (url.is_valid()) {
    clearing_origin_ = origin;

    // Call OnUsageCleared when StorageInfoFetcher::ClearStorage and
    // BrowsingDataLocalStorageHelper::DeleteOrigin are done.
    base::RepeatingClosure barrier = base::BarrierClosure(
        2, base::BindOnce(&SiteSettingsHandler::OnUsageCleared,
                          base::Unretained(this)));

    // Start by clearing the storage data asynchronously.
    scoped_refptr<StorageInfoFetcher> storage_info_fetcher
        = new StorageInfoFetcher(profile_);
    storage_info_fetcher->ClearStorage(
        url.host(),
        static_cast<blink::mojom::StorageType>(static_cast<int>(storage_type)),
        base::BindRepeating(&SiteSettingsHandler::OnStorageCleared,
                            base::Unretained(this), barrier));

    // Also clear the *local* storage data.
    GetLocalStorageHelper()->DeleteOrigin(url, barrier);
  }
}

void SiteSettingsHandler::HandleFetchUsbDevices(const base::ListValue* args) {
  AllowJavascript();

  CHECK_EQ(1U, args->GetSize());
  const base::Value* callback_id;
  CHECK(args->Get(0, &callback_id));

  base::ListValue exceptions;
  const site_settings::ChooserTypeNameEntry* chooser_type =
      site_settings::ChooserTypeFromGroupName(site_settings::kGroupTypeUsb);
  // TODO(finnur): Figure out whether incognito permissions are also needed.
  site_settings::GetChooserExceptionsFromProfile(
      profile_, false, *chooser_type, &exceptions);
  ResolveJavascriptCallback(*callback_id, exceptions);
}

void SiteSettingsHandler::HandleRemoveUsbDevice(const base::ListValue* args) {
  CHECK_EQ(3U, args->GetSize());

  std::string origin_string;
  CHECK(args->GetString(0, &origin_string));
  GURL requesting_origin(origin_string);
  CHECK(requesting_origin.is_valid());

  std::string embedding_origin_string;
  CHECK(args->GetString(1, &embedding_origin_string));
  GURL embedding_origin(embedding_origin_string);
  CHECK(embedding_origin.is_valid());

  const base::DictionaryValue* object = nullptr;
  CHECK(args->GetDictionary(2, &object));

  const site_settings::ChooserTypeNameEntry* chooser_type =
      site_settings::ChooserTypeFromGroupName(site_settings::kGroupTypeUsb);
  ChooserContextBase* chooser_context = chooser_type->get_context(profile_);
  chooser_context->RevokeObjectPermission(requesting_origin, embedding_origin,
                                          *object);
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

  if (type == CONTENT_SETTINGS_TYPE_SOUND &&
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
  std::map<std::string, std::set<std::string>> all_sites_map;

  // TODO(https://crbug.com/835712): Assess performance of this method for
  // unusually large numbers of stored content settings.

  // Add sites that are using any local storage to the list.
  GetLocalStorageHelper()->StartFetching(base::BindRepeating(
      &SiteSettingsHandler::OnLocalStorageFetched, base::Unretained(this)));

  // Retrieve a list of embargoed settings to check separately. This ensures
  // that only settings included in |content_types| will be listed in all sites.
  ContentSettingsForOneType embargo_settings;
  map->GetSettingsForOneType(CONTENT_SETTINGS_TYPE_PERMISSION_AUTOBLOCKER_DATA,
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
          CreateOrAppendSiteGroupEntry(&all_sites_map, url);
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
      if (PatternAppliesToSingleOrigin(e))
        CreateOrAppendSiteGroupEntry(&all_sites_map,
                                     GURL(e.primary_pattern.ToString()));
    }
  }

  base::Value result(base::Value::Type::LIST);
  ConvertSiteGroupMapToListValue(all_sites_map, &result, profile);
  ResolveJavascriptCallback(*callback_id, result);
}

void SiteSettingsHandler::OnLocalStorageFetched(
    const std::list<BrowsingDataLocalStorageHelper::LocalStorageInfo>&
        local_storage_info) {
  std::map<std::string, int> origin_size_map;
  std::map<std::string, std::set<std::string>> all_sites_map;
  for (const BrowsingDataLocalStorageHelper::LocalStorageInfo& info :
       local_storage_info) {
    origin_size_map.emplace(info.origin_url.spec(), info.size);
    CreateOrAppendSiteGroupEntry(&all_sites_map, info.origin_url);
  }
  base::Value result(base::Value::Type::LIST);
  ConvertSiteGroupMapToListValue(all_sites_map, &result, profile_);

  // Merge the origin usage number into |result|.
  for (size_t i = 0; i < result.GetList().size(); ++i) {
    base::Value* site_group = &result.GetList()[i];
    base::Value* origin_list = site_group->FindKey(kOriginList);

    for (size_t i = 0; i < origin_list->GetList().size(); ++i) {
      base::Value* origin_info = &origin_list->GetList()[i];
      const std::string& origin = origin_info->FindKey("origin")->GetString();
      const auto& size_info = origin_size_map.find(origin);
      if (size_info != origin_size_map.end())
        origin_info->SetKey("usage", base::Value(size_info->second));
    }
  }
  FireWebUIListener("onLocalStorageListFetched", result);
}

void SiteSettingsHandler::HandleGetFormattedBytes(const base::ListValue* args) {
  AllowJavascript();

  CHECK_EQ(2U, args->GetSize());
  const base::Value* callback_id;
  CHECK(args->Get(0, &callback_id));
  int num_bytes;
  CHECK(args->GetInteger(1, &num_bytes));

  const base::string16 string = ui::FormatBytes(num_bytes);
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
    if (content_type == CONTENT_SETTINGS_TYPE_SOUND) {
      ContentSetting default_setting =
          map->GetDefaultContentSetting(CONTENT_SETTINGS_TYPE_SOUND, nullptr);
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
                                     CONTENT_SETTINGS_TYPE_PLUGINS_DATA,
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

  if (content_type == CONTENT_SETTINGS_TYPE_SOUND) {
    ContentSetting default_setting =
        map->GetDefaultContentSetting(CONTENT_SETTINGS_TYPE_SOUND, nullptr);
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
  // TODO(dschuyler): Review whether |secondary_pattern_string| should be used.
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

  if (content_type == CONTENT_SETTINGS_TYPE_SOUND) {
    ContentSetting default_setting =
        map->GetDefaultContentSetting(CONTENT_SETTINGS_TYPE_SOUND, nullptr);
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

void SiteSettingsHandler::HandleIsPatternValid(
    const base::ListValue* args) {
  AllowJavascript();
  CHECK_EQ(2U, args->GetSize());
  const base::Value* callback_id;
  CHECK(args->Get(0, &callback_id));
  std::string pattern_string;
  CHECK(args->GetString(1, &pattern_string));

  ContentSettingsPattern pattern =
      ContentSettingsPattern::FromString(pattern_string);
  bool valid = pattern.IsValid();

  // If the input is just '*' don't allow it, even though it's a valid pattern.
  // This changes the default setting.
  if (pattern == ContentSettingsPattern::Wildcard())
    valid = false;

  ResolveJavascriptCallback(*callback_id, base::Value(valid));
}

void SiteSettingsHandler::HandleUpdateIncognitoStatus(
    const base::ListValue* args) {
  AllowJavascript();
  SendIncognitoStatus(profile_, /*was_destroyed=*/ false);
}

void SiteSettingsHandler::SendIncognitoStatus(
    Profile* profile, bool was_destroyed) {
  if (!IsJavascriptAllowed())
    return;

  // When an incognito profile is destroyed, it sends out the destruction
  // message before destroying, so HasOffTheRecordProfile for profile_ won't
  // return false until after the profile actually been destroyed.
  bool incognito_enabled = profile_->HasOffTheRecordProfile() &&
      !(was_destroyed && profile == profile_->GetOffTheRecordProfile());

  FireWebUIListener("onIncognitoStatusChanged", base::Value(incognito_enabled));
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
        content::ZoomLevelToZoomFactor(zoom_level.zoom_level) * 100 + 0.5);
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
  status.SetKey("enabled",
                base::Value(UnifiedAutoplayConfig::IsBlockAutoplayUserModifiable(
                    profile_)));

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

void SiteSettingsHandler::SetBrowsingDataLocalStorageHelperForTesting(
    scoped_refptr<BrowsingDataLocalStorageHelper> helper) {
  DCHECK(!local_storage_helper_);
  local_storage_helper_ = helper;
}

BrowsingDataLocalStorageHelper* SiteSettingsHandler::GetLocalStorageHelper() {
  if (!local_storage_helper_)
    local_storage_helper_ = new BrowsingDataLocalStorageHelper(profile_);
  return local_storage_helper_.get();
}

}  // namespace settings
