// Copyright 2014 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/search_engines/util.h"

#include <stddef.h>
#include <stdint.h>

#include <limits>
#include <map>
#include <set>
#include <string>
#include <unordered_map>
#include <vector>

#include "base/check_is_test.h"
#include "base/check_op.h"
#include "base/feature_list.h"
#include "base/not_fatal_until.h"
#include "base/ranges/algorithm.h"
#include "base/time/time.h"
#include "components/country_codes/country_codes.h"
#include "components/prefs/pref_service.h"
#include "components/search_engines/keyword_web_data_service.h"
#include "components/search_engines/search_engine_choice/search_engine_choice_service.h"
#include "components/search_engines/search_engine_choice/search_engine_choice_utils.h"
#include "components/search_engines/search_engines_pref_names.h"
#include "components/search_engines/search_engines_switches.h"
#include "components/search_engines/template_url.h"
#include "components/search_engines/template_url_prepopulate_data.h"
#include "components/search_engines/template_url_service.h"
#include "components/search_engines/template_url_starter_pack_data.h"

namespace {

// Indicates whether updates to the search engines database are needed.
struct MergeEngineRequirements {
  // `metadata.HasBuiltinKeywordUpdate()` and
  // `metadata.HasStarterPackUpdate()` indicate the status for the
  // two types of search engines, and when they are `true`, individual fields
  // will contain the associated metadata that should be also added to the
  // database.
  WDKeywordsResult::Metadata metadata;

  // The status to which `prefs::kDefaultSearchProviderKeywordsUseExtendedList`
  // should be set.
  enum class ShouldKeywordsUseExtendedList { kUnknown, kYes, kNo };
  ShouldKeywordsUseExtendedList should_keywords_use_extended_list =
      ShouldKeywordsUseExtendedList::kUnknown;
};

MergeEngineRequirements ComputeMergeEnginesRequirements(
    PrefService* prefs,
    search_engines::SearchEngineChoiceService* search_engine_choice_service,
    const WDKeywordsResult::Metadata& keywords_metadata) {
  if (!prefs) {
    CHECK_IS_TEST();
    return {};
  }
  if (!search_engine_choice_service) {
    CHECK_IS_TEST();
    return {};
  }

  const int prepopulate_resource_keyword_version =
      TemplateURLPrepopulateData::GetDataVersion(prefs);
  const int country_id = search_engine_choice_service->GetCountryId();
  const bool should_keywords_use_extended_list =
      search_engines::IsEeaChoiceCountry(country_id);

  bool update_builtin_keywords;
  if (search_engines::HasSearchEngineCountryListOverride()) {
    // The search engine list is being explicitly overridden, so also force
    // recomputing it for the keywords database.
    update_builtin_keywords = true;
  } else if (keywords_metadata.builtin_keyword_data_version >
             prepopulate_resource_keyword_version) {
    // The version in the database is more recent than the version in the Chrome
    // binary. Downgrades are not supported, so don't update it.
    update_builtin_keywords = false;
  } else if (keywords_metadata.builtin_keyword_data_version <
             prepopulate_resource_keyword_version) {
    // The built-in data from `prepopulated_engines.json` has been updated.
    update_builtin_keywords = true;
  } else if (keywords_metadata.builtin_keyword_country != 0 &&
             keywords_metadata.builtin_keyword_country != country_id) {
    // The country associated with the profile has changed.
    // We skip cases where the country was not previously set to avoid
    // unnecessary churn. We expect that by the time this might matter, the
    // client will have this data populated when the search engine choice
    // feature gets enabled.
    update_builtin_keywords = true;
  } else if (prefs->GetBoolean(
                 prefs::kDefaultSearchProviderKeywordsUseExtendedList) !=
             should_keywords_use_extended_list) {
    // The state of the search engine choice feature has changed.
    // We started writing the pref while we were not checking the country
    // before. Once the feature flag is removed, we can clean up this pref.
    update_builtin_keywords = true;
  } else {
    update_builtin_keywords = false;
  }

  MergeEngineRequirements merge_requirements;

  if (update_builtin_keywords) {
    merge_requirements.metadata.builtin_keyword_data_version =
        prepopulate_resource_keyword_version;
    merge_requirements.metadata.builtin_keyword_country = country_id;
    merge_requirements.should_keywords_use_extended_list =
        should_keywords_use_extended_list
            ? MergeEngineRequirements::ShouldKeywordsUseExtendedList::kYes
            : MergeEngineRequirements::ShouldKeywordsUseExtendedList::kNo;
  }

  const int starter_pack_data_version =
      TemplateURLStarterPackData::GetDataVersion();
  if (keywords_metadata.starter_pack_version < starter_pack_data_version) {
    merge_requirements.metadata.starter_pack_version =
        starter_pack_data_version;
  }

  return merge_requirements;
}

}  // namespace

