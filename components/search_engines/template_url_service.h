// Copyright 2014 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_SEARCH_ENGINES_TEMPLATE_URL_SERVICE_H_
#define COMPONENTS_SEARCH_ENGINES_TEMPLATE_URL_SERVICE_H_

#include <stddef.h>

#include <map>
#include <memory>
#include <set>
#include <string>
#include <utility>
#include <vector>

#include "base/callback_list.h"
#include "base/functional/callback.h"
#include "base/functional/callback_forward.h"
#include "base/gtest_prod_util.h"
#include "base/memory/raw_ptr.h"
#include "base/memory/weak_ptr.h"
#include "base/observer_list.h"
#include "base/time/default_clock.h"
#include "base/time/time.h"
#include "build/build_config.h"
#include "build/chromeos_buildflags.h"
#include "components/keyed_service/core/keyed_service.h"
#include "components/prefs/pref_change_registrar.h"
#include "components/search_engines/choice_made_location.h"
#include "components/search_engines/default_search_manager.h"
#include "components/search_engines/enterprise/enterprise_site_search_manager.h"
#include "components/search_engines/keyword_web_data_service.h"
#include "components/search_engines/search_host_to_urls_map.h"
#include "components/search_engines/search_terms_data.h"
#include "components/search_engines/template_url.h"
#include "components/sync/model/sync_change.h"
#include "components/sync/model/syncable_service.h"
#include "components/sync/protocol/search_engine_specifics.pb.h"
#include "components/webdata/common/web_data_service_consumer.h"
#if BUILDFLAG(IS_ANDROID)
#include "base/android/scoped_java_ref.h"
#endif

class GURL;
class PrefService;
class TemplateURLServiceClient;
class TemplateURLServiceObserver;
struct TemplateURLData;
#if BUILDFLAG(IS_ANDROID)
class TemplateUrlServiceAndroid;
#endif

namespace search_engines {
class SearchEngineChoiceService;
class ChoiceScreenData;
}

namespace syncer {
class SyncData;
}

namespace user_prefs {
class PrefRegistrySyncable;
}

namespace url {
class Origin;
}

// TemplateURLService is the backend for keywords. It's used by
// KeywordAutocomplete.
//
// TemplateURLService stores a vector of TemplateURLs. The TemplateURLs are
// persisted to the database maintained by KeywordWebDataService.
// *ALL* mutations to the TemplateURLs must funnel through TemplateURLService.
// This allows TemplateURLService to notify listeners of changes as well as keep
// the database in sync.
//
// TemplateURLService does not load the vector of TemplateURLs in its
// constructor (except for testing). Use the Load method to trigger a load.
// When TemplateURLService has completed loading, observers are notified via
// OnTemplateURLServiceChanged, or by a callback registered prior to calling
// the Load method.
//
// TemplateURLService takes ownership of any TemplateURL passed to it. If there
// is a KeywordWebDataService, deletion is handled by KeywordWebDataService,
// otherwise TemplateURLService handles deletion.

