// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/autofill/core/browser/metrics/profile_import_metrics.h"

#include <string_view>

#include "base/containers/contains.h"
#include "base/i18n/char_iterator.h"
#include "base/metrics/histogram_functions.h"
#include "base/strings/strcat.h"
#include "components/autofill/core/browser/data_manager/addresses/address_data_cleaner.h"
#include "components/autofill/core/browser/data_quality/addresses/profile_requirement_utils.h"
#include "components/autofill/core/browser/form_import/addresses/autofill_profile_import_process.h"
#include "components/autofill/core/browser/metrics/autofill_metrics_utils.h"
#include "services/metrics/public/cpp/ukm_builders.h"
#include "third_party/icu/source/common/unicode/uchar.h"

namespace autofill::autofill_metrics {

namespace {

const char* GetAddressPromptDecisionMetricsSuffix(
    AutofillClient::AddressPromptUserDecision decision) {
  switch (decision) {
    case AutofillClient::AddressPromptUserDecision::kUndefined:
      return ".Undefined";
    case AutofillClient::AddressPromptUserDecision::kUserNotAsked:
      return ".UserNotAsked";
    case AutofillClient::AddressPromptUserDecision::kAccepted:
      return ".Accepted";
    case AutofillClient::AddressPromptUserDecision::kDeclined:
      return ".Declined";
    case AutofillClient::AddressPromptUserDecision::kEditAccepted:
      return ".EditAccepted";
    case AutofillClient::AddressPromptUserDecision::kEditDeclined:
      return ".EditDeclined";
    case AutofillClient::AddressPromptUserDecision::kNever:
      return ".Never";
    case AutofillClient::AddressPromptUserDecision::kIgnored:
      return ".Ignored";
    case AutofillClient::AddressPromptUserDecision::kMessageTimeout:
      return ".MessageTimeout";
    case AutofillClient::AddressPromptUserDecision::kMessageDeclined:
      return ".MessageDeclined";
    case AutofillClient::AddressPromptUserDecision::kAutoDeclined:
      return ".AutoDeclined";
  }
  NOTREACHED();
}

AddressValidZipCodeSeparatorMetric GetAddressValidZipCodeSeparatorMetric(
    UChar32 code_point) {
  switch (code_point) {
    case 0x002D:
      return AddressValidZipCodeSeparatorMetric::kHyphenMinus;
    case 0x2013:
      return AddressValidZipCodeSeparatorMetric::kEnDash;
    case 0x2014:
      return AddressValidZipCodeSeparatorMetric::kEmDash;
    case 0x2010:
      return AddressValidZipCodeSeparatorMetric::kHyphen;
    case 0x2011:
      return AddressValidZipCodeSeparatorMetric::kNonBreakingHyphen;
    case 0x2212:
      return AddressValidZipCodeSeparatorMetric::kMinusSign;
    case 0x02D7:
      return AddressValidZipCodeSeparatorMetric::kModifierMinus;
    case 0x2012:
      return AddressValidZipCodeSeparatorMetric::kFigureDash;
    case 0x2015:
      return AddressValidZipCodeSeparatorMetric::kHorizontalBar;
    case 0xFE63:
      return AddressValidZipCodeSeparatorMetric::kSmallHyphenMinus;
    case 0xFF0D:
      return AddressValidZipCodeSeparatorMetric::kFullwidthHyphenMinus;
    case 0x0020:
      return AddressValidZipCodeSeparatorMetric::kSpace;
    case 0x00A0:
      return AddressValidZipCodeSeparatorMetric::kNonBreakingSpace;
    case 0x2002:
      return AddressValidZipCodeSeparatorMetric::kEnSpace;
    case 0x2003:
      return AddressValidZipCodeSeparatorMetric::kEmSpace;
    case 0x2009:
      return AddressValidZipCodeSeparatorMetric::kThinSpace;
    case 0x3000:
      return AddressValidZipCodeSeparatorMetric::kIdeographicSpace;
    case 0x2007:
      return AddressValidZipCodeSeparatorMetric::kFigureSpace;
    case 0x202F:
      return AddressValidZipCodeSeparatorMetric::kNarrowNonBreakingSpace;
    default:
      return AddressValidZipCodeSeparatorMetric::kOther;
  }
}

// LINT.IfChange(GetImportTypeMetricsString)

std::string_view GetImportTypeEditedMetricsString(
    AutofillProfileImportType type) {
  switch (type) {
    case AutofillProfileImportType::kNewProfile:
      return "NewProfile";
    case AutofillProfileImportType::kConfirmableMerge:
    case AutofillProfileImportType::kConfirmableMergeAndSilentUpdate:
      return "UpdateProfile";
    case AutofillProfileImportType::kProfileMigration:
    case AutofillProfileImportType::kProfileMigrationAndSilentUpdate:
      return "MigrateProfile";
    case AutofillProfileImportType::kHomeAndWorkSuperset:
      return "HomeAndWorkSuperset";
    case AutofillProfileImportType::kNameEmailSuperset:
      return "NameEmailSuperset";
    case AutofillProfileImportType::kHomeWorkNameEmailMerge:
      return "HomeWorkNameEmailMerge";
    // Those import types do not cause save/update/migrate/merge bubble to be
    // displayed, thus they will never lead to emission of this metric.
    case AutofillProfileImportType::kDuplicateImport:
    case AutofillProfileImportType::kSilentUpdate:
    case AutofillProfileImportType::kSuppressedNewProfile:
    case AutofillProfileImportType::kSuppressedConfirmableMergeAndSilentUpdate:
    case AutofillProfileImportType::kSuppressedConfirmableMerge:
    case AutofillProfileImportType::kImportTypeUnspecified:
      NOTREACHED();
  }
  NOTREACHED();
}

// LINT.ThenChange(//tools/metrics/histograms/metadata/autofill/histograms.xml:Autofill.ProfileImport.EditedType.ImportTypes)

}  // namespace

void LogAddressProfileImportUkm(
    ukm::UkmRecorder* ukm_recorder,
    ukm::SourceId source_id,
    AutofillProfileImportType import_type,
    AutofillClient::AddressPromptUserDecision user_decision,
    const ProfileImportMetadata& profile_import_metadata,
    size_t num_edited_fields,
    std::optional<AutofillProfile> import_candidate,
    const std::vector<const AutofillProfile*>& existing_profiles,
    std::string_view app_locale) {
  ukm::builders::Autofill2_AddressProfileImport builder(source_id);
  builder
      .SetAutocompleteUnrecognizedImport(
          profile_import_metadata
              .did_import_from_unrecognized_autocomplete_field)
      .SetImportType(static_cast<int64_t>(import_type))
      .SetNumberOfEditedFields(num_edited_fields)
      .SetPhoneNumberStatus(
          static_cast<int64_t>(profile_import_metadata.phone_import_status))
      .SetUserDecision(static_cast<int64_t>(user_decision))
      .SetUserHasExistingProfile(!existing_profiles.empty());
  if (import_type == AutofillProfileImportType::kNewProfile &&
      !existing_profiles.empty() && import_candidate) {
    builder.SetDuplicationRank(GetDuplicationRank(
        AddressDataCleaner::CalculateMinimalIncompatibleProfileWithTypeSets(
            *import_candidate, existing_profiles,
            AutofillProfileComparator(app_locale))));
  }
  builder.Record(ukm_recorder);
}

void LogAddressFormImportRequirementMetric(
    AddressProfileImportRequirementMetric metric) {
  base::UmaHistogramEnumeration("Autofill.AddressProfileImportRequirements",
                                metric);
}

void LogAddressFormImportRequirementMetric(const AutofillProfile& profile) {
  std::vector<AddressProfileImportRequirementMetric> requirements =
      ValidateProfileImportRequirements(profile);
  for (AddressProfileImportRequirementMetric& requirement : requirements) {
    LogAddressFormImportRequirementMetric(requirement);
  }

  bool is_zip_missing = base::Contains(
      requirements,
      AddressProfileImportRequirementMetric::kZipRequirementViolated);
  bool is_state_missing = base::Contains(
      requirements,
      AddressProfileImportRequirementMetric::kStateRequirementViolated);
  bool is_city_missing = base::Contains(
      requirements,
      AddressProfileImportRequirementMetric::kCityRequirementViolated);
  bool is_line1_missing = base::Contains(
      requirements,
      AddressProfileImportRequirementMetric::kLine1RequirementViolated);
  const auto metric =
      static_cast<AddressProfileImportCountrySpecificFieldRequirementsMetric>(
          (is_zip_missing ? 0b1 : 0) | (is_state_missing ? 0b10 : 0) |
          (is_city_missing ? 0b100 : 0) | (is_line1_missing ? 0b1000 : 0));
  base::UmaHistogramEnumeration(
      "Autofill.AddressProfileImportCountrySpecificFieldRequirements", metric);
}

void LogAddressFormImportStatusMetric(AddressProfileImportStatusMetric metric) {
  base::UmaHistogramEnumeration("Autofill.AddressProfileImportStatus", metric);
}

void LogProfileImportType(AutofillProfileImportType import_type) {
  base::UmaHistogramEnumeration("Autofill.ProfileImport.ProfileImportType",
                                import_type);
}

void LogSilentUpdatesProfileImportType(AutofillProfileImportType import_type) {
  base::UmaHistogramEnumeration(
      "Autofill.ProfileImport.SilentUpdatesProfileImportType", import_type);
}

void LogNewProfileImportDecision(
    AutofillClient::AddressPromptUserDecision decision,
    const ProfileImportMetadata& profile_import_metadata,
    const std::vector<const AutofillProfile*>& existing_profiles,
    const AutofillProfile& import_candidate,
    std::string_view app_locale) {
  constexpr std::string_view kNameBase =
      "Autofill.ProfileImport.NewProfileDecision2.";
  base::UmaHistogramEnumeration(base::StrCat({kNameBase, "Aggregate"}),
                                decision);

  if (existing_profiles.empty()) {
    base::UmaHistogramEnumeration(
        base::StrCat({kNameBase, "UserHasNoExistingProfiles"}), decision);
  } else {
    base::UmaHistogramEnumeration(
        base::StrCat({kNameBase, "UserHasExistingProfile"}), decision);

    int duplication_rank = GetDuplicationRank(
        AddressDataCleaner::CalculateMinimalIncompatibleProfileWithTypeSets(
            import_candidate, existing_profiles,
            AutofillProfileComparator(app_locale)));
    if (duplication_rank == 1) {
      base::UmaHistogramEnumeration(
          base::StrCat({kNameBase, "UserHasQuasiDuplicateProfile"}), decision);
    }
  }
  if (profile_import_metadata.observed_split_zip) {
    base::UmaHistogramEnumeration(
        "Autofill.ProfileImport.SplitZipFields.NewProfileDecision", decision);
  }
}

void LogHomeWorkNameEmailMergeImportDecision(
    AutofillClient::AddressPromptUserDecision decision) {
  base::UmaHistogramEnumeration(
      "Autofill.ProfileImport.HomeOrWorkAndNameEmailMergeDecision", decision);
}

void LogNewProfileStorageLocation(const AutofillProfile& import_candidate) {
  base::UmaHistogramEnumeration(
      "Autofill.ProfileImport.StorageNewAddressIsSavedTo",
      import_candidate.record_type());
}

void LogProfileUpdateImportDecision(
    AutofillClient::AddressPromptUserDecision decision,
    const std::vector<const AutofillProfile*>& existing_profiles,
    const AutofillProfile& import_candidate,
    std::string_view app_locale) {
  constexpr std::string_view kNameBase =
      "Autofill.ProfileImport.UpdateProfileDecision2.";
  base::UmaHistogramEnumeration(base::StrCat({kNameBase, "Aggregate"}),
                                decision);

  int duplication_rank = GetDuplicationRank(
      AddressDataCleaner::CalculateMinimalIncompatibleProfileWithTypeSets(
          import_candidate, existing_profiles,
          AutofillProfileComparator(app_locale)));
  if (duplication_rank == 1) {
    base::UmaHistogramEnumeration(
        base::StrCat({kNameBase, "UserHasQuasiDuplicateProfile"}), decision);
  }
}

void LogNameEmailSupersetImportDecision(
    AutofillClient::AddressPromptUserDecision decision) {
  base::UmaHistogramEnumeration(
      "Autofill.ProfileImport.NameEmailSupersetProfileDecision", decision);
}

void LogHomeAndWorkSupersetImportDecision(
    AutofillClient::AddressPromptUserDecision decision) {
  base::UmaHistogramEnumeration(
      "Autofill.ProfileImport.HomeAndWorkSupersetProfileDecision", decision);
}

void LogHomeAndWorkSupersetAffectedType(FieldType affected_type) {
  base::UmaHistogramEnumeration(
      "Autofill.ProfileImport.HomeAndWorkSupersetAffectedType",
      ConvertSettingsVisibleFieldTypeForMetrics(affected_type));
}

void LogProfileImportTypeEditedType(AutofillProfileImportType import_type,
                                    FieldType edited_type) {
  base::UmaHistogramEnumeration(
      base::StrCat({"Autofill.ProfileImport.",
                    GetImportTypeEditedMetricsString(import_type),
                    "EditedType"}),
      ConvertSettingsVisibleFieldTypeForMetrics(edited_type));
}

// static
void LogRemovedSettingInaccessibleFields(bool did_remove) {
  base::UmaHistogramBoolean(
      "Autofill.ProfileImport.InaccessibleFieldsRemoved.Total", did_remove);
}

// static
void LogRemovedSettingInaccessibleField(FieldType field) {
  base::UmaHistogramEnumeration(
      "Autofill.ProfileImport.InaccessibleFieldsRemoved.ByFieldType",
      ConvertSettingsVisibleFieldTypeForMetrics(field));
}

// static
void LogPhoneNumberImportParsingResult(bool parsed_successfully) {
  base::UmaHistogramBoolean("Autofill.ProfileImport.PhoneNumberParsed",
                            parsed_successfully);
}

void LogProfileUpdateAffectedType(
    FieldType affected_type,
    AutofillClient::AddressPromptUserDecision decision) {
  // Record the decision-specific metric.
  base::UmaHistogramEnumeration(
      base::StrCat({"Autofill.ProfileImport.UpdateProfileAffectedType",
                    GetAddressPromptDecisionMetricsSuffix(decision)}),
      ConvertSettingsVisibleFieldTypeForMetrics(affected_type));

  // But also collect an histogram for any decision.
  base::UmaHistogramEnumeration(
      "Autofill.ProfileImport.UpdateProfileAffectedType.Any",
      ConvertSettingsVisibleFieldTypeForMetrics(affected_type));
}

void LogUpdateProfileNumberOfAffectedFields(
    int number_of_edited_fields,
    AutofillClient::AddressPromptUserDecision decision) {
  // Record the decision-specific metric.
  base::UmaHistogramExactLinear(
      base::StrCat(
          {"Autofill.ProfileImport.UpdateProfileNumberOfAffectedFields",
           GetAddressPromptDecisionMetricsSuffix(decision)}),
      number_of_edited_fields, /*exclusive_max=*/15);

  // But also collect an histogram for any decision.
  base::UmaHistogramExactLinear(
      "Autofill.ProfileImport.UpdateProfileNumberOfAffectedFields.Any",
      number_of_edited_fields, /*exclusive_max=*/15);
}

void LogProfileMigrationImportDecision(
    AutofillClient::AddressPromptUserDecision decision) {
  base::UmaHistogramEnumeration("Autofill.ProfileImport.MigrateProfileDecision",
                                decision);
}

void LogZipCodeLengthMetric(std::u16string_view zip) {
  base::UmaHistogramExactLinear(
      "Autofill.ProfileImportValidCandidate.ZipCode.Length", zip.size(), 20);
}

void LogZipCodeSeparatorMetric(std::u16string_view zip) {
  if (zip.empty()) {
    return;
  }

  for (base::i18n::UTF16CharIterator it(zip); !it.end(); it.Advance()) {
    if (!u_isalnum(it.get())) {
      base::UmaHistogramEnumeration(
          "Autofill.ProfileImportValidCandidate.ZipCode.Separator",
          GetAddressValidZipCodeSeparatorMetric(it.get()));
      return;
    }
  }

  base::UmaHistogramEnumeration(
      "Autofill.ProfileImportValidCandidate.ZipCode.Separator",
      AddressValidZipCodeSeparatorMetric::kNoSeparator);
}

}  // namespace autofill::autofill_metrics
