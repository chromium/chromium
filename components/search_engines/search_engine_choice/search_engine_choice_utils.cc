// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/search_engines/search_engine_choice/search_engine_choice_utils.h"

#include <optional>
#include <string>

#include "base/check_deref.h"
#include "base/check_is_test.h"
#include "base/check_op.h"
#include "base/command_line.h"
#include "base/containers/fixed_flat_map.h"
#include "base/containers/to_vector.h"
#include "base/metrics/histogram_functions.h"
#include "base/metrics/user_metrics.h"
#include "base/strings/stringprintf.h"
#include "base/values.h"
#include "base/version.h"
#include "build/branding_buildflags.h"
#include "components/country_codes/country_codes.h"
#include "components/policy/core/common/policy_service.h"
#include "components/policy/policy_constants.h"
#include "components/prefs/pref_service.h"
#include "components/search_engines/eea_countries_ids.h"
#include "components/search_engines/search_engine_type.h"
#include "components/search_engines/search_engines_pref_names.h"
#include "components/search_engines/search_engines_switches.h"
#include "components/search_engines/search_terms_data.h"
#include "components/search_engines/template_url_data.h"
#include "components/search_engines/template_url_prepopulate_data.h"
#include "components/search_engines/template_url_service.h"
#include "components/signin/public/base/signin_switches.h"
#include "components/strings/grit/components_strings.h"
#include "ui/base/l10n/l10n_util.h"

#if !BUILDFLAG(IS_ANDROID)
#include "components/grit/components_scaled_resources.h"  // nogncheck
#include "ui/resources/grit/ui_resources.h"               // nogncheck
#endif

