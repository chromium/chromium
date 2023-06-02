// Copyright 2016 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_WEBUI_SETTINGS_SITE_SETTINGS_HANDLER_H_
#define CHROME_BROWSER_UI_WEBUI_SETTINGS_SITE_SETTINGS_HANDLER_H_

#include <map>
#include <memory>
#include <set>
#include <string>
#include <vector>

#include "base/gtest_prod_util.h"
#include "base/memory/raw_ptr.h"
#include "base/memory/weak_ptr.h"
#include "base/scoped_multi_source_observation.h"
#include "base/values.h"
#include "build/chromeos_buildflags.h"
#include "chrome/browser/browsing_data/cookies_tree_model.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/profiles/profile_observer.h"
#include "chrome/browser/ui/webui/settings/settings_page_ui_handler.h"
#include "components/content_settings/core/browser/content_settings_observer.h"
#include "components/content_settings/core/browser/host_content_settings_map.h"
#include "components/permissions/object_permission_context_base.h"
#include "components/prefs/pref_store.h"
#include "content/public/browser/host_zoom_map.h"
#include "ppapi/buildflags/buildflags.h"
#include "third_party/abseil-cpp/absl/types/optional.h"
#include "third_party/abseil-cpp/absl/types/variant.h"
#include "url/origin.h"

class BrowsingDataModel;
class PrefChangeRegistrar;

