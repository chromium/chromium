// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/regional_capabilities/program_settings.h"

#include "base/containers/contains.h"
#include "base/feature_list.h"
#include "base/notreached.h"
#include "components/country_codes/country_codes.h"
#include "components/regional_capabilities/eea_countries_ids.h"
#include "components/regional_capabilities/regional_capabilities_switches.h"
#include "ui/base/device_form_factor.h"

namespace regional_capabilities {
namespace {

constexpr country_codes::CountryId kTaiyakiCountry("JP");

constexpr ProgramSettings kWaffleSettings{
    .program = Program::kWaffle,
    .associated_countries = base::raw_span<const country_codes::CountryId>(
        kEeaChoiceCountriesIds.begin(),
        kEeaChoiceCountriesIds.end()),
    .search_engine_list_type = SearchEngineListType::kShuffled,
    .selection_from_settings_counts_as_choice_screen_choice = true,
    .choice_screen_eligibility_config =
        ChoiceScreenEligibilityConfig{
            .managed_users_can_be_eligible = true,
            .should_preserve_non_prepopulated_dse = true,
            .should_preserve_imported_choice = false,
            .should_preserve_non_google_dse = true,
            .restrict_to_associated_countries = false,
            .restrict_surfaces_to_fre_only = false,
            .highlight_current_default = false,
        },
};

constexpr ProgramSettings kWaffleWithLocationRestrictionSettings = []() {
  ProgramSettings ret = kWaffleSettings;
  ret.choice_screen_eligibility_config->restrict_to_associated_countries = true;
  return ret;
}();

constexpr ProgramSettings kTaiyakiSettings{
    .program = Program::kTaiyaki,
    .associated_countries =
        base::raw_span<const country_codes::CountryId>(&kTaiyakiCountry, 1u),
    .search_engine_list_type = SearchEngineListType::kShuffled,
    .selection_from_settings_counts_as_choice_screen_choice = false,
    .choice_screen_eligibility_config =
        ChoiceScreenEligibilityConfig{
            .managed_users_can_be_eligible = false,
            .should_preserve_non_prepopulated_dse = false,
            .should_preserve_imported_choice = true,
            .should_preserve_non_google_dse = false,
            .restrict_to_associated_countries = true,
            .restrict_surfaces_to_fre_only = false,
            .highlight_current_default = true,
        },
};

#if BUILDFLAG(IS_IOS) || BUILDFLAG(IS_ANDROID)
constexpr ProgramSettings kTaiyakiSettingsFreOnly = []() {
  ProgramSettings ret = kTaiyakiSettings;
  ret.choice_screen_eligibility_config->restrict_surfaces_to_fre_only = true;
  return ret;
}();
#endif  // BUILDFLAG(IS_IOS) || BUILDFLAG(IS_ANDROID)

constexpr ProgramSettings kDefaultSettings{
    .program = Program::kDefault,
    .associated_countries = base::raw_span<const country_codes::CountryId>(),
    .search_engine_list_type = SearchEngineListType::kTopN,
    .selection_from_settings_counts_as_choice_screen_choice = false,
    .choice_screen_eligibility_config = std::nullopt,
};

// "Fake" program used for baseline checks. Announces itself as Taiyaki, but
// actually behaves like Default. Used when the `switches::kTaiyaki` feature is
// disabled.
constexpr ProgramSettings kNoOpTaiyakiSettings = []() {
  ProgramSettings ret = kDefaultSettings;
  ret.program = kTaiyakiSettings.program;
  ret.associated_countries = kTaiyakiSettings.associated_countries;
  return ret;
}();

}  // namespace

int SerializeProgram(Program program) {
  return static_cast<int>(program);
}

bool IsValidSerializedProgram(int serialized_program) {
  switch (static_cast<Program>(serialized_program)) {
    case Program::kDefault:
    case Program::kTaiyaki:
    case Program::kWaffle:
      return true;
  }

  return false;
}

bool IsInProgramRegion(Program program,
                       const country_codes::CountryId& country_id) {
  return base::Contains(GetSettingsForProgram(program).associated_countries,
                        country_id);
}

bool IsClientCompatibleWithProgram(Program program) {
  switch (program) {
    case Program::kTaiyaki:
#if BUILDFLAG(IS_IOS) || BUILDFLAG(IS_ANDROID)
      switch (ui::GetDeviceFormFactor()) {
        case ui::DEVICE_FORM_FACTOR_PHONE:
        case ui::DEVICE_FORM_FACTOR_FOLDABLE:
          return true;
        case ui::DEVICE_FORM_FACTOR_DESKTOP:
        case ui::DEVICE_FORM_FACTOR_TABLET:
        case ui::DEVICE_FORM_FACTOR_TV:
        case ui::DEVICE_FORM_FACTOR_AUTOMOTIVE:
        case ui::DEVICE_FORM_FACTOR_XR:
          break;
      }
#endif
      return false;
    case Program::kWaffle:
    case Program::kDefault:
      return true;
  }
  NOTREACHED();
}

const ProgramSettings& GetSettingsForProgram(Program program) {
  switch (program) {
    case Program::kTaiyaki:
#if BUILDFLAG(IS_IOS) || BUILDFLAG(IS_ANDROID)
      if (!base::FeatureList::IsEnabled(switches::kTaiyaki)) {
        return kNoOpTaiyakiSettings;
      }

      switch (switches::kTaiyakiChoiceScreenSurface.Get()) {
        case switches::RegionalCapabilitiesChoiceScreenSurface::kInFreOnly:
          return kTaiyakiSettingsFreOnly;
        case switches::RegionalCapabilitiesChoiceScreenSurface::kAll:
          return kTaiyakiSettings;
      }
      NOTREACHED() << "Unknown choice screen surface: "
                   << static_cast<int>(
                          switches::kTaiyakiChoiceScreenSurface.Get());

#else
      return kNoOpTaiyakiSettings;
#endif
    case Program::kWaffle:
      if (base::FeatureList::IsEnabled(
              switches::kWaffleRestrictToAssociatedCountries)) {
        return kWaffleWithLocationRestrictionSettings;
      }
      return kWaffleSettings;
    case Program::kDefault:
      return kDefaultSettings;
  }
  NOTREACHED();
}

}  // namespace regional_capabilities