std::u16string GetDefaultSearchEngineName(TemplateURLService* service) {
  DCHECK(service);
  const TemplateURL* const default_provider =
      service->GetDefaultSearchProvider();
  if (!default_provider) {
    // TODO(cpu): bug 1187517. It is possible to have no default provider.
    // returning an empty string is a stopgap measure for the crash
    // http://code.google.com/p/chromium/issues/detail?id=2573
    return std::u16string();
  }
  return default_provider->short_name();
}

GURL GetDefaultSearchURLForSearchTerms(TemplateURLService* service,
                                       const std::u16string& terms) {
  DCHECK(service);
  const TemplateURL* default_provider = service->GetDefaultSearchProvider();
  if (!default_provider)
    return GURL();
  const TemplateURLRef& search_url = default_provider->url_ref();
  DCHECK(search_url.SupportsReplacement(service->search_terms_data()));
  TemplateURLRef::SearchTermsArgs search_terms_args(terms);
  search_terms_args.append_extra_query_params_from_command_line = true;
  return GURL(search_url.ReplaceSearchTerms(search_terms_args,
                                            service->search_terms_data()));
}

void RemoveDuplicatePrepopulateIDs(
    KeywordWebDataService* service,
    const std::vector<std::unique_ptr<TemplateURLData>>& prepopulated_urls,
    TemplateURL* default_search_provider,
    TemplateURLService::OwnedTemplateURLVector* template_urls,
    const SearchTermsData& search_terms_data,
    std::set<std::string>* removed_keyword_guids) {
  DCHECK(template_urls);
  TemplateURLService::OwnedTemplateURLVector checked_urls;

  // For convenience construct an ID->TemplateURL* map from |prepopulated_urls|.
  std::map<int, TemplateURLData*> prepopulated_url_map;
  for (const auto& url : prepopulated_urls)
    prepopulated_url_map[url->prepopulate_id] = url.get();

  constexpr size_t invalid_index = std::numeric_limits<size_t>::max();
  // A helper structure for deduplicating elements with the same prepopulate_id.
  struct DuplicationData {
    DuplicationData() : index_representative(invalid_index) {}

    // The index into checked_urls at which the best representative is stored.
    size_t index_representative;

    // Proper duplicates for consideration during selection phase.  This
    // does not include the representative stored in checked_urls.
    TemplateURLService::OwnedTemplateURLVector duplicates;
  };
  // Map from prepopulate_id to data for deduplication and selection.
  std::unordered_map<int, DuplicationData> duplication_map;

  const auto has_default_search_keyword = [&](const auto& turl) {
    return default_search_provider &&
           (default_search_provider->prepopulate_id() ==
            turl->prepopulate_id()) &&
           default_search_provider->HasSameKeywordAs(turl->data(),
                                                     search_terms_data);
  };

  // Deduplication phase: move elements into new vector, preserving order while
  // gathering duplicates into separate container for selection.
  for (auto& turl : *template_urls) {
    const int prepopulate_id = turl->prepopulate_id();
    if (prepopulate_id) {
      auto& duplication_data = duplication_map[prepopulate_id];
      if (duplication_data.index_representative == invalid_index) {
        // This is the first found.
        duplication_data.index_representative = checked_urls.size();
        checked_urls.push_back(std::move(turl));
      } else {
        // This is a duplicate.
        duplication_data.duplicates.push_back(std::move(turl));
      }
    } else {
      checked_urls.push_back(std::move(turl));
    }
  }

  // Selection and cleanup phase: swap out elements if necessary to ensure new
  // vector contains only the best representative for each prepopulate_id.
  // Then delete the remaining duplicates.
  for (auto& id_data : duplication_map) {
    const auto prepopulated_url = prepopulated_url_map.find(id_data.first);
    const auto has_prepopulated_keyword = [&](const auto& turl) {
      return (prepopulated_url != prepopulated_url_map.end()) &&
             turl->HasSameKeywordAs(*prepopulated_url->second,
                                    search_terms_data);
    };

    // If the user-selected DSE is a prepopulated engine its properties will
    // either come from the prepopulation origin or from the user preferences
    // file (see DefaultSearchManager). Those properties will end up
    // overwriting whatever we load now anyway. If we are eliminating
    // duplicates, then, we err on the side of keeping the thing that looks
    // more like the value we will end up with in the end.
    // Otherwise, a URL is best if it matches the prepopulated data's keyword;
    // if none match, just fall back to using the one with the lowest ID.
    auto& best = checked_urls[id_data.second.index_representative];
    if (!has_default_search_keyword(best)) {
      bool matched_keyword = has_prepopulated_keyword(best);
      for (auto& duplicate : id_data.second.duplicates) {
        if (has_default_search_keyword(duplicate)) {
          best.swap(duplicate);
          break;
        } else if (matched_keyword) {
          continue;
        } else if (has_prepopulated_keyword(duplicate)) {
          best.swap(duplicate);
          matched_keyword = true;
        } else if (duplicate->id() < best->id()) {
          best.swap(duplicate);
        }
      }
    }

    // Clean up what's left.
    for (const auto& duplicate : id_data.second.duplicates) {
      if (service) {
        service->RemoveKeyword(duplicate->id());
        if (removed_keyword_guids)
          removed_keyword_guids->insert(duplicate->sync_guid());
      }
    }
  }

  // Return the checked URLs.
  template_urls->swap(checked_urls);
}

