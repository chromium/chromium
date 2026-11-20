// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/autofill/core/browser/metrics/stored_profile_metrics.h"

#include <stddef.h>
#include <stdint.h>

#include <algorithm>
#include <string>
#include <string_view>
#include <vector>

#include "base/check.h"
#include "base/check_op.h"
#include "base/containers/span.h"
#include "base/i18n/char_iterator.h"
#include "base/i18n/icubridge/icu_bridge.h"
#include "base/i18n/icubridge/normalizer.h"
#include "base/metrics/histogram_functions.h"
#include "base/strings/strcat.h"
#include "base/time/time.h"
#include "components/autofill/core/browser/country_type.h"
#include "components/autofill/core/browser/data_model/addresses/autofill_profile.h"
#include "components/autofill/core/browser/field_types.h"
#include "components/autofill/core/browser/metrics/autofill_metrics_util.h"
#include "components/autofill/core/common/autofill_clock.h"
#include "components/autofill/core/common/autofill_constants.h"
#include "components/autofill/core/common/dense_set.h"
#include "third_party/icu/source/common/unicode/uchar.h"
#include "third_party/icu/source/common/unicode/uscript.h"
#include "third_party/icu/source/common/unicode/utypes.h"

namespace autofill::autofill_metrics {

namespace {

// Top-level name and address text fields evaluated for script classification.
// Excludes leaf sub-components (covered by `NAME_FULL` and
// `ADDRESS_HOME_STREET_ADDRESS`), `ADDRESS_HOME_COUNTRY` (2-letter ASCII ISO
// code), postal codes (`ADDRESS_HOME_ZIP`, `ADDRESS_HOME_SORTING_CODE`), and
// phonetic `ALTERNATIVE_*` names.
constexpr FieldTypeSet kEvaluatedFieldTypes = {
    NAME_FULL,
    ADDRESS_HOME_STREET_ADDRESS,
    ADDRESS_HOME_DEPENDENT_LOCALITY,
    ADDRESS_HOME_CITY,
    ADDRESS_HOME_STATE,
    ADDRESS_HOME_ADMIN_LEVEL2,
};

constexpr DenseSet<AutofillProfileCharacterScriptType> kLatinScripts = {
    AutofillProfileCharacterScriptType::kAsciiOnly,
    AutofillProfileCharacterScriptType::kLatin1Supplement,
    AutofillProfileCharacterScriptType::kLatinExtended,
};

DenseSet<AutofillProfileCharacterScriptType> ClassifyString(
    std::u16string_view text) {
  // Do NOT reuse this for general Autofill string comparison/deduplication —
  // use `normalization::NormalizeForComparison()` instead. Here we
  // intentionally preserve diacritics (to distinguish ASCII, Latin-1
  // Supplement, and Latin Extended) and only apply Unicode NFC so NFD
  // combining marks (e.g. 'e' + U+0301) compose into their precomposed code
  // point ('é') instead of being skipped as non-letters.
  const std::u16string normalized =
      base::i18n::IcuBridge::GetInstance().normalizer().Normalize(
          base::i18n::IcuBridge::Normalizer::NormalizationForm::NFC, text);
  DenseSet<AutofillProfileCharacterScriptType> scripts;
  for (base::i18n::UTF16CharIterator iter(normalized); !iter.end();
       iter.Advance()) {
    const UChar32 c = iter.get();
    const uint32_t category_mask = U_GET_GC_MASK(c);

    // Skip non-letter characters (e.g. digits, punctuation, whitespace).
    const bool is_letter = (category_mask & U_GC_L_MASK) != 0;
    if (!is_letter) {
      continue;
    }

    UErrorCode status = U_ZERO_ERROR;
    const UScriptCode script = uscript_getScript(c, &status);
    if (U_FAILURE(status)) {
      continue;
    }

    // Only major scripts have dedicated buckets; others fall into
    // `kOtherNonLatin`. Note that `uscript_getScript` never returns composite
    // codes like `USCRIPT_JAPANESE` or `USCRIPT_SIMPLIFIED_HAN` (all Han
    // characters return `USCRIPT_HAN`).
    switch (script) {
      case USCRIPT_LATIN: {
        // Skip uncased Latin letters such as the Spanish/Portuguese ordinal
        // indicators 'ª' and 'º'.
        const bool is_cased_letter = (category_mask & U_GC_LC_MASK) != 0;
        if (!is_cased_letter) {
          continue;
        }
        switch (ublock_getCode(c)) {
          case UBLOCK_BASIC_LATIN:
            scripts.insert(AutofillProfileCharacterScriptType::kAsciiOnly);
            break;
          case UBLOCK_HALFWIDTH_AND_FULLWIDTH_FORMS:
            // Ignore fullwidth Latin letters (e.g. "Ａ", "Ｂ"), which are
            // common in Japanese addresses and would otherwise classify them as
            // `kMixed`.
            break;
          case UBLOCK_LATIN_1_SUPPLEMENT:
            scripts.insert(
                AutofillProfileCharacterScriptType::kLatin1Supplement);
            break;
          default:
            scripts.insert(AutofillProfileCharacterScriptType::kLatinExtended);
            break;
        }
        break;
      }
      case USCRIPT_CYRILLIC:
        scripts.insert(AutofillProfileCharacterScriptType::kCyrillic);
        break;
      case USCRIPT_ARABIC:
        scripts.insert(AutofillProfileCharacterScriptType::kArabic);
        break;
      case USCRIPT_HAN:
      case USCRIPT_HIRAGANA:
      case USCRIPT_KATAKANA:
      case USCRIPT_HANGUL:
      case USCRIPT_BOPOMOFO:
        scripts.insert(AutofillProfileCharacterScriptType::kCjk);
        break;
      case USCRIPT_DEVANAGARI:
      case USCRIPT_BENGALI:
      case USCRIPT_GURMUKHI:
      case USCRIPT_GUJARATI:
      case USCRIPT_ORIYA:
      case USCRIPT_TAMIL:
      case USCRIPT_TELUGU:
      case USCRIPT_KANNADA:
      case USCRIPT_MALAYALAM:
      case USCRIPT_SINHALA:
      case USCRIPT_OL_CHIKI:
      case USCRIPT_MEITEI_MAYEK:
        scripts.insert(AutofillProfileCharacterScriptType::kIndic);
        break;
      case USCRIPT_GREEK:
        scripts.insert(AutofillProfileCharacterScriptType::kGreek);
        break;
      case USCRIPT_HEBREW:
        scripts.insert(AutofillProfileCharacterScriptType::kHebrew);
        break;
      case USCRIPT_THAI:
        scripts.insert(AutofillProfileCharacterScriptType::kThai);
        break;
      case USCRIPT_COMMON:
      case USCRIPT_INHERITED:
        break;
      default:
        scripts.insert(AutofillProfileCharacterScriptType::kOtherNonLatin);
        break;
    }
  }
  return scripts;
}

AutofillProfileCharacterScriptType ClassifyProfileScripts(
    const AutofillProfile& profile) {
  DenseSet<AutofillProfileCharacterScriptType> scripts;
  for (FieldType type : kEvaluatedFieldTypes) {
    const std::u16string value = profile.GetRawInfo(type);
    if (!value.empty()) {
      scripts.insert_all(ClassifyString(value));
    }
  }

  if (scripts.empty()) {
    return AutofillProfileCharacterScriptType::kUnspecified;
  }
  if (kLatinScripts.contains_all(scripts)) {
    return *scripts.rbegin();
  }
  return scripts.size() == 1 ? *scripts.begin()
                             : AutofillProfileCharacterScriptType::kMixed;
}

}  // namespace

void LogStoredProfileCountStatistics(AutofillProfileRecordTypeCategory category,
                                     const StoredProfileCounts& counts) {
  const std::string kSuffix = GetProfileCategorySuffix(category);

  base::UmaHistogramCounts1M("Autofill.StoredProfileCount." + kSuffix,
                             counts.total);
  // For users without any profiles do not record the other metrics.
  if (counts.total == 0) {
    return;
  }
  DCHECK_LE(counts.disused, counts.total);
  size_t used = counts.total - counts.disused;
  base::UmaHistogramCounts1000("Autofill.StoredProfileUsedCount." + kSuffix,
                               used);
  base::UmaHistogramCounts1000("Autofill.StoredProfileDisusedCount." + kSuffix,
                               counts.disused);
  base::UmaHistogramPercentage(
      "Autofill.StoredProfileUsedPercentage." + kSuffix,
      100 * used / counts.total);
}

void LogStoredProfileDaysSinceLastUse(
    AutofillProfileRecordTypeCategory category,
    size_t days) {
  base::UmaHistogramCounts1000(
      base::StrCat({"Autofill.DaysSinceLastUse.StoredProfile.",
                    GetProfileCategorySuffix(category)}),
      days);
}

void LogStoredProfileMetrics(
    const std::vector<const AutofillProfile*>& profiles) {
  const base::Time now = AutofillClock::Now();
  // Counts stored profile metrics for all profile of the given `category` and
  // emits UMA metrics for them.
  auto count_and_log = [&](AutofillProfileRecordTypeCategory category) {
    StoredProfileCounts counts;
    for (const AutofillProfile* profile : profiles) {
      if (category != GetCategoryOfProfile(*profile)) {
        continue;
      }
      const base::TimeDelta time_since_last_use =
          now - profile->usage_history().use_date();
      LogStoredProfileDaysSinceLastUse(category, time_since_last_use.InDays());
      counts.total++;
      counts.disused += time_since_last_use > kDisusedDataModelTimeDelta;
    }
    LogStoredProfileCountStatistics(category, counts);
  };

  count_and_log(AutofillProfileRecordTypeCategory::kLocalOrSyncable);
  count_and_log(AutofillProfileRecordTypeCategory::kAccountChrome);
  count_and_log(AutofillProfileRecordTypeCategory::kAccountNonChrome);
  count_and_log(AutofillProfileRecordTypeCategory::kAccountHome);
  count_and_log(AutofillProfileRecordTypeCategory::kAccountWork);
  count_and_log(AutofillProfileRecordTypeCategory::kAccountNameEmail);

  // Update the metrics when adding a new address type.
  static_assert(AutofillProfile::RecordType::kMaxValue ==
                AutofillProfile::RecordType::kAccountNameEmail);

  base::UmaHistogramCounts1M("Autofill.StoredProfileCount.Total",
                             profiles.size());

  base::UmaHistogramCounts1M(
      "Autofill.StoredProfileCount.TotalPostalAddressProfiles",
      std::ranges::count_if(profiles, [](const AutofillProfile* p) {
        return autofill_metrics::IsPostalAddress(*p);
      }));
}

void LogStoredProfileCountWithAlternativeName(
    base::span<const AutofillProfile* const> profiles) {
  size_t count =
      std::ranges::count_if(profiles, [](const AutofillProfile* profile) {
        return profile->GetAddressCountryCode() == AddressCountryCode("JP") &&
               profile->HasInfo(ALTERNATIVE_FULL_NAME);
      });

  base::UmaHistogramCounts100("Autofill.StoredProfileCount.WithAlternativeName",
                              count);
}

void LogStoredProfileScriptTypeMetrics(
    base::span<const AutofillProfile> profiles) {
  CHECK(!profiles.empty());

  size_t ascii_only_count = 0;
  size_t latin1_supplement_count = 0;
  size_t latin_extended_count = 0;

  for (const AutofillProfile& profile : profiles) {
    const AutofillProfileCharacterScriptType script_type =
        ClassifyProfileScripts(profile);
    base::UmaHistogramEnumeration("Autofill.StoredProfile.CharacterScriptType",
                                  script_type);

    switch (script_type) {
      case AutofillProfileCharacterScriptType::kAsciiOnly:
        ascii_only_count++;
        break;
      case AutofillProfileCharacterScriptType::kLatin1Supplement:
        latin1_supplement_count++;
        break;
      case AutofillProfileCharacterScriptType::kLatinExtended:
        latin_extended_count++;
        break;
      case AutofillProfileCharacterScriptType::kUnspecified:
      case AutofillProfileCharacterScriptType::kCyrillic:
      case AutofillProfileCharacterScriptType::kArabic:
      case AutofillProfileCharacterScriptType::kCjk:
      case AutofillProfileCharacterScriptType::kIndic:
      case AutofillProfileCharacterScriptType::kGreek:
      case AutofillProfileCharacterScriptType::kHebrew:
      case AutofillProfileCharacterScriptType::kThai:
      case AutofillProfileCharacterScriptType::kOtherNonLatin:
      case AutofillProfileCharacterScriptType::kMixed:
        break;
    }
  }

  base::UmaHistogramCounts100(
      "Autofill.StoredProfileCountByScriptType.AsciiOnly", ascii_only_count);
  base::UmaHistogramCounts100(
      "Autofill.StoredProfileCountByScriptType.Latin1Supplement",
      latin1_supplement_count);
  base::UmaHistogramCounts100(
      "Autofill.StoredProfileCountByScriptType.LatinExtended",
      latin_extended_count);
}

}  // namespace autofill::autofill_metrics
