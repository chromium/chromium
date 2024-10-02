// Copyright 2014 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/search_engines/search_engines_switches.h"
#ifdef UNSAFE_BUFFERS_BUILD
// TODO(crbug.com/40285824): Remove this and convert code to safer constructs.
#pragma allow_unsafe_buffers
#endif

#include <algorithm>
#include <iterator>
#include <memory>
#include <string>
#include <utility>

#include "base/auto_reset.h"
#include "base/base64.h"
#include "base/base64url.h"
#include "base/check_deref.h"
#include "base/check_is_test.h"
#include "base/containers/contains.h"
#include "base/containers/flat_map.h"
#include "base/debug/crash_logging.h"
#include "base/feature_list.h"
#include "base/format_macros.h"
#include "base/functional/bind.h"
#include "base/functional/callback.h"
#include "base/functional/callback_helpers.h"
#include "base/i18n/case_conversion.h"
#include "base/memory/raw_ptr.h"
#include "base/metrics/field_trial_params.h"
#include "base/metrics/histogram_functions.h"
#include "base/metrics/histogram_macros.h"
#include "base/not_fatal_until.h"
#include "base/notreached.h"
#include "base/observer_list.h"
#include "base/rand_util.h"
#include "base/ranges/algorithm.h"
#include "base/strings/string_split.h"
#include "base/strings/string_util.h"
#include "base/strings/utf_string_conversions.h"
#include "build/build_config.h"
#include "build/chromeos_buildflags.h"
#include "components/omnibox/common/omnibox_features.h"
#include "components/pref_registry/pref_registry_syncable.h"
#include "components/prefs/pref_service.h"
#include "components/search_engines/choice_made_location.h"
#include "components/search_engines/enterprise/enterprise_site_search_manager.h"
#include "components/search_engines/keyword_web_data_service.h"
#include "components/search_engines/search_engine_choice/search_engine_choice_service.h"
#include "components/search_engines/search_engine_choice/search_engine_choice_utils.h"
#include "components/search_engines/search_engine_type.h"
#include "components/search_engines/search_engines_pref_names.h"
#include "components/search_engines/search_terms_data.h"
#include "components/search_engines/template_url.h"
#include "components/search_engines/template_url_data.h"
#include "components/search_engines/template_url_prepopulate_data.h"
#include "components/search_engines/template_url_service.h"
#include "components/search_engines/template_url_service_client.h"
#include "components/search_engines/template_url_service_observer.h"
#include "components/search_engines/template_url_starter_pack_data.h"
#include "components/search_engines/util.h"
#include "components/sync/model/sync_change.h"
#include "components/sync/model/sync_change_processor.h"
#include "components/sync/protocol/entity_specifics.pb.h"
#include "components/sync/protocol/search_engine_specifics.pb.h"
#include "components/url_formatter/url_fixer.h"
#include "net/base/registry_controlled_domains/registry_controlled_domain.h"
#include "url/gurl.h"
#include "url/origin.h"

#if BUILDFLAG(IS_ANDROID)
#include "components/search_engines/android/template_url_service_android.h"
#endif

typedef SearchHostToURLsMap::TemplateURLSet TemplateURLSet;
typedef TemplateURLService::SyncDataMap SyncDataMap;

namespace {

const char kDeleteSyncedEngineHistogramName[] =
    "Search.DeleteSyncedSearchEngine";
// TODO(yoangela): Consider sharing this const with
//  "Omnibox.KeywordModeUsageByEngineType.Accepted" in omnibox_edit_model.cc.
const char kKeywordModeUsageByEngineTypeHistogramName[] =
    "Omnibox.KeywordModeUsageByEngineType";

// Values for an enumerated histogram used to track whenever an ACTION_DELETE is
// sent to the server for search engines. These are persisted. Do not re-number.
enum DeleteSyncedSearchEngineEvent {
  DELETE_ENGINE_USER_ACTION = 0,
  DELETE_ENGINE_PRE_SYNC = 1,
  DELETE_ENGINE_EMPTY_FIELD = 2,
  DELETE_ENGINE_MAX,
};

// Returns true iff the change in |change_list| at index |i| should not be sent
// up to the server based on its GUIDs presence in |sync_data| or when compared
// to changes after it in |change_list|.
// The criteria is:
//  1) It is an ACTION_UPDATE or ACTION_DELETE and the sync_guid associated
//     with it is NOT found in |sync_data|. We can only update and remove
//     entries that were originally from the Sync server.
//  2) It is an ACTION_ADD and the sync_guid associated with it is found in
//     |sync_data|. We cannot re-add entries that Sync already knew about.
//  3) There is an update after an update for the same GUID. We prune earlier
//     ones just to save bandwidth (Sync would normally coalesce them).
bool ShouldRemoveSyncChange(size_t index,
                            syncer::SyncChangeList* change_list,
                            const SyncDataMap* sync_data) {
  DCHECK(index < change_list->size());
  const syncer::SyncChange& change_i = (*change_list)[index];
  const std::string guid = change_i.sync_data().GetSpecifics()
      .search_engine().sync_guid();
  syncer::SyncChange::SyncChangeType type = change_i.change_type();
  if ((type == syncer::SyncChange::ACTION_UPDATE ||
       type == syncer::SyncChange::ACTION_DELETE) &&
       sync_data->find(guid) == sync_data->end())
    return true;
  if (type == syncer::SyncChange::ACTION_ADD &&
      sync_data->find(guid) != sync_data->end())
    return true;
  if (type == syncer::SyncChange::ACTION_UPDATE) {
    for (size_t j = index + 1; j < change_list->size(); j++) {
      const syncer::SyncChange& change_j = (*change_list)[j];
      if ((syncer::SyncChange::ACTION_UPDATE == change_j.change_type()) &&
          (change_j.sync_data().GetSpecifics().search_engine().sync_guid() ==
              guid))
        return true;
    }
  }
  return false;
}

// Remove SyncChanges that should not be sent to the server from |change_list|.
// This is done to eliminate incorrect SyncChanges added by the merge and
// conflict resolution logic when it is unsure of whether or not an entry is new
// from Sync or originally from the local model. This also removes changes that
// would be otherwise be coalesced by Sync in order to save bandwidth.
void PruneSyncChanges(const SyncDataMap* sync_data,
                      syncer::SyncChangeList* change_list) {
  for (size_t i = 0; i < change_list->size(); ) {
    if (ShouldRemoveSyncChange(i, change_list, sync_data))
      change_list->erase(change_list->begin() + i);
    else
      ++i;
  }
}

// Returns true if |turl|'s GUID is not found inside |sync_data|. This is to be
// used in MergeDataAndStartSyncing to differentiate between TemplateURLs from
// Sync and TemplateURLs that were initially local, assuming |sync_data| is the
// |initial_sync_data| parameter.
bool IsFromSync(const TemplateURL* turl, const SyncDataMap& sync_data) {
  return base::Contains(sync_data, turl->sync_guid());
}

// Log the number of instances of a keyword that exist, with zero or more
// underscores, which could occur as the result of conflict resolution.
void LogDuplicatesHistogram(
    const TemplateURLService::TemplateURLVector& template_urls) {
  std::map<std::u16string, int> duplicates;
  for (auto it = template_urls.begin(); it != template_urls.end(); ++it) {
    std::u16string keyword = (*it)->keyword();
    base::TrimString(keyword, u"_", &keyword);
    duplicates[keyword]++;
  }

  // Count the keywords with duplicates.
  int num_dupes = 0;
  for (std::map<std::u16string, int>::const_iterator it = duplicates.begin();
       it != duplicates.end(); ++it) {
    if (it->second > 1)
      num_dupes++;
  }

  UMA_HISTOGRAM_COUNTS_100("Search.SearchEngineDuplicateCounts", num_dupes);
}

bool Contains(TemplateURLService::OwnedTemplateURLVector* template_urls,
              const TemplateURL* turl) {
  return FindTemplateURL(template_urls, turl) != template_urls->end();
}

bool IsCreatedByExtension(const TemplateURL* template_url) {
  return template_url->type() ==
             TemplateURL::NORMAL_CONTROLLED_BY_EXTENSION ||
         template_url->type() == TemplateURL::OMNIBOX_API_EXTENSION;
}

// Checks if `new_values` has updated versions of `existing_turl`. Only fields
// set by the `SiteSearchSettings` policy are checked.
bool ShouldMergeEnterpriseSiteSearchEngines(const TemplateURL& existing_turl,
                                            const TemplateURL& new_values) {
  CHECK_EQ(existing_turl.keyword(), new_values.keyword());

  return existing_turl.short_name() != new_values.short_name() ||
         existing_turl.url() != new_values.url() ||
         existing_turl.featured_by_policy() != new_values.featured_by_policy();
}

// Creates a new `TemplateURL` that copies updates fields from `new_values` into
// `existing_turl`. Only fields set by the `SiteSearchSettings` policy are
// copied from `new_values`, all other fields are copied unchanged from
// `existing_turl`.
TemplateURL MergeEnterpriseSiteSearchEngines(const TemplateURL& existing_turl,
                                             const TemplateURL& new_values) {
  CHECK_EQ(existing_turl.keyword(), new_values.keyword());

  TemplateURLData merged_data(existing_turl.data());
  merged_data.SetShortName(new_values.short_name());
  merged_data.SetURL(new_values.url());
  merged_data.featured_by_policy = new_values.featured_by_policy();
  return TemplateURL(merged_data);
}

}  // namespace

// TemplateURLService::LessWithPrefix -----------------------------------------

class TemplateURLService::LessWithPrefix {
 public:
  // We want to find the set of keywords that begin with a prefix.  The STL
  // algorithms will return the set of elements that are "equal to" the
  // prefix, where "equal(x, y)" means "!(cmp(x, y) || cmp(y, x))".  When
  // cmp() is the typical std::less<>, this results in lexicographic equality;
  // we need to extend this to mark a prefix as "not less than" a keyword it
  // begins, which will cause the desired elements to be considered "equal to"
  // the prefix.  Note: this is still a strict weak ordering, as required by
  // equal_range() (though I will not prove that here).
  //
  // Unfortunately the calling convention is not "prefix and element" but
  // rather "two elements", so we pass the prefix as a fake "element" which has
  // a NULL KeywordDataElement pointer.
  bool operator()(const KeywordToTURL::value_type& elem1,
                  const KeywordToTURL::value_type& elem2) const {
    return (elem1.second == nullptr)
               ? (elem2.first.compare(0, elem1.first.length(), elem1.first) > 0)
               : (elem1.first < elem2.first);
  }
};

// TemplateURLService::Scoper -------------------------------------------------

class TemplateURLService::Scoper {
 public:
  // Keep one of these handles in scope to coalesce all the notifications into a
  // single notification. Likewise, BatchModeScoper defers web data service
  // operations into a batch operation.
  //
  // Notifications are sent when the last outstanding handle is destroyed and
  // |model_mutated_notification_pending_| is true.
  //
  // The web data service batch operation is performed when the batch mode level
  // is 0 and more than one operation is pending. This check happens when
  // BatchModeScoper is destroyed.
  explicit Scoper(TemplateURLService* service)
      : batch_mode_scoper_(
            std::make_unique<KeywordWebDataService::BatchModeScoper>(
                service->web_data_service_.get())),
        service_(service) {
    ++service_->outstanding_scoper_handles_;
  }

  Scoper(const Scoper&) = delete;
  Scoper& operator=(const Scoper&) = delete;

  // When a Scoper is destroyed, the handle count is updated. If the handle
  // count is at zero, notify the observers that the model has changed if
  // service is loaded and model was mutated.
  ~Scoper() {
    DCHECK_GT(service_->outstanding_scoper_handles_, 0);

    --service_->outstanding_scoper_handles_;
    if (service_->outstanding_scoper_handles_ == 0 &&
        service_->model_mutated_notification_pending_) {
      service_->model_mutated_notification_pending_ = false;

      if (!service_->loaded_)
        return;

      for (auto& observer : service_->model_observers_)
        observer.OnTemplateURLServiceChanged();
    }
  }

 private:
  std::unique_ptr<KeywordWebDataService::BatchModeScoper> batch_mode_scoper_;
  raw_ptr<TemplateURLService> service_;
};

// TemplateURLService::PreLoadingProviders -------------------------------------

class TemplateURLService::PreLoadingProviders {
 public:
  PreLoadingProviders() = default;
  ~PreLoadingProviders() = default;

  const TemplateURL* default_search_provider() const {
    return default_search_provider_.get();
  }

  TemplateURL* default_search_provider() {
    return default_search_provider_.get();
  }

  void set_default_search_provider(
      std::unique_ptr<TemplateURL> default_search_provider) {
    default_search_provider_ = std::move(default_search_provider);
  }

  TemplateURLService::OwnedTemplateURLVector TakeSiteSearchEngines() {
    return std::move(site_search_engines_);
  }

  void set_site_search_engines(
      TemplateURLService::OwnedTemplateURLVector&& site_search_engines) {
    site_search_engines_ = std::move(site_search_engines);
  }

  // Looks up `keyword` and returns the best `TemplateURL` for it. Returns
  // `nullptr` if the keyword was not found. The caller should not try to delete
  // the returned pointer; the data store retains ownership of it.
  const TemplateURL* GetTemplateURLForKeyword(
      const std::u16string& keyword) const {
    return GetTemplateURLForSelector(base::BindRepeating(
        [](const std::u16string& keyword, const TemplateURL& turl) {
          return turl.keyword() == keyword;
        },
        keyword));
  }

  // Returns that `TemplateURL` with the specified GUID, or nullptr if not
  // found.The caller should not try to delete the returned pointer; the data
  // store retains ownership of it.
  const TemplateURL* GetTemplateURLForGUID(const std::string& guid) const {
    return GetTemplateURLForSelector(base::BindRepeating(
        [](const std::string& guid, const TemplateURL& turl) {
          return turl.sync_guid() == guid;
        },
        guid));
  }

  // Returns the best `TemplateURL` found with a URL using the specified `host`,
  // or nullptr if no such URL can be found.
  const TemplateURL* GetTemplateURLForHost(
      const std::string& host,
      const SearchTermsData& search_terms_data) const {
    return GetTemplateURLForSelector(base::BindRepeating(
        [](const std::string& host, const SearchTermsData* search_terms_data,
           const TemplateURL& turl) {
          return turl.GenerateSearchURL(*search_terms_data).host_piece() ==
                 host;
        },
        host, &search_terms_data));
  }

 private:
  // Returns a pointer to a `TemplateURL` `t` such that `selector(t) == true`.
  // Prioritizes DSP.
  const TemplateURL* GetTemplateURLForSelector(
      base::RepeatingCallback<bool(const TemplateURL& turl)> selector) const {
    if (default_search_provider() && selector.Run(*default_search_provider())) {
      return default_search_provider();
    }

    for (auto& site_search_engine : site_search_engines_) {
      if (selector.Run(*site_search_engine)) {
        return site_search_engine.get();
      }
    }

    return nullptr;
  }

  // A temporary location for the DSE until Web Data has been loaded and it can
  // be merged into |template_urls_|.
  std::unique_ptr<TemplateURL> default_search_provider_;

  // A temporary location for site search set by policy until Web Data has been
  // loaded and it can be merged into |template_urls_|.
  TemplateURLService::OwnedTemplateURLVector site_search_engines_;
};

