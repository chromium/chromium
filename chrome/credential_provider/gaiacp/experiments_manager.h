// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_CREDENTIAL_PROVIDER_GAIACP_EXPERIMENTS_MANAGER_H_
#define CHROME_CREDENTIAL_PROVIDER_GAIACP_EXPERIMENTS_MANAGER_H_

#include <map>
#include <string>
#include <vector>

namespace credential_provider {

// List of experiments which must be kept in sync with the metadata for the
// experiment below.
enum Experiment { TEST_CLIENT_FLAG, TEST_CLIENT_FLAG2 };

class ExperimentsManager {
 public:
  // Gets the singleton instance.
  static ExperimentsManager* Get();

  // Called as the first thing when ExperimentsManager is getting constructed.
  // Registers all the experiments used in GCPW and ESA with their default
  // values. If fetching experiments happen to fail, defaults set by this
  // function are used.
  void RegisterExperiments();

  // Reloads the experiments for the given |sid|.
  bool ReloadExperiments(const std::wstring& sid);

  // Returns the experiment value for the provided |sid| and |experiment|.
  std::string GetExperimentForUser(const std::string& sid,
                                   Experiment experiment);

  // Returns the experiment value as a boolean for the provided |sid| and
  // |experiment|.
  bool GetExperimentForUserAsBool(const std::string& sid,
                                  Experiment experiment);

  // Return true if experiments feature is enabled.
  bool ExperimentsEnabled() const;

  // Returns the list of experiments to fetch from the backend.
  std::vector<std::string> GetExperimentsList() const;

 private:
  // Returns the storage used for the instance pointer.
  static ExperimentsManager** GetInstanceStorage();

  ExperimentsManager();
  virtual ~ExperimentsManager();

  // Updates the cached values of experiments with the recent fetch.
  void ReloadAllExperiments();

  // sid to experiment value mapping.
  typedef std::map<std::string, std::string> PerUserValues;

  // default value and per user experiment value pair.
  typedef std::pair<std::string, PerUserValues> ExperimentValue;

  // Map of features to <default value, per user value mapping>
  std::map<std::string, ExperimentValue> experiments_to_values_;

  // Struct which contains an individual experiment name and default value.
  struct ExperimentMetadata {
    Experiment experiment;
    std::string name;
    std::string default_value;
  };

  // Metadata for list of supported experiments.
  std::vector<ExperimentMetadata> experiments = {
      // TODO(crbug.com/40156649): Clean up the test experiments when actual
      // experiments are introduced. These were added for e2e testing.
      {TEST_CLIENT_FLAG, "test_client_flag", "false"},
      {TEST_CLIENT_FLAG2, "test_client_flag2", "false"}};
};

}  // namespace credential_provider

#endif  // CHROME_CREDENTIAL_PROVIDER_GAIACP_EXPERIMENTS_MANAGER_H_
