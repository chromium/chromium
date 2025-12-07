// Copyright 2014 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/search_engines/template_url_service.h"

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
#include "base/containers/fixed_flat_map.h"
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
#include "base/notreached.h"
#include "base/observer_list.h"
#include "base/rand_util.h"
#include "base/strings/strcat.h"
#include "base/strings/string_split.h"
#include "base/strings/string_util.h"
#include "base/strings/stringprintf.h"
#include "base/strings/utf_string_conversions.h"
#include "build/build_config.h"
#include "components/country_codes/country_codes.h"
#include "components/google/core/common/google_util.h"
#include "components/omnibox/common/omnibox_feature_configs.h"
#include "components/omnibox/common/omnibox_features.h"
#include "components/pref_registry/pref_registry_syncable.h"
#include "components/prefs/pref_service.h"
#include "components/regional_capabilities/access/country_access_reason.h"
#include "components/regional_capabilities/regional_capabilities_country_id.h"
#include "components/search_engines/choice_made_location.h"
#include "components/search_engines/enterprise/enterprise_search_manager.h"
#include "components/search_engines/keyword_web_data_service.h"
#include "components/search_engines/regulatory_extension_type.h"
#include "components/search_engines/search_engine_choice/search_engine_choice_service.h"
#include "components/search_engines/search_engine_choice/search_engine_choice_utils.h"
#include "components/search_engines/search_engine_type.h"
#include "components/search_engines/search_engines_pref_names.h"
#include "components/search_engines/search_engines_switches.h"
#include "components/search_engines/search_terms_data.h"
#include "components/search_engines/template_url.h"
#include "components/search_engines/template_url_data.h"
#include "components/search_engines/template_url_prepopulate_data.h"
#include "components/search_engines/template_url_prepopulate_data_resolver.h"
#include "components/search_engines/template_url_service_client.h"
#include "components/search_engines/template_url_service_observer.h"
#include "components/search_engines/template_url_starter_pack_data.h"
#include "components/search_engines/util.h"
#include "components/sync/base/features.h"
#include "components/sync/model/sync_change.h"
#include "components/sync/model/sync_change_processor.h"
#include "components/sync/protocol/entity_data.h"
#include "components/sync/protocol/entity_specifics.pb.h"
#include "components/sync/protocol/search_engine_specifics.pb.h"
#include "components/url_formatter/url_fixer.h"
#include "net/base/registry_controlled_domains/registry_controlled_domain.h"
#include "url/gurl.h"
#include "url/origin.h"
#include "url/third_party/mozilla/url_parse.h"
#include "url/url_util.h"

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
  const std::string guid =
      change_i.sync_data().GetSpecifics().search_engine().sync_guid();
  syncer::SyncChange::SyncChangeType type = change_i.change_type();
  if ((type == syncer::SyncChange::ACTION_UPDATE ||
       type == syncer::SyncChange::ACTION_DELETE) &&
      sync_data->find(guid) == sync_data->end()) {
    return true;
  }
  if (type == syncer::SyncChange::ACTION_ADD &&
      sync_data->find(guid) != sync_data->end()) {
    return true;
  }
  if (type == syncer::SyncChange::ACTION_UPDATE) {
    for (size_t j = index + 1; j < change_list->size(); j++) {
      const syncer::SyncChange& change_j = (*change_list)[j];
      if ((syncer::SyncChange::ACTION_UPDATE == change_j.change_type()) &&
          (change_j.sync_data().GetSpecifics().search_engine().sync_guid() ==
           guid)) {
        return true;
      }
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
  for (size_t i = 0; i < change_list->size();) {
    if (ShouldRemoveSyncChange(i, change_list, sync_data)) {
      change_list->erase(change_list->begin() + i);
    } else {
      ++i;
    }
  }
}

// Returns true if |turl|'s GUID is not found inside |sync_data|. This is to be
// used in MergeDataAndStartSyncing to differentiate between TemplateURLs from
// Sync and TemplateURLs that were initially local, assuming |sync_data| is the
// |initial_sync_data| parameter.
bool IsFromSync(const TemplateURL* turl, const SyncDataMap& sync_data) {
  return base::Contains(sync_data, turl->sync_guid());
}

bool Contains(TemplateURLService::OwnedTemplateURLVector* template_urls,
              const TemplateURL* turl) {
  return FindTemplateURL(template_urls, turl) != template_urls->end();
}

bool IsCreatedByExtension(const TemplateURL& template_url) {
  return template_url.type() == TemplateURL::NORMAL_CONTROLLED_BY_EXTENSION ||
         template_url.type() == TemplateURL::OMNIBOX_API_EXTENSION;
}

// Check if `is_active` status should be merged.  This is true if the
// `new_values` is enforced by policy. This handles two scenarios:
// 1. Recommended policy update: If an admin updates a recommended policy
//    (e.g., changes the engine name), a user-deactivated engine should remain
//    deactivated. Returns false.
// 2. Recommended to mandatory policy update: If an admin changes a policy
//    from recommended to mandatory, a user-deactivated engine should be
//    force-activated. Returns true.
// This preserves user deactivation for recommended site search engines unless
// the policy becomes mandatory.
bool ShouldMergeEnterpriseSearchEnginesActiveStatus(
    const TemplateURLData& existing_data,
    const TemplateURL& new_values) {
  return new_values.enforced_by_policy() &&
         existing_data.is_active != new_values.is_active();
}

// Checks if `new_values` has updated versions of `existing_turl`. Only fields
// set by the search engine policies are checked.
bool ShouldMergeEnterpriseSearchEngines(const TemplateURL& existing_turl,
                                        const TemplateURL& new_values) {
  CHECK_EQ(existing_turl.keyword(), new_values.keyword());

  return existing_turl.short_name() != new_values.short_name() ||
         existing_turl.url() != new_values.url() ||
         existing_turl.suggestions_url() != new_values.suggestions_url() ||
         existing_turl.featured_by_policy() !=
             new_values.featured_by_policy() ||
         (existing_turl.policy_origin() ==
              TemplateURLData::PolicyOrigin::kSearchAggregator &&
          existing_turl.favicon_url() != new_values.favicon_url()) ||
         existing_turl.enforced_by_policy() !=
             new_values.enforced_by_policy() ||
         ShouldMergeEnterpriseSearchEnginesActiveStatus(existing_turl.data(),
                                                        new_values);
}

// Creates a new `TemplateURL` that copies updates fields from `new_values` into
// `existing_turl`. Only fields set by policy are copied from `new_values`, all
// other fields are copied unchanged from `existing_turl`.
TemplateURLData MergeEnterpriseSearchEngines(TemplateURLData existing_data,
                                             const TemplateURL& new_values) {
  CHECK_EQ(existing_data.keyword(), new_values.keyword());

  TemplateURLData merged_data(existing_data);
  merged_data.SetShortName(new_values.short_name());
  merged_data.SetURL(new_values.url());
  merged_data.suggestions_url = new_values.suggestions_url();
  merged_data.featured_by_policy = new_values.featured_by_policy();
  if (existing_data.policy_origin ==
      TemplateURLData::PolicyOrigin::kSearchAggregator) {
    merged_data.favicon_url = new_values.favicon_url();
  }
  merged_data.enforced_by_policy = new_values.enforced_by_policy();
  if (ShouldMergeEnterpriseSearchEnginesActiveStatus(existing_data,
                                                     new_values)) {
    merged_data.is_active = new_values.is_active();
  }
  return merged_data;
}

std::unique_ptr<TemplateURL> UpdateExistingURLWithAccountData(
    const TemplateURL* existing_turl,
    const TemplateURLData& account_data) {
  std::optional<TemplateURLData> local_data;
  if (existing_turl && existing_turl->GetLocalData()) {
    local_data = existing_turl->GetLocalData();
    local_data->sync_guid = account_data.sync_guid;
  }
  return std::make_unique<TemplateURL>(std::move(local_data),
                                       std::move(account_data));
}

// If the TemplateURLData comes from a prepopulated URL available in the current
// country, update all its fields save for the keyword, short name and id so
// that they match the internal prepopulated URL. TemplateURLs not coming from
// a prepopulated URL are not modified.
TemplateURLData UpdateTemplateURLDataIfPrepopulated(
    const TemplateURLData& data,
    const TemplateURLPrepopulateData::Resolver& prepopulate_data_resolver) {
  int prepopulate_id = data.prepopulate_id;
  if (data.prepopulate_id == 0) {
    return data;
  }

  std::vector<std::unique_ptr<TemplateURLData>> prepopulated_urls =
      prepopulate_data_resolver.GetPrepopulatedEngines();

  TemplateURL turl(data);
  for (const auto& url : prepopulated_urls) {
    if (url->prepopulate_id == prepopulate_id) {
      MergeIntoEngineData(&turl, url.get());
      return *url;
    }
  }
  return data;
}

// Explicitly converts from ActiveStatus enum in sync protos to enum in
// TemplateURLData.
TemplateURLData::ActiveStatus ActiveStatusFromSync(
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

bool IsUntouchedAutogeneratedTemplateURLDataAndShouldNotSync(
    const TemplateURLData& data) {
  return data.safe_for_autoreplace &&
         data.is_active == TemplateURLData::ActiveStatus::kUnspecified;
}

bool IsUntouchedAutogeneratedRemoteTemplateURLAndShouldNotSync(
    const sync_pb::SearchEngineSpecifics& specifics) {
  return specifics.safe_for_autoreplace() &&
         ActiveStatusFromSync(specifics.is_active()) ==
             TemplateURLData::ActiveStatus::kUnspecified;
}

bool IsAccountDataActive(const TemplateURL* turl) {
  if (turl->GetAccountData() &&
      &turl->GetAccountData().value() == &turl->data()) {
    return true;
  }
  CHECK_EQ(&turl->GetLocalData().value(), &turl->data());
  return false;
}

std::string_view SyncChangeTypeToHistogramSuffix(
    syncer::SyncChange::SyncChangeType type) {
  switch (type) {
    case syncer::SyncChange::ACTION_ADD:
      return "Added";
    case syncer::SyncChange::ACTION_UPDATE:
      return "Updated";
    case syncer::SyncChange::ACTION_DELETE:
      return "Deleted";
  }
  NOTREACHED();
}

// Logs the number of changes of each type to the histogram
// `histogram_prefix_{Type}` upon MergeDataAndStartSyncing and
// ProcessSyncChanges.
void LogSyncChangesToHistogram(const syncer::SyncChangeList& change_list,
                               std::string_view histogram_prefix) {
  auto counts = base::MakeFixedFlatMap<syncer::SyncChange::SyncChangeType, int>(
      {{syncer::SyncChange::ACTION_ADD, 0},
       {syncer::SyncChange::ACTION_UPDATE, 0},
       {syncer::SyncChange::ACTION_DELETE, 0}});
  for (const syncer::SyncChange& change : change_list) {
    // No ADDs should be committed upon initial or incremental update.
    CHECK(!base::FeatureList::IsEnabled(
              syncer::kSeparateLocalAndAccountSearchEngines) ||
          change.change_type() != syncer::SyncChange::ACTION_ADD);
    ++counts.at(change.change_type());
  }
  for (const auto& [type, count] : counts) {
    base::UmaHistogramCounts100(
        base::StringPrintf("%s_%s", histogram_prefix,
                           SyncChangeTypeToHistogramSuffix(type)),
        count);
  }
}

bool ShouldCommitUpdateToAccount(
    const std::optional<TemplateURLData>& old_account_data,
    const std::optional<TemplateURLData>& new_account_data) {
  CHECK(base::FeatureList::IsEnabled(
      syncer::kSeparateLocalAndAccountSearchEngines));
  if (old_account_data == new_account_data || !new_account_data.has_value()) {
    // Account data is unchanged or does not exist.
    return false;
  }
  bool account_data_changed = true;
  // If no local data exists, account data is newly added and hence
  // `account_data_changed` is true.
  if (old_account_data.has_value()) {
    // Avoid favicon-only changes.
    TemplateURLData new_account_data_copy = *new_account_data;
    new_account_data_copy.favicon_url = old_account_data->favicon_url;
    account_data_changed = new_account_data_copy != *old_account_data;
  }
  base::UmaHistogramBoolean("Sync.SearchEngine.FaviconOnlyUpdate",
                            !account_data_changed);
  return account_data_changed;
}

// Checks if `url` is a Google AI mode URL. Uses the `udm` query param. Only
// works for Google URLs because it's unknown what other search providers will
// use to distinguish their AI mode and traditional search URLs.
bool IsGoogleAiModeUrl(GURL url) {
  // Check that:
  // 1. `url` contains a `udm=50` query param which distinguish Google AI mode
  //    and traditional search URLs. This check alone isn't sufficient because
  //    any website could coincidentally use the same query param for its own
  //    purposes.
  // 2. `url` is a Google URL. This check is done 2nd because it's slower (0.5us
  //    v 5us).

  std::string_view query = url.query();
  url::Component query_iterator(0, query.length());
  url::Component key, value;
  bool udm_50 = false;
  while (url::ExtractQueryKeyValue(query, &query_iterator, &key, &value) &&
         !udm_50) {
    std::string_view key_string = query.substr(key.begin, key.len);
    std::string_view value_string = query.substr(value.begin, value.len);
    udm_50 = key_string == "udm" && value_string == "50";
  }

  return udm_50 && google_util::IsGoogleDomainUrl(
                       url, google_util::DISALLOW_SUBDOMAIN,
                       google_util::DISALLOW_NON_STANDARD_PORTS);
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

      if (!service_->loaded_) {
        return;
      }

      for (auto& observer : service_->model_observers_) {
        observer.OnTemplateURLServiceChanged();
      }
    }
  }

 private:
  std::unique_ptr<KeywordWebDataService::BatchModeScoper> batch_mode_scoper_;
  raw_ptr<TemplateURLService, DanglingUntriaged> service_;
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

  TemplateURLService::OwnedTemplateURLVector TakeSearchEngines() {
    return std::move(search_engines_);
  }

  void set_search_engines(
      TemplateURLService::OwnedTemplateURLVector&& search_engines) {
    search_engines_ = std::move(search_engines);
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
          return turl.GenerateSearchURL(*search_terms_data).host() == host;
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

    for (auto& search_engine : search_engines_) {
      if (selector.Run(*search_engine)) {
        return search_engine.get();
      }
    }

    return nullptr;
  }

  // A temporary location for the DSE until Web Data has been loaded and it can
  // be merged into |template_urls_|.
  std::unique_ptr<TemplateURL> default_search_provider_;

  // A temporary location for site search set by policy until Web Data has been
  // loaded and it can be merged into |template_urls_|.
  TemplateURLService::OwnedTemplateURLVector search_engines_;
};

