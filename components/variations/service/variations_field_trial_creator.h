// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_VARIATIONS_SERVICE_VARIATIONS_FIELD_TRIAL_CREATOR_H_
#define COMPONENTS_VARIATIONS_SERVICE_VARIATIONS_FIELD_TRIAL_CREATOR_H_

#include <memory>
#include <string>
#include <unordered_map>
#include <vector>

#include "base/compiler_specific.h"
#include "base/memory/raw_ptr.h"
#include "base/metrics/field_trial.h"
#include "base/time/time.h"
#include "build/build_config.h"
#include "components/variations/client_filterable_state.h"
#include "components/variations/entropy_provider.h"
#include "components/variations/metrics.h"
#include "components/variations/proto/study.pb.h"
#include "components/variations/seed_response.h"
#include "components/variations/service/buildflags.h"
#include "components/variations/service/ui_string_overrider.h"
#include "components/variations/variations_seed_store.h"
#include "components/version_info/channel.h"

namespace metrics {
class MetricsStateManager;
}

namespace variations {

// Just maps one set of enum values to another. Nothing to see here.
Study::Channel ConvertProductChannelToStudyChannel(
    version_info::Channel product_channel);

// Denotes whether Chrome used a variations seed. Also captures (a) the kind of
// seed and (b) the conditions under which the seed was used or failed to be
// used. Exposed for testing.
//
// These values are persisted to logs. Entries should not be renumbered and
// numeric values should never be reused.
enum class SeedUsage {
  kRegularSeedUsed = 0,
  kExpiredRegularSeedNotUsed = 1,
  kUnloadableRegularSeedNotUsed = 2,
  kSafeSeedUsed = 3,
  kExpiredSafeSeedNotUsed = 4,
  // The below three enumerators were deprecated in M100.
  // kCorruptedSafeSeedNotUsed = 5,
  // kRegularSeedUsedAfterEmptySafeSeedLoaded = 6,
  // kExpiredRegularSeedNotUsedAfterEmptySafeSeedLoaded = 7,
  // kCorruptedRegularSeedNotUsedAfterEmptySafeSeedLoaded = 8,
  kRegularSeedForFutureMilestoneNotUsed = 9,
  kSafeSeedForFutureMilestoneNotUsed = 10,
  kUnloadableSafeSeedNotUsed = 11,
  kNullSeedUsed = 12,
  kMaxValue = kNullSeedUsed,
};

// Denotes a variations seed's expiry state. Exposed for testing.
//
// These values are persisted to logs. Entries should not be renumbered and
// numeric values should never be reused.
enum class VariationsSeedExpiry {
  kNotExpired = 0,
  kFetchTimeMissing = 1,
  kExpired = 2,
  kMaxValue = kExpired,
};

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

// Used to set up field trials based on stored variations seed data.
class VariationsFieldTrialCreator {
 public:
  // Caller is responsible for ensuring that objects passed to the constructor
  // stay valid for the lifetime of this object.
  VariationsFieldTrialCreator(VariationsServiceClient* client,
                              std::unique_ptr<VariationsSeedStore> seed_store,
                              const UIStringOverrider& ui_string_overrider);

  VariationsFieldTrialCreator(const VariationsFieldTrialCreator&) = delete;
  VariationsFieldTrialCreator& operator=(const VariationsFieldTrialCreator&) =
      delete;

  virtual ~VariationsFieldTrialCreator();

  // Returns what variations will consider to be the latest country. Returns
  // empty if it is not available.
  std::string GetLatestCountry() const;

  VariationsSeedStore* seed_store() { return seed_store_.get(); }

  // Sets up field trials based on stored variations seed data. Returns whether
  // setup completed successfully.
  //
  // |variation_ids| allows for forcing ids selected in chrome://flags.
  // |command_line_variation_ids| allows for forcing ids through the
  // "--force-variation-ids" command line flag. It should be a comma-separated
  // list of variation ids. Ids prefixed with the character "t" will be treated
  // as Trigger Variation Ids.
  // |extra_overrides| gives a list of feature overrides that should be applied
  // after the features explicitly disabled/enabled from the command line via
  // --disable-features and --enable-features, but before field trials.
  // |feature_list| contains the list of all active features for this client.
  // Must not be null.
  // |metrics_state_manager| facilitates signaling that Chrome has not yet
  // exited cleanly. Must not be null.
  // |platform_field_trials| provides the platform-specific field trial setup
  // for Chrome. Must not be null.
  // |safe_seed_manager| should be notified of the combined server and client
  // state that was activated to create the field trials (only when the return
  // value is true). Must not be null.
  // |add_entropy_source_to_variations_ids| controls if variations ID for the
  // low entropy source should be added to FIRST_PARTY variation headers.
  // TODO(b/263797385): eliminate this argument if we can always add the ID.
  //
  // NOTE: The ordering of the FeatureList method calls is such that the
  // explicit --disable-features and --enable-features from the command line
  // take precedence over |extra_overrides|, which takes precedence over the
  // field trials.
  bool SetUpFieldTrials(
      const std::vector<std::string>& variation_ids,
      const std::string& command_line_variation_ids,
      const std::vector<base::FeatureList::FeatureOverrideInfo>&
          extra_overrides,
      std::unique_ptr<base::FeatureList> feature_list,
      metrics::MetricsStateManager* metrics_state_manager,
      PlatformFieldTrials* platform_field_trials,
      SafeSeedManager* safe_seed_manager,
      bool add_entropy_source_to_variations_ids);

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