// TemplateURLService ---------------------------------------------------------
TemplateURLService::TemplateURLService(
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
    )
    : prefs_(prefs),
      search_engine_choice_service_(search_engine_choice_service),
      search_terms_data_(std::move(search_terms_data)),
      web_data_service_(web_data_service),
      client_(std::move(client)),
      dsp_change_callback_(dsp_change_callback),
      pre_loading_providers_(std::make_unique<PreLoadingProviders>()),
      default_search_manager_(
          &prefs,
          &search_engine_choice_service,
          base::BindRepeating(&TemplateURLService::ApplyDefaultSearchChange,
                              base::Unretained(this))
#if BUILDFLAG(IS_CHROMEOS_LACROS)
              ,
          for_lacros_main_profile
#endif  //  BUILDFLAG(IS_CHROMEOS_LACROS)
          ),
      enterprise_site_search_manager_(GetEnterpriseSiteSearchManager(&prefs)) {
  DCHECK(search_terms_data_);
  Init();
}

TemplateURLService::TemplateURLService(
    PrefService& prefs,
    search_engines::SearchEngineChoiceService& search_engine_choice_service,
    base::span<const TemplateURLService::Initializer> initializers)
    : TemplateURLService(
          prefs,
          search_engine_choice_service,
          /*search_terms_data=*/std::make_unique<SearchTermsData>(),
          /*web_data_service=*/nullptr,
          /*client=*/nullptr,
          /*dsp_change_callback=*/base::RepeatingClosure()
#if BUILDFLAG(IS_CHROMEOS_LACROS)
              ,
          /*for_lacros_main_profile=*/false
#endif  // BUILDFLAG(IS_CHROMEOS_LACROS)
      ) {
  // This constructor is not intended to be used outside of tests.
  CHECK_IS_TEST();
  ApplyInitializersForTesting(initializers);  // IN-TEST
}

TemplateURLService::~TemplateURLService() {
  // |web_data_service_| should be deleted during Shutdown().
  DCHECK(!web_data_service_);
}

// static
void TemplateURLService::RegisterProfilePrefs(
    user_prefs::PrefRegistrySyncable* registry) {
#if BUILDFLAG(IS_IOS) || BUILDFLAG(IS_ANDROID)
  uint32_t flags = PrefRegistry::NO_REGISTRATION_FLAGS;
#else
  uint32_t flags = user_prefs::PrefRegistrySyncable::SYNCABLE_PREF;
#endif
  registry->RegisterStringPref(prefs::kSyncedDefaultSearchProviderGUID,
                               std::string(),
                               flags);
  registry->RegisterStringPref(prefs::kDefaultSearchProviderGUID,
                               std::string());
  registry->RegisterBooleanPref(prefs::kDefaultSearchProviderEnabled, true);
  registry->RegisterBooleanPref(
      prefs::kDefaultSearchProviderContextMenuAccessAllowed, true);

  registry->RegisterInt64Pref(
      prefs::kDefaultSearchProviderChoiceScreenCompletionTimestamp, 0);
  registry->RegisterStringPref(
      prefs::kDefaultSearchProviderChoiceScreenCompletionVersion,
      std::string());
  registry->RegisterDictionaryPref(
      prefs::kDefaultSearchProviderPendingChoiceScreenDisplayState);

#if BUILDFLAG(IS_IOS)
  registry->RegisterIntegerPref(
      prefs::kDefaultSearchProviderChoiceScreenSkippedCount, 0);
#endif
}

#if BUILDFLAG(IS_ANDROID)
base::android::ScopedJavaLocalRef<jobject> TemplateURLService::GetJavaObject() {
  if (!template_url_service_android_) {
    template_url_service_android_ =
        std::make_unique<TemplateUrlServiceAndroid>(this);
  }
  return template_url_service_android_->GetJavaObject();
}
#endif

bool TemplateURLService::CanAddAutogeneratedKeyword(
    const std::u16string& keyword,
    const GURL& url) {
  DCHECK(!keyword.empty());  // This should only be called for non-empty
                             // keywords. If we need to support empty kewords
                             // the code needs to change slightly.
  const TemplateURL* existing_url = GetTemplateURLForKeyword(keyword);
  if (existing_url) {
    // TODO(tommycli): Currently, this code goes one step beyond
    // safe_for_autoreplace() and also forbids automatically modifying
    // prepopulated engines. That's debatable, as we already update prepopulated
    // provider favicons as the user browses. See UpdateProviderFavicons().
    return existing_url->safe_for_autoreplace() &&
           existing_url->prepopulate_id() == 0;
  }

  // We don't have a TemplateURL with keyword.  We still may not allow this
  // keyword if there's evidence we may have created this keyword before and
  // the user renamed it (because, for instance, the keyword is a common word
  // that may interfere with search queries).  An easy heuristic for this is
  // whether the user has a TemplateURL that has been manually modified (e.g.,
  // renamed) connected to the same host.
  return !url.is_valid() || url.host().empty() ||
      CanAddAutogeneratedKeywordForHost(url.host());
}

bool TemplateURLService::IsPrepopulatedOrDefaultProviderByPolicy(
    const TemplateURL* t_url) const {
  return (t_url->prepopulate_id() > 0 ||
          t_url->created_by_policy() ==
              TemplateURLData::CreatedByPolicy::kDefaultSearchProvider ||
          t_url->created_from_play_api()) &&
         t_url->SupportsReplacement(search_terms_data());
}

bool TemplateURLService::ShowInDefaultList(const TemplateURL* t_url) const {
  return t_url == default_search_provider_ ||
         IsPrepopulatedOrDefaultProviderByPolicy(t_url);
}

bool TemplateURLService::ShowInActivesList(const TemplateURL* t_url) const {
  return t_url->is_active() == TemplateURLData::ActiveStatus::kTrue;
}

bool TemplateURLService::HiddenFromLists(const TemplateURL* t_url) const {
  switch (t_url->created_by_policy()) {
    case TemplateURLData::CreatedByPolicy::kNoPolicy:
      // Hide if the preferred search engine for the keyword is created by
      // policy. The call to `GetTemplateURLForKeyword` already ensure
      // prioritization of search engines, so there is no need to replicate the
      // logic here.
      return GetTemplateURLForKeyword(t_url->keyword())->created_by_policy() !=
             TemplateURLData::CreatedByPolicy::kNoPolicy;

    case TemplateURLData::CreatedByPolicy::kDefaultSearchProvider:
      return false;

    case TemplateURLData::CreatedByPolicy::kSiteSearch: {
      // Always show featured Enterprise site search engines.
      if (t_url->featured_by_policy()) {
        return false;
      }

      // A featured site search engine with keyword "work" is represented by two
      // TemplateURLs in the service:
      // - One with `featured_by_policy = true` and keyword "@work"
      // - One with `featured_by_policy = false` and keyword "work"
      //
      // In the settings page, we want to show only one entry with both keywords
      // separated by a comma ("@work, work"). The logic below hides the one
      // that doesn't start with the "@" symbol.
      //
      // It also handles one corner case when the user explicitely created a
      // site search engine with keyword "work", which overrides the one with
      // the same keyword created by policy. In that case, we want to show both
      // the Enterprise one with keyword "@work" and the user-defined one.
      const TemplateURL* t_url_with_at =
          GetTemplateURLForKeyword(u"@" + t_url->keyword());
      return t_url_with_at &&
             t_url_with_at->created_by_policy() ==
                 TemplateURLData::CreatedByPolicy::kSiteSearch &&
             t_url_with_at->featured_by_policy();
    }
  }
}

bool TemplateURLService::FeaturedOverridesNonFeatured(
    const TemplateURL* template_url) const {
  CHECK(template_url);

  if (template_url->created_by_policy() !=
          TemplateURLData::CreatedByPolicy::kSiteSearch ||
      !template_url->featured_by_policy()) {
    return false;
  }

  const std::u16string& keyword = template_url->keyword();
  CHECK(!keyword.empty());
  CHECK_EQ(keyword[0], u'@');

  const TemplateURL* turl_without_at =
      GetTemplateURLForKeyword(std::u16string(keyword, 1));
  return turl_without_at &&
         turl_without_at->created_by_policy() ==
             TemplateURLData::CreatedByPolicy::kSiteSearch &&
         !turl_without_at->featured_by_policy();
}

void TemplateURLService::AddMatchingKeywords(const std::u16string& prefix,
                                             bool supports_replacement_only,
                                             TemplateURLVector* matches) {
  AddMatchingKeywordsHelper(keyword_to_turl_, prefix, supports_replacement_only,
                            matches);
}

TemplateURL* TemplateURLService::GetTemplateURLForKeyword(
    const std::u16string& keyword) {
  return const_cast<TemplateURL*>(
      static_cast<const TemplateURLService*>(this)->
          GetTemplateURLForKeyword(keyword));
}

const TemplateURL* TemplateURLService::GetTemplateURLForKeyword(
    const std::u16string& keyword) const {
  // Finds and returns the best match for |keyword|.
  const auto match_range = keyword_to_turl_.equal_range(keyword);
  if (match_range.first != match_range.second) {
    // Among the matches for |keyword| in the multimap, return the best one.
    return std::min_element(
               match_range.first, match_range.second,
               [](const auto& a, const auto& b) {
                 return a.second->IsBetterThanConflictingEngine(b.second);
               })
        ->second;
  }

  return loaded_ ? nullptr
                 : pre_loading_providers_->GetTemplateURLForKeyword(keyword);
}

TemplateURL* TemplateURLService::GetTemplateURLForGUID(
    const std::string& sync_guid) {
return const_cast<TemplateURL*>(
      static_cast<const TemplateURLService*>(this)->
          GetTemplateURLForGUID(sync_guid));
}

const TemplateURL* TemplateURLService::GetTemplateURLForGUID(
    const std::string& sync_guid) const {
  auto elem(guid_to_turl_.find(sync_guid));
  if (elem != guid_to_turl_.end()) {
    return elem->second;
  }

  return loaded_ ? nullptr
                 : pre_loading_providers_->GetTemplateURLForGUID(sync_guid);
}

TemplateURL* TemplateURLService::GetTemplateURLForHost(
    const std::string& host) {
  return const_cast<TemplateURL*>(
      static_cast<const TemplateURLService*>(this)->
          GetTemplateURLForHost(host));
}

const TemplateURL* TemplateURLService::GetTemplateURLForHost(
    const std::string& host) const {
  if (loaded_) {
    // `provider_map_` takes care of finding the best TemplateURL for `host`.
    return provider_map_->GetTemplateURLForHost(host);
  }

  return loaded_ ? nullptr
                 : pre_loading_providers_->GetTemplateURLForHost(
                       host, search_terms_data());
}

size_t TemplateURLService::GetTemplateURLCountForHostForLogging(
    const std::string& host) const {
  DCHECK(loaded_);
  auto* host_urls = provider_map_->GetURLsForHost(host);
  return host_urls ? host_urls->size() : 0;
}

TemplateURL* TemplateURLService::Add(
    std::unique_ptr<TemplateURL> template_url) {
  DCHECK(template_url);
  DCHECK(
      !IsCreatedByExtension(template_url.get()) ||
      (!FindTemplateURLForExtension(template_url->extension_info_->extension_id,
                                    template_url->type()) &&
       template_url->id() == kInvalidTemplateURLID));

  return Add(std::move(template_url), true);
}

TemplateURL* TemplateURLService::AddWithOverrides(
    std::unique_ptr<TemplateURL> template_url,
    const std::u16string& short_name,
    const std::u16string& keyword,
    const std::string& url) {
  DCHECK(!short_name.empty());
  DCHECK(!keyword.empty());
  DCHECK(!url.empty());
  template_url->data_.SetShortName(short_name);
  template_url->data_.SetKeyword(keyword);
  template_url->SetURL(url);
  return Add(std::move(template_url));
}

void TemplateURLService::Remove(const TemplateURL* template_url) {
  // CHECK that we aren't trying to Remove() the default search provider.
  // This has happened before, and causes permanent damage to the user Profile,
  // which can then be Synced to other installations. It's better to crash
  // immediately, and that's why this isn't a DCHECK. https://crbug.com/1164024
  {
    const TemplateURL* default_provider = GetDefaultSearchProvider();

    // TODO(tommycli): Once we are sure this never happens in practice, we can
    // remove this CrashKeyString, but we should keep the CHECK.
    static base::debug::CrashKeyString* crash_key =
        base::debug::AllocateCrashKeyString("removed_turl_keyword",
                                            base::debug::CrashKeySize::Size256);
    base::debug::ScopedCrashKeyString auto_clear(
        crash_key, base::UTF16ToUTF8(template_url->keyword()));

    CHECK_NE(template_url, default_provider);

    // Before we are loaded, we want to CHECK that we aren't accidentally
    // removing the in-table representation of the Default Search Engine.
    //
    // But users in the wild do indeed have engines with duplicated sync GUIDs.
    // For instance, Extensions Override Settings API used to have a bug that
    // would clone GUIDs. So therefore skip the check after loading.
    // https://crbug.com/1166372#c13
    if (!loaded() && default_provider &&
        default_provider->type() !=
            TemplateURL::Type::NORMAL_CONTROLLED_BY_EXTENSION &&
        template_url->type() !=
            TemplateURL::Type::NORMAL_CONTROLLED_BY_EXTENSION) {
      CHECK_NE(template_url->sync_guid(), default_provider->sync_guid());
    }
  }

  auto i = FindTemplateURL(&template_urls_, template_url);
  if (i == template_urls_.end())
    return;

  Scoper scoper(this);
  model_mutated_notification_pending_ = true;

  RemoveFromMaps(template_url);

  // Remove it from the vector containing all TemplateURLs.
  std::unique_ptr<TemplateURL> scoped_turl = std::move(*i);
  template_urls_.erase(i);

  if (template_url->type() == TemplateURL::NORMAL) {
    if (web_data_service_) {
      web_data_service_->RemoveKeyword(template_url->id());
    }
    // Inform sync of the deletion.
    ProcessTemplateURLChange(FROM_HERE, template_url,
                             syncer::SyncChange::ACTION_DELETE);

    // The default search engine can't be deleted. But the user defined DSE can
    // be hidden by an extension or policy and then deleted. Clean up the user
    // prefs then.
    if (template_url->sync_guid() ==
        GetDefaultSearchProviderGuidFromPrefs(prefs_.get())) {
      SetDefaultSearchProviderGuidToPrefs(prefs_.get(), std::string());
    }

    UMA_HISTOGRAM_ENUMERATION(kDeleteSyncedEngineHistogramName,
                              DELETE_ENGINE_USER_ACTION, DELETE_ENGINE_MAX);
  }

  if (loaded_ && client_) {
    client_->DeleteAllSearchTermsForKeyword(template_url->id());
  }
}

void TemplateURLService::RemoveExtensionControlledTURL(
    const std::string& extension_id,
    TemplateURL::Type type) {
  TemplateURL* url = FindTemplateURLForExtension(extension_id, type);
  if (!url)
    return;
  // NULL this out so that we can call Remove.
  if (default_search_provider_ == url)
    default_search_provider_ = nullptr;
  Remove(url);
}

void TemplateURLService::RemoveAutoGeneratedBetween(base::Time created_after,
                                                    base::Time created_before) {
  RemoveAutoGeneratedForUrlsBetween(base::NullCallback(), created_after,
                                    created_before);
}

