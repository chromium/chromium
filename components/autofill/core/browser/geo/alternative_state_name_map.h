// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_AUTOFILL_CORE_BROWSER_GEO_ALTERNATIVE_STATE_NAME_MAP_H_
#define COMPONENTS_AUTOFILL_CORE_BROWSER_GEO_ALTERNATIVE_STATE_NAME_MAP_H_

#include <optional>
#include <string>

#include "base/i18n/case_conversion.h"
#include "base/no_destructor.h"
#include "base/synchronization/lock.h"
#include "base/thread_annotations.h"
#include "base/types/strong_alias.h"
#include "components/autofill/core/browser/proto/states.pb.h"

namespace autofill {

// AlternativeStateNameMap encapsulates mappings from state names in the
// profiles to their localized and the abbreviated names.
//
// AlternativeStateNameMap is used for the filling of state fields, comparison
// of profiles, determining mergeability of the address profiles and required
// |ADDRESS_HOME_STATE| votes to be sent to the server.
//
// AlternativeStateNameMap can provide the following data for the states:
//  1. The state string stored in the address profile denoted by
//       state_string_from_profile in this class.
//  2. The canonical state name (StateEntry::canonical_name) which acts as the
//       unique identifier representing the state (unique within a country).
//  3. The abbreviations of the state (StateEntry::abbreviations).
//  4. The alternative names of the state (StateEntry::alternative_names).
//
// StateEntry holds the information about the abbreviations and the
// alternative names of the state which is determined after comparison with the
// state values saved in the address profiles if they match.
//
// The main map |localized_state_names_map_| maps the tuple
// (country_code, canonical state name) as the key to the corresponding
// StateEntry object (with the information about the abbreviations and the
// alternative names) as the value.
//
// The |localized_state_names_reverse_lookup_map_| takes in the
// country_code and StateEntry::name, StateEntry::abbreviations or
// ::alternative_names as the key and the canonical state name as the value.
//
// Example: Considering "California" as the state_string_from_profile and
//          the corresponding StateEntry object:
//             {
//                'canonical_name': 'California',
//                'abbreviations': ['CA'],
//                'alternate_names': ['The Golden State']
//             }
//
//          1. StateEntry::canonical_name (i.e "California" in this case) acts
//              as the canonical state name.
//          2. ("US", "California") acts as the key and the above StateEntry
//              object is added as the value in the
//              |localized_state_names_map_|.
//          3. Entries added to |localized_state_names_reverse_lookup_map_|
//              are:
//               a. ("US", "California") -> "California"
//               b. ("US", "CA") -> "California"
//               c. ("US", "TheGoldenState") -> "California"
//
// In case, the user has an unknown state in the profile, nothing is added to
// the AlternativeStateNameMap;
class AlternativeStateNameMap {
 public:
  // Represents ISO 3166-1 alpha-2 codes (always uppercase ASCII).
  using CountryCode = base::StrongAlias<class CountryCodeTag, std::string>;

  // Represents either a canonical state name, or an abbreviation, or an
  // alternative name or normalized state name from the profile.
  using StateName = base::StrongAlias<class StateNameTag, std::u16string>;

  // States can be represented as different strings (different spellings,
  // translations, abbreviations). All representations of a single state in a
  // single country are mapped to the same canonical name.
  using CanonicalStateName =
      base::StrongAlias<class CanonicalStateNameTag, std::u16string>;

  static AlternativeStateNameMap* GetInstance();

  // Removes |kCharsToStrip| from |text| and returns the normalized text.
  static StateName NormalizeStateName(const StateName& text);

  // Calls |GetCanonicalStateName()| member method of AlternativeStateNameMap
  // and returns the canonical state name corresponding to |country_code| and
  // |state_name| if present.
  static std::optional<AlternativeStateNameMap::CanonicalStateName>
  GetCanonicalStateName(const std::string& country_code,
                        const std::u16string& state_name);

  ~AlternativeStateNameMap() = delete;
  AlternativeStateNameMap(const AlternativeStateNameMap&) = delete;
  AlternativeStateNameMap& operator=(const AlternativeStateNameMap&) = delete;

  // Returns the canonical name (StateEntry::canonical_name) from the
  // |localized_state_names_map_| based on
  // (|country_code|, |state_name|).
  // |is_state_name_normalized| denotes whether the |state_name| has been
  // normalized or not.
  std::optional<CanonicalStateName> GetCanonicalStateName(
      const CountryCode& country_code,
      const StateName& state_name,
      bool is_state_name_normalized = false) const;

  // Returns the value present in |localized_state_names_map_| corresponding
  // to (|country_code|, |state_string_from_profile|). In case, the entry does
  // not exist in the map, std::nullopt is returned.
  std::optional<StateEntry> GetEntry(
      const CountryCode& country_code,
      const StateName& state_string_from_profile) const;

  // Adds ((|country_code|, state key), |state_entry|) to the
  // |localized_state_names_map_|, where state key corresponds to
  // |normalized_canonical_state_name|.
  // Also, each entry from |normalized_alternative_state_names| is added as a
  // tuple ((|country_code|, |entry|), |normalized_canonical_state_name|) to the
  // |localized_state_names_reverse_lookup_map_|.
  void AddEntry(
      const CountryCode& country_code,
      const StateName& normalized_state_value_from_profile,
      const StateEntry& state_entry,
      const std::vector<StateName>& normalized_alternative_state_names,
      const CanonicalStateName& normalized_canonical_state_name);

  // Returns true if the |localized_state_names_map_| is empty.
  bool IsLocalisedStateNamesMapEmpty() const;

#if defined(UNIT_TEST)
  // Clears the map for testing purposes.
  void ClearAlternativeStateNameMapForTesting() {
    ClearAlternativeStateNameMap();
  }
#endif

 private:
  AlternativeStateNameMap();

  // Clears the |localized_state_names_map_| and
  // |localized_state_names_reverse_lookup_map_|.
  // Used only for testing purposes.
  void ClearAlternativeStateNameMap();

  // A custom comparator for the
  // |localized_state_names_reverse_lookup_map_| that ignores the case
  // of the string on comparisons.
  struct CaseInsensitiveLessComparator {
    bool operator()(const std::pair<CountryCode, StateName>& lhs,
                    const std::pair<CountryCode, StateName>& rhs) const {
      // Compares the country codes that are always uppercase ASCII.
      if (lhs.first != rhs.first)
        return lhs.first.value() < rhs.first.value();

      return base::i18n::ToLower(lhs.second.value()) <
             base::i18n::ToLower(rhs.second.value());
    }
  };

  // Since the constructor is private, |base::NoDestructor| must be friend to be
  // allowed to construct the class.
  friend class base::NoDestructor<AlternativeStateNameMap>;

  // TODO(crbug.com/40261113): Remove lock.
  mutable base::Lock lock_;

  // A map that stores the alternative state names. The map is keyed
  // by the country_code and the canonical state name (or
  // normalized_state_value_from_profile in case no canonical state name is
  // known) while the value is the StateEntry object.
  std::map<std::pair<CountryCode, CanonicalStateName>, StateEntry>
      localized_state_names_map_ GUARDED_BY(lock_);

  // The map is keyed by the country_code and the abbreviation or
  // canonical name or the alternative name of the state.
  std::map<std::pair<CountryCode, StateName>,
           CanonicalStateName,
           CaseInsensitiveLessComparator>
      localized_state_names_reverse_lookup_map_ GUARDED_BY(lock_);
};

}  // namespace autofill

#endif  // COMPONENTS_AUTOFILL_CORE_BROWSER_GEO_ALTERNATIVE_STATE_NAME_MAP_H_
