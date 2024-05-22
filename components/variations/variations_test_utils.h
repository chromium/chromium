// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_VARIATIONS_VARIATIONS_TEST_UTILS_H_
#define COMPONENTS_VARIATIONS_VARIATIONS_TEST_UTILS_H_

#include <set>
#include <string>

#include "base/containers/span.h"
#include "base/memory/scoped_refptr.h"
#include "base/metrics/field_trial.h"
#include "base/test/mock_entropy_provider.h"
#include "components/variations/active_field_trials.h"
#include "components/variations/client_filterable_state.h"
#include "components/variations/entropy_provider.h"
#include "components/variations/field_trial_config/fieldtrial_testing_config.h"
#include "components/variations/proto/variations_seed.pb.h"
#include "components/variations/synthetic_trial_registry.h"
#include "components/variations/variations_associated_data.h"

class PrefService;

namespace variations {

struct ClientFilterableState;

// Packages signed variations seed data into a tuple for use with
// WriteSeedData(). This allows for encapsulated seed information to be created
// below for generic test seeds as well as seeds which cause crashes.
struct SignedSeedData {
  base::span<const char*> study_names;  // Names of all studies in the seed.
  const char* base64_uncompressed_data;
  const char* base64_compressed_data;
  const char* base64_signature;

  // Out-of-line constructor/destructor/copy/move required for 'complex'
  // classes.
  SignedSeedData(base::span<const char*> in_study_names,
                 const char* in_base64_uncompressed_data,
                 const char* in_base64_compressed_data,
                 const char* in_base64_signature);
  ~SignedSeedData();
  SignedSeedData(const SignedSeedData&);
  SignedSeedData(SignedSeedData&&);
  SignedSeedData& operator=(const SignedSeedData&);
  SignedSeedData& operator=(SignedSeedData&&);
};

// Packages variations seed pref keys into a tuple for use with StoreSeedInfo().
// This allow easily writing signed seed data into either the safe seed or
// regular seed locations in Local State.
struct SignedSeedPrefKeys {
  const char* base64_compressed_data_key;
  const char* base64_signature_key;
};

// The test seed data is associated with a VariationsSeed with one study,
// "UMA-Uniformity-Trial-10-Percent", and ten equally weighted groups: "default"
// and "group_01" through "group_09". The study is not associated with channels,
// platforms, or features.
extern const SignedSeedData kTestSeedData;

// The crashing seed data contains a CrashingStudy that enables the
// variations::kForceFieldTrialSetupCrashForTesting feature at 100% on all
// platforms and on all channels except Unknown.
extern const SignedSeedData kCrashingSeedData;

// The pref keys used to store safe signed variations seed data.
extern const SignedSeedPrefKeys kSafeSeedPrefKeys;

// The pref keys used to store regular signed variations seed data.
extern const SignedSeedPrefKeys kRegularSeedPrefKeys;

// Mock field trial testing config.
extern const FieldTrialTestingConfig kTestingConfig;

// Disables the use of the field trial testing config to exercise
// VariationsFieldTrialCreator::CreateTrialsFromSeed().
void DisableTestingConfig();

// Enables the use of the field trial testing config.
void EnableTestingConfig();

// Decodes the variations header and extracts the variation ids.
bool ExtractVariationIds(const std::string& variations,
                         std::set<VariationID>* variation_ids,
                         std::set<VariationID>* trigger_ids);

// Creates FieldTrial from given |key| and |id|.
scoped_refptr<base::FieldTrial> CreateTrialAndAssociateId(
    const std::string& trial_name,
    const std::string& default_group_name,
    IDCollectionKey key,
    VariationID id);

// Simulates a crash by setting the clean exit pref to false and disabling
// the steps to update the pref on clean shutdown.
void SimulateCrash(PrefService* local_state);

// Writes |seed_info| into |local_state| using the given seed |pref_keys|.
void WriteSeedData(PrefService* local_state,
                   const SignedSeedData& seed_data,
                   const SignedSeedPrefKeys& pref_keys);

// Returns true if all of the study_names listed in |seed_data| exist in the
// (global) field trial list.
bool FieldTrialListHasAllStudiesFrom(const SignedSeedData& seed_data);

// Resets variations. Ensures that maps can be cleared between tests since they
// are stored as process singleton.
void ResetVariations();

// A no-op UIStringOverrideCallback implementation.
inline void NoopUIStringOverrideCallback(uint32_t hash,
                                         const std::u16string& string) {}

// Create a ClientFilterableState with valid, but unimportant values.
// Tests that actually expect specific values should set them on the result.
std::unique_ptr<ClientFilterableState> CreateDummyClientFilterableState();

// An mock entropy result that will always pick the first non-zero weight group.
constexpr double kAlwaysUseFirstGroup = 0;
// An mock entropy result that will always pick the last non-zero weight group.
constexpr double kAlwaysUseLastGroup = 1.0 - 1e-8;

// EntropyProviders that return known values.
class MockEntropyProviders : public EntropyProviders {
 public:
  struct Results {
    double low_entropy = kAlwaysUseLastGroup;
    std::optional<double> high_entropy = std::nullopt;
    std::optional<double> limited_entropy = std::nullopt;
  };
  explicit MockEntropyProviders(Results results,
                                uint32_t low_entropy_domain = 8000);
  ~MockEntropyProviders() override;

  const base::FieldTrial::EntropyProvider& low_entropy() const override;
  const base::FieldTrial::EntropyProvider& default_entropy() const override;
  const base::FieldTrial::EntropyProvider& limited_entropy() const override;

 private:
  base::MockEntropyProvider low_provider_;
  base::MockEntropyProvider high_provider_;
  base::MockEntropyProvider limited_provider_;
};

// Returns a hex string of the GZipped, base64 encoded, and serialized seed.
std::string GZipAndB64EncodeToHexString(const VariationsSeed& seed);

// Returns whether the active group ids includes the given trial name.
bool ContainsTrialName(const std::vector<ActiveGroupId>& active_group_ids,
                       std::string_view trial_name);

// Returns whether the active group ids includes the given trial name with the
// given group name.
bool ContainsTrialAndGroupName(
    const std::vector<ActiveGroupId>& active_group_ids,
    std::string_view trial_name,
    std::string_view group_name);

}  // namespace variations

#endif  // COMPONENTS_VARIATIONS_VARIATIONS_TEST_UTILS_H_
