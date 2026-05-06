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
#include "base/debug/crash_logging.h"
#include "base/debug/dump_without_crashing.h"
#include "base/metrics/histogram_functions.h"
#include "base/not_fatal_until.h"
#include "base/strings/string_number_conversions.h"
#include "base/strings/utf_string_conversions.h"
#include "base/time/time.h"
#include "components/country_codes/country_codes.h"
#include "components/google/core/common/google_util.h"
#include "components/lens/lens_overlay_invocation_source.h"
#include "components/lens/lens_overlay_mime_type.h"
#include "components/lens/lens_url_utils.h"
#include "components/prefs/pref_service.h"
#include "components/regional_capabilities/regional_capabilities_switches.h"
#include "components/regional_capabilities/regional_capabilities_utils.h"
#include "components/search_engines/keyword_web_data_service.h"
#include "components/search_engines/search_engine_choice/search_engine_choice_service.h"
#include "components/search_engines/search_engine_choice/search_engine_choice_utils.h"
#include "components/search_engines/search_engines_pref_names.h"
#include "components/search_engines/template_url.h"
#include "components/search_engines/template_url_data_util.h"
#include "components/search_engines/template_url_prepopulate_data.h"
#include "components/search_engines/template_url_prepopulate_data_resolver.h"
#include "components/search_engines/template_url_service.h"
#include "components/search_engines/template_url_starter_pack_data.h"
#include "components/url_formatter/url_fixer.h"
#include "net/base/url_util.h"
#include "third_party/lens_server_proto/lens_overlay_contextual_inputs.pb.h"
#include "third_party/lens_server_proto/lens_overlay_request_id.pb.h"
#include "third_party/search_engines_data/resources/definitions/prepopulated_engines.h"