// Returns the TemplateURL with id specified from the list of TemplateURLs.
// If not found, returns NULL.
TemplateURL* GetTemplateURLByID(
    const TemplateURLService::TemplateURLVector& template_urls,
    int64_t id) {
  for (auto i(template_urls.begin()); i != template_urls.end(); ++i) {
    if ((*i)->id() == id) {
      return *i;
    }
  }
  return nullptr;
}

TemplateURL* FindURLByPrepopulateID(
    const TemplateURLService::TemplateURLVector& template_urls,
    int prepopulate_id) {
  for (auto i = template_urls.begin(); i < template_urls.end(); ++i) {
    if ((*i)->prepopulate_id() == prepopulate_id)
      return *i;
  }
  return nullptr;
}

void MergeIntoEngineData(const TemplateURL* original_turl,
                         TemplateURLData* url_to_update,
                         TemplateURLMergeOption merge_option) {
  DCHECK(original_turl->prepopulate_id() == 0 ||
         original_turl->prepopulate_id() == url_to_update->prepopulate_id);
  DCHECK(original_turl->starter_pack_id() == 0 ||
         original_turl->starter_pack_id() == url_to_update->starter_pack_id);
  // When the user modified search engine's properties or search engine is
  // imported from Play API data we need to preserve certain search engine
  // properties from overriding with prepopulated data.
  bool preserve_user_edits =
      (merge_option != TemplateURLMergeOption::kOverwriteUserEdits &&
       (!original_turl->safe_for_autoreplace() ||
        original_turl->created_from_play_api()));
  if (preserve_user_edits) {
    url_to_update->safe_for_autoreplace = original_turl->safe_for_autoreplace();
    url_to_update->SetShortName(original_turl->short_name());
    url_to_update->SetKeyword(original_turl->keyword());
    if (original_turl->created_from_play_api()) {
      // TODO(crbug.com/40646573): Search url from Play API might contain
      // attribution info and therefore should be preserved through prepopulated
      // data update. In the future we might decide to take different approach
      // to pass attribution info to search providers.
      url_to_update->SetURL(original_turl->url());
    }
  }
  url_to_update->id = original_turl->id();
  url_to_update->sync_guid = original_turl->sync_guid();
  url_to_update->date_created = original_turl->date_created();
  url_to_update->last_modified = original_turl->last_modified();
  url_to_update->created_from_play_api = original_turl->created_from_play_api();
}

