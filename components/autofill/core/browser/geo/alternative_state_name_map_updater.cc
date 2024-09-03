// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/autofill/core/browser/geo/alternative_state_name_map_updater.h"

#include <memory>
#include <string>
#include <vector>

#include "base/containers/contains.h"
#include "base/files/file_path.h"
#include "base/files/file_util.h"
#include "base/functional/bind.h"
#include "base/functional/callback_helpers.h"
#include "base/logging.h"
#include "base/ranges/algorithm.h"
#include "base/strings/string_util.h"
#include "base/strings/utf_string_conversions.h"
#include "base/task/sequenced_task_runner.h"
#include "base/task/task_traits.h"
#include "base/task/thread_pool.h"
#include "components/autofill/core/browser/address_data_manager.h"
#include "components/autofill/core/browser/geo/country_data.h"
#include "components/autofill/core/common/autofill_features.h"
#include "components/autofill/core/common/autofill_l10n_util.h"
#include "components/autofill/core/common/autofill_prefs.h"
#include "components/prefs/pref_service.h"

namespace autofill {

namespace {

// Returns data read from the file specified in |file|.
std::string LoadDataFromFile(const base::FilePath& file) {
  DCHECK(!file.empty());

  std::string data;
  if (!base::PathExists(file)) {
    VLOG(1) << "File does not exist: " << file;
    return std::string();
  }

  if (!base::ReadFileToString(file, &data)) {
    VLOG(1) << "Failed reading from file: " << file;
    return std::string();
  }

  return data;
}

}  // namespace

AlternativeStateNameMapUpdater::AlternativeStateNameMapUpdater(
    PrefService* local_state,
    AddressDataManager* address_data_manager)
    : address_data_manager_(address_data_manager), local_state_(local_state) {
  adm_observer_.Observe(address_data_manager_);
}

AlternativeStateNameMapUpdater::~AlternativeStateNameMapUpdater() = default;

// static
bool AlternativeStateNameMapUpdater::ContainsState(
    const std::vector<AlternativeStateNameMap::StateName>&
        stripped_alternative_state_names,
    const AlternativeStateNameMap::StateName&
        stripped_state_value_from_profile) {
  l10n::CaseInsensitiveCompare compare;

  // Returns true if |str1| is same as |str2| in a case-insensitive comparison.
  return std::ranges::any_of(
      stripped_alternative_state_names,
      [&](const AlternativeStateNameMap::StateName& text) {
        return compare.StringsEqual(text.value(),
                                    stripped_state_value_from_profile.value());
      });
}

void AlternativeStateNameMapUpdater::OnAddressDataChanged() {
  PopulateAlternativeStateNameMap();
}

void AlternativeStateNameMapUpdater::PopulateAlternativeStateNameMap(
    base::OnceClosure callback) {
  DCHECK(address_data_manager_);
  std::vector<const AutofillProfile*> profiles =
      address_data_manager_->GetProfiles();

  CountryToStateNamesListMapping country_to_state_names_map;
  for (const AutofillProfile* profile : profiles) {
    const AlternativeStateNameMap::CountryCode country(base::UTF16ToUTF8(
        profile->GetInfo(AutofillType(HtmlFieldType::kCountryCode),
                         address_data_manager_->app_locale())));

    const AlternativeStateNameMap::StateName state_name(profile->GetInfo(
        ADDRESS_HOME_STATE, address_data_manager_->app_locale()));
    const AlternativeStateNameMap::StateName normalized_state =
        AlternativeStateNameMap::NormalizeStateName(state_name);

    if (country.value().empty() || normalized_state.value().empty())
      continue;

    if (parsed_state_values_.find({country, normalized_state}) !=
        parsed_state_values_.end()) {
      continue;
    }

    country_to_state_names_map[country].push_back(normalized_state);
    parsed_state_values_.insert({country, normalized_state});
  }

  LoadStatesData(std::move(country_to_state_names_map), local_state_,
                 std::move(callback));
}

void AlternativeStateNameMapUpdater::LoadStatesData(
    CountryToStateNamesListMapping country_to_state_names_map,
    PrefService* pref_service,
    base::OnceClosure done_callback) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

  // Would be null in the case of tests.
  if (!pref_service) {
    return;
  }

  // Get the states data installation path from |pref_service| which is set by
  // the component updater once it downloads the states data and should be safe
  // to use.
  const base::FilePath data_download_path =
      pref_service->GetFilePath(prefs::kAutofillStatesDataDir);

  const std::vector<std::string>& country_codes =
      CountryDataMap::GetInstance()->country_codes();

  // Remove all invalid country names.
  std::erase_if(country_to_state_names_map,
                [&country_codes](
                    const CountryToStateNamesListMapping::value_type& entry) {
                  return !base::Contains(country_codes, entry.first.value());
                });

  // If the installed directory path is empty, it means that the component is
  // not ready for use yet.
  if (data_download_path.empty() || country_to_state_names_map.empty()) {
    is_alternative_state_name_map_populated_ = true;
    std::move(done_callback).Run();
    return;
  }

