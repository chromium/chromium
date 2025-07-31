// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/regional_capabilities/program_settings.h"

#include "base/notreached.h"
#include "components/country_codes/country_codes.h"
#include "components/regional_capabilities/eea_countries_ids.h"

namespace regional_capabilities {
namespace {

constexpr country_codes::CountryId kTaiyakiCountry("JP");

}

const ProgramSettings kWaffleSettings{
    .program = Program::kWaffle,
    .search_engine_list_type = SearchEngineListType::kShuffled,
    .can_show_search_engine_choice_screen = true,
};

const ProgramSettings kTaiyakiSettings{
    .program = Program::kTaiyaki,
    .search_engine_list_type = SearchEngineListType::kShuffled,
    .can_show_search_engine_choice_screen = true,
};

const ProgramSettings kDefaultSettings{
    .program = Program::kDefault,
    .search_engine_list_type = SearchEngineListType::kTopFive,
    .can_show_search_engine_choice_screen = false,
};

bool IsInProgramRegion(Program program, country_codes::CountryId country_id) {
  switch (program) {
    case Program::kTaiyaki:
      return country_id == kTaiyakiCountry;
    case Program::kWaffle:
      return kEeaChoiceCountriesIds.contains(country_id);
    case Program::kDefault:
      NOTREACHED();
  }
}

}  // namespace regional_capabilities