ActionsFromCurrentData::ActionsFromCurrentData() = default;

ActionsFromCurrentData::ActionsFromCurrentData(
    const ActionsFromCurrentData& other) = default;

ActionsFromCurrentData::~ActionsFromCurrentData() = default;

void MergeEnginesFromPrepopulateData(
    KeywordWebDataService* service,
    std::vector<std::unique_ptr<TemplateURLData>>* prepopulated_urls,
    TemplateURLService::OwnedTemplateURLVector* template_urls,
    TemplateURL* default_search_provider,
    std::set<std::string>* removed_keyword_guids) {
  DCHECK(prepopulated_urls);
  DCHECK(template_urls);

  ActionsFromCurrentData actions(CreateActionsFromCurrentPrepopulateData(
      prepopulated_urls, *template_urls, default_search_provider));

  ApplyActionsFromCurrentData(actions, service, template_urls,
                              default_search_provider, removed_keyword_guids);
}

ActionsFromCurrentData CreateActionsFromCurrentPrepopulateData(
    std::vector<std::unique_ptr<TemplateURLData>>* prepopulated_urls,
    const TemplateURLService::OwnedTemplateURLVector& existing_urls,
    const TemplateURL* default_search_provider) {
  // Create a map to hold all provided |template_urls| that originally came from
  // prepopulate data (i.e. have a non-zero prepopulate_id()).
  TemplateURL* play_api_turl = nullptr;
  std::map<int, TemplateURL*> id_to_turl;
  for (auto& turl : existing_urls) {
    if (turl->created_from_play_api()) {
      DCHECK_EQ(nullptr, play_api_turl);
      play_api_turl = turl.get();
    }
    int prepopulate_id = turl->prepopulate_id();
    if (prepopulate_id > 0)
      id_to_turl[prepopulate_id] = turl.get();
  }

  // For each current prepopulated URL, check whether |template_urls| contained
  // a matching prepopulated URL.  If so, update the passed-in URL to match the
  // current data.  (If the passed-in URL was user-edited, we persist the user's
  // name and keyword.)  If not, add the prepopulated URL.
  ActionsFromCurrentData actions;
  for (auto& prepopulated_url : *prepopulated_urls) {
    const int prepopulated_id = prepopulated_url->prepopulate_id;
    DCHECK_NE(0, prepopulated_id);

    auto existing_url_iter = id_to_turl.find(prepopulated_id);
    TemplateURL* existing_url = nullptr;
    if (existing_url_iter != id_to_turl.end()) {
      existing_url = existing_url_iter->second;
      id_to_turl.erase(existing_url_iter);
    } else if (play_api_turl &&
               play_api_turl->keyword() == prepopulated_url->keyword()) {
      existing_url = play_api_turl;
    }

    if (existing_url != nullptr) {
      // Update the data store with the new prepopulated data. Preserve user
      // edits to the name and keyword.
      MergeIntoEngineData(existing_url, prepopulated_url.get());
      // Update last_modified to ensure that if this entry is later merged with
      // entries from Sync, the conflict resolution logic knows that this was
      // updated and propagates the new values to the server.
      prepopulated_url->last_modified = base::Time::Now();
      actions.edited_engines.push_back({existing_url, *prepopulated_url});
    } else {
      actions.added_engines.push_back(*prepopulated_url);
    }
  }

  // The block above removed all the URLs from the |id_to_turl| map that were
  // found in the prepopulate data.  Any remaining URLs that haven't been
  // user-edited or made default can be removed from the data store.
  // We assume that this entry is equivalent to the DSE if its prepopulate ID
  // and keyword both match. If the prepopulate ID _does_ match all properties
  // will be replaced with those from |default_search_provider| anyway.
  for (auto& i : id_to_turl) {
    TemplateURL* template_url = i.second;
    if ((template_url->safe_for_autoreplace()) &&
        (!default_search_provider ||
         (template_url->prepopulate_id() !=
          default_search_provider->prepopulate_id()) ||
         (template_url->keyword() != default_search_provider->keyword()))) {
      if (template_url->created_from_play_api()) {
        // Don't remove the entry created from Play API. Just reset
        // prepopulate_id for it.
        TemplateURLData data = template_url->data();
        data.prepopulate_id = 0;
        actions.edited_engines.push_back({template_url, data});
      } else {
        actions.removed_engines.push_back(template_url);
      }
    }
  }

  return actions;
}