  // Allow the platform that is used to filter the set of active trials to be
  // overridden.
  void OverrideVariationsPlatform(Study::Platform platform_override);

  // Overrides cached UI strings on the resource bundle once it is initialized.
  void OverrideCachedUIStrings();

  // Returns whether the map of the cached UI strings to override is empty.
  bool IsOverrideResourceMapEmpty();

  // Returns the locale that was used for evaluating trials.
  const std::string& application_locale() const { return application_locale_; }

 protected:
  // Get the platform we're running on, respecting OverrideVariationsPlatform().
  // Protected for testing.
  Study::Platform GetPlatform();

  // Overrides the string resource specified by |hash| with |str| in the
  // resource bundle. Protected for testing.
  void OverrideUIString(uint32_t hash, const std::u16string& str);

  // Get the client's current form factor. Protected for testing.
  Study::FormFactor GetCurrentFormFactor();

#if BUILDFLAG(FIELDTRIAL_TESTING_ENABLED)
  // Applies the field trial testing config defined in
  // testing/variations/fieldtrial_testing_config.json to the current session.
  // Protected and virtual for testing.
  virtual void ApplyFieldTrialTestingConfig(base::FeatureList* feature_list);
#endif  // BUILDFLAG(FIELDTRIAL_TESTING_ENABLED)

 private:
  // Returns true if the loaded VariationsSeed has expired. An expired seed is
  // one that (a) was fetched over |kMaxVariationsSeedAgeDays| ago and (b) is
  // older than the binary build time.
  //
  // Also, records a couple VariationsSeed-related metrics.
  bool HasSeedExpired(bool is_safe_seed);

  // Returns true if the loaded VariationsSeed is for a future milestone (e.g.
  // if the client is on M92 and the seed was fetched with M93). A seed for a
  // future milestone is invalid as it may be missing studies filtered out by
  // the server.
  bool IsSeedForFutureMilestone(bool is_safe_seed);

  // Creates field trials based on the variations seed loaded from local state.
  // If there is a problem loading the seed data, all trials specified by the
  // seed may not be created. Some field trials are configured to override or
  // associate with (for reporting) specific features. These associations are
  // registered with |feature_list|. Returns true if trials were created
  // successfully; and if so, stores the loaded variations state into the
  // |safe_seed_manager|.
  bool CreateTrialsFromSeed(const EntropyProviders& entropy_providers,
                            base::FeatureList* feature_list,
                            SafeSeedManager* safe_seed_manager);

  // Reads a seed's data and signature from the file at |seed_path| and writes
  // them to Local State. Exits Chrome (A) if the file's contents can't be
  // loaded or (B) if the contents do not contain |kVariationsCompressedSeed| or
  // |kVariationsSeedSignature|. Also forces Chrome to not run in variations
  // safe mode. Used for variations seed testing.
  void LoadSeedFromFile(const base::FilePath& seed_path);

  // Returns the seed store. Virtual for testing.
  virtual VariationsSeedStore* GetSeedStore();

  PrefService* local_state() { return seed_store_->local_state(); }
  const PrefService* local_state() const { return seed_store_->local_state(); }

  raw_ptr<VariationsServiceClient> client_;

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
  std::unordered_map<int, std::u16string> overridden_strings_map_;

  SEQUENCE_CHECKER(sequence_checker_);
};

// A testing feature that forces a crash during field trial creation
// on developer and test builds.
BASE_DECLARE_FEATURE(kForceFieldTrialSetupCrashForTesting);

}  // namespace variations

#endif  // COMPONENTS_VARIATIONS_SERVICE_VARIATIONS_FIELD_TRIAL_CREATOR_H_