void TemplateURLService::RemoveAutoGeneratedForUrlsBetween(
    const base::RepeatingCallback<bool(const GURL&)>& url_filter,
    base::Time created_after,
    base::Time created_before) {
  Scoper scoper(this);

  for (size_t i = 0; i < template_urls_.size();) {
    TemplateURL* turl = template_urls_[i].get();
    if (turl->date_created() >= created_after &&
        (created_before.is_null() || turl->date_created() < created_before) &&
        turl->safe_for_autoreplace() && turl->prepopulate_id() == 0 &&
        turl->starter_pack_id() == 0 && !MatchesDefaultSearchProvider(turl) &&
        (url_filter.is_null() ||
         url_filter.Run(turl->GenerateSearchURL(search_terms_data())))) {
      Remove(turl);
    } else {
      ++i;
    }
  }
}

void TemplateURLService::RegisterOmniboxKeyword(
    const std::string& extension_id,
    const std::string& extension_name,
    const std::string& keyword,
    const std::string& template_url_string,
    const base::Time& extension_install_time) {
  DCHECK(loaded_);

  if (FindTemplateURLForExtension(extension_id,
                                  TemplateURL::OMNIBOX_API_EXTENSION))
    return;

  TemplateURLData data;
  data.SetShortName(base::UTF8ToUTF16(extension_name));
  data.SetKeyword(base::UTF8ToUTF16(keyword));
  data.SetURL(template_url_string);
  Add(std::make_unique<TemplateURL>(data, TemplateURL::OMNIBOX_API_EXTENSION,
                                    extension_id, extension_install_time,
                                    false));
}

TemplateURLService::TemplateURLVector TemplateURLService::GetTemplateURLs() {
  TemplateURLVector result;
  for (const auto& turl : template_urls_)
    result.push_back(turl.get());
  return result;
}

std::unique_ptr<search_engines::ChoiceScreenData>
TemplateURLService::GetChoiceScreenData() {
  OwnedTemplateURLVector owned_template_urls;

  // We call `GetPrepopulatedEngines` instead of
  // `GetSearchProvidersUsingLoadedEngines` because the latter will return the
  // list of search engines that might have been modified by the user (by
  // changing the engine's keyword in settings for example).
  // Changing this will cause issues in the icon generation behavior that's
  // handled by `generate_search_engine_icons.py`.
  std::vector<std::unique_ptr<TemplateURLData>> engines =
      TemplateURLPrepopulateData::GetPrepopulatedEngines(
          &prefs_.get(), &search_engine_choice_service_.get());
  for (const auto& engine : engines) {
    owned_template_urls.push_back(std::make_unique<TemplateURL>(*engine));
  }

  return std::make_unique<search_engines::ChoiceScreenData>(
      std::move(owned_template_urls),
      search_engine_choice_service_->GetCountryId(),
      search_terms_data());
}

TemplateURLService::TemplateURLVector
TemplateURLService::GetFeaturedEnterpriseSearchEngines() const {
  TemplateURLVector result;
  for (const auto& turl : template_urls_) {
    if (turl->created_by_policy() ==
            TemplateURLData::CreatedByPolicy::kSiteSearch &&
        turl->featured_by_policy()) {
      result.push_back(turl.get());
    }
  }
  return result;
}

#if BUILDFLAG(IS_ANDROID)
TemplateURLService::OwnedTemplateURLDataVector
TemplateURLService::GetTemplateURLsForCountry(const std::string& country_code) {
  return TemplateURLPrepopulateData::GetLocalPrepopulatedEngines(country_code,
                                                                 prefs_.get());
}
#endif

void TemplateURLService::IncrementUsageCount(TemplateURL* url) {
  DCHECK(url);
  // Extension-controlled search engines are not persisted.
  if (url->type() != TemplateURL::NORMAL)
    return;
  if (!Contains(&template_urls_, url))
    return;
  ++url->data_.usage_count;

  if (web_data_service_)
    web_data_service_->UpdateKeyword(url->data());
}

void TemplateURLService::ResetTemplateURL(TemplateURL* url,
                                          const std::u16string& title,
                                          const std::u16string& keyword,
                                          const std::string& search_url) {
  DCHECK(!IsCreatedByExtension(url));
  DCHECK(!keyword.empty());
  DCHECK(!search_url.empty());
  TemplateURLData data(url->data());
  data.SetShortName(title);
  data.SetKeyword(keyword);
  if (search_url != data.url()) {
    data.SetURL(search_url);
    // The urls have changed, reset the favicon url.
    data.favicon_url = GURL();
  }
  data.safe_for_autoreplace = false;
  data.last_modified = clock_->Now();
  data.is_active = TemplateURLData::ActiveStatus::kTrue;

  Update(url, TemplateURL(data));
}

void TemplateURLService::SetIsActiveTemplateURL(TemplateURL* url,
                                                bool is_active) {
  DCHECK(url);

  TemplateURLData data(url->data());
  std::string histogram_name = kKeywordModeUsageByEngineTypeHistogramName;
  if (is_active) {
    data.is_active = TemplateURLData::ActiveStatus::kTrue;
    data.safe_for_autoreplace = false;
    histogram_name.append(".Activated");
  } else {
    data.is_active = TemplateURLData::ActiveStatus::kFalse;
    histogram_name.append(".Deactivated");
  }

  Update(url, TemplateURL(data));

  base::UmaHistogramEnumeration(
      histogram_name, url->GetBuiltinEngineType(),
      BuiltinEngineType::KEYWORD_MODE_ENGINE_TYPE_MAX);
}

#if BUILDFLAG(IS_ANDROID)
// static
TemplateURLData TemplateURLService::CreatePlayAPITemplateURLData(
    const std::u16string& keyword,
    const std::u16string& name,
    const std::string& search_url,
    const std::string& suggest_url,
    const std::string& favicon_url,
    const std::string& new_tab_url,
    const std::string& image_url,
    const std::string& image_url_post_params,
    const std::string& image_translate_url,
    const std::string& image_translate_source_language_param_key,
    const std::string& image_translate_target_language_param_key) {
  TemplateURLData data;
  data.SetShortName(name);
  data.SetKeyword(keyword);
  data.SetURL(search_url);
  data.suggestions_url = suggest_url;
  data.favicon_url = GURL(favicon_url);
  data.new_tab_url = new_tab_url;
  data.image_url = image_url;
  data.image_url_post_params = image_url_post_params;
  data.image_translate_url = image_translate_url;
  data.image_translate_source_language_param_key =
      image_translate_source_language_param_key;
  data.image_translate_target_language_param_key =
      image_translate_target_language_param_key;
  data.created_from_play_api = true;
  // Play API engines are created by explicit user gesture, and should not be
  // auto-replaceable by an auto-generated engine as the user browses.
  data.safe_for_autoreplace = false;
  data.is_active = TemplateURLData::ActiveStatus::kTrue;
  return data;
}

bool TemplateURLService::ResetPlayAPISearchEngine(
    const TemplateURLData& new_play_api_turl_data) {
  CHECK(loaded());
  CHECK(new_play_api_turl_data.created_from_play_api);

  auto new_play_api_turl =
      std::make_unique<TemplateURL>(new_play_api_turl_data);

  SCOPED_CRASH_KEY_NUMBER("ResetPlayAPISearchEngine", "OldDspSource",
                          default_search_provider_source_);
  SCOPED_CRASH_KEY_STRING64(
      "ResetPlayAPISearchEngine", "OldDspKw",
      default_search_provider_
          ? base::UTF16ToUTF8(default_search_provider_->keyword())
          : "<null>");
  std::u16string old_play_keyword;

  Scoper scoper{this};

  // Part 1. Add the new play engine
  // Can fail if there is an old play engine or if there is a better engine
  // matching the new keyword.

  // 1.A) The Play API search engine is not guaranteed to be the best engine for
  // `keyword`, if there are user-defined, extension, or policy engines that can
  // take precedence. In practice on Android, this rarely happens, as only
  // policy engines are possible.
  const auto match_range =
      keyword_to_turl_.equal_range(new_play_api_turl->keyword());
  for (auto it = match_range.first; it != match_range.second; ++it) {
    TemplateURL* same_keyword_engine = it->second;
    if (same_keyword_engine->created_from_play_api()) {
      // We will look into replacing this one below, don't consider it a blocker
      // yet.
      continue;
    }

    if (same_keyword_engine->IsBetterThanConflictingEngine(
            new_play_api_turl.get())) {
      // We won't be able to add the new search engine at all.
      return false;
    }
  }

  // 1.B) We can only have 1 Play API engine at a time. we have to remove the
  // old one, if it exits. If it's the current default, we'll have to remove it
  // first.
  auto found = base::ranges::find_if(template_urls_,
                                     &TemplateURL::created_from_play_api);
  if (found != template_urls_.cend()) {
    // There is already an old Play API engine. To proceed we'll need to remove
    // it.
    TemplateURL* old_play_api_engine = found->get();
    old_play_keyword = old_play_api_engine->keyword();
    if (old_play_api_engine == default_search_provider_) {
      // The DSE can't be removed from the loaded engines. We need to clear the
      // DSE first. The old Play API engine should be replaceable, since having
      // it as DSE means that we don't have a policy-enforced engine, and we
      // know that the incoming engine otherwise meets the criteria to be to be
      // set as DSE.
      CHECK(CanMakeDefault(new_play_api_turl.get()), base::NotFatalUntil::M129);

      // Clearing the member is OK here, we just have to make sure it is
      // re-populated by the time `scoper` is cleared.
      default_search_provider_ = nullptr;
    }

    Remove(old_play_api_engine);
  }

  SCOPED_CRASH_KEY_STRING64("ResetPlayAPISearchEngine", "OldPlayKw",
                            base::UTF16ToUTF8(old_play_keyword));

  TemplateURL* new_play_api_turl_ptr = Add(std::move(new_play_api_turl));

  // Adding the engine should be successful, we already checked for blockers
  // above.
  CHECK(new_play_api_turl_ptr, base::NotFatalUntil::M129);

  // Part 2: Set as DSE.
  // It is still possible that policies control the DSE, so ensure we don't
  // break that.
  if (CanMakeDefault(new_play_api_turl_ptr)) {
    SetUserSelectedDefaultSearchProvider(
        new_play_api_turl_ptr,
        search_engines::ChoiceMadeLocation::kChoiceScreen);
  }

  CHECK(default_search_provider_, base::NotFatalUntil::M132);
  return true;
}
#endif  // BUILDFLAG(IS_ANDROID)

void TemplateURLService::UpdateProviderFavicons(
    const GURL& potential_search_url,
    const GURL& favicon_url) {
  DCHECK(loaded_);
  DCHECK(potential_search_url.is_valid());

  const TemplateURLSet* urls_for_host =
      provider_map_->GetURLsForHost(potential_search_url.host());
  if (!urls_for_host)
    return;

  // Make a copy of the container of the matching TemplateURLs, as the original
  // container is invalidated as we update the contained TemplateURLs.
  TemplateURLSet urls_for_host_copy(*urls_for_host);

  Scoper scoper(this);
  for (TemplateURL* turl : urls_for_host_copy) {
    if (!IsCreatedByExtension(turl) &&
        turl->IsSearchURL(potential_search_url, search_terms_data()) &&
        turl->favicon_url() != favicon_url) {
      TemplateURLData data(turl->data());
      data.favicon_url = favicon_url;
      Update(turl, TemplateURL(data));
    }
  }
}

bool TemplateURLService::CanMakeDefault(const TemplateURL* url) const {
  return (default_search_provider_source_ == DefaultSearchManager::FROM_USER ||
          default_search_provider_source_ ==
              DefaultSearchManager::FROM_POLICY_RECOMMENDED ||
          default_search_provider_source_ ==
              DefaultSearchManager::FROM_FALLBACK) &&
         (url != GetDefaultSearchProvider()) &&
         url->url_ref().SupportsReplacement(search_terms_data()) &&
         (url->type() == TemplateURL::NORMAL) && (url->starter_pack_id() == 0);
}

void TemplateURLService::SetUserSelectedDefaultSearchProvider(
    TemplateURL* url,
    search_engines::ChoiceMadeLocation choice_made_location) {
  // Omnibox keywords cannot be made default. Extension-controlled search
  // engines can be made default only by the extension itself because they
  // aren't persisted.
  DCHECK(!url || !IsCreatedByExtension(url));
  if (url) {
    url->data_.is_active = TemplateURLData::ActiveStatus::kTrue;
  }

  bool selection_added = false;

  if (load_failed_) {
    // Skip the DefaultSearchManager, which will persist to user preferences.
    if ((default_search_provider_source_ == DefaultSearchManager::FROM_USER) ||
        (default_search_provider_source_ ==
         DefaultSearchManager::FROM_FALLBACK)) {
      ApplyDefaultSearchChange(url ? &url->data() : nullptr,
                               DefaultSearchManager::FROM_USER);
      selection_added = true;
    } else {
      // When we are setting the search engine choice from choice screens,
      // the DSP source is expected to allow the search engine to be changed by
      // the user. But theoretically there is a possibility that a policy
      // kicked in after a choice screen was shown, that could be a way to
      // enter this state
      // TODO(crbug.com/328041262): Investigate mitigation options.
      CHECK_NE(choice_made_location, search_engines::ChoiceMadeLocation::kOther,
               base::NotFatalUntil::M127);
    }
  } else {
    // We rely on the DefaultSearchManager to call ApplyDefaultSearchChange if,
    // in fact, the effective DSE changes.
    if (url) {
      default_search_manager_.SetUserSelectedDefaultSearchEngine(url->data());
      selection_added = true;
    } else {
      default_search_manager_.ClearUserSelectedDefaultSearchEngine();
    }
  }

  if (selection_added &&
      // The choice record below should only be done when called from a path
      // associated with a fully featured search engine choice screen.
      choice_made_location != search_engines::ChoiceMadeLocation::kOther) {
    search_engine_choice_service_->RecordChoiceMade(choice_made_location, this);
  }

#if BUILDFLAG(IS_ANDROID)
    // Commit the pref immediately so it isn't lost if the app is killed.
    // TODO(b/316887441): Investigate removing this.
    prefs_->CommitPendingWrite();
#endif
}

const TemplateURL* TemplateURLService::GetDefaultSearchProvider() const {
  return loaded_ ? default_search_provider_.get()
                 : pre_loading_providers_->default_search_provider();
}

url::Origin TemplateURLService::GetDefaultSearchProviderOrigin() const {
  const TemplateURL* template_url = GetDefaultSearchProvider();
  if (template_url) {
    GURL search_url = template_url->GenerateSearchURL(search_terms_data());
    return url::Origin::Create(search_url);
  }
  return url::Origin();
}

const TemplateURL*
TemplateURLService::GetDefaultSearchProviderIgnoringExtensions() const {
  std::unique_ptr<TemplateURLData> next_search =
      default_search_manager_.GetDefaultSearchEngineIgnoringExtensions();
  if (!next_search)
    return nullptr;

  // Find the TemplateURL matching the data retrieved.
  auto iter = base::ranges::find_if(
      template_urls_, [this, &next_search](const auto& turl_to_check) {
        return TemplateURL::MatchesData(turl_to_check.get(), next_search.get(),
                                        search_terms_data());
      });
  return iter == template_urls_.end() ? nullptr : iter->get();
}

bool TemplateURLService::IsSearchResultsPageFromDefaultSearchProvider(
    const GURL& url) const {
  const TemplateURL* default_provider = GetDefaultSearchProvider();
  return default_provider &&
      default_provider->IsSearchURL(url, search_terms_data());
}

