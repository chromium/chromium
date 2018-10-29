// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/search_engines/template_url_service.h"

#include <algorithm>

#include "base/auto_reset.h"
#include "base/callback.h"
#include "base/debug/crash_logging.h"
#include "base/format_macros.h"
#include "base/guid.h"
#include "base/metrics/histogram_macros.h"
#include "base/stl_util.h"
#include "base/strings/string_split.h"
#include "base/strings/utf_string_conversions.h"
#include "components/pref_registry/pref_registry_syncable.h"
#include "components/prefs/pref_service.h"
#include "components/rappor/rappor_service_impl.h"
#include "components/search_engines/search_engines_pref_names.h"
#include "components/search_engines/search_terms_data.h"
#include "components/search_engines/template_url_prepopulate_data.h"
#include "components/search_engines/template_url_service_client.h"
#include "components/search_engines/template_url_service_observer.h"
#include "components/search_engines/util.h"
#include "components/sync/model/sync_change_processor.h"
#include "components/sync/model/sync_error_factory.h"
#include "components/sync/protocol/search_engine_specifics.pb.h"
#include "components/sync/protocol/sync.pb.h"
#include "components/url_formatter/url_fixer.h"
#include "components/variations/variations_associated_data.h"
#include "net/base/registry_controlled_domains/registry_controlled_domain.h"
#include "url/gurl.h"

typedef SearchHostToURLsMap::TemplateURLSet TemplateURLSet;
typedef TemplateURLService::SyncDataMap SyncDataMap;

