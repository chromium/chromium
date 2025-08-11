// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/regional_capabilities/program_settings.h"

#include "base/containers/contains.h"
#include "base/notreached.h"
#include "components/country_codes/country_codes.h"
#include "components/regional_capabilities/eea_countries_ids.h"
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
    .can_show_search_engine_choice_screen = true,
};

constexpr ProgramSettings kTaiyakiSettings{
    .program = Program::kTaiyaki,
    .associated_countries =
        base::raw_span<const country_codes::CountryId>(&kTaiyakiCountry, 1u),
    .search_engine_list_type = SearchEngineListType::kShuffled,
    .can_show_search_engine_choice_screen = true,
};

constexpr ProgramSettings kDefaultSettings{
    .program = Program::kDefault,
    .associated_countries = base::raw_span<const country_codes::CountryId>(),
    .search_engine_list_type = SearchEngineListType::kTopFive,
    .can_show_search_engine_choice_screen = false,
};

}  // namespace

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
}

const ProgramSettings& GetSettingsForProgram(Program program) {
  switch (program) {
    case Program::kTaiyaki:
      return kTaiyakiSettings;
    case Program::kWaffle:
      return kWaffleSettings;
    case Program::kDefault:
      return kDefaultSettings;
  }
}

}  // namespace regional_capabilities