GURL TemplateURLService::GenerateSearchURLForDefaultSearchProvider(
    const std::u16string& search_terms) const {
  const TemplateURL* default_provider = GetDefaultSearchProvider();
  return default_provider ? default_provider->GenerateSearchURL(
                                search_terms_data(), search_terms)
                          : GURL();
}

std::optional<TemplateURLService::SearchMetadata>
TemplateURLService::ExtractSearchMetadata(const GURL& url) const {
  const TemplateURL* template_url = GetTemplateURLForHost(url.host());
  if (!template_url) {
    return std::nullopt;
  }

  GURL normalized_url;
  std::u16string normalized_search_terms;
  bool is_valid_search_url =
      template_url && template_url->KeepSearchTermsInURL(
                          url, search_terms_data(),
                          /*keep_search_intent_params=*/true,

                          /*normalize_search_terms=*/true, &normalized_url,
                          &normalized_search_terms);
  if (!is_valid_search_url) {
    return std::nullopt;
  }

  return SearchMetadata{template_url, normalized_url, normalized_search_terms};
}

bool TemplateURLService::IsSideSearchSupportedForDefaultSearchProvider() const {
  const TemplateURL* default_provider = GetDefaultSearchProvider();
  return default_provider && default_provider->IsSideSearchSupported();
}

bool TemplateURLService::IsSideImageSearchSupportedForDefaultSearchProvider()
    const {
  const TemplateURL* default_provider = GetDefaultSearchProvider();
  return default_provider && default_provider->IsSideImageSearchSupported();
}

GURL TemplateURLService::GenerateSideSearchURLForDefaultSearchProvider(
    const GURL& search_url,
    const std::string& version) const {
  DCHECK(IsSideSearchSupportedForDefaultSearchProvider());
  return GetDefaultSearchProvider()->GenerateSideSearchURL(search_url, version,
                                                           search_terms_data());
}

GURL TemplateURLService::RemoveSideSearchParamFromURL(
    const GURL& search_url) const {
  if (!IsSideSearchSupportedForDefaultSearchProvider())
    return search_url;
  return GetDefaultSearchProvider()->RemoveSideSearchParamFromURL(search_url);
}

GURL TemplateURLService::GenerateSideImageSearchURLForDefaultSearchProvider(
    const GURL& search_url,
    const std::string& version) const {
  DCHECK(IsSideImageSearchSupportedForDefaultSearchProvider());
  return GetDefaultSearchProvider()->GenerateSideImageSearchURL(search_url,
                                                                version);
}

GURL TemplateURLService::RemoveSideImageSearchParamFromURL(
    const GURL& search_url) const {
  if (!IsSideImageSearchSupportedForDefaultSearchProvider())
    return search_url;
  return GetDefaultSearchProvider()->RemoveSideImageSearchParamFromURL(
      search_url);
}

bool TemplateURLService::IsExtensionControlledDefaultSearch() const {
  return default_search_provider_source_ ==
      DefaultSearchManager::FROM_EXTENSION;
}

void TemplateURLService::RepairPrepopulatedSearchEngines() {
  // Can't clean DB if it hasn't been loaded.
  DCHECK(loaded());

  Scoper scoper(this);

  if ((default_search_provider_source_ == DefaultSearchManager::FROM_USER) ||
      (default_search_provider_source_ ==
          DefaultSearchManager::FROM_FALLBACK)) {
    // Clear |default_search_provider_| in case we want to remove the engine it
    // points to. This will get reset at the end of the function anyway.
    default_search_provider_ = nullptr;
  }

  std::vector<std::unique_ptr<TemplateURLData>> prepopulated_urls =
      TemplateURLPrepopulateData::GetPrepopulatedEngines(
          &prefs_.get(), &search_engine_choice_service_.get());
  DCHECK(!prepopulated_urls.empty());
  ActionsFromCurrentData actions(CreateActionsFromCurrentPrepopulateData(
      &prepopulated_urls, template_urls_, default_search_provider_));

  // Remove items.
  for (auto i = actions.removed_engines.begin();
       i < actions.removed_engines.end(); ++i)
    Remove(*i);

  // Edit items.
  for (auto i(actions.edited_engines.begin()); i < actions.edited_engines.end();
       ++i) {
    TemplateURL new_values(i->second);
    Update(i->first, new_values);
  }

  // Add items.
  for (std::vector<TemplateURLData>::const_iterator i =
           actions.added_engines.begin();
       i < actions.added_engines.end();
       ++i) {
    Add(std::make_unique<TemplateURL>(*i));
  }

  base::AutoReset<DefaultSearchChangeOrigin> change_origin(
      &dsp_change_origin_, DSP_CHANGE_PROFILE_RESET);

  default_search_manager_.ClearUserSelectedDefaultSearchEngine();

  if (default_search_provider_) {
    // Set fallback engine as user selected, because repair is considered a user
    // action and we are expected to sync new fallback engine to other devices.
    const TemplateURLData* fallback_engine_data =
        default_search_manager_.GetFallbackSearchEngine();
    if (fallback_engine_data) {
      TemplateURL* fallback_engine =
          FindPrepopulatedTemplateURL(fallback_engine_data->prepopulate_id);
      // The fallback engine is created from built-in/override data that should
      // always have a prepopulate ID, so this engine should always exist after
      // a repair.
      DCHECK(fallback_engine);
      // Write the fallback engine's GUID to prefs, which will cause
      // OnDefaultSearchProviderGUIDChanged() to set it as the new
      // user-selected engine.
      SetDefaultSearchProviderGuidToPrefs(prefs_.get(),
                                          fallback_engine->sync_guid());
    }
  } else {
    // If the default search provider came from a user pref we would have been
    // notified of the new (fallback-provided) value in
    // ClearUserSelectedDefaultSearchEngine() above. Since we are here, the
    // value was presumably originally a fallback value (which may have been
    // repaired).
    DefaultSearchManager::Source source;
    const TemplateURLData* new_dse =
        default_search_manager_.GetDefaultSearchEngine(&source);
    ApplyDefaultSearchChange(new_dse, source);
  }
}

void TemplateURLService::RepairStarterPackEngines() {
  DCHECK(loaded());

  Scoper scoper(this);

  std::vector<std::unique_ptr<TemplateURLData>> starter_pack_engines =
      TemplateURLStarterPackData::GetStarterPackEngines();
  DCHECK(!starter_pack_engines.empty());
  ActionsFromCurrentData actions(CreateActionsFromCurrentStarterPackData(
      &starter_pack_engines, template_urls_));

  // Remove items.
  for (auto i = actions.removed_engines.begin();
       i < actions.removed_engines.end(); ++i) {
    Remove(*i);
  }

  // Edit items.
  for (auto i(actions.edited_engines.begin()); i < actions.edited_engines.end();
       ++i) {
    Update(i->first, TemplateURL(i->second));
  }

  // Add items.
  for (std::vector<TemplateURLData>::const_iterator i =
           actions.added_engines.begin();
       i < actions.added_engines.end(); ++i) {
    Add(std::make_unique<TemplateURL>(*i));
  }
}

void TemplateURLService::AddObserver(TemplateURLServiceObserver* observer) {
  model_observers_.AddObserver(observer);
}

void TemplateURLService::RemoveObserver(TemplateURLServiceObserver* observer) {
  model_observers_.RemoveObserver(observer);
}

void TemplateURLService::Load() {
  if (loaded_ || load_handle_ || disable_load_)
    return;

  if (web_data_service_)
    load_handle_ = web_data_service_->GetKeywords(this);
  else
    ChangeToLoadedState();
}

base::CallbackListSubscription TemplateURLService::RegisterOnLoadedCallback(
    base::OnceClosure callback) {
  return loaded_ ? base::CallbackListSubscription()
                 : on_loaded_callbacks_.Add(std::move(callback));
}

void TemplateURLService::EmitTemplateURLActiveOnStartupHistogram(
    OwnedTemplateURLVector* template_urls) {
  DCHECK(template_urls);

  for (auto& turl : *template_urls) {
    std::string histogram_name = kKeywordModeUsageByEngineTypeHistogramName;
    histogram_name.append(
        (turl->is_active() == TemplateURLData::ActiveStatus::kTrue)
            ? ".ActiveOnStartup"
            : ".InactiveOnStartup");
    base::UmaHistogramEnumeration(
        histogram_name, turl->GetBuiltinEngineType(),
        BuiltinEngineType::KEYWORD_MODE_ENGINE_TYPE_MAX);
  }
}

void TemplateURLService::OnWebDataServiceRequestDone(
    KeywordWebDataService::Handle h,
    std::unique_ptr<WDTypedResult> result) {
  // Reset the load_handle so that we don't try and cancel the load in
  // the destructor.
  load_handle_ = 0;

  if (!result) {
    // Results are null if the database went away or (most likely) wasn't
    // loaded.
    load_failed_ = true;
    web_data_service_ = nullptr;
    ChangeToLoadedState();
    return;
  }

  std::unique_ptr<OwnedTemplateURLVector> template_urls =
      std::make_unique<OwnedTemplateURLVector>();
  WDKeywordsResult::Metadata updated_keywords_metadata;
  {
    GetSearchProvidersUsingKeywordResult(
        *result, web_data_service_.get(), &prefs_.get(),
        &search_engine_choice_service_.get(), template_urls.get(),
        (default_search_provider_source_ == DefaultSearchManager::FROM_USER)
            ? pre_loading_providers_->default_search_provider()
            : nullptr,
        search_terms_data(), updated_keywords_metadata, &pre_sync_deletes_);
  }

  Scoper scoper(this);

  {
    PatchMissingSyncGUIDs(template_urls.get());
    MaybeSetIsActiveSearchEngines(template_urls.get());
    EmitTemplateURLActiveOnStartupHistogram(template_urls.get());
    SetTemplateURLs(std::move(template_urls));

    // This initializes provider_map_ which should be done before
    // calling UpdateKeywordSearchTermsForURL.
    ChangeToLoadedState();

    // Index any visits that occurred before we finished loading.
    for (size_t i = 0; i < visits_to_add_.size(); ++i)
      UpdateKeywordSearchTermsForURL(visits_to_add_[i]);
    visits_to_add_.clear();

    if (updated_keywords_metadata.HasBuiltinKeywordData()) {
      web_data_service_->SetBuiltinKeywordDataVersion(
          updated_keywords_metadata.builtin_keyword_data_version);
      web_data_service_->SetBuiltinKeywordCountry(
          updated_keywords_metadata.builtin_keyword_country);

      // Added 20/08/2024.
      // This is used for database cleanup.
      // TODO(b/361013517): Remove the call and cleanup the code in a year.
      web_data_service_->ClearBuiltinKeywordMilestone();
    }

    if (updated_keywords_metadata.HasStarterPackData()) {
      web_data_service_->SetStarterPackKeywordVersion(
          updated_keywords_metadata.starter_pack_version);
    }
  }

  if (default_search_provider_) {
    SearchEngineType engine_type =
        default_search_provider_->GetEngineType(search_terms_data());
    base::UmaHistogramEnumeration("Search.DefaultSearchProviderType2",
                                  engine_type, SEARCH_ENGINE_MAX);
    if (default_search_provider_->created_by_policy() ==
        TemplateURLData::CreatedByPolicy::kDefaultSearchProvider) {
      base::UmaHistogramEnumeration(
          "Search.DefaultSearchProviderType2.SetByEnterprisePolicy",
          engine_type, SEARCH_ENGINE_MAX);
    }
  }
}

std::u16string TemplateURLService::GetKeywordShortName(
    const std::u16string& keyword,
    bool* is_omnibox_api_extension_keyword,
    bool* is_gemini_keyword) const {
  const TemplateURL* template_url = GetTemplateURLForKeyword(keyword);

  // TODO(sky): Once LocationBarView adds a listener to the TemplateURLService
  // to track changes to the model, this should become a DCHECK.
  if (template_url) {
    *is_gemini_keyword =
        template_url->starter_pack_id() == TemplateURLStarterPackData::kGemini;
    *is_omnibox_api_extension_keyword =
        template_url->type() == TemplateURL::OMNIBOX_API_EXTENSION;
    return template_url->AdjustedShortNameForLocaleDirection();
  }
  *is_omnibox_api_extension_keyword = false;
  return std::u16string();
}

void TemplateURLService::OnHistoryURLVisited(const URLVisitedDetails& details) {
  if (!loaded_)
    visits_to_add_.push_back(details);
  else
    UpdateKeywordSearchTermsForURL(details);
}

void TemplateURLService::Shutdown() {
  for (auto& observer : model_observers_)
    observer.OnTemplateURLServiceShuttingDown();

  if (client_)
    client_->Shutdown();
  // This check has to be done at Shutdown() instead of in the dtor to ensure
  // that no clients of KeywordWebDataService are holding ptrs to it after the
  // first phase of the KeyedService Shutdown() process.
  if (load_handle_) {
    DCHECK(web_data_service_);
    web_data_service_->CancelRequest(load_handle_);
  }
  web_data_service_ = nullptr;
}

void TemplateURLService::WaitUntilReadyToSync(base::OnceClosure done) {
  DCHECK(!on_loaded_callback_for_sync_);

  // We force a load here to allow remote updates to be processed, without
  // waiting for the lazy load.
  Load();

  if (loaded_)
    std::move(done).Run();
  else
    on_loaded_callback_for_sync_ = std::move(done);
}

syncer::SyncDataList TemplateURLService::GetAllSyncData(
    syncer::DataType type) const {
  DCHECK_EQ(syncer::SEARCH_ENGINES, type);

  syncer::SyncDataList current_data;
  for (const auto& turl : template_urls_) {
    // We don't sync keywords managed by policy.
    if (turl->created_by_policy() !=
        TemplateURLData::CreatedByPolicy::kNoPolicy) {
      continue;
    }
    // We don't sync extension-controlled search engines.
    if (turl->type() != TemplateURL::NORMAL)
      continue;
    current_data.push_back(CreateSyncDataFromTemplateURL(*turl));
  }

  return current_data;
}