namespace {

const char kDeleteSyncedEngineHistogramName[] =
    "Search.DeleteSyncedSearchEngine";

// Values for an enumerated histogram used to track whenever an ACTION_DELETE is
// sent to the server for search engines.
enum DeleteSyncedSearchEngineEvent {
  DELETE_ENGINE_USER_ACTION,
  DELETE_ENGINE_PRE_SYNC,
  DELETE_ENGINE_EMPTY_FIELD,
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
  return base::ContainsKey(sync_data, turl->sync_guid());
}

// Log the number of instances of a keyword that exist, with zero or more
// underscores, which could occur as the result of conflict resolution.
void LogDuplicatesHistogram(
    const TemplateURLService::TemplateURLVector& template_urls) {
  std::map<base::string16, int> duplicates;
  for (auto it = template_urls.begin(); it != template_urls.end(); ++it) {
    base::string16 keyword = (*it)->keyword();
    base::TrimString(keyword, base::ASCIIToUTF16("_"), &keyword);
    duplicates[keyword]++;
  }

  // Count the keywords with duplicates.
  int num_dupes = 0;
  for (std::map<base::string16, int>::const_iterator it = duplicates.begin();
       it != duplicates.end(); ++it) {
    if (it->second > 1)
      num_dupes++;
  }

  UMA_HISTOGRAM_COUNTS_100("Search.SearchEngineDuplicateCounts", num_dupes);
}

// Returns the length of the registry portion of a hostname.  For example,
// www.google.co.uk will return 5 (the length of co.uk).
size_t GetRegistryLength(const base::string16& host) {
  return net::registry_controlled_domains::PermissiveGetHostRegistryLength(
      host, net::registry_controlled_domains::EXCLUDE_UNKNOWN_REGISTRIES,
      net::registry_controlled_domains::EXCLUDE_PRIVATE_REGISTRIES);
}

// Returns the domain name (including registry) of a hostname.  For example,
// www.google.co.uk will return google.co.uk.
base::string16 GetDomainAndRegistry(const base::string16& host) {
  return base::UTF8ToUTF16(
      net::registry_controlled_domains::GetDomainAndRegistry(
          base::UTF16ToUTF8(host),
          net::registry_controlled_domains::EXCLUDE_PRIVATE_REGISTRIES));
}

// For keywords that look like hostnames, returns whether KeywordProvider
// should require users to type a prefix of the hostname to match against
// them, rather than just the domain name portion. In other words, returns
// whether the prefix before the domain name should be considered important
// for matching purposes. Returns true if the experiment isn't active.
bool OmniboxFieldTrialKeywordRequiresRegistry() {
  // This would normally be
  // bool OmniboxFieldTrial::KeywordRequiresRegistry()
  // but that would create a dependency cycle since omnibox depends on
  // search_engines (and search -> search_engines)
  constexpr char kBundledExperimentFieldTrialName[] =
      "OmniboxBundledExperimentV1";
  constexpr char kKeywordRequiresRegistryRule[] = "KeywordRequiresRegistry";
  const std::string value = variations::GetVariationParamValue(
      kBundledExperimentFieldTrialName, kKeywordRequiresRegistryRule);
  return value.empty() || (value == "true");
}

// Returns the length of the important part of the |keyword|, assumed to be
// associated with the TemplateURL.  For instance, for the keyword
// google.co.uk, this can return 6 (the length of "google").
size_t GetMeaningfulKeywordLength(const base::string16& keyword,
                                  const TemplateURL* turl) {
  // Using Omnibox from here is a layer violation and should be fixed.
  if (OmniboxFieldTrialKeywordRequiresRegistry())
    return keyword.length();
  const size_t registry_length = GetRegistryLength(keyword);
  if (registry_length == std::string::npos)
    return keyword.length();
  DCHECK_LT(registry_length, keyword.length());
  // The meaningful keyword length is the length of any portion before the
  // registry ("co.uk") and its preceding dot.
  return keyword.length() - (registry_length ? (registry_length + 1) : 0);
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
  bool operator()(
      const KeywordToTURLAndMeaningfulLength::value_type& elem1,
      const KeywordToTURLAndMeaningfulLength::value_type& elem2) const {
    return (elem1.second.first == nullptr)
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
  TemplateURLService* service_;

  DISALLOW_COPY_AND_ASSIGN(Scoper);
};

// TemplateURLService ---------------------------------------------------------

TemplateURLService::TemplateURLService(
    PrefService* prefs,
    std::unique_ptr<SearchTermsData> search_terms_data,
    const scoped_refptr<KeywordWebDataService>& web_data_service,
    std::unique_ptr<TemplateURLServiceClient> client,
    GoogleURLTracker* google_url_tracker,
    rappor::RapporServiceImpl* rappor_service,
    const base::Closure& dsp_change_callback)
    : prefs_(prefs),
      search_terms_data_(std::move(search_terms_data)),
      web_data_service_(web_data_service),
      client_(std::move(client)),
      google_url_tracker_(google_url_tracker),
      rappor_service_(rappor_service),
      dsp_change_callback_(dsp_change_callback),
      default_search_manager_(
          prefs_,
          base::BindRepeating(&TemplateURLService::ApplyDefaultSearchChange,
                              base::Unretained(this))) {
  DCHECK(search_terms_data_);
  Init(nullptr, 0);
}

TemplateURLService::TemplateURLService(const Initializer* initializers,
                                       const int count)
    : default_search_manager_(
          prefs_,
          base::BindRepeating(&TemplateURLService::ApplyDefaultSearchChange,
                              base::Unretained(this))) {
  Init(initializers, count);
}

TemplateURLService::~TemplateURLService() {
  // |web_data_service_| should be deleted during Shutdown().
  DCHECK(!web_data_service_);
}

// static
void TemplateURLService::RegisterProfilePrefs(
    user_prefs::PrefRegistrySyncable* registry) {
#if defined(OS_IOS) || defined(OS_ANDROID)
  uint32_t flags = PrefRegistry::NO_REGISTRATION_FLAGS;
#else
  uint32_t flags = user_prefs::PrefRegistrySyncable::SYNCABLE_PREF;
#endif
  registry->RegisterStringPref(prefs::kSyncedDefaultSearchProviderGUID,
                               std::string(),
                               flags);
  registry->RegisterBooleanPref(prefs::kDefaultSearchProviderEnabled, true);
}

bool TemplateURLService::CanAddAutogeneratedKeyword(
    const base::string16& keyword,
    const GURL& url,
    const TemplateURL** template_url_to_replace) {
  DCHECK(!keyword.empty());  // This should only be called for non-empty
                             // keywords. If we need to support empty kewords
                             // the code needs to change slightly.
  const TemplateURL* existing_url = GetTemplateURLForKeyword(keyword);
  if (template_url_to_replace)
    *template_url_to_replace = existing_url;
  if (existing_url) {
    // We already have a TemplateURL for this keyword. Only allow it to be
    // replaced if the TemplateURL can be replaced.
    return CanReplace(existing_url);
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

bool TemplateURLService::IsPrepopulatedOrCreatedByPolicy(
    const TemplateURL* t_url) const {
  return (t_url->prepopulate_id() > 0 || t_url->created_by_policy()) &&
      t_url->SupportsReplacement(search_terms_data());
}

bool TemplateURLService::ShowInDefaultList(const TemplateURL* t_url) const {
  return t_url == default_search_provider_ ||
      IsPrepopulatedOrCreatedByPolicy(t_url);
}

void TemplateURLService::AddMatchingKeywords(
    const base::string16& prefix,
    bool supports_replacement_only,
    TURLsAndMeaningfulLengths* matches) {
  AddMatchingKeywordsHelper(
      keyword_to_turl_and_length_, prefix, supports_replacement_only, matches);
}

void TemplateURLService::AddMatchingDomainKeywords(
    const base::string16& prefix,
    bool supports_replacement_only,
    TURLsAndMeaningfulLengths* matches) {
  AddMatchingKeywordsHelper(
      keyword_domain_to_turl_and_length_, prefix, supports_replacement_only,
      matches);
}

TemplateURL* TemplateURLService::GetTemplateURLForKeyword(
    const base::string16& keyword) {
  return const_cast<TemplateURL*>(
      static_cast<const TemplateURLService*>(this)->
          GetTemplateURLForKeyword(keyword));
}

const TemplateURL* TemplateURLService::GetTemplateURLForKeyword(
    const base::string16& keyword) const {
  auto elem(keyword_to_turl_and_length_.find(keyword));
  if (elem != keyword_to_turl_and_length_.end())
    return elem->second.first;
  return (!loaded_ && initial_default_search_provider_ &&
          (initial_default_search_provider_->keyword() == keyword))
             ? initial_default_search_provider_.get()
             : nullptr;
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
  if (elem != guid_to_turl_.end())
    return elem->second;
  return (!loaded_ && initial_default_search_provider_ &&
          (initial_default_search_provider_->sync_guid() == sync_guid))
             ? initial_default_search_provider_.get()
             : nullptr;
}

TemplateURL* TemplateURLService::GetTemplateURLForHost(
    const std::string& host) {
  return const_cast<TemplateURL*>(
      static_cast<const TemplateURLService*>(this)->
          GetTemplateURLForHost(host));
}

const TemplateURL* TemplateURLService::GetTemplateURLForHost(
    const std::string& host) const {
  if (loaded_)
    return provider_map_->GetTemplateURLForHost(host);
  TemplateURL* initial_dsp = initial_default_search_provider_.get();
  return (initial_dsp &&
          (initial_dsp->GenerateSearchURL(search_terms_data()).host_piece() ==
           host))
             ? initial_dsp
             : nullptr;
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
    const base::string16& short_name,
    const base::string16& keyword,
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
  DCHECK_NE(template_url, default_search_provider_);

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
    if (web_data_service_)
      web_data_service_->RemoveKeyword(template_url->id());

    // Inform sync of the deletion.
    ProcessTemplateURLChange(FROM_HERE, template_url,
                             syncer::SyncChange::ACTION_DELETE);

    // The default search engine can't be deleted. But the user defined DSE can
    // be hidden by an extension or policy and then deleted. Clean up the user
    // prefs then.
    if (prefs_ &&
        (template_url->sync_guid() ==
         prefs_->GetString(prefs::kSyncedDefaultSearchProviderGUID))) {
      prefs_->SetString(prefs::kSyncedDefaultSearchProviderGUID, std::string());
    }

    UMA_HISTOGRAM_ENUMERATION(kDeleteSyncedEngineHistogramName,
                              DELETE_ENGINE_USER_ACTION, DELETE_ENGINE_MAX);
  }

  if (loaded_ && client_)
    client_->DeleteAllSearchTermsForKeyword(template_url->id());
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

void TemplateURLService::RemoveAutoGeneratedSince(base::Time created_after) {
  RemoveAutoGeneratedBetween(created_after, base::Time());
}

void TemplateURLService::RemoveAutoGeneratedBetween(base::Time created_after,
                                                    base::Time created_before) {
  RemoveAutoGeneratedForUrlsBetween(base::Callback<bool(const GURL&)>(),
                                    created_after, created_before);
}

void TemplateURLService::RemoveAutoGeneratedForUrlsBetween(
    const base::Callback<bool(const GURL&)>& url_filter,
    base::Time created_after,
    base::Time created_before) {
  Scoper scoper(this);

  for (size_t i = 0; i < template_urls_.size();) {
    if (template_urls_[i]->date_created() >= created_after &&
        (created_before.is_null() ||
         template_urls_[i]->date_created() < created_before) &&
        CanReplace(template_urls_[i].get()) &&
        (url_filter.is_null() ||
         url_filter.Run(
             template_urls_[i]->GenerateSearchURL(search_terms_data())))) {
      Remove(template_urls_[i].get());
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
                                          const base::string16& title,
                                          const base::string16& keyword,
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
  Update(url, TemplateURL(data));
}

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
  return
      ((default_search_provider_source_ == DefaultSearchManager::FROM_USER) ||
       (default_search_provider_source_ ==
        DefaultSearchManager::FROM_FALLBACK)) &&
      (url != GetDefaultSearchProvider()) &&
      url->url_ref().SupportsReplacement(search_terms_data()) &&
      (url->type() == TemplateURL::NORMAL);
}

void TemplateURLService::SetUserSelectedDefaultSearchProvider(
    TemplateURL* url) {
  // Omnibox keywords cannot be made default. Extension-controlled search
  // engines can be made default only by the extension itself because they
  // aren't persisted.
  DCHECK(!url || !IsCreatedByExtension(url));
  if (load_failed_) {
    // Skip the DefaultSearchManager, which will persist to user preferences.
    if ((default_search_provider_source_ == DefaultSearchManager::FROM_USER) ||
        (default_search_provider_source_ ==
         DefaultSearchManager::FROM_FALLBACK)) {
      ApplyDefaultSearchChange(url ? &url->data() : nullptr,
                               DefaultSearchManager::FROM_USER);
    }
  } else {
    // We rely on the DefaultSearchManager to call ApplyDefaultSearchChange if,
    // in fact, the effective DSE changes.
    if (url)
      default_search_manager_.SetUserSelectedDefaultSearchEngine(url->data());
    else
      default_search_manager_.ClearUserSelectedDefaultSearchEngine();
  }
}

const TemplateURL* TemplateURLService::GetDefaultSearchProvider() const {
  return loaded_ ? default_search_provider_
                 : initial_default_search_provider_.get();
}

bool TemplateURLService::IsSearchResultsPageFromDefaultSearchProvider(
    const GURL& url) const {
  const TemplateURL* default_provider = GetDefaultSearchProvider();
  return default_provider &&
      default_provider->IsSearchURL(url, search_terms_data());
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
      TemplateURLPrepopulateData::GetPrepopulatedEngines(prefs_, nullptr);
  DCHECK(!prepopulated_urls.empty());
  ActionsFromPrepopulateData actions(CreateActionsFromCurrentPrepopulateData(
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
      // OnSyncedDefaultSearchProviderGUIDChanged() to set it as the new
      // user-selected engine.
      prefs_->SetString(prefs::kSyncedDefaultSearchProviderGUID,
                        fallback_engine->sync_guid());
    }
    RequestGoogleURLTrackerServerCheckIfNecessary();
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

std::unique_ptr<TemplateURLService::Subscription>
TemplateURLService::RegisterOnLoadedCallback(
    const base::RepeatingClosure& callback) {
  return loaded_ ? std::unique_ptr<TemplateURLService::Subscription>()
                 : on_loaded_callbacks_.Add(callback);
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
  int new_resource_keyword_version = 0;
  {
    GetSearchProvidersUsingKeywordResult(
        *result, web_data_service_.get(), prefs_, template_urls.get(),
        (default_search_provider_source_ == DefaultSearchManager::FROM_USER)
            ? initial_default_search_provider_.get()
            : nullptr,
        search_terms_data(), &new_resource_keyword_version, &pre_sync_deletes_);
  }

  Scoper scoper(this);

  {
    PatchMissingSyncGUIDs(template_urls.get());
    SetTemplateURLs(std::move(template_urls));

    // This initializes provider_map_ which should be done before
    // calling UpdateKeywordSearchTermsForURL.
    ChangeToLoadedState();

    // Index any visits that occurred before we finished loading.
    for (size_t i = 0; i < visits_to_add_.size(); ++i)
      UpdateKeywordSearchTermsForURL(visits_to_add_[i]);
    visits_to_add_.clear();

    if (new_resource_keyword_version)
      web_data_service_->SetBuiltinKeywordVersion(new_resource_keyword_version);
  }

  if (default_search_provider_) {
    UMA_HISTOGRAM_ENUMERATION(
        "Search.DefaultSearchProviderType",
        default_search_provider_->GetEngineType(search_terms_data()),
        SEARCH_ENGINE_MAX);

    if (rappor_service_) {
      rappor_service_->RecordSampleString(
          "Search.DefaultSearchProvider", rappor::ETLD_PLUS_ONE_RAPPOR_TYPE,
          net::registry_controlled_domains::GetDomainAndRegistry(
              default_search_provider_->url_ref().GetHost(search_terms_data()),
              net::registry_controlled_domains::INCLUDE_PRIVATE_REGISTRIES));
    }
  }
}

base::string16 TemplateURLService::GetKeywordShortName(
    const base::string16& keyword,
    bool* is_omnibox_api_extension_keyword) const {
  const TemplateURL* template_url = GetTemplateURLForKeyword(keyword);

  // TODO(sky): Once LocationBarView adds a listener to the TemplateURLService
  // to track changes to the model, this should become a DCHECK.
  if (template_url) {
    *is_omnibox_api_extension_keyword =
        template_url->type() == TemplateURL::OMNIBOX_API_EXTENSION;
    return template_url->AdjustedShortNameForLocaleDirection();
  }
  *is_omnibox_api_extension_keyword = false;
  return base::string16();
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

syncer::SyncDataList TemplateURLService::GetAllSyncData(
    syncer::ModelType type) const {
  DCHECK_EQ(syncer::SEARCH_ENGINES, type);

  syncer::SyncDataList current_data;
  for (const auto& turl : template_urls_) {
    // We don't sync keywords managed by policy.
    if (turl->created_by_policy())
      continue;
    // We don't sync extension-controlled search engines.
    if (turl->type() != TemplateURL::NORMAL)
      continue;
    current_data.push_back(CreateSyncDataFromTemplateURL(*turl));
  }

  return current_data;
}

syncer::SyncError TemplateURLService::ProcessSyncChanges(
    const base::Location& from_here,
    const syncer::SyncChangeList& change_list) {
  if (!models_associated_) {
    syncer::SyncError error(FROM_HERE,
                            syncer::SyncError::DATATYPE_ERROR,
                            "Models not yet associated.",
                            syncer::SEARCH_ENGINES);
    return error;
  }
  DCHECK(loaded_);

  base::AutoReset<bool> processing_changes(&processing_syncer_changes_, true);

  Scoper scoper(this);

  // We've started syncing, so set our origin member to the base Sync value.
  // As we move through Sync Code, we may set this to increasingly specific
  // origins so we can tell what exactly caused a DSP change.
  base::AutoReset<DefaultSearchChangeOrigin> change_origin(&dsp_change_origin_,
      DSP_CHANGE_SYNC_UNINTENTIONAL);

  syncer::SyncChangeList new_changes;
  syncer::SyncError error;
  for (auto iter = change_list.begin(); iter != change_list.end(); ++iter) {
    DCHECK_EQ(syncer::SEARCH_ENGINES, iter->sync_data().GetDataType());

    std::string guid =
        iter->sync_data().GetSpecifics().search_engine().sync_guid();
    TemplateURL* existing_turl = GetTemplateURLForGUID(guid);
    std::unique_ptr<TemplateURL> turl(
        CreateTemplateURLFromTemplateURLAndSyncData(
            client_.get(), prefs_, search_terms_data(), existing_turl,
            iter->sync_data(), &new_changes));
    if (!turl)
      continue;

    // Explicitly don't check for conflicts against extension keywords; in this
    // case the functions which modify the keyword map know how to handle the
    // conflicts.
    // TODO(mpcomplete): If we allow editing extension keywords, then those will
    // need to undergo conflict resolution.
    TemplateURL* existing_keyword_turl =
        FindNonExtensionTemplateURLForKeyword(turl->keyword());
    if (iter->change_type() == syncer::SyncChange::ACTION_DELETE) {
      if (!existing_turl) {
        error = sync_error_factory_->CreateAndUploadError(
            FROM_HERE,
            "ProcessSyncChanges failed on ChangeType ACTION_DELETE");
        continue;
      }
      if (existing_turl == GetDefaultSearchProvider()) {
        // The only way Sync can attempt to delete the default search provider
        // is if we had changed the kSyncedDefaultSearchProviderGUID
        // preference, but perhaps it has not yet been received. To avoid
        // situations where this has come in erroneously, we will un-delete
        // the current default search from the Sync data. If the pref really
        // does arrive later, then default search will change to the correct
        // entry, but we'll have this extra entry sitting around. The result is
        // not ideal, but it prevents a far more severe bug where the default is
        // unexpectedly swapped to something else. The user can safely delete
        // the extra entry again later, if they choose. Most users who do not
        // look at the search engines UI will not notice this.
        // Note that we append a special character to the end of the keyword in
        // an attempt to avoid a ping-poinging situation where receiving clients
        // may try to continually delete the resurrected entry.
        base::string16 updated_keyword = UniquifyKeyword(*existing_turl, true);
        TemplateURLData data(existing_turl->data());
        data.SetKeyword(updated_keyword);
        TemplateURL new_turl(data);
        Update(existing_turl, new_turl);

        syncer::SyncData sync_data = CreateSyncDataFromTemplateURL(new_turl);
        new_changes.push_back(syncer::SyncChange(FROM_HERE,
                                                 syncer::SyncChange::ACTION_ADD,
                                                 sync_data));
        // Ignore the delete attempt. This means we never end up resetting the
        // default search provider due to an ACTION_DELETE from sync.
        continue;
      }

      Remove(existing_turl);
    } else if (iter->change_type() == syncer::SyncChange::ACTION_ADD) {
      if (existing_turl) {
        error = sync_error_factory_->CreateAndUploadError(
            FROM_HERE,
            "ProcessSyncChanges failed on ChangeType ACTION_ADD");
        continue;
      }
      const std::string guid = turl->sync_guid();
      if (existing_keyword_turl) {
        // Resolve any conflicts so we can safely add the new entry.
        ResolveSyncKeywordConflict(turl.get(), existing_keyword_turl,
                                   &new_changes);
      }
      base::AutoReset<DefaultSearchChangeOrigin> change_origin(
          &dsp_change_origin_, DSP_CHANGE_SYNC_ADD);
      // Force the local ID to kInvalidTemplateURLID so we can add it.
      TemplateURLData data(turl->data());
      data.id = kInvalidTemplateURLID;
      std::unique_ptr<TemplateURL> added_ptr =
          std::make_unique<TemplateURL>(data);
      TemplateURL* added = added_ptr.get();
      if (Add(std::move(added_ptr)))
        MaybeUpdateDSEViaPrefs(added);
    } else if (iter->change_type() == syncer::SyncChange::ACTION_UPDATE) {
      if (!existing_turl) {
        error = sync_error_factory_->CreateAndUploadError(
            FROM_HERE,
            "ProcessSyncChanges failed on ChangeType ACTION_UPDATE");
        continue;
      }
      if (existing_keyword_turl && (existing_keyword_turl != existing_turl)) {
        // Resolve any conflicts with other entries so we can safely update the
        // keyword.
        ResolveSyncKeywordConflict(turl.get(), existing_keyword_turl,
                                   &new_changes);
      }
      if (Update(existing_turl, *turl))
        MaybeUpdateDSEViaPrefs(existing_turl);
    } else {
      // We've unexpectedly received an ACTION_INVALID.
      error = sync_error_factory_->CreateAndUploadError(
          FROM_HERE,
          "ProcessSyncChanges received an ACTION_INVALID");
    }
  }


  // If something went wrong, we want to prematurely exit to avoid pushing
  // inconsistent data to Sync. We return the last error we received.
  if (error.IsSet())
    return error;

  error = sync_processor_->ProcessSyncChanges(from_here, new_changes);

  return error;
}

syncer::SyncMergeResult TemplateURLService::MergeDataAndStartSyncing(
    syncer::ModelType type,
    const syncer::SyncDataList& initial_sync_data,
    std::unique_ptr<syncer::SyncChangeProcessor> sync_processor,
    std::unique_ptr<syncer::SyncErrorFactory> sync_error_factory) {
  DCHECK(loaded_);
  DCHECK_EQ(type, syncer::SEARCH_ENGINES);
  DCHECK(!sync_processor_);
  DCHECK(sync_processor);
  DCHECK(sync_error_factory);
  syncer::SyncMergeResult merge_result(type);

  // Disable sync if we failed to load.
  if (load_failed_) {
    merge_result.set_error(syncer::SyncError(
        FROM_HERE, syncer::SyncError::DATATYPE_ERROR,
        "Local database load failed.", syncer::SEARCH_ENGINES));
    return merge_result;
  }

  sync_processor_ = std::move(sync_processor);
  sync_error_factory_ = std::move(sync_error_factory);

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

  merge_result.set_num_items_before_association(local_data_map.size());
  for (SyncDataMap::const_iterator iter = sync_data_map.begin();
      iter != sync_data_map.end(); ++iter) {
    TemplateURL* local_turl = GetTemplateURLForGUID(iter->first);
    std::unique_ptr<TemplateURL> sync_turl(
        CreateTemplateURLFromTemplateURLAndSyncData(
            client_.get(), prefs_, search_terms_data(), local_turl,
            iter->second, &new_changes));
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
        merge_result.set_num_items_modified(
            merge_result.num_items_modified() + 1);
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
                             &local_data_map, &merge_result);
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
  merge_result.set_num_items_after_association(
      GetAllSyncData(syncer::SEARCH_ENGINES).size());
  merge_result.set_error(
      sync_processor_->ProcessSyncChanges(FROM_HERE, new_changes));
  if (merge_result.error().IsSet())
    return merge_result;

  // The ACTION_DELETEs from this set are processed. Empty it so we don't try to
  // reuse them on the next call to MergeDataAndStartSyncing.
  pre_sync_deletes_.clear();

  models_associated_ = true;
  return merge_result;
}

void TemplateURLService::StopSyncing(syncer::ModelType type) {
  DCHECK_EQ(type, syncer::SEARCH_ENGINES);
  models_associated_ = false;
  sync_processor_.reset();
  sync_error_factory_.reset();
}

void TemplateURLService::ProcessTemplateURLChange(
    const base::Location& from_here,
    const TemplateURL* turl,
    syncer::SyncChange::SyncChangeType type) {
  DCHECK_NE(type, syncer::SyncChange::ACTION_INVALID);
  DCHECK(turl);

  if (!models_associated_)
    return;  // Not syncing.

  if (processing_syncer_changes_)
    return;  // These are changes originating from us. Ignore.

  // Avoid syncing keywords managed by policy.
  if (turl->created_by_policy())
    return;

  // Avoid syncing extension-controlled search engines.
  if (turl->type() == TemplateURL::NORMAL_CONTROLLED_BY_EXTENSION)
    return;

  syncer::SyncData sync_data = CreateSyncDataFromTemplateURL(*turl);
  syncer::SyncChangeList changes = {
      syncer::SyncChange(from_here, type, sync_data)};
  sync_processor_->ProcessSyncChanges(FROM_HERE, changes);
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

  return syncer::SyncData::CreateLocalData(se_specifics->sync_guid(),
                                           se_specifics->keyword(),
                                           specifics);
}

// static
std::unique_ptr<TemplateURL>
TemplateURLService::CreateTemplateURLFromTemplateURLAndSyncData(
    TemplateURLServiceClient* client,
    PrefService* prefs,
    const SearchTermsData& search_terms_data,
    const TemplateURL* existing_turl,
    const syncer::SyncData& sync_data,
    syncer::SyncChangeList* change_list) {
  DCHECK(change_list);

  sync_pb::SearchEngineSpecifics specifics =
      sync_data.GetSpecifics().search_engine();

  // Past bugs might have caused either of these fields to be empty.  Just
  // delete this data off the server.
  if (specifics.url().empty() || specifics.sync_guid().empty()) {
    change_list->push_back(
        syncer::SyncChange(FROM_HERE,
                           syncer::SyncChange::ACTION_DELETE,
                           sync_data));
    UMA_HISTOGRAM_ENUMERATION(kDeleteSyncedEngineHistogramName,
        DELETE_ENGINE_EMPTY_FIELD, DELETE_ENGINE_MAX);
    return nullptr;
  }

  TemplateURLData data(existing_turl ?
      existing_turl->data() : TemplateURLData());
  data.SetShortName(base::UTF8ToUTF16(specifics.short_name()));
  data.originating_url = GURL(specifics.originating_url());
  base::string16 keyword(base::UTF8ToUTF16(specifics.keyword()));
  // NOTE: Once this code has shipped in a couple of stable releases, we can
  // probably remove the migration portion, comment out the
  // "autogenerate_keyword" field entirely in the .proto file, and fold the
  // empty keyword case into the "delete data" block above.
  bool reset_keyword =
      specifics.autogenerate_keyword() || specifics.keyword().empty();
  if (reset_keyword)
    keyword = base::ASCIIToUTF16("dummy");  // Will be replaced below.
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

  std::unique_ptr<TemplateURL> turl(new TemplateURL(data));
  // If this TemplateURL matches a built-in prepopulated template URL, it's
  // possible that sync is trying to modify fields that should not be touched.
  // Revert these fields to the built-in values.
  UpdateTemplateURLIfPrepopulated(turl.get(), prefs);

  DCHECK_EQ(TemplateURL::NORMAL, turl->type());
  if (reset_keyword || deduped) {
    if (reset_keyword)
      turl->ResetKeywordIfNecessary(search_terms_data, true);
    syncer::SyncData sync_data = CreateSyncDataFromTemplateURL(*turl);
    change_list->push_back(syncer::SyncChange(FROM_HERE,
                                              syncer::SyncChange::ACTION_UPDATE,
                                              sync_data));
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

void TemplateURLService::Init(const Initializer* initializers,
                              int num_initializers) {
  if (client_)
    client_->SetOwner(this);

  // GoogleURLTracker is not created in tests.
  if (google_url_tracker_) {
    google_url_updated_subscription_ =
        google_url_tracker_->RegisterCallback(base::Bind(
            &TemplateURLService::GoogleBaseURLChanged, base::Unretained(this)));
  }

  if (prefs_) {
    pref_change_registrar_.Init(prefs_);
    pref_change_registrar_.Add(
        prefs::kSyncedDefaultSearchProviderGUID,
        base::Bind(
            &TemplateURLService::OnSyncedDefaultSearchProviderGUIDChanged,
            base::Unretained(this)));
  }

  DefaultSearchManager::Source source = DefaultSearchManager::FROM_USER;
  const TemplateURLData* dse =
      default_search_manager_.GetDefaultSearchEngine(&source);

  Scoper scoper(this);

  ApplyDefaultSearchChange(dse, source);

  if (num_initializers > 0) {
    // This path is only hit by test code and is used to simulate a loaded
    // TemplateURLService.
    ChangeToLoadedState();

    // Add specific initializers, if any.
    for (int i(0); i < num_initializers; ++i) {
      DCHECK(initializers[i].keyword);
      DCHECK(initializers[i].url);
      DCHECK(initializers[i].content);

      // TemplateURLService ends up owning the TemplateURL, don't try and free
      // it.
      TemplateURLData data;
      data.SetShortName(base::UTF8ToUTF16(initializers[i].content));
      data.SetKeyword(base::UTF8ToUTF16(initializers[i].keyword));
      data.SetURL(initializers[i].url);
      Add(std::make_unique<TemplateURL>(data));

      // Set the first provided identifier to be the default.
      if (i == 0)
        default_search_manager_.SetUserSelectedDefaultSearchEngine(data);
    }
  }

  // Request a server check for the correct Google URL if Google is the
  // default search engine.
  RequestGoogleURLTrackerServerCheckIfNecessary();
}

TemplateURL* TemplateURLService::BestEngineForKeyword(TemplateURL* engine1,
                                                      TemplateURL* engine2) {
  DCHECK(engine1);
  DCHECK(engine2);
  DCHECK_EQ(engine1->keyword(), engine2->keyword());

  // We should only have overlapping keywords when at least one comes from
  // an extension.
  DCHECK(IsCreatedByExtension(engine1) || IsCreatedByExtension(engine2));

  // TODO(a-v-y) Remove following code for non extension engines when reasons
  // for crash https://bugs.chromium.org/p/chromium/issues/detail?id=697745
  // become clear.
  if (!IsCreatedByExtension(engine1) && !IsCreatedByExtension(engine2))
    return CanReplace(engine1) ? engine2 : engine1;

  if (engine2->type() == engine1->type()) {
    return engine1->extension_info_->install_time >
                   engine2->extension_info_->install_time
               ? engine1
               : engine2;
  }
  if (engine2->type() == TemplateURL::NORMAL_CONTROLLED_BY_EXTENSION) {
    return engine1->type() == TemplateURL::OMNIBOX_API_EXTENSION ? engine1
                                                                 : engine2;
  }
  return engine2->type() == TemplateURL::OMNIBOX_API_EXTENSION ? engine2
                                                               : engine1;
}

void TemplateURLService::RemoveFromMaps(const TemplateURL* template_url) {
  const base::string16& keyword = template_url->keyword();
  DCHECK_NE(0U, keyword_to_turl_and_length_.count(keyword));
  if (keyword_to_turl_and_length_[keyword].first == template_url) {
    // We need to check whether the keyword can now be provided by another
    // TemplateURL. See the comments for BestEngineForKeyword() for more
    // information on extension keywords and how they can coexist with
    // non-extension keywords.
    TemplateURL* best_fallback = nullptr;
    for (const auto& turl : template_urls_) {
      if ((turl.get() != template_url) && (turl->keyword() == keyword)) {
        if (best_fallback)
          best_fallback = BestEngineForKeyword(best_fallback, turl.get());
        else
          best_fallback = turl.get();
      }
    }
    RemoveFromDomainMap(template_url);
    if (best_fallback) {
      AddToMap(best_fallback);
      AddToDomainMap(best_fallback);
    } else {
      keyword_to_turl_and_length_.erase(keyword);
    }
  }

  if (template_url->type() == TemplateURL::OMNIBOX_API_EXTENSION)
    return;

  if (!template_url->sync_guid().empty())
    guid_to_turl_.erase(template_url->sync_guid());
  // |provider_map_| is only initialized after loading has completed.
  if (loaded_) {
    provider_map_->Remove(template_url);
  }
}

void TemplateURLService::AddToMaps(TemplateURL* template_url) {
  bool template_url_is_omnibox_api =
      template_url->type() == TemplateURL::OMNIBOX_API_EXTENSION;
  const base::string16& keyword = template_url->keyword();
  KeywordToTURLAndMeaningfulLength::const_iterator i =
      keyword_to_turl_and_length_.find(keyword);
  if (i == keyword_to_turl_and_length_.end()) {
    AddToMap(template_url);
    AddToDomainMap(template_url);
  } else {
    TemplateURL* existing_url = i->second.first;
    DCHECK_NE(existing_url, template_url);
    if (BestEngineForKeyword(existing_url, template_url) != existing_url) {
      RemoveFromDomainMap(existing_url);
      AddToMap(template_url);
      AddToDomainMap(template_url);
    }
  }

  if (template_url_is_omnibox_api)
    return;

  if (!template_url->sync_guid().empty())
    guid_to_turl_[template_url->sync_guid()] = template_url;
  // |provider_map_| is only initialized after loading has completed.
  if (loaded_)
    provider_map_->Add(template_url, search_terms_data());
}

void TemplateURLService::RemoveFromDomainMap(const TemplateURL* template_url) {
  const base::string16 domain = GetDomainAndRegistry(template_url->keyword());
  if (domain.empty())
    return;

  const auto match_range(
      keyword_domain_to_turl_and_length_.equal_range(domain));
  for (auto it(match_range.first); it != match_range.second; ) {
    if (it->second.first == template_url)
      it = keyword_domain_to_turl_and_length_.erase(it);
    else
      ++it;
  }
}

void TemplateURLService::AddToDomainMap(TemplateURL* template_url) {
  const base::string16 domain = GetDomainAndRegistry(template_url->keyword());
  // Only bother adding an entry to the domain map if its key in the domain
  // map would be different from the key in the regular map.
  if (domain != template_url->keyword()) {
    keyword_domain_to_turl_and_length_.insert(std::make_pair(
        domain,
        TURLAndMeaningfulLength(
            template_url, GetMeaningfulKeywordLength(domain, template_url))));
  }
}

void TemplateURLService::AddToMap(TemplateURL* template_url) {
  const base::string16& keyword = template_url->keyword();
  keyword_to_turl_and_length_[keyword] =
      TURLAndMeaningfulLength(
          template_url, GetMeaningfulKeywordLength(keyword, template_url));
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
      initial_default_search_provider_
          ? &initial_default_search_provider_->data()
          : nullptr,
      default_search_provider_source_);
  initial_default_search_provider_.reset();

  on_loaded_callbacks_.Notify();
}

bool TemplateURLService::CanAddAutogeneratedKeywordForHost(
    const std::string& host) const {
  const TemplateURLSet* urls = provider_map_->GetURLsForHost(host);
  if (!urls)
    return true;

  return std::all_of(urls->begin(), urls->end(), [](const TemplateURL* turl) {
    return turl->safe_for_autoreplace();
  });
}

bool TemplateURLService::CanReplace(const TemplateURL* t_url) const {
  return !ShowInDefaultList(t_url) && t_url->safe_for_autoreplace();
}

TemplateURL* TemplateURLService::FindNonExtensionTemplateURLForKeyword(
    const base::string16& keyword) {
  TemplateURL* keyword_turl = GetTemplateURLForKeyword(keyword);
  if (!keyword_turl || (keyword_turl->type() == TemplateURL::NORMAL))
    return keyword_turl;
  // The extension keyword in the model may be hiding a replaceable
  // non-extension keyword.  Look for it.
  for (const auto& turl : template_urls_) {
    if ((turl->type() == TemplateURL::NORMAL) &&
        (turl->keyword() == keyword))
      return turl.get();
  }
  return nullptr;
}

bool TemplateURLService::Update(TemplateURL* existing_turl,
                                const TemplateURL& new_values) {
  DCHECK(existing_turl);
  DCHECK(!IsCreatedByExtension(existing_turl));
  if (!Contains(&template_urls_, existing_turl))
    return false;

  Scoper scoper(this);
  model_mutated_notification_pending_ = true;

  base::string16 old_keyword = existing_turl->keyword();
  TemplateURLID previous_id = existing_turl->id();
  RemoveFromMaps(existing_turl);

  // Check if new keyword conflicts with another normal engine.
  // This is possible when autogeneration of the keyword for a Google default
  // search provider at load time causes it to conflict with an existing
  // keyword. In this case we delete the existing keyword if it's replaceable,
  // or else undo the change in keyword for |existing_turl|.
  // Conflicts with extension engines are handled in AddToMaps/RemoveFromMaps
  // functions.
  // Search for conflicting keyword turl before updating values of
  // existing_turl.
  const TemplateURL* conflicting_keyword_turl =
      FindNonExtensionTemplateURLForKeyword(new_values.keyword());

  bool keep_old_keyword = false;
  if (conflicting_keyword_turl && conflicting_keyword_turl != existing_turl) {
    if (CanReplace(conflicting_keyword_turl))
      Remove(conflicting_keyword_turl);
    else
      keep_old_keyword = true;
  }
  // Update existing turl with new values. This must happen after calling
  // Remove(conflicting_keyword_turl) above, since otherwise during that
  // function RemoveFromMaps() may find |existing_turl| as an alternate engine
  // for the same keyword.  Duplicate keyword handling is only meant for the
  // case of extensions, and if done here would leave internal state
  // inconsistent (e.g. |existing_turl| would already be re-added to maps before
  // calling AddToMaps() below).
  existing_turl->CopyFrom(new_values);
  existing_turl->data_.id = previous_id;
  if (keep_old_keyword)
    existing_turl->data_.SetKeyword(old_keyword);

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

  DCHECK(!HasDuplicateKeywords());
  return true;
}

// static
void TemplateURLService::UpdateTemplateURLIfPrepopulated(
    TemplateURL* template_url,
    PrefService* prefs) {
  int prepopulate_id = template_url->prepopulate_id();
  if (template_url->prepopulate_id() == 0)
    return;

  std::vector<std::unique_ptr<TemplateURLData>> prepopulated_urls =
      TemplateURLPrepopulateData::GetPrepopulatedEngines(prefs, nullptr);

  for (const auto& url : prepopulated_urls) {
    if (url->prepopulate_id == prepopulate_id) {
      MergeIntoPrepopulatedEngineData(template_url, url.get());
      template_url->CopyFrom(TemplateURL(*url));
    }
  }
}

void TemplateURLService::MaybeUpdateDSEViaPrefs(TemplateURL* synced_turl) {
  if (prefs_ &&
      (synced_turl->sync_guid() ==
          prefs_->GetString(prefs::kSyncedDefaultSearchProviderGUID))) {
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
    base::string16 search_terms;
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

void TemplateURLService::RequestGoogleURLTrackerServerCheckIfNecessary() {
  if (default_search_provider_ &&
      default_search_provider_->HasGoogleBaseURLs(search_terms_data()) &&
      google_url_tracker_)
    google_url_tracker_->RequestServerCheck();
}

void TemplateURLService::GoogleBaseURLChanged() {
  if (!loaded_) {
    if (initial_default_search_provider_.get() &&
        initial_default_search_provider_->HasGoogleBaseURLs(
            search_terms_data())) {
      initial_default_search_provider_->InvalidateCachedValues();
      initial_default_search_provider_->ResetKeywordIfNecessary(
          search_terms_data(), false);
    }
    return;
  }

  // Prepare the queue of TemplateURLs which must be updated. We cannot directly
  // iterate through template_urls_ while we're updating because sometimes we
  // want to remove TemplateURL.
  std::set<TemplateURL*> turls_to_update;
  for (const auto& turl : template_urls_)
    turls_to_update.insert(turl.get());

  Scoper scoper(this);

  while (!turls_to_update.empty()) {
    auto it = turls_to_update.begin();
    TemplateURL* turl = *it;
    turls_to_update.erase(it);
    if (turl->HasGoogleBaseURLs(search_terms_data())) {
      TemplateURL updated_turl(turl->data());
      updated_turl.ResetKeywordIfNecessary(search_terms_data(), false);
      KeywordToTURLAndMeaningfulLength::const_iterator existing_entry =
          keyword_to_turl_and_length_.find(updated_turl.keyword());
      if (existing_entry != keyword_to_turl_and_length_.end()) {
        TemplateURL* existing_turl = existing_entry->second.first;
        if (existing_turl != turl) {
          // The new autogenerated keyword conflicts with another TemplateURL.
          // Overwrite it if it's replaceable; otherwise, leave |turl| using its
          // current keyword.  (This will not prevent |turl| from auto-updating
          // the keyword in the future if the conflicting TemplateURL
          // disappears.) Note that we must still update |turl| in this case, or
          // the |provider_map_| will not be updated correctly.
          if (CanReplace(existing_turl)) {
            Remove(existing_turl);
            // Remove |existing_url| from the queue we're iterating through.
            // Perhaps there is no |existing_url| in this queue already if this
            // cycle processed |existing_url| before |turl|.
            turls_to_update.erase(existing_turl);
          } else {
            updated_turl.data_.SetKeyword(turl->keyword());
          }
        }
      }
      // This will send the keyword change to sync.  Note that other clients
      // need to reset the keyword to an appropriate local value when this
      // change arrives; see CreateTemplateURLFromTemplateURLAndSyncData().
      Update(turl, updated_turl);
    }
  }
}

void TemplateURLService::ApplyDefaultSearchChange(
    const TemplateURLData* data,
    DefaultSearchManager::Source source) {
  if (!ApplyDefaultSearchChangeNoMetrics(data, source))
    return;

  UMA_HISTOGRAM_ENUMERATION(
      "Search.DefaultSearchChangeOrigin", dsp_change_origin_, DSP_CHANGE_MAX);

  if (GetDefaultSearchProvider() &&
      GetDefaultSearchProvider()->HasGoogleBaseURLs(search_terms_data()) &&
      !dsp_change_callback_.is_null())
    dsp_change_callback_.Run();
}

bool TemplateURLService::ApplyDefaultSearchChangeNoMetrics(
    const TemplateURLData* data,
    DefaultSearchManager::Source source) {
  if (!loaded_) {
    // Set |initial_default_search_provider_| from the preferences. This is
    // mainly so we can hold ownership until we get to the point where the list
    // of keywords from Web Data is the owner of everything including the
    // default.
    bool changed = !TemplateURL::MatchesData(
        initial_default_search_provider_.get(), data, search_terms_data());
    TemplateURL::Type initial_engine_type =
        (source == DefaultSearchManager::FROM_EXTENSION)
            ? TemplateURL::NORMAL_CONTROLLED_BY_EXTENSION
            : TemplateURL::NORMAL;
    initial_default_search_provider_ =
        data ? std::make_unique<TemplateURL>(*data, initial_engine_type)
             : nullptr;
    default_search_provider_source_ = source;
    return changed;
  }

  // Prevent recursion if we update the value stored in default_search_manager_.
  // Note that we exclude the case of data == NULL because that could cause a
  // false positive for recursion when the initial_default_search_provider_ is
  // NULL due to policy. We'll never actually get recursion with data == NULL.
  if (source == default_search_provider_source_ && data != nullptr &&
      TemplateURL::MatchesData(default_search_provider_, data,
                               search_terms_data()))
    return false;

  // This may be deleted later. Use exclusively for pointer comparison to detect
  // a change.
  TemplateURL* previous_default_search_engine = default_search_provider_;

  Scoper scoper(this);

  if (default_search_provider_source_ == DefaultSearchManager::FROM_POLICY ||
      source == DefaultSearchManager::FROM_POLICY) {
    // We do this both to remove any no-longer-applicable policy-defined DSE as
    // well as to add the new one, if appropriate.
    UpdateProvidersCreatedByPolicy(
        &template_urls_,
        source == DefaultSearchManager::FROM_POLICY ? data : nullptr);
  }

  // |default_search_provider_source_| must be set before calling Update(),
  // since that function needs to know the source of the update in question.
  default_search_provider_source_ = source;

  if (!data) {
    default_search_provider_ = nullptr;
  } else if (source == DefaultSearchManager::FROM_EXTENSION) {
    default_search_provider_ = FindMatchingDefaultExtensionTemplateURL(*data);
    DCHECK(default_search_provider_);
  } else if (source == DefaultSearchManager::FROM_FALLBACK) {
    default_search_provider_ =
        FindPrepopulatedTemplateURL(data->prepopulate_id);
    if (default_search_provider_) {
      TemplateURLData update_data(*data);
      update_data.sync_guid = default_search_provider_->sync_guid();
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
    }
    if (default_search_provider_ && prefs_) {
      prefs_->SetString(prefs::kSyncedDefaultSearchProviderGUID,
                        default_search_provider_->sync_guid());
    }
  }

  bool changed = default_search_provider_ != previous_default_search_engine;
  if (changed) {
    model_mutated_notification_pending_ = true;
    RequestGoogleURLTrackerServerCheckIfNecessary();
  }

  return changed;
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

  // If |template_url| is not created by an extension, its keyword must not
  // conflict with any already in the model.
  if (!IsCreatedByExtension(template_url.get())) {
    // Note that we can reach here during the loading phase while processing the
    // template URLs from the web data service.  In this case,
    // GetTemplateURLForKeyword() will look not only at what's already in the
    // model, but at the |initial_default_search_provider_|.  Since this engine
    // will presumably also be present in the web data, we need to double-check
    // that any "pre-existing" entries we find are actually coming from
    // |template_urls_|, lest we detect a "conflict" between the
    // |initial_default_search_provider_| and the web data version of itself.
    TemplateURL* existing_turl =
        FindNonExtensionTemplateURLForKeyword(template_url->keyword());

    if (existing_turl && Contains(&template_urls_, existing_turl)) {
      DCHECK_NE(existing_turl, template_url.get());
      if (CanReplace(existing_turl)) {
        Remove(existing_turl);
      } else if (CanReplace(template_url.get())) {
        return nullptr;
      } else {
        // Neither engine can be replaced. Uniquify the existing keyword.
        base::string16 new_keyword = UniquifyKeyword(*existing_turl, false);
        ResetTemplateURL(existing_turl, existing_turl->short_name(),
                         new_keyword, existing_turl->url());
        DCHECK_EQ(new_keyword, existing_turl->keyword());
      }
    }
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

  DCHECK(!HasDuplicateKeywords());
  return template_url_ptr;
}

// |template_urls| are the TemplateURLs loaded from the database.
// |default_from_prefs| is the default search provider from the preferences, or
// NULL if the DSE is not policy-defined.
//
// This function removes from the vector and the database all the TemplateURLs
// that were set by policy, unless it is the current default search provider, in
// which case it is updated with the data from prefs.
void TemplateURLService::UpdateProvidersCreatedByPolicy(
    OwnedTemplateURLVector* template_urls,
    const TemplateURLData* default_from_prefs) {
  DCHECK(template_urls);

  Scoper scoper(this);

  for (auto i = template_urls->begin(); i != template_urls->end();) {
    TemplateURL* template_url = i->get();
    if (template_url->created_by_policy()) {
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

      TemplateURLID id = template_url->id();
      RemoveFromMaps(template_url);
      i = template_urls->erase(i);
      if (web_data_service_)
        web_data_service_->RemoveKeyword(id);
    } else {
      ++i;
    }
  }

  if (default_from_prefs) {
    default_search_provider_ = nullptr;
    default_search_provider_source_ = DefaultSearchManager::FROM_POLICY;
    TemplateURLData new_data(*default_from_prefs);
    if (new_data.sync_guid.empty())
      new_data.sync_guid = base::GenerateGUID();
    new_data.created_by_policy = true;
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

base::string16 TemplateURLService::UniquifyKeyword(const TemplateURL& turl,
                                                   bool force) {
  DCHECK(!IsCreatedByExtension(&turl));
  if (!force) {
    // Already unique.
    if (!GetTemplateURLForKeyword(turl.keyword()))
      return turl.keyword();

    // First, try to return the generated keyword for the TemplateURL.
    GURL gurl(turl.url());
    if (gurl.is_valid()) {
      base::string16 keyword_candidate = TemplateURL::GenerateKeyword(gurl);
      if (!GetTemplateURLForKeyword(keyword_candidate))
        return keyword_candidate;
    }
  }

  // We try to uniquify the keyword by appending a special character to the end.
  // This is a best-effort approach where we try to preserve the original
  // keyword and let the user do what they will after our attempt.
  base::string16 keyword_candidate(turl.keyword());
  do {
    keyword_candidate.append(base::ASCIIToUTF16("_"));
  } while (GetTemplateURLForKeyword(keyword_candidate));

  return keyword_candidate;
}

bool TemplateURLService::IsLocalTemplateURLBetter(
    const TemplateURL* local_turl,
    const TemplateURL* sync_turl,
    bool prefer_local_default) const {
  DCHECK(GetTemplateURLForGUID(local_turl->sync_guid()));
  return local_turl->last_modified() > sync_turl->last_modified() ||
         local_turl->created_by_policy() ||
         (prefer_local_default && local_turl == GetDefaultSearchProvider());
}

void TemplateURLService::ResolveSyncKeywordConflict(
    TemplateURL* unapplied_sync_turl,
    TemplateURL* applied_sync_turl,
    syncer::SyncChangeList* change_list) {
  DCHECK(loaded_);
  DCHECK(unapplied_sync_turl);
  DCHECK(applied_sync_turl);
  DCHECK(change_list);
  DCHECK_EQ(applied_sync_turl->keyword(), unapplied_sync_turl->keyword());
  DCHECK_EQ(TemplateURL::NORMAL, applied_sync_turl->type());

  Scoper scoper(this);

  // Both |unapplied_sync_turl| and |applied_sync_turl| are known to Sync, so
  // don't delete either of them. Instead, determine which is "better" and
  // uniquify the other one, sending an update to the server for the updated
  // entry.
  const bool applied_turl_is_better =
      IsLocalTemplateURLBetter(applied_sync_turl, unapplied_sync_turl);
  TemplateURL* loser = applied_turl_is_better ?
      unapplied_sync_turl : applied_sync_turl;
  base::string16 new_keyword = UniquifyKeyword(*loser, false);
  DCHECK(!GetTemplateURLForKeyword(new_keyword));
  if (applied_turl_is_better) {
    // Just set the keyword of |unapplied_sync_turl|. The caller is responsible
    // for adding or updating unapplied_sync_turl in the local model.
    unapplied_sync_turl->data_.SetKeyword(new_keyword);
  } else {
    // Update |applied_sync_turl| in the local model with the new keyword.
    TemplateURLData data(applied_sync_turl->data());
    data.SetKeyword(new_keyword);
    Update(applied_sync_turl, TemplateURL(data));
  }
  // The losing TemplateURL should have their keyword updated. Send a change to
  // the server to reflect this change.
  syncer::SyncData sync_data = CreateSyncDataFromTemplateURL(*loser);
  change_list->push_back(syncer::SyncChange(FROM_HERE,
      syncer::SyncChange::ACTION_UPDATE,
      sync_data));
}

void TemplateURLService::MergeInSyncTemplateURL(
    TemplateURL* sync_turl,
    const SyncDataMap& sync_data,
    syncer::SyncChangeList* change_list,
    SyncDataMap* local_data,
    syncer::SyncMergeResult* merge_result) {
  DCHECK(sync_turl);
  DCHECK(!GetTemplateURLForGUID(sync_turl->sync_guid()));
  DCHECK(IsFromSync(sync_turl, sync_data));

  TemplateURL* conflicting_turl =
      FindNonExtensionTemplateURLForKeyword(sync_turl->keyword());
  bool should_add_sync_turl = true;

  Scoper scoper(this);

  // Resolve conflicts with local TemplateURLs.
  if (conflicting_turl) {
    // Modify |conflicting_turl| to make room for |sync_turl|.
    if (IsFromSync(conflicting_turl, sync_data)) {
      // |conflicting_turl| is already known to Sync, so we're not allowed to
      // remove it. In this case, we want to uniquify the worse one and send an
      // update for the changed keyword to sync. We can reuse the logic from
      // ResolveSyncKeywordConflict for this.
      ResolveSyncKeywordConflict(sync_turl, conflicting_turl, change_list);
      merge_result->set_num_items_modified(
          merge_result->num_items_modified() + 1);
    } else {
      // |conflicting_turl| is not yet known to Sync. If it is better, then we
      // want to transfer its values up to sync. Otherwise, we remove it and
      // allow the entry from Sync to overtake it in the model.
      const std::string guid = conflicting_turl->sync_guid();
      if (IsLocalTemplateURLBetter(conflicting_turl, sync_turl)) {
        ResetTemplateURLGUID(conflicting_turl, sync_turl->sync_guid());
        syncer::SyncData sync_data =
            CreateSyncDataFromTemplateURL(*conflicting_turl);
        change_list->push_back(syncer::SyncChange(
            FROM_HERE, syncer::SyncChange::ACTION_UPDATE, sync_data));
        // Note that in this case we do not add the Sync TemplateURL to the
        // local model, since we've effectively "merged" it in by updating the
        // local conflicting entry with its sync_guid.
        should_add_sync_turl = false;
        merge_result->set_num_items_modified(
            merge_result->num_items_modified() + 1);
      } else {
        // We guarantee that this isn't the local search provider. Otherwise,
        // local would have won.
        DCHECK(conflicting_turl != GetDefaultSearchProvider());
        Remove(conflicting_turl);
        merge_result->set_num_items_deleted(
            merge_result->num_items_deleted() + 1);
      }
      // This TemplateURL was either removed or overwritten in the local model.
      // Remove the entry from the local data so it isn't pushed up to Sync.
      local_data->erase(guid);
    }
    // prepopulate_id 0 effectively means unspecified; i.e. that the turl isn't
    // a pre-populated one, so we want to ignore that case.
  } else if (sync_turl->prepopulate_id() != 0) {
    // Check for a turl with a conflicting prepopulate_id. This detects the case
    // where the user changes a prepopulated engine's keyword on one client,
    // then begins syncing on another client.  We want to reflect this keyword
    // change to that prepopulated URL on other clients instead of assuming that
    // the modified TemplateURL is a new entity.
    TemplateURL* conflicting_prepopulated_turl =
        FindPrepopulatedTemplateURL(sync_turl->prepopulate_id());

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
    if (conflicting_prepopulated_turl &&
        !IsFromSync(conflicting_prepopulated_turl, sync_data) &&
        !IsLocalTemplateURLBetter(conflicting_prepopulated_turl, sync_turl,
                                  false)) {
      std::string guid = conflicting_prepopulated_turl->sync_guid();
      if (conflicting_prepopulated_turl == default_search_provider_) {
        bool pref_matched =
            prefs_->GetString(prefs::kSyncedDefaultSearchProviderGUID) ==
            default_search_provider_->sync_guid();
        // Update the existing engine in-place.
        Update(default_search_provider_, TemplateURL(sync_turl->data()));
        // If prefs::kSyncedDefaultSearchProviderGUID matched
        // |default_search_provider_|'s GUID before, then update it to match its
        // new GUID. If the pref didn't match before, then it probably refers to
        // a new search engine from Sync which just hasn't been added locally
        // yet, so leave it alone in that case.
        if (pref_matched) {
          prefs_->SetString(prefs::kSyncedDefaultSearchProviderGUID,
                            default_search_provider_->sync_guid());
        }

        should_add_sync_turl = false;
        merge_result->set_num_items_modified(
            merge_result->num_items_modified() + 1);
      } else {
        Remove(conflicting_prepopulated_turl);
        merge_result->set_num_items_deleted(merge_result->num_items_deleted() +
                                            1);
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
    merge_result->set_num_items_added(merge_result->num_items_added() + 1);
  }
}

void TemplateURLService::PatchMissingSyncGUIDs(
    OwnedTemplateURLVector* template_urls) {
  DCHECK(template_urls);
  for (auto& template_url : *template_urls) {
    DCHECK(template_url);
    if (template_url->sync_guid().empty() &&
        (template_url->type() == TemplateURL::NORMAL)) {
      template_url->data_.sync_guid = base::GenerateGUID();
      if (web_data_service_)
        web_data_service_->UpdateKeyword(template_url->data());
    }
  }
}

void TemplateURLService::OnSyncedDefaultSearchProviderGUIDChanged() {
  base::AutoReset<DefaultSearchChangeOrigin> change_origin(
      &dsp_change_origin_, DSP_CHANGE_SYNC_PREF);

  std::string new_guid =
      prefs_->GetString(prefs::kSyncedDefaultSearchProviderGUID);
  if (new_guid.empty()) {
    default_search_manager_.ClearUserSelectedDefaultSearchEngine();
    return;
  }

  const TemplateURL* turl = GetTemplateURLForGUID(new_guid);
  if (turl)
    default_search_manager_.SetUserSelectedDefaultSearchEngine(turl->data());
}

template <typename Container>
void TemplateURLService::AddMatchingKeywordsHelper(
    const Container& keyword_to_turl_and_length,
    const base::string16& prefix,
    bool supports_replacement_only,
    TURLsAndMeaningfulLengths* matches) {
  // Sanity check args.
  if (prefix.empty())
    return;
  DCHECK(matches);

  // Find matching keyword range.  Searches the element map for keywords
  // beginning with |prefix| and stores the endpoints of the resulting set in
  // |match_range|.
  const auto match_range(std::equal_range(
      keyword_to_turl_and_length.begin(), keyword_to_turl_and_length.end(),
      typename Container::value_type(prefix,
                                     TURLAndMeaningfulLength(nullptr, 0)),
      LessWithPrefix()));

  // Add to vector of matching keywords.
  for (typename Container::const_iterator i(match_range.first);
    i != match_range.second; ++i) {
    if (!supports_replacement_only ||
        i->second.first->url_ref().SupportsReplacement(search_terms_data()))
      matches->push_back(i->second);
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

bool TemplateURLService::HasDuplicateKeywords() const {
  std::map<base::string16, TemplateURL*> keyword_to_template_url;
  for (const auto& template_url : template_urls_) {
    // Validate no duplicate normal engines with same keyword.
    if (!IsCreatedByExtension(template_url.get())) {
      if (keyword_to_template_url.find(template_url->keyword()) !=
          keyword_to_template_url.end()) {
        return true;
      }
      keyword_to_template_url[template_url->keyword()] = template_url.get();
    }
  }
  return false;
}
