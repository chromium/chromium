// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_WEBUI_SETTINGS_SITE_SETTINGS_HANDLER_H_
#define CHROME_BROWSER_UI_WEBUI_SETTINGS_SITE_SETTINGS_HANDLER_H_

#include <map>
#include <memory>
#include <set>
#include <string>

#include "base/containers/flat_set.h"
#include "base/scoped_observer.h"
#include "chrome/browser/browsing_data/cookies_tree_model.h"
#include "chrome/browser/permissions/chooser_context_base.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/profiles/profile_observer.h"
#include "chrome/browser/ui/webui/settings/settings_page_ui_handler.h"
#include "chrome/browser/web_applications/components/app_registrar.h"
#include "chrome/browser/web_applications/web_app_registrar.h"
#include "components/content_settings/core/browser/content_settings_observer.h"
#include "components/content_settings/core/browser/host_content_settings_map.h"
#include "content/public/browser/host_zoom_map.h"
#include "ppapi/buildflags/buildflags.h"

class PrefChangeRegistrar;

namespace base {
class ListValue;
}

namespace settings {

// Chrome "ContentSettings" settings page UI handler.
class SiteSettingsHandler : public SettingsPageUIHandler,
                            public content_settings::Observer,
                            public ProfileObserver,
                            public ChooserContextBase::PermissionObserver,
                            public CookiesTreeModel::Observer {
 public:
  explicit SiteSettingsHandler(Profile* profile,
                               web_app::AppRegistrar& web_app_registrar);
  ~SiteSettingsHandler() override;

  // SettingsPageUIHandler:
  void RegisterMessages() override;

  void OnJavascriptAllowed() override;
  void OnJavascriptDisallowed() override;

  // Usage info.
  void OnGetUsageInfo();

  // CookiesTreeModel::Observer:
  // TODO(https://crbug.com/835712): Listen for backend data changes and notify
  // WebUI
  void TreeNodesAdded(ui::TreeModel* model,
                      ui::TreeModelNode* parent,
                      size_t start,
                      size_t count) override;
  void TreeNodesRemoved(ui::TreeModel* model,
                        ui::TreeModelNode* parent,
                        size_t start,
                        size_t count) override;
  void TreeNodeChanged(ui::TreeModel* model, ui::TreeModelNode* node) override;
  void TreeModelEndBatch(CookiesTreeModel* model) override;

#if defined(OS_CHROMEOS)
  // Alert the Javascript that the |kEnableDRM| pref has changed.
  void OnPrefEnableDrmChanged();
#endif

  // content_settings::Observer:
  void OnContentSettingChanged(const ContentSettingsPattern& primary_pattern,
                               const ContentSettingsPattern& secondary_pattern,
                               ContentSettingsType content_type,
                               const std::string& resource_identifier) override;

  // ProfileObserver:
  void OnOffTheRecordProfileCreated(Profile* off_the_record) override;
  void OnProfileWillBeDestroyed(Profile* profile) override;

  // ChooserContextBase::PermissionObserver implementation:
  void OnChooserObjectPermissionChanged(
      ContentSettingsType guard_content_settings_type,
      ContentSettingsType data_content_settings_type) override;

  // content::HostZoomMap subscription.
  void OnZoomLevelChanged(const content::HostZoomMap::ZoomLevelChange& change);

 private:
  friend class SiteSettingsHandlerChooserExceptionTest;
  friend class SiteSettingsHandlerInfobarTest;
  friend class SiteSettingsHandlerTest;
  FRIEND_TEST_ALL_PREFIXES(SiteSettingsHandlerChooserExceptionTest,
                           HandleGetChooserExceptionListForUsb);
  FRIEND_TEST_ALL_PREFIXES(SiteSettingsHandlerChooserExceptionTest,
                           HandleResetChooserExceptionForSiteForUsb);
  FRIEND_TEST_ALL_PREFIXES(SiteSettingsHandlerInfobarTest,
                           SettingPermissionsTriggersInfobar);
  FRIEND_TEST_ALL_PREFIXES(SiteSettingsHandlerTest,
                           BlockAutoplay_SendOnRequest);
  FRIEND_TEST_ALL_PREFIXES(SiteSettingsHandlerTest, BlockAutoplay_Update);
#if BUILDFLAG(ENABLE_PLUGINS)
  FRIEND_TEST_ALL_PREFIXES(SiteSettingsHandlerTest,
                           ChangingFlashSettingForSiteIsRemembered);
#endif
  FRIEND_TEST_ALL_PREFIXES(SiteSettingsHandlerTest, DefaultSettingSource);
  FRIEND_TEST_ALL_PREFIXES(SiteSettingsHandlerTest, ExceptionHelpers);
  FRIEND_TEST_ALL_PREFIXES(SiteSettingsHandlerTest, ExtensionDisplayName);
  FRIEND_TEST_ALL_PREFIXES(SiteSettingsHandlerTest, GetAllSites);
  FRIEND_TEST_ALL_PREFIXES(SiteSettingsHandlerTest, OnStorageFetched);
  FRIEND_TEST_ALL_PREFIXES(SiteSettingsHandlerTest, GetAndSetDefault);
  FRIEND_TEST_ALL_PREFIXES(SiteSettingsHandlerTest, GetAndSetForInvalidURLs);
  FRIEND_TEST_ALL_PREFIXES(SiteSettingsHandlerTest, GetAndSetOriginPermissions);
  FRIEND_TEST_ALL_PREFIXES(SiteSettingsHandlerTest, Incognito);
  FRIEND_TEST_ALL_PREFIXES(SiteSettingsHandlerTest, Origins);
  FRIEND_TEST_ALL_PREFIXES(SiteSettingsHandlerTest, Patterns);
  FRIEND_TEST_ALL_PREFIXES(SiteSettingsHandlerTest, PatternsAndContentType);
  FRIEND_TEST_ALL_PREFIXES(SiteSettingsHandlerTest, SessionOnlyException);
  FRIEND_TEST_ALL_PREFIXES(SiteSettingsHandlerTest, ZoomLevels);
  FRIEND_TEST_ALL_PREFIXES(SiteSettingsHandlerTest,
                           HandleClearEtldPlus1DataAndCookies);
  FRIEND_TEST_ALL_PREFIXES(SiteSettingsHandlerTest, HandleGetFormattedBytes);
  FRIEND_TEST_ALL_PREFIXES(SiteSettingsHandlerTest,
                           NotificationPermissionRevokeUkm);

  // Creates the CookiesTreeModel if necessary.
  void EnsureCookiesTreeModelCreated();

  // Add or remove this class as an observer for content settings and chooser
  // contexts corresponding to |profile|.
  void ObserveSourcesForProfile(Profile* profile);
  void StopObservingSourcesForProfile(Profile* profile);

  // Calculates the data storage that has been used for each origin, and
  // stores the information in the |all_sites_map| and |origin_size_map|.
  void GetOriginStorage(
      std::map<std::string, std::set<std::string>>* all_sites_map,
      std::map<std::string, int64_t>* origin_size_map);

  // Calculates the number of cookies for each etld+1 and each origin, and
  // stores the information in the |all_sites_map| and |origin_cookie_map|.
  void GetOriginCookies(
      std::map<std::string, std::set<std::string>>* all_sites_map,
      std::map<std::string, int>* origin_cookie_map);

  // Asynchronously fetches the usage for a given origin. Replies back with
  // OnGetUsageInfo above.
  void HandleFetchUsageTotal(const base::ListValue* args);

  // Deletes the storage being used for a given host.
  void HandleClearUsage(const base::ListValue* args);

  // Gets and sets the default value for a particular content settings type.
  void HandleSetDefaultValueForContentType(const base::ListValue* args);
  void HandleGetDefaultValueForContentType(const base::ListValue* args);

  // Returns a list of sites with permissions settings, grouped by their
  // eTLD+1. Recreates the cookies tree model to fetch the cookie and usage
  // data, which will send the list of sites with cookies or usage data to
  // the front end when fetching finished.
  void HandleGetAllSites(const base::ListValue* args);

  // Called when the list of origins using storage has been fetched, and sends
  // this list back to the front end.
  void OnStorageFetched();

  // Returns a list of sites, grouped by their effective top level domain plus
  // 1, with their cookies number and data usage information. This method will
  // only be called after HandleGetAllSites is called.
  base::Value PopulateCookiesAndUsageData(Profile* profile);

  // Converts a given number of bytes into a human-readable format, with data
  // units.
  void HandleGetFormattedBytes(const base::ListValue* args);

  // Returns the list of site exceptions for a given content settings type.
  void HandleGetExceptionList(const base::ListValue* args);

  // Returns the list of chooser exceptions for a given chooser type.
  void HandleGetChooserExceptionList(const base::ListValue* args);

  // Gets and sets a list of ContentSettingTypes for an origin.
  // TODO(https://crbug.com/739241): Investigate replacing the
  // '*CategoryPermissionForPattern' equivalents below with these methods.
  void HandleGetOriginPermissions(const base::ListValue* args);
  void HandleSetOriginPermissions(const base::ListValue* args);

  // Clears the Flash data setting used to remember if the user has changed the
  // Flash permission for an origin.
  void HandleClearFlashPref(const base::ListValue* args);

  // Handles setting and resetting an origin permission.
  void HandleResetCategoryPermissionForPattern(const base::ListValue* args);
  void HandleSetCategoryPermissionForPattern(const base::ListValue* args);

  // Handles resetting a chooser exception for the given site.
  void HandleResetChooserExceptionForSite(const base::ListValue* args);

  // Returns whether a given string is a valid origin.
  void HandleIsOriginValid(const base::ListValue* args);

  // Returns whether the pattern is valid given the type.
  void HandleIsPatternValidForType(const base::ListValue* args);

  // Looks up whether an incognito session is active.
  void HandleUpdateIncognitoStatus(const base::ListValue* args);

  // Notifies the JS side whether incognito is enabled.
  void SendIncognitoStatus(Profile* profile, bool was_destroyed);

  // Handles the request for a list of all zoom levels.
  void HandleFetchZoomLevels(const base::ListValue* args);

  // Sends the zoom level list down to the web ui.
  void SendZoomLevels();

  // Removes a particular zoom level for a given host.
  void HandleRemoveZoomLevel(const base::ListValue* args);

  // Handles the request to send block autoplay state.
  void HandleFetchBlockAutoplayStatus(const base::ListValue* args);

  // Notifies the JS side about the state of the block autoplay toggle.
  void SendBlockAutoplayStatus();

  // Updates the block autoplay enabled pref when the UI is toggled.
  void HandleSetBlockAutoplayEnabled(const base::ListValue* args);

  // Clear web storage data and cookies from cookies tree model for an ETLD+1.
  void HandleClearEtldPlus1DataAndCookies(const base::ListValue* args);

  // Record metrics for actions on All Sites Page.
  void HandleRecordAction(const base::ListValue* args);

  void SetCookiesTreeModelForTesting(
      std::unique_ptr<CookiesTreeModel> cookies_tree_model);

  void ClearAllSitesMapForTesting();

  Profile* profile_;
  web_app::AppRegistrar& app_registrar_;

  ScopedObserver<Profile, ProfileObserver> observed_profiles_{this};

  // Keeps track of events related to zooming.
  std::unique_ptr<content::HostZoomMap::Subscription>
      host_zoom_map_subscription_;

  // The host for which to fetch usage.
  std::string usage_host_;

  // The origin for which to clear usage.
  std::string clearing_origin_;

  // Change observer for content settings.
  ScopedObserver<HostContentSettingsMap, content_settings::Observer> observer_{
      this};

  // Change observer for chooser permissions.
  ScopedObserver<ChooserContextBase, ChooserContextBase::PermissionObserver>
      chooser_observer_{this};

  // Change observer for prefs.
  std::unique_ptr<PrefChangeRegistrar> pref_change_registrar_;

  std::unique_ptr<CookiesTreeModel> cookies_tree_model_;

  // Whether to send all sites list on cookie tree model update.
  bool send_sites_list_ = false;

  // Populated every time the user reloads the All Sites page.
  std::map<std::string, std::set<std::string>> all_sites_map_;

  // Store the origins that has permission settings.
  std::set<std::string> origin_permission_set_;

  // Whether to send site detail data on cookie tree model update.
  bool update_site_details_ = false;

  DISALLOW_COPY_AND_ASSIGN(SiteSettingsHandler);
};

}  // namespace settings

#endif  // CHROME_BROWSER_UI_WEBUI_SETTINGS_SITE_SETTINGS_HANDLER_H_
