// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_AUTOFILL_CORE_BROWSER_GEO_ALTERNATIVE_STATE_NAME_MAP_UPDATER_H_
#define COMPONENTS_AUTOFILL_CORE_BROWSER_GEO_ALTERNATIVE_STATE_NAME_MAP_UPDATER_H_

#include <memory>
#include <string>
#include <vector>

#include "base/callback.h"
#include "base/memory/weak_ptr.h"
#include "base/sequence_checker.h"
#include "components/autofill/core/browser/geo/alternative_state_name_map.h"

class PrefService;

namespace autofill {

using CountryToStateNamesListMapping =
    std::map<AlternativeStateNameMap::CountryCode,
             std::vector<AlternativeStateNameMap::StateName>>;

// The AlternativeStateNameMap is a singleton to map between canonical state
// names and alternative representations. This class encapsulates all aspects
// about loading state data from disk and adding it to the
// AlternativeStateNameMap.
class AlternativeStateNameMapUpdater {
 public:
  AlternativeStateNameMapUpdater();
  ~AlternativeStateNameMapUpdater();
  AlternativeStateNameMapUpdater(const AlternativeStateNameMapUpdater&) =
      delete;
  AlternativeStateNameMapUpdater& operator=(
      const AlternativeStateNameMapUpdater&) = delete;

  // Creates and posts jobs to the |task_runner_| for reading the state data
  // files and populating AlternativeStateNameMap. Once all files are read and
  // the data is incorporated into AlternativeStateNameMap, |done_callback| is
  // fired. |country_to_state_names_map| specifies which state data of which
  // countries to load.
  // Each call to LoadStatesData triggers loading state data files, so requests
  // should be batched up.
  void LoadStatesData(
      const CountryToStateNamesListMapping& country_to_state_names_map,
      PrefService* pref_service,
      base::OnceClosure done_callback);

#if defined(UNIT_TEST)
  // A wrapper around |ProcessLoadedStateFileContent| used for testing purposes.
  void ProcessLoadedStateFileContentForTesting(
      const std::vector<AlternativeStateNameMap::StateName>&
          state_values_from_profiles,
      const std::string& data,
      base::OnceClosure callback) {
    ++number_pending_init_tasks_;
    pending_init_done_callbacks_.push_back(std::move(callback));
    ProcessLoadedStateFileContent(state_values_from_profiles, data);
  }

  // A wrapper around |ContainsState| used for testing purposes.
  static bool ContainsStateForTesting(
      const std::vector<AlternativeStateNameMap::StateName>&
          stripped_alternative_state_names,
      const AlternativeStateNameMap::StateName&
          stripped_state_values_from_profile) {
    return ContainsState(stripped_alternative_state_names,
                         stripped_state_values_from_profile);
  }
#endif

 private:
  // Compares |stripped_state_value_from_profile| with the entries in
  // |stripped_state_alternative_names| and returns true if a match is found.
  static bool ContainsState(
      const std::vector<AlternativeStateNameMap::StateName>&
          stripped_alternative_state_names,
      const AlternativeStateNameMap::StateName&
          stripped_state_values_from_profile);

  // Each entry in |state_values_from_profiles| is compared with the states
  // |data| read from the files and then inserted into the
  // AlternativeStateNameMap.
  void ProcessLoadedStateFileContent(
      const std::vector<AlternativeStateNameMap::StateName>&
          state_values_from_profiles,
      const std::string& data);

  // Builds and returns a list of all the names of the state (including its
  // abbreviations) from the |state_entry| into |state_names|.
  std::vector<AlternativeStateNameMap::StateName> ExtractAllStateNames(
      const StateEntry& state_entry);

  // TaskRunner for reading files from disk.
  scoped_refptr<base::SequencedTaskRunner> task_runner_;

  // In case of concurrent requests to load states data, the callbacks are
  // queued in |pending_init_done_callbacks_| and triggered once the
  // |number_pending_init_tasks_| returns to 0.
  std::vector<base::OnceClosure> pending_init_done_callbacks_;
  int number_pending_init_tasks_ = 0;

  SEQUENCE_CHECKER(sequence_checker_);

  // base::WeakPtr ensures that the callback bound to the object is canceled
  // when that object is destroyed.
  base::WeakPtrFactory<AlternativeStateNameMapUpdater> weak_ptr_factory_{this};
};

}  // namespace autofill

#endif  // COMPONENTS_AUTOFILL_CORE_BROWSER_GEO_ALTERNATIVE_STATE_NAME_MAP_UPDATER_H_