std::optional<syncer::ModelError> TemplateURLService::ProcessSyncChanges(
    const base::Location& from_here,
    const syncer::SyncChangeList& change_list) {
  if (!models_associated_) {
    return syncer::ModelError(FROM_HERE, "Models not yet associated.");
  }
  DCHECK(loaded_);

  base::AutoReset<bool> processing_changes(&processing_syncer_changes_, true);

  Scoper scoper(this);

  // We've started syncing, so set our origin member to the base Sync value.
  // As we move through Sync Code, we may set this to increasingly specific
  // origins so we can tell what exactly caused a DSP change.
  base::AutoReset<DefaultSearchChangeOrigin> change_origin_unintentional(
      &dsp_change_origin_, DSP_CHANGE_SYNC_UNINTENTIONAL);

  syncer::SyncChangeList new_changes;
  std::optional<syncer::ModelError> error;
  for (auto iter = change_list.begin(); iter != change_list.end(); ++iter) {
    DCHECK_EQ(syncer::SEARCH_ENGINES, iter->sync_data().GetDataType());

    TemplateURL* existing_turl = GetTemplateURLForGUID(
        iter->sync_data().GetSpecifics().search_engine().sync_guid());
    std::unique_ptr<TemplateURL> turl =
        CreateTemplateURLFromTemplateURLAndSyncData(
            client_.get(), &prefs_.get(), &search_engine_choice_service_.get(),
            search_terms_data(), existing_turl, iter->sync_data(),
            &new_changes);
    if (!turl)
      continue;

    const std::string error_msg =
        "ProcessSyncChanges failed on ChangeType " +
        syncer::SyncChange::ChangeTypeToString(iter->change_type());
    if (iter->change_type() == syncer::SyncChange::ACTION_DELETE) {
      if (!existing_turl) {
        // Can't DELETE a non-existent engine.
        error = syncer::ModelError(FROM_HERE, error_msg);
        continue;
      }

      // We can get an ACTION_DELETE for the default search provider if the user
      // has changed the default search provider on a different machine, and we
      // get the search engine update before the preference update.
      //
      // In this case, postpone the delete, because we never want to reset the
      // default search provider as a result of ACTION_DELETE. If the preference
      // update arrives later, the engine will be removed. We still may be stuck
      // with an extra search engine entry in the edge case (due to storing the
      // deletion in memory), but it's better than most alternatives.
      //
      // In the past, we tried re-creating the deleted TemplateURL, but it was
      // likely a source of duplicate search engine entries. crbug.com/1022775
      if (existing_turl != GetDefaultSearchProvider()) {
        Remove(existing_turl);
      } else {
        postponed_deleted_default_engine_guid_ = existing_turl->sync_guid();
      }
      continue;
    }

    // Because TemplateURLService sometimes ignores remote Sync changes which
    // we cannot cleanly apply, we need to handle ADD and UPDATE together.
    // Ignore what the other Sync layers THINK the change type is. Instead:
    // If we have an existing engine, treat as an update.
    DCHECK(iter->change_type() == syncer::SyncChange::ACTION_ADD ||
           iter->change_type() == syncer::SyncChange::ACTION_UPDATE);

    if (!existing_turl) {
      base::AutoReset<DefaultSearchChangeOrigin> change_origin_add(
          &dsp_change_origin_, DSP_CHANGE_SYNC_ADD);
      // Force the local ID to kInvalidTemplateURLID so we can add it.
      TemplateURLData data(turl->data());
      data.id = kInvalidTemplateURLID;

      TemplateURL* added = Add(std::make_unique<TemplateURL>(data));
      if (added) {
        MaybeUpdateDSEViaPrefs(added);
      }
    } else {
      // Since we've already found |existing_turl| by GUID, this Update() should
      // always return true, but we still don't want to crash if it fails.
      DCHECK(existing_turl);
      bool update_success = Update(existing_turl, *turl);
      DCHECK(update_success);

      MaybeUpdateDSEViaPrefs(existing_turl);
    }
  }

  // If something went wrong, we want to prematurely exit to avoid pushing
  // inconsistent data to Sync. We return the last error we received.
  if (error) {
    return error;
  }

  return sync_processor_->ProcessSyncChanges(from_here, new_changes);
}

base::WeakPtr<syncer::SyncableService> TemplateURLService::AsWeakPtr() {
  return weak_ptr_factory_.GetWeakPtr();
}

std::optional<syncer::ModelError> TemplateURLService::MergeDataAndStartSyncing(
    syncer::DataType type,
    const syncer::SyncDataList& initial_sync_data,
    std::unique_ptr<syncer::SyncChangeProcessor> sync_processor) {
  DCHECK(loaded_);
  DCHECK_EQ(type, syncer::SEARCH_ENGINES);
  DCHECK(!sync_processor_);
  DCHECK(sync_processor);

  // Disable sync if we failed to load.
  if (load_failed_) {
    return syncer::ModelError(FROM_HERE, "Local database load failed.");
  }

  sync_processor_ = std::move(sync_processor);

  // We do a lot of calls to Add/Remove/ResetTemplateURL here, so ensure we
  // don't step on our own toes.
  base::AutoReset<bool> processing_changes(&processing_syncer_changes_, true);

  Scoper scoper(this);

  // We've started syncing, so set our origin member to the base Sync value.
  // As we move through Sync Code, we may set this to increasingly specific
  // origins so we can tell what exactly caused a DSP change.
  base::AutoReset<DefaultSearchChangeOrigin> change_origin(&dsp_change_origin_,
      DSP_CHANGE_SYNC_UNINTENTIONAL);

  syncer::SyncChangeList new_changes;

  // Build maps of our sync GUIDs to syncer::SyncData.
  SyncDataMap local_data_map = CreateGUIDToSyncDataMap(
      GetAllSyncData(syncer::SEARCH_ENGINES));
  SyncDataMap sync_data_map = CreateGUIDToSyncDataMap(initial_sync_data);

  for (SyncDataMap::const_iterator iter = sync_data_map.begin();
      iter != sync_data_map.end(); ++iter) {
    TemplateURL* local_turl = GetTemplateURLForGUID(iter->first);
    std::unique_ptr<TemplateURL> sync_turl(
        CreateTemplateURLFromTemplateURLAndSyncData(
            client_.get(), &prefs_.get(), &search_engine_choice_service_.get(),
            search_terms_data(), local_turl, iter->second, &new_changes));
    if (!sync_turl)
      continue;

    if (pre_sync_deletes_.find(sync_turl->sync_guid()) !=
        pre_sync_deletes_.end()) {
      // This entry was deleted before the initial sync began (possibly through
      // preprocessing in TemplateURLService's loading code). Ignore it and send
      // an ACTION_DELETE up to the server.
      new_changes.push_back(
          syncer::SyncChange(FROM_HERE,
                             syncer::SyncChange::ACTION_DELETE,
                             iter->second));
      UMA_HISTOGRAM_ENUMERATION(kDeleteSyncedEngineHistogramName,
          DELETE_ENGINE_PRE_SYNC, DELETE_ENGINE_MAX);
      continue;
    }

    if (local_turl) {
      DCHECK(IsFromSync(local_turl, sync_data_map));
      // This local search engine is already synced. If the timestamp differs
      // from Sync, we need to update locally or to the cloud. Note that if the
      // timestamps are equal, we touch neither.
      if (sync_turl->last_modified() > local_turl->last_modified()) {
        // We've received an update from Sync. We should replace all synced
        // fields in the local TemplateURL. Note that this includes the
        // TemplateURLID and the TemplateURL may have to be reparsed. This
        // also makes the local data's last_modified timestamp equal to Sync's,
        // avoiding an Update on the next MergeData call.
        Update(local_turl, *sync_turl);
      } else if (sync_turl->last_modified() < local_turl->last_modified()) {
        // Otherwise, we know we have newer data, so update Sync with our
        // data fields.
        new_changes.push_back(
            syncer::SyncChange(FROM_HERE,
                               syncer::SyncChange::ACTION_UPDATE,
                               local_data_map[local_turl->sync_guid()]));
      }
      local_data_map.erase(iter->first);
    } else {
      // The search engine from the cloud has not been synced locally. Merge it
      // into our local model. This will handle any conflicts with local (and
      // already-synced) TemplateURLs. It will prefer to keep entries from Sync
      // over not-yet-synced TemplateURLs.
      MergeInSyncTemplateURL(sync_turl.get(), sync_data_map, &new_changes,
                             &local_data_map);
    }
  }


  // The remaining SyncData in local_data_map should be everything that needs to
  // be pushed as ADDs to sync.
  for (SyncDataMap::const_iterator iter = local_data_map.begin();
      iter != local_data_map.end(); ++iter) {
    new_changes.push_back(
        syncer::SyncChange(FROM_HERE,
                           syncer::SyncChange::ACTION_ADD,
                           iter->second));
  }

  // Do some post-processing on the change list to ensure that we are sending
  // valid changes to sync_processor_.
  PruneSyncChanges(&sync_data_map, &new_changes);

  LogDuplicatesHistogram(GetTemplateURLs());
  std::optional<syncer::ModelError> error =
      sync_processor_->ProcessSyncChanges(FROM_HERE, new_changes);
  if (!error.has_value()) {
    // The ACTION_DELETEs from this set are processed. Empty it so we don't try
    // to reuse them on the next call to MergeDataAndStartSyncing.
    pre_sync_deletes_.clear();

    models_associated_ = true;
  }

  return error;
}

void TemplateURLService::StopSyncing(syncer::DataType type) {
  DCHECK_EQ(type, syncer::SEARCH_ENGINES);
  models_associated_ = false;
  sync_processor_.reset();
}

void TemplateURLService::ProcessTemplateURLChange(
    const base::Location& from_here,
    const TemplateURL* turl,
    syncer::SyncChange::SyncChangeType type) {
  DCHECK(turl);

  if (!models_associated_)
    return;  // Not syncing.

  if (processing_syncer_changes_)
    return;  // These are changes originating from us. Ignore.

  // Avoid syncing keywords managed by policy.
  if (turl->created_by_policy() !=
      TemplateURLData::CreatedByPolicy::kNoPolicy) {
    return;
  }

  // Avoid syncing extension-controlled search engines.
  if (turl->type() == TemplateURL::NORMAL_CONTROLLED_BY_EXTENSION)
    return;

  syncer::SyncData sync_data = CreateSyncDataFromTemplateURL(*turl);
  syncer::SyncChangeList changes = {
      syncer::SyncChange(from_here, type, sync_data)};
  sync_processor_->ProcessSyncChanges(FROM_HERE, changes);
}

bool TemplateURLService::IsEeaChoiceCountry() {
  return search_engines::IsEeaChoiceCountry(
      search_engine_choice_service_->GetCountryId());
}

std::string TemplateURLService::GetSessionToken() {
  base::TimeTicks current_time(base::TimeTicks::Now());
  // Renew token if it expired.
  if (current_time > token_expiration_time_) {
    std::array<uint8_t, 12> raw_data;
    base::RandBytes(raw_data);
    base::Base64UrlEncode(raw_data,
                          base::Base64UrlEncodePolicy::INCLUDE_PADDING,
                          &current_token_);
  }

  // Extend expiration time another 60 seconds.
  token_expiration_time_ = current_time + base::Seconds(60);
  return current_token_;
}

void TemplateURLService::ClearSessionToken() {
  token_expiration_time_ = base::TimeTicks();
}

// static
TemplateURLData::ActiveStatus TemplateURLService::ActiveStatusFromSync(
    sync_pb::SearchEngineSpecifics_ActiveStatus is_active) {
  switch (is_active) {
    case sync_pb::SearchEngineSpecifics_ActiveStatus::
        SearchEngineSpecifics_ActiveStatus_ACTIVE_STATUS_UNSPECIFIED:
      return TemplateURLData::ActiveStatus::kUnspecified;
    case sync_pb::SearchEngineSpecifics_ActiveStatus::
        SearchEngineSpecifics_ActiveStatus_ACTIVE_STATUS_TRUE:
      return TemplateURLData::ActiveStatus::kTrue;
    case sync_pb::SearchEngineSpecifics_ActiveStatus::
        SearchEngineSpecifics_ActiveStatus_ACTIVE_STATUS_FALSE:
      return TemplateURLData::ActiveStatus::kFalse;
  }
}

// static
sync_pb::SearchEngineSpecifics_ActiveStatus
TemplateURLService::ActiveStatusToSync(
    TemplateURLData::ActiveStatus is_active) {
  switch (is_active) {
    case TemplateURLData::ActiveStatus::kUnspecified:
      return sync_pb::SearchEngineSpecifics_ActiveStatus::
          SearchEngineSpecifics_ActiveStatus_ACTIVE_STATUS_UNSPECIFIED;
    case TemplateURLData::ActiveStatus::kTrue:
      return sync_pb::SearchEngineSpecifics_ActiveStatus::
          SearchEngineSpecifics_ActiveStatus_ACTIVE_STATUS_TRUE;
    case TemplateURLData::ActiveStatus::kFalse:
      return sync_pb::SearchEngineSpecifics_ActiveStatus::
          SearchEngineSpecifics_ActiveStatus_ACTIVE_STATUS_FALSE;
  }
}

// static
syncer::SyncData TemplateURLService::CreateSyncDataFromTemplateURL(
    const TemplateURL& turl) {
  sync_pb::EntitySpecifics specifics;
  sync_pb::SearchEngineSpecifics* se_specifics =
      specifics.mutable_search_engine();
  se_specifics->set_short_name(base::UTF16ToUTF8(turl.short_name()));
  se_specifics->set_keyword(base::UTF16ToUTF8(turl.keyword()));
  se_specifics->set_favicon_url(turl.favicon_url().spec());
  se_specifics->set_url(turl.url());
  se_specifics->set_safe_for_autoreplace(turl.safe_for_autoreplace());
  se_specifics->set_originating_url(turl.originating_url().spec());
  se_specifics->set_date_created(turl.date_created().ToInternalValue());
  se_specifics->set_input_encodings(
      base::JoinString(turl.input_encodings(), ";"));
  se_specifics->set_suggestions_url(turl.suggestions_url());
  se_specifics->set_prepopulate_id(turl.prepopulate_id());
  if (!turl.image_url().empty())
    se_specifics->set_image_url(turl.image_url());
  se_specifics->set_new_tab_url(turl.new_tab_url());
  if (!turl.search_url_post_params().empty())
    se_specifics->set_search_url_post_params(turl.search_url_post_params());
  if (!turl.suggestions_url_post_params().empty()) {
    se_specifics->set_suggestions_url_post_params(
        turl.suggestions_url_post_params());
  }
  if (!turl.image_url_post_params().empty())
    se_specifics->set_image_url_post_params(turl.image_url_post_params());
  se_specifics->set_last_modified(turl.last_modified().ToInternalValue());
  se_specifics->set_sync_guid(turl.sync_guid());
  for (size_t i = 0; i < turl.alternate_urls().size(); ++i)
    se_specifics->add_alternate_urls(turl.alternate_urls()[i]);
  se_specifics->set_is_active(ActiveStatusToSync(turl.is_active()));
  se_specifics->set_starter_pack_id(turl.starter_pack_id());

  return syncer::SyncData::CreateLocalData(se_specifics->sync_guid(),
                                           se_specifics->keyword(),
                                           specifics);
}