class TemplateURLService final : public WebDataServiceConsumer,
                                 public KeyedService,
                                 public syncer::SyncableService {
 public:
  using QueryTerms = std::map<std::string, std::string>;
  using TemplateURLVector = TemplateURL::TemplateURLVector;
  using OwnedTemplateURLVector = TemplateURL::OwnedTemplateURLVector;
  using SyncDataMap = std::map<std::string, syncer::SyncData>;
  using OwnedTemplateURLDataVector =
      EnterpriseSiteSearchManager::OwnedTemplateURLDataVector;

  static constexpr char kSiteSearchPolicyConflictCountHistogramName[] =
      "Search.SiteSearchPolicyConflict";
  static constexpr char
      kSiteSearchPolicyHasConflictWithFeaturedHistogramName[] =
          "Search.SiteSearchPolicyConflict.HasConflictWith.WithFeatured";
  static constexpr char
      kSiteSearchPolicyHasConflictWithNonFeaturedHistogramName[] =
          "Search.SiteSearchPolicyConflict.HasConflictWith.WithNonFeatured";

  // Struct used for initializing the data store with fake data.
  // Each initializer is mapped to a TemplateURL.
  struct Initializer {
    const char* const keyword;
    const char* const url;
    const char* const content;
  };

  struct URLVisitedDetails {
    GURL url;
    bool is_keyword_transition;
  };

  // Search metadata that's often used to persist into History.
  struct SearchMetadata {
    raw_ptr<const TemplateURL> template_url;
    GURL normalized_url;
    std::u16string search_terms;
  };

  // Values for an enumerated histogram used to track keyword conflicts between
  // search engines created by the SiteSearchSettings policy and search engines
  // the user manually edited. Keep in sync with `SiteSearchPolicyConflictType`
  // in tools/metrics/histograms/enums.xml.
  enum class SiteSearchPolicyConflictType {
    kNone = 0,
    kWithFeatured = 1,
    kWithNonFeatured = 2,
    kMaxValue = kWithNonFeatured,
  };

  TemplateURLService(
      PrefService& prefs,
      search_engines::SearchEngineChoiceService& search_engine_choice_service,
      std::unique_ptr<SearchTermsData> search_terms_data,
      const scoped_refptr<KeywordWebDataService>& web_data_service,
      std::unique_ptr<TemplateURLServiceClient> client,
      const base::RepeatingClosure& dsp_change_callback
#if BUILDFLAG(IS_CHROMEOS_LACROS)
      ,
      bool for_lacros_main_profile
#endif  // BUILDFLAG(IS_CHROMEOS_LACROS)
  );

  // For testing only. `initializers` will be used to simulate having loaded
  // some template URL data.
  explicit TemplateURLService(
      PrefService& prefs,
      search_engines::SearchEngineChoiceService& search_engine_choice_service,
      base::span<const TemplateURLService::Initializer> initializers = {});

  TemplateURLService(const TemplateURLService&) = delete;
  TemplateURLService& operator=(const TemplateURLService&) = delete;

  ~TemplateURLService() override;

  // Register Profile preferences in |registry|.
  static void RegisterProfilePrefs(user_prefs::PrefRegistrySyncable* registry);

#if BUILDFLAG(IS_ANDROID)
  base::android::ScopedJavaLocalRef<jobject> GetJavaObject();
#endif

  // Returns true if there is no TemplateURL that conflicts with the
  // keyword/url pair, or there is one but it can be replaced.
  //
  // |url| is the URL of the search query.  This is used to prevent auto-adding
  // a keyword for hosts already associated with a manually-edited keyword.
  bool CanAddAutogeneratedKeyword(const std::u16string& keyword,
                                  const GURL& url);

  // Returns whether the engine is a "pre-existing" engine, either from the
  // prepopulate list or created by DefaultSearchProvider* policies.
  bool IsPrepopulatedOrDefaultProviderByPolicy(
      const TemplateURL* template_url) const;

  // Returns whether |template_url| should be shown in the list of engines
  // most likely to be selected as a default engine. This is meant to highlight
  // the current default, as well as the other most likely choices of default
  // engine, separately from a full list of all TemplateURLs (which might be
  // very long).
  bool ShowInDefaultList(const TemplateURL* template_url) const;

  // Returns whether |template_url| should be shown in the list of active
  // engines, including active search engines and search engines created by
  // the SiteSearchSettings policy.
  bool ShowInActivesList(const TemplateURL* template_url) const;

  // Returns whether |template_url| should be hidden from all lists of engines.
  bool HiddenFromLists(const TemplateURL* template_url) const;

  // Returns true if `template_url` corresponds to a featured Enterprise site
  // search engine (e.g. with keyword "@work") that hides the corresponding
  // non-featured engine (e.g. with keyword "work") in the Settings page.
  bool FeaturedOverridesNonFeatured(const TemplateURL* template_url) const;

  // Adds to |matches| all TemplateURLs whose keywords begin with |prefix|,
  // sorted shortest-keyword-first. If |supports_replacement_only| is true, only
  // TemplateURLs that support replacement are returned. This method must be
  // efficient, since it's run roughly once per omnibox keystroke.
  void AddMatchingKeywords(const std::u16string& prefix,
                           bool supports_replacement_only,
                           TemplateURLVector* matches);

  // Looks up |keyword| and returns the best TemplateURL for it.  Returns
  // nullptr if the keyword was not found. The caller should not try to delete
  // the returned pointer; the data store retains ownership of it.
  TemplateURL* GetTemplateURLForKeyword(const std::u16string& keyword);
  const TemplateURL* GetTemplateURLForKeyword(
      const std::u16string& keyword) const;

  // Returns that TemplateURL with the specified GUID, or NULL if not found.
  // The caller should not try to delete the returned pointer; the data store
  // retains ownership of it.
  TemplateURL* GetTemplateURLForGUID(const std::string& sync_guid);
  const TemplateURL* GetTemplateURLForGUID(const std::string& sync_guid) const;

  // Returns the best TemplateURL found with a URL using the specified |host|,
  // or nullptr if there are no such TemplateURLs.
  TemplateURL* GetTemplateURLForHost(const std::string& host);
  const TemplateURL* GetTemplateURLForHost(const std::string& host) const;

  // Returns the TemplateURL corresponding to |starter_pack_id|, if any.
  TemplateURL* FindStarterPackTemplateURL(int starter_pack_id);

  // Returns the number of TemplateURLs that match `host`. Used for logging.
  // Caller must ensure TemplateURLService is loaded before calling this.
  // TODO(crbug.com/40224222): Delete after bug is fixed.
  size_t GetTemplateURLCountForHostForLogging(const std::string& host) const;

  // Adds a new TemplateURL to this model.
  //
  // This function guarantees that on return the model will not have two non-
  // extension TemplateURLs with the same keyword.  If that means that it cannot
  // add the provided argument, it will return null.  Otherwise it will return
  // the raw pointer to the TemplateURL.
  //
  // Returns a raw pointer to |template_url| if the addition succeeded, or null
  // on failure.  (Many callers need still need a raw pointer to the TemplateURL
  // so they can access it later.)
  TemplateURL* Add(std::unique_ptr<TemplateURL> template_url);

  // Like Add(), but overwrites the |template_url|'s values with the provided
  // ones.
  TemplateURL* AddWithOverrides(std::unique_ptr<TemplateURL> template_url,
                                const std::u16string& short_name,
                                const std::u16string& keyword,
                                const std::string& url);

  // Removes the keyword from the model. This deletes the supplied TemplateURL.
  // This fails if the supplied template_url is the default search provider.
  void Remove(const TemplateURL* template_url);

  // Removes any TemplateURL of the specified |type| associated with
  // |extension_id|. Unlike with Remove(), this can be called when the
  // TemplateURL in question is the current default search provider.
  void RemoveExtensionControlledTURL(const std::string& extension_id,
                                     TemplateURL::Type type);

  // Removes all auto-generated keywords that were created in the specified
  // range.
  void RemoveAutoGeneratedBetween(base::Time created_after,
                                  base::Time created_before);

  // Removes all auto-generated keywords that were created in the specified
  // range and match |url_filter|. If |url_filter| is_null(), deletes all
  // auto-generated keywords in the range.
  void RemoveAutoGeneratedForUrlsBetween(
      const base::RepeatingCallback<bool(const GURL&)>& url_filter,
      base::Time created_after,
      base::Time created_before);

  // Adds a TemplateURL for an extension with an omnibox keyword.
  // Only 1 keyword is allowed for a given extension. If a keyword
  // already exists for this extension, does nothing.
  void RegisterOmniboxKeyword(const std::string& extension_id,
                              const std::string& extension_name,
                              const std::string& keyword,
                              const std::string& template_url_string,
                              const base::Time& extension_install_time);

  // Returns the set of URLs describing the keywords. The elements are owned
  // by TemplateURLService and should not be deleted.
  TemplateURLVector GetTemplateURLs();

  // Returns key information needed to show a search engine choice screen, like
  // the template URLs for the engines to show. See
  // `search_engines::ChoiceScreenData` for more details.
  std::unique_ptr<search_engines::ChoiceScreenData> GetChoiceScreenData();

  TemplateURLService::TemplateURLVector GetFeaturedEnterpriseSearchEngines()
      const;

#if BUILDFLAG(IS_ANDROID)
  // Returns the list prepopulated template URLs for `country_code`.
  // `country_code` is a two-character uppercase ISO 3166-1 country code.
  // Usage restricted to Android. Other platforms should rely on the other
  // functions that will return this data for the profile's current country.
  OwnedTemplateURLDataVector GetTemplateURLsForCountry(
      const std::string& country_code);
#endif

  // Increment the usage count of a keyword.
  // Called when a URL is loaded that was generated from a keyword.
  void IncrementUsageCount(TemplateURL* url);

  // Resets the title, keyword and search url of the specified TemplateURL.
  // The TemplateURL is marked as not replaceable.
  void ResetTemplateURL(TemplateURL* url,
                        const std::u16string& title,
                        const std::u16string& keyword,
                        const std::string& search_url);

  // Sets the `is_active` field of the specified TemplateURL to `kTrue` or
  // `kFalse`. Called when a user explicitly activates/deactivates the search
  // engine.
  void SetIsActiveTemplateURL(TemplateURL* url, bool is_active);

#if BUILDFLAG(IS_ANDROID)
  // Creates a `TemplateURLData` from the provided raw data, and marks it as
  // coming from an Play / Android OS-level search engine choice screen.
  static TemplateURLData CreatePlayAPITemplateURLData(
      const std::u16string& keyword,
      const std::u16string& name,
      const std::string& search_url,
      const std::string& suggest_url = std::string(),
      const std::string& favicon_url = std::string(),
      const std::string& new_tab_url = std::string(),
      const std::string& image_url = std::string(),
      const std::string& image_url_post_params = std::string(),
      const std::string& image_translate_url = std::string(),
      const std::string& image_translate_source_language_param_key =
          std::string(),
      const std::string& image_translate_target_language_param_key =
          std::string());

  // Register a new search provider from `new_play_api_turl_data` and sets
  // it as the default search provider.
  //
  // If there is already existing search provider that was created from Play,
  // it will be removed.
  bool ResetPlayAPISearchEngine(const TemplateURLData& new_play_api_turl_data);
#endif  // BUILDFLAG(IS_ANDROID)

  // Updates any search providers matching |potential_search_url| with the new
  // favicon location |favicon_url|.
  void UpdateProviderFavicons(const GURL& potential_search_url,
                              const GURL& favicon_url);

  // Return true if the given |url| can be made the default. This returns false
  // regardless of |url| if the default search provider is managed by policy or
  // controlled by an extension.
  bool CanMakeDefault(const TemplateURL* url) const;

  // Set the default search provider. `url` may be null.
  // This will assert if the default search is managed; the UI should not be
  // invoking this method in that situation.
  // `choice_made_location` indicates in which context the user made the
  // selection, which will affect how some prefs are set and record additional
  // metrics.
  void SetUserSelectedDefaultSearchProvider(
      TemplateURL* url,
      search_engines::ChoiceMadeLocation choice_made_location =
          search_engines::ChoiceMadeLocation::kOther);

  // Returns the default search provider. If the TemplateURLService hasn't been
  // loaded, the default search provider is pulled from preferences.
  //
  // NOTE: This may return null in certain circumstances such as:
  //       1.) Unit test mode
  //       2.) The default search engine is disabled by policy.
  const TemplateURL* GetDefaultSearchProvider() const;

  // Returns the Origin of the user's default search engine. If a default search
  // engine is set and its URL is valid, the Origin of that URL is returned.
  // Otherwise, an opaque Origin with a unique nonce is returned.
  url::Origin GetDefaultSearchProviderOrigin() const;

  // Returns the default search provider, ignoring any that were provided by an
  // extension.
  const TemplateURL* GetDefaultSearchProviderIgnoringExtensions() const;

  // Returns true if the |url| is a search results page from the default search
  // provider.
  bool IsSearchResultsPageFromDefaultSearchProvider(const GURL& url) const;

  // Generates a search results page URL for the default search provider with
  // the given search terms. Returns an empty GURL if the default search
  // provider is not available.
  GURL GenerateSearchURLForDefaultSearchProvider(
      const std::u16string& search_terms) const;

  // Returns search metadata if |url| is a valid Search URL.
  std::optional<SearchMetadata> ExtractSearchMetadata(const GURL& url) const;

  // Returns true if the default search provider supports the side search
  // feature.
  bool IsSideSearchSupportedForDefaultSearchProvider() const;

  // Returns true if the default search provider supports the opening
  // image search requests in the side panel.
  bool IsSideImageSearchSupportedForDefaultSearchProvider() const;

  // Generates a side search URL for the default search provider's search url.
  GURL GenerateSideSearchURLForDefaultSearchProvider(
      const GURL& search_url,
      const std::string& version) const;

  // Takes a search URL that belongs to this side search in the side panel and
  // removes the side search param from the URL.
  GURL RemoveSideSearchParamFromURL(const GURL& side_search_url) const;

  // Generates a side image search URL for the default search provider's image
  // search url.
  GURL GenerateSideImageSearchURLForDefaultSearchProvider(
      const GURL& image_search_url,
      const std::string& version) const;

  // Takes a search URL that belongs to this image search in the side panel and
  // removes the side image search param from the URL.
  GURL RemoveSideImageSearchParamFromURL(const GURL& image_search_url) const;

  // Returns true if the default search is managed through group policy.
  bool is_default_search_managed() const {
    return default_search_provider_source_ == DefaultSearchManager::FROM_POLICY;
  }

  // Returns true if the default search provider is controlled by an extension.
  bool IsExtensionControlledDefaultSearch() const;

  // Returns the default search specified in the prepopulated data, if it
  // exists.  If not, returns first URL in |template_urls_|, or NULL if that's
  // empty. The returned object is owned by TemplateURLService and can be
  // destroyed at any time so should be used right after the call.
  TemplateURL* FindNewDefaultSearchProvider();

  // Performs the same actions that happen when the prepopulate data version is
  // revved: all existing prepopulated entries are checked against the current
  // prepopulate data, any now-extraneous safe_for_autoreplace() entries are
  // removed, any existing engines are reset to the provided data (except for
  // user-edited names or keywords), and any new prepopulated engines are
  // added.
  //
  // After this, the default search engine is reset to the default entry in the
  // prepopulate data.
  void RepairPrepopulatedSearchEngines();

  // Performs the same actions that happen when the starter pack data version is
  // revved: all existing starter pack entries are checked against the current
  // starter pack data, any now-extraneous safe_for_autoreplace() entries are
  // removed, any existing engines are reset to the provided data (except for
  // user-edited names or keywords), and any new starter pack engines are
  // added.  Unlike `RepairPrepopulatedSearchEngines()`, this does not modify
  // the default search engine entry.
  void RepairStarterPackEngines();

  // Observers used to listen for changes to the model.
  // TemplateURLService does NOT delete the observers when deleted.
  void AddObserver(TemplateURLServiceObserver* observer);
  void RemoveObserver(TemplateURLServiceObserver* observer);

  // Loads the keywords. This has no effect if the keywords have already been
  // loaded.
  // Observers are notified when loading completes via the method
  // OnTemplateURLServiceChanged.
  void Load();

  // Registers a callback to be called when the service has loaded.
  //
  // If the service has already loaded, this function does nothing.
  base::CallbackListSubscription RegisterOnLoadedCallback(
      base::OnceClosure callback);

#if defined(UNIT_TEST)
  void set_loaded(bool value) { loaded_ = value; }

  // Turns Load() into a no-op.
  void set_disable_load(bool value) { disable_load_ = value; }
#endif

  // Whether or not the keywords have been loaded.
  bool loaded() const { return loaded_; }

  // Notification that the keywords have been loaded.
  // This is invoked from WebDataService, and should not be directly
  // invoked.
  void OnWebDataServiceRequestDone(
      KeywordWebDataService::Handle h,
      std::unique_ptr<WDTypedResult> result) override;

  // Returns the locale-direction-adjusted short name for the given keyword.
  // Also sets the out param to indicate whether the keyword belongs to an
  // Omnibox extension or the Gemini starter pack engine.
  std::u16string GetKeywordShortName(const std::u16string& keyword,
                                     bool* is_omnibox_api_extension_keyword,
                                     bool* is_gemini_keyword) const;

  // Called by the history service when a URL is visited.
  void OnHistoryURLVisited(const URLVisitedDetails& details);

  // KeyedService implementation.
  void Shutdown() override;

  // syncer::SyncableService implementation.

  // Waits until keywords have been loaded.
  void WaitUntilReadyToSync(base::OnceClosure done) override;

  // Returns all syncable TemplateURLs from this model as SyncData. This should
  // include every search engine and no Extension keywords.
  syncer::SyncDataList GetAllSyncData(syncer::DataType type) const;
  // Process new search engine changes from Sync, merging them into our local
  // data. This may send notifications if local search engines are added,
  // updated or removed.
  std::optional<syncer::ModelError> ProcessSyncChanges(
      const base::Location& from_here,
      const syncer::SyncChangeList& change_list) override;
  // Merge initial search engine data from Sync and push any local changes up
  // to Sync. This may send notifications if local search engines are added,
  // updated or removed.
  std::optional<syncer::ModelError> MergeDataAndStartSyncing(
      syncer::DataType type,
      const syncer::SyncDataList& initial_sync_data,
      std::unique_ptr<syncer::SyncChangeProcessor> sync_processor) override;
  void StopSyncing(syncer::DataType type) override;
  base::WeakPtr<SyncableService> AsWeakPtr() override;

  // Processes a local TemplateURL change for Sync. |turl| is the TemplateURL
  // that has been modified, and |type| is the Sync ChangeType that took place.
  // This may send a new SyncChange to the cloud. If our model has not yet been
  // associated with Sync, or if this is triggered by a Sync change, then this
  // does nothing.
  void ProcessTemplateURLChange(const base::Location& from_here,
                                const TemplateURL* turl,
                                syncer::SyncChange::SyncChangeType type);

  // Returns whether the device is from an EEA country. This is consistent with
  // countries which are eligible for the EEA default search engine choice
  // prompt. "Default country" or "country at install" are used for
  // SearchEngineChoiceCountry. It might be different than what LocaleUtils
  // returns.
  bool IsEeaChoiceCountry();

  // Returns a SearchTermsData which can be used to call TemplateURL methods.
  const SearchTermsData& search_terms_data() const {
    return *search_terms_data_;
  }

  // Calls `ApplyDefaultSearchChangeNoMetrics`. Used for testing.
  bool ApplyDefaultSearchChangeForTesting(const TemplateURLData* data,
                                          DefaultSearchManager::Source source);

  // Obtains a session token, regenerating if necessary.
  std::string GetSessionToken();

  // Clears the session token. Should be called when the user clears browsing
  // data.
  void ClearSessionToken();

  // Explicitly converts from ActiveStatus enum in sync protos to enum in
  // TemplateURLData.
  static TemplateURLData::ActiveStatus ActiveStatusFromSync(
      sync_pb::SearchEngineSpecifics_ActiveStatus is_active);

  // Explicitly converts from ActiveStatus enum in TemplateURLData to enum in
  // sync protos.
  static sync_pb::SearchEngineSpecifics_ActiveStatus ActiveStatusToSync(
      TemplateURLData::ActiveStatus is_active);

  // Returns a SyncData with a sync representation of the search engine data
  // from |turl|.
  static syncer::SyncData CreateSyncDataFromTemplateURL(
      const TemplateURL& turl);

  // Creates a new heap-allocated TemplateURL* which is populated by overlaying
  // |sync_data| atop |existing_turl|.  |existing_turl| may be NULL; if not it
  // remains unmodified.  The caller owns the returned TemplateURL*.
  //
  // If the created TemplateURL is migrated in some way from out-of-date sync
  // data, an appropriate SyncChange is added to |change_list|.  If the sync
  // data is bad for some reason, an ACTION_DELETE change is added and the
  // function returns NULL.
  // `search_engine_choice_service` is used to obtain the country code (for the
  // list of prepopulated engines).
  static std::unique_ptr<TemplateURL>
  CreateTemplateURLFromTemplateURLAndSyncData(
      TemplateURLServiceClient* client,
      PrefService* prefs,
      search_engines::SearchEngineChoiceService* search_engine_choice_service,
      const SearchTermsData& search_terms_data,
      const TemplateURL* existing_turl,
      const syncer::SyncData& sync_data,
      syncer::SyncChangeList* change_list);

  // Returns a map mapping Sync GUIDs to pointers to syncer::SyncData.
  static SyncDataMap CreateGUIDToSyncDataMap(
      const syncer::SyncDataList& sync_data);

#if defined(UNIT_TEST)
  void set_clock(std::unique_ptr<base::Clock> clock) {
    clock_ = std::move(clock);
  }
#endif

 private:
  FRIEND_TEST_ALL_PREFIXES(TemplateURLServiceTest, TestManagedDefaultSearch);
  FRIEND_TEST_ALL_PREFIXES(TemplateURLServiceTest,
                           UpdateKeywordSearchTermsForURL);
  FRIEND_TEST_ALL_PREFIXES(TemplateURLServiceTest,
                           DontUpdateKeywordSearchForNonReplaceable);
  FRIEND_TEST_ALL_PREFIXES(TemplateURLServiceTest, ChangeGoogleBaseValue);
  FRIEND_TEST_ALL_PREFIXES(TemplateURLServiceTest, MergeDeletesUnusedProviders);
  FRIEND_TEST_ALL_PREFIXES(TemplateURLServiceTest, AddOmniboxExtensionKeyword);
  FRIEND_TEST_ALL_PREFIXES(TemplateURLServiceTest, ExtensionsWithSameKeywords);
  FRIEND_TEST_ALL_PREFIXES(TemplateURLServiceTest,
                           KeywordConflictNonReplaceableEngines);
  FRIEND_TEST_ALL_PREFIXES(TemplateURLServiceTest, LastVisitedTimeUpdate);
  FRIEND_TEST_ALL_PREFIXES(TemplateURLServiceTest,
                           RepairPrepopulatedSearchEngines);
  FRIEND_TEST_ALL_PREFIXES(TemplateURLServiceTest, RepairStarterPackEngines);
  FRIEND_TEST_ALL_PREFIXES(TemplateURLServiceSyncTest, PreSyncDeletes);
  FRIEND_TEST_ALL_PREFIXES(TemplateURLServiceSyncTest, MergeInSyncTemplateURL);
  FRIEND_TEST_ALL_PREFIXES(LocationBarModelTest, GoogleBaseURL);
  FRIEND_TEST_ALL_PREFIXES(TemplateURLServiceUnitTest, SessionToken);

  friend class InstantUnitTestBase;
  friend class Scoper;
  friend class TemplateURLServiceTestUtil;
  friend class TemplateUrlServiceAndroid;

  using GUIDToTURL =
      std::map<std::string, raw_ptr<TemplateURL, CtnExperimental>>;

  // A mapping from keywords to the corresponding TemplateURLs.
  // This is a multimap, so the system can
  // efficiently tolerate multiple engines with the same keyword, like from
  // extensions.
  //
  // The values for any given keyword are not sorted. Users that want the best
  // value for each key must traverse through all matching items. The vast
  // majority of keywords should only have one item.
  using KeywordToTURL =
      std::multimap<std::u16string, raw_ptr<TemplateURL, CtnExperimental>>;

  // Declaration of values to be used in an enumerated histogram to tally
  // changes to the default search provider from various entry points. In
  // particular, we use this to see what proportion of changes are from Sync
  // entry points, to help spot erroneous Sync activity.
  enum DefaultSearchChangeOrigin {
    // Various known Sync entry points.
    DSP_CHANGE_SYNC_PREF,
    DSP_CHANGE_SYNC_ADD,
    DSP_CHANGE_SYNC_DELETE,
    DSP_CHANGE_SYNC_NOT_MANAGED,
    // "Other" origins. We differentiate between Sync and not Sync so we know if
    // certain changes were intentionally from the system, or possibly some
    // unintentional change from when we were Syncing.
    DSP_CHANGE_SYNC_UNINTENTIONAL,
    // All changes that don't fall into another category; we can't reorder the
    // list for clarity as this would screw up stat collection.
    DSP_CHANGE_OTHER,
    // Changed through "Profile Reset" feature.
    DSP_CHANGE_PROFILE_RESET,
    // Changed by an extension through the Override Settings API.
    DSP_CHANGE_OVERRIDE_SETTINGS_EXTENSION,
    // New DSP during database/prepopulate data load, which was not previously
    // in the known engine set, and with no previous value in prefs.  The
    // typical time to see this is during first run.
    DSP_CHANGE_NEW_ENGINE_NO_PREFS,
    // Boundary value.
    DSP_CHANGE_MAX,
  };

  // Helper functor for FindMatchingKeywords(), for finding the range of
  // keywords which begin with a prefix.
  class LessWithPrefix;

  // Used to defer notifications until the last Scoper is destroyed by leaving
  // the scope of a code block.
  class Scoper;

  // Helper class that stores DSP and enterprise site search engines set before
  // the keywords table has been fully loaded.
  class PreLoadingProviders;

  void Init();

  // Simulate a loaded `TemplateURLService`.
  void ApplyInitializersForTesting(
      base::span<const TemplateURLService::Initializer> initializers);

  // Removes |template_url| from various internal maps
  // (|keyword_to_turl_|, |guid_to_turl_|, |provider_map_|).
  void RemoveFromMaps(const TemplateURL* template_url);

  // Adds |template_url| to various internal maps
  // (|keyword_to_turl_|, |guid_to_turl_|, |provider_map_|) if
  // appropriate.  (It might not be appropriate if, for instance,
  // |template_url|'s keyword conflicts with the keyword of a custom search
  // engine already existing in the maps that is not allowed to be replaced.)
  void AddToMaps(TemplateURL* template_url);

  // Sets the keywords. This is used once the keywords have been loaded.
  // This does NOT notify the delegate or the database.
  void SetTemplateURLs(std::unique_ptr<OwnedTemplateURLVector> urls);

  // Transitions to the loaded state.
  void ChangeToLoadedState();

  // Applies a DSE change and reports metrics if appropriate.
  void ApplyDefaultSearchChange(const TemplateURLData* new_dse_data,
                                DefaultSearchManager::Source source);

  // Applies site search changes and reports metrics if appropriate.
  void EnterpriseSiteSearchChanged(
      OwnedTemplateURLDataVector&& policy_site_search_engines);

  // Applies a DSE change. May be called at startup or after transitioning to
  // the loaded state. Returns true if a change actually occurred.
  bool ApplyDefaultSearchChangeNoMetrics(const TemplateURLData* new_dse_data,
                                         DefaultSearchManager::Source source);

  // Applies changes due to Enterprise policy `SiteSearchSettings`. Called after
  // transitioning to the loaded state.
  void ApplyEnterpriseSiteSearchChanges(
      OwnedTemplateURLVector&& policy_site_search_engines);

  // Returns false if there is a TemplateURL that has a search url with the
  // specified host and that TemplateURL has been manually modified.
  bool CanAddAutogeneratedKeywordForHost(const std::string& host) const;

  // Updates the information in |existing_turl| using the information from
  // |new_values|, but the ID for |existing_turl| is retained. Returns whether
  // |existing_turl| was found in |template_urls_| and thus could be updated.
  //
  // NOTE: This should not be called with an extension keyword as there are no
  // updates needed in that case.
  bool Update(TemplateURL* existing_turl, const TemplateURL& new_values);

  // If the TemplateURL comes from a prepopulated URL available in the current
  // country, update all its fields save for the keyword, short name and id so
  // that they match the internal prepopulated URL. TemplateURLs not coming from
  // a prepopulated URL are not modified.
  static void UpdateTemplateURLIfPrepopulated(
      TemplateURL* existing_turl,
      PrefService* prefs,
      search_engines::SearchEngineChoiceService* search_engine_choice_service);

  // If the TemplateURL's sync GUID matches the kSyncedDefaultSearchProviderGUID
  // preference it will be used to update the DSE in prefs.
  // OnDefaultSearchChange may be triggered as a result.
  void MaybeUpdateDSEViaPrefs(TemplateURL* synced_turl);

  // Iterates through the TemplateURLs to see if one matches the visited url.
  // For each TemplateURL whose url matches the visited url
  // SetKeywordSearchTermsForURL is invoked.
  void UpdateKeywordSearchTermsForURL(const URLVisitedDetails& details);

  // Updates the last_visited time of |url| to the current time.
  void UpdateTemplateURLVisitTime(TemplateURL* url);

  // If necessary, generates a visit for the site http:// + t_url.keyword().
  void AddTabToSearchVisit(const TemplateURL& t_url);

  // Adds a new TemplateURL to this model.
  //
  // If |newly_adding| is false, we assume that this TemplateURL was already
  // part of the model in the past, and therefore we don't need to do things
  // like assign it an ID or notify sync.
  //
  // This function guarantees that on return the model will not have two non-
  // extension TemplateURLs with the same keyword.  If that means that it cannot
  // add the provided argument, it will return null.  Otherwise it will return
  // the raw pointer to the TemplateURL.
  //
  // Returns a raw pointer to |template_url| if the addition succeeded, or null
  // on failure.  (Many callers need still need a raw pointer to the TemplateURL
  // so they can access it later.)
  TemplateURL* Add(std::unique_ptr<TemplateURL> template_url,
                   bool newly_adding);

  // Updates |template_urls| so that the only entry corresponding to default
  // provider set by policy is |default_from_prefs|. |default_from_prefs| may be
  // NULL if there is no policy-defined DSE in effect.
  void UpdateDefaultProvidersCreatedByPolicy(
      OwnedTemplateURLVector* template_urls,
      const TemplateURLData* default_from_prefs,
      bool is_mandatory);

  // Resets the sync GUID of the specified TemplateURL and persists the change
  // to the database. This does not notify observers.
  void ResetTemplateURLGUID(TemplateURL* url, const std::string& guid);

  // Adds |sync_turl| into the local model, possibly removing or updating a
  // local TemplateURL to make room for it. This expects |sync_turl| to be a new
  // entry from Sync, not currently known to the local model. |sync_data| should
  // be a SyncDataMap where the contents are entries initially known to Sync
  // during MergeDataAndStartSyncing.
  // Any necessary updates to Sync will be appended to |change_list|. This can
  // include updates on local TemplateURLs, if they are found in |sync_data|.
  // |initial_data| should be a SyncDataMap of the entries known to the local
  // model during MergeDataAndStartSyncing. If |sync_turl| replaces a local
  // entry, that entry is removed from |initial_data| to prevent it from being
  // sent up to Sync.
  // This should only be called from MergeDataAndStartSyncing.
  void MergeInSyncTemplateURL(TemplateURL* sync_turl,
                              const SyncDataMap& sync_data,
                              syncer::SyncChangeList* change_list,
                              SyncDataMap* local_data);

  // Goes through a vector of TemplateURLs and ensure that both the in-memory
  // and database copies have valid sync_guids. This is to fix crbug.com/102038,
  // where old entries were being pushed to Sync without a sync_guid.
  void PatchMissingSyncGUIDs(OwnedTemplateURLVector* template_urls);

  // Called when `kSyncedDefaultSearchProviderGUID` or
  // `kDefaultSearchProviderGUID` is modified.
  void OnDefaultSearchProviderGUIDChanged();

  // Goes through a vector of TemplateURLs and sets is_active to true if it was
  // not previously set (currently kUnspecified) and has been interacted with
  // by the user.
  void MaybeSetIsActiveSearchEngines(OwnedTemplateURLVector* template_urls);

  // Adds to |matches| all TemplateURLs stored in |keyword_to_turl|
  // whose keywords begin with |prefix|, sorted shortest-keyword-first.  If
  // |supports_replacement_only| is true, only TemplateURLs that support
  // replacement are returned.
  template <typename Container>
  void AddMatchingKeywordsHelper(const Container& keyword_to_turl,
                                 const std::u16string& prefix,
                                 bool supports_replacement_only,
                                 TemplateURLVector* matches);

  // Returns the TemplateURL corresponding to |prepopulated_id|, if any.
  TemplateURL* FindPrepopulatedTemplateURL(int prepopulated_id);

  // Returns the TemplateURL associated with |extension_id|, if any.
  TemplateURL* FindTemplateURLForExtension(const std::string& extension_id,
                                           TemplateURL::Type type);

  // Finds any NORMAL_CONTROLLED_BY_EXTENSION engine that matches |data| and
  // wants to be default. Returns nullptr if not found.
  TemplateURL* FindMatchingDefaultExtensionTemplateURL(
      const TemplateURLData& data);

  // This method removes all TemplateURLs that meet all three criteria:
  //  - Duplicate: Shares the same keyword as |candidate|.
  //  - Replaceable: Engine is eligible for automatic removal. See CanReplace().
  //  - Worse: There exists a better engine with the same keyword.
  //
  // This method must run BEFORE |candidate| is added to the engine list / map.
  // It would be simpler to run the algorithm AFTER |candidate| is added, but
  // that makes extra sync updates, observer notifications, and database churn.
  //
  // This method returns true if |candidate| ITSELF is rendundant.
  // But notably, this method NEVER calls Remove() on |candidate|, leaving the
  // correct handling to its caller.
  bool RemoveDuplicateReplaceableEnginesOf(TemplateURL* candidate);

  // Returns true if |turl| matches the default search provider. This method
  // does both a GUID comparison, because while the model is being loaded, the
  // DSE may be sourced from prefs, and we still want to consider the
  // corresponding database entry a match. https://crbug.com/1164024
  bool MatchesDefaultSearchProvider(TemplateURL* turl) const;

  // Emits the UMA Histogram for the number of search engines that are active
  // and inactive at load time.
  void EmitTemplateURLActiveOnStartupHistogram(
      OwnedTemplateURLVector* template_urls);

  // Returns an instance of |EnterpriseSiteSearchManager| if feature
  // |kSiteSearchSettingsPolicy| or nullptr otherwise.
  std::unique_ptr<EnterpriseSiteSearchManager> GetEnterpriseSiteSearchManager(
      PrefService* prefs);

  // Logs a histogram to track keyword conflicts between search engines created
  // by the SiteSearchSettings policy and search engines the user manually
  // edited.
  void LogSiteSearchPolicyConflict(
      const OwnedTemplateURLVector& policy_site_search_engines);

  // ---------- Browser state related members ---------------------------------
  raw_ref<PrefService> prefs_;

  raw_ref<search_engines::SearchEngineChoiceService>
      search_engine_choice_service_;

  std::unique_ptr<SearchTermsData> search_terms_data_ =
      std::make_unique<SearchTermsData>();

  // ---------- Dependencies on other components ------------------------------
  // Service used to store entries.
  scoped_refptr<KeywordWebDataService> web_data_service_;

  std::unique_ptr<TemplateURLServiceClient> client_;

  // This closure is run when the default search provider is set to Google.
  base::RepeatingClosure dsp_change_callback_;

  PrefChangeRegistrar pref_change_registrar_;

  // Mapping from keyword to the TemplateURL.
  KeywordToTURL keyword_to_turl_;

  // Mapping from Sync GUIDs to the TemplateURL.
  GUIDToTURL guid_to_turl_;

  // Mapping from keyword to TemplateURLs created by the `SiteSearchSettings`
  // policy.
  base::flat_map<std::u16string, raw_ptr<TemplateURL, CtnExperimental>>
      enterprise_site_search_keyword_to_turl_;

  OwnedTemplateURLVector template_urls_;

  base::ObserverList<TemplateURLServiceObserver> model_observers_;

  // Maps from host to set of TemplateURLs whose search url host is host.
  std::unique_ptr<SearchHostToURLsMap> provider_map_ =
      std::make_unique<SearchHostToURLsMap>();

  // Whether the keywords have been loaded.
  bool loaded_ = false;

  // Set when the web data service fails to load properly.  This prevents
  // further communication with sync or writing to prefs, so we don't persist
  // inconsistent state data anywhere.
  bool load_failed_ = false;

  // Whether Load() is disabled. True only in testing contexts.
  bool disable_load_ = false;

  // If non-zero, we're waiting on a load.
  KeywordWebDataService::Handle load_handle_ = 0;

  // All visits that occurred before we finished loading. Once loaded
  // UpdateKeywordSearchTermsForURL is invoked for each element of the vector.
  std::vector<URLVisitedDetails> visits_to_add_;

  // Once loaded, the default search provider.  This is a pointer to a
  // TemplateURL owned by |template_urls_|.
  //
  // TODO(tommycli): Can we combine this with initial_default_search_provider_?
  // Essentially all direct usages of this variable need to first check that
  // |loading_| is true, and should call GetDefaultSearchProvider() instead.
  // Example of a regression due to this mistake: https://crbug.com/1164024.
  raw_ptr<TemplateURL, DanglingUntriaged> default_search_provider_ = nullptr;

  // Temporary location for the DSE and enterprise site search engines until
  // Web Data has been loaded, so it can be merged into `template_urls_`.
  std::unique_ptr<PreLoadingProviders> pre_loading_providers_;

  // Source of the default search provider.
  DefaultSearchManager::Source default_search_provider_source_;

  // ID assigned to next TemplateURL added to this model. This is an ever
  // increasing integer that is initialized from the database.
  TemplateURLID next_id_ = kInvalidTemplateURLID + 1;

  // Used to retrieve the current time, in base::Time units.
  std::unique_ptr<base::Clock> clock_ = std::make_unique<base::DefaultClock>();

  // Do we have an active association between the TemplateURLs and sync models?
  // Set in MergeDataAndStartSyncing, reset in StopSyncing. While this is not
  // set, we ignore any local search engine changes (when we start syncing we
  // will look up the most recent values anyways).
  bool models_associated_ = false;

  // Whether we're currently processing changes from the syncer. While this is
  // true, we ignore any local search engine changes, since we triggered them.
  bool processing_syncer_changes_ = false;

  // We never want reentrancy while applying a default search engine change.
  // This can happen when deleting keyword conflicts. crbug.com/1031506
  bool applying_default_search_engine_change_ = false;

  // Sync's syncer::SyncChange handler. We push all our changes through this.
  std::unique_ptr<syncer::SyncChangeProcessor> sync_processor_;

  // A set of sync GUIDs denoting TemplateURLs that have been removed from this
  // model or the underlying KeywordWebDataService prior to
  // MergeDataAndStartSyncing.
  // This set is used to determine what entries from the server we want to
  // ignore locally and return a delete command for.
  std::set<std::string> pre_sync_deletes_;

  // This is used to log the origin of changes to the default search provider.
  // We set this value to increasingly specific values when we know what is the
  // cause/origin of a default search change.
  DefaultSearchChangeOrigin dsp_change_origin_ = DSP_CHANGE_OTHER;

  // Stores a list of callbacks to be run after TemplateURLService has loaded.
  base::OnceClosureList on_loaded_callbacks_;

  // Similar to |on_loaded_callbacks_| but used for WaitUntilReadyToSync().
  base::OnceClosure on_loaded_callback_for_sync_;

  // Helper class to manage the default search engine.
  DefaultSearchManager default_search_manager_;

  // Site search engines defined by enterprise policy. Set as nullptr if feature
  // |kSiteSearchSettingsPolicy| is not enabled.
  std::unique_ptr<EnterpriseSiteSearchManager> enterprise_site_search_manager_;

  // This tracks how many Scoper handles exist. When the number of handles drops
  // to zero, a notification is made to observers if
  // |model_mutated_notification_pending_| is true.
  int outstanding_scoper_handles_ = 0;

  // Used to track if a notification is necessary due to the model being
  // mutated. The outermost Scoper handles, can be used to defer notifications,
  // but if no model mutation occurs, the deferred notification can be skipped.
  bool model_mutated_notification_pending_ = false;

  // Session token management.
  std::string current_token_;
  base::TimeTicks token_expiration_time_;

  // Latest deletion of default search engine, contains sync GUID of the update
  // with deletion. Used to postpone the deletion in case the default search
  // engine changes later. See ProcessSyncChanges() for details.
  std::string postponed_deleted_default_engine_guid_;

#if BUILDFLAG(IS_ANDROID)
  // Manage and fetch the java object that wraps this TemplateURLService on
  // android.
  std::unique_ptr<TemplateUrlServiceAndroid> template_url_service_android_;
#endif

  base::WeakPtrFactory<TemplateURLService> weak_ptr_factory_{this};
};

#endif  // COMPONENTS_SEARCH_ENGINES_TEMPLATE_URL_SERVICE_H_
