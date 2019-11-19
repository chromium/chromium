// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/search_engines/util.h"

#include <stddef.h>
#include <stdint.h>

#include <algorithm>
#include <limits>
#include <map>
#include <set>
#include <string>
#include <unordered_map>
#include <vector>

#include "base/logging.h"
#include "base/time/time.h"
#include "components/prefs/pref_service.h"
#include "components/search_engines/template_url.h"
#include "components/search_engines/template_url_prepopulate_data.h"
#include "components/search_engines/template_url_service.h"

base::string16 GetDefaultSearchEngineName(TemplateURLService* service) {
  DCHECK(service);
  const TemplateURL* const default_provider =
      service->GetDefaultSearchProvider();
  if (!default_provider) {
    // TODO(cpu): bug 1187517. It is possible to have no default provider.
    // returning an empty string is a stopgap measure for the crash
    // http://code.google.com/p/chromium/issues/detail?id=2573
    return base::string16();
  }
  return default_provider->short_name();
}

GURL GetDefaultSearchURLForSearchTerms(TemplateURLService* service,
                                       const base::string16& terms) {
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

void MergeIntoPrepopulatedEngineData(const TemplateURL* original_turl,
                                     TemplateURLData* prepopulated_url) {
  DCHECK(original_turl->prepopulate_id() == 0 ||
         original_turl->prepopulate_id() == prepopulated_url->prepopulate_id);
  // When the user modified search engine's properties or search engine is
  // imported from Play API data we need to preserve certain search engine
  // properties from overriding with prepopulated data.
  if (!original_turl->safe_for_autoreplace() ||
      original_turl->created_from_play_api()) {
    prepopulated_url->safe_for_autoreplace =
        original_turl->safe_for_autoreplace();
    prepopulated_url->SetShortName(original_turl->short_name());
    prepopulated_url->SetKeyword(original_turl->keyword());
    if (original_turl->created_from_play_api()) {
      // TODO(crbug/1002271): Search url from Play API might contain attribution
      // info and therefore should be preserved through prepopulated data
      // update. In the future we might decide to take different approach to
      // pass attribution info to search providers.
      prepopulated_url->SetURL(original_turl->url());
    }
  }
  prepopulated_url->id = original_turl->id();
  prepopulated_url->sync_guid = original_turl->sync_guid();
  prepopulated_url->date_created = original_turl->date_created();
  prepopulated_url->last_modified = original_turl->last_modified();
  prepopulated_url->created_from_play_api =
      original_turl->created_from_play_api();
}

ActionsFromPrepopulateData::ActionsFromPrepopulateData() {}

ActionsFromPrepopulateData::ActionsFromPrepopulateData(
    const ActionsFromPrepopulateData& other) = default;

ActionsFromPrepopulateData::~ActionsFromPrepopulateData() {}

void MergeEnginesFromPrepopulateData(
    KeywordWebDataService* service,
    std::vector<std::unique_ptr<TemplateURLData>>* prepopulated_urls,
    TemplateURLService::OwnedTemplateURLVector* template_urls,
    TemplateURL* default_search_provider,
    std::set<std::string>* removed_keyword_guids) {
  DCHECK(prepopulated_urls);
  DCHECK(template_urls);

  ActionsFromPrepopulateData actions(CreateActionsFromCurrentPrepopulateData(
      prepopulated_urls, *template_urls, default_search_provider));

  // Remove items.
  for (const auto* removed_engine : actions.removed_engines) {
    auto j = FindTemplateURL(template_urls, removed_engine);
    DCHECK(j != template_urls->end());
    DCHECK(!default_search_provider ||
           (*j)->prepopulate_id() != default_search_provider->prepopulate_id());
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

ActionsFromPrepopulateData CreateActionsFromCurrentPrepopulateData(
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
  ActionsFromPrepopulateData actions;
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
      MergeIntoPrepopulatedEngineData(existing_url, prepopulated_url.get());
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
  for (auto i = id_to_turl.begin(); i != id_to_turl.end(); ++i) {
    TemplateURL* template_url = i->second;
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

void GetSearchProvidersUsingKeywordResult(
    const WDTypedResult& result,
    KeywordWebDataService* service,
    PrefService* prefs,
    TemplateURLService::OwnedTemplateURLVector* template_urls,
    TemplateURL* default_search_provider,
    const SearchTermsData& search_terms_data,
    int* new_resource_keyword_version,
    std::set<std::string>* removed_keyword_guids) {
  DCHECK(template_urls);
  DCHECK(template_urls->empty());
  DCHECK_EQ(KEYWORDS_RESULT, result.GetType());
  DCHECK(new_resource_keyword_version);

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

  *new_resource_keyword_version = keyword_result.builtin_keyword_version;
  GetSearchProvidersUsingLoadedEngines(service, prefs, template_urls,
                                       default_search_provider,
                                       search_terms_data,
                                       new_resource_keyword_version,
                                       removed_keyword_guids);
}

void GetSearchProvidersUsingLoadedEngines(
    KeywordWebDataService* service,
    PrefService* prefs,
    TemplateURLService::OwnedTemplateURLVector* template_urls,
    TemplateURL* default_search_provider,
    const SearchTermsData& search_terms_data,
    int* resource_keyword_version,
    std::set<std::string>* removed_keyword_guids) {
  DCHECK(template_urls);
  DCHECK(resource_keyword_version);
  std::vector<std::unique_ptr<TemplateURLData>> prepopulated_urls =
      TemplateURLPrepopulateData::GetPrepopulatedEngines(prefs, nullptr);
  RemoveDuplicatePrepopulateIDs(service, prepopulated_urls,
                                default_search_provider, template_urls,
                                search_terms_data, removed_keyword_guids);

  const int prepopulate_resource_keyword_version =
      TemplateURLPrepopulateData::GetDataVersion(prefs);
  if (*resource_keyword_version < prepopulate_resource_keyword_version) {
    MergeEnginesFromPrepopulateData(service, &prepopulated_urls, template_urls,
                                    default_search_provider,
                                    removed_keyword_guids);
    *resource_keyword_version = prepopulate_resource_keyword_version;
  } else {
    *resource_keyword_version = 0;
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
  return std::find_if(urls->begin(), urls->end(),
                      [url](const std::unique_ptr<TemplateURL>& ptr) {
                        return ptr.get() == url;
                      });
}
