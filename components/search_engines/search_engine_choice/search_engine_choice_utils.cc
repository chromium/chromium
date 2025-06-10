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
#include "base/feature_list.h"
#include "base/metrics/histogram_functions.h"
#include "base/metrics/user_metrics.h"
#include "base/not_fatal_until.h"
#include "base/strings/stringprintf.h"
#include "base/types/expected.h"
#include "base/values.h"
#include "base/version.h"
#include "base/version_info/version_info.h"
#include "build/branding_buildflags.h"
#include "components/country_codes/country_codes.h"
#include "components/policy/core/common/policy_service.h"
#include "components/policy/policy_constants.h"
#include "components/prefs/pref_service.h"
#include "components/search_engines/choice_made_location.h"
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

using ::country_codes::CountryId;

namespace search_engines {

namespace {
// Serialization keys for `ChoiceScreenDisplayState`.
constexpr char kDisplayStateCountryIdKey[] = "country_id";
constexpr char kDisplayStateSearchEnginesKey[] = "search_engines";
constexpr char kDisplayStateSelectedEngineIndexKey[] = "selected_engine_index";

}  // namespace

ChoiceScreenDisplayState::ChoiceScreenDisplayState(
    std::vector<SearchEngineType> search_engines,
    CountryId country_id,
    std::optional<int> selected_engine_index)
    : search_engines(std::move(search_engines)),
      selected_engine_index(selected_engine_index),
      country_id(country_id) {}

ChoiceScreenDisplayState::ChoiceScreenDisplayState(
    const ChoiceScreenDisplayState& other) = default;

ChoiceScreenDisplayState::~ChoiceScreenDisplayState() = default;

base::Value::Dict ChoiceScreenDisplayState::ToDict() const {
  auto dict = base::Value::Dict();

  dict.Set(kDisplayStateCountryIdKey, country_id.Serialize());

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
  std::optional<int> parsed_country_code =
      dict.FindInt(kDisplayStateCountryIdKey);
  std::optional<CountryId> parsed_country_id;
  if (parsed_country_code.has_value()) {
    parsed_country_id = CountryId::Deserialize(parsed_country_code.value());
  }
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
    CountryId country_id,
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

void RecordChoiceScreenDefaultSearchProviderType(
    SearchEngineType engine_type,
    ChoiceMadeLocation choice_location) {
  base::UmaHistogramEnumeration(
      kSearchEngineChoiceScreenDefaultSearchEngineTypeHistogram, engine_type,
      SEARCH_ENGINE_MAX);
  if (choice_location == ChoiceMadeLocation::kChoiceScreen) {
    base::UmaHistogramEnumeration(
        kSearchEngineChoiceScreenDefaultSearchEngineType2Histogram, engine_type,
        SEARCH_ENGINE_MAX);
  }
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

void WipeSearchEngineChoicePrefs(PrefService& profile_prefs,
                                 SearchEngineChoiceWipeReason reason) {
  base::UmaHistogramEnumeration(kSearchEngineChoiceWipeReasonHistogram, reason);
  if (reason == SearchEngineChoiceWipeReason::kDeviceRestored &&
      profile_prefs.HasPrefPath(
          prefs::kDefaultSearchProviderChoiceScreenCompletionTimestamp)) {
    profile_prefs.SetInt64(
        prefs::kDefaultSearchProviderChoiceInvalidationTimestamp,
        base::Time::Now().ToDeltaSinceWindowsEpoch().InSeconds());
  }

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

base::expected<ChoiceCompletionMetadata, ChoiceCompletionMetadata::ParseError>
GetChoiceCompletionMetadata(const PrefService& prefs) {
  if (!prefs.HasPrefPath(
          prefs::kDefaultSearchProviderChoiceScreenCompletionVersion)) {
    return base::unexpected(
        prefs.HasPrefPath(
            prefs::kDefaultSearchProviderChoiceScreenCompletionTimestamp)
            ? ChoiceCompletionMetadata::ParseError::kMissingVersion
            : ChoiceCompletionMetadata::ParseError::kAbsent);
  }

  base::Version version(prefs.GetString(
      prefs::kDefaultSearchProviderChoiceScreenCompletionVersion));
  if (!version.IsValid() ||
      version.components().size() !=
          version_info::GetVersion().components().size()) {
    return base::unexpected(
        ChoiceCompletionMetadata::ParseError::kInvalidVersion);
  }

  // Note: Other error conditions don't have dedicated handling, so we log all
  // of them as `kOther`.

  base::Time timestamp =
      base::Time::FromDeltaSinceWindowsEpoch(base::Seconds(prefs.GetInt64(
          prefs::kDefaultSearchProviderChoiceScreenCompletionTimestamp)));

  if (timestamp.is_null()) {
    return base::unexpected(ChoiceCompletionMetadata::ParseError::kOther);
  }

  return ChoiceCompletionMetadata{
      .timestamp = timestamp,
      .version = version,
  };
}

void ClearSearchEngineChoiceInvalidation(PrefService& prefs) {
  prefs.ClearPref(prefs::kDefaultSearchProviderChoiceInvalidationTimestamp);
}

bool IsSearchEngineChoiceInvalid(PrefService& prefs) {
  if (!base::FeatureList::IsEnabled(
          switches::kInvalidateSearchEngineChoiceOnDeviceRestoreDetection)) {
    // Ensure that we never consider a search engine choice invalid when the
    // feature is disabled. This could happen if a user changes experiment
    // groups for example.
    return false;
  }

  if (prefs.GetInt64(prefs::kDefaultSearchProviderChoiceInvalidationTimestamp) >
      0) {
    CHECK(!prefs.HasPrefPath(
              prefs::kDefaultSearchProviderChoiceScreenCompletionTimestamp),
          base::NotFatalUntil::M140);
    return true;
  }

  return false;
}

void SetChoiceCompletionMetadata(PrefService& prefs,
                                 ChoiceCompletionMetadata metadata) {
  // Verify that any invalidation has already been cleared. Otherwise the
  // completion
  // will be ignored.
  CHECK(!IsSearchEngineChoiceInvalid(prefs), base::NotFatalUntil::M140);

  prefs.SetInt64(prefs::kDefaultSearchProviderChoiceScreenCompletionTimestamp,
                 metadata.timestamp.ToDeltaSinceWindowsEpoch().InSeconds());
  prefs.SetString(prefs::kDefaultSearchProviderChoiceScreenCompletionVersion,
                  metadata.version.GetString());
}

std::optional<base::Time> GetChoiceScreenCompletionTimestamp(
    PrefService& prefs) {
  auto metadata = GetChoiceCompletionMetadata(prefs);
  if (!metadata.has_value()) {
    return std::nullopt;
  }

  return metadata->timestamp;
}

#if !BUILDFLAG(IS_ANDROID)
std::u16string GetMarketingSnippetString(
    const TemplateURLData& template_url_data) {
  constexpr bool kEnableBuiltinSearchProviderAssets =
      !!BUILDFLAG(ENABLE_BUILTIN_SEARCH_PROVIDER_ASSETS);

  // TODO(crbug.com/420943295): `GetMarketingSnippetResourceId()` is generated
  // code. The flag-gating should be moved there directly.
  int snippet_resource_id =
      kEnableBuiltinSearchProviderAssets
          ? GetMarketingSnippetResourceId(template_url_data.keyword())
          : -1;

  return snippet_resource_id == -1
             ? l10n_util::GetStringFUTF16(
                   IDS_SEARCH_ENGINE_FALLBACK_MARKETING_SNIPPET,
                   template_url_data.short_name())
             : l10n_util::GetStringUTF16(snippet_resource_id);
}

#endif  // !BUILDFLAG(IS_ANDROID)

}  // namespace search_engines
