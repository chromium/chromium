// Copyright 2014 The Chromium Authors
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

#include "base/base64url.h"
#include "base/check_is_test.h"
#include "base/check_op.h"
#include "base/strings/string_number_conversions.h"
#include "base/time/time.h"
#include "components/country_codes/country_codes.h"
#include "components/google/core/common/google_util.h"
#include "components/lens/lens_overlay_mime_type.h"
#include "components/prefs/pref_service.h"
#include "components/regional_capabilities/regional_capabilities_utils.h"
#include "components/search_engines/keyword_web_data_service.h"
#include "components/search_engines/search_engine_choice/search_engine_choice_service.h"
#include "components/search_engines/search_engine_choice/search_engine_choice_utils.h"
#include "components/search_engines/search_engines_pref_names.h"
#include "components/search_engines/template_url.h"
#include "components/search_engines/template_url_prepopulate_data.h"
#include "components/search_engines/template_url_prepopulate_data_resolver.h"
#include "components/search_engines/template_url_service.h"
#include "components/search_engines/template_url_starter_pack_data.h"
#include "net/base/url_util.h"
#include "third_party/lens_server_proto/lens_overlay_contextual_inputs.pb.h"
#include "third_party/lens_server_proto/lens_overlay_request_id.pb.h"

namespace {

constexpr char kContextualInputsParameterKey[] = "cinpts";
constexpr char kSearchSessionIdParameterKey[] = "gsessionid";
constexpr char kLnsSurfaceParameterKey[] = "lns_surface";
constexpr char kVisualRequestIdQueryParameter[] = "vsrid";
constexpr char kVisualInputTypeQueryParameter[] = "vit";
constexpr char kVisualInputTypeQueryParameterPdfValue[] = "pdf";
constexpr char kVisualInputTypeQueryParameterImageValue[] = "img";
constexpr char kVisualInputTypeQueryParameterWebpageValue[] = "wp";
constexpr char kQuerySubmissionTimeQueryParameter[] = "qsubts";
constexpr char kClientUploadDurationQueryParameter[] = "cud";
constexpr char kAimUdmQueryParameterValue[] = "50";
constexpr char kMultimodalUdmQueryParameterValue[] = "24";
constexpr char kUnimodalUdmQueryParameterValue[] = "26";

// Computes whether updates to the search engines database are needed.
//
// `metadata.HasBuiltinKeywordUpdate()` and
// `metadata.HasStarterPackUpdate()` indicate the status for the
// two types of search engines, and when they are `true`, individual fields
// will contain the associated metadata that should be also added to the
// database.
WDKeywordsResult::Metadata ComputeMergeEnginesRequirements(
    const TemplateURLPrepopulateData::Resolver& prepopulate_data_resolver,
    const WDKeywordsResult::Metadata& keywords_metadata) {
  WDKeywordsResult::Metadata out_metadata;

  std::optional<TemplateURLPrepopulateData::BuiltinKeywordsMetadata>
      builtin_keywords_metadata =
          prepopulate_data_resolver.ComputeDatabaseUpdateRequirements(
              keywords_metadata);
  if (builtin_keywords_metadata.has_value()) {
    out_metadata.builtin_keyword_data_version =
        builtin_keywords_metadata->data_version;
    out_metadata.builtin_keyword_country =
        builtin_keywords_metadata->country_id;
  }

  const int starter_pack_data_version =
      template_url_starter_pack_data::GetDataVersion();
  if (keywords_metadata.starter_pack_version < starter_pack_data_version) {
    out_metadata.starter_pack_version = starter_pack_data_version;
  }

  return out_metadata;
}

std::string GetMimeTypeParamValue(lens::MimeType mime_type) {
  switch (mime_type) {
    case lens::MimeType::kPdf:
      return kVisualInputTypeQueryParameterPdfValue;
    case lens::MimeType::kImage:
      return kVisualInputTypeQueryParameterImageValue;
    case lens::MimeType::kAnnotatedPageContent:
      return kVisualInputTypeQueryParameterWebpageValue;
    default:
      NOTREACHED() << "File type not supported.";
  }
}

GURL GetSearchUrlWithUdm(TemplateURLService* turl_service,
                         omnibox::ChromeAimEntryPoint aim_entrypoint,
                         const std::string& udm_value,
                         const base::Time& query_start_time,
                         const std::u16string& query_text,
                         std::map<std::string, std::string> additional_params) {
  const TemplateURLRef& url_ref =
      turl_service->GetDefaultSearchProvider()->url_ref();
  TemplateURLRef::SearchTermsArgs search_term_args =
      TemplateURLRef::SearchTermsArgs(query_text);
  GURL result_url = GURL(url_ref.ReplaceSearchTerms(
      search_term_args, turl_service->search_terms_data()));
  // Append all additional params.
  for (auto const& param : additional_params) {
    result_url = net::AppendOrReplaceQueryParameter(result_url, param.first,
                                                    param.second);
  }
  result_url = net::AppendOrReplaceQueryParameter(result_url, "udm", udm_value);
  // Don't override the aep param from `additional_params`. This value could be
  // given alongside the match from the server. This should keep precedence
  // over the generic entrypoint value.
  if (!additional_params.contains("aep")) {
    result_url = net::AppendOrReplaceQueryParameter(
        result_url, "aep",
        base::NumberToString(static_cast<int>(aim_entrypoint)));
  }
  base::Time query_submission_time = base::Time::Now();
  result_url = net::AppendOrReplaceQueryParameter(
      result_url, kClientUploadDurationQueryParameter,
      base::NumberToString(
          (query_submission_time - query_start_time).InMilliseconds()));
  result_url = net::AppendOrReplaceQueryParameter(
      result_url, kQuerySubmissionTimeQueryParameter,
      base::NumberToString(
          query_submission_time.InMillisecondsSinceUnixEpoch()));
  return result_url;
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
  if (!default_provider) {
    return GURL();
  }
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
  for (const auto& url : prepopulated_urls) {
    prepopulated_url_map[url->prepopulate_id] = url.get();
  }

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
        if (removed_keyword_guids) {
          removed_keyword_guids->insert(duplicate->sync_guid());
        }
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
    if ((*i)->prepopulate_id() == prepopulate_id) {
      return *i;
    }
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
  // imported from regulatory extensions we need to preserve certain search
  // engine properties from overriding with prepopulated data.
  bool preserve_user_edits =
      merge_option != TemplateURLMergeOption::kOverwriteUserEdits &&
      (!original_turl->safe_for_autoreplace() ||
       original_turl->CreatedByRegulatoryProgram());
  if (preserve_user_edits) {
    url_to_update->safe_for_autoreplace = original_turl->safe_for_autoreplace();
    url_to_update->SetShortName(original_turl->short_name());
    url_to_update->SetKeyword(original_turl->keyword());
    if (original_turl->CreatedByRegulatoryProgram()) {
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
  url_to_update->regulatory_origin = original_turl->data().regulatory_origin;
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
  std::map<std::u16string_view, TemplateURL*> regulatory_entries;
  std::map<int, TemplateURL*> id_to_turl;
  for (auto& turl : existing_urls) {
    if (turl->CreatedByRegulatoryProgram()) {
      regulatory_entries.insert({turl->keyword(), turl.get()});
    }
    int prepopulate_id = turl->prepopulate_id();
    if (prepopulate_id > 0) {
      id_to_turl[prepopulate_id] = turl.get();
    }
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
    } else if (auto iter = regulatory_entries.find(prepopulated_url->keyword());
               iter != regulatory_entries.end()) {
      existing_url = iter->second;
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
      if (template_url->CreatedByRegulatoryProgram()) {
        // Don't remove the entry created from regulatory extensions. Just reset
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

void MergeEnginesFromStarterPackData(
    KeywordWebDataService* service,
    TemplateURLService::OwnedTemplateURLVector* template_urls,
    TemplateURL* default_search_provider,
    std::set<std::string>* removed_keyword_guids,
    TemplateURLMergeOption merge_option) {
  DCHECK(template_urls);

  std::vector<std::unique_ptr<TemplateURLData>> starter_pack_urls =
      template_url_starter_pack_data::GetStarterPackEngines();

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
    if (starter_pack_id > 0) {
      id_to_turl[starter_pack_id] = turl.get();
    }
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
    CHECK(j != template_urls->end());
    DCHECK(!default_search_provider ||
           (*j)->prepopulate_id() !=
               default_search_provider->prepopulate_id() ||
           (*j)->keyword() != default_search_provider->keyword());
    std::unique_ptr<TemplateURL> template_url = std::move(*j);
    template_urls->erase(j);
    if (service) {
      service->RemoveKeyword(template_url->id());
      if (removed_keyword_guids) {
        removed_keyword_guids->insert(template_url->sync_guid());
      }
    }
  }

  // Edit items.
  for (const auto& edited_engine : actions.edited_engines) {
    const TemplateURLData& data = edited_engine.second;
    if (service) {
      service->UpdateKeyword(data);
    }

    // Replace the entry in |template_urls| with the updated one.
    auto j = FindTemplateURL(template_urls, edited_engine.first);
    *j = std::make_unique<TemplateURL>(data);
  }

  // Add items.
  for (const auto& added_engine : actions.added_engines) {
    template_urls->push_back(std::make_unique<TemplateURL>(added_engine));
  }
}

void GetSearchProvidersUsingKeywordResult(
    const WDKeywordsResult& keyword_result,
    KeywordWebDataService* service,
    PrefService* prefs,
    const TemplateURLPrepopulateData::Resolver& template_url_data_resolver,
    TemplateURLService::OwnedTemplateURLVector* template_urls,
    TemplateURL* default_search_provider,
    const SearchTermsData& search_terms_data,
    WDKeywordsResult::Metadata& out_updated_keywords_metadata,
    std::set<std::string>* removed_keyword_guids) {
  DCHECK(template_urls);
  DCHECK(template_urls->empty());

  for (TemplateURLData keyword : keyword_result.keywords) {
    // Fix any duplicate encodings in the local database.  Note that we don't
    // adjust the last_modified time of this keyword; this way, we won't later
    // overwrite any changes on the sync server that happened to this keyword
    // since the last time we synced.  Instead, we also run a de-duping pass on
    // the server-provided data in
    // TemplateURLService::CreateTemplateURLFromTemplateURLAndSyncData() and
    // update the server with the merged, de-duped results at that time.  We
    // still fix here, though, to correct problems in clients that have disabled
    // search engine sync, since in that case that code will never be reached.
    if (DeDupeEncodings(&keyword.input_encodings) && service) {
      service->UpdateKeyword(keyword);
    }
    template_urls->push_back(std::make_unique<TemplateURL>(keyword));
  }

  out_updated_keywords_metadata = keyword_result.metadata;
  GetSearchProvidersUsingLoadedEngines(
      service, prefs, template_url_data_resolver, template_urls,
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
    const TemplateURLPrepopulateData::Resolver& template_url_data_resolver,
    TemplateURLService::OwnedTemplateURLVector* template_urls,
    TemplateURL* default_search_provider,
    const SearchTermsData& search_terms_data,
    WDKeywordsResult::Metadata& in_out_keywords_metadata,
    std::set<std::string>* removed_keyword_guids) {
  DCHECK(template_urls);
  std::vector<std::unique_ptr<TemplateURLData>> prepopulated_urls =
      template_url_data_resolver.GetPrepopulatedEngines();
  RemoveDuplicatePrepopulateIDs(service, prepopulated_urls,
                                default_search_provider, template_urls,
                                search_terms_data, removed_keyword_guids);

  WDKeywordsResult::Metadata required_metadata =
      ComputeMergeEnginesRequirements(template_url_data_resolver,
                                      in_out_keywords_metadata);

  if (required_metadata.HasBuiltinKeywordData()) {
    MergeEnginesFromPrepopulateData(service, &prepopulated_urls, template_urls,
                                    default_search_provider,
                                    removed_keyword_guids);
  }

  if (required_metadata.HasStarterPackData()) {
    bool overwrite_user_edits =
        (in_out_keywords_metadata.starter_pack_version <
         template_url_starter_pack_data::GetFirstCompatibleDataVersion());
    MergeEnginesFromStarterPackData(
        service, template_urls, default_search_provider, removed_keyword_guids,
        (overwrite_user_edits ? TemplateURLMergeOption::kOverwriteUserEdits
                              : TemplateURLMergeOption::kDefault));
  }

  in_out_keywords_metadata = required_metadata;
}

bool DeDupeEncodings(std::vector<std::string>* encodings) {
  std::vector<std::string> deduped_encodings;
  std::set<std::string> encoding_set;
  for (std::vector<std::string>::const_iterator i(encodings->begin());
       i != encodings->end(); ++i) {
    if (encoding_set.insert(*i).second) {
      deduped_encodings.push_back(*i);
    }
  }
  encodings->swap(deduped_encodings);
  return encodings->size() != deduped_encodings.size();
}

TemplateURLService::OwnedTemplateURLVector::iterator FindTemplateURL(
    TemplateURLService::OwnedTemplateURLVector* urls,
    const TemplateURL* url) {
  return std::ranges::find(*urls, url, &std::unique_ptr<TemplateURL>::get);
}

bool IsAimURL(const GURL& url) {
  if (!google_util::IsGoogleSearchUrl(url)) {
    return false;
  }
  std::string udm;
  bool has_udm = net::GetValueForKeyInQuery(url, "udm", &udm);
  return has_udm && udm == kAimUdmQueryParameterValue;
}

GURL GetUrlForAim(TemplateURLService* turl_service,
                  omnibox::ChromeAimEntryPoint aim_entrypoint,
                  const base::Time& query_start_time,
                  const std::u16string& query_text,
                  std::map<std::string, std::string> additional_params) {
  return GetSearchUrlWithUdm(turl_service, aim_entrypoint,
                             kAimUdmQueryParameterValue, query_start_time,
                             query_text, additional_params);
}

GURL GetUrlForMultimodalSearch(
    TemplateURLService* turl_service,
    bool is_aim_search,
    omnibox::ChromeAimEntryPoint aim_entrypoint,
    const base::Time& query_start_time,
    const std::string& search_session_id,
    const std::unique_ptr<lens::LensOverlayRequestId> request_id,
    const lens::MimeType mime_type,
    const std::string& lns_surface,
    const std::u16string& query_text,
    std::map<std::string, std::string> additional_params) {
  GURL result_url = GetSearchUrlWithUdm(
      turl_service, aim_entrypoint,
      is_aim_search ? kAimUdmQueryParameterValue
                    : (query_text.empty() ? kUnimodalUdmQueryParameterValue
                                          : kMultimodalUdmQueryParameterValue),
      query_start_time, query_text, additional_params);
  std::string serialized_request_id;
  CHECK(request_id->SerializeToString(&serialized_request_id));
  std::string encoded_request_id;
  base::Base64UrlEncode(serialized_request_id,
                        base::Base64UrlEncodePolicy::OMIT_PADDING,
                        &encoded_request_id);
  result_url = net::AppendOrReplaceQueryParameter(
      result_url, kVisualRequestIdQueryParameter, encoded_request_id);
  result_url = net::AppendOrReplaceQueryParameter(
      result_url, kVisualInputTypeQueryParameter,
      GetMimeTypeParamValue(mime_type));
  result_url = net::AppendOrReplaceQueryParameter(
      result_url, kSearchSessionIdParameterKey, search_session_id);
  result_url = net::AppendOrReplaceQueryParameter(
      result_url, kLnsSurfaceParameterKey, lns_surface);
  return result_url;
}

GURL GetUrlForMultimodalSearch(
    TemplateURLService* turl_service,
    bool is_aim_search,
    omnibox::ChromeAimEntryPoint aim_entrypoint,
    const base::Time& query_start_time,
    const std::string& search_session_id,
    const std::unique_ptr<lens::LensOverlayContextualInputs> contextual_inputs,
    const std::string& lns_surface,
    const std::u16string& query_text,
    std::map<std::string, std::string> additional_params) {
  GURL result_url = GetSearchUrlWithUdm(
      turl_service, aim_entrypoint,
      is_aim_search ? kAimUdmQueryParameterValue
                    : (query_text.empty() ? kUnimodalUdmQueryParameterValue
                                          : kMultimodalUdmQueryParameterValue),
      query_start_time, query_text, additional_params);
  std::string serialized_contextual_inputs;
  CHECK(contextual_inputs->SerializeToString(&serialized_contextual_inputs));
  std::string encoded_contextual_inputs;
  base::Base64UrlEncode(serialized_contextual_inputs,
                        base::Base64UrlEncodePolicy::OMIT_PADDING,
                        &encoded_contextual_inputs);
  result_url = net::AppendOrReplaceQueryParameter(
      result_url, kContextualInputsParameterKey, encoded_contextual_inputs);
  result_url = net::AppendOrReplaceQueryParameter(
      result_url, kSearchSessionIdParameterKey, search_session_id);
  result_url = net::AppendOrReplaceQueryParameter(
      result_url, kLnsSurfaceParameterKey, lns_surface);
  return result_url;
}
