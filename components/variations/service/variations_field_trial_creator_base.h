// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_VARIATIONS_SERVICE_VARIATIONS_FIELD_TRIAL_CREATOR_BASE_H_
#define COMPONENTS_VARIATIONS_SERVICE_VARIATIONS_FIELD_TRIAL_CREATOR_BASE_H_

// This file contains a base class for VariationsFieldTrialCreator. Its primary
// goal is to minimize generated code size, even at the expense of
// functionality, and to be usable in ChromeOS early boot as part of a
// standalone executable.
// In particular, it will *not*:
// 1) determine locale (this requires linking in libicu, which is several
//    hundred KiB). Instead, it will accept locale as a parameter to the
//    constructor.
// 2) modify cached UI strings (we remove the ui::ResourceBundle dep entirely)
//
// All such functionality must be implemented by subclasses (e.g.
// VariationsFieldTrialCreator)

#include <cstdint>
#include <memory>
#include <string>
#include <unordered_map>
#include <vector>

#include "base/compiler_specific.h"
#include "base/containers/flat_set.h"
#include "base/memory/raw_ptr.h"
#include "base/metrics/field_trial.h"
#include "base/time/time.h"
#include "base/version_info/channel.h"
#include "build/build_config.h"
#include "components/variations/client_filterable_state.h"
#include "components/variations/entropy_provider.h"
#include "components/variations/metrics.h"
#include "components/variations/proto/study.pb.h"
#include "components/variations/seed_response.h"
#include "components/variations/service/buildflags.h"
#include "components/variations/service/limited_entropy_synthetic_trial.h"
#include "components/variations/service/safe_seed_manager.h"
#include "components/variations/service/variations_service_client.h"
#include "components/variations/variations_seed_store.h"
#include "components/version_info/channel.h"

namespace metrics {
class MetricsStateManager;
}

namespace variations {

class SyntheticTrialRegistry;

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
class SafeSeedManagerBase;
class VariationsServiceClient;

// Used to set up field trials based on stored variations seed data.
class VariationsFieldTrialCreatorBase {
 public:
  // Caller is responsible for ensuring that the VariationsServiceClient passed
  // to the constructor stays valid for the lifetime of this object.
  // |locale_cb| computes the locale, given a PrefService for local_state.
  //
  // The client will be registered to the limited entropy synthetic trial iff
  // |limited_entropy_synthetic_trial| is not null. Caller is responsible for
  // ensuring |limited_entropy_synthetic_trial| stays valid for the lifetime of
  // this object.
  VariationsFieldTrialCreatorBase(
      VariationsServiceClient* client,
      std::unique_ptr<VariationsSeedStore> seed_store,
      base::OnceCallback<std::string(PrefService*)> locale_cb,
      LimitedEntropySyntheticTrial* limited_entropy_synthetic_trial);

  VariationsFieldTrialCreatorBase(const VariationsFieldTrialCreatorBase&) =
      delete;
  VariationsFieldTrialCreatorBase& operator=(
      const VariationsFieldTrialCreatorBase&) = delete;

  virtual ~VariationsFieldTrialCreatorBase();

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
  // |synthetic_trial_registry| provides an interface to register synthetic
  // trials. Must not be null.
  // |platform_field_trials| provides the
  // platform-specific field trial setup for Chrome. Must not be null.
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
      SyntheticTrialRegistry* synthetic_trial_registry,
      PlatformFieldTrials* platform_field_trials,
      SafeSeedManagerBase* safe_seed_manager,
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

  // Calculates the Seed Freshness
  base::Time CalculateSeedFreshness();

  // Returns the locale that was used for evaluating trials.
  const std::string& application_locale() const { return application_locale_; }

  SeedType seed_type() const { return seed_type_; }

  // Overrides cached UI strings on the resource bundle once it is initialized.
  // To be implemented by subclasses, if they have need for UI strings.
  virtual void OverrideCachedUIStrings() = 0;

  // Returns whether the map of the cached UI strings to override is empty.
  // To be implemented by subclasses, if they have need for UI strings.
  virtual bool IsOverrideResourceMapEmpty() = 0;

  // Whether the limited entropy randomization source is enabled on this client.
  // The `trial` param controls the output, which will be false if `trial` is
  // null or the trial is not enabled. `trial` can be a nullptr when the client
  // is not eligible for limited entropy randomization (e.g. Android WebView).
  static bool IsLimitedEntropyRandomizationSourceEnabled(
      version_info::Channel channel,
      LimitedEntropySyntheticTrial* trial);

