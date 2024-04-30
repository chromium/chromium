// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/credential_provider/gaiacp/experiments_manager.h"

#include <string_view>
#include <vector>

#include "base/files/file.h"
#include "base/json/json_reader.h"
#include "base/strings/string_util.h"
#include "base/strings/utf_string_conversions.h"
#include "chrome/credential_provider/gaiacp/gcp_utils.h"
#include "chrome/credential_provider/gaiacp/logging.h"
#include "chrome/credential_provider/gaiacp/reg_utils.h"

namespace credential_provider {
namespace {

// Registry key to control whether experiments feature is enabled.
const wchar_t kExperimentsEnabledRegKey[] = L"experiments_enabled";

// Name of the keys found in the experiments server response.
const char kResponseExperimentsKeyName[] = "experiments";
const char kResponseFeatureKeyName[] = "feature";
const char kResponseValueKeyName[] = "value";

}  // namespace

// static
ExperimentsManager* ExperimentsManager::Get() {
  return *GetInstanceStorage();
}

// static
ExperimentsManager** ExperimentsManager::GetInstanceStorage() {
  static ExperimentsManager instance;
  static ExperimentsManager* instance_storage = &instance;
  return &instance_storage;
}

ExperimentsManager::ExperimentsManager() {
  if (ExperimentsEnabled()) {
    RegisterExperiments();
    ReloadAllExperiments();
  }
}

ExperimentsManager::~ExperimentsManager() = default;

void ExperimentsManager::RegisterExperiments() {
  experiments_to_values_.clear();
  for (ExperimentMetadata em : experiments) {
    // Experiment name to default value mapping is created prior to reading
    // experiments file.
    experiments_to_values_[em.name].first = em.default_value;
  }
}

// Reloads the experiment values from fetched experiments file for the given
// |sid|. Loads the fetched experiments values into |experiments_to_values_|.
bool ExperimentsManager::ReloadExperiments(const std::wstring& sid) {
  uint32_t open_flags = base::File::FLAG_OPEN | base::File::FLAG_READ;
  std::unique_ptr<base::File> experiments_file = GetOpenedFileForUser(
      sid, open_flags, kGcpwExperimentsDirectory, kGcpwUserExperimentsFileName);
  if (!experiments_file) {
    return false;
  }

  std::vector<char> buffer(experiments_file->GetLength());
  experiments_file->Read(0, buffer.data(), buffer.size());
  experiments_file.reset();

  std::optional<base::Value> experiments_data =
      base::JSONReader::Read(std::string_view(buffer.data(), buffer.size()),
                             base::JSON_ALLOW_TRAILING_COMMAS);
  if (!experiments_data || !experiments_data->is_dict()) {
    LOGFN(ERROR) << "Failed to read experiments data from file!";
    return false;
  }

  const base::Value::List* experiments_value =
      experiments_data->GetDict().FindList(kResponseExperimentsKeyName);
  if (!experiments_value) {
    LOGFN(ERROR) << "User experiments not found!";
    return false;
  }

  for (const auto& item : *experiments_value) {
    const auto& item_dict = item.GetDict();
    auto* f = item_dict.FindString(kResponseFeatureKeyName);
    auto* v = item_dict.FindString(kResponseValueKeyName);
    if (!f || !v) {
      LOGFN(WARNING) << "Either feature or value are not found!";
    }

    experiments_to_values_[*f].second[base::WideToUTF8(sid)] = *v;
  }
  return true;
}

// TODO(crbug.com/40155245): Reload experiments if they were fetched by ESA.
void ExperimentsManager::ReloadAllExperiments() {
  std::map<std::wstring, UserTokenHandleInfo> sid_to_gaia_id;
  HRESULT hr = GetUserTokenHandles(&sid_to_gaia_id);

  if (FAILED(hr)) {
    LOGFN(ERROR) << "Unable to get the list of associated users";
    return;
  }

  for (auto& sid : sid_to_gaia_id) {
    if (!ReloadExperiments(sid.first)) {
      LOGFN(ERROR) << "Error when loading experiments for user with sid "
                   << sid.first;
    }
  }
}

std::string ExperimentsManager::GetExperimentForUser(const std::string& sid,
                                                     Experiment experiment) {
  std::string experiment_name = experiments[experiment].name;

  // There shouldn't be a case where a registered experiment can't be found, but
  // just in case we return an empty string.
  if (experiments_to_values_.find(experiment_name) ==
      experiments_to_values_.end())
    return "";

  auto& sid_to_experiment_values =
      experiments_to_values_[experiment_name].second;

  // If the experiment value doesn't exist for the given sid, return the default
  // value.
  if (sid_to_experiment_values.find(sid) == sid_to_experiment_values.end()) {
    return experiments_to_values_[experiment_name].first;
  }

  return sid_to_experiment_values[sid];
}

bool ExperimentsManager::GetExperimentForUserAsBool(const std::string& sid,
                                                    Experiment experiment) {
  return base::ToLowerASCII(GetExperimentForUser(sid, experiment)) == "true";
}

bool ExperimentsManager::ExperimentsEnabled() const {
  return GetGlobalFlagOrDefault(kExperimentsEnabledRegKey, 1);
}

std::vector<std::string> ExperimentsManager::GetExperimentsList() const {
  std::vector<std::string> experiment_names;
  for (auto& experiment : experiments)
    experiment_names.push_back(experiment.name);
  return experiment_names;
}

}  // namespace credential_provider
