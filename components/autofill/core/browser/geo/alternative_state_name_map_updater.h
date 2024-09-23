// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_AUTOFILL_CORE_BROWSER_GEO_ALTERNATIVE_STATE_NAME_MAP_UPDATER_H_
#define COMPONENTS_AUTOFILL_CORE_BROWSER_GEO_ALTERNATIVE_STATE_NAME_MAP_UPDATER_H_

#include <map>
#include <memory>
#include <set>
#include <string>
#include <utility>
#include <vector>

#include "base/functional/callback.h"
#include "base/functional/callback_helpers.h"
#include "base/memory/raw_ptr.h"
#include "base/memory/weak_ptr.h"
#include "base/scoped_observation.h"
#include "base/sequence_checker.h"
#include "base/task/sequenced_task_runner.h"
#include "components/autofill/core/browser/address_data_manager.h"
#include "components/autofill/core/browser/geo/alternative_state_name_map.h"
#include "components/autofill/core/browser/personal_data_manager_observer.h"

class PrefService;

namespace autofill {

using CountryToStateNamesListMapping =
    std::map<AlternativeStateNameMap::CountryCode,
             std::vector<AlternativeStateNameMap::StateName>>;

// The AlternativeStateNameMap is a singleton to map between canonical state
// names and alternative representations. This class acts as an observer to the
// AddressDataManager and encapsulates all aspects about loading state data from
// disk and adding it to the AlternativeStateNameMap.
class AlternativeStateNameMapUpdater : public AddressDataManager::Observer {
 public:
  AlternativeStateNameMapUpdater(PrefService* local_state,
                                 AddressDataManager* address_data_manager);
  ~AlternativeStateNameMapUpdater() override;
  AlternativeStateNameMapUpdater(const AlternativeStateNameMapUpdater&) =
      delete;
  AlternativeStateNameMapUpdater& operator=(
      const AlternativeStateNameMapUpdater&) = delete;

  // AddressDataManager::Observer:
  void OnAddressDataChanged() override;

  // Extracts the country and state values from the profiles and adds them to
  // the AlternativeStateNameMap.
  void PopulateAlternativeStateNameMap(
      base::OnceClosure callback = base::DoNothing());

  // Getter method for |is_alternative_state_name_map_populated_|.
  bool is_alternative_state_name_map_populated() const {
    return is_alternative_state_name_map_populated_;
  }

#if defined(UNIT_TEST)
  // A wrapper around |LoadStatesData| used for testing purposes.
  void LoadStatesDataForTesting(
      CountryToStateNamesListMapping country_to_state_names_map,
      PrefService* pref_service,
      base::OnceClosure done_callback) {
    LoadStatesData(std::move(country_to_state_names_map), pref_service,
                   std::move(done_callback));
  }

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
          stripped_state_value_from_profile) {
    return ContainsState(stripped_alternative_state_names,
                         stripped_state_value_from_profile);
  }

  // Setter for |local_state_| used for testing purposes.
  void set_local_state_for_testing(PrefService* pref_service) {
    local_state_ = pref_service;
  }
#endif  // defined(UNIT_TEST)

 private:
  // Compares |stripped_state_value_from_profile| with the entries in
  // |stripped_state_alternative_names| and returns true if a match is found.
  static bool ContainsState(
      const std::vector<AlternativeStateNameMap::StateName>&
          stripped_alternative_state_names,
      const AlternativeStateNameMap::StateName&
          stripped_state_value_from_profile);

  // Creates and posts jobs to the |task_runner_| for reading the state data
  // files and populating AlternativeStateNameMap. Once all files are read and
  // the data is incorporated into AlternativeStateNameMap, |done_callback| is
  // fired. |country_to_state_names_map| specifies which state data of which
  // countries to load.
  // Each call to LoadStatesData triggers loading state data files, so requests
  // should be batched up.
  void LoadStatesData(CountryToStateNamesListMapping country_to_state_names_map,
                      PrefService* pref_service,
                      base::OnceClosure done_callback);

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

  // Lazily initializes and returns |task_runner_|.
  scoped_refptr<base::SequencedTaskRunner>& GetTaskRunner();

  // TaskRunner for reading files from disk.
  scoped_refptr<base::SequencedTaskRunner> task_runner_;

  // A pointer to an instance of AddressDataManager used to fetch the profiles
  // data and register this class as an obsever.
  const raw_ptr<AddressDataManager> address_data_manager_ = nullptr;

  // The browser local_state that stores the states data installation path.
  raw_ptr<PrefService> local_state_ = nullptr;

  // In case of concurrent requests to load states data, the callbacks are
  // queued in |pending_init_done_callbacks_| and triggered once the
  // |number_pending_init_tasks_| returns to 0.
  std::vector<base::OnceClosure> pending_init_done_callbacks_;
  int number_pending_init_tasks_ = 0;

  // False, if the AlternativeStateNameMap has not been populated yet.
  bool is_alternative_state_name_map_populated_ = false;

  // Keeps track of all the state values from the current profile that have been
  // parsed.
  std::set<std::pair<AlternativeStateNameMap::CountryCode,
                     AlternativeStateNameMap::StateName>>
      parsed_state_values_;

  SEQUENCE_CHECKER(sequence_checker_);

  base::ScopedObservation<AddressDataManager, AddressDataManager::Observer>
      adm_observer_{this};

  // base::WeakPtr ensures that the callback bound to the object is canceled
  // when that object is destroyed.
  base::WeakPtrFactory<AlternativeStateNameMapUpdater> weak_ptr_factory_{this};
};

}  // namespace autofill

#endif  // COMPONENTS_AUTOFILL_CORE_BROWSER_GEO_ALTERNATIVE_STATE_NAME_MAP_UPDATER_H_