const std::string& GetDefaultSearchProviderGuidFromPrefs(PrefService& prefs) {
  return base::FeatureList::IsEnabled(switches::kSearchEngineChoiceTrigger)
             ? prefs.GetString(prefs::kDefaultSearchProviderGUID)
             : prefs.GetString(prefs::kSyncedDefaultSearchProviderGUID);
}

void SetDefaultSearchProviderGuidToPrefs(PrefService& prefs,
                                         const std::string& value) {
  prefs.SetString(prefs::kSyncedDefaultSearchProviderGUID, value);
  if (base::FeatureList::IsEnabled(switches::kSearchEngineChoiceTrigger)) {
    prefs.SetString(prefs::kDefaultSearchProviderGUID, value);
  }
}

void MergeEnginesFromStarterPackData(
    KeywordWebDataService* service,
    TemplateURLService::OwnedTemplateURLVector* template_urls,
    TemplateURL* default_search_provider,
    std::set<std::string>* removed_keyword_guids,
    TemplateURLMergeOption merge_option) {
  DCHECK(template_urls);

  std::vector<std::unique_ptr<TemplateURLData>> starter_pack_urls =
      TemplateURLStarterPackData::GetStarterPackEngines();

  ActionsFromCurrentData actions(CreateActionsFromCurrentStarterPackData(
      &starter_pack_urls, *template_urls, merge_option));

  ApplyActionsFromCurrentData(actions, service, template_urls,
                              default_search_provider, removed_keyword_guids);
}

ActionsFromCurrentData CreateActionsFromCurrentStarterPackData(
    std::vector<std::unique_ptr<TemplateURLData>>* starter_pack_urls,
    const TemplateURLService::OwnedTemplateURLVector& existing_urls,
    TemplateURLMergeOption merge_option) {
  // Create a map to hold all provided |template_urls| that originally came from
  // starter_pack data (i.e. have a non-zero starter_pack_id()).
  std::map<int, TemplateURL*> id_to_turl;
  for (auto& turl : existing_urls) {
    int starter_pack_id = turl->starter_pack_id();
    if (starter_pack_id > 0)
      id_to_turl[starter_pack_id] = turl.get();
  }

  // For each current starter pack URL, check whether |template_urls| contained
  // a matching starter pack URL.  If so, update the passed-in URL to match the
  // current data.  (If the passed-in URL was user-edited, we persist the user's
  // name and keyword.)  If not, add the prepopulated URL.
  ActionsFromCurrentData actions;
  for (auto& url : *starter_pack_urls) {
    const int starter_pack_id = url->starter_pack_id;
    DCHECK_NE(0, starter_pack_id);

    auto existing_url_iter = id_to_turl.find(starter_pack_id);
    TemplateURL* existing_url = nullptr;
    if (existing_url_iter != id_to_turl.end()) {
      existing_url = existing_url_iter->second;
      id_to_turl.erase(existing_url_iter);
    }

    if (existing_url != nullptr) {
      // Update the data store with the new prepopulated data. Preserve user
      // edits to the name and keyword unless `merge_option` is set to
      // kOverwriteUserEdits.
      MergeIntoEngineData(existing_url, url.get(), merge_option);
      // Update last_modified to ensure that if this entry is later merged with
      // entries from Sync, the conflict resolution logic knows that this was
      // updated and propagates the new values to the server.
      url->last_modified = base::Time::Now();
      actions.edited_engines.push_back({existing_url, *url});
    } else {
      actions.added_engines.push_back(*url);
    }
  }

  // The block above removed all the URLs from the |id_to_turl| map that were
  // found in the prepopulate data.  Any remaining URLs that haven't been
  // user-edited can be removed from the data store.
  for (auto& i : id_to_turl) {
    TemplateURL* template_url = i.second;
    if (template_url->safe_for_autoreplace()) {
      actions.removed_engines.push_back(template_url);
    }
  }

  return actions;
}