namespace settings {

// Chrome "ContentSettings" settings page UI handler.
class SiteSettingsHandler
    : public SettingsPageUIHandler,
      public content_settings::Observer,
      public ProfileObserver,
      public permissions::ObjectPermissionContextBase::PermissionObserver,
      public CookiesTreeModel::Observer {
 public:
  // The key used to group origins together in the UI. If two origins map to
  // the same GroupingKey, they should be displayed in the same UI group. For
  // normal web content, this will be equal to the eTLD+1.
  class GroupingKey {
   public:
    static GroupingKey Create(const url::Origin& origin);
    static GroupingKey CreateFromEtldPlus1(const std::string& etld_plus1);
    static GroupingKey Deserialize(const std::string& serialized);

    GroupingKey(const GroupingKey& other);
    GroupingKey& operator=(const GroupingKey& other);
    ~GroupingKey();

    std::string Serialize() const;

    // Returns the eTLD+1 that this GroupingKey represents, or nullopt if it
    // doesn't represent an eTLD+1.
    absl::optional<std::string> GetEtldPlusOne() const;

    // Returns the origin that this GroupingKey represents, or nullopt if it
    // doesn't represent an origin.
    absl::optional<url::Origin> GetOrigin() const;

    bool operator<(const GroupingKey& other) const;

   private:
    explicit GroupingKey(const absl::variant<std::string, url::Origin>& value);

    url::Origin ToOrigin() const;

    // eTLD+1 or Origin
    absl::variant<std::string, url::Origin> value_;
  };

  using AllSitesMap =
      std::map<GroupingKey, std::set<std::pair<url::Origin, bool>>>;

  explicit SiteSettingsHandler(Profile* profile);

  SiteSettingsHandler(const SiteSettingsHandler&) = delete;
  SiteSettingsHandler& operator=(const SiteSettingsHandler&) = delete;

  ~SiteSettingsHandler() override;

  // SettingsPageUIHandler:
  void RegisterMessages() override;

  void OnJavascriptAllowed() override;
  void OnJavascriptDisallowed() override;

  // Usage info.
  void OnGetUsageInfo();

  void BrowsingDataModelCreated(std::unique_ptr<BrowsingDataModel> model);

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
  void TreeModelEndBatchDeprecated(CookiesTreeModel* model) override;

  // content_settings::Observer:
  void OnContentSettingChanged(const ContentSettingsPattern& primary_pattern,
                               const ContentSettingsPattern& secondary_pattern,
                               ContentSettingsType content_type) override;

  // ProfileObserver:
  void OnOffTheRecordProfileCreated(Profile* off_the_record) override;
  void OnProfileWillBeDestroyed(Profile* profile) override;

  // ObjectPermissionContextBase::PermissionObserver implementation:
  void OnObjectPermissionChanged(
      absl::optional<ContentSettingsType> guard_content_settings_type,
      ContentSettingsType data_content_settings_type) override;

  void OnZoomLevelChanged(const content::HostZoomMap::ZoomLevelChange& change);

 private:
  friend class SiteSettingsHandlerBaseTest;
  friend class SiteSettingsHandlerChooserExceptionTest;
  friend class SiteSettingsHandlerInfobarTest;
  // TODO(crbug.com/1373962): Remove this friend class when
  // Persistent Permissions is launched.
  friend class PersistentPermissionsSiteSettingsHandlerTest;
  FRIEND_TEST_ALL_PREFIXES(PersistentPermissionsSiteSettingsHandlerTest,
                           HandleGetFileSystemGrants);
  FRIEND_TEST_ALL_PREFIXES(PersistentPermissionsSiteSettingsHandlerTest,
                           HandleRevokeFileSystemGrant);
  FRIEND_TEST_ALL_PREFIXES(PersistentPermissionsSiteSettingsHandlerTest,
                           HandleRevokeFileSystemGrants);
  FRIEND_TEST_ALL_PREFIXES(SiteSettingsHandlerInfobarTest,
                           SettingPermissionsTriggersInfobar);
  FRIEND_TEST_ALL_PREFIXES(SiteSettingsHandlerTest,
                           BlockAutoplay_SendOnRequest);
  FRIEND_TEST_ALL_PREFIXES(SiteSettingsHandlerTest, BlockAutoplay_Update);
#if BUILDFLAG(ENABLE_PLUGINS)
  FRIEND_TEST_ALL_PREFIXES(SiteSettingsHandlerTest,
                           ChangingFlashSettingForSiteIsRemembered);
#endif
  FRIEND_TEST_ALL_PREFIXES(SiteSettingsHandlerTest, Cookies);
  FRIEND_TEST_ALL_PREFIXES(SiteSettingsHandlerTest, DefaultSettingSource);
  FRIEND_TEST_ALL_PREFIXES(SiteSettingsHandlerTest, ExceptionHelpers);
  FRIEND_TEST_ALL_PREFIXES(SiteSettingsHandlerTest, ExtensionDisplayName);
  FRIEND_TEST_ALL_PREFIXES(SiteSettingsHandlerTest, GetAllSites);
  FRIEND_TEST_ALL_PREFIXES(SiteSettingsHandlerTest, GetRecentSitePermissions);
  FRIEND_TEST_ALL_PREFIXES(SiteSettingsHandlerTest, OnStorageFetched);
  FRIEND_TEST_ALL_PREFIXES(SiteSettingsHandlerTest, GetAndSetDefault);
  FRIEND_TEST_ALL_PREFIXES(SiteSettingsHandlerTest, GetAndSetForInvalidURLs);
  FRIEND_TEST_ALL_PREFIXES(SiteSettingsHandlerTest, GetAndSetOriginPermissions);
  FRIEND_TEST_ALL_PREFIXES(SiteSettingsHandlerTest, Incognito);
  FRIEND_TEST_ALL_PREFIXES(SiteSettingsHandlerTest, IncognitoExceptions);
  FRIEND_TEST_ALL_PREFIXES(SiteSettingsHandlerTest,
                           ResetCategoryPermissionForEmbargoedOrigins);
  FRIEND_TEST_ALL_PREFIXES(SiteSettingsHandlerTest,
                           ResetCategoryPermissionForInvalidOrigins);
  FRIEND_TEST_ALL_PREFIXES(SiteSettingsHandlerTest, Origins);
  FRIEND_TEST_ALL_PREFIXES(SiteSettingsHandlerTest, Patterns);
  FRIEND_TEST_ALL_PREFIXES(SiteSettingsHandlerTest, PatternsAndContentType);
  FRIEND_TEST_ALL_PREFIXES(SiteSettingsHandlerTest, SessionOnlyException);
  FRIEND_TEST_ALL_PREFIXES(SiteSettingsHandlerTest, ZoomLevels);
  FRIEND_TEST_ALL_PREFIXES(SiteSettingsHandlerTest,
                           HandleClearSiteGroupDataAndCookies);
  FRIEND_TEST_ALL_PREFIXES(SiteSettingsHandlerTest,
                           HandleClearUnpartitionedUsage);
  FRIEND_TEST_ALL_PREFIXES(SiteSettingsHandlerTest, ClearClientHints);
  FRIEND_TEST_ALL_PREFIXES(SiteSettingsHandlerTest, ClearReducedAcceptLanguage);
  FRIEND_TEST_ALL_PREFIXES(SiteSettingsHandlerTest,
                           HandleClearPartitionedUsage);
  FRIEND_TEST_ALL_PREFIXES(SiteSettingsHandlerTest, CookieSettingDescription);
  FRIEND_TEST_ALL_PREFIXES(SiteSettingsHandlerTest, HandleGetFormattedBytes);
  FRIEND_TEST_ALL_PREFIXES(SiteSettingsHandlerTest,
                           NotificationPermissionRevokeUkm);
  FRIEND_TEST_ALL_PREFIXES(SiteSettingsHandlerTest, ExcludeWebUISchemesInLists);
  FRIEND_TEST_ALL_PREFIXES(SiteSettingsHandlerTest,
                           IncludeWebUISchemesInGetOriginPermissions);
  FRIEND_TEST_ALL_PREFIXES(SiteSettingsHandlerTest, HandleGetUsageInfo);
  FRIEND_TEST_ALL_PREFIXES(SiteSettingsHandlerTest,
                           HandleGetFpsMembershipLabel);
  FRIEND_TEST_ALL_PREFIXES(SiteSettingsHandlerTest, NonTreeModelDeletion);
  FRIEND_TEST_ALL_PREFIXES(SiteSettingsHandlerTest, FirstPartySetsMembership);
  FRIEND_TEST_ALL_PREFIXES(
      SiteSettingsHandlerInfobarTest,
      SettingPermissionsDoesNotTriggerInfobarOnDifferentProfile);
#if BUILDFLAG(ENABLE_EXTENSIONS)
  FRIEND_TEST_ALL_PREFIXES(SiteSettingsHandlerTest, HandleGetExtensionName);
#endif
  FRIEND_TEST_ALL_PREFIXES(SiteSettingsHandlerTest, IsolatedWebAppUsageInfo);
  FRIEND_TEST_ALL_PREFIXES(SiteSettingsHandlerIsolatedWebAppTest, ZoomLevel);
  FRIEND_TEST_ALL_PREFIXES(SiteSettingsHandlerIsolatedWebAppTest,
                           ZoomLevelsSortedByAppName);

  // Rebuilds the BrowsingDataModel & CookiesTreeModel. Pending requests are
  // serviced when both models are built.
  void RebuildModels();
  void ModelBuilt();
  void ServicePendingRequests();

  // Add or remove this class as an observer for content settings and chooser
  // contexts corresponding to |profile|.
  void ObserveSourcesForProfile(Profile* profile);
  void StopObservingSourcesForProfile(Profile* profile);

  // Calculates the data storage that has been used for each origin, and
  // stores the information in the |all_sites_map| and |origin_size_map|.
  void GetOriginStorage(AllSitesMap* all_sites_map,
                        std::map<url::Origin, int64_t>* origin_size_map);

  // Calculates the number of cookies for each etld+1 and each host, and
  // stores the information in the |all_sites_map| and |host_cookie_map|.
  void GetHostCookies(
      AllSitesMap* all_sites_map,
      std::map<std::pair<std::string, absl::optional<std::string>>, int>*
          host_cookie_map);

  // Asynchronously fetches the usage for a given origin. Replies back with
  // OnGetUsageInfo above.
  void HandleFetchUsageTotal(const base::Value::List& args);

  // Asynchronously fetches the fps membership information label.
  void HandleGetFpsMembershipLabel(const base::Value::List& args);

  // Deletes the storage being used for a given host.
  void HandleClearUnpartitionedUsage(const base::Value::List& args);

  void HandleClearPartitionedUsage(const base::Value::List& args);

  // Gets and sets the default value for a particular content settings type.
  void HandleSetDefaultValueForContentType(const base::Value::List& args);
  void HandleGetDefaultValueForContentType(const base::Value::List& args);

  // Returns a list of sites with permissions settings, grouped by their
  // eTLD+1. Recreates the cookies tree model to fetch the cookie and usage
  // data, which will send the list of sites with cookies or usage data to
  // the front end when fetching finished.
  void HandleGetAllSites(const base::Value::List& args);

  // Returns a list of content settings types that are controlled via a standard
  // permissions UI and should be made visible to the user. There is a single
  // nullable string argument, which represents an associated origin. See
  // `SiteSettingsPrefsBrowserProxy#getCategoryList`.
  void HandleGetCategoryList(const base::Value::List& args);

  // Returns a string for display describing the current cookie settings.
  void HandleGetCookieSettingDescription(const base::Value::List& args);

  // Returns a list containing the most recent permission changes for the
  // content types that are visiblein settings, grouped by origin/profile
  // (incognito, regular) combinations, limited to N origin/profile pairings.
  // This includes permission changes made by embargo, but does not include
  // permissions enforced via policy.
  void HandleGetRecentSitePermissions(const base::Value::List& args);

  // Called when the list of origins using storage has been fetched, and sends
  // this list back to the front end.
  void OnStorageFetched();

  // Returns a list of sites, grouped by their effective top level domain plus
  // 1, with their cookies number and data usage information. This method will
  // only be called after HandleGetAllSites is called.
  base::Value::List PopulateCookiesAndUsageData(Profile* profile);

  // Converts a given number of bytes into a human-readable format, with data
  // units.
  void HandleGetFormattedBytes(const base::Value::List& args);

  // Returns the list of site exceptions for a given content settings type.
  void HandleGetExceptionList(const base::Value::List& args);

  // Returns the list of chooser exceptions for a given chooser type.
  void HandleGetChooserExceptionList(const base::Value::List& args);

  // Returns the list of the allowed permission grants as defined by the
  // File System Access API.
  void HandleGetFileSystemGrants(const base::Value::List& args);

  // Revokes the File System Access permission for a given origin
  // and file path.
  void HandleRevokeFileSystemGrant(const base::Value::List& args);

  // Revokes all of the File System Access permissions for a given origin.
  void HandleRevokeFileSystemGrants(const base::Value::List& args);

  // Gets and sets a list of ContentSettingTypes for an origin.
  // TODO(https://crbug.com/739241): Investigate replacing the
  // '*CategoryPermissionForPattern' equivalents below with these methods.
  void HandleGetOriginPermissions(const base::Value::List& args);
  void HandleSetOriginPermissions(const base::Value::List& args);

  // Handles setting and resetting an origin permission.
  void HandleResetCategoryPermissionForPattern(const base::Value::List& args);
  void HandleSetCategoryPermissionForPattern(const base::Value::List& args);

  // TODO(andypaicu, crbug.com/880684): Update to only expect a list of three
  // arguments, replacing the current (requesting,embedding) arguments with
  // simply (origin) and update all call sites.
  // Handles resetting a chooser exception for the given site.
  void HandleResetChooserExceptionForSite(const base::Value::List& args);

  // Returns whether a given string is a valid origin.
  void HandleIsOriginValid(const base::Value::List& args);

  // Returns whether the pattern is valid given the type.
  void HandleIsPatternValidForType(const base::Value::List& args);

  // Looks up whether an incognito session is active.
  void HandleUpdateIncognitoStatus(const base::Value::List& args);

  // Notifies the JS side whether incognito is enabled.
  void SendIncognitoStatus(Profile* profile, bool was_destroyed);

  // Handles the request for a list of all zoom levels.
  void HandleFetchZoomLevels(const base::Value::List& args);

  // Sends the zoom level list down to the web ui.
  void SendZoomLevels();

  // Removes a particular zoom level for a given host.
  void HandleRemoveZoomLevel(const base::Value::List& args);

  // Handles the request to send block autoplay state.
  void HandleFetchBlockAutoplayStatus(const base::Value::List& args);

  // Notifies the JS side about the state of the block autoplay toggle.
  void SendBlockAutoplayStatus();

  // Updates the block autoplay enabled pref when the UI is toggled.
  void HandleSetBlockAutoplayEnabled(const base::Value::List& args);

  // Clear web storage data and cookies from CookiesTreeModel for a site group.
  void HandleClearSiteGroupDataAndCookies(const base::Value::List& args);

  // Record metrics for actions on All Sites Page.
  void HandleRecordAction(const base::Value::List& args);

  // Gets a plural string for the given number of cookies.
  void HandleGetNumCookiesString(const base::Value::List& args);

  // Provides an opportunity for site data which is not integrated into the
  // tree model to be removed when entries for |origins| are removed.
  // TODO(crbug.com/1271155): This function is a temporary hack while the
  // CookiesTreeModel is deprecated.
  void RemoveNonTreeModelData(const std::vector<url::Origin>& origins);

  void SetModelsForTesting(
      std::unique_ptr<CookiesTreeModel> cookies_tree_model,
      std::unique_ptr<BrowsingDataModel> browsing_data_model);

  void ClearAllSitesMapForTesting();

  // Notifies the JS side the effective cookies setting has changed and
  // provides the updated description label for display.
  void SendCookieSettingDescription();

  // Returns a dictionary containing the lists of the allowed permission
  // grant objects granted via the File System Access API, per origin.
  base::Value::List PopulateFileSystemGrantData();

  // Sends the list of notification permissions to review to the WebUI.
  void SendNotificationPermissionReviewList();

  const raw_ptr<Profile, DanglingUntriaged> profile_;

  base::ScopedMultiSourceObservation<Profile, ProfileObserver>
      observed_profiles_{this};

  // Keeps track of events related to zooming.
  std::vector<base::CallbackListSubscription> host_zoom_map_subscriptions_;

  // The origin for which to fetch usage.
  std::string usage_origin_;

  // Change observer for content settings.
  base::ScopedMultiSourceObservation<HostContentSettingsMap,
                                     content_settings::Observer>
      observations_{this};

  // Change observer for chooser permissions.
  base::ScopedMultiSourceObservation<
      permissions::ObjectPermissionContextBase,
      permissions::ObjectPermissionContextBase::PermissionObserver>
      chooser_observations_{this};

  // Change observer for prefs.
  std::unique_ptr<PrefChangeRegistrar> pref_change_registrar_;

  // Models which power the UI.
  std::unique_ptr<CookiesTreeModel> cookies_tree_model_;
  std::unique_ptr<BrowsingDataModel> browsing_data_model_;

  int num_models_being_built_ = 0;

  // Whether the models was set for testing. Allows the handler to avoid
  // resetting the models.
  bool models_set_for_testing_ = false;

  // Populated every time the user reloads the All Sites page.
  // Maps GroupingKeys to sets of (origin, is_partitioned) pairs.
  AllSitesMap all_sites_map_;

  // Stores the origins that have permission settings.
  std::set<url::Origin> origin_permission_set_;

  // Whether to send all sites list on model update.
  bool send_sites_list_ = false;

  // Whether to send site detail data on model update.
  bool update_site_details_ = false;

  // Time when all sites list was requested. Used to record metrics on how long
  // does it take to fetch storage.
  base::TimeTicks request_started_time_;

  base::WeakPtrFactory<SiteSettingsHandler> weak_ptr_factory_{this};
};

}  // namespace settings

#endif  // CHROME_BROWSER_UI_WEBUI_SETTINGS_SITE_SETTINGS_HANDLER_H_
