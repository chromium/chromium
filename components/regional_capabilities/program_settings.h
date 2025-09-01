// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_REGIONAL_CAPABILITIES_PROGRAM_SETTINGS_H_
#define COMPONENTS_REGIONAL_CAPABILITIES_PROGRAM_SETTINGS_H_

#include "base/memory/raw_span.h"
#include "components/country_codes/country_codes.h"

namespace regional_capabilities {

// These values are persisted to prefs. Entries should not be renumbered and
// numeric values should never be reused.
// GENERATED_JAVA_ENUM_PACKAGE: org.chromium.components.regional_capabilities
// GENERATED_JAVA_CLASS_NAME_OVERRIDE: RegionalProgram
enum class Program : int {
  kDefault = 1,
  kTaiyaki = 2,
  kWaffle = 3,
};

// Describes how search engines should be listed.
enum class SearchEngineListType {
  // The top N (`kTopSearchEnginesThreshold` at most) engines of the current
  // country's list should be used, in the order specified by the regional
  // settings.
  kTopN,
  // The list of search engines should be fully shuffled.
  kShuffled,
};

// Describes how features should adjust themselves based on the program.
struct ProgramSettings {
  Program program;
  base::raw_span<const country_codes::CountryId> associated_countries;
  SearchEngineListType search_engine_list_type;
  bool can_show_search_engine_choice_screen;
};

// Returns the integer representation of `program`.
int SerializeProgram(Program program);

// Returns whether `serialized_program` represents a known program.
bool IsValidSerializedProgram(int serialized_program);

bool IsInProgramRegion(Program program,
                       const country_codes::CountryId& profile_country);

bool IsClientCompatibleWithProgram(Program program);

const ProgramSettings& GetSettingsForProgram(Program program);

}  // namespace regional_capabilities

#endif  // COMPONENTS_REGIONAL_CAPABILITIES_PROGRAM_SETTINGS_H_
