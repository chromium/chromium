// Copyright 2016 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_WEBUI_SETTINGS_SITE_SETTINGS_HELPER_H_
#define CHROME_BROWSER_UI_WEBUI_SETTINGS_SITE_SETTINGS_HELPER_H_

#include <map>
#include <memory>
#include <set>
#include <string>
#include <string_view>
#include <utility>
#include <vector>

#include "base/values.h"
#include "components/content_settings/core/common/content_settings.h"
#include "components/content_settings/core/common/content_settings_pattern.h"
#include "components/content_settings/core/common/content_settings_types.h"
#include "components/prefs/pref_service.h"
#include "extensions/common/extension.h"

class HostContentSettingsMap;
class Profile;
struct UrlIdentity;

namespace content {
class WebUI;
}

namespace permissions {
class ObjectPermissionContextBase;
}

namespace web_app {
class IsolatedWebAppUrlInfo;
}  // namespace web_app

namespace site_settings {

struct SiteExceptionInfo {
  ContentSetting content_setting;
  bool is_embargoed;
  base::Time expiration;
};

struct StorageAccessEmbeddingException {
  ContentSettingsPattern secondary_pattern;
  bool is_incognito;
  bool is_embargoed;
  base::Time expiration;
};

// An enum representing the source of a per-site content setting (corresponds to
// UI enum of the same name in constants.ts).
// Note: this should not be used for default content setting sources, which
// instead use `ProviderToDefaultSettingSourceString`.
enum class SiteSettingSource {
  kAdsFilterBlocklist,
  kEmbargo,
  kInsecureOrigin,
  kKillSwitch,
  kAllowlist,
  kPolicy,
  kExtension,
  kHostedApp,
  kPreference,
  kDefault,
  kNumSources,
};

// Maps from a pair(secondary pattern, incognito)  to a setting and if it's
// embargoed.
typedef std::map<std::pair<ContentSettingsPattern, bool>, SiteExceptionInfo>
    OnePatternSettings;

// Maps from a pair (primary pattern, source) to a OnePatternSettings. All the
// mappings in OnePatternSettings share the given primary pattern and source.
//
// The operator< in ContentSettingsPattern, determines that by default the
// preferences are saved in lowest precedence pattern to the highest. However,
// we want to show the patterns with the highest precedence (the more specific
// ones) on the top, hence `std::greater<>`.
typedef std::map<std::pair<ContentSettingsPattern, SiteSettingSource>,
                 OnePatternSettings,
                 std::greater<>>
    AllPatternsSettings;

// A set of <origin, source, incognito> tuple for organizing granted permission
// objects that belong to the same device.

using ChooserExceptionDetails =
    std::set<std::tuple<GURL, SiteSettingSource, bool>>;

constexpr char kChooserType[] = "chooserType";
constexpr char kCloseDescription[] = "closeDescription";
constexpr char kDisabled[] = "disabled";
constexpr char kDisplayName[] = "displayName";
constexpr char kDescription[] = "description";
constexpr char kEmbeddingOrigin[] = "embeddingOrigin";
constexpr char kEmbeddingDisplayName[] = "embeddingDisplayName";
constexpr char kExceptions[] = "exceptions";
constexpr char kFileSystemFilePath[] = "filePath";
constexpr char kFileSystemIsDirectory[] = "isDirectory";
constexpr char kFileSystemEditGrants[] = "editGrants";
constexpr char kFileSystemViewGrants[] = "viewGrants";
constexpr char kHostOrSpec[] = "hostOrSpec";
constexpr char kIncognito[] = "incognito";
constexpr char kIsEmbargoed[] = "isEmbargoed";
constexpr char kObject[] = "object";
constexpr char kOpenDescription[] = "openDescription";
constexpr char kOrigin[] = "origin";
constexpr char kOrigins[] = "origins";
constexpr char kOriginForFavicon[] = "originForFavicon";
constexpr char kPermissions[] = "permissions";
constexpr char kPolicyIndicator[] = "indicator";
constexpr char kReaderName[] = "readerName";
constexpr char kRecentPermissions[] = "recentPermissions";
constexpr char kSetting[] = "setting";
constexpr char kSites[] = "sites";
constexpr char kSource[] = "source";
constexpr char kType[] = "type";
constexpr char kNotificationPermissionsReviewListMaybeChangedEvent[] =
    "notification-permission-review-list-maybe-changed";

// Returns whether a group name has been registered for the given type.
bool HasRegisteredGroupName(ContentSettingsType type);

// Converts a ContentSettingsType to/from its group name identifier.
ContentSettingsType ContentSettingsTypeFromGroupName(std::string_view name);
std::string_view ContentSettingsTypeToGroupName(ContentSettingsType type);

// Returns a list of all content settings types that correspond to permissions
// and which should be displayed in chrome://settings. An origin and profile may
// be passed to get lists pertinent to particular origins and their settings.
std::vector<ContentSettingsType> GetVisiblePermissionCategories(
    const std::string& origin = std::string(),
    Profile* profile = nullptr);

// Converts a SiteSettingSource to its string identifier.
std::string SiteSettingSourceToString(const SiteSettingSource source);

// Helper function to construct a dictionary for a File System exception.
base::Value::Dict GetFileSystemExceptionForPage(
    ContentSettingsType content_type,
    Profile* profile,
    const std::string& origin,
    const base::FilePath& file_path,
    const ContentSetting& setting,
    SiteSettingSource source,
    bool incognito,
    bool is_embargoed = false);

// Calculates the number of days between now and `expiration_timestamp`,
// timestamp of when a setting is going to expire, and returns the appropriate
// string for display in site settings. Only looks at the date between now and
// `expiration_timestamp` i.e. doesn't take into account time.

// E.g. current time 03/07 18:00. If expiration is in:
//   03/07 01:00 then, time diff is 17h, and returns 0.
//   04/07 19:00 then, time diff is 23h, but returns 1.
//   05/07 19:00 then, time diff is 47h, and returns 2.
//   05/07 17:00 then, time diff is 49h, and returns 2.
std::u16string GetExpirationDescription(const base::Time& expiration_timestamp);

// Helper function to construct a dictionary for a storage access exceptions
// grouped by origin.
base::Value::Dict GetStorageAccessExceptionForPage(
    Profile* profile,
    const ContentSettingsPattern& pattern,
    const std::string& display_name,
    ContentSetting setting,
    const std::vector<StorageAccessEmbeddingException>& exceptions);

// Helper function to construct a dictionary for an exception.
base::Value::Dict GetExceptionForPage(
    ContentSettingsType content_type,
    Profile* profile,
    const ContentSettingsPattern& pattern,
    const ContentSettingsPattern& secondary_pattern,
    const std::string& display_name,
    const ContentSetting& setting,
    const SiteSettingSource source,
    const base::Time& expiration,
    bool incognito,
    bool is_embargoed = false);

// Helper function to construct a dictionary for a hosted app exception.
void AddExceptionForHostedApp(const std::string& url_pattern,
                              const extensions::Extension& app,
                              base::Value::List* exceptions);

// Fills in |exceptions| with Values for the given |type| from |profile|.
void GetExceptionsForContentType(ContentSettingsType type,
                                 Profile* profile,
                                 content::WebUI* web_ui,
                                 bool incognito,
                                 base::Value::List* exceptions);

// Fills in |exceptions| with Values for the Storage Access exception for the
// given content setting (such as enabled or blocked) from a |profile| and its
// |incognito_profile|, if applicable.
void GetStorageAccessExceptions(ContentSetting content_setting,
                                Profile* profile,
                                Profile* incognito_profile,
                                content::WebUI* web_ui,
                                base::Value::List* exceptions);

// Fills in object saying what the current settings is for the category (such as
// enabled or blocked) and the source of that setting (such preference, policy,
// or extension).
void GetContentCategorySetting(const HostContentSettingsMap* map,
                               ContentSettingsType content_type,
                               base::Value::Dict* object);

// Retrieves the current setting for a given origin, category pair, the source
// of that setting, and its display name, which will be different if it's an
// extension. Note this is similar to GetContentCategorySetting() above but this
// goes through the PermissionManager (preferred, see https://crbug.com/739241).
ContentSetting GetContentSettingForOrigin(Profile* profile,
                                          const HostContentSettingsMap* map,
                                          const GURL& origin,
                                          ContentSettingsType content_type,
                                          SiteSettingSource* source);

// Returns URLs with granted entries from the File System Access API.
void GetFileSystemGrantedEntries(std::vector<base::Value::Dict>* exceptions,
                                 Profile* profile,
                                 bool incognito);

// Returns all site permission exceptions for a given content type
std::vector<ContentSettingPatternSource>
GetSingleOriginExceptionsForContentType(HostContentSettingsMap* map,
                                        ContentSettingsType content_type);

// This struct facilitates lookup of a chooser context factory function by name
// for a given content settings type and is declared early so that it can used
// by functions below.
struct ChooserTypeNameEntry {
  permissions::ObjectPermissionContextBase* (*get_context)(Profile*);
  const char* name;
};

struct ContentSettingsTypeNameEntry {
  ContentSettingsType type;
  const char* name;
};

const ChooserTypeNameEntry* ChooserTypeFromGroupName(std::string_view name);

// Creates a chooser exception object for the object with |display_name|. The
// object contains the following properties
// * displayName: string,
// * object: Object,
// * chooserType: string,
// * sites: Array<SiteException>
// The structure of the SiteException objects is the same as the objects
// returned by GetExceptionForPage().
base::Value::Dict CreateChooserExceptionObject(
    const std::u16string& display_name,
    const base::Value& object,
    const std::string& chooser_type,
    const ChooserExceptionDetails& chooser_exception_details,
    Profile* profile);

// Returns an array of chooser exception objects.
base::Value::List GetChooserExceptionListFromProfile(
    Profile* profile,
    const ChooserTypeNameEntry& chooser_type);

// Takes |url| and converts it into an individual origin string or retrieves
// name of the extension or Isolated Web App it belongs to. If |hostname_only|
// is true, returns |url|'s hostname for HTTP/HTTPS pages or unknown
// extension/IWA URLs, otherwise an origin string will be returned that
// includes the scheme if it's non-cryptographic.
UrlIdentity GetUrlIdentityForGURL(Profile* profile,
                                  const GURL& url,
                                  bool hostname_only);
std::string GetDisplayNameForGURL(Profile* profile,
                                  const GURL& url,
                                  bool hostname_only);

// Returns data about all currently installed Isolated Web Apps.
std::vector<web_app::IsolatedWebAppUrlInfo> GetInstalledIsolatedWebApps(
    Profile* profile);

}  // namespace site_settings

#endif  // CHROME_BROWSER_UI_WEBUI_SETTINGS_SITE_SETTINGS_HELPER_H_