// static
std::unique_ptr<TemplateURL>
TemplateURLService::CreateTemplateURLFromTemplateURLAndSyncData(
    TemplateURLServiceClient* client,
    PrefService* prefs,
    search_engines::SearchEngineChoiceService* search_engine_choice_service,
    const SearchTermsData& search_terms_data,
    const TemplateURL* existing_turl,
    const syncer::SyncData& sync_data,
    syncer::SyncChangeList* change_list) {
  DCHECK(change_list);

  sync_pb::SearchEngineSpecifics specifics =
      sync_data.GetSpecifics().search_engine();

  // Past bugs might have caused either of these fields to be empty.  Just
  // delete this data off the server.
  if (specifics.url().empty() || specifics.sync_guid().empty() ||
      specifics.keyword().empty()) {
    change_list->emplace_back(FROM_HERE, syncer::SyncChange::ACTION_DELETE,
                              sync_data);
    UMA_HISTOGRAM_ENUMERATION(kDeleteSyncedEngineHistogramName,
        DELETE_ENGINE_EMPTY_FIELD, DELETE_ENGINE_MAX);
    return nullptr;
  }

  // Throw out anything from sync that has an invalid starter pack ID.  This
  // might happen occasionally when the starter pack gets new entries that are
  // not yet supported in this version of Chrome.
  if (specifics.starter_pack_id() >=
      TemplateURLStarterPackData::kMaxStarterPackID) {
    return nullptr;
  }

  TemplateURLData data(existing_turl ?
      existing_turl->data() : TemplateURLData());
  data.SetShortName(base::UTF8ToUTF16(specifics.short_name()));
  data.originating_url = GURL(specifics.originating_url());
  std::u16string keyword(base::UTF8ToUTF16(specifics.keyword()));
  DCHECK(!keyword.empty());
  data.SetKeyword(keyword);
  data.SetURL(specifics.url());
  data.suggestions_url = specifics.suggestions_url();
  data.image_url = specifics.image_url();
  data.new_tab_url = specifics.new_tab_url();
  data.search_url_post_params = specifics.search_url_post_params();
  data.suggestions_url_post_params = specifics.suggestions_url_post_params();
  data.image_url_post_params = specifics.image_url_post_params();
  data.favicon_url = GURL(specifics.favicon_url());
  data.safe_for_autoreplace = specifics.safe_for_autoreplace();
  data.input_encodings = base::SplitString(
      specifics.input_encodings(), ";",
      base::TRIM_WHITESPACE, base::SPLIT_WANT_ALL);
  // If the server data has duplicate encodings, we'll want to push an update
  // below to correct it.  Note that we also fix this in
  // GetSearchProvidersUsingKeywordResult(), since otherwise we'd never correct
  // local problems for clients which have disabled search engine sync.
  bool deduped = DeDupeEncodings(&data.input_encodings);
  data.date_created = base::Time::FromInternalValue(specifics.date_created());
  data.last_modified = base::Time::FromInternalValue(specifics.last_modified());
  data.prepopulate_id = specifics.prepopulate_id();
  data.sync_guid = specifics.sync_guid();
  data.alternate_urls.clear();
  for (int i = 0; i < specifics.alternate_urls_size(); ++i)
    data.alternate_urls.push_back(specifics.alternate_urls(i));
  data.is_active = ActiveStatusFromSync(specifics.is_active());
  data.starter_pack_id = specifics.starter_pack_id();

  std::unique_ptr<TemplateURL> turl(new TemplateURL(data));
  // If this TemplateURL matches a built-in prepopulated template URL, it's
  // possible that sync is trying to modify fields that should not be touched.
  // Revert these fields to the built-in values.
  UpdateTemplateURLIfPrepopulated(turl.get(), prefs,
                                  search_engine_choice_service);

  DCHECK_EQ(TemplateURL::NORMAL, turl->type());
  if (deduped) {
    syncer::SyncData updated_sync_data = CreateSyncDataFromTemplateURL(*turl);
    change_list->push_back(syncer::SyncChange(
        FROM_HERE, syncer::SyncChange::ACTION_UPDATE, updated_sync_data));
  } else if (turl->IsGoogleSearchURLWithReplaceableKeyword(search_terms_data)) {
    if (!existing_turl) {
      // We're adding a new TemplateURL that uses the Google base URL, so set
      // its keyword appropriately for the local environment.
      turl->ResetKeywordIfNecessary(search_terms_data, false);
    } else if (existing_turl->IsGoogleSearchURLWithReplaceableKeyword(
        search_terms_data)) {
      // Ignore keyword changes triggered by the Google base URL changing on
      // another client.  If the base URL changes in this client as well, we'll
      // pick that up separately at the appropriate time.  Otherwise, changing
      // the keyword here could result in having the wrong keyword for the local
      // environment.
      turl->data_.SetKeyword(existing_turl->keyword());
    }
  }

  return turl;
}

// static
SyncDataMap TemplateURLService::CreateGUIDToSyncDataMap(
    const syncer::SyncDataList& sync_data) {
  SyncDataMap data_map;
  for (auto i(sync_data.begin()); i != sync_data.end(); ++i)
    data_map[i->GetSpecifics().search_engine().sync_guid()] = *i;
  return data_map;
}

void TemplateURLService::Init() {
  if (client_)
    client_->SetOwner(this);

  pref_change_registrar_.Init(&prefs_.get());
  if (base::FeatureList::IsEnabled(switches::kSearchEngineChoiceTrigger)) {
    // We migrate `kSyncedDefaultSearchProviderGUID` to
    // `kDefaultSearchProviderGUID` if the latter was never set.
    if (!prefs_->HasPrefPath(prefs::kDefaultSearchProviderGUID)) {
      prefs_->SetString(
          prefs::kDefaultSearchProviderGUID,
          prefs_->GetString(prefs::kSyncedDefaultSearchProviderGUID));
    }

    pref_change_registrar_.Add(
        prefs::kDefaultSearchProviderGUID,
        base::BindRepeating(
            &TemplateURLService::OnDefaultSearchProviderGUIDChanged,
            base::Unretained(this)));
  } else {
    // TODO(b/364828491): Deprecate `kSyncedDefaultSearchProviderGUID`.
    pref_change_registrar_.Add(
        prefs::kSyncedDefaultSearchProviderGUID,
        base::BindRepeating(
            &TemplateURLService::OnDefaultSearchProviderGUIDChanged,
            base::Unretained(this)));
  }

  DefaultSearchManager::Source source = DefaultSearchManager::FROM_USER;
  const TemplateURLData* dse =
      default_search_manager_.GetDefaultSearchEngine(&source);

  Scoper scoper(this);

  ApplyDefaultSearchChange(dse, source);
}

void TemplateURLService::ApplyInitializersForTesting(
    base::span<const TemplateURLService::Initializer> initializers) {
  // This path is only hit by test code and is used to simulate a loaded
  // TemplateURLService.
  CHECK_IS_TEST();

  if (initializers.empty()) {
    return;
  }

  ChangeToLoadedState();

  // Add specific initializers, if any.
  for (size_t i = 0; i < initializers.size(); ++i) {
    CHECK(initializers[i].keyword);
    CHECK(initializers[i].url);
    CHECK(initializers[i].content);

    // TemplateURLService ends up owning the TemplateURL, don't try and free
    // it.
    TemplateURLData data;
    data.SetShortName(base::UTF8ToUTF16(initializers[i].content));
    data.SetKeyword(base::UTF8ToUTF16(initializers[i].keyword));
    data.SetURL(initializers[i].url);
    // Set all to active by default for testing purposes.
    data.is_active = TemplateURLData::ActiveStatus::kTrue;
    Add(std::make_unique<TemplateURL>(data));

    // Set the first provided identifier to be the default.
    if (i == 0) {
      default_search_manager_.SetUserSelectedDefaultSearchEngine(data);
    }
  }
}

void TemplateURLService::RemoveFromMaps(const TemplateURL* template_url) {
  const std::u16string& keyword = template_url->keyword();

  // Remove from |keyword_to_turl_|. No need to find the best
  // fallback. We choose the best one as-needed from the multimap.
  const auto match_range = keyword_to_turl_.equal_range(keyword);
  for (auto it = match_range.first; it != match_range.second;) {
    if (it->second == template_url) {
      it = keyword_to_turl_.erase(it);
    } else {
      ++it;
    }
  }

  if (template_url->type() == TemplateURL::OMNIBOX_API_EXTENSION)
    return;

  if (!template_url->sync_guid().empty()) {
    guid_to_turl_.erase(template_url->sync_guid());
    if (postponed_deleted_default_engine_guid_ == template_url->sync_guid()) {
      // `template_url` has been updated locally or removed, discard incoming
      // deletion.
      postponed_deleted_default_engine_guid_.clear();
    }
  }

  if (template_url->created_by_policy() ==
      TemplateURLData::CreatedByPolicy::kSiteSearch) {
    enterprise_site_search_keyword_to_turl_.erase(keyword);
  }

  // |provider_map_| is only initialized after loading has completed.
  if (loaded_) {
    provider_map_->Remove(template_url);
  }
}

void TemplateURLService::AddToMaps(TemplateURL* template_url) {
  const std::u16string& keyword = template_url->keyword();
  keyword_to_turl_.insert(std::make_pair(keyword, template_url));

  if (template_url->type() == TemplateURL::OMNIBOX_API_EXTENSION)
    return;

  if (!template_url->sync_guid().empty())
    guid_to_turl_[template_url->sync_guid()] = template_url;

  if (template_url->created_by_policy() ==
      TemplateURLData::CreatedByPolicy::kSiteSearch) {
    enterprise_site_search_keyword_to_turl_[keyword] = template_url;
  }

  // |provider_map_| is only initialized after loading has completed.
  if (loaded_)
    provider_map_->Add(template_url, search_terms_data());
}

void TemplateURLService::SetTemplateURLs(
    std::unique_ptr<OwnedTemplateURLVector> urls) {
  Scoper scoper(this);

  // Partition the URLs first, instead of implementing the loops below by simply
  // scanning the input twice.  While it's not supposed to happen normally, it's
  // possible for corrupt databases to return multiple entries with the same
  // keyword.  In this case, the first loop may delete the first entry when
  // adding the second.  If this happens, the second loop must not attempt to
  // access the deleted entry.  Partitioning ensures this constraint.
  auto first_invalid = std::partition(
      urls->begin(), urls->end(), [](const std::unique_ptr<TemplateURL>& turl) {
        return turl->id() != kInvalidTemplateURLID;
      });

  // First, add the items that already have id's, so that the next_id_ gets
  // properly set.
  for (auto i = urls->begin(); i != first_invalid; ++i) {
    next_id_ = std::max(next_id_, (*i)->id());
    Add(std::move(*i), false);
  }

  // Next add the new items that don't have id's.
  for (auto i = first_invalid; i != urls->end(); ++i)
    Add(std::move(*i));
}

void TemplateURLService::ChangeToLoadedState() {
  DCHECK(!loaded_);

  provider_map_->Init(template_urls_, search_terms_data());
  loaded_ = true;

  ApplyDefaultSearchChangeNoMetrics(
      pre_loading_providers_->default_search_provider()
          ? &pre_loading_providers_->default_search_provider()->data()
          : nullptr,
      default_search_provider_source_);
  if (base::FeatureList::IsEnabled(omnibox::kSiteSearchSettingsPolicy)) {
    ApplyEnterpriseSiteSearchChanges(
        pre_loading_providers_->TakeSiteSearchEngines());
  }
  pre_loading_providers_.reset();

  if (on_loaded_callback_for_sync_)
    std::move(on_loaded_callback_for_sync_).Run();

  on_loaded_callbacks_.Notify();
}

bool TemplateURLService::CanAddAutogeneratedKeywordForHost(
    const std::string& host) const {
  const TemplateURLSet* urls = provider_map_->GetURLsForHost(host);
  if (!urls)
    return true;

  return base::ranges::all_of(*urls, [](const TemplateURL* turl) {
    return turl->safe_for_autoreplace();
  });
}

bool TemplateURLService::Update(TemplateURL* existing_turl,
                                const TemplateURL& new_values) {
  DCHECK(existing_turl);
  if (!Contains(&template_urls_, existing_turl))
    return false;

  Scoper scoper(this);
  model_mutated_notification_pending_ = true;

  TemplateURLID previous_id = existing_turl->id();
  RemoveFromMaps(existing_turl);

  // Update existing turl with new values and add back to the map.
  // We don't do any keyword conflict handling here, as TemplateURLService
  // already can pick the best engine out of duplicates. Replaceable duplicates
  // will be culled during next startup's Add() loop. We did this to keep
  // Update() simple: it never fails, and never deletes |existing_engine|.
  existing_turl->CopyFrom(new_values);
  existing_turl->data_.id = previous_id;

  AddToMaps(existing_turl);

  if (existing_turl->type() == TemplateURL::NORMAL) {
    if (web_data_service_)
      web_data_service_->UpdateKeyword(existing_turl->data());

    // Inform sync of the update.
    ProcessTemplateURLChange(FROM_HERE, existing_turl,
                             syncer::SyncChange::ACTION_UPDATE);
  }

  // Even if the DSE is controlled by an extension or policy, update the user
  // preferences as they may take over later.
  if (default_search_provider_source_ != DefaultSearchManager::FROM_FALLBACK)
    MaybeUpdateDSEViaPrefs(existing_turl);

  return true;
}

// static
void TemplateURLService::UpdateTemplateURLIfPrepopulated(
    TemplateURL* template_url,
    PrefService* prefs,
    search_engines::SearchEngineChoiceService* search_engine_choice_service) {
  int prepopulate_id = template_url->prepopulate_id();
  if (template_url->prepopulate_id() == 0)
    return;

  std::vector<std::unique_ptr<TemplateURLData>> prepopulated_urls =
      TemplateURLPrepopulateData::GetPrepopulatedEngines(
          prefs, search_engine_choice_service);

  for (const auto& url : prepopulated_urls) {
    if (url->prepopulate_id == prepopulate_id) {
      MergeIntoEngineData(template_url, url.get());
      template_url->CopyFrom(TemplateURL(*url));
    }
  }
}

void TemplateURLService::MaybeUpdateDSEViaPrefs(TemplateURL* synced_turl) {
  // The DSE is not synced anymore when the `kSearchEngineChoiceTrigger` feature
  // is enabled.
  // TODO(b/341011768): Remove DSE sync code.
  if (base::FeatureList::IsEnabled(switches::kSearchEngineChoiceTrigger)) {
    return;
  }

  if (synced_turl->sync_guid() ==
      GetDefaultSearchProviderGuidFromPrefs(prefs_.get())) {
    default_search_manager_.SetUserSelectedDefaultSearchEngine(
        synced_turl->data());
  }
}

void TemplateURLService::UpdateKeywordSearchTermsForURL(
    const URLVisitedDetails& details) {
  if (!details.url.is_valid())
    return;

  const TemplateURLSet* urls_for_host =
      provider_map_->GetURLsForHost(details.url.host());
  if (!urls_for_host)
    return;

  TemplateURL* visited_url = nullptr;
  for (auto i = urls_for_host->begin(); i != urls_for_host->end(); ++i) {
    std::u16string search_terms;
    if ((*i)->ExtractSearchTermsFromURL(details.url, search_terms_data(),
                                        &search_terms) &&
        !search_terms.empty()) {
      if (details.is_keyword_transition) {
        // The visit is the result of the user entering a keyword, generate a
        // KEYWORD_GENERATED visit for the KEYWORD so that the keyword typed
        // count is boosted.
        AddTabToSearchVisit(**i);
      }
      if (client_) {
        client_->SetKeywordSearchTermsForURL(
            details.url, (*i)->id(), search_terms);
      }
      // Caches the matched TemplateURL so its last_visited could be updated
      // later after iteration.
      // Note: Update() will replace the entry from the container of this
      // iterator, so update here directly will cause an error about it.
      if (!IsCreatedByExtension(*i))
        visited_url = *i;
    }
  }
  if (visited_url)
    UpdateTemplateURLVisitTime(visited_url);
}

void TemplateURLService::UpdateTemplateURLVisitTime(TemplateURL* url) {
  TemplateURLData data(url->data());
  data.last_visited = clock_->Now();
  Update(url, TemplateURL(data));
}

void TemplateURLService::AddTabToSearchVisit(const TemplateURL& t_url) {
  // Only add visits for entries the user hasn't modified. If the user modified
  // the entry the keyword may no longer correspond to the host name. It may be
  // possible to do something more sophisticated here, but it's so rare as to
  // not be worth it.
  if (!t_url.safe_for_autoreplace())
    return;

  if (!client_)
    return;

  GURL url(url_formatter::FixupURL(base::UTF16ToUTF8(t_url.keyword()),
                                   std::string()));
  if (!url.is_valid())
    return;

  // Synthesize a visit for the keyword. This ensures the url for the keyword is
  // autocompleted even if the user doesn't type the url in directly.
  client_->AddKeywordGeneratedVisit(url);
}