// TemplateURLService ---------------------------------------------------------
TemplateURLService::TemplateURLService(
    PrefService& prefs,
    search_engines::SearchEngineChoiceService& search_engine_choice_service,
    TemplateURLPrepopulateData::Resolver& prepopulate_data_resolver,
    std::unique_ptr<SearchTermsData> search_terms_data,
    const scoped_refptr<KeywordWebDataService>& web_data_service,
    std::unique_ptr<TemplateURLServiceClient> client,
    const base::RepeatingClosure& dsp_change_callback)
    : prefs_(prefs),
      search_engine_choice_service_(search_engine_choice_service),
      prepopulate_data_resolver_(prepopulate_data_resolver),
      search_terms_data_(std::move(search_terms_data)),
      web_data_service_(web_data_service),
      client_(std::move(client)),
      dsp_change_callback_(dsp_change_callback),
      pre_loading_providers_(std::make_unique<PreLoadingProviders>()),
      default_search_manager_(
          &prefs,
          &search_engine_choice_service,
          prepopulate_data_resolver_.get(),
          base::BindRepeating(&TemplateURLService::ApplyDefaultSearchChange,
                              base::Unretained(this))),
      enterprise_search_manager_(GetEnterpriseSearchManager(&prefs)) {
  DCHECK(search_terms_data_);
  Init();
}

TemplateURLService::TemplateURLService(
    PrefService& prefs,
    search_engines::SearchEngineChoiceService& search_engine_choice_service,
    TemplateURLPrepopulateData::Resolver& prepopulate_data_resolver,
    base::span<const TemplateURLService::Initializer> initializers)
    : TemplateURLService(
          prefs,
          search_engine_choice_service,
          prepopulate_data_resolver,
          /*search_terms_data=*/std::make_unique<SearchTermsData>(),
          /*web_data_service=*/nullptr,
          /*client=*/nullptr,
          /*dsp_change_callback=*/base::RepeatingClosure()) {
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
  registry->RegisterStringPref(prefs::kDefaultSearchProviderGUID,
                               std::string());
  registry->RegisterBooleanPref(prefs::kDefaultSearchProviderEnabled, true);
  registry->RegisterBooleanPref(
      prefs::kDefaultSearchProviderContextMenuAccessAllowed, true);
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
                             // keywords. If we need to support empty keywords
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
  return !url.is_valid() || url.GetHost().empty() ||
         CanAddAutogeneratedKeywordForHost(url.GetHost());
}

bool TemplateURLService::IsPrepopulatedOrDefaultProviderByPolicy(
    const TemplateURL* t_url) const {
  return (t_url->prepopulate_id() > 0 ||
          t_url->CreatedByDefaultSearchProviderPolicy() ||
          t_url->CreatedByRegulatoryProgram()) &&
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
  switch (t_url->policy_origin()) {
    case TemplateURLData::PolicyOrigin::kDefaultSearchProvider:
      return false;

    case TemplateURLData::PolicyOrigin::kNoPolicy:
    case TemplateURLData::PolicyOrigin::kSiteSearch:
    case TemplateURLData::PolicyOrigin::kSearchAggregator:
      // Hide if another engine (e.g., one set by user/policy) takes precedence
      // for the same keyword. `GetTemplateURLForKeyword` already ensures
      // prioritization of search engines, so there is no need to replicate the
      // logic here.
      return t_url != GetTemplateURLForKeyword(t_url->keyword());
  }
}

void TemplateURLService::AddMatchingKeywords(const std::u16string& prefix,
                                             bool supports_replacement_only,
                                             TemplateURLVector* turls) {
  // Sanity check args.
  if (prefix.empty() || !turls) {
    return;
  }

  // Find matching keyword range.  Searches the element map for keywords
  // beginning with |prefix| and stores the endpoints of the resulting set in
  // |match_range|.
  const auto match_range(std::equal_range(
      keyword_to_turl_.begin(), keyword_to_turl_.end(),
      typename KeywordToTURL::value_type(prefix, nullptr), LessWithPrefix()));

  // Add to vector of matching keywords.
  for (auto i = match_range.first; i != match_range.second; ++i) {
    if (!supports_replacement_only ||
        i->second->url_ref().SupportsReplacement(search_terms_data())) {
      turls->push_back(i->second);
    }
  }
}

TemplateURL* TemplateURLService::GetTemplateURLForKeyword(
    const std::u16string& keyword) {
  return const_cast<TemplateURL*>(
      static_cast<const TemplateURLService*>(this)->GetTemplateURLForKeyword(
          keyword));
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
      static_cast<const TemplateURLService*>(this)->GetTemplateURLForGUID(
          sync_guid));
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
      static_cast<const TemplateURLService*>(this)->GetTemplateURLForHost(
          host));
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