void ApplyActionsFromCurrentData(
    ActionsFromCurrentData actions,
    KeywordWebDataService* service,
    TemplateURLService::OwnedTemplateURLVector* template_urls,
    TemplateURL* default_search_provider,
    std::set<std::string>* removed_keyword_guids) {
  DCHECK(template_urls);

  // Remove items.
  for (const TemplateURL* removed_engine : actions.removed_engines) {
    auto j = FindTemplateURL(template_urls, removed_engine);
    CHECK(j != template_urls->end(), base::NotFatalUntil::M130);
    DCHECK(!default_search_provider ||
           (*j)->prepopulate_id() !=
               default_search_provider->prepopulate_id() ||
           (*j)->keyword() != default_search_provider->keyword());
    std::unique_ptr<TemplateURL> template_url = std::move(*j);
    template_urls->erase(j);
    if (service) {
      service->RemoveKeyword(template_url->id());
      if (removed_keyword_guids)
        removed_keyword_guids->insert(template_url->sync_guid());
    }
  }

  // Edit items.
  for (const auto& edited_engine : actions.edited_engines) {
    const TemplateURLData& data = edited_engine.second;
    if (service)
      service->UpdateKeyword(data);

    // Replace the entry in |template_urls| with the updated one.
    auto j = FindTemplateURL(template_urls, edited_engine.first);
    *j = std::make_unique<TemplateURL>(data);
  }

  // Add items.
  for (const auto& added_engine : actions.added_engines)
    template_urls->push_back(std::make_unique<TemplateURL>(added_engine));
}

void GetSearchProvidersUsingKeywordResult(
    const WDTypedResult& result,
    KeywordWebDataService* service,
    PrefService* prefs,
    search_engines::SearchEngineChoiceService* search_engine_choice_service,
    TemplateURLService::OwnedTemplateURLVector* template_urls,
    TemplateURL* default_search_provider,
    const SearchTermsData& search_terms_data,
    WDKeywordsResult::Metadata& out_updated_keywords_metadata,
    std::set<std::string>* removed_keyword_guids) {
  DCHECK(template_urls);
  DCHECK(template_urls->empty());
  DCHECK_EQ(KEYWORDS_RESULT, result.GetType());

  WDKeywordsResult keyword_result = reinterpret_cast<
      const WDResult<WDKeywordsResult>*>(&result)->GetValue();

  for (auto& keyword : keyword_result.keywords) {
    // Fix any duplicate encodings in the local database.  Note that we don't
    // adjust the last_modified time of this keyword; this way, we won't later
    // overwrite any changes on the sync server that happened to this keyword
    // since the last time we synced.  Instead, we also run a de-duping pass on
    // the server-provided data in
    // TemplateURLService::CreateTemplateURLFromTemplateURLAndSyncData() and
    // update the server with the merged, de-duped results at that time.  We
    // still fix here, though, to correct problems in clients that have disabled
    // search engine sync, since in that case that code will never be reached.
    if (DeDupeEncodings(&keyword.input_encodings) && service)
      service->UpdateKeyword(keyword);
    template_urls->push_back(std::make_unique<TemplateURL>(keyword));
  }

  out_updated_keywords_metadata = keyword_result.metadata;
  GetSearchProvidersUsingLoadedEngines(
      service, prefs, search_engine_choice_service, template_urls,
      default_search_provider, search_terms_data, out_updated_keywords_metadata,
      removed_keyword_guids);

  // If a data change happened, it should not cause a version downgrade.
  // Upgrades (builtin > new) or feature-related merges (builtin == new) only
  // are expected.
  DCHECK(!out_updated_keywords_metadata.HasBuiltinKeywordData() ||
         out_updated_keywords_metadata.builtin_keyword_data_version >=
             keyword_result.metadata.builtin_keyword_data_version);
}

