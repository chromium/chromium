// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_VARIATIONS_SERVICE_VARIATIONS_FIELD_TRIAL_CREATOR_H_
#define COMPONENTS_VARIATIONS_SERVICE_VARIATIONS_FIELD_TRIAL_CREATOR_H_

#include <memory>
#include <set>
#include <string>
#include <unordered_map>
#include <vector>

#include "base/compiler_specific.h"
#include "base/macros.h"
#include "base/metrics/field_trial.h"
#include "components/variations/client_filterable_state.h"
#include "components/variations/proto/study.pb.h"
#include "components/variations/seed_response.h"
#include "components/variations/service/ui_string_overrider.h"
#include "components/variations/variations_seed_store.h"

namespace variations {

enum LoadPermanentConsistencyCountryResult {
  LOAD_COUNTRY_NO_PREF_NO_SEED = 0,
  LOAD_COUNTRY_NO_PREF_HAS_SEED,
  LOAD_COUNTRY_INVALID_PREF_NO_SEED,
  LOAD_COUNTRY_INVALID_PREF_HAS_SEED,
  LOAD_COUNTRY_HAS_PREF_NO_SEED_VERSION_EQ,
  LOAD_COUNTRY_HAS_PREF_NO_SEED_VERSION_NEQ,
  LOAD_COUNTRY_HAS_BOTH_VERSION_EQ_COUNTRY_EQ,
  LOAD_COUNTRY_HAS_BOTH_VERSION_EQ_COUNTRY_NEQ,
  LOAD_COUNTRY_HAS_BOTH_VERSION_NEQ_COUNTRY_EQ,
  LOAD_COUNTRY_HAS_BOTH_VERSION_NEQ_COUNTRY_NEQ,
  LOAD_COUNTRY_HAS_PERMANENT_OVERRIDDEN_COUNTRY,
  LOAD_COUNTRY_MAX,
};

class PlatformFieldTrials;
class SafeSeedManager;
class VariationsServiceClient;

// Used to setup field trials based on stored variations seed data.
class VariationsFieldTrialCreator {
 public:
  // Caller is responsible for ensuring that objects passed to the constructor
  // stay valid for the lifetime of this object.
  VariationsFieldTrialCreator(PrefService* local_state,
                              VariationsServiceClient* client,
                              std::unique_ptr<VariationsSeedStore> seed_store,
                              const UIStringOverrider& ui_string_overrider);
  virtual ~VariationsFieldTrialCreator();

  // Returns what variations will consider to be the latest country. Returns
  // empty if it is not available.
  std::string GetLatestCountry() const;

  VariationsSeedStore* seed_store() { return seed_store_.get(); }

  // Sets up field trials based on stored variations seed data. Returns whether
  // setup completed successfully.
  // |kEnableGpuBenchmarking|, |kEnableFeatures|, |kDisableFeatures| are
  // feature controlling flags not directly accesible from variations.
  // |unforcable_field_trials| contains the list of trials that can not be
  // overridden.
  // |variation_ids| allows for forcing ids selected in chrome://flags and/or
  // specified using the command-line flag.
  // |low_entropy_provider| allows for field trial randomization.
  // |feature_list| contains the list of all active features for this client.
  // |platform_field_trials| provides the platform specific field trial set up
  // for Chrome.
  // |safe_seed_manager| should be notified of the combined server and client
  // state that was activated to create the field trials (only when the return
  // value is true).
  // |extra_overrides| gives a list of feature overrides that should be applied
  // after the features explicitly disabled/enabled from the command line via
  // --disable-features and --enable-features, but before field trials.
  // Note: The ordering of the FeatureList method calls is such that the
  // explicit --disable-features and --enable-features from the command line
  // take precedence over the |extra_overrides|, which take precedence over the
  // field trials.
  bool SetupFieldTrials(
      const char* kEnableGpuBenchmarking,
      const char* kEnableFeatures,
      const char* kDisableFeatures,
      const std::set<std::string>& unforceable_field_trials,
      const std::vector<std::string>& variation_ids,
      const std::vector<base::FeatureList::FeatureOverrideInfo>&
          extra_overrides,
      std::unique_ptr<const base::FieldTrial::EntropyProvider>
          low_entropy_provider,
      std::unique_ptr<base::FeatureList> feature_list,
      PlatformFieldTrials* platform_field_trials,
      SafeSeedManager* safe_seed_manager);