namespace {

using ::TemplateURLPrepopulateData::PrepopulatedEngine;

constexpr char kContextualInputsParameterKey[] = "cinpts";
constexpr char kSearchSessionIdParameterKey[] = "gsessionid";
constexpr char kLnsSurfaceParameterKey[] = "lns_surface";
constexpr char kVisualRequestIdQueryParameter[] = "vsrid";
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

GURL GetBaseSearchUrl(TemplateURLService* turl_service,
                      omnibox::ChromeAimEntryPoint aim_entrypoint,
                      bool is_aim_search,
                      const base::Time& query_start_time,
                      const std::u16string& query_text,
                      std::map<std::string, std::string> additional_params) {
  const TemplateURLRef& url_ref =
      turl_service->GetDefaultSearchProvider()->url_ref();
  TemplateURLRef::SearchTermsArgs search_term_args =
      TemplateURLRef::SearchTermsArgs(query_text);
  GURL result_url = GURL(url_ref.ReplaceSearchTerms(
      search_term_args, turl_service->search_terms_data()));

  if (is_aim_search) {
    // For AIM queries, add udm=50 as a fallback if no udm or nem param is
    // present.
    if (additional_params.count("udm") == 0 &&
        additional_params.count("nem") == 0) {
      additional_params["udm"] = kAimUdmQueryParameterValue;
    }
  }

  // Append all additional params.
  for (auto const& param : additional_params) {
    result_url = net::AppendOrReplaceQueryParameter(result_url, param.first,
                                                    param.second);
  }

  if (!is_aim_search) {
    std::string udm_value = query_text.empty()
                                ? kUnimodalUdmQueryParameterValue
                                : kMultimodalUdmQueryParameterValue;
    result_url =
        net::AppendOrReplaceQueryParameter(result_url, "udm", udm_value);
  }

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

const PrepopulatedEngine* GetMigrationSource(int migrated_engine_id) {
  if (!base::FeatureList::IsEnabled(switches::kPrepopulatedEnginesMigration)) {
    return nullptr;
  }

  const regional_capabilities::MigratingEngines& migrating_engines =
      regional_capabilities::GetMigratingPrepopulatedEngines();
  if (auto migrating_engine_it = migrating_engines.find(migrated_engine_id);
      migrating_engine_it != migrating_engines.end()) {
    return migrating_engine_it->second;
  }

  return nullptr;
}

// Checks `template_url` against `default_search_provider`
//
// Assumptions:
// - `template_url` is coming from the previous run's keywords database, so its
// data might be corresponding to prepopulated data from a different region.
// - `default_search_provider` is coming from prefs, and already got through a
// reconciliation step with the current region's prepopulated engines.
//
// `template_url` is considered as matching the DSP if its `prepopulate_id`
// matches the DSP's or the DSP's pre-migration engine's ID. Stricter matching
// that includes the keyword may be done when `check_keyword` is set or when the
// DSP is not derived from a prepopulated engine.
bool MatchesDefaultSearchProvider(const TemplateURL* template_url,
                                  const TemplateURL* default_search_provider,
                                  bool check_keyword) {
  if (!default_search_provider) {
    return false;
  }

  if (template_url->keyword() != default_search_provider->keyword()) {
    if (template_url->safe_for_autoreplace() && check_keyword) {
      // User modifications can't explain the keyword mismatch, so count this as
      // not matching.
      return false;
    }

    if (template_url->prepopulate_id() == 0) {
      // Custom engines need to rely on keywords for matching, regardless of
      // `check_keyword`.
      return false;
    }
  }

  if (template_url->prepopulate_id() ==
      default_search_provider->prepopulate_id()) {
    return true;
  }

  if (const PrepopulatedEngine* pre_migration_engine =
          GetMigrationSource(default_search_provider->prepopulate_id());
      pre_migration_engine &&
      pre_migration_engine->id == template_url->prepopulate_id()) {
    return true;
  }

  return false;
}

// LINT.IfChange(EntryPreservationReason)
enum class EntryPreservationReason {
  kMatchesDefaultSearchProvider = 1,
  kMatchesOtherDefaultSearchProvider = 2,
  kNotSafeForAutoreplace = 3,
  kNonPrepopulatedFromRegulatoryProgram = 4,
  kMaxValue = kNonPrepopulatedFromRegulatoryProgram,
};
// LINT.ThenChange(/tools/metrics/histograms/metadata/omnibox/enums.xml:EntryPreservationReason)

void RecordEntryPreservationReason(EntryPreservationReason reason) {
  base::UmaHistogramEnumeration(
      "Omnibox.TemplateUrl.DBRefresh.EntryPreservationReason", reason);
}

// LINT.IfChange(PrepopulatedEngineMigrationAction)
enum class PrepopulatedEngineMigrationAction {
  kMigratedDefaultProvider = 0,
  kMigratedNonDefault = 1,
  kMaxValue = kMigratedNonDefault,
};
// LINT.ThenChange(/tools/metrics/histograms/metadata/omnibox/enums.xml:PrepopulatedEngineMigrationAction)

void RecordMigrationAction(PrepopulatedEngineMigrationAction action) {
  base::UmaHistogramEnumeration("Omnibox.TemplateUrl.DBRefresh.MigrationAction",
                                action);
}

void RecordDefaultSearchMatchCount(int entries_matching_dsp_to_reconcile,
                                   bool is_unreconciled_count) {
  base::UmaHistogramCounts100(
      is_unreconciled_count
          ? "Omnibox.TemplateUrl.DBRefresh.UnmatchedDefaultSearchCount"
          : "Omnibox.TemplateUrl.DBRefresh.DefaultSearchMatchCountById",
      entries_matching_dsp_to_reconcile);
}

}  // namespace

bool IsSearchEngineNameValidToUse(const std::u16string& name_input) {
  return !base::CollapseWhitespace(name_input, true).empty();
}

bool IsSearchEngineKeywordValidToUse(const std::u16string& keyword_input,
                                     const TemplateURLService* service,
                                     const TemplateURL* existing_url) {
  std::u16string keyword_input_trimmed(
      base::CollapseWhitespace(keyword_input, true));
  if (keyword_input_trimmed.empty()) {
    return false;  // Do not allow empty keyword.
  }

  // The omnibox doesn't properly handle search keywords with whitespace,
  // so do not allow such keywords.
  if (keyword_input_trimmed.find_first_of(base::kWhitespaceUTF16) !=
      std::u16string::npos) {
    return false;
  }

  const TemplateURL* turl_with_keyword =
      service->GetTemplateURLForKeyword(keyword_input_trimmed);
  return (!turl_with_keyword || turl_with_keyword == existing_url);
}

std::string GetFixedUpSearchEngineUrl(
    const std::string& url_input,
    const SearchTermsData& search_terms_data) {
  std::u16string url16;
  base::TrimWhitespace(base::UTF8ToUTF16(url_input), base::TRIM_ALL, &url16);
  if (url16.empty()) {
    return std::string();
  }
  std::string url = TemplateURLRef::DisplayURLToURLRef(url16);

  // Parse the string as a URL to determine the scheme. If we need to, add the
  // scheme. As the scheme may be expanded (as happens with {google:baseURL})
  // we need to replace the search terms before testing for the scheme.
  TemplateURLData data;
  data.SetURL(url);
  TemplateURL t_url(data);
  std::string expanded_url(t_url.url_ref().ReplaceSearchTerms(
      TemplateURLRef::SearchTermsArgs(u"x"), search_terms_data));
  url::Parsed parts;
  std::string scheme(url_formatter::SegmentURL(expanded_url, &parts));
  if (!parts.scheme.is_valid()) {
    url.insert(0, scheme + "://");
  }

  return url;
}

bool IsSearchEngineURLValidToUse(const std::string& url_input,
                                 const TemplateURLService* service,
                                 const TemplateURL* existing_url) {
  std::string url =
      GetFixedUpSearchEngineUrl(url_input, service->search_terms_data());
  if (url.empty()) {
    return false;
  }

  // Convert |url| to a TemplateURLRef so we can check its validity even if it
  // contains replacement strings.  We do this by constructing a dummy
  // TemplateURL owner because |existing_url| might be nullptr and we can't
  // call TemplateURLRef::IsValid() when its owner is nullptr.
  TemplateURLData data;
  data.SetURL(url);
  TemplateURL t_url(data);
  const TemplateURLRef& template_ref = t_url.url_ref();
  if (!template_ref.IsValid(service->search_terms_data())) {
    return false;
  }

  // If this is going to be the default search engine, it must support
  // replacement.
  if (!template_ref.SupportsReplacement(service->search_terms_data()) &&
      existing_url && existing_url == service->GetDefaultSearchProvider()) {
    return false;
  }

  // Replace any search term with a placeholder string and make sure the
  // resulting URL is valid.
  return GURL(template_ref.ReplaceSearchTerms(
                  TemplateURLRef::SearchTermsArgs(u"x"),
                  service->search_terms_data()))
      .is_valid();
}

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

void MergeIntoEngineData(const TemplateURLData& original_turl,
                         TemplateURLData& data_to_update,
                         TemplateURLMergeOption merge_option) {
  bool is_id_migration =
      original_turl.prepopulate_id != 0 &&
      data_to_update.prepopulate_id != original_turl.prepopulate_id &&
      base::FeatureList::IsEnabled(switches::kPrepopulatedEnginesMigration);

  if (merge_option == TemplateURLMergeOption::kSplitPrepopulatedEntry) {
    CHECK(is_id_migration);
  } else if (merge_option ==
             TemplateURLMergeOption::kSettingAsDefaultProvider) {
    CHECK(original_turl.prepopulate_id != 0 &&
              data_to_update.prepopulate_id == original_turl.prepopulate_id ||
          // The pair of engines to merge was selected based on keywords, but
          // due to keyword-based migration, they might not be matching. See
          // `ReconcilingTemplateURLDataHolder::GetOrComputeKeyword`.
          original_turl.CreatedByRegulatoryProgram() || is_id_migration);
  } else {
    CHECK(!is_id_migration);
    CHECK(original_turl.prepopulate_id == 0 ||
          original_turl.prepopulate_id == data_to_update.prepopulate_id);

    CHECK(original_turl.starter_pack_id ==
              static_cast<int>(
                  template_url_starter_pack_data::StarterPackId::kNone) ||
          original_turl.starter_pack_id == data_to_update.starter_pack_id);
  }

  if (merge_option != TemplateURLMergeOption::kOverwriteUserEdits) {
    // Preserve key fields from being overwritten with prepopulated data.

    data_to_update.safe_for_autoreplace = original_turl.safe_for_autoreplace;

    bool preserve_program_properties =
        original_turl.CreatedByRegulatoryProgram() &&
        merge_option != TemplateURLMergeOption::kSettingAsDefaultProvider;
    bool preserve_user_edits =
        !original_turl.safe_for_autoreplace || preserve_program_properties;

    if (preserve_user_edits) {
      data_to_update.SetShortName(original_turl.short_name());
      data_to_update.SetKeyword(original_turl.keyword());
    }

    if (preserve_program_properties) {
      // TODO(crbug.com/480856411): Search url from Play API might contain
      // attribution info and therefore should be preserved through prepopulated
      // data update (see crbug.com/40646573).
      // Feb 2026 update: This might not be necessary, as the "regulatory
      // extensions" mechanisms allows to do this while supporting updates.
      data_to_update.SetURL(original_turl.url());
    }
  }

  if (is_id_migration) {
    RecordMigrationAction(
        merge_option == TemplateURLMergeOption::kSettingAsDefaultProvider
            ? PrepopulatedEngineMigrationAction::kMigratedDefaultProvider
            : PrepopulatedEngineMigrationAction::kMigratedNonDefault);

    // The data from `original_turl` has been merged into `data_to_update`, but
    // `data_to_update` has a different `prepopulate_id`. This could lead to
    // reconciliation issues on clients which don't have the latest data yet,
    // and future confusion about how the GUID for this entry was generated.
    // Reset it to limit such issues.
    // For the `kSettingAsDefaultProvider` flow, this will also trigger the
    // prefs to be updated.
    data_to_update.GenerateSyncGUID();
  } else {
    data_to_update.sync_guid = original_turl.sync_guid;
  }

  if (merge_option == TemplateURLMergeOption::kSettingAsDefaultProvider) {
    data_to_update.last_visited = original_turl.last_visited;
    data_to_update.favicon_url = original_turl.favicon_url;
  }

  data_to_update.id = original_turl.id;
  data_to_update.date_created = original_turl.date_created;
  data_to_update.last_modified = original_turl.last_modified;
  data_to_update.regulatory_origin = original_turl.regulatory_origin;
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
    const TemplateURLPrepopulateData::Resolver& template_url_data_resolver,
    std::set<std::string>* removed_keyword_guids) {
  DCHECK(prepopulated_urls);
  DCHECK(template_urls);

  ActionsFromCurrentData actions(CreateActionsFromCurrentPrepopulateData(
      prepopulated_urls, *template_urls, default_search_provider,
      template_url_data_resolver));

  ApplyActionsFromCurrentData(actions, service, template_urls,
                              default_search_provider, removed_keyword_guids);
}

// Returns 2 values:
// - an iterator to `id_to_existing_turl`, indicating which entry was matched,
// or `id_to_existing_turl.end()` if none was matched.
// - The type of merging to be applied between `prepopulated_url` and the match
// entry.
std::pair<std::map<int, TemplateURL*>::iterator, TemplateURLMergeOption>
MatchIncomingPrepopulatedEntry(
    const TemplateURLPrepopulateData::Resolver& template_url_data_resolver,
    const TemplateURLData& prepopulated_url,
    std::map<int, TemplateURL*>& id_to_existing_turl,
    const TemplateURL* existing_turl_matching_default_search_provider) {
  const int prepopulated_id = prepopulated_url.prepopulate_id;
  DCHECK_NE(0, prepopulated_id);

  if (const PrepopulatedEngine* pre_migration_engine =
          GetMigrationSource(prepopulated_id)) {
    // `prepopulated_url` is a migrated engine. Find whether there is an
    // existing Template URL matching the old version of the engine.
    if (auto existing_url_iter =
            id_to_existing_turl.find(pre_migration_engine->id);
        existing_url_iter != id_to_existing_turl.end()) {
      // Some existing entry matched. Verify we're matching fully, not just on
      // prepopulated ID.

      const TemplateURL* existing_url = existing_url_iter->second;
      if (template_url_data_resolver.MatchesEngineUnderMigration(
              existing_url->data(), pre_migration_engine)) {
        return {existing_url_iter,
                TemplateURLMergeOption::kSplitPrepopulatedEntry};
      }

      // The existing engine does not fully match, but it was picked as closest
      // to the current DSP, per logic from `MatchesDefaultSearchProvider`.
      if (existing_url == existing_turl_matching_default_search_provider) {
        return {existing_url_iter,
                TemplateURLMergeOption::kSplitPrepopulatedEntry};
      }
    }
  }

  // The found entry will be matched, and `end()` will indicate that nothing was
  // found.
  return {id_to_existing_turl.find(prepopulated_id),
          TemplateURLMergeOption::kDefault};
}

ActionsFromCurrentData CreateActionsFromCurrentPrepopulateData(
    std::vector<std::unique_ptr<TemplateURLData>>* prepopulated_urls,
    const TemplateURLService::OwnedTemplateURLVector& existing_urls,
    const TemplateURL* default_search_provider,
    const TemplateURLPrepopulateData::Resolver& template_url_data_resolver) {
  // Tracking variables to improve the efficiency of the reconciliation.

  // Keyword and entry for the existing engine created through a device choice
  // program.
  std::map<std::u16string_view, TemplateURL*> regulatory_entries;

  // All existing Template URLs that originally came from prepopulate data (i.e.
  // have a non-zero prepopulate_id()).
  std::map<int, TemplateURL*> id_to_turl;

  // Tracking of existing entries that match the DSP, and of the one that is
  // selected as best representative for it.
  int entries_matching_dsp_to_reconcile = 0;
  TemplateURL* dsp_match = nullptr;

  for (auto& turl : existing_urls) {
    if (turl->CreatedByRegulatoryProgram()) {
      // This might be a deprecated keyword, as variants for some engines have
      // changed since programs were introduced. See
      // `ReconcilingTemplateURLDataHolder::GetOrComputeKeyword()`.
      regulatory_entries.insert({turl->keyword(), turl.get()});
    }
    int prepopulate_id = turl->prepopulate_id();
    if (prepopulate_id > 0) {
      id_to_turl[prepopulate_id] = turl.get();
    }
    if (MatchesDefaultSearchProvider(turl.get(), default_search_provider,
                                     /*check_keyword=*/false)) {
      if (!dsp_match ||
          // Keep the one with the higher `prepopulate_id`, which will
          // prioritise the post-migration entry.
          prepopulate_id > dsp_match->prepopulate_id()) {
        dsp_match = turl.get();
      }

      ++entries_matching_dsp_to_reconcile;
    }
  }

  RecordDefaultSearchMatchCount(entries_matching_dsp_to_reconcile,
                                /*is_unreconciled_count=*/false);

  // Debugging https://crbug.com/492852740
  bool is_dsp_from_policy =
      default_search_provider && default_search_provider->enforced_by_policy();
  bool is_dsp_from_extension =
      default_search_provider &&
      (default_search_provider->type() == TemplateURL::OMNIBOX_API_EXTENSION ||
       default_search_provider->type() ==
           TemplateURL::NORMAL_CONTROLLED_BY_EXTENSION);

  SCOPED_CRASH_KEY_BOOL("KwdbRefresh", "has_dsp_match", dsp_match != nullptr);

  // Breakdown of the explanations for a DSP mismatch.
  bool has_mismatch_explanation =
      // There is no DSP.
      !default_search_provider ||
      // There is no set of existing turls to get a match from.
      existing_urls.empty();

  // - Confirmed and expected reasons
  SCOPED_CRASH_KEY_BOOL("KwdbRefresh", "has_no_preloaded_dsp",
                        default_search_provider == nullptr);
  SCOPED_CRASH_KEY_BOOL("KwdbRefresh", "is_existing_urls_empty",
                        existing_urls.empty());
  // - Other hypotheses
  //   Not confirmed because they should normally not be brought up through
  //   pre-loading DSP, or their first appearance should come after the keywords
  //   DB is loaded, and then they should have been added to it.
  SCOPED_CRASH_KEY_BOOL("KwdbRefresh", "is_dsp_from_policy",
                        is_dsp_from_policy);
  SCOPED_CRASH_KEY_BOOL("KwdbRefresh", "is_dsp_from_extension",
                        is_dsp_from_extension);
  SCOPED_CRASH_KEY_BOOL(
      "KwdbRefresh", "is_from_reg_program",
      default_search_provider &&
          default_search_provider->CreatedByRegulatoryProgram());

  if (!dsp_match && !has_mismatch_explanation) {
    // This is not implemented with a `CHECK` for various reasons:
    // - It's a pre-existing behaviour
    // - Some of the ways to trigger it are explicitly not blocked upstream on
    //   some platforms, during prefs loading.
    // So we keep this as a `DumpWithoutCrashing` to avoid causing test
    // failures, while still allowing to collect data, validating the logic in
    // this function and following-up with some defensive checks.
    base::debug::DumpWithoutCrashing();
  }

  // We expect to only have one regulatory program engine at a time, see
  // `TemplateURLService::ResetPlayAPISearchEngine()`.
  CHECK_LE(regulatory_entries.size(), 1u, base::NotFatalUntil::M150);

  // For each current prepopulated URL, check whether |template_urls| contained
  // a matching prepopulated URL.  If so, update the passed-in URL to match the
  // current data.  (If the passed-in URL was user-edited, we persist the user's
  // name and keyword.)  If not, add the prepopulated URL.
  ActionsFromCurrentData actions;
  for (auto& prepopulated_url : *prepopulated_urls) {
    const int prepopulated_id = prepopulated_url->prepopulate_id;
    DCHECK_NE(0, prepopulated_id);

    TemplateURL* existing_url = nullptr;
    TemplateURLMergeOption merge_option;
    if (auto [existing_url_it, computed_merge_option] =
            MatchIncomingPrepopulatedEntry(template_url_data_resolver,
                                           *prepopulated_url.get(), id_to_turl,
                                           dsp_match);
        existing_url_it != id_to_turl.end()) {
      existing_url = existing_url_it->second;
      merge_option = computed_merge_option;
      id_to_turl.erase(existing_url_it);
    } else if (auto iter = regulatory_entries.find(prepopulated_url->keyword());
               iter != regulatory_entries.end()) {
      // TODO(crbug.com/490069353): Investigate whether we should remove the
      // entry from the maps.
      existing_url = iter->second;
      merge_option = TemplateURLMergeOption::kDefault;
    }

    if (existing_url != nullptr) {
      if (existing_url == dsp_match) {
        // DSP matched and processed. This tracking variable is not needed
        // anymore and can be cleared.
        dsp_match = nullptr;
        --entries_matching_dsp_to_reconcile;
      }

      // Update the data store with the new prepopulated data. Preserve user
      // edits to the name and keyword.
      MergeIntoEngineData(existing_url->data(), *prepopulated_url.get(),
                          merge_option);
      // Update last_modified to ensure that if this entry is later merged with
      // entries from Sync, the conflict resolution logic knows that this was
      // updated and propagates the new values to the server.
      prepopulated_url->last_modified = base::Time::Now();
      actions.edited_engines.emplace_back(existing_url, *prepopulated_url);
    } else {
      actions.added_engines.push_back(*prepopulated_url);
    }
  }

  // The block above removed all the URLs from the `id_to_turl` map that were
  // found in the prepopulate data. Any remaining URLs that haven't been
  // user-edited or made default can be removed from the data store.
  for (const auto& [id, template_url] : id_to_turl) {
    if (entries_matching_dsp_to_reconcile > 0 &&
        MatchesDefaultSearchProvider(template_url, default_search_provider,
                                     /*check_keyword=*/true)) {
      --entries_matching_dsp_to_reconcile;

      RecordEntryPreservationReason(
          template_url == dsp_match
              ? EntryPreservationReason::kMatchesDefaultSearchProvider
              : EntryPreservationReason::kMatchesOtherDefaultSearchProvider);
      continue;  // Preserve the not-yet-matched default search provider.
    }

    if (!template_url->safe_for_autoreplace()) {
      RecordEntryPreservationReason(
          EntryPreservationReason::kNotSafeForAutoreplace);
      continue;  // Preserve user modified entries.
    }

    if (template_url->CreatedByRegulatoryProgram()) {
      RecordEntryPreservationReason(
          EntryPreservationReason::kNonPrepopulatedFromRegulatoryProgram);
      // Preserve the entry created from regulatory extensions, but reset its
      // `prepopulate_id`.
      // TODO(crbug.com/480856411): Revisit whether clearing the
      // `prepopulate_id` is still desirable.
      TemplateURLData data = template_url->data();
      data.prepopulate_id = 0;
      actions.edited_engines.emplace_back(template_url, data);
      continue;
    }

    // Remove all other entries.
    actions.removed_engines.push_back(template_url);
  }

  RecordDefaultSearchMatchCount(entries_matching_dsp_to_reconcile,
                                /*is_unreconciled_count=*/true);

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
    template_url_starter_pack_data::StarterPackId starter_pack_id =
        turl->starter_pack_id();
    if (starter_pack_id !=
        template_url_starter_pack_data::StarterPackId::kNone) {
      id_to_turl[static_cast<int>(starter_pack_id)] = turl.get();
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
      MergeIntoEngineData(existing_url->data(), *url.get(), merge_option);
      // Update last_modified to ensure that if this entry is later merged with
      // entries from Sync, the conflict resolution logic knows that this was
      // updated and propagates the new values to the server.
      url->last_modified = base::Time::Now();
      actions.edited_engines.emplace_back(existing_url, *url);
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
    MergeEnginesFromPrepopulateData(
        service, &prepopulated_urls, template_urls, default_search_provider,
        template_url_data_resolver, removed_keyword_guids);
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

bool IsAimZeroStateURL(const GURL& url) {
  if (!google_util::IsGoogleDomainUrl(
          url, google_util::ALLOW_SUBDOMAIN,
          google_util::DISALLOW_NON_STANDARD_PORTS)) {
    return false;
  }
  std::string udm;
  bool has_udm = net::GetValueForKeyInQuery(url, "udm", &udm);
  return has_udm && udm == kAimUdmQueryParameterValue &&
         !google_util::IsGoogleSearchUrl(url);
}

GURL GetUrlForAim(
    TemplateURLService* turl_service,
    omnibox::ChromeAimEntryPoint aim_entrypoint,
    const base::Time& query_start_time,
    const std::u16string& query_text,
    const std::optional<lens::LensOverlayInvocationSource> invocation_source,
    std::map<std::string, std::string> additional_params) {
  GURL result_url = GetBaseSearchUrl(turl_service, aim_entrypoint,
                                     /*is_aim_search=*/true, query_start_time,
                                     query_text, additional_params);
  if (invocation_source.has_value()) {
    // If the invocation source is set, send the contextual tasks invocation
    // source, as only the unmigrated LensOverlay flow, which uses a different
    // code path for url generation, should be sending non contextual tasks
    // invocation sources. This prevents non LensOverlay flows (i.e. the
    // omnibox popup) from polluting the metrics for the existing LensOverlay
    // feature.
    result_url = lens::AppendInvocationSourceParamToURL(
        result_url, *invocation_source, /*is_contextual_tasks=*/true);
  }
  return result_url;
}

GURL GetUrlForMultimodalSearch(
    TemplateURLService* turl_service,
    bool is_aim_search,
    omnibox::ChromeAimEntryPoint aim_entrypoint,
    const base::Time& query_start_time,
    const std::string& search_session_id,
    const std::unique_ptr<lens::LensOverlayRequestId> request_id,
    const std::optional<lens::LensOverlayInvocationSource> invocation_source,
    const std::string& lns_surface,
    const std::u16string& query_text,
    std::map<std::string, std::string> additional_params) {
  GURL result_url =
      GetBaseSearchUrl(turl_service, aim_entrypoint, is_aim_search,
                       query_start_time, query_text, additional_params);
  if (request_id) {
    std::string serialized_request_id;
    CHECK(request_id->SerializeToString(&serialized_request_id));
    std::string encoded_request_id;
    base::Base64UrlEncode(serialized_request_id,
                          base::Base64UrlEncodePolicy::OMIT_PADDING,
                          &encoded_request_id);
    result_url = net::AppendOrReplaceQueryParameter(
        result_url, kVisualRequestIdQueryParameter, encoded_request_id);
  }

  if (invocation_source.has_value()) {
    // If the invocation source is set, this is a Lens query that is migrated
    // to the common ContextualSearchSessionHandle, which is only used for the
    // contextual tasks flow.
    result_url = lens::AppendInvocationSourceParamToURL(
        result_url, *invocation_source, /*is_contextual_tasks=*/true);
  }
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
    const std::optional<lens::LensOverlayInvocationSource> invocation_source,
    const std::string& lns_surface,
    const std::u16string& query_text,
    std::map<std::string, std::string> additional_params) {
  GURL result_url =
      GetBaseSearchUrl(turl_service, aim_entrypoint, is_aim_search,
                       query_start_time, query_text, additional_params);
  std::string serialized_contextual_inputs;
  CHECK(contextual_inputs->SerializeToString(&serialized_contextual_inputs));
  std::string encoded_contextual_inputs;
  base::Base64UrlEncode(serialized_contextual_inputs,
                        base::Base64UrlEncodePolicy::OMIT_PADDING,
                        &encoded_contextual_inputs);
  if (invocation_source.has_value()) {
    // If the invocation source is set, this is a Lens query that is migrated
    // to the common ContextualSearchSessionHandle, which is only used for the
    // contextual tasks flow.
    result_url = lens::AppendInvocationSourceParamToURL(
        result_url, *invocation_source, /*is_contextual_tasks=*/true);
  }
  result_url = net::AppendOrReplaceQueryParameter(
      result_url, kContextualInputsParameterKey, encoded_contextual_inputs);
  result_url = net::AppendOrReplaceQueryParameter(
      result_url, kSearchSessionIdParameterKey, search_session_id);
  result_url = net::AppendOrReplaceQueryParameter(
      result_url, kLnsSurfaceParameterKey, lns_surface);
  return result_url;
}