  pending_init_done_callbacks_.push_back(std::move(done_callback));

  // The |country_to_state_names_map| maps country_code names to a vector of
  // state names that are associated with this corresponding country.
  for (const auto& [country_code, states] : country_to_state_names_map) {
    // This is a security check to ensure that we only attempt to read files
    // that match to known countries.
    if (!base::Contains(country_codes, country_code.value()))
      continue;

    ++number_pending_init_tasks_;

    // |country_code| is used as the filename.
    // Example -> File "DE" contains the geographical states data of Germany.
    GetTaskRunner()->PostTaskAndReplyWithResult(
        FROM_HERE,
        base::BindOnce(&LoadDataFromFile,
                       data_download_path.AppendASCII(country_code.value())),
        base::BindOnce(
            &AlternativeStateNameMapUpdater::ProcessLoadedStateFileContent,
            weak_ptr_factory_.GetWeakPtr(), states));
  }
}

void AlternativeStateNameMapUpdater::ProcessLoadedStateFileContent(
    const std::vector<AlternativeStateNameMap::StateName>&
        stripped_state_values_from_profiles,
    const std::string& data) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

  DCHECK_GT(number_pending_init_tasks_, 0);
  --number_pending_init_tasks_;

  StatesInCountry states_data;

  if (!data.empty() && states_data.ParseFromString(data)) {
    DCHECK(states_data.has_country_code());
    AlternativeStateNameMap::CountryCode country_code =
        AlternativeStateNameMap::CountryCode(states_data.country_code());

    // Boolean flags that denote in |match_found[i]| whether the match has been
    // found for |stripped_state_values_from_profiles[i]|.
    std::vector<bool> match_found(stripped_state_values_from_profiles.size(),
                                  false);

    AlternativeStateNameMap* alternative_state_name_map =
        AlternativeStateNameMap::GetInstance();

    // Iterates over the states data loaded from the file and builds a list of
    // the state names and its variations. For each value v in
    // |stripped_state_values_from_profiles|, v is compared with the values in
    // the above created states list (if a match is not found for v yet). If the
    // comparison results in a match, the corresponding entry is added to the
    // |AlternativeStateNameMap|.
    for (const auto& state_entry : states_data.states()) {
      // Build a list of all the names of the state (including its
      // abbreviations) in |state_names|.
      const std::vector<AlternativeStateNameMap::StateName> state_names =
          ExtractAllStateNames(state_entry);

      // Canonical name is always the first entry in the |state_names|.
      DCHECK(!state_names.empty());
      AlternativeStateNameMap::CanonicalStateName
          normalized_canonical_state_name(state_names[0].value());

      for (size_t i = 0; i < stripped_state_values_from_profiles.size(); i++) {
        if (match_found[i])
          continue;

        // If |stripped_state_values_from_profile[i]| is in the set of names of
        // the state under consideration, add it to the AlternativeStateNameMap.
        if (ContainsState(state_names,
                          stripped_state_values_from_profiles[i])) {
          alternative_state_name_map->AddEntry(
              country_code, stripped_state_values_from_profiles[i], state_entry,
              state_names, normalized_canonical_state_name);
          match_found[i] = true;
        }
      }
    }
  }

  // When all pending tasks are completed, trigger and clear the pending
  // callbacks.
  if (number_pending_init_tasks_ == 0) {
    is_alternative_state_name_map_populated_ = true;
    for (auto& callback : std::exchange(pending_init_done_callbacks_, {}))
      std::move(callback).Run();
  }
}

std::vector<AlternativeStateNameMap::StateName>
AlternativeStateNameMapUpdater::ExtractAllStateNames(
    const StateEntry& state_entry) {
  DCHECK(state_entry.has_canonical_name());

  std::vector<AlternativeStateNameMap::StateName> state_names;
  state_names.reserve(1u + state_entry.abbreviations_size() +
                      state_entry.alternative_names_size());

  state_names.emplace_back(AlternativeStateNameMap::NormalizeStateName(
      AlternativeStateNameMap::StateName(
          base::UTF8ToUTF16(state_entry.canonical_name()))));
  for (const auto& abbr : state_entry.abbreviations()) {
    state_names.emplace_back(AlternativeStateNameMap::NormalizeStateName(
        AlternativeStateNameMap::StateName(base::UTF8ToUTF16(abbr))));
  }
  for (const auto& alternative_name : state_entry.alternative_names()) {
    state_names.emplace_back(AlternativeStateNameMap::NormalizeStateName(
        AlternativeStateNameMap::StateName(
            base::UTF8ToUTF16(alternative_name))));
  }

  return state_names;
}

scoped_refptr<base::SequencedTaskRunner>&
AlternativeStateNameMapUpdater::GetTaskRunner() {
  if (!task_runner_) {
    task_runner_ = base::ThreadPool::CreateSequencedTaskRunner(
        {base::MayBlock(), base::TaskPriority::BEST_EFFORT});
  }
  return task_runner_;
}

}  // namespace autofill
