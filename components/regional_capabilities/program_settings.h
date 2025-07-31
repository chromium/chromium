// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_REGIONAL_CAPABILITIES_PROGRAM_SETTINGS_H_
#define COMPONENTS_REGIONAL_CAPABILITIES_PROGRAM_SETTINGS_H_

namespace country_codes {
class CountryId;
}

namespace regional_capabilities {

enum class Program {
  kTaiyaki,
  kWaffle,
  kDefault,
};

// Describes how search engines should be listed.
enum class SearchEngineListType {
  // The top 5 (at most) engines of the current country's list should be used,
  // in the order specified by the regional settings.
  kTopFive,
  // The list of search engines should be fully shuffled.
  kShuffled,
};

// Describes how features should adjust themselves based on the program.
struct ProgramSettings {
  Program program;
  SearchEngineListType search_engine_list_type;
  bool can_show_search_engine_choice_screen;
};

extern const ProgramSettings kWaffleSettings;
extern const ProgramSettings kTaiyakiSettings;
extern const ProgramSettings kDefaultSettings;

bool IsInProgramRegion(Program program,
                       country_codes::CountryId profile_country);

}  // namespace regional_capabilities

#endif  // COMPONENTS_REGIONAL_CAPABILITIES_PROGRAM_SETTINGS_H_