  // Returns all of the client state used for filtering studies.
  // As a side-effect, may update the stored permanent consistency country.
  std::unique_ptr<ClientFilterableState> GetClientFilterableStateForVersion(
      const base::Version& version);

  // Loads the country code to use for filtering permanent consistency studies,
  // updating the stored country code if the stored value was for a different
  // Chrome version. The country used for permanent consistency studies is kept
  // consistent between Chrome upgrades in order to avoid annoying the user due
  // to experiment churn while traveling.
  std::string LoadPermanentConsistencyCountry(
      const base::Version& version,
      const std::string& latest_country);

  // Sets the stored permanent country pref for this client.
  void StorePermanentCountry(const base::Version& version,
                             const std::string& country);

  // Sets the stored permanent variations overridden country pref for this
  // client.
  void StoreVariationsOverriddenCountry(const std::string& country);

  // Records the time of the most recent successful fetch.
  void RecordLastFetchTime();

  // Allow the platform that is used to filter the set of active trials to be
  // overridden.
  void OverrideVariationsPlatform(Study::Platform platform_override);

  // Overrides cached UI strings on the resource bundle once it is initialized.
  void OverrideCachedUIStrings();

  // Returns whether the map of the cached UI strings to override is empty.
  bool IsOverrideResourceMapEmpty();

  // Returns the locale that was used for evaluating trials.
  const std::string& application_locale() const { return application_locale_; }

  // Returns the short hardware class value used to evaluate variations hardware
  // class filters. Only implemented on CrOS and Android - returns empty string
  // on other platforms.
  static std::string GetShortHardwareClass();

 private:
  // Loads the seed from the variations store into |seed|, and records metrics
  // about the loaded seed. Returns true on success, in which case |seed| will
  // contain the loaded data, and |seed_data| and |base64_signature| will
  // contain the raw pref values.
  bool LoadSeed(VariationsSeed* seed,
                std::string* seed_data,
                std::string* base64_signature) WARN_UNUSED_RESULT;

  // Loads the safe seed from the variations store into |seed| and updates any
  // relevant fields in |client_state|. If the load succeeds, records metrics
  // about the loaded seed. Returns whether the load succeeded.
  bool LoadSafeSeed(VariationsSeed* seed,
                    ClientFilterableState* client_state) WARN_UNUSED_RESULT;

  // Creates field trials based on the variations seed loaded from local state.
  // If there is a problem loading the seed data, all trials specified by the
  // seed may not be created. Some field trials are configured to override or
  // associate with (for reporting) specific features. These associations are
  // registered with |feature_list|. Returns true if trials were created
  // successfully; and if so, stores the loaded variations state into the
  // |safe_seed_manager|.
  bool CreateTrialsFromSeed(
      std::unique_ptr<const base::FieldTrial::EntropyProvider>
          low_entropy_provider,
      base::FeatureList* feature_list,
      SafeSeedManager* safe_seed_manager);

  // Overrides the string resource specified by |hash| with |str| in the
  // resource bundle.
  void OverrideUIString(uint32_t hash, const base::string16& str);

  // Returns the seed store. Virtual for testing.
  virtual VariationsSeedStore* GetSeedStore();

  // Get the platform we're running on, respecting OverrideVariationsPlatform().
  Study::Platform GetPlatform();

  PrefService* local_state() { return seed_store_->local_state(); }
  const PrefService* local_state() const { return seed_store_->local_state(); }

  VariationsServiceClient* client_;

  UIStringOverrider ui_string_overrider_;

  std::unique_ptr<VariationsSeedStore> seed_store_;

  // Tracks whether |CreateTrialsFromSeed| has been called, to ensure that it is
  // called at most once.
  bool create_trials_from_seed_called_;

  // The application locale won't change after the startup, so we cache the
  // value the first time when GetApplicationLocale() is called in the
  // constructor.
  std::string application_locale_;

  // Indiciate if OverrideVariationsPlatform has been used to set
  // |platform_override_|.
  bool has_platform_override_;

  // Platform to be used for variations filtering, overridding the current
  // platform.
  Study::Platform platform_override_;

  // Caches the UI strings which need to be overridden in the resource bundle.
  // These strings are cached before the resource bundle is initialized.
  std::unordered_map<int, base::string16> overridden_strings_map_;

  SEQUENCE_CHECKER(sequence_checker_);

  DISALLOW_COPY_AND_ASSIGN(VariationsFieldTrialCreator);
};

}  // namespace variations

#endif  // COMPONENTS_VARIATIONS_SERVICE_VARIATIONS_FIELD_TRIAL_CREATOR_H_