namespace search_engines {

namespace {
#if !BUILDFLAG(IS_ANDROID)
// Defines `kSearchEngineResourceIdMap`.
#include "components/search_engines/generated_search_engine_resource_ids-inc.cc"
#endif

// Serialization keys for `ChoiceScreenDisplayState`.
constexpr char kDisplayStateCountryIdKey[] = "country_id";
constexpr char kDisplayStateSearchEnginesKey[] = "search_engines";
constexpr char kDisplayStateSelectedEngineIndexKey[] = "selected_engine_index";

}  // namespace

ChoiceScreenDisplayState::ChoiceScreenDisplayState(
    std::vector<SearchEngineType> search_engines,
    int country_id,
    std::optional<int> selected_engine_index)
    : search_engines(std::move(search_engines)),
      selected_engine_index(selected_engine_index),
      country_id(country_id) {}

ChoiceScreenDisplayState::ChoiceScreenDisplayState(
    const ChoiceScreenDisplayState& other) = default;

ChoiceScreenDisplayState::~ChoiceScreenDisplayState() = default;

base::Value::Dict ChoiceScreenDisplayState::ToDict() const {
  auto dict = base::Value::Dict();

  dict.Set(kDisplayStateCountryIdKey, country_id);

  base::Value::List* search_engines_array =
      dict.EnsureList(kDisplayStateSearchEnginesKey);
  for (SearchEngineType search_engine_type : search_engines) {
    search_engines_array->Append(static_cast<int>(search_engine_type));
  }

  if (selected_engine_index.has_value()) {
    dict.Set(kDisplayStateSelectedEngineIndexKey,
             selected_engine_index.value());
  }

  return dict;
}

// static
std::optional<ChoiceScreenDisplayState> ChoiceScreenDisplayState::FromDict(
    const base::Value::Dict& dict) {
  std::optional<int> parsed_country_id =
      dict.FindInt(kDisplayStateCountryIdKey);
  const base::Value::List* parsed_search_engines =
      dict.FindList(kDisplayStateSearchEnginesKey);
  std::optional<int> parsed_selected_engine_index =
      dict.FindInt(kDisplayStateSelectedEngineIndexKey);

  if (dict.FindBool("list_is_modified_by_current_default").value_or(false)) {
    // We stopped writing this field, as we totally stopped including the
    // current default in the list. If we find old persisted data where
    // this is `true`, just consider it invalid, as we wouldn't log anything
    // for it anyway.
    // TODO(crbug.com/343915066): Entries older than 14 days are considered
    // expired, we can remove this code branch and the dictionary key in M130+
    return std::nullopt;
  }

  if (!parsed_country_id.has_value() ||
      !parsed_search_engines) {
    return std::nullopt;
  }

  std::vector<SearchEngineType> search_engines;
  for (const base::Value& search_engine_type : *parsed_search_engines) {
    search_engines.push_back(
        static_cast<SearchEngineType>(search_engine_type.GetInt()));
  }

  return ChoiceScreenDisplayState(
      search_engines, parsed_country_id.value(),
      parsed_selected_engine_index);
}

ChoiceScreenData::ChoiceScreenData(
    TemplateURL::OwnedTemplateURLVector owned_template_urls,
    int country_id,
    const SearchTermsData& search_terms_data)
    : search_engines_(std::move(owned_template_urls)),
      display_state_(ChoiceScreenDisplayState(
          base::ToVector(
              search_engines_,
              [&search_terms_data](const std::unique_ptr<TemplateURL>& t_url) {
                return t_url->GetEngineType(search_terms_data);
              }),
          country_id)) {}

ChoiceScreenData::~ChoiceScreenData() = default;

bool IsEeaChoiceCountry(int country_id) {
  // Consider the search engine list command line country override as an EEA
  // region country to display the search engine choice screen.
  return HasSearchEngineCountryListOverride()
             ? true
             : kEeaChoiceCountriesIds.contains(country_id);
}

void RecordChoiceScreenProfileInitCondition(
    SearchEngineChoiceScreenConditions condition) {
  base::UmaHistogramEnumeration(
      kSearchEngineChoiceScreenProfileInitConditionsHistogram, condition);
}

void RecordChoiceScreenNavigationCondition(
    SearchEngineChoiceScreenConditions condition) {
  base::UmaHistogramEnumeration(
      kSearchEngineChoiceScreenNavigationConditionsHistogram, condition);
}

void RecordChoiceScreenEvent(SearchEngineChoiceScreenEvents event) {
  base::UmaHistogramEnumeration(kSearchEngineChoiceScreenEventsHistogram,
                                event);

  if (event == SearchEngineChoiceScreenEvents::kChoiceScreenWasDisplayed ||
      event == SearchEngineChoiceScreenEvents::kFreChoiceScreenWasDisplayed ||
      event == SearchEngineChoiceScreenEvents::
                   kProfileCreationChoiceScreenWasDisplayed) {
    base::RecordAction(
        base::UserMetricsAction("SearchEngineChoiceScreenShown"));
  }
}

void RecordChoiceScreenDefaultSearchProviderType(SearchEngineType engine_type) {
  base::UmaHistogramEnumeration(
      kSearchEngineChoiceScreenDefaultSearchEngineTypeHistogram, engine_type,
      SEARCH_ENGINE_MAX);
}

void RecordChoiceScreenSelectedIndex(int selected_engine_index) {
  base::UmaHistogramExactLinear(
      kSearchEngineChoiceScreenSelectedEngineIndexHistogram,
      selected_engine_index,
      TemplateURLPrepopulateData::kMaxEeaPrepopulatedEngines);
}

void RecordChoiceScreenPositionsCountryMismatch(bool has_mismatch) {
  base::UmaHistogramBoolean(
      kSearchEngineChoiceScreenShowedEngineAtCountryMismatchHistogram,
      has_mismatch);
}

void RecordChoiceScreenPositions(
    const std::vector<SearchEngineType>& displayed_search_engines) {
  for (int i = 0; i < static_cast<int>(displayed_search_engines.size()); ++i) {
    // Using `UmaHistogramSparse()` instead of `UmaHistogramEnumeration()` as
    // it is more space efficient when logging just one value (in most cases)
    // for each index.
    base::UmaHistogramSparse(
        base::StringPrintf(
            kSearchEngineChoiceScreenShowedEngineAtHistogramPattern, i),
        displayed_search_engines[i]);
  }
}

void RecordUnexpectedSearchProvider(const TemplateURLData& data) {
  base::UmaHistogramSparse(kSearchEngineChoiceUnexpectedIdHistogram,
                           data.prepopulate_id);
}

void WipeSearchEngineChoicePrefs(PrefService& profile_prefs,
                                 WipeSearchEngineChoiceReason reason) {
  base::UmaHistogramEnumeration(kSearchEngineChoiceWipeReasonHistogram, reason);
  profile_prefs.ClearPref(
      prefs::kDefaultSearchProviderChoiceScreenCompletionTimestamp);
  profile_prefs.ClearPref(
      prefs::kDefaultSearchProviderChoiceScreenCompletionVersion);
  profile_prefs.ClearPref(
      prefs::kDefaultSearchProviderPendingChoiceScreenDisplayState);

#if BUILDFLAG(IS_IOS)
    profile_prefs.ClearPref(
        prefs::kDefaultSearchProviderChoiceScreenSkippedCount);
#endif
}

std::optional<SearchEngineCountryOverride> GetSearchEngineCountryOverride() {
  base::CommandLine* command_line = base::CommandLine::ForCurrentProcess();
  if (!command_line->HasSwitch(switches::kSearchEngineChoiceCountry)) {
    return std::nullopt;
  }

  std::string country_id =
      base::CommandLine::ForCurrentProcess()->GetSwitchValueASCII(
          switches::kSearchEngineChoiceCountry);

  if (country_id == switches::kDefaultListCountryOverride) {
    return SearchEngineCountryListOverride::kEeaDefault;
  }
  if (country_id == switches::kEeaListCountryOverride) {
    return SearchEngineCountryListOverride::kEeaAll;
  }
  return country_codes::CountryStringToCountryID(country_id);
}

bool HasSearchEngineCountryListOverride() {
  std::optional<SearchEngineCountryOverride> country_override =
      GetSearchEngineCountryOverride();
  if (!country_override.has_value()) {
    return false;
  }

  return absl::holds_alternative<SearchEngineCountryListOverride>(
      country_override.value());
}

#if !BUILDFLAG(IS_ANDROID)
std::u16string GetMarketingSnippetString(
    const TemplateURLData& template_url_data) {
  int snippet_resource_id =
      GetMarketingSnippetResourceId(template_url_data.keyword());

  return snippet_resource_id == -1
             ? l10n_util::GetStringFUTF16(
                   IDS_SEARCH_ENGINE_FALLBACK_MARKETING_SNIPPET,
                   template_url_data.short_name())
             : l10n_util::GetStringUTF16(snippet_resource_id);
}

int GetIconResourceId(const std::u16string& engine_keyword) {
  // `kSearchEngineResourceIdMap` is defined in
  // `components/search_engines/generated_search_engine_resource_ids-inc.cc`
  const base::fixed_flat_map<std::u16string_view, int,
                             kSearchEngineResourceIdMap.size()>::const_iterator
      iterator = kSearchEngineResourceIdMap.find(engine_keyword);
  return iterator == kSearchEngineResourceIdMap.cend() ? -1 : iterator->second;
}

#endif

}  // namespace search_engines