void TemplateURLService::ApplyDefaultSearchChange(
    const TemplateURLData* data,
    DefaultSearchManager::Source source) {
  if (!ApplyDefaultSearchChangeNoMetrics(data, source))
    return;

  if (GetDefaultSearchProvider() &&
      GetDefaultSearchProvider()->HasGoogleBaseURLs(search_terms_data()) &&
      !dsp_change_callback_.is_null())
    dsp_change_callback_.Run();
}

bool TemplateURLService::ApplyDefaultSearchChangeForTesting(
    const TemplateURLData* data,
    DefaultSearchManager::Source source) {
  CHECK_IS_TEST();
  return ApplyDefaultSearchChangeNoMetrics(data, source);
}

bool TemplateURLService::ApplyDefaultSearchChangeNoMetrics(
    const TemplateURLData* data,
    DefaultSearchManager::Source source) {
  // We do not want any sort of reentrancy while changing the default search
  // engine. This can occur when resolving conflicting entries. In those cases,
  // it's best to early exit and let the original process finish.
  if (applying_default_search_engine_change_)
    return false;
  base::AutoReset<bool> applying_change(&applying_default_search_engine_change_,
                                        true);

  if (!loaded_) {
    // Set pre-loading default search provider from the preferences. This is
    // mainly so we can hold ownership until we get to the point where the list
    // of keywords from Web Data is the owner of everything including the
    // default.
    bool changed = !TemplateURL::MatchesData(
        pre_loading_providers_->default_search_provider(), data,
        search_terms_data());
    TemplateURL::Type initial_engine_type =
        (source == DefaultSearchManager::FROM_EXTENSION)
            ? TemplateURL::NORMAL_CONTROLLED_BY_EXTENSION
            : TemplateURL::NORMAL;
    pre_loading_providers_->set_default_search_provider(
        data ? std::make_unique<TemplateURL>(*data, initial_engine_type)
             : nullptr);
    default_search_provider_source_ = source;
    return changed;
  }

  // This may be deleted later. Use exclusively for pointer comparison to detect
  // a change.
  TemplateURL* previous_default_search_engine = default_search_provider_;

  Scoper scoper(this);

  if (default_search_provider_source_ == DefaultSearchManager::FROM_POLICY ||
      default_search_provider_source_ ==
          DefaultSearchManager::FROM_POLICY_RECOMMENDED ||
      source == DefaultSearchManager::FROM_POLICY ||
      source == DefaultSearchManager::FROM_POLICY_RECOMMENDED) {
    // We do this both to remove any no-longer-applicable policy-defined DSE as
    // well as to add the new one, if appropriate.
    UpdateDefaultProvidersCreatedByPolicy(
        &template_urls_,
        source == DefaultSearchManager::FROM_POLICY ||
                source == DefaultSearchManager::FROM_POLICY_RECOMMENDED
            ? data
            : nullptr,
        /*is_mandatory=*/source == DefaultSearchManager::FROM_POLICY);
  }

  // |default_search_provider_source_| must be set before calling Update(),
  // since that function needs to know the source of the update in question.
  default_search_provider_source_ = source;

  if (!data) {
    default_search_provider_ = nullptr;
  } else if (source == DefaultSearchManager::FROM_EXTENSION) {
    default_search_provider_ = FindMatchingDefaultExtensionTemplateURL(*data);
  } else if (source == DefaultSearchManager::FROM_FALLBACK) {
    default_search_provider_ =
        FindPrepopulatedTemplateURL(data->prepopulate_id);
    if (default_search_provider_) {
      TemplateURLData update_data(*data);
      update_data.sync_guid = default_search_provider_->sync_guid();

      // Now that we are auto-updating the favicon_url as the user browses,
      // respect the favicon_url entry in the database, instead of falling back
      // to the one in the prepopulated list.
      update_data.favicon_url = default_search_provider_->favicon_url();

      if (!default_search_provider_->safe_for_autoreplace()) {
        update_data.safe_for_autoreplace = false;
        update_data.SetKeyword(default_search_provider_->keyword());
        update_data.SetShortName(default_search_provider_->short_name());
      }
      Update(default_search_provider_, TemplateURL(update_data));
    } else {
      // Normally the prepopulated fallback should be present in
      // |template_urls_|, but in a few cases it might not be:
      // (1) Tests that initialize the TemplateURLService in peculiar ways.
      // (2) If the user deleted the pre-populated default and we subsequently
      // lost their user-selected value.
      default_search_provider_ = Add(std::make_unique<TemplateURL>(*data));
      DCHECK(default_search_provider_)
          << "Add() to repair the DSE must never fail.";
    }
  } else if (source == DefaultSearchManager::FROM_USER) {
    default_search_provider_ = GetTemplateURLForGUID(data->sync_guid);
    if (!default_search_provider_ && data->prepopulate_id) {
      default_search_provider_ =
          FindPrepopulatedTemplateURL(data->prepopulate_id);
    }
    TemplateURLData new_data(*data);
    if (default_search_provider_) {
      Update(default_search_provider_, TemplateURL(new_data));
    } else {
      new_data.id = kInvalidTemplateURLID;
      default_search_provider_ = Add(std::make_unique<TemplateURL>(new_data));
      DCHECK(default_search_provider_)
          << "Add() to repair the DSE must never fail.";
    }
    if (default_search_provider_) {
      SetDefaultSearchProviderGuidToPrefs(
          prefs_.get(), default_search_provider_->sync_guid());
    }
  }

  if (default_search_provider_ == previous_default_search_engine) {
    // Default search engine hasn't changed.
    return false;
  }

  model_mutated_notification_pending_ = true;
  if (!postponed_deleted_default_engine_guid_.empty()) {
    // There was a postponed deletion for the previous default search engine,
    // remove it now.
    TemplateURL* existing_turl =
        GetTemplateURLForGUID(postponed_deleted_default_engine_guid_);
    if (existing_turl) {
      // Remove() below CHECKs that the current default search engine is not
      // deleted.
      Remove(existing_turl);
    }
    postponed_deleted_default_engine_guid_.clear();
  }

  return true;
}

void TemplateURLService::ApplyEnterpriseSiteSearchChanges(
    TemplateURLService::OwnedTemplateURLVector&& policy_site_search_engines) {
  CHECK(loaded_);

  Scoper scoper(this);

  LogSiteSearchPolicyConflict(policy_site_search_engines);

  base::flat_set<std::u16string> new_keywords;
  base::ranges::transform(
      policy_site_search_engines,
      std::inserter(new_keywords, new_keywords.begin()),
      [](const std::unique_ptr<TemplateURL>& turl) { return turl->keyword(); });

  // Remove old site search entries no longer present in the policy's list.
  //
  // Note: auxiliary `keywords_to_remove` is needed to avoid reentry issues
  //       while removing elements from
  //       `enterprise_site_search_keyword_to_turl_`.
  //
  // Note: This can be made more idiomatic once Chromium style allows
  //       `std::views::keys`:
  //       std::copy_if(
  //           std::views::keys(enterprise_site_search_keyword_to_turl_),
  //           std::inserter(keywords_to_remove, keywords_to_remove.begin()),
  //           [new_keywords] (const std::u16string& keyword) {
  //             new_keywords.find(keyword) == new_keywords.end()
  //           });
  base::flat_set<std::u16string> keywords_to_remove;
  for (auto [keyword, _] : enterprise_site_search_keyword_to_turl_) {
    if (new_keywords.find(keyword) == new_keywords.end()) {
      keywords_to_remove.insert(keyword);
    }
  }
  base::ranges::for_each(
      keywords_to_remove, [this](const std::u16string& keyword) {
        Remove(enterprise_site_search_keyword_to_turl_[keyword]);
      });

  // Either add new site search entries or update existing ones if necessary.
  for (auto& site_search_engine : policy_site_search_engines) {
    const std::u16string& keyword = site_search_engine->keyword();
    auto it = enterprise_site_search_keyword_to_turl_.find(keyword);
    if (it == enterprise_site_search_keyword_to_turl_.end()) {
      Add(std::move(site_search_engine), /*newly_adding=*/true);
    } else if (ShouldMergeEnterpriseSiteSearchEngines(
                   /*existing_turl=*/*it->second,
                   /*new_values=*/*site_search_engine)) {
      Update(/*existing_turl=*/it->second,
             /*new_values=*/MergeEnterpriseSiteSearchEngines(
                 /*existing_turl=*/*it->second,
                 /*new_values=*/*site_search_engine));
    }
  }

  // TODO(b/314162426): Check interaction with DSP not set by policy.
  // TODO(b/309456406): Override existing SE if keywords starts with "@" and
  //                    this is a featured site search entry.
}

void TemplateURLService::EnterpriseSiteSearchChanged(
    OwnedTemplateURLDataVector&& policy_site_search_engines) {
  OwnedTemplateURLVector turl_site_search_engines;
  for (const std::unique_ptr<TemplateURLData>& it :
       policy_site_search_engines) {
    turl_site_search_engines.push_back(
        std::make_unique<TemplateURL>(*it, TemplateURL::NORMAL));
  }

  if (loaded_) {
    ApplyEnterpriseSiteSearchChanges(std::move(turl_site_search_engines));
  } else {
    pre_loading_providers_->set_site_search_engines(
        std::move(turl_site_search_engines));
  }
}

TemplateURL* TemplateURLService::Add(std::unique_ptr<TemplateURL> template_url,
                                     bool newly_adding) {
  DCHECK(template_url);

  Scoper scoper(this);

  if (newly_adding) {
    DCHECK_EQ(kInvalidTemplateURLID, template_url->id());
    DCHECK(!Contains(&template_urls_, template_url.get()));
    template_url->data_.id = ++next_id_;
  }

  template_url->ResetKeywordIfNecessary(search_terms_data(), false);

  // Early exit if the newly added TemplateURL was a replaceable duplicate.
  // No need to inform either Sync or flag on the model-mutated in that case.
  if (RemoveDuplicateReplaceableEnginesOf(template_url.get())) {
    return nullptr;
  }

  TemplateURL* template_url_ptr = template_url.get();
  template_urls_.push_back(std::move(template_url));
  AddToMaps(template_url_ptr);

  if (newly_adding && (template_url_ptr->type() == TemplateURL::NORMAL)) {
    if (web_data_service_)
      web_data_service_->AddKeyword(template_url_ptr->data());

    // Inform sync of the addition. Note that this will assign a GUID to
    // template_url and add it to the guid_to_turl_.
    ProcessTemplateURLChange(FROM_HERE, template_url_ptr,
                             syncer::SyncChange::ACTION_ADD);
  }

  if (template_url_ptr)
    model_mutated_notification_pending_ = true;

  return template_url_ptr;
}

// |template_urls| are the TemplateURLs loaded from the database.
// |default_from_prefs| is the default search provider from the preferences, or
// NULL if the DSE is not policy-defined.
//
// This function removes from the vector and the database all the TemplateURLs
// that were set by policy as default provider, unless it is the current default
// search provider, in which case it is updated with the data from prefs.
void TemplateURLService::UpdateDefaultProvidersCreatedByPolicy(
    OwnedTemplateURLVector* template_urls,
    const TemplateURLData* default_from_prefs,
    bool is_mandatory) {
  DCHECK(template_urls);

  Scoper scoper(this);

  for (auto i = template_urls->begin(); i != template_urls->end();) {
    TemplateURL* template_url = i->get();
    if (template_url->created_by_policy() ==
        TemplateURLData::CreatedByPolicy::kDefaultSearchProvider) {
      if (default_from_prefs &&
          TemplateURL::MatchesData(template_url, default_from_prefs,
                                   search_terms_data())) {
        // If the database specified a default search provider that was set
        // by policy, and the default search provider from the preferences
        // is also set by policy and they are the same, keep the entry in the
        // database and the |default_search_provider|.
        default_search_provider_ = template_url;
        // Prevent us from saving any other entries, or creating a new one.
        default_from_prefs = nullptr;
        ++i;
        continue;
      }

      // If the previous default search provider was set as a recommended policy
      // and the new provider is not set by policy, keep the previous provider
      // in the database. This allows the recommended provider to remain in the
      // list if the user switches to a different provider.
      if (template_url->enforced_by_policy() || default_from_prefs) {
        TemplateURLID id = template_url->id();
        RemoveFromMaps(template_url);
        i = template_urls->erase(i);
        if (web_data_service_) {
          web_data_service_->RemoveKeyword(id);
        }
      } else {
        ++i;
      }
    } else {
      ++i;
    }
  }

  if (default_from_prefs) {
    default_search_provider_ = nullptr;
    default_search_provider_source_ =
        is_mandatory ? DefaultSearchManager::FROM_POLICY
                     : DefaultSearchManager::FROM_POLICY_RECOMMENDED;
    TemplateURLData new_data(*default_from_prefs);
    if (new_data.sync_guid.empty())
      new_data.GenerateSyncGUID();
    new_data.created_by_policy =
        TemplateURLData::CreatedByPolicy::kDefaultSearchProvider;
    new_data.enforced_by_policy = is_mandatory;
    new_data.safe_for_autoreplace = false;
    new_data.is_active = TemplateURLData::ActiveStatus::kTrue;
    std::unique_ptr<TemplateURL> new_dse_ptr =
        std::make_unique<TemplateURL>(new_data);
    TemplateURL* new_dse = new_dse_ptr.get();
    if (Add(std::move(new_dse_ptr)))
      default_search_provider_ = new_dse;
  }
}

void TemplateURLService::ResetTemplateURLGUID(TemplateURL* url,
                                              const std::string& guid) {
  DCHECK(loaded_);
  DCHECK(!guid.empty());

  TemplateURLData data(url->data());
  data.sync_guid = guid;
  Update(url, TemplateURL(data));
}