TemplateURL* TemplateURLService::Add(
    std::unique_ptr<TemplateURL> template_url) {
  DCHECK(template_url);
  DCHECK(!IsCreatedByExtension(*template_url.get()) ||
         (!FindTemplateURLForExtension(
              template_url->GetExtensionInfo()->extension_id,
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
  template_url->set_short_name(short_name);
  template_url->set_keyword(keyword);
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

  // To ensure that policy engines are not added again on next
  // policy fetch, mark the keyword as overridden in the pref.
  if (template_url->CanPolicyBeOverridden()) {
    AddOverriddenKeywordForTemplateURL(template_url);
  }

  auto i = FindTemplateURL(&template_urls_, template_url);
  if (i == template_urls_.end()) {
    return;
  }

  Scoper scoper(this);
  model_mutated_notification_pending_ = true;

  RemoveFromMaps(template_url);

  // Remove it from the vector containing all TemplateURLs.
  std::unique_ptr<TemplateURL> scoped_turl = std::move(*i);
  template_urls_.erase(i);

  if (template_url->type() == TemplateURL::NORMAL) {
    if (web_data_service_ && template_url->GetLocalData()) {
      web_data_service_->RemoveKeyword(template_url->id());
    }
    // Inform sync of the deletion.
    ProcessTemplateURLChange(FROM_HERE, const_cast<TemplateURL*>(template_url),
                             syncer::SyncChange::ACTION_DELETE);

    // The default search engine can't be deleted. But the user defined DSE can
    // be hidden by an extension or policy and then deleted. Clean up the user
    // prefs then.
    if (template_url->sync_guid() ==
        prefs_->GetString(prefs::kDefaultSearchProviderGUID)) {
      prefs_->SetString(prefs::kDefaultSearchProviderGUID, std::string());
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
  if (!url) {
    return;
  }
  // NULL this out so that we can call Remove.
  if (default_search_provider_ == url) {
    default_search_provider_ = nullptr;
  }
  Remove(url);
  RemoveFromUnscopedModeExtensionIdsIfPresent(extension_id);
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

void TemplateURLService::RegisterExtensionControlledTURL(
    const std::string& extension_id,
    const std::string& extension_name,
    const std::string& keyword,
    const std::string& template_url_string,
    const base::Time& extension_install_time,
    const bool unscoped_mode_allowed) {
  DCHECK(loaded_);

  if (FindTemplateURLForExtension(extension_id,
                                  TemplateURL::OMNIBOX_API_EXTENSION)) {
    return;
  }

  if (unscoped_mode_allowed) {
    AddToUnscopedModeExtensionIds(extension_id);
  }

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
  for (const auto& turl : template_urls_) {
    result.push_back(turl.get());
  }
  return result;
}

std::unique_ptr<search_engines::ChoiceScreenData>
TemplateURLService::GetChoiceScreenData() {
  return search_engine_choice_service_->GetChoiceScreenData(
      search_terms_data(), GetDefaultSearchProvider());
}

TemplateURL* TemplateURLService::GetEnterpriseSearchAggregatorEngine() const {
  auto it = std::ranges::find_if(
      enterprise_search_keyword_to_turl_, [](const auto& keyword_and_turl) {
        return keyword_and_turl.second
            ->CreatedByEnterpriseSearchAggregatorPolicy();
      });
  return it == enterprise_search_keyword_to_turl_.end() ? nullptr : it->second;
}

bool TemplateURLService::IsShortcutRequiredForSearchAggregatorEngine() const {
  return enterprise_search_manager_ &&
         enterprise_search_manager_->GetRequireShortcutValue();
}

TemplateURLService::TemplateURLVector
TemplateURLService::GetFeaturedEnterpriseSiteSearchEngines() const {
  TemplateURLVector result;
  for (const auto& turl : template_urls_) {
    if (turl->CreatedByNonDefaultSearchProviderPolicy() &&
        !turl->CreatedByEnterpriseSearchAggregatorPolicy() &&
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
  if (url->type() != TemplateURL::NORMAL) {
    return;
  }
  if (!Contains(&template_urls_, url)) {
    return;
  }
  url->IncrementUsageCount();

  if (web_data_service_) {
    web_data_service_->UpdateKeyword(url->data());
  }
}

void TemplateURLService::ResetTemplateURL(TemplateURL* url,
                                          const std::u16string& title,
                                          const std::u16string& keyword,
                                          const std::string& search_url) {
  DCHECK(!IsCreatedByExtension(*url));
  DCHECK(!keyword.empty());
  DCHECK(!search_url.empty());

  // Similar to `TemplateURLService::Remove`, mark the keyword as overridden
  // in the pref to prevent a policy created search engine from overriding this
  // one.
  if (url->CanPolicyBeOverridden()) {
    AddOverriddenKeywordForTemplateURL(url);
  }

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
  data.policy_origin = TemplateURLData::PolicyOrigin::kNoPolicy;

  Update(url, base::FeatureList::IsEnabled(
                  syncer::kSeparateLocalAndAccountSearchEngines)
                  ? TemplateURL(data, data)
                  : TemplateURL(data));
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

  Update(url, base::FeatureList::IsEnabled(
                  syncer::kSeparateLocalAndAccountSearchEngines)
                  ? TemplateURL(data, data)
                  : TemplateURL(data));

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
  data.regulatory_origin = RegulatoryExtensionType::kAndroidEEA;
  // Play API engines are created by explicit user gesture, and should not be
  // auto-replaceable by an auto-generated engine as the user browses.
  data.safe_for_autoreplace = false;
  data.is_active = TemplateURLData::ActiveStatus::kTrue;
  return data;
}

bool TemplateURLService::ResetPlayAPISearchEngine(
    const TemplateURLData& new_play_api_turl_data) {
  CHECK(loaded());
  CHECK(new_play_api_turl_data.regulatory_origin ==
        RegulatoryExtensionType::kAndroidEEA);

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
    if (same_keyword_engine->GetRegulatoryExtensionType() ==
        RegulatoryExtensionType::kAndroidEEA) {
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
  auto found = std::ranges::find_if(template_urls_, [](const auto& turl) {
    return turl->GetRegulatoryExtensionType() ==
           RegulatoryExtensionType::kAndroidEEA;
  });

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
      CHECK(CanMakeDefault(new_play_api_turl.get()));

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
  CHECK(new_play_api_turl_ptr);

  // Part 2: Set as DSE.
  // It is still possible that policies control the DSE, so ensure we don't
  // break that.
  if (CanMakeDefault(new_play_api_turl_ptr)) {
    SetUserSelectedDefaultSearchProvider(
        new_play_api_turl_ptr,
        search_engines::ChoiceMadeLocation::kChoiceScreen);
  }

  CHECK(default_search_provider_);
  return true;
}
#endif  // BUILDFLAG(IS_ANDROID)

void TemplateURLService::UpdateProviderFavicons(
    const GURL& potential_search_url,
    const GURL& favicon_url) {
  DCHECK(loaded_);
  DCHECK(potential_search_url.is_valid());

  const TemplateURLSet* urls_for_host =
      provider_map_->GetURLsForHost(potential_search_url.GetHost());
  if (!urls_for_host) {
    return;
  }

  // Make a copy of the container of the matching TemplateURLs, as the original
  // container is invalidated as we update the contained TemplateURLs.
  TemplateURLSet urls_for_host_copy(*urls_for_host);

  Scoper scoper(this);
  for (TemplateURL* turl : urls_for_host_copy) {
    if (!IsCreatedByExtension(*turl) &&
        turl->policy_origin() !=
            TemplateURLData::PolicyOrigin::kSearchAggregator &&
        turl->IsSearchURL(potential_search_url, search_terms_data()) &&
        turl->favicon_url() != favicon_url) {
      TemplateURLData data(turl->data());
      data.favicon_url = favicon_url;
      UpdateData(turl, data);
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
         (url->type() == TemplateURL::NORMAL) &&
         (url->starter_pack_id() == 0) &&
         (!url->CreatedByNonDefaultSearchProviderPolicy());
}

void TemplateURLService::SetUserSelectedDefaultSearchProvider(
    TemplateURL* url,
    search_engines::ChoiceMadeLocation choice_made_location) {
  // Omnibox keywords cannot be made default. Extension-controlled search
  // engines can be made default only by the extension itself because they
  // aren't persisted.
  DCHECK(!url || !IsCreatedByExtension(*url));
  if (url) {
    url->set_is_active(TemplateURLData::ActiveStatus::kTrue);
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
      CHECK_NE(choice_made_location,
               search_engines::ChoiceMadeLocation::kOther);
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

DefaultSearchManager* TemplateURLService::GetDefaultSearchManager() {
  return &default_search_manager_;
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
  if (!next_search) {
    return nullptr;
  }

  // Find the TemplateURL matching the data retrieved.
  auto iter = std::ranges::find_if(
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
  const TemplateURL* template_url = GetTemplateURLForHost(url.GetHost());
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
      prepopulate_data_resolver_->GetPrepopulatedEngines();
  DCHECK(!prepopulated_urls.empty());
  ActionsFromCurrentData actions(CreateActionsFromCurrentPrepopulateData(
      &prepopulated_urls, template_urls_, default_search_provider_));

  // Remove items.
  for (auto i = actions.removed_engines.begin();
       i < actions.removed_engines.end(); ++i) {
    Remove(*i);
  }

  // Edit items.
  for (auto i(actions.edited_engines.begin()); i < actions.edited_engines.end();
       ++i) {
    UpdateData(i->first, i->second);
  }

  // Add items.
  for (std::vector<TemplateURLData>::const_iterator i =
           actions.added_engines.begin();
       i < actions.added_engines.end(); ++i) {
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
      prefs_->SetString(prefs::kDefaultSearchProviderGUID,
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
      template_url_starter_pack_data::GetStarterPackEngines();
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
    UpdateData(i->first, i->second);
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
  if (loaded_ || load_handle_ || disable_load_) {
    return;
  }

  if (web_data_service_) {
    load_handle_ = web_data_service_->GetKeywords(this);
  } else {
    ChangeToLoadedState();
  }
}

base::CallbackListSubscription TemplateURLService::RegisterOnLoadedCallback(
    base::OnceClosure callback) {
  return loaded_ ? base::CallbackListSubscription()
                 : on_loaded_callbacks_.Add(std::move(callback));
}

void TemplateURLService::LogActiveTemplateUrlsOnStartup(
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

void TemplateURLService::LogTemplateUrlTypesOnStartup(
    OwnedTemplateURLVector* template_urls) {
  DCHECK(template_urls);

  // Initialize counts for each type of `TemplateURL`.
  int num_total_turl = 0;
  int num_prepopulated = 0;
  int num_featured_policy_set_site_search = 0;
  int num_policy_set_aggregator = 0;
  int num_featured_policy_set_aggregator = 0;
  int num_starter_pack = 0;
  int num_extension_set_search = 0;
  int num_non_featured_policy_set_site_search = 0;
  int num_policy_set_default_search = 0;
  int num_user_set_default_search = 0;
  int num_user_set_substituting_site_search = 0;
  int num_user_set_non_substituting_site_search = 0;
  int num_featured_allow_user_override_policy_set_site_search = 0;
  int num_non_featured_allow_user_override_policy_set_site_search = 0;

  // Count the number of each type of `TemplateURL`.
  for (auto& turl : *template_urls) {
    const TemplateURLData& data = turl->data();
    // Prepopulated keywords can have `is_active()` equal to
    // `ActiveStatus::kTrue` or `ActiveStatus::kUnspecified`.
    bool is_prepopulated =
        data.prepopulate_id != 0 &&
        turl->is_active() != TemplateURLData::ActiveStatus::kFalse;
    if ((!is_prepopulated &&
         turl->is_active() == TemplateURLData::ActiveStatus::kUnspecified) ||
        turl->is_active() == TemplateURLData::ActiveStatus::kFalse) {
      continue;
    }
    num_total_turl++;
    if (is_prepopulated) {
      num_prepopulated++;
    } else if (turl->featured_by_policy()) {
      if (data.CreatedBySiteSearchPolicy()) {
        data.enforced_by_policy
            ? num_featured_policy_set_site_search++
            : num_featured_allow_user_override_policy_set_site_search++;
      } else if (data.CreatedByEnterpriseSearchAggregatorPolicy()) {
        num_featured_policy_set_aggregator++;
      } else {
        NOTREACHED();
      }
    } else if (data.starter_pack_id != 0) {
      num_starter_pack++;
    } else if (turl->type() == TemplateURL::NORMAL_CONTROLLED_BY_EXTENSION ||
               turl->type() == TemplateURL::OMNIBOX_API_EXTENSION) {
      num_extension_set_search++;
    } else if (data.CreatedBySiteSearchPolicy()) {
      data.enforced_by_policy
          ? num_non_featured_policy_set_site_search++
          : num_non_featured_allow_user_override_policy_set_site_search++;
    } else if (data.CreatedByEnterpriseSearchAggregatorPolicy()) {
      num_policy_set_aggregator++;
    } else if (data.CreatedByDefaultSearchProviderPolicy()) {
      num_policy_set_default_search++;
    } else if (GetDefaultSearchProvider() &&
               data.url() == GetDefaultSearchProvider()->url()) {
      num_user_set_default_search++;
    } else if (!data.CreatedByPolicy()) {
      turl->SupportsReplacement(search_terms_data())
          ? num_user_set_substituting_site_search++
          : num_user_set_non_substituting_site_search++;
    } else {
      NOTREACHED();
    }
  }

  base::UmaHistogramExactLinear(base::StringPrintf(kKeywordCountHistogramName),
                                num_total_turl, 50);

  base::UmaHistogramExactLinear(
      base::StringPrintf("%s.FeaturedSiteSearchSetByPolicy",
                         kKeywordCountHistogramName),
      num_featured_policy_set_site_search, 50);

  base::UmaHistogramExactLinear(
      base::StringPrintf("%s.SearchAggregatorSetByPolicy",
                         kKeywordCountHistogramName),
      num_policy_set_aggregator, 50);

  base::UmaHistogramExactLinear(
      base::StringPrintf("%s.FeaturedSearchAggregatorSetByPolicy",
                         kKeywordCountHistogramName),
      num_featured_policy_set_aggregator, 50);

  base::UmaHistogramExactLinear(
      base::StringPrintf("%s.StarterPack", kKeywordCountHistogramName),
      num_starter_pack, 50);

  base::UmaHistogramExactLinear(
      base::StringPrintf("%s.Prepopulated", kKeywordCountHistogramName),
      num_prepopulated, 50);

  base::UmaHistogramExactLinear(
      base::StringPrintf("%s.SearchEngineSetByExtension",
                         kKeywordCountHistogramName),
      num_extension_set_search, 50);

  base::UmaHistogramExactLinear(
      base::StringPrintf("%s.NonFeaturedSiteSearchSetByPolicy",
                         kKeywordCountHistogramName),
      num_non_featured_policy_set_site_search, 50);

  base::UmaHistogramExactLinear(
      base::StringPrintf("%s.DefaultSearchEngineSetByPolicy",
                         kKeywordCountHistogramName),
      num_policy_set_default_search, 50);

  base::UmaHistogramExactLinear(
      base::StringPrintf("%s.DefaultSearchEngineSetByUser",
                         kKeywordCountHistogramName),
      num_user_set_default_search, 50);

  base::UmaHistogramExactLinear(
      base::StringPrintf("%s.SubstitutingSiteSearchSetByUser",
                         kKeywordCountHistogramName),
      num_user_set_substituting_site_search, 50);

  base::UmaHistogramExactLinear(
      base::StringPrintf("%s.NonSubstitutingSiteSearchSetByUser",
                         kKeywordCountHistogramName),
      num_user_set_non_substituting_site_search, 50);

  base::UmaHistogramExactLinear(
      base::StringPrintf("%s.FeaturedAllowUserOverrideSiteSearchSetByPolicy",
                         kKeywordCountHistogramName),
      num_featured_allow_user_override_policy_set_site_search, 50);

  base::UmaHistogramExactLinear(
      base::StringPrintf("%s.NonFeaturedAllowUserOverrideSiteSearchSetByPolicy",
                         kKeywordCountHistogramName),
      num_non_featured_allow_user_override_policy_set_site_search, 50);
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

  DCHECK_EQ(KEYWORDS_RESULT, result->GetType());
  std::unique_ptr<OwnedTemplateURLVector> template_urls =
      std::make_unique<OwnedTemplateURLVector>();
  WDKeywordsResult::Metadata updated_keywords_metadata;

  {
    const WDKeywordsResult& keyword_result =
        reinterpret_cast<const WDResult<WDKeywordsResult>*>(result.get())
            ->GetValue();
    initial_keywords_database_country_ =
        keyword_result.metadata.builtin_keyword_country;
    GetSearchProvidersUsingKeywordResult(
        keyword_result, web_data_service_.get(), &prefs_.get(),
        prepopulate_data_resolver_.get(), template_urls.get(),
        (default_search_provider_source_ == DefaultSearchManager::FROM_USER)
            ? pre_loading_providers_->default_search_provider()
            : nullptr,
        search_terms_data(), updated_keywords_metadata, &pre_sync_deletes_);
    updated_keywords_database_country_ =
        updated_keywords_metadata.builtin_keyword_country;
  }

  Scoper scoper(this);

  {
    PatchMissingSyncGUIDs(template_urls.get());
    MaybeSetIsActiveSearchEngines(template_urls.get());
    LogActiveTemplateUrlsOnStartup(template_urls.get());
    LogTemplateUrlTypesOnStartup(template_urls.get());
    SetTemplateURLs(std::move(template_urls));

    // This initializes provider_map_ which should be done before
    // calling UpdateKeywordSearchTermsForURL.
    ChangeToLoadedState();

    // Index any visits that occurred before we finished loading.
    for (const auto& visit_to_add : visits_to_add_) {
      UpdateKeywordSearchTermsForURL(visit_to_add);
    }
    visits_to_add_.clear();

    if (updated_keywords_metadata.HasBuiltinKeywordData()) {
      web_data_service_->SetBuiltinKeywordDataVersion(
          updated_keywords_metadata.builtin_keyword_data_version);
      web_data_service_->SetBuiltinKeywordCountry(
          updated_keywords_metadata.builtin_keyword_country->GetRestricted(
              regional_capabilities::CountryAccessKey(
                  regional_capabilities::CountryAccessReason::
                      kTemplateURLServiceDatabaseMetadataCaching)));

    }

    if (updated_keywords_metadata.HasStarterPackData()) {
      web_data_service_->SetStarterPackKeywordVersion(
          updated_keywords_metadata.starter_pack_version);
    }
  }

  if (default_search_provider_) {
    SearchEngineType engine_type =
        default_search_provider_->GetEngineType(search_terms_data());
    // Check for search engines types not present in prepopulated_engines.json.
    // TODO(https://issues.chromium.org/405167888): Remove this check once it is
    // no longer necessary to track these additional search engine types.
    if (engine_type == SEARCH_ENGINE_OTHER) {
      GURL search_url = GURL(default_search_provider_->url());
      if (search_url.is_valid() &&
          url::DomainIs(search_url.host(), "siteadvisor.com")) {
        engine_type = SEARCH_ENGINE_MCAFEE;
      }
    }
    base::UmaHistogramEnumeration("Search.DefaultSearchProviderType2",
                                  engine_type, SEARCH_ENGINE_MAX);
    if (default_search_provider_->CreatedByDefaultSearchProviderPolicy()) {
      base::UmaHistogramEnumeration(
          "Search.DefaultSearchProviderType2.SetByEnterprisePolicy",
          engine_type, SEARCH_ENGINE_MAX);
    } else if (default_search_provider_source_ ==
               DefaultSearchManager::FROM_EXTENSION) {
      base::UmaHistogramEnumeration(
          "Search.DefaultSearchProviderType2.SetByExtension", engine_type,
          SEARCH_ENGINE_MAX);
    } else if (default_search_provider_source_ ==
               DefaultSearchManager::FROM_USER) {
      base::UmaHistogramEnumeration(
          "Search.DefaultSearchProviderType2.SetByUser", engine_type,
          SEARCH_ENGINE_MAX);
    } else if (default_search_provider_source_ ==
               DefaultSearchManager::FROM_FALLBACK) {
      base::UmaHistogramEnumeration(
          "Search.DefaultSearchProviderType2.Fallback", engine_type,
          SEARCH_ENGINE_MAX);
    }
  }
}

void TemplateURLService::OnHistoryURLVisited(const URLVisitedDetails& details) {
  if (!loaded_) {
    visits_to_add_.push_back(details);
  } else {
    UpdateKeywordSearchTermsForURL(details);
  }
}

void TemplateURLService::Shutdown() {
  for (auto& observer : model_observers_) {
    observer.OnTemplateURLServiceShuttingDown();
  }

  if (client_) {
    client_->Shutdown();
  }
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

  if (loaded_) {
    std::move(done).Run();
  } else {
    on_loaded_callback_for_sync_ = std::move(done);
  }
}

syncer::SyncDataList TemplateURLService::GetAllSyncData(
    syncer::DataType type) const {
  DCHECK_EQ(syncer::SEARCH_ENGINES, type);

  syncer::SyncDataList current_data;
  for (const auto& turl : template_urls_) {
    // Don't sync keywords managed by policy.
    if (turl->CreatedByPolicy()) {
      continue;
    }
    // Don't sync local or extension-controlled search engines.
    if (turl->type() != TemplateURL::NORMAL) {
      continue;
    }

    TemplateURLData data = turl->data();
    if (base::FeatureList::IsEnabled(
            syncer::kSeparateLocalAndAccountSearchEngines)) {
      // Don't sync search-engines with no account data, if
      // kSeparateLocalAndAccountSearchEngines flag is enabled.
      if (!turl->GetAccountData().has_value()) {
        continue;
      }
      data = turl->GetAccountData().value();
    }
    // Don't sync autogenerated search engines that the user has never
    // interacted with (if feature is enabled).
    if (IsUntouchedAutogeneratedTemplateURLDataAndShouldNotSync(data)) {
      const bool is_prepopulated_entry = turl->prepopulate_id() != 0;
      base::UmaHistogramBoolean(
          "Sync.SearchEngine.LocalUntouchedAutogenerated."
          "IsPrepopulatedEntry",
          is_prepopulated_entry);
      base::UmaHistogramBoolean(
          "Sync.SearchEngine.LocalUntouchedAutogenerated."
          "IsStarterPackEntry",
          turl->starter_pack_id() != 0);
      // Avoid ignoring prepopulated search engines. See crbug.com/404407977.
      if (!is_prepopulated_entry) {
        continue;
      }
    }
    current_data.push_back(CreateSyncDataFromTemplateURLData(data));
  }

  return current_data;
}

std::optional<syncer::ModelError> TemplateURLService::ProcessSyncChanges(
    const base::Location& from_here,
    const syncer::SyncChangeList& change_list) {
  if (!models_associated_) {
    return syncer::ModelError(
        FROM_HERE, syncer::ModelError::Type::kSearchEngineModelsNotAssociated);
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
            client_.get(), prepopulate_data_resolver_.get(),
            search_terms_data(), existing_turl, iter->sync_data(),
            &new_changes);
    if (!turl) {
      continue;
    }

    const std::string error_msg =
        "ProcessSyncChanges failed on ChangeType " +
        syncer::SyncChange::ChangeTypeToString(iter->change_type());
    if (iter->change_type() == syncer::SyncChange::ACTION_DELETE) {
      if (!existing_turl ||
          (base::FeatureList::IsEnabled(
               syncer::kSeparateLocalAndAccountSearchEngines) &&
           !existing_turl->GetAccountData())) {
        // Can't DELETE a non-existent engine at the account level.
        error = syncer::ModelError(
            FROM_HERE, syncer::ModelError::Type::
                           kSearchEngineDeleteNonExistentAtAccountLevel);
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
      if (base::FeatureList::IsEnabled(
              syncer::kSeparateLocalAndAccountSearchEngines) &&
          existing_turl->GetLocalData()) {
        Update(existing_turl, TemplateURL(*existing_turl->GetLocalData()));
      } else if (existing_turl != GetDefaultSearchProvider()) {
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

      // If flag is enabled, add `data` as account data member instead.
      base::FeatureList::IsEnabled(
          syncer::kSeparateLocalAndAccountSearchEngines)
          ? Add(std::make_unique<TemplateURL>(std::nullopt, data))
          : Add(std::make_unique<TemplateURL>(data));
    } else {
      // Since we've already found |existing_turl| by GUID, this Update() should
      // always return true, but we still don't want to crash if it fails.
      DCHECK(existing_turl);
      bool update_success = Update(existing_turl, *turl);
      DCHECK(update_success);
    }
  }

  // If something went wrong, we want to prematurely exit to avoid pushing
  // inconsistent data to Sync. We return the last error we received.
  if (error) {
    return error;
  }

  LogSyncChangesToHistogram(
      new_changes, "Sync.SearchEngine.ChangesCommittedUponIncrementalUpdate");
  return sync_processor_->ProcessSyncChanges(from_here, new_changes);
}

base::WeakPtr<syncer::SyncableService> TemplateURLService::AsWeakPtr() {
  return weak_ptr_factory_.GetWeakPtr();
}

std::string TemplateURLService::GetClientTag(
    const syncer::EntityData& entity_data) const {
  DCHECK(entity_data.specifics.has_search_engine());
  return entity_data.specifics.search_engine().sync_guid();
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
    return syncer::ModelError(
        FROM_HERE, syncer::ModelError::Type::kSearchEngineLocalDbLoadFailed);
  }

  sync_processor_ = std::move(sync_processor);

  // We do a lot of calls to Add/Remove/ResetTemplateURL here, so ensure we
  // don't step on our own toes.
  base::AutoReset<bool> processing_changes(&processing_syncer_changes_, true);

  Scoper scoper(this);

  // We've started syncing, so set our origin member to the base Sync value.
  // As we move through Sync Code, we may set this to increasingly specific
  // origins so we can tell what exactly caused a DSP change.
  base::AutoReset<DefaultSearchChangeOrigin> change_origin(
      &dsp_change_origin_, DSP_CHANGE_SYNC_UNINTENTIONAL);

  syncer::SyncChangeList new_changes;

  const bool separate_local_and_account_search_engines =
      base::FeatureList::IsEnabled(
          syncer::kSeparateLocalAndAccountSearchEngines);

  // Build maps of our sync GUIDs to syncer::SyncData.
  SyncDataMap local_data_map =
      CreateGUIDToSyncDataMap(GetAllSyncData(syncer::SEARCH_ENGINES));
  CHECK(!separate_local_and_account_search_engines || local_data_map.empty())
      << "No account data should be pre-existing locally.";
  SyncDataMap sync_data_map = CreateGUIDToSyncDataMap(initial_sync_data);

  for (SyncDataMap::const_iterator iter = sync_data_map.begin();
       iter != sync_data_map.end(); ++iter) {
    TemplateURL* local_turl = GetTemplateURLForGUID(iter->first);
    std::unique_ptr<TemplateURL> sync_turl(
        CreateTemplateURLFromTemplateURLAndSyncData(
            client_.get(), prepopulate_data_resolver_.get(),
            search_terms_data(), local_turl, iter->second, &new_changes));
    if (!sync_turl) {
      continue;
    }

    if (base::Contains(pre_sync_deletes_, sync_turl->sync_guid())) {
      // This entry was deleted before the initial sync began (possibly through
      // preprocessing in TemplateURLService's loading code). Ignore it and send
      // an ACTION_DELETE up to the server.
      new_changes.emplace_back(FROM_HERE, syncer::SyncChange::ACTION_DELETE,
                               iter->second);
      UMA_HISTOGRAM_ENUMERATION(kDeleteSyncedEngineHistogramName,
                                DELETE_ENGINE_PRE_SYNC, DELETE_ENGINE_MAX);
      continue;
    }

    if (local_turl) {
      DCHECK(IsFromSync(local_turl, sync_data_map));
      if (separate_local_and_account_search_engines) {
        // `sync_turl` holds both the local data and the account data. Update
        // the saved entry.
        Update(local_turl, *sync_turl);
        continue;
      }
      // This local search engine is already synced. If the timestamp differs
      // from Sync, we need to update locally or to the cloud. Note that if the
      // timestamps are equal, we touch neither.
      if (sync_turl->last_modified() > local_turl->last_modified() ||
          // It is possible that `local_turl` was filtered out in
          // GetAllSyncData() above. In such case, `sync_turl` should win.
          !local_data_map.contains(local_turl->sync_guid())) {
        // We've received an update from Sync. We should replace all synced
        // fields in the local TemplateURL. Note that this includes the
        // TemplateURLID and the TemplateURL may have to be reparsed. This
        // also makes the local data's last_modified timestamp equal to Sync's,
        // avoiding an Update on the next MergeData call.
        Update(local_turl, *sync_turl);
      } else if (sync_turl->last_modified() < local_turl->last_modified()) {
        // Otherwise, we know we have newer data, so update Sync with our
        // data fields.
        new_changes.emplace_back(FROM_HERE, syncer::SyncChange::ACTION_UPDATE,
                                 local_data_map[local_turl->sync_guid()]);
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

  // Avoid committing local data if the flag is enabled.
  if (!separate_local_and_account_search_engines) {
    // The remaining SyncData in local_data_map should be everything that needs
    // to be pushed as ADDs to sync.
    for (SyncDataMap::const_iterator iter = local_data_map.begin();
         iter != local_data_map.end(); ++iter) {
      new_changes.emplace_back(FROM_HERE, syncer::SyncChange::ACTION_ADD,
                               iter->second);
    }
  }

  // Do some post-processing on the change list to ensure that we are sending
  // valid changes to sync_processor_.
  PruneSyncChanges(&sync_data_map, &new_changes);

  LogSyncChangesToHistogram(new_changes,
                            "Sync.SearchEngine.ChangesCommittedUponSyncStart");
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
  CHECK_EQ(type, syncer::SEARCH_ENGINES);
  models_associated_ = false;
  sync_processor_.reset();

  base::AutoReset<bool> processing_changes(&processing_syncer_changes_, true);
  // Cleanup account template urls.
  for (size_t i = 0; i < template_urls_.size();) {
    TemplateURL* turl = template_urls_[i].get();
    // Skip if the turl has no account data.
    if (!turl->GetAccountData()) {
      ++i;
      continue;
    }
    // If turl has local data, remove only the account data. This is done by
    // updating turl with a new TemplateURL containing only the local data
    // instead of just dropping the account data to ensure all the mappings are
    // correctly updated. Else, remove turl.
    base::UmaHistogramBoolean(
        "Sync.SearchEngine.HasLocalDataDuringStopSyncing2",
        turl->GetLocalData().has_value());
    if (turl->GetLocalData()) {
      Update(turl, TemplateURL(*turl->GetLocalData()));
      ++i;
    } else if (turl != GetDefaultSearchProvider()) {
      Remove(turl);
    } else {
      // Copy the account data to local. It is not safe to remove the default
      // search provider. And given that this case should only be reached upon a
      // user explicitly setting the default search engine to this, it should
      // be okay to leave the data (similar to the dual-write case).
      base::UmaHistogramBoolean(
          "Sync.SearchEngine.AccountDefaultSearchEngineCopiedToLocal", true);
      Update(turl, TemplateURL(turl->data()));
      ++i;
    }
  }
}

void TemplateURLService::OnBrowserShutdown(syncer::DataType type) {
  CHECK_EQ(type, syncer::SEARCH_ENGINES);
  models_associated_ = false;
  sync_processor_.reset();
  // Skip removing the account search engines on browser shutdown, as this is
  // not really needed, plus the TemplateURLs will all be regenerated upon
  // browser startup.
}

void TemplateURLService::ProcessTemplateURLChange(
    const base::Location& from_here,
    TemplateURL* turl,
    syncer::SyncChange::SyncChangeType type) {
  DCHECK(turl);

  if (!models_associated_) {
    return;  // Not syncing.
  }

  if (processing_syncer_changes_) {
    return;  // These are changes originating from us. Ignore.
  }

  // Avoid syncing keywords managed by policy.
  if (turl->CreatedByPolicy()) {
    return;
  }

  // Avoid syncing extension-controlled search engines.
  if (turl->type() == TemplateURL::NORMAL_CONTROLLED_BY_EXTENSION) {
    return;
  }

  TemplateURLData data = turl->data();
  if (base::FeatureList::IsEnabled(
          syncer::kSeparateLocalAndAccountSearchEngines) &&
      (type == syncer::SyncChange::ACTION_DELETE ||
       type == syncer::SyncChange::ACTION_UPDATE)) {
    if (!turl->GetAccountData().has_value()) {
      // Nothing to commit if there was no account data to begin with.
      return;
    }
    data = turl->GetAccountData().value();
  }

  // Avoid syncing autogenerated search engines that the user has never
  // interacted with (if feature is enabled).
  const bool is_untouched_autogenerated_turl_and_should_not_sync =
      IsUntouchedAutogeneratedTemplateURLDataAndShouldNotSync(data);
  const std::string_view histogram_suffix =
      SyncChangeTypeToHistogramSuffix(type);
  base::UmaHistogramBoolean(
      base::StrCat(
          {"Sync.SearchEngine.UntouchedAutogenerated", histogram_suffix}),
      is_untouched_autogenerated_turl_and_should_not_sync);
  if (is_untouched_autogenerated_turl_and_should_not_sync) {
    const bool is_prepopulated_entry = turl->prepopulate_id() != 0;
    base::UmaHistogramBoolean(
        base::StringPrintf(
            "Sync.SearchEngine.UntouchedAutogenerated%s.IsPrepopulatedEntry",
            histogram_suffix),
        is_prepopulated_entry);
    base::UmaHistogramBoolean(
        base::StringPrintf(
            "Sync.SearchEngine.UntouchedAutogenerated%s.IsStarterPackEntry",
            histogram_suffix),
        turl->starter_pack_id() != 0);
    // Avoid ignoring prepopulated search engines. See crbug.com/404407977.
    if (!is_prepopulated_entry) {
      return;
    }
  }

  if (base::FeatureList::IsEnabled(
          syncer::kSeparateLocalAndAccountSearchEngines) &&
      type == syncer::SyncChange::ACTION_ADD) {
    // Dual-write active value to local and account.
    turl->CopyActiveValueToLocalAndAccount();
  }

  syncer::SyncData sync_data = CreateSyncDataFromTemplateURLData(data);
  syncer::SyncChangeList changes = {
      syncer::SyncChange(from_here, type, sync_data)};
  sync_processor_->ProcessSyncChanges(FROM_HERE, changes);
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
syncer::SyncData TemplateURLService::CreateSyncDataFromTemplateURLData(
    const TemplateURLData& data) {
  sync_pb::EntitySpecifics specifics;
  sync_pb::SearchEngineSpecifics* se_specifics =
      specifics.mutable_search_engine();

  se_specifics->set_short_name(base::UTF16ToUTF8(data.short_name()));
  se_specifics->set_keyword(base::UTF16ToUTF8(data.keyword()));
  se_specifics->set_favicon_url(data.favicon_url.spec());
  se_specifics->set_url(data.url());
  se_specifics->set_safe_for_autoreplace(data.safe_for_autoreplace);
  se_specifics->set_originating_url(data.originating_url.spec());
  se_specifics->set_date_created(data.date_created.ToInternalValue());
  se_specifics->set_input_encodings(
      base::JoinString(data.input_encodings, ";"));
  se_specifics->set_suggestions_url(data.suggestions_url);
  se_specifics->set_prepopulate_id(data.prepopulate_id);
  if (!data.image_url.empty()) {
    se_specifics->set_image_url(data.image_url);
  }
  se_specifics->set_new_tab_url(data.new_tab_url);
  if (!data.search_url_post_params.empty()) {
    se_specifics->set_search_url_post_params(data.search_url_post_params);
  }
  if (!data.suggestions_url_post_params.empty()) {
    se_specifics->set_suggestions_url_post_params(
        data.suggestions_url_post_params);
  }
  if (!data.image_url_post_params.empty()) {
    se_specifics->set_image_url_post_params(data.image_url_post_params);
  }
  se_specifics->set_last_modified(data.last_modified.ToInternalValue());
  se_specifics->set_sync_guid(data.sync_guid);
  for (const std::string& alternate_url : data.alternate_urls) {
    se_specifics->add_alternate_urls(alternate_url);
  }
  se_specifics->set_is_active(ActiveStatusToSync(data.is_active));
  se_specifics->set_starter_pack_id(data.starter_pack_id);

  return syncer::SyncData::CreateLocalData(se_specifics->sync_guid(),
                                           se_specifics->keyword(), specifics);
}

// static
std::unique_ptr<TemplateURL>
TemplateURLService::CreateTemplateURLFromTemplateURLAndSyncData(
    TemplateURLServiceClient* client,
    const TemplateURLPrepopulateData::Resolver& prepopulate_data_resolver,
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
      template_url_starter_pack_data::kMaxStarterPackId) {
    return nullptr;
  }

  // Autogenerated, un-touched keywords are no longer synced, but may still
  // exist on the server from before. Ignore these.
  // TODO(crbug.com/361374753): After this change is shipped and confirmed to be
  // safe/the intended behavior going forward, update this to delete
  // previously synced autogenerated keywords from the server (as is done
  // above with empty urls, etc.).
  const bool is_untouched_autogenerated_turl_and_should_not_sync =
      IsUntouchedAutogeneratedRemoteTemplateURLAndShouldNotSync(specifics);
  base::UmaHistogramBoolean(
      "Sync.SearchEngine.RemoteSearchEngineIsUntouchedAutogenerated",
      is_untouched_autogenerated_turl_and_should_not_sync);
  if (is_untouched_autogenerated_turl_and_should_not_sync) {
    const bool is_prepopulated_entry = specifics.prepopulate_id() != 0;
    base::UmaHistogramBoolean(
        "Sync.SearchEngine.RemoteUntouchedAutogenerated."
        "IsPrepopulatedEntry",
        is_prepopulated_entry);
    base::UmaHistogramBoolean(
        "Sync.SearchEngine.RemoteUntouchedAutogenerated."
        "IsStarterPackEntry",
        specifics.starter_pack_id() != 0);
    // Avoid ignoring prepopulated search engines. See crbug.com/404407977.
    if (!is_prepopulated_entry) {
      return nullptr;
    }
  }

  TemplateURLData data;
  // If flag is enabled, `data` will be added to a separate account data member.
  // Thus avoid copying from `existing_turl` in this case.
  if (!base::FeatureList::IsEnabled(
          syncer::kSeparateLocalAndAccountSearchEngines) &&
      existing_turl) {
    data = existing_turl->data();
  }
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
  data.input_encodings =
      base::SplitString(specifics.input_encodings(), ";", base::TRIM_WHITESPACE,
                        base::SPLIT_WANT_ALL);
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
  for (int i = 0; i < specifics.alternate_urls_size(); ++i) {
    data.alternate_urls.push_back(specifics.alternate_urls(i));
  }
  data.is_active = ActiveStatusFromSync(specifics.is_active());
  data.starter_pack_id = specifics.starter_pack_id();

  // If this TemplateURL matches a built-in prepopulated template URL, it's
  // possible that sync is trying to modify fields that should not be touched.
  // Revert these fields to the built-in values.
  data = UpdateTemplateURLDataIfPrepopulated(data, prepopulate_data_resolver);

  // If the flag is set, `data` is written to a separate account data. Else it
  // is written to the local data.
  std::unique_ptr<TemplateURL> turl =
      base::FeatureList::IsEnabled(
          syncer::kSeparateLocalAndAccountSearchEngines)
          ? UpdateExistingURLWithAccountData(existing_turl, data)
          : std::make_unique<TemplateURL>(data);

  DCHECK_EQ(TemplateURL::NORMAL, turl->type());
  if (deduped) {
    change_list->push_back(
        syncer::SyncChange(FROM_HERE, syncer::SyncChange::ACTION_UPDATE,
                           CreateSyncDataFromTemplateURLData(data)));
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
      turl->set_keyword(existing_turl->keyword());
    }
  }

  return turl;
}

// static
SyncDataMap TemplateURLService::CreateGUIDToSyncDataMap(
    const syncer::SyncDataList& sync_data) {
  SyncDataMap data_map;
  for (const auto& i : sync_data) {
    data_map[i.GetSpecifics().search_engine().sync_guid()] = i;
  }
  return data_map;
}

void TemplateURLService::Init() {
  if (client_) {
    client_->SetOwner(this);
  }

  pref_change_registrar_.Init(&prefs_.get());
  pref_change_registrar_.Add(
      prefs::kDefaultSearchProviderGUID,
      base::BindRepeating(
          &TemplateURLService::OnDefaultSearchProviderGUIDChanged,
          base::Unretained(this)));

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

  if (template_url->type() == TemplateURL::OMNIBOX_API_EXTENSION) {
    return;
  }

  if (!template_url->sync_guid().empty()) {
    guid_to_turl_.erase(template_url->sync_guid());
    if (postponed_deleted_default_engine_guid_ == template_url->sync_guid()) {
      // `template_url` has been updated locally or removed, discard incoming
      // deletion.
      postponed_deleted_default_engine_guid_.clear();
    }
  }

  if (template_url->CreatedByNonDefaultSearchProviderPolicy()) {
    enterprise_search_keyword_to_turl_.erase(keyword);
  }

  // |provider_map_| is only initialized after loading has completed.
  if (loaded_) {
    provider_map_->Remove(template_url);
  }
}

void TemplateURLService::AddToMaps(TemplateURL* template_url) {
  const std::u16string& keyword = template_url->keyword();
  keyword_to_turl_.insert(std::make_pair(keyword, template_url));

  if (template_url->type() == TemplateURL::OMNIBOX_API_EXTENSION) {
    return;
  }

  if (!template_url->sync_guid().empty()) {
    guid_to_turl_[template_url->sync_guid()] = template_url;
  }

  if (template_url->CreatedByNonDefaultSearchProviderPolicy()) {
    enterprise_search_keyword_to_turl_[keyword] = template_url;
  }

  // |provider_map_| is only initialized after loading has completed.
  if (loaded_) {
    provider_map_->Add(template_url, search_terms_data());
  }
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
  for (auto i = first_invalid; i != urls->end(); ++i) {
    Add(std::move(*i));
  }
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
  ApplyEnterpriseSearchChanges(pre_loading_providers_->TakeSearchEngines());
  pre_loading_providers_.reset();

  if (on_loaded_callback_for_sync_) {
    std::move(on_loaded_callback_for_sync_).Run();
  }

  on_loaded_callbacks_.Notify();
}

bool TemplateURLService::CanAddAutogeneratedKeywordForHost(
    const std::string& host) const {
  const TemplateURLSet* urls = provider_map_->GetURLsForHost(host);
  if (!urls) {
    return true;
  }

  return std::ranges::all_of(*urls, [](const TemplateURL* turl) {
    return turl->safe_for_autoreplace();
  });
}

bool TemplateURLService::Update(TemplateURL* existing_turl,
                                const TemplateURL& new_values) {
  DCHECK(existing_turl);
  if (!Contains(&template_urls_, existing_turl)) {
    return false;
  }

  // Avoid using account data in `new_values` if sync is not running, but not
  // when processing changes from sync.
  const bool should_remove_account_data =
      new_values.GetAccountData() &&
      (!base::FeatureList::IsEnabled(
           syncer::kSeparateLocalAndAccountSearchEngines) ||
       (!models_associated_ && !processing_syncer_changes_));
  // Mark if account data has changed, since it is possible that only the
  // current local data was updated. In such case, avoid sending any update to
  // sync.
  const bool should_send_update_to_sync =
      !base::FeatureList::IsEnabled(
          syncer::kSeparateLocalAndAccountSearchEngines) ||
      ShouldCommitUpdateToAccount(existing_turl->GetAccountData(),
                                  new_values.GetAccountData());
  // It is possible that corresponding local data didn't exist before and now
  // `new_values` writes local data. In such case, an add operation needs to be
  // performed on the database instead of update.
  const bool is_newly_adding_local_data =
      new_values.GetLocalData() && !existing_turl->GetLocalData();

  Scoper scoper(this);
  model_mutated_notification_pending_ = true;

  TemplateURLID previous_id = existing_turl->id();
  RemoveFromMaps(existing_turl);

  // Update existing turl with new values and add back to the map.
  // We don't do any keyword conflict handling here, as TemplateURLService
  // already can pick the best engine out of duplicates. Replaceable duplicates
  // will be culled during next startup's Add() loop. We did this to keep
  // Update() simple: it never fails, and never deletes |existing_engine|.
  if (should_remove_account_data) {
    existing_turl->CopyFrom(TemplateURL(*new_values.GetLocalData()));
  } else {
    existing_turl->CopyFrom(new_values);
  }
  existing_turl->set_id(previous_id);

  AddToMaps(existing_turl);

  if (existing_turl->type() == TemplateURL::NORMAL) {
    if (web_data_service_ && existing_turl->GetLocalData()) {
      if (is_newly_adding_local_data) {
        web_data_service_->AddKeyword(*existing_turl->GetLocalData());
      } else {
        web_data_service_->UpdateKeyword(*existing_turl->GetLocalData());
      }
    }

    if (should_send_update_to_sync) {
      // Inform sync of the update.
      ProcessTemplateURLChange(FROM_HERE, existing_turl,
                               syncer::SyncChange::ACTION_UPDATE);
    }
  }

  return true;
}

bool TemplateURLService::UpdateData(TemplateURL* existing_turl,
                                    TemplateURLData new_data) {
  return IsAccountDataActive(existing_turl)
             ? Update(existing_turl,
                      TemplateURL(existing_turl->GetLocalData(), new_data))
             : Update(existing_turl,
                      TemplateURL(new_data, existing_turl->GetAccountData()));
}

void TemplateURLService::UpdateKeywordSearchTermsForURL(
    const URLVisitedDetails& details) {
  if (!details.url.is_valid()) {
    return;
  }

  // AI mode URLs should not be stored. Otherwise, since they fit the
  // traditional search `TemplateURL`'s URL, those would be incorrectly
  // attributed.
  if (omnibox_feature_configs::AiMode::Get()
          .do_not_show_historic_aim_suggestions &&
      IsGoogleAiModeUrl(details.url)) {
    return;
  }

  const TemplateURLSet* urls_for_host =
      provider_map_->GetURLsForHost(details.url.GetHost());
  if (!urls_for_host) {
    return;
  }

  TemplateURL* visited_url = nullptr;
  for (auto i = urls_for_host->begin(); i != urls_for_host->end(); ++i) {
    TemplateURL& template_url = **i;

    // AI mode keyword should not be attributed. Otherwise, they would be
    // incorrectly attributed by traditional search URLs, which fit the AI mode
    // `TemplateURL`'s URL.
    if (template_url.starter_pack_id() ==
        template_url_starter_pack_data::StarterPackId::kAiMode) {
      continue;
    }

    std::u16string search_terms;
    if (template_url.ExtractSearchTermsFromURL(details.url, search_terms_data(),
                                               &search_terms) &&
        !search_terms.empty()) {
      if (details.is_keyword_transition) {
        // The visit is the result of the user entering a keyword, generate a
        // KEYWORD_GENERATED visit for the KEYWORD so that the keyword typed
        // count is boosted.
        AddTabToSearchVisit(template_url);
      }
      if (client_) {
        client_->SetKeywordSearchTermsForURL(details.url, template_url.id(),
                                             search_terms);
      }
      // Caches the matched TemplateURL so its last_visited could be updated
      // later after iteration.
      // Note: Update() will replace the entry from the container of this
      // iterator, so update here directly will cause an error about it.
      if (!IsCreatedByExtension(template_url)) {
        visited_url = &template_url;
      }
    }
  }
  if (visited_url) {
    UpdateTemplateURLVisitTime(visited_url);
  }
}

void TemplateURLService::AddToUnscopedModeExtensionIds(
    const std::string& extension_id) {
  CHECK(!extension_id.empty());
  unscoped_mode_extension_ids_.insert(extension_id);
}

void TemplateURLService::RemoveFromUnscopedModeExtensionIdsIfPresent(
    const std::string& extension_id) {
  if (unscoped_mode_extension_ids_.contains(extension_id)) {
    unscoped_mode_extension_ids_.erase(extension_id);
  }
}

std::set<std::string> TemplateURLService::GetUnscopedModeExtensionIds() const {
  return unscoped_mode_extension_ids_;
}

void TemplateURLService::UpdateTemplateURLVisitTime(TemplateURL* url) {
  TemplateURLData data(url->data());
  data.last_visited = clock_->Now();
  UpdateData(url, data);
}

void TemplateURLService::AddTabToSearchVisit(const TemplateURL& t_url) {
  // Only add visits for entries the user hasn't modified. If the user modified
  // the entry the keyword may no longer correspond to the host name. It may be
  // possible to do something more sophisticated here, but it's so rare as to
  // not be worth it.
  if (!t_url.safe_for_autoreplace()) {
    return;
  }

  if (!client_) {
    return;
  }

  GURL url(url_formatter::FixupURL(base::UTF16ToUTF8(t_url.keyword()),
                                   std::string()));
  if (!url.is_valid()) {
    return;
  }

  // Synthesize a visit for the keyword. This ensures the url for the keyword is
  // autocompleted even if the user doesn't type the url in directly.
  client_->AddKeywordGeneratedVisit(url);
}

void TemplateURLService::ApplyDefaultSearchChange(
    const TemplateURLData* data,
    DefaultSearchManager::Source source) {
  if (!ApplyDefaultSearchChangeNoMetrics(data, source)) {
    return;
  }

  if (GetDefaultSearchProvider() &&
      GetDefaultSearchProvider()->HasGoogleBaseURLs(search_terms_data()) &&
      !dsp_change_callback_.is_null()) {
    dsp_change_callback_.Run();
  }
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
  if (applying_default_search_engine_change_) {
    return false;
  }
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
      UpdateData(default_search_provider_, update_data);
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
      UpdateData(default_search_provider_, new_data);
    } else {
      new_data.id = kInvalidTemplateURLID;
      default_search_provider_ = Add(std::make_unique<TemplateURL>(new_data));
      DCHECK(default_search_provider_)
          << "Add() to repair the DSE must never fail.";
    }
    if (default_search_provider_) {
      prefs_->SetString(prefs::kDefaultSearchProviderGUID,
                        default_search_provider_->sync_guid());
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

void TemplateURLService::ApplyEnterpriseSearchChanges(
    TemplateURLService::OwnedTemplateURLVector&& policy_search_engines) {
  CHECK(loaded_);

  Scoper scoper(this);

  LogSearchPolicyConflict(policy_search_engines);

  base::flat_set<std::u16string> new_keywords;
  std::ranges::transform(
      policy_search_engines, std::inserter(new_keywords, new_keywords.begin()),
      [](const std::unique_ptr<TemplateURL>& turl) { return turl->keyword(); });

  // Remove old site search entries no longer present in the policy's list.
  //
  // Note: auxiliary `keywords_to_remove` is needed to avoid reentry issues
  //       while removing elements from
  //       `enterprise_search_keyword_to_turl_`.
  //
  // Note: This can be made more idiomatic once Chromium style allows
  //       `std::views::keys`:
  //       std::copy_if(
  //           std::views::keys(enterprise_search_keyword_to_turl_),
  //           std::inserter(keywords_to_remove, keywords_to_remove.begin()),
  //           [new_keywords] (const std::u16string& keyword) {
  //             new_keywords.find(keyword) == new_keywords.end()
  //           });
  base::flat_set<std::u16string> keywords_to_remove;
  for (auto [keyword, _] : enterprise_search_keyword_to_turl_) {
    if (new_keywords.find(keyword) == new_keywords.end()) {
      keywords_to_remove.insert(keyword);
    }
  }
  std::ranges::for_each(keywords_to_remove,
                        [this](const std::u16string& keyword) {
                          Remove(enterprise_search_keyword_to_turl_[keyword]);
                        });

  // Either add new site search entries or update existing ones if necessary.
  for (auto& search_engine : policy_search_engines) {
    const std::u16string& keyword = search_engine->keyword();
    auto it = enterprise_search_keyword_to_turl_.find(keyword);
    if (it == enterprise_search_keyword_to_turl_.end()) {
      Add(std::move(search_engine), /*newly_adding=*/true);
    } else if (ShouldMergeEnterpriseSearchEngines(
                   /*existing_turl=*/*it->second,
                   /*new_values=*/*search_engine)) {
      UpdateData(/*existing_turl=*/it->second,
                 /*new_data=*/MergeEnterpriseSearchEngines(
                     /*existing_data=*/it->second->data(),
                     /*new_values=*/*search_engine));
    }
  }
}

void TemplateURLService::EnterpriseSearchChanged(
    OwnedTemplateURLDataVector&& policy_search_engines) {
  OwnedTemplateURLVector turl_search_engines;
  for (const std::unique_ptr<TemplateURLData>& it : policy_search_engines) {
    turl_search_engines.push_back(
        std::make_unique<TemplateURL>(*it, TemplateURL::NORMAL));
  }

  if (loaded_) {
    ApplyEnterpriseSearchChanges(std::move(turl_search_engines));
  } else {
    pre_loading_providers_->set_search_engines(std::move(turl_search_engines));
  }
}

TemplateURL* TemplateURLService::Add(std::unique_ptr<TemplateURL> template_url,
                                     bool newly_adding) {
  DCHECK(template_url);

  Scoper scoper(this);

  // Remove account data from `template_url` if sync is not running yet, but not
  // when processing sync changes.
  if ((!base::FeatureList::IsEnabled(
           syncer::kSeparateLocalAndAccountSearchEngines) ||
       (!models_associated_ && !processing_syncer_changes_)) &&
      template_url->GetAccountData()) {
    if (!template_url->GetLocalData()) {
      return nullptr;
    }
    template_url = std::make_unique<TemplateURL>(template_url->GetLocalData(),
                                                 std::nullopt);
  }

  if (newly_adding) {
    DCHECK_EQ(kInvalidTemplateURLID, template_url->id());
    DCHECK(!Contains(&template_urls_, template_url.get()));
    template_url->set_id(++next_id_);
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
    base::UmaHistogramBoolean("Sync.SearchEngine.AddedKeywordHasAccountData",
                              template_url_ptr->GetAccountData().has_value());
    // Inform sync of the addition. Note that this will assign a GUID to
    // template_url and add it to the guid_to_turl_.
    ProcessTemplateURLChange(FROM_HERE, template_url_ptr,
                             syncer::SyncChange::ACTION_ADD);

    if (web_data_service_ && template_url_ptr->GetLocalData()) {
      web_data_service_->AddKeyword(*template_url_ptr->GetLocalData());
    }
  }

  if (template_url_ptr) {
    model_mutated_notification_pending_ = true;
  }

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
    if (template_url->CreatedByDefaultSearchProviderPolicy()) {
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
    if (new_data.sync_guid.empty()) {
      new_data.GenerateSyncGUID();
    }
    new_data.policy_origin =
        TemplateURLData::PolicyOrigin::kDefaultSearchProvider;
    new_data.enforced_by_policy = is_mandatory;
    new_data.safe_for_autoreplace = false;
    new_data.is_active = TemplateURLData::ActiveStatus::kTrue;
    std::unique_ptr<TemplateURL> new_dse_ptr =
        std::make_unique<TemplateURL>(new_data);
    TemplateURL* new_dse = new_dse_ptr.get();
    if (Add(std::move(new_dse_ptr))) {
      default_search_provider_ = new_dse;
    }
  }
}

void TemplateURLService::ResetTemplateURLGUID(TemplateURL* url,
                                              const std::string& guid) {
  DCHECK(loaded_);
  DCHECK(!guid.empty());

  TemplateURLData data(url->data());
  data.sync_guid = guid;
  UpdateData(url, data);
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
    // engines selected as part of regulatory program, as those also seem
    // local-only and should not be merged into Synced engines.
    // crbug.com/1414224.
    if (local_turl->type() == TemplateURL::NORMAL &&
        !local_turl->CreatedByPolicy() &&
        !local_turl->CreatedByRegulatoryProgram()) {
      local_duplicates.push_back(local_turl);
    }
  }
  std::ranges::sort(local_duplicates, [&](const auto& a, const auto& b) {
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

    // `conflicting_turl` is not yet known to Sync. Merge only with the best
    // match.
    // TODO(crbug.com/374903497): Take into account the below conflict
    // resolution.
    if (base::FeatureList::IsEnabled(
            syncer::kSeparateLocalAndAccountSearchEngines)) {
      const bool is_default_search_provider =
          conflicting_turl == GetDefaultSearchProvider();
      base::UmaHistogramBoolean(
          "Sync.SearchEngine.DuplicateIsDefaultSearchProvider",
          is_default_search_provider);
      // Skip overriding the default search provider.
      if (is_default_search_provider) {
        ResetTemplateURLGUID(conflicting_turl, sync_turl->sync_guid());
      } else {
        Update(conflicting_turl, *UpdateExistingURLWithAccountData(
                                     conflicting_turl, sync_turl->data()));
      }
      should_add_sync_turl = false;
      break;
    }

    // |conflicting_turl| is not yet known to Sync. If it is better, then we
    // want to transfer its values up to sync. Otherwise, we remove it and
    // allow the entry from Sync to overtake it in the model.
    const std::string guid = conflicting_turl->sync_guid();
    if (conflicting_turl == GetDefaultSearchProvider() ||
        conflicting_turl->IsBetterThanConflictingEngine(sync_turl)) {
      ResetTemplateURLGUID(conflicting_turl, sync_turl->sync_guid());

      syncer::SyncData updated_sync_data =
          CreateSyncDataFromTemplateURLData(conflicting_turl->data());
      change_list->push_back(
          syncer::SyncChange(FROM_HERE, syncer::SyncChange::ACTION_UPDATE,
                             std::move(updated_sync_data)));

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
      // If flag is enabled, keep account data alongside the conflicting local
      // data.
      if (base::FeatureList::IsEnabled(
              syncer::kSeparateLocalAndAccountSearchEngines)) {
        // Update default search provider guid if the conflicting turl is the
        // default search provider.
        if (conflicting_built_in_turl == default_search_provider_ &&
            prefs_->GetString(prefs::kDefaultSearchProviderGUID) ==
                default_search_provider_->sync_guid()) {
          prefs_->SetString(prefs::kDefaultSearchProviderGUID,
                            sync_turl->sync_guid());
        }
        Update(conflicting_built_in_turl,
               *UpdateExistingURLWithAccountData(conflicting_built_in_turl,
                                                 sync_turl->data()));
        should_add_sync_turl = false;
      } else {
        std::string guid = conflicting_built_in_turl->sync_guid();
        if (conflicting_built_in_turl == default_search_provider_) {
          bool pref_matched =
              prefs_->GetString(prefs::kDefaultSearchProviderGUID) ==
              default_search_provider_->sync_guid();
          // Update the existing engine in-place.
          Update(default_search_provider_, TemplateURL(sync_turl->data()));
          // If prefs::kSyncedDefaultSearchProviderGUID matched
          // |default_search_provider_|'s GUID before, then update it to match
          // its new GUID. If the pref didn't match before, then it probably
          // refers to a new search engine from Sync which just hasn't been
          // added locally yet, so leave it alone in that case.
          if (pref_matched) {
            prefs_->SetString(prefs::kDefaultSearchProviderGUID,
                              default_search_provider_->sync_guid());
          }

          should_add_sync_turl = false;
        } else {
          Remove(conflicting_built_in_turl);
        }
        // Remove the local data so it isn't written to sync.
        local_data->erase(guid);
      }
    }
  }

  if (should_add_sync_turl) {
    // Force the local ID to kInvalidTemplateURLID so we can add it.
    TemplateURLData data(sync_turl->data());
    data.id = kInvalidTemplateURLID;
    // If flag is enabled, `data` is added as account data. Else it is set as
    // local data in the new TemplateURL.
    std::unique_ptr<TemplateURL> added_ptr =
        base::FeatureList::IsEnabled(
            syncer::kSeparateLocalAndAccountSearchEngines)
            ? std::make_unique<TemplateURL>(std::nullopt, data)
            : std::make_unique<TemplateURL>(data);
    base::AutoReset<DefaultSearchChangeOrigin> change_origin(
        &dsp_change_origin_, DSP_CHANGE_SYNC_ADD);
    Add(std::move(added_ptr));
  }
}

void TemplateURLService::PatchMissingSyncGUIDs(
    OwnedTemplateURLVector* template_urls) {
  DCHECK(template_urls);
  for (auto& template_url : *template_urls) {
    DCHECK(template_url);
    if (template_url->sync_guid().empty() &&
        (template_url->type() == TemplateURL::NORMAL)) {
      template_url->GenerateSyncGUID();
      if (web_data_service_) {
        web_data_service_->UpdateKeyword(template_url->data());
      }
    }
  }
}

void TemplateURLService::OnDefaultSearchProviderGUIDChanged() {
  base::AutoReset<DefaultSearchChangeOrigin> change_origin(
      &dsp_change_origin_, DSP_CHANGE_SYNC_PREF);

  std::string new_guid = prefs_->GetString(prefs::kDefaultSearchProviderGUID);
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
      turl->set_is_active(TemplateURLData::ActiveStatus::kTrue);
      turl->set_safe_for_autoreplace(false);
      if (web_data_service_) {
        web_data_service_->UpdateKeyword(turl->data());
      }
    }
  }
}

TemplateURL* TemplateURLService::FindPrepopulatedTemplateURL(
    int prepopulated_id) {
  DCHECK(prepopulated_id);
  for (const auto& turl : template_urls_) {
    if (turl->prepopulate_id() == prepopulated_id) {
      return turl.get();
    }
  }
  return nullptr;
}

TemplateURL* TemplateURLService::FindStarterPackTemplateURL(
    int starter_pack_id) {
  DCHECK(starter_pack_id);
  for (const auto& turl : template_urls_) {
    if (turl->starter_pack_id() == starter_pack_id) {
      return turl.get();
    }
  }
  return nullptr;
}

TemplateURL* TemplateURLService::FindTemplateURLForExtension(
    const std::string& extension_id,
    TemplateURL::Type type) {
  DCHECK_NE(TemplateURL::NORMAL, type);
  for (const auto& turl : template_urls_) {
    if (turl->type() == type && turl->GetExtensionId() == extension_id) {
      return turl.get();
    }
  }
  return nullptr;
}

TemplateURL* TemplateURLService::FindMatchingDefaultExtensionTemplateURL(
    const TemplateURLData& data) {
  for (const auto& turl : template_urls_) {
    if (turl->type() == TemplateURL::NORMAL_CONTROLLED_BY_EXTENSION &&
        turl->GetExtensionInfo()->wants_to_be_default_engine &&
        TemplateURL::MatchesData(turl.get(), &data, search_terms_data())) {
      return turl.get();
    }
  }
  return nullptr;
}

bool TemplateURLService::RemoveDuplicateReplaceableEnginesOf(
    TemplateURL* candidate) {
  DCHECK(candidate);

  // Do not replace existing search engines if `candidate` was created by the
  // policy.
  if (candidate->CreatedByNonDefaultSearchProviderPolicy()) {
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
    // deleting the DSE until it's no longer set as the DSE, analogous to how we
    // handle ACTION_DELETE of the DSE in ProcessSyncChanges().
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
  if (!default_provider) {
    return false;
  }

  return turl->sync_guid() == default_provider->sync_guid();
}

std::unique_ptr<EnterpriseSearchManager>
TemplateURLService::GetEnterpriseSearchManager(PrefService* prefs) {
#if BUILDFLAG(IS_WIN) || BUILDFLAG(IS_MAC) || BUILDFLAG(IS_LINUX) || \
    BUILDFLAG(IS_CHROMEOS)
  return std::make_unique<EnterpriseSearchManager>(
      prefs, base::BindRepeating(&TemplateURLService::EnterpriseSearchChanged,
                                 base::Unretained(this)));
#else
  return nullptr;
#endif
}

void TemplateURLService::AddOverriddenKeywordForTemplateURL(
    const TemplateURL* template_url) {
  CHECK(template_url && template_url->CanPolicyBeOverridden());
  if (enterprise_search_manager_) {
    enterprise_search_manager_->AddOverriddenKeyword(
        base::UTF16ToUTF8(template_url->keyword()));
  }
}

void TemplateURLService::LogSearchPolicyConflict(
    const TemplateURLService::OwnedTemplateURLVector& policy_search_engines) {
  if (policy_search_engines.empty()) {
    // No need to record conflict histograms if the SearchSettings policy
    // doesn't create any search engine.
    return;
  }

  bool has_conflict_with_featured = false;
  bool has_conflict_with_non_featured = false;
  for (const auto& policy_turl : policy_search_engines) {
    const std::u16string& keyword = policy_turl->keyword();
    CHECK(!keyword.empty());

    const auto match_range = keyword_to_turl_.equal_range(keyword);
    bool conflicts_with_active =
        std::any_of(match_range.first, match_range.second,
                    [](const KeywordToTURL::value_type& entry) {
                      return !entry.second->CreatedByPolicy() &&
                             !entry.second->safe_for_autoreplace();
                    });
    SearchPolicyConflictType type =
        conflicts_with_active
            ? (policy_turl->featured_by_policy()
                   ? SearchPolicyConflictType::kWithFeatured
                   : SearchPolicyConflictType::kWithNonFeatured)
            : SearchPolicyConflictType::kNone;
    base::UmaHistogramEnumeration(kSearchPolicyConflictCountHistogramName,
                                  type);

    has_conflict_with_featured |=
        type == SearchPolicyConflictType::kWithFeatured;
    has_conflict_with_non_featured |=
        type == SearchPolicyConflictType::kWithNonFeatured;
  }

  base::UmaHistogramBoolean(kSearchPolicyHasConflictWithFeaturedHistogramName,
                            has_conflict_with_featured);
  base::UmaHistogramBoolean(
      kSearchPolicyHasConflictWithNonFeaturedHistogramName,
      has_conflict_with_non_featured);
}