void GetSearchProvidersUsingLoadedEngines(
    KeywordWebDataService* service,
    PrefService* prefs,
    search_engines::SearchEngineChoiceService* search_engine_choice_service,
    TemplateURLService::OwnedTemplateURLVector* template_urls,
    TemplateURL* default_search_provider,
    const SearchTermsData& search_terms_data,
    WDKeywordsResult::Metadata& in_out_keywords_metadata,
    std::set<std::string>* removed_keyword_guids) {
  DCHECK(template_urls);
  std::vector<std::unique_ptr<TemplateURLData>> prepopulated_urls =
      TemplateURLPrepopulateData::GetPrepopulatedEngines(
          prefs, search_engine_choice_service);
  RemoveDuplicatePrepopulateIDs(service, prepopulated_urls,
                                default_search_provider, template_urls,
                                search_terms_data, removed_keyword_guids);

  MergeEngineRequirements merge_requirements = ComputeMergeEnginesRequirements(
      prefs, search_engine_choice_service, in_out_keywords_metadata);

  if (merge_requirements.metadata.HasBuiltinKeywordData()) {
    MergeEnginesFromPrepopulateData(service, &prepopulated_urls, template_urls,
                                    default_search_provider,
                                    removed_keyword_guids);
  }

  if (merge_requirements.metadata.HasStarterPackData()) {
    bool overwrite_user_edits =
        (in_out_keywords_metadata.starter_pack_version <
         TemplateURLStarterPackData::GetFirstCompatibleDataVersion());
    MergeEnginesFromStarterPackData(
        service, template_urls, default_search_provider, removed_keyword_guids,
        (overwrite_user_edits ? TemplateURLMergeOption::kOverwriteUserEdits
                              : TemplateURLMergeOption::kDefault));
  }

  in_out_keywords_metadata = merge_requirements.metadata;
  switch (merge_requirements.should_keywords_use_extended_list) {
    case MergeEngineRequirements::ShouldKeywordsUseExtendedList::kUnknown:
      // Do nothing.
      break;
    case MergeEngineRequirements::ShouldKeywordsUseExtendedList::kYes:
      prefs->SetBoolean(prefs::kDefaultSearchProviderKeywordsUseExtendedList,
                        true);
      break;
    case MergeEngineRequirements::ShouldKeywordsUseExtendedList::kNo:
      prefs->ClearPref(prefs::kDefaultSearchProviderKeywordsUseExtendedList);
      break;
  }
}

bool DeDupeEncodings(std::vector<std::string>* encodings) {
  std::vector<std::string> deduped_encodings;
  std::set<std::string> encoding_set;
  for (std::vector<std::string>::const_iterator i(encodings->begin());
       i != encodings->end(); ++i) {
    if (encoding_set.insert(*i).second)
      deduped_encodings.push_back(*i);
  }
  encodings->swap(deduped_encodings);
  return encodings->size() != deduped_encodings.size();
}

TemplateURLService::OwnedTemplateURLVector::iterator FindTemplateURL(
    TemplateURLService::OwnedTemplateURLVector* urls,
    const TemplateURL* url) {
  return base::ranges::find(*urls, url, &std::unique_ptr<TemplateURL>::get);
}