void TemplateURLService::MergeInSyncTemplateURL(
    TemplateURL* sync_turl,
    const SyncDataMap& sync_data,
    syncer::SyncChangeList* change_list,
    SyncDataMap* local_data) {
  DCHECK(sync_turl);
  DCHECK(!GetTemplateURLForGUID(sync_turl->sync_guid()));
  DCHECK(IsFromSync(sync_turl, sync_data));

  bool should_add_sync_turl = true;

  Scoper scoper(this);

  // First resolve conflicts with local duplicate keyword NORMAL TemplateURLs,
  // working from best to worst.
  DCHECK(sync_turl->type() == TemplateURL::NORMAL);
  std::vector<TemplateURL*> local_duplicates;
  const auto match_range = keyword_to_turl_.equal_range(sync_turl->keyword());
  for (auto it = match_range.first; it != match_range.second; ++it) {
    TemplateURL* local_turl = it->second;
    // The conflict resolution code below sometimes resets the TemplateURL's
    // GUID, which can trigger deleting any Policy-created engines. Avoid this
    // use-after-free bug by excluding any Policy-created engines. Also exclude
    // Play API created engines, as those also seem local-only and should not
    // be merged into Synced engines. crbug.com/1414224.
    if (local_turl->type() == TemplateURL::NORMAL &&
        local_turl->created_by_policy() ==
            TemplateURLData::CreatedByPolicy::kNoPolicy &&
        !local_turl->created_from_play_api()) {
      local_duplicates.push_back(local_turl);
    }
  }
  base::ranges::sort(local_duplicates, [&](const auto& a, const auto& b) {
    return a->IsBetterThanConflictingEngine(b);
  });
  for (TemplateURL* conflicting_turl : local_duplicates) {
    if (IsFromSync(conflicting_turl, sync_data)) {
      // |conflicting_turl| is already known to Sync, so we're not allowed to
      // remove it. Just leave it. TemplateURLService can tolerate duplicates.
      // TODO(tommycli): Eventually we should figure out a way to merge
      // substantively identical ones or somehow otherwise cull the herd.
      continue;
    }

    // |conflicting_turl| is not yet known to Sync. If it is better, then we
    // want to transfer its values up to sync. Otherwise, we remove it and
    // allow the entry from Sync to overtake it in the model.
    const std::string guid = conflicting_turl->sync_guid();
    if (conflicting_turl == GetDefaultSearchProvider() ||
        conflicting_turl->IsBetterThanConflictingEngine(sync_turl)) {
      ResetTemplateURLGUID(conflicting_turl, sync_turl->sync_guid());
      syncer::SyncData updated_sync_data =
          CreateSyncDataFromTemplateURL(*conflicting_turl);
      change_list->push_back(syncer::SyncChange(
          FROM_HERE, syncer::SyncChange::ACTION_UPDATE, updated_sync_data));
      // Note that in this case we do not add the Sync TemplateURL to the
      // local model, since we've effectively "merged" it in by updating the
      // local conflicting entry with its sync_guid.
      should_add_sync_turl = false;
    } else {
      // We guarantee that this isn't the local search provider. Otherwise,
      // local would have won.
      DCHECK(conflicting_turl != GetDefaultSearchProvider());
      Remove(conflicting_turl);
    }
    // This TemplateURL was either removed or overwritten in the local model.
    // Remove the entry from the local data so it isn't pushed up to Sync.
    local_data->erase(guid);
  }

  // Try to take over a local built-in (prepopulated or starter pack) entry,
  // assuming we haven't already run into a keyword conflict.
  if (local_duplicates.empty() &&
      (sync_turl->prepopulate_id() != 0 || sync_turl->starter_pack_id() != 0)) {
    // Check for a turl with a conflicting prepopulate_id. This detects the case
    // where the user changes a prepopulated engine's keyword on one client,
    // then begins syncing on another client.  We want to reflect this keyword
    // change to that prepopulated URL on other clients instead of assuming that
    // the modified TemplateURL is a new entity.
    TemplateURL* conflicting_built_in_turl =
        (sync_turl->prepopulate_id() != 0)
            ? FindPrepopulatedTemplateURL(sync_turl->prepopulate_id())
            : FindStarterPackTemplateURL(sync_turl->starter_pack_id());

    // If we found a conflict, and the sync entity is better, apply the remote
    // changes locally. We consider |sync_turl| better if it's been modified
    // more recently and the local TemplateURL isn't yet known to sync. We will
    // consider the sync entity better even if the local TemplateURL is the
    // current default, since in this case being default does not necessarily
    // mean the user explicitly set this TemplateURL as default. If we didn't do
    // this, changes to the keywords of prepopulated default engines would never
    // be applied to other clients.
    // If we can't safely replace the local entry with the synced one, or merge
    // the relevant changes in, we give up and leave both intact.
    if (conflicting_built_in_turl &&
        !IsFromSync(conflicting_built_in_turl, sync_data) &&
        sync_turl->IsBetterThanConflictingEngine(conflicting_built_in_turl)) {
      std::string guid = conflicting_built_in_turl->sync_guid();
      if (conflicting_built_in_turl == default_search_provider_) {
        bool pref_matched =
            GetDefaultSearchProviderGuidFromPrefs(prefs_.get()) ==
            default_search_provider_->sync_guid();
        // Update the existing engine in-place.
        Update(default_search_provider_, TemplateURL(sync_turl->data()));
        // If prefs::kSyncedDefaultSearchProviderGUID matched
        // |default_search_provider_|'s GUID before, then update it to match its
        // new GUID. If the pref didn't match before, then it probably refers to
        // a new search engine from Sync which just hasn't been added locally
        // yet, so leave it alone in that case.
        if (pref_matched) {
          SetDefaultSearchProviderGuidToPrefs(
              prefs_.get(), default_search_provider_->sync_guid());
        }

        should_add_sync_turl = false;
      } else {
        Remove(conflicting_built_in_turl);
      }
      // Remove the local data so it isn't written to sync.
      local_data->erase(guid);
    }
  }

  if (should_add_sync_turl) {
    // Force the local ID to kInvalidTemplateURLID so we can add it.
    TemplateURLData data(sync_turl->data());
    data.id = kInvalidTemplateURLID;
    std::unique_ptr<TemplateURL> added_ptr =
        std::make_unique<TemplateURL>(data);
    TemplateURL* added = added_ptr.get();
    base::AutoReset<DefaultSearchChangeOrigin> change_origin(
        &dsp_change_origin_, DSP_CHANGE_SYNC_ADD);
    if (Add(std::move(added_ptr)))
      MaybeUpdateDSEViaPrefs(added);
  }
}

void TemplateURLService::PatchMissingSyncGUIDs(
    OwnedTemplateURLVector* template_urls) {
  DCHECK(template_urls);
  for (auto& template_url : *template_urls) {
    DCHECK(template_url);
    if (template_url->sync_guid().empty() &&
        (template_url->type() == TemplateURL::NORMAL)) {
      template_url->data_.GenerateSyncGUID();
      if (web_data_service_)
        web_data_service_->UpdateKeyword(template_url->data());
    }
  }
}

void TemplateURLService::OnDefaultSearchProviderGUIDChanged() {
  base::AutoReset<DefaultSearchChangeOrigin> change_origin(
      &dsp_change_origin_, DSP_CHANGE_SYNC_PREF);

  std::string new_guid = GetDefaultSearchProviderGuidFromPrefs(prefs_.get());
  if (new_guid.empty()) {
    default_search_manager_.ClearUserSelectedDefaultSearchEngine();
    return;
  }

  const TemplateURL* turl = GetTemplateURLForGUID(new_guid);
  if (turl) {
    default_search_manager_.SetUserSelectedDefaultSearchEngine(turl->data());
  }
}

void TemplateURLService::MaybeSetIsActiveSearchEngines(
    OwnedTemplateURLVector* template_urls) {
  DCHECK(template_urls);
  for (auto& turl : *template_urls) {
    DCHECK(turl);
    // An turl is "active" if it has ever been used or manually added/modified.
    // |safe_for_autoreplace| is false if the entry has been modified.
    if (turl->is_active() == TemplateURLData::ActiveStatus::kUnspecified &&
        (!turl->safe_for_autoreplace() || turl->usage_count() > 0)) {
      turl->data_.is_active = TemplateURLData::ActiveStatus::kTrue;
      turl->data_.safe_for_autoreplace = false;
      if (web_data_service_)
        web_data_service_->UpdateKeyword(turl->data());
    }
  }
}

template <typename Container>
void TemplateURLService::AddMatchingKeywordsHelper(
    const Container& keyword_to_turl,
    const std::u16string& prefix,
    bool supports_replacement_only,
    TemplateURLVector* matches) {
  // Sanity check args.
  if (prefix.empty())
    return;
  DCHECK(matches);

  // Find matching keyword range.  Searches the element map for keywords
  // beginning with |prefix| and stores the endpoints of the resulting set in
  // |match_range|.
  const auto match_range(std::equal_range(
      keyword_to_turl.begin(), keyword_to_turl.end(),
      typename Container::value_type(prefix, nullptr), LessWithPrefix()));

  // Add to vector of matching keywords.
  for (typename Container::const_iterator i(match_range.first);
    i != match_range.second; ++i) {
    if (!supports_replacement_only ||
        i->second->url_ref().SupportsReplacement(search_terms_data())) {
      matches->push_back(i->second);
    }
  }
}

TemplateURL* TemplateURLService::FindPrepopulatedTemplateURL(
    int prepopulated_id) {
  DCHECK(prepopulated_id);
  for (const auto& turl : template_urls_) {
    if (turl->prepopulate_id() == prepopulated_id)
      return turl.get();
  }
  return nullptr;
}

TemplateURL* TemplateURLService::FindStarterPackTemplateURL(
    int starter_pack_id) {
  DCHECK(starter_pack_id);
  for (const auto& turl : template_urls_) {
    if (turl->starter_pack_id() == starter_pack_id)
      return turl.get();
  }
  return nullptr;
}

TemplateURL* TemplateURLService::FindTemplateURLForExtension(
    const std::string& extension_id,
    TemplateURL::Type type) {
  DCHECK_NE(TemplateURL::NORMAL, type);
  for (const auto& turl : template_urls_) {
    if (turl->type() == type && turl->GetExtensionId() == extension_id)
      return turl.get();
  }
  return nullptr;
}

TemplateURL* TemplateURLService::FindMatchingDefaultExtensionTemplateURL(
    const TemplateURLData& data) {
  for (const auto& turl : template_urls_) {
    if (turl->type() == TemplateURL::NORMAL_CONTROLLED_BY_EXTENSION &&
        turl->extension_info_->wants_to_be_default_engine &&
        TemplateURL::MatchesData(turl.get(), &data, search_terms_data()))
      return turl.get();
  }
  return nullptr;
}

bool TemplateURLService::RemoveDuplicateReplaceableEnginesOf(
    TemplateURL* candidate) {
  DCHECK(candidate);

  // Do not replace existing search engines if `candidate` was created by the
  // `SiteSearchSettings` policy.
  if (candidate->created_by_policy() ==
      TemplateURLData::CreatedByPolicy::kSiteSearch) {
    return false;
  }

  const std::u16string& keyword = candidate->keyword();

  // If there's not at least one conflicting TemplateURL, there's nothing to do.
  const auto match_range = keyword_to_turl_.equal_range(keyword);
  if (match_range.first == match_range.second) {
    return false;
  }

  // Gather the replaceable TemplateURLs to be removed. We don't do it in-place,
  // because Remove() invalidates iterators.
  std::vector<TemplateURL*> replaceable_turls;
  for (auto it = match_range.first; it != match_range.second; ++it) {
    TemplateURL* turl = it->second;
    DCHECK_NE(turl, candidate) << "This algorithm runs BEFORE |candidate| is "
                                  "added to the keyword map.";

    // Built-in engines are marked as safe_for_autoreplace(). But because
    // they are shown in the Default Search Engines Settings UI, users would
    // find it confusing if they were ever automatically removed.
    // https://crbug.com/1164024
    if (turl->safe_for_autoreplace() && turl->prepopulate_id() == 0 &&
        turl->starter_pack_id() == 0) {
      replaceable_turls.push_back(turl);
    }
  }

  // Find the BEST engine for |keyword| factoring in the new |candidate|.
  TemplateURL* best = GetTemplateURLForKeyword(keyword);
  if (!best || candidate->IsBetterThanConflictingEngine(best)) {
    best = candidate;
  }

  // Remove all the replaceable TemplateURLs that are not the best.
  for (TemplateURL* turl : replaceable_turls) {
    DCHECK_NE(turl, candidate);

    // Never actually remove the DSE during this phase. This handling defers
    // deleting the DSE until it's no longer set as the DSE, analagous to how
    // we handle ACTION_DELETE of the DSE in ProcessSyncChanges().
    if (turl != best && !MatchesDefaultSearchProvider(turl)) {
      Remove(turl);
    }
  }

  // Caller needs to know if |candidate| would have been deleted.
  // Also always successfully add prepopulated engines, for two reasons:
  //  1. The DSE repair logic in ApplyDefaultSearchChangeNoMetrics() relies on
  //     Add()ing back the DSE always succeeding. https://crbug.com/1164024
  //  2. If we don't do this, we have a weird order-dependence on the
  //     replaceability of prepopulated engines, given that we refuse to add
  //     prepopulated engines to the |replaceable_engines| vector.
  //
  // Given the necessary special casing of prepopulated engines, we may consider
  // marking prepopulated engines as NOT safe_for_autoreplace(), but there's a
  // few obstacles to this:
  //  1. Prepopulated engines are not user-created, and therefore meet the
  //     definition of safe_for_autoreplace().
  //  2. If we mark them as NOT safe_for_autoreplace(), we can no longer
  //     distinguish between prepopulated engines that user has edited, vs. not
  //     edited.
  //
  // One more caveat: In 2019, we made prepopulated engines have a
  // deterministically generated Sync GUID in order to prevent duplicate
  // prepopulated engines when two clients start syncing at the same time.
  // When combined with the requirement that we can never fail to add a
  // prepopulated engine, this could leads to two engines having the same GUID.
  //
  // TODO(tommycli): After M89, we need to investigate solving the contradiction
  // above. Most probably: the solution is to stop Syncing prepopulated engines
  // and make the GUIDs actually globally unique again.
  return candidate != best && candidate->safe_for_autoreplace() &&
         candidate->prepopulate_id() == 0 && candidate->starter_pack_id() == 0;
}

bool TemplateURLService::MatchesDefaultSearchProvider(TemplateURL* turl) const {
  DCHECK(turl);
  const TemplateURL* default_provider = GetDefaultSearchProvider();
  if (!default_provider)
    return false;

  return turl->sync_guid() == default_provider->sync_guid();
}

std::unique_ptr<EnterpriseSiteSearchManager>
TemplateURLService::GetEnterpriseSiteSearchManager(PrefService* prefs) {
#if BUILDFLAG(IS_WIN) || BUILDFLAG(IS_MAC) || BUILDFLAG(IS_LINUX) || \
    BUILDFLAG(IS_CHROMEOS)
  return base::FeatureList::IsEnabled(omnibox::kSiteSearchSettingsPolicy)
             ? std::make_unique<EnterpriseSiteSearchManager>(
                   prefs, base::BindRepeating(
                              &TemplateURLService::EnterpriseSiteSearchChanged,
                              base::Unretained(this)))
             : nullptr;
#else
  return nullptr;
#endif
}

void TemplateURLService::LogSiteSearchPolicyConflict(
    const TemplateURLService::OwnedTemplateURLVector&
        policy_site_search_engines) {
  if (policy_site_search_engines.empty()) {
    // No need to record conflict histograms if the SiteSearchSettings policy
    // doesn't create any search engine.
    return;
  }

  bool has_conflict_with_featured = false;
  bool has_conflict_with_non_featured = false;
  for (const auto& policy_turl : policy_site_search_engines) {
    const std::u16string& keyword = policy_turl->keyword();
    CHECK(!keyword.empty());

    const auto match_range = keyword_to_turl_.equal_range(keyword);
    bool conflicts_with_active =
        std::any_of(match_range.first, match_range.second,
                    [](const KeywordToTURL::value_type& entry) {
                      return entry.second->created_by_policy() ==
                                 TemplateURLData::CreatedByPolicy::kNoPolicy &&
                             !entry.second->safe_for_autoreplace();
                    });
    SiteSearchPolicyConflictType type =
        conflicts_with_active
            ? (policy_turl->featured_by_policy()
                   ? SiteSearchPolicyConflictType::kWithFeatured
                   : SiteSearchPolicyConflictType::kWithNonFeatured)
            : SiteSearchPolicyConflictType::kNone;
    base::UmaHistogramEnumeration(kSiteSearchPolicyConflictCountHistogramName,
                                  type);

    has_conflict_with_featured |=
        type == SiteSearchPolicyConflictType::kWithFeatured;
    has_conflict_with_non_featured |=
        type == SiteSearchPolicyConflictType::kWithNonFeatured;
  }

  base::UmaHistogramBoolean(
      kSiteSearchPolicyHasConflictWithFeaturedHistogramName,
      has_conflict_with_featured);
  base::UmaHistogramBoolean(
      kSiteSearchPolicyHasConflictWithNonFeaturedHistogramName,
      has_conflict_with_non_featured);
}
