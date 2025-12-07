// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_REGIONAL_CAPABILITIES_PROGRAM_SETTINGS_H_
#define COMPONENTS_REGIONAL_CAPABILITIES_PROGRAM_SETTINGS_H_

#include "base/containers/enum_set.h"
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

  kMin = kDefault,
  kMax = kWaffle,
};

using ProgramSet = base::EnumSet<Program, Program::kMin, Program::kMax>;

// Describes how search engines should be listed.
enum class SearchEngineListType {
  // The top N (`kTopSearchEnginesThreshold` at most) engines of the current
  // country's list should be used, in the order specified by the regional
  // settings.
  kTopN,
  // The list of search engines should be fully shuffled.
  kShuffled,
};

// Describes how the program affects the search engine choice screen eligibility
// logic.
//
// Note: The order of the fields is important, and reflects the priority order
// in which eligibility checks are performed and their relative precedence.
struct ChoiceScreenEligibilityConfig {
  // Whether managed/supervised users can be eligible for the choice screen.
  bool managed_users_can_be_eligible;
  // Relates to default search engine selections associated with a non-builtin
  // search engine service, likely entered manually be the user.
  bool should_preserve_non_prepopulated_dse;
  // Relates to to the choices that we identified as having been made on another
  // device and imported through Backup & Restore.
  bool should_preserve_imported_choice;
  // Relates to default search engine selections associated with a non-Google
  // service.
  bool should_preserve_non_google_dse;
  // When `true`, the choice screen is only shown if the current country as
  // indicated by variations country is one of the associated countries. When
  // restriction is requested, the choice screen won't be presented if the
  // current country is not available (unknown country is not considered an
  // associated country).
  bool restrict_to_associated_countries;
  // If true, the choice screen is only eligible to be shown in FRE.
  bool restrict_surfaces_to_fre_only;

  // If true, the current default will be highlighted on the choice screen.
  //
  // Note: Not technically an eligibility-related attribute, but doesn't
  // make sense to be specified for non-choice screen programs. It
  // can be moved out in the future if we need more choice screen display config
  // attributes later.
  bool highlight_current_default;
};

// Describes how features should adjust themselves based on the program.
struct ProgramSettings {
  // Identifier for the program represented by this settings instance.
  Program program;

  // Countries associated with this program. May be used to check that the
  // profile country and sometimes the current variations country are matching
  // with the active program.
  base::raw_span<const country_codes::CountryId> associated_countries;

  // Indicates how list of search engines to be loaded in this profile should be
  // put together.
  SearchEngineListType search_engine_list_type;

  // Indicates whether user search engine selections made from settings (as the
  // current only non strictly choice screen choice surface) can count the same
  // way a choice from a choice screen with the same provided options would.
  bool selection_from_settings_counts_as_choice_screen_choice;

  // Indicates how the choice screen eligibility is affected by the active
  // program.
  // When `std::nullopt`, it means the program does not involve choice screens.
  std::optional<ChoiceScreenEligibilityConfig> choice_screen_eligibility_config;
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