 protected:
  // Get the platform we're running on, respecting OverrideVariationsPlatform().
  // Protected for testing.
  Study::Platform GetPlatform();

  // Get the client's current form factor. Protected for testing.
  Study::FormFactor GetCurrentFormFactor();

#if BUILDFLAG(FIELDTRIAL_TESTING_ENABLED)
  // Applies the field trial testing config defined in
  // testing/variations/fieldtrial_testing_config.json to the current session.
  // Protected and virtual for testing.
  virtual void ApplyFieldTrialTestingConfig(base::FeatureList* feature_list);
#endif  // BUILDFLAG(FIELDTRIAL_TESTING_ENABLED)

  // Read the google group memberships from local-state prefs.
  // Protected for testing.
  base::flat_set<uint64_t> GetGoogleGroupsFromPrefs();

  // Overrides the string resource specified by |hash| with |str| in the
  // resource bundle. Protected for testing.
  // To be implemented by subclasses if they need UI string overrides.
  virtual void OverrideUIString(uint32_t hash, const std::u16string& str) = 0;

 private:
  // Returns true if the loaded VariationsSeed has expired. An expired seed is
  // one that (a) was fetched over |kMaxSeedAgeDays| ago and (b) is older than
  // the binary build time.
  //
  // Also, records a couple VariationsSeed-related metrics.
  bool HasSeedExpired();

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
                            SafeSeedManagerBase* safe_seed_manager,
                            SyntheticTrialRegistry* synthetic_trial_registry);

  // Reads a seed's data and signature from the file at |json_seed_path| and
  // writes them to Local State. Exits Chrome if (A) the file's contents can't
  // be loaded or (B) if the contents do not contain |kVariationsCompressedSeed|
  // or |kVariationsSeedSignature|. Also forces Chrome to not run in variations
  // safe mode. Used for variations seed testing.
  void LoadSeedFromJsonFile(const base::FilePath& json_seed_path);

  // Returns whether the conditions to activate the limited entropy synthetic
  // trial are met.
  bool ShouldActivateLimitedEntropySyntheticTrial(const VariationsSeed& seed);

  // Registers the group assignment of the limited entropy synthetic trial if
  // the activation condition are met (as given by
  // ShouldActivateLimitedEntropySyntheticTrial()).
  void RegisterLimitedEntropySyntheticTrialIfNeeded(
      const VariationsSeed& seed,
      SyntheticTrialRegistry* synthetic_trial_registry);

  // Returns the seed store. Virtual for testing.
  virtual VariationsSeedStore* GetSeedStore();

  PrefService* local_state() { return seed_store_->local_state(); }
  const PrefService* local_state() const { return seed_store_->local_state(); }

  raw_ptr<VariationsServiceClient> client_;

  std::unique_ptr<VariationsSeedStore> seed_store_;

  // Seed type used for variations.
  SeedType seed_type_ = SeedType::kNullSeed;

  // Tracks whether |CreateTrialsFromSeed| has been called, to ensure that it is
  // called at most once.
  bool create_trials_from_seed_called_;

  // The application locale won't change after the startup, so we cache the
  // value the first time when GetApplicationLocale() is called in the
  // constructor.
  std::string application_locale_;

  // Indicate if OverrideVariationsPlatform has been used to set
  // |platform_override_|.
  bool has_platform_override_;

  // Platform to be used for variations filtering, overriding the current
  // platform.
  Study::Platform platform_override_;

  // Caches the UI strings which need to be overridden in the resource bundle.
  // These strings are cached before the resource bundle is initialized.
  std::unordered_map<int, std::u16string> overridden_strings_map_;

  // Configurations related to the limited entropy synthetic trial.
  raw_ptr<LimitedEntropySyntheticTrial> limited_entropy_synthetic_trial_;

  SEQUENCE_CHECKER(sequence_checker_);
};

// A testing feature that forces a crash during field trial creation
// on developer and test builds.
BASE_DECLARE_FEATURE(kForceFieldTrialSetupCrashForTesting);

}  // namespace variations

#endif  // COMPONENTS_VARIATIONS_SERVICE_VARIATIONS_FIELD_TRIAL_CREATOR_BASE_H_
