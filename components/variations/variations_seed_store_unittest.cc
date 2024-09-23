// Copyright 2014 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/variations/variations_seed_store.h"

#include <memory>
#include <utility>

#include "base/base64.h"
#include "base/build_time.h"
#include "base/functional/callback_helpers.h"
#include "base/run_loop.h"
#include "base/test/metrics/histogram_tester.h"
#include "base/test/protobuf_matchers.h"
#include "base/test/scoped_command_line.h"
#include "base/test/task_environment.h"
#include "base/time/time.h"
#include "base/version.h"
#include "build/build_config.h"
#include "components/prefs/testing_pref_service.h"
#include "components/variations/client_filterable_state.h"
#include "components/variations/pref_names.h"
#include "components/variations/proto/study.pb.h"
#include "components/variations/proto/variations_seed.pb.h"
#include "components/variations/variations_safe_seed_store_local_state.h"
#include "components/variations/variations_switches.h"
#include "components/variations/variations_test_utils.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/zlib/google/compression_utils.h"

#if BUILDFLAG(IS_ANDROID)
#include "components/variations/android/variations_seed_bridge.h"
#endif  // BUILDFLAG(IS_ANDROID)

#if BUILDFLAG(IS_CHROMEOS_ASH)
#include "chromeos/ash/components/dbus/featured/fake_featured_client.h"
#include "chromeos/ash/components/dbus/featured/featured.pb.h"
#endif  // BUILDFLAG(IS_CHROMEOS_ASH)

namespace variations {
namespace {

using ::base::test::EqualsProto;

// The sentinel value that may be stored as the latest variations seed value in
// prefs to indicate that the latest seed is identical to the safe seed.
// Note: This constant is intentionally duplicated in the test because it is
// persisted to disk. In order to maintain backward-compatibility, it's
// important that code continue to correctly handle this specific constant, even
// if the constant used internally in the implementation changes.
constexpr char kIdenticalToSafeSeedSentinel[] = "safe_seed_content";

// TODO(crbug.com/40764723): Consider consolidating TestVariationsSeedStore and
// SignatureVerifyingVariationsSeedStore. Outside of tests, signature
// verification is enabled although prior to crrev.com/c/2181564, signature
// verification was not done on iOS or Android.
class TestVariationsSeedStore : public VariationsSeedStore {
 public:
  explicit TestVariationsSeedStore(
      PrefService* local_state,
      std::unique_ptr<SeedResponse> initial_seed = nullptr,
      bool use_first_run_prefs = true)
      : VariationsSeedStore(
            local_state,
            std::move(initial_seed),
            /*signature_verification_enabled=*/false,
            std::make_unique<VariationsSafeSeedStoreLocalState>(local_state),
            use_first_run_prefs) {}
  ~TestVariationsSeedStore() override = default;
};

class SignatureVerifyingVariationsSeedStore : public VariationsSeedStore {
 public:
  explicit SignatureVerifyingVariationsSeedStore(PrefService* local_state)
      : VariationsSeedStore(
            local_state,
            std::make_unique<VariationsSafeSeedStoreLocalState>(local_state)) {}

  SignatureVerifyingVariationsSeedStore(
      const SignatureVerifyingVariationsSeedStore&) = delete;
  SignatureVerifyingVariationsSeedStore& operator=(
      const SignatureVerifyingVariationsSeedStore&) = delete;

  ~SignatureVerifyingVariationsSeedStore() override = default;
};

// Creates a base::Time object from the corresponding raw value. The specific
// implementation is not important; it's only important that distinct inputs map
// to distinct outputs.
base::Time WrapTime(int64_t time) {
  return base::Time::FromDeltaSinceWindowsEpoch(base::Microseconds(time));
}

// Populates |seed| with simple test data. The resulting seed will contain one
// study called "test", which contains one experiment called "abc" with
// probability weight 100. |seed|'s study field will be cleared before adding
// the new study.
VariationsSeed CreateTestSeed() {
  VariationsSeed seed;
  Study* study = seed.add_study();
  study->set_name("test");
  study->set_default_experiment_name("abc");
  Study_Experiment* experiment = study->add_experiment();
  experiment->set_name("abc");
  experiment->set_probability_weight(100);
  seed.set_serial_number("123");
  return seed;
}

// Returns a ClientFilterableState with all fields set to "interesting" values
// for testing.
std::unique_ptr<ClientFilterableState> CreateTestClientFilterableState() {
  std::unique_ptr<ClientFilterableState> client_state =
      std::make_unique<ClientFilterableState>(
          base::BindOnce([] { return false; }),
          base::BindOnce([] { return base::flat_set<uint64_t>(); }));
  client_state->locale = "es-MX";
  client_state->reference_date = WrapTime(1234554321);
  client_state->version = base::Version("1.2.3.4");
  client_state->channel = Study::CANARY;
  client_state->form_factor = Study::PHONE;
  client_state->platform = Study::PLATFORM_MAC;
  client_state->hardware_class = "mario";
  client_state->is_low_end_device = true;
  client_state->session_consistency_country = "mx";
  client_state->permanent_consistency_country = "br";
  return client_state;
}

// Serializes |seed| to protobuf binary format.
std::string SerializeSeed(const VariationsSeed& seed) {
  std::string serialized_seed;
  seed.SerializeToString(&serialized_seed);
  return serialized_seed;
}

// Compresses |data| using Gzip compression and returns the result.
std::string Gzip(const std::string& data) {
  std::string compressed;
  const bool result = compression::GzipCompress(data, &compressed);
  EXPECT_TRUE(result);
  return compressed;
}

// Gzips |data| and then base64-encodes it.
std::string GzipAndBase64Encode(const std::string& data) {
  return base::Base64Encode(Gzip(data));
}

// Serializes |seed| to gzipped base64-encoded protobuf binary format.
std::string SerializeSeedBase64(const VariationsSeed& seed) {
  return GzipAndBase64Encode(SerializeSeed(seed));
}

// Wrapper over base::Base64Decode() that returns the result.
std::string Base64DecodeData(const std::string& data) {
  std::string decoded;
  EXPECT_TRUE(base::Base64Decode(data, &decoded));
  return decoded;
}

// Sample seeds and the server produced delta between them to verify that the
// client code is able to decode the deltas produced by the server.
struct {
  const std::string base64_initial_seed_data =
      "CigxN2E4ZGJiOTI4ODI0ZGU3ZDU2MGUyODRlODY1ZDllYzg2NzU1MTE0ElgKDFVNQVN0YWJp"
      "bGl0eRjEyomgBTgBQgtTZXBhcmF0ZUxvZ0oLCgdEZWZhdWx0EABKDwoLU2VwYXJhdGVMb2cQ"
      "ZFIVEgszNC4wLjE4MDEuMCAAIAEgAiADEkQKIFVNQS1Vbmlmb3JtaXR5LVRyaWFsLTEwMC1Q"
      "ZXJjZW50GIDjhcAFOAFCCGdyb3VwXzAxSgwKCGdyb3VwXzAxEAFgARJPCh9VTUEtVW5pZm9y"
      "bWl0eS1UcmlhbC01MC1QZXJjZW50GIDjhcAFOAFCB2RlZmF1bHRKDAoIZ3JvdXBfMDEQAUoL"
      "CgdkZWZhdWx0EAFgAQ==";
  const std::string base64_new_seed_data =
      "CigyNGQzYTM3ZTAxYmViOWYwNWYzMjM4YjUzNWY3MDg1ZmZlZWI4NzQwElgKDFVNQVN0YWJp"
      "bGl0eRjEyomgBTgBQgtTZXBhcmF0ZUxvZ0oLCgdEZWZhdWx0EABKDwoLU2VwYXJhdGVMb2cQ"
      "ZFIVEgszNC4wLjE4MDEuMCAAIAEgAiADEpIBCh9VTUEtVW5pZm9ybWl0eS1UcmlhbC0yMC1Q"
      "ZXJjZW50GIDjhcAFOAFCB2RlZmF1bHRKEQoIZ3JvdXBfMDEQARijtskBShEKCGdyb3VwXzAy"
      "EAEYpLbJAUoRCghncm91cF8wMxABGKW2yQFKEQoIZ3JvdXBfMDQQARimtskBShAKB2RlZmF1"
      "bHQQARiitskBYAESWAofVU1BLVVuaWZvcm1pdHktVHJpYWwtNTAtUGVyY2VudBiA44XABTgB"
      "QgdkZWZhdWx0Sg8KC25vbl9kZWZhdWx0EAFKCwoHZGVmYXVsdBABUgQoACgBYAE=";
  const std::string base64_delta_data =
      "KgooMjRkM2EzN2UwMWJlYjlmMDVmMzIzOGI1MzVmNzA4NWZmZWViODc0MAAqW+4BkgEKH1VN"
      "QS1Vbmlmb3JtaXR5LVRyaWFsLTIwLVBlcmNlbnQYgOOFwAU4AUIHZGVmYXVsdEoRCghncm91"
      "cF8wMRABGKO2yQFKEQoIZ3JvdXBfMDIQARiktskBShEKCGdyb3VwXzAzEAEYpbbJAUoRCghn"
      "cm91cF8wNBABGKa2yQFKEAoHZGVmYXVsdBABGKK2yQFgARJYCh9VTUEtVW5pZm9ybWl0eS1U"
      "cmlhbC01MC1QZXJjZW50GIDjhcAFOAFCB2RlZmF1bHRKDwoLbm9uX2RlZmF1bHQQAUoLCgdk"
      "ZWZhdWx0EAFSBCgAKAFgAQ==";

  std::string GetInitialSeedData() {
    return Base64DecodeData(base64_initial_seed_data);
  }

  std::string GetInitialSeedDataAsPrefValue() {
    return GzipAndBase64Encode(GetInitialSeedData());
  }

  std::string GetNewSeedData() {
    return Base64DecodeData(base64_new_seed_data);
  }

  std::string GetDeltaData() { return Base64DecodeData(base64_delta_data); }

} kSeedDeltaTestData;

// Sets all seed-related prefs to non-default values. Used to verify whether
// pref values were cleared.
void SetAllSeedPrefsToNonDefaultValues(PrefService* prefs) {
  const base::Time now = base::Time::Now();
  const base::TimeDelta delta = base::Days(1);

  // Regular seed prefs:
  prefs->SetString(prefs::kVariationsCompressedSeed, "coffee");
  prefs->SetTime(prefs::kVariationsLastFetchTime, now);
  prefs->SetTime(prefs::kVariationsSeedDate, now - delta * 1);
  prefs->SetString(prefs::kVariationsSeedSignature, "tea");

  // Safe seed prefs:
  prefs->SetString(prefs::kVariationsSafeCompressedSeed, "ketchup");
  prefs->SetTime(prefs::kVariationsSafeSeedDate, now - delta * 2);
  prefs->SetTime(prefs::kVariationsSafeSeedFetchTime, now - delta * 3);
  prefs->SetString(prefs::kVariationsSafeSeedLocale, "en-MX");
  prefs->SetInteger(prefs::kVariationsSafeSeedMilestone, 90);
  prefs->SetString(prefs::kVariationsSafeSeedPermanentConsistencyCountry, "mx");
  prefs->SetString(prefs::kVariationsSafeSeedSessionConsistencyCountry, "gt");
  prefs->SetString(prefs::kVariationsSafeSeedSignature, "mustard");
}

// Checks whether the given pref has its default value in |prefs|.
bool PrefHasDefaultValue(const TestingPrefServiceSimple& prefs,
                         const char* pref_name) {
  return prefs.FindPreference(pref_name)->IsDefaultValue();
}

void CheckRegularSeedPrefsAreSet(const TestingPrefServiceSimple& prefs) {
  EXPECT_FALSE(PrefHasDefaultValue(prefs, prefs::kVariationsCompressedSeed));
  EXPECT_FALSE(PrefHasDefaultValue(prefs, prefs::kVariationsLastFetchTime));
  EXPECT_FALSE(PrefHasDefaultValue(prefs, prefs::kVariationsSeedDate));
  EXPECT_FALSE(PrefHasDefaultValue(prefs, prefs::kVariationsSeedSignature));
}

void CheckRegularSeedPrefsAreCleared(const TestingPrefServiceSimple& prefs) {
  EXPECT_TRUE(PrefHasDefaultValue(prefs, prefs::kVariationsCompressedSeed));
  EXPECT_TRUE(PrefHasDefaultValue(prefs, prefs::kVariationsLastFetchTime));
  EXPECT_TRUE(PrefHasDefaultValue(prefs, prefs::kVariationsSeedDate));
  EXPECT_TRUE(PrefHasDefaultValue(prefs, prefs::kVariationsSeedSignature));
}

void CheckSafeSeedPrefsAreSet(const TestingPrefServiceSimple& prefs) {
  EXPECT_FALSE(
      PrefHasDefaultValue(prefs, prefs::kVariationsSafeCompressedSeed));
  EXPECT_FALSE(PrefHasDefaultValue(prefs, prefs::kVariationsSafeSeedDate));
  EXPECT_FALSE(PrefHasDefaultValue(prefs, prefs::kVariationsSafeSeedFetchTime));
  EXPECT_FALSE(PrefHasDefaultValue(prefs, prefs::kVariationsSafeSeedLocale));
  EXPECT_FALSE(PrefHasDefaultValue(prefs, prefs::kVariationsSafeSeedMilestone));
  EXPECT_FALSE(PrefHasDefaultValue(
      prefs, prefs::kVariationsSafeSeedPermanentConsistencyCountry));
  EXPECT_FALSE(PrefHasDefaultValue(
      prefs, prefs::kVariationsSafeSeedSessionConsistencyCountry));
  EXPECT_FALSE(PrefHasDefaultValue(prefs, prefs::kVariationsSafeSeedSignature));
}

void CheckSafeSeedPrefsAreCleared(const TestingPrefServiceSimple& prefs) {
  EXPECT_TRUE(PrefHasDefaultValue(prefs, prefs::kVariationsSafeCompressedSeed));
  EXPECT_TRUE(PrefHasDefaultValue(prefs, prefs::kVariationsSafeSeedDate));
  EXPECT_TRUE(PrefHasDefaultValue(prefs, prefs::kVariationsSafeSeedFetchTime));
  EXPECT_TRUE(PrefHasDefaultValue(prefs, prefs::kVariationsSafeSeedLocale));
  EXPECT_TRUE(PrefHasDefaultValue(prefs, prefs::kVariationsSafeSeedMilestone));
  EXPECT_TRUE(PrefHasDefaultValue(
      prefs, prefs::kVariationsSafeSeedPermanentConsistencyCountry));
  EXPECT_TRUE(PrefHasDefaultValue(
      prefs, prefs::kVariationsSafeSeedSessionConsistencyCountry));
  EXPECT_TRUE(PrefHasDefaultValue(prefs, prefs::kVariationsSafeSeedSignature));
}

}  // namespace

TEST(VariationsSeedStoreTest, LoadSeed_ValidSeed) {
  // Store good seed data to test if loading from prefs works.
  const VariationsSeed seed = CreateTestSeed();
  const std::string base64_seed = SerializeSeedBase64(seed);
  const std::string base64_seed_signature = "a test signature, ignored.";

  TestingPrefServiceSimple prefs;
  VariationsSeedStore::RegisterPrefs(prefs.registry());
  prefs.SetString(prefs::kVariationsCompressedSeed, base64_seed);
  prefs.SetString(prefs::kVariationsSeedSignature, base64_seed_signature);

  TestVariationsSeedStore seed_store(&prefs);
  base::HistogramTester histogram_tester;
  VariationsSeed loaded_seed;
  std::string loaded_seed_data;
  std::string loaded_base64_seed_signature;
  // Check that loading a seed works correctly.
  EXPECT_TRUE(seed_store.LoadSeed(&loaded_seed, &loaded_seed_data,
                                  &loaded_base64_seed_signature));

  // Verify metrics.
  histogram_tester.ExpectUniqueSample("Variations.SeedLoadResult",
                                      LoadSeedResult::kSuccess, 1);

  // Check that the loaded data is the same as the original.
  EXPECT_EQ(SerializeSeed(seed), SerializeSeed(loaded_seed));
  EXPECT_EQ(SerializeSeed(seed), loaded_seed_data);
  EXPECT_EQ(base64_seed_signature, loaded_base64_seed_signature);
  // Make sure the pref hasn't been changed.
  EXPECT_FALSE(PrefHasDefaultValue(prefs, prefs::kVariationsCompressedSeed));
  EXPECT_EQ(base64_seed, prefs.GetString(prefs::kVariationsCompressedSeed));
}

TEST(VariationsSeedStoreTest, LoadSeed_InvalidSeed) {
  TestingPrefServiceSimple prefs;
  VariationsSeedStore::RegisterPrefs(prefs.registry());
  SetAllSeedPrefsToNonDefaultValues(&prefs);
  prefs.SetString(prefs::kVariationsCompressedSeed, "this should fail");

  // Loading an invalid seed should return false.
  TestVariationsSeedStore seed_store(&prefs);
  base::HistogramTester histogram_tester;
  VariationsSeed loaded_seed;
  std::string loaded_seed_data;
  std::string loaded_base64_seed_signature;
  EXPECT_FALSE(seed_store.LoadSeed(&loaded_seed, &loaded_seed_data,
                                   &loaded_base64_seed_signature));

  // Verify metrics and prefs.
  histogram_tester.ExpectUniqueSample("Variations.SeedLoadResult",
                                      LoadSeedResult::kCorruptBase64, 1);
  CheckRegularSeedPrefsAreCleared(prefs);
  CheckSafeSeedPrefsAreSet(prefs);
}

TEST(VariationsSeedStoreTest, LoadSeed_InvalidSignature) {
  const VariationsSeed seed = CreateTestSeed();
  const std::string base64_seed = SerializeSeedBase64(seed);
  const std::string base64_seed_signature = "a deeply compromised signature.";

  TestingPrefServiceSimple prefs;
  VariationsSeedStore::RegisterPrefs(prefs.registry());
  SetAllSeedPrefsToNonDefaultValues(&prefs);
  prefs.SetString(prefs::kVariationsCompressedSeed, base64_seed);
  prefs.SetString(prefs::kVariationsSeedSignature, base64_seed_signature);

  // Loading a valid seed with an invalid signature should return false and
  // clear all associated prefs when signature verification is enabled.
  SignatureVerifyingVariationsSeedStore seed_store(&prefs);
  base::HistogramTester histogram_tester;
  VariationsSeed loaded_seed;
  std::string loaded_seed_data;
  std::string loaded_base64_seed_signature;
  EXPECT_FALSE(seed_store.LoadSeed(&loaded_seed, &loaded_seed_data,
                                   &loaded_base64_seed_signature));

  // Verify metrics and prefs.
  histogram_tester.ExpectUniqueSample("Variations.SeedLoadResult",
                                      LoadSeedResult::kInvalidSignature, 1);
  CheckRegularSeedPrefsAreCleared(prefs);
  CheckSafeSeedPrefsAreSet(prefs);
}

TEST(VariationsSeedStoreTest, LoadSeed_InvalidProto) {
  const std::string base64_seed = GzipAndBase64Encode("Not a proto");

  TestingPrefServiceSimple prefs;
  VariationsSeedStore::RegisterPrefs(prefs.registry());
  SetAllSeedPrefsToNonDefaultValues(&prefs);
  prefs.SetString(prefs::kVariationsCompressedSeed, base64_seed);

  // Loading a valid seed with an invalid signature should return false and
  // clear all associated prefs when signature verification is enabled.
  TestVariationsSeedStore seed_store(&prefs);
  base::HistogramTester histogram_tester;
  VariationsSeed loaded_seed;
  std::string loaded_seed_data;
  std::string loaded_base64_seed_signature;
  EXPECT_FALSE(seed_store.LoadSeed(&loaded_seed, &loaded_seed_data,
                                   &loaded_base64_seed_signature));

  // Verify metrics and prefs.
  histogram_tester.ExpectUniqueSample("Variations.SeedLoadResult",
                                      LoadSeedResult::kCorruptProtobuf, 1);
  CheckRegularSeedPrefsAreCleared(prefs);
  CheckSafeSeedPrefsAreSet(prefs);
}

TEST(VariationsSeedStoreTest, LoadSeed_RejectEmptySignature) {
  const VariationsSeed seed = CreateTestSeed();
  const std::string base64_seed = SerializeSeedBase64(seed);
  const std::string base64_seed_signature = "";

  TestingPrefServiceSimple prefs;
  VariationsSeedStore::RegisterPrefs(prefs.registry());
  SetAllSeedPrefsToNonDefaultValues(&prefs);
  prefs.SetString(prefs::kVariationsCompressedSeed, base64_seed);
  prefs.SetString(prefs::kVariationsSeedSignature, base64_seed_signature);

  // Loading a valid seed with an empty signature should fail and clear all
  // associated prefs when signature verification is enabled.
  SignatureVerifyingVariationsSeedStore seed_store(&prefs);
  base::HistogramTester histogram_tester;
  VariationsSeed loaded_seed;
  std::string loaded_seed_data;
  std::string loaded_base64_seed_signature;
  EXPECT_FALSE(seed_store.LoadSeed(&loaded_seed, &loaded_seed_data,
                                   &loaded_base64_seed_signature));

  // Verify metrics and prefs.
  histogram_tester.ExpectUniqueSample("Variations.SeedLoadResult",
                                      LoadSeedResult::kInvalidSignature, 1);
  CheckRegularSeedPrefsAreCleared(prefs);
  CheckSafeSeedPrefsAreSet(prefs);
}

TEST(VariationsSeedStoreTest, LoadSeed_AcceptEmptySignature) {
  const VariationsSeed seed = CreateTestSeed();
  const std::string base64_seed = SerializeSeedBase64(seed);
  const std::string base64_seed_signature = "";

  TestingPrefServiceSimple prefs;
  VariationsSeedStore::RegisterPrefs(prefs.registry());
  SetAllSeedPrefsToNonDefaultValues(&prefs);
  prefs.SetString(prefs::kVariationsCompressedSeed, base64_seed);
  prefs.SetString(prefs::kVariationsSeedSignature, base64_seed_signature);

  // Loading a valid seed with an empty signature should succeed iff
  // switches::kAcceptEmptySeedSignatureForTesting is on the command line.
  base::test::ScopedCommandLine scoped_command_line;
  scoped_command_line.GetProcessCommandLine()->AppendSwitch(
      switches::kAcceptEmptySeedSignatureForTesting);

  SignatureVerifyingVariationsSeedStore seed_store(&prefs);
  base::HistogramTester histogram_tester;
  VariationsSeed loaded_seed;
  std::string loaded_seed_data;
  std::string loaded_base64_seed_signature;
  EXPECT_TRUE(seed_store.LoadSeed(&loaded_seed, &loaded_seed_data,
                                  &loaded_base64_seed_signature));

  // Verify metrics and prefs.
  histogram_tester.ExpectUniqueSample("Variations.SeedLoadResult",
                                      LoadSeedResult::kSuccess, 1);
  CheckRegularSeedPrefsAreSet(prefs);
  CheckSafeSeedPrefsAreSet(prefs);
}

TEST(VariationsSeedStoreTest, LoadSeed_EmptySeed) {
  TestingPrefServiceSimple prefs;
  VariationsSeedStore::RegisterPrefs(prefs.registry());
  ASSERT_TRUE(PrefHasDefaultValue(prefs, prefs::kVariationsCompressedSeed));

  // Loading an empty seed should return false.
  TestVariationsSeedStore seed_store(&prefs);
  base::HistogramTester histogram_tester;
  VariationsSeed loaded_seed;
  std::string loaded_seed_data;
  std::string loaded_base64_seed_signature;
  EXPECT_FALSE(seed_store.LoadSeed(&loaded_seed, &loaded_seed_data,
                                   &loaded_base64_seed_signature));

  // Verify metrics.
  histogram_tester.ExpectUniqueSample("Variations.SeedLoadResult",
                                      LoadSeedResult::kEmpty, 1);
}

TEST(VariationsSeedStoreTest, LoadSeed_IdenticalToSafeSeed) {
  // Store good seed data to the safe seed prefs, and store a sentinel value to
  // the latest seed pref, to verify that loading via the alias works.
  const VariationsSeed seed = CreateTestSeed();
  const std::string base64_seed = SerializeSeedBase64(seed);
  const std::string base64_seed_signature = "a test signature, ignored.";

  TestingPrefServiceSimple prefs;
  VariationsSeedStore::RegisterPrefs(prefs.registry());
  prefs.SetString(prefs::kVariationsCompressedSeed,
                  kIdenticalToSafeSeedSentinel);
  prefs.SetString(prefs::kVariationsSafeCompressedSeed, base64_seed);
  prefs.SetString(prefs::kVariationsSeedSignature, base64_seed_signature);

  TestVariationsSeedStore seed_store(&prefs);
  base::HistogramTester histogram_tester;
  VariationsSeed loaded_seed;
  std::string loaded_seed_data;
  std::string loaded_base64_seed_signature;
  // Check that loading the seed works correctly.
  EXPECT_TRUE(seed_store.LoadSeed(&loaded_seed, &loaded_seed_data,
                                  &loaded_base64_seed_signature));

  // Verify metrics.
  histogram_tester.ExpectUniqueSample("Variations.SeedLoadResult",
                                      LoadSeedResult::kSuccess, 1);

  // Check that the loaded data is the same as the original.
  EXPECT_EQ(SerializeSeed(seed), SerializeSeed(loaded_seed));
  EXPECT_EQ(SerializeSeed(seed), loaded_seed_data);
  EXPECT_EQ(base64_seed_signature, loaded_base64_seed_signature);
}

TEST(VariationsSeedStoreTest, ApplyDeltaPatch) {
  std::string output;
  EXPECT_TRUE(VariationsSeedStore::ApplyDeltaPatch(
      kSeedDeltaTestData.GetInitialSeedData(),
      kSeedDeltaTestData.GetDeltaData(), &output));
  EXPECT_EQ(kSeedDeltaTestData.GetNewSeedData(), output);
}

class VariationsStoreSeedDataTest : public ::testing::Test,
                                    public ::testing::WithParamInterface<bool> {
 public:
  VariationsStoreSeedDataTest() = default;
  ~VariationsStoreSeedDataTest() override = default;

  bool RequireSynchronousStores() { return GetParam(); }

  struct Params {
    std::string country_code;
    bool is_delta_compressed;
    bool is_gzip_compressed;
  };

  // Wrapper for VariationsSeedStore::StoreSeedData() exposing a more convenient
  // API. Invokes either the underlying function either in sync or async mode,
  // but if async, it blocks on its completion.
  bool StoreSeedData(VariationsSeedStore& seed_store,
                     const std::string& seed_data,
                     const Params& params = {}) {
    base::RunLoop run_loop;
    seed_store.StoreSeedData(
        seed_data, /*base64_seed_signature=*/std::string(), params.country_code,
        base::Time::Now(), params.is_delta_compressed,
        params.is_gzip_compressed,
        base::BindOnce(&VariationsStoreSeedDataTest::OnSeedStoreResult,
                       base::Unretained(this), run_loop.QuitClosure()),
        RequireSynchronousStores());
    // If we're testing synchronous stores, we shouldn't issue a Run() call so
    // that the test verifies that the operation completed synchronously.
    if (!RequireSynchronousStores())
      run_loop.Run();
    return store_success_;
  }

  void OnSeedStoreResult(base::RepeatingClosure quit_closure,
                         bool store_success,
                         VariationsSeed seed) {
    store_success_ = store_success;
    stored_seed_.Swap(&seed);
    quit_closure.Run();
  }

  base::test::TaskEnvironment task_environment_;
  bool store_success_ = false;
  VariationsSeed stored_seed_;
};

INSTANTIATE_TEST_SUITE_P(All, VariationsStoreSeedDataTest, ::testing::Bool());

TEST_P(VariationsStoreSeedDataTest, StoreSeedData) {
  const VariationsSeed seed = CreateTestSeed();
  const std::string serialized_seed = SerializeSeed(seed);

  TestingPrefServiceSimple prefs;
  VariationsSeedStore::RegisterPrefs(prefs.registry());
  TestVariationsSeedStore seed_store(&prefs);

  ASSERT_TRUE(StoreSeedData(seed_store, serialized_seed));
  // Make sure the pref was actually set.
  EXPECT_FALSE(PrefHasDefaultValue(prefs, prefs::kVariationsCompressedSeed));

  std::string loaded_compressed_seed =
      prefs.GetString(prefs::kVariationsCompressedSeed);
  // Make sure the stored seed from pref is the same as the seed we created.
  EXPECT_EQ(loaded_compressed_seed, GzipAndBase64Encode(serialized_seed));

  // Check if trying to store a bad seed leaves the pref unchanged.
  prefs.ClearPref(prefs::kVariationsCompressedSeed);
  ASSERT_FALSE(StoreSeedData(seed_store, "should fail"));
  EXPECT_TRUE(PrefHasDefaultValue(prefs, prefs::kVariationsCompressedSeed));
}

TEST_P(VariationsStoreSeedDataTest, ParsedSeed) {
  const VariationsSeed seed = CreateTestSeed();
  const std::string serialized_seed = SerializeSeed(seed);

  TestingPrefServiceSimple prefs;
  VariationsSeedStore::RegisterPrefs(prefs.registry());
  TestVariationsSeedStore seed_store(&prefs);

  ASSERT_TRUE(StoreSeedData(seed_store, serialized_seed));
  EXPECT_EQ(serialized_seed, SerializeSeed(stored_seed_));
}

TEST_P(VariationsStoreSeedDataTest, CountryCode) {
  TestingPrefServiceSimple prefs;
  VariationsSeedStore::RegisterPrefs(prefs.registry());
  TestVariationsSeedStore seed_store(&prefs);

  // Test with a valid header value.
  std::string seed = SerializeSeed(CreateTestSeed());
  ASSERT_TRUE(
      StoreSeedData(seed_store, seed, {.country_code = "test_country"}));
  EXPECT_EQ("test_country", prefs.GetString(prefs::kVariationsCountry));

  // Test with no country code specified - which should preserve the old value.
  ASSERT_TRUE(StoreSeedData(seed_store, seed));
  EXPECT_EQ("test_country", prefs.GetString(prefs::kVariationsCountry));
}

TEST_P(VariationsStoreSeedDataTest, GzippedSeed) {
  const VariationsSeed seed = CreateTestSeed();
  const std::string serialized_seed = SerializeSeed(seed);
  std::string compressed_seed = Gzip(serialized_seed);

  TestingPrefServiceSimple prefs;
  VariationsSeedStore::RegisterPrefs(prefs.registry());
  TestVariationsSeedStore seed_store(&prefs);

  ASSERT_TRUE(
      StoreSeedData(seed_store, compressed_seed, {.is_gzip_compressed = true}));
  EXPECT_EQ(serialized_seed, SerializeSeed(stored_seed_));
}

TEST_P(VariationsStoreSeedDataTest, GzippedEmptySeed) {
  std::string empty_seed;
  std::string compressed_seed = Gzip(empty_seed);

  TestingPrefServiceSimple prefs;
  VariationsSeedStore::RegisterPrefs(prefs.registry());
  TestVariationsSeedStore seed_store(&prefs);

  store_success_ = true;
  StoreSeedData(seed_store, compressed_seed, {.is_gzip_compressed = true});
  EXPECT_FALSE(store_success_);
}

TEST_P(VariationsStoreSeedDataTest, DeltaCompressed) {
  TestingPrefServiceSimple prefs;
  VariationsSeedStore::RegisterPrefs(prefs.registry());

  prefs.SetString(prefs::kVariationsCompressedSeed,
                  kSeedDeltaTestData.GetInitialSeedDataAsPrefValue());
  prefs.SetString(prefs::kVariationsSeedSignature, "ignored signature");

  TestVariationsSeedStore seed_store(&prefs);

  ASSERT_TRUE(StoreSeedData(seed_store, kSeedDeltaTestData.GetDeltaData(),
                            {.is_delta_compressed = true}));
  EXPECT_EQ(kSeedDeltaTestData.GetNewSeedData(), SerializeSeed(stored_seed_));
}

TEST_P(VariationsStoreSeedDataTest, DeltaCompressedGzipped) {
  TestingPrefServiceSimple prefs;
  VariationsSeedStore::RegisterPrefs(prefs.registry());

  prefs.SetString(prefs::kVariationsCompressedSeed,
                  kSeedDeltaTestData.GetInitialSeedDataAsPrefValue());
  prefs.SetString(prefs::kVariationsSeedSignature, "ignored signature");

  TestVariationsSeedStore seed_store(&prefs);

  ASSERT_TRUE(StoreSeedData(seed_store, Gzip(kSeedDeltaTestData.GetDeltaData()),
                            {
                                .is_delta_compressed = true,
                                .is_gzip_compressed = true,
                            }));
  EXPECT_EQ(kSeedDeltaTestData.GetNewSeedData(), SerializeSeed(stored_seed_));
}

TEST_P(VariationsStoreSeedDataTest, DeltaButNoInitialSeed) {
  TestingPrefServiceSimple prefs;
  VariationsSeedStore::RegisterPrefs(prefs.registry());

  TestVariationsSeedStore seed_store(&prefs);

  store_success_ = true;
  StoreSeedData(seed_store, Gzip(kSeedDeltaTestData.GetDeltaData()),
                {
                    .is_delta_compressed = true,
                    .is_gzip_compressed = true,
                });
  EXPECT_FALSE(store_success_);
}

TEST_P(VariationsStoreSeedDataTest, BadDelta) {
  TestingPrefServiceSimple prefs;
  VariationsSeedStore::RegisterPrefs(prefs.registry());

  prefs.SetString(prefs::kVariationsCompressedSeed,
                  kSeedDeltaTestData.GetInitialSeedDataAsPrefValue());
  prefs.SetString(prefs::kVariationsSeedSignature, "ignored signature");

  TestVariationsSeedStore seed_store(&prefs);

  store_success_ = true;
  // Provide a gzipped delta, when gzip is not expected.
  StoreSeedData(seed_store, Gzip(kSeedDeltaTestData.GetDeltaData()),
                {.is_delta_compressed = true});
  EXPECT_FALSE(store_success_);
}

TEST_P(VariationsStoreSeedDataTest, IdenticalToSafeSeed) {
  const VariationsSeed seed = CreateTestSeed();
  const std::string serialized_seed = SerializeSeed(seed);
  const std::string base64_seed = SerializeSeedBase64(seed);

  TestingPrefServiceSimple prefs;
  VariationsSeedStore::RegisterPrefs(prefs.registry());
  prefs.SetString(prefs::kVariationsSafeCompressedSeed, base64_seed);

  TestVariationsSeedStore seed_store(&prefs);
  ASSERT_TRUE(StoreSeedData(seed_store, serialized_seed));

  // Verify that the pref has a sentinel value, rather than the full string.
  EXPECT_EQ(kIdenticalToSafeSeedSentinel,
            prefs.GetString(prefs::kVariationsCompressedSeed));

  // Verify that loading the stored seed returns the original seed value.
  VariationsSeed loaded_seed;
  std::string loaded_seed_data;
  std::string unused_loaded_base64_seed_signature;
  EXPECT_TRUE(seed_store.LoadSeed(&loaded_seed, &loaded_seed_data,
                                  &unused_loaded_base64_seed_signature));

  EXPECT_EQ(SerializeSeed(seed), SerializeSeed(loaded_seed));
  EXPECT_EQ(SerializeSeed(seed), loaded_seed_data);
}

// Verifies that the cached serial number is correctly updated when a new seed
// is saved.
TEST_P(VariationsStoreSeedDataTest,
       GetLatestSerialNumber_UpdatedWithNewStoredSeed) {
  // Store good seed data initially.
  const VariationsSeed seed = CreateTestSeed();
  const std::string base64_seed = SerializeSeedBase64(seed);
  const std::string base64_seed_signature = "a completely ignored signature";

  TestingPrefServiceSimple prefs;
  VariationsSeedStore::RegisterPrefs(prefs.registry());
  prefs.SetString(prefs::kVariationsCompressedSeed, base64_seed);
  prefs.SetString(prefs::kVariationsSeedSignature, base64_seed_signature);

  // Call GetLatestSerialNumber() once to prime the cached value.
  TestVariationsSeedStore seed_store(&prefs);
  EXPECT_EQ("123", seed_store.GetLatestSerialNumber());

  VariationsSeed new_seed = CreateTestSeed();
  new_seed.set_serial_number("456");
  ASSERT_TRUE(StoreSeedData(seed_store, SerializeSeed(new_seed)));
  EXPECT_EQ("456", seed_store.GetLatestSerialNumber());
}

TEST(VariationsSeedStoreTest, LoadSafeSeed_ValidSeed) {
  // Store good seed data to test if loading from prefs works.
  const VariationsSeed seed = CreateTestSeed();
  const std::string base64_seed = SerializeSeedBase64(seed);
  const std::string base64_seed_signature = "a test signature, ignored.";
  const base::Time reference_date = base::Time::Now();
  const base::Time fetch_time = reference_date - base::Days(3);
  const std::string locale = "en-US";
  const std::string permanent_consistency_country = "us";
  const std::string session_consistency_country = "ca";

  TestingPrefServiceSimple prefs;
  VariationsSeedStore::RegisterPrefs(prefs.registry());
  prefs.SetString(prefs::kVariationsSafeCompressedSeed, base64_seed);
  prefs.SetString(prefs::kVariationsSafeSeedSignature, base64_seed_signature);
  prefs.SetTime(prefs::kVariationsSafeSeedDate, reference_date);
  prefs.SetTime(prefs::kVariationsSafeSeedFetchTime, fetch_time);
  prefs.SetString(prefs::kVariationsSafeSeedLocale, locale);
  prefs.SetString(prefs::kVariationsSafeSeedPermanentConsistencyCountry,
                  permanent_consistency_country);
  prefs.SetString(prefs::kVariationsSafeSeedSessionConsistencyCountry,
                  session_consistency_country);

  // Attempt to load a valid safe seed.
  TestVariationsSeedStore seed_store(&prefs);
  base::HistogramTester histogram_tester;
  VariationsSeed loaded_seed;
  std::unique_ptr<ClientFilterableState> client_state =
      CreateTestClientFilterableState();
  EXPECT_TRUE(seed_store.LoadSafeSeed(&loaded_seed, client_state.get()));

  // Verify metrics.
  histogram_tester.ExpectUniqueSample("Variations.SafeMode.LoadSafeSeed.Result",
                                      LoadSeedResult::kSuccess, 1);

  // Check that the loaded data is the same as the original.
  EXPECT_EQ(SerializeSeed(seed), SerializeSeed(loaded_seed));
  EXPECT_EQ(locale, client_state->locale);
  EXPECT_EQ(reference_date, client_state->reference_date);
  EXPECT_EQ(permanent_consistency_country,
            client_state->permanent_consistency_country);
  EXPECT_EQ(session_consistency_country,
            client_state->session_consistency_country);

  // Make sure that other data in the |client_state| hasn't been changed.
  std::unique_ptr<ClientFilterableState> original_state =
      CreateTestClientFilterableState();
  EXPECT_EQ(original_state->version, client_state->version);
  EXPECT_EQ(original_state->channel, client_state->channel);
  EXPECT_EQ(original_state->form_factor, client_state->form_factor);
  EXPECT_EQ(original_state->platform, client_state->platform);
  EXPECT_EQ(original_state->hardware_class, client_state->hardware_class);
  EXPECT_EQ(original_state->is_low_end_device, client_state->is_low_end_device);

  // Make sure the pref hasn't been changed.
  EXPECT_FALSE(
      PrefHasDefaultValue(prefs, prefs::kVariationsSafeCompressedSeed));
  EXPECT_EQ(base64_seed, prefs.GetString(prefs::kVariationsSafeCompressedSeed));
}

TEST(VariationsSeedStoreTest, LoadSafeSeed_CorruptSeed) {
  TestingPrefServiceSimple prefs;
  VariationsSeedStore::RegisterPrefs(prefs.registry());
  SetAllSeedPrefsToNonDefaultValues(&prefs);
  prefs.SetString(prefs::kVariationsSafeCompressedSeed, "this should fail");

  // Attempt to load a corrupted safe seed.
  TestVariationsSeedStore seed_store(&prefs);
  base::HistogramTester histogram_tester;
  VariationsSeed loaded_seed;
  std::unique_ptr<ClientFilterableState> client_state =
      CreateTestClientFilterableState();
  EXPECT_FALSE(seed_store.LoadSafeSeed(&loaded_seed, client_state.get()));

  // Verify metrics and prefs.
  histogram_tester.ExpectUniqueSample("Variations.SafeMode.LoadSafeSeed.Result",
                                      LoadSeedResult::kCorruptBase64, 1);
  CheckSafeSeedPrefsAreCleared(prefs);
  CheckRegularSeedPrefsAreSet(prefs);

  // Moreover, loading an invalid seed should leave the |client_state|
  // unmodified.
  std::unique_ptr<ClientFilterableState> original_state =
      CreateTestClientFilterableState();
  EXPECT_EQ(original_state->locale, client_state->locale);
  EXPECT_EQ(original_state->reference_date, client_state->reference_date);
  EXPECT_EQ(original_state->session_consistency_country,
            client_state->session_consistency_country);
  EXPECT_EQ(original_state->permanent_consistency_country,
            client_state->permanent_consistency_country);
}

TEST(VariationsSeedStoreTest, LoadSafeSeed_InvalidSignature) {
  const VariationsSeed seed = CreateTestSeed();
  const std::string base64_seed = SerializeSeedBase64(seed);
  const std::string base64_seed_signature = "a deeply compromised signature.";

  TestingPrefServiceSimple prefs;
  VariationsSeedStore::RegisterPrefs(prefs.registry());
  SetAllSeedPrefsToNonDefaultValues(&prefs);
  prefs.SetString(prefs::kVariationsSafeCompressedSeed, base64_seed);
  prefs.SetString(prefs::kVariationsSafeSeedSignature, base64_seed_signature);

  // Attempt to load a valid safe seed with an invalid signature while signature
  // verification is enabled.
  SignatureVerifyingVariationsSeedStore seed_store(&prefs);
  base::HistogramTester histogram_tester;
  VariationsSeed loaded_seed;
  std::unique_ptr<ClientFilterableState> client_state =
      CreateTestClientFilterableState();
  EXPECT_FALSE(seed_store.LoadSafeSeed(&loaded_seed, client_state.get()));

  // Verify metrics and prefs.
  histogram_tester.ExpectUniqueSample("Variations.SafeMode.LoadSafeSeed.Result",
                                      LoadSeedResult::kInvalidSignature, 1);
  CheckSafeSeedPrefsAreCleared(prefs);
  CheckRegularSeedPrefsAreSet(prefs);

  // Moreover, the passed-in |client_state| should remain unmodified.
  std::unique_ptr<ClientFilterableState> original_state =
      CreateTestClientFilterableState();
  EXPECT_EQ(original_state->locale, client_state->locale);
  EXPECT_EQ(original_state->reference_date, client_state->reference_date);
  EXPECT_EQ(original_state->session_consistency_country,
            client_state->session_consistency_country);
  EXPECT_EQ(original_state->permanent_consistency_country,
            client_state->permanent_consistency_country);
}

TEST(VariationsSeedStoreTest, LoadSafeSeed_EmptySeed) {
  TestingPrefServiceSimple prefs;
  VariationsSeedStore::RegisterPrefs(prefs.registry());
  ASSERT_TRUE(PrefHasDefaultValue(prefs, prefs::kVariationsSafeCompressedSeed));

  // Attempt to load an empty safe seed.
  TestVariationsSeedStore seed_store(&prefs);
  base::HistogramTester histogram_tester;
  VariationsSeed loaded_seed;
  std::unique_ptr<ClientFilterableState> client_state =
      CreateTestClientFilterableState();
  EXPECT_FALSE(seed_store.LoadSafeSeed(&loaded_seed, client_state.get()));

  // Verify metrics.
  histogram_tester.ExpectUniqueSample("Variations.SafeMode.LoadSafeSeed.Result",
                                      LoadSeedResult::kEmpty, 1);
}

struct InvalidSafeSeedTestParams {
  const std::string test_name;
  const std::string seed;
  const std::string signature;
  StoreSeedResult store_seed_result;
  std::optional<VerifySignatureResult> verify_signature_result = std::nullopt;
};

using StoreInvalidSafeSeedTest =
    ::testing::TestWithParam<InvalidSafeSeedTestParams>;

INSTANTIATE_TEST_SUITE_P(
    All,
    StoreInvalidSafeSeedTest,
    ::testing::Values(
        InvalidSafeSeedTestParams{
            .test_name = "EmptySeed",
            .seed = "",
            .signature = "unused signature",
            .store_seed_result = StoreSeedResult::kFailedEmptyGzipContents},
        InvalidSafeSeedTestParams{
            .test_name = "InvalidSeed",
            .seed = "invalid seed",
            .signature = "unused signature",
            .store_seed_result = StoreSeedResult::kFailedParse},
        InvalidSafeSeedTestParams{
            .test_name = "InvalidSignature",
            .seed = SerializeSeed(CreateTestSeed()),
            // A well-formed signature that does not correspond to the seed.
            .signature = kTestSeedData.base64_signature,
            .store_seed_result = StoreSeedResult::kFailedSignature,
            .verify_signature_result = VerifySignatureResult::INVALID_SEED}),
    [](const ::testing::TestParamInfo<InvalidSafeSeedTestParams>& params) {
      return params.param.test_name;
    });

// Verify that attempting to store an invalid safe seed fails and does not
// modify Local State's existing safe-seed-related prefs.
TEST_P(StoreInvalidSafeSeedTest, StoreSafeSeed) {
  TestingPrefServiceSimple prefs;
  VariationsSeedStore::RegisterPrefs(prefs.registry());

  // Set a safe seed and its associated prefs to their expected values. Also,
  // specify different safe seed pref values that are later attempted to be
  // stored.
  const std::string expected_seed = "a seed";
  InvalidSafeSeedTestParams params = GetParam();
  const std::string seed_to_store = params.seed;
  prefs.SetString(prefs::kVariationsSafeCompressedSeed, expected_seed);

  const std::string expected_signature = "a signature";
  const std::string signature_to_store = params.signature;
  prefs.SetString(prefs::kVariationsSafeSeedSignature, expected_signature);

  const int expected_milestone = 90;
  const int milestone_to_store = 91;
  prefs.SetInteger(prefs::kVariationsSafeSeedMilestone, expected_milestone);

  const base::Time now = base::Time::Now();
  const base::Time expected_fetch_time = now - base::Hours(3);
  const base::Time fetch_time_to_store = now - base::Hours(1);
  prefs.SetTime(prefs::kVariationsSafeSeedFetchTime, expected_fetch_time);

  std::unique_ptr<ClientFilterableState> client_state =
      CreateTestClientFilterableState();

  const std::string expected_locale = "en-US";
  client_state->locale = "pt-PT";
  prefs.SetString(prefs::kVariationsSafeSeedLocale, expected_locale);

  const std::string expected_permanent_consistency_country = "US";
  client_state->permanent_consistency_country = "CA";
  prefs.SetString(prefs::kVariationsSafeSeedPermanentConsistencyCountry,
                  expected_permanent_consistency_country);

  const std::string expected_session_consistency_country = "BR";
  client_state->session_consistency_country = "PT";
  prefs.SetString(prefs::kVariationsSafeSeedSessionConsistencyCountry,
                  expected_session_consistency_country);

  const base::Time expected_date = now - base::Days(2);
  client_state->reference_date = now - base::Days(1);
  prefs.SetTime(prefs::kVariationsSafeSeedDate, expected_date);

  SignatureVerifyingVariationsSeedStore seed_store(&prefs);
  base::HistogramTester histogram_tester;

  // Verify that attempting to store an invalid seed fails.
  EXPECT_FALSE(seed_store.StoreSafeSeed(seed_to_store, signature_to_store,
                                        milestone_to_store, *client_state,
                                        fetch_time_to_store));

  // Verify that none of the safe seed prefs were overwritten.
  EXPECT_EQ(prefs.GetString(prefs::kVariationsSafeCompressedSeed),
            expected_seed);
  EXPECT_EQ(prefs.GetString(prefs::kVariationsSafeSeedSignature),
            expected_signature);
  EXPECT_EQ(prefs.GetString(prefs::kVariationsSafeSeedLocale), expected_locale);
  EXPECT_EQ(prefs.GetInteger(prefs::kVariationsSafeSeedMilestone),
            expected_milestone);
  EXPECT_EQ(
      prefs.GetString(prefs::kVariationsSafeSeedPermanentConsistencyCountry),
      expected_permanent_consistency_country);
  EXPECT_EQ(
      prefs.GetString(prefs::kVariationsSafeSeedSessionConsistencyCountry),
      expected_session_consistency_country);
  EXPECT_EQ(prefs.GetTime(prefs::kVariationsSafeSeedDate), expected_date);
  EXPECT_EQ(prefs.GetTime(prefs::kVariationsSafeSeedFetchTime),
            expected_fetch_time);

  // Verify metrics.
  histogram_tester.ExpectUniqueSample(
      "Variations.SafeMode.StoreSafeSeed.Result", params.store_seed_result, 1);
  if (params.verify_signature_result.has_value()) {
    histogram_tester.ExpectUniqueSample(
        "Variations.SafeMode.StoreSafeSeed.SignatureValidity",
        params.verify_signature_result.value(), 1);
  }
}

TEST(VariationsSeedStoreTest, StoreSafeSeed_ValidSignature) {
  std::string expected_seed;
  ASSERT_TRUE(base::Base64Decode(kTestSeedData.base64_uncompressed_data,
                                 &expected_seed));
  const std::string expected_signature = kTestSeedData.base64_signature;
  const int expected_seed_milestone = 92;

  auto client_state = CreateDummyClientFilterableState();
  const std::string expected_locale = "en-US";
  client_state->locale = expected_locale;
  const base::Time now = base::Time::Now();
  const base::Time expected_date = now - base::Days(1);
  client_state->reference_date = expected_date;
  const std::string expected_permanent_consistency_country = "US";
  client_state->permanent_consistency_country =
      expected_permanent_consistency_country;
  const std::string expected_session_consistency_country = "CA";
  client_state->session_consistency_country =
      expected_session_consistency_country;
  const base::Time expected_fetch_time = now - base::Hours(6);

  TestingPrefServiceSimple prefs;
  VariationsSeedStore::RegisterPrefs(prefs.registry());
  SignatureVerifyingVariationsSeedStore seed_store(&prefs);
  base::HistogramTester histogram_tester;

  // Verify that storing the safe seed succeeded.
  EXPECT_TRUE(seed_store.StoreSafeSeed(expected_seed, expected_signature,
                                       expected_seed_milestone, *client_state,
                                       expected_fetch_time));

  // Verify that safe-seed-related prefs were successfully stored.
  const std::string safe_seed =
      prefs.GetString(prefs::kVariationsSafeCompressedSeed);
  std::string decoded_compressed_seed;
  ASSERT_TRUE(base::Base64Decode(safe_seed, &decoded_compressed_seed));
  EXPECT_EQ(Gzip(expected_seed), decoded_compressed_seed);
  EXPECT_EQ(prefs.GetString(prefs::kVariationsSafeSeedSignature),
            expected_signature);
  EXPECT_EQ(prefs.GetString(prefs::kVariationsSafeSeedLocale), expected_locale);
  EXPECT_EQ(prefs.GetInteger(prefs::kVariationsSafeSeedMilestone),
            expected_seed_milestone);
  EXPECT_EQ(
      prefs.GetString(prefs::kVariationsSafeSeedPermanentConsistencyCountry),
      expected_permanent_consistency_country);
  EXPECT_EQ(
      prefs.GetString(prefs::kVariationsSafeSeedSessionConsistencyCountry),
      expected_session_consistency_country);
  EXPECT_EQ(prefs.GetTime(prefs::kVariationsSafeSeedDate), expected_date);
  EXPECT_EQ(prefs.GetTime(prefs::kVariationsSafeSeedFetchTime),
            expected_fetch_time);

  // Verify metrics.
  histogram_tester.ExpectUniqueSample(
      "Variations.SafeMode.StoreSafeSeed.Result", StoreSeedResult::kSuccess, 1);
  histogram_tester.ExpectUniqueSample(
      "Variations.SafeMode.StoreSafeSeed.SignatureValidity",
      VerifySignatureResult::VALID_SIGNATURE, 1);
}

TEST(VariationsSeedStoreTest, StoreSafeSeed_IdenticalToLatestSeed) {
  const VariationsSeed seed = CreateTestSeed();
  const std::string serialized_seed = SerializeSeed(seed);
  const std::string base64_seed = SerializeSeedBase64(seed);
  const std::string signature = "a completely ignored signature";
  auto unused_client_state = CreateDummyClientFilterableState();
  const int unused_seed_milestone = 92;
  const base::Time safe_seed_fetch_time = WrapTime(12345);

  TestingPrefServiceSimple prefs;
  VariationsSeedStore::RegisterPrefs(prefs.registry());
  prefs.SetString(prefs::kVariationsCompressedSeed, base64_seed);
  const base::Time last_fetch_time = WrapTime(99999);
  prefs.SetTime(prefs::kVariationsLastFetchTime, last_fetch_time);

  TestVariationsSeedStore seed_store(&prefs);
  base::HistogramTester histogram_tester;
  EXPECT_TRUE(seed_store.StoreSafeSeed(
      serialized_seed, signature, unused_seed_milestone, *unused_client_state,
      safe_seed_fetch_time));

  // Verify the latest seed value was migrated to a sentinel value, rather than
  // the full string.
  EXPECT_EQ(kIdenticalToSafeSeedSentinel,
            prefs.GetString(prefs::kVariationsCompressedSeed));

  // Verify that loading the stored seed returns the original seed value.
  VariationsSeed loaded_seed;
  std::string loaded_seed_data;
  std::string unused_loaded_base64_seed_signature;
  EXPECT_TRUE(seed_store.LoadSeed(&loaded_seed, &loaded_seed_data,
                                  &unused_loaded_base64_seed_signature));

  EXPECT_EQ(SerializeSeed(seed), SerializeSeed(loaded_seed));
  EXPECT_EQ(SerializeSeed(seed), loaded_seed_data);

  // Verify that the safe seed prefs indeed contain the serialized seed value
  // and that the last fetch time was copied from the latest seed.
  EXPECT_EQ(base64_seed, prefs.GetString(prefs::kVariationsSafeCompressedSeed));
  VariationsSeed loaded_safe_seed;
  EXPECT_TRUE(
      seed_store.LoadSafeSeed(&loaded_safe_seed, unused_client_state.get()));
  EXPECT_EQ(SerializeSeed(seed), SerializeSeed(loaded_safe_seed));
  EXPECT_EQ(last_fetch_time, seed_store.GetSafeSeedFetchTime());

  // Verify metrics.
  histogram_tester.ExpectUniqueSample(
      "Variations.SafeMode.StoreSafeSeed.Result", StoreSeedResult::kSuccess, 1);
}

TEST(VariationsSeedStoreTest, StoreSafeSeed_PreviouslyIdenticalToLatestSeed) {
  // Create two distinct seeds: an old one saved as both the safe and the latest
  // seed value, and a new one that should overwrite only the stored safe seed
  // value.
  const VariationsSeed old_seed = CreateTestSeed();
  VariationsSeed new_seed = CreateTestSeed();
  new_seed.set_serial_number("12345678");
  ASSERT_NE(SerializeSeed(old_seed), SerializeSeed(new_seed));

  const std::string serialized_old_seed = SerializeSeed(old_seed);
  const std::string base64_old_seed = SerializeSeedBase64(old_seed);
  const std::string serialized_new_seed = SerializeSeed(new_seed);
  const std::string base64_new_seed = SerializeSeedBase64(new_seed);
  const std::string signature = "a completely ignored signature";
  const base::Time fetch_time = WrapTime(12345);
  auto unused_client_state = CreateDummyClientFilterableState();
  const int unused_seed_milestone = 92;

  TestingPrefServiceSimple prefs;
  VariationsSeedStore::RegisterPrefs(prefs.registry());
  prefs.SetString(prefs::kVariationsSafeCompressedSeed, base64_old_seed);
  prefs.SetString(prefs::kVariationsCompressedSeed,
                  kIdenticalToSafeSeedSentinel);

  TestVariationsSeedStore seed_store(&prefs);
  base::HistogramTester histogram_tester;
  EXPECT_TRUE(seed_store.StoreSafeSeed(serialized_new_seed, signature,
                                       unused_seed_milestone,
                                       *unused_client_state, fetch_time));

  // Verify the latest seed value was copied before the safe seed was
  // overwritten.
  EXPECT_EQ(base64_old_seed, prefs.GetString(prefs::kVariationsCompressedSeed));

  // Verify that loading the stored seed returns the old seed value.
  VariationsSeed loaded_seed;
  std::string loaded_seed_data;
  std::string unused_loaded_base64_seed_signature;
  EXPECT_TRUE(seed_store.LoadSeed(&loaded_seed, &loaded_seed_data,
                                  &unused_loaded_base64_seed_signature));

  EXPECT_EQ(SerializeSeed(old_seed), SerializeSeed(loaded_seed));
  EXPECT_EQ(SerializeSeed(old_seed), loaded_seed_data);

  // Verify that the safe seed prefs indeed contain the new seed's serialized
  // value.
  EXPECT_EQ(base64_new_seed,
            prefs.GetString(prefs::kVariationsSafeCompressedSeed));
  VariationsSeed loaded_safe_seed;
  EXPECT_TRUE(
      seed_store.LoadSafeSeed(&loaded_safe_seed, unused_client_state.get()));
  EXPECT_EQ(SerializeSeed(new_seed), SerializeSeed(loaded_safe_seed));
  EXPECT_EQ(fetch_time, seed_store.GetSafeSeedFetchTime());

  // Verify metrics.
  histogram_tester.ExpectUniqueSample(
      "Variations.SafeMode.StoreSafeSeed.Result", StoreSeedResult::kSuccess, 1);
}

TEST(VariationsSeedStoreTest, VerifySeedSignature) {
  // A valid seed and signature pair generated using the server's private key.
  const std::string uncompressed_base64_seed_data =
      kTestSeedData.base64_uncompressed_data;
  const std::string base64_seed_signature = kTestSeedData.base64_signature;

  std::string base64_seed_data;
  {
    std::string seed_data;
    ASSERT_TRUE(base::Base64Decode(uncompressed_base64_seed_data, &seed_data));
    VariationsSeed seed;
    ASSERT_TRUE(seed.ParseFromString(seed_data));
    base64_seed_data = SerializeSeedBase64(seed);
  }

  TestingPrefServiceSimple prefs;
  VariationsSeedStore::RegisterPrefs(prefs.registry());

  // The above inputs should be valid.
  {
    prefs.SetString(prefs::kVariationsCompressedSeed, base64_seed_data);
    prefs.SetString(prefs::kVariationsSeedSignature, base64_seed_signature);
    SignatureVerifyingVariationsSeedStore seed_store(&prefs);

    base::HistogramTester histogram_tester;
    VariationsSeed seed;
    std::string seed_data;
    std::string seed_signature;
    EXPECT_TRUE(seed_store.LoadSeed(&seed, &seed_data, &seed_signature));
    histogram_tester.ExpectUniqueSample(
        "Variations.LoadSeedSignature",
        static_cast<base::HistogramBase::Sample>(
            VerifySignatureResult::VALID_SIGNATURE),
        1);
  }

  // If there's no signature, the corresponding result should be returned.
  {
    prefs.SetString(prefs::kVariationsCompressedSeed, base64_seed_data);
    prefs.SetString(prefs::kVariationsSeedSignature, std::string());
    SignatureVerifyingVariationsSeedStore seed_store(&prefs);

    base::HistogramTester histogram_tester;
    VariationsSeed seed;
    std::string seed_data;
    std::string seed_signature;
    EXPECT_FALSE(seed_store.LoadSeed(&seed, &seed_data, &seed_signature));
    histogram_tester.ExpectUniqueSample(
        "Variations.LoadSeedSignature",
        static_cast<base::HistogramBase::Sample>(
            VerifySignatureResult::MISSING_SIGNATURE),
        1);
  }

  // Using non-base64 encoded value as signature should fail.
  {
    prefs.SetString(prefs::kVariationsCompressedSeed, base64_seed_data);
    prefs.SetString(prefs::kVariationsSeedSignature,
                    "not a base64-encoded string");
    SignatureVerifyingVariationsSeedStore seed_store(&prefs);

    base::HistogramTester histogram_tester;
    VariationsSeed seed;
    std::string seed_data;
    std::string seed_signature;
    EXPECT_FALSE(seed_store.LoadSeed(&seed, &seed_data, &seed_signature));
    histogram_tester.ExpectUniqueSample(
        "Variations.LoadSeedSignature",
        static_cast<base::HistogramBase::Sample>(
            VerifySignatureResult::DECODE_FAILED),
        1);
  }

  // Using a different signature (e.g. the base64 seed data) should fail.
  // OpenSSL doesn't distinguish signature decode failure from the
  // signature not matching.
  {
    prefs.SetString(prefs::kVariationsCompressedSeed, base64_seed_data);
    prefs.SetString(prefs::kVariationsSeedSignature, base64_seed_data);
    SignatureVerifyingVariationsSeedStore seed_store(&prefs);

    base::HistogramTester histogram_tester;
    VariationsSeed seed;
    std::string seed_data;
    std::string seed_signature;
    EXPECT_FALSE(seed_store.LoadSeed(&seed, &seed_data, &seed_signature));
    histogram_tester.ExpectUniqueSample(
        "Variations.LoadSeedSignature",
        static_cast<base::HistogramBase::Sample>(
            VerifySignatureResult::INVALID_SEED),
        1);
  }

  // Using a different seed should not match the signature.
  {
    std::string seed_data;
    ASSERT_TRUE(base::Base64Decode(uncompressed_base64_seed_data, &seed_data));
    VariationsSeed wrong_seed;
    ASSERT_TRUE(wrong_seed.ParseFromString(seed_data));
    (*wrong_seed.mutable_study(0)->mutable_name())[0] = 'x';
    std::string base64_wrong_seed_data = SerializeSeedBase64(wrong_seed);

    prefs.SetString(prefs::kVariationsCompressedSeed, base64_wrong_seed_data);
    prefs.SetString(prefs::kVariationsSeedSignature, base64_seed_signature);
    SignatureVerifyingVariationsSeedStore seed_store(&prefs);

    base::HistogramTester histogram_tester;
    VariationsSeed seed;
    std::string seed_signature;
    EXPECT_FALSE(seed_store.LoadSeed(&seed, &seed_data, &seed_signature));
    histogram_tester.ExpectUniqueSample(
        "Variations.LoadSeedSignature",
        static_cast<base::HistogramBase::Sample>(
            VerifySignatureResult::INVALID_SEED),
        1);
  }
}

TEST(VariationsSeedStoreTest, LastFetchTime_DistinctSeeds) {
  TestingPrefServiceSimple prefs;
  VariationsSeedStore::RegisterPrefs(prefs.registry());
  prefs.SetString(prefs::kVariationsCompressedSeed, "one");
  prefs.SetString(prefs::kVariationsSafeCompressedSeed, "not one");
  prefs.SetTime(prefs::kVariationsLastFetchTime, WrapTime(1));
  prefs.SetTime(prefs::kVariationsSafeSeedFetchTime, WrapTime(0));

  base::Time start_time = WrapTime(10);
  TestVariationsSeedStore seed_store(&prefs);
  seed_store.RecordLastFetchTime(WrapTime(11));

  // Verify that the last fetch time was updated.
  const base::Time last_fetch_time =
      prefs.GetTime(prefs::kVariationsLastFetchTime);
  EXPECT_EQ(WrapTime(11), last_fetch_time);
  EXPECT_GE(last_fetch_time, start_time);

  // Verify that the safe seed's fetch time was *not* updated.
  EXPECT_EQ(WrapTime(0), prefs.GetTime(prefs::kVariationsSafeSeedFetchTime));
}

TEST(VariationsSeedStoreTest, LastFetchTime_IdenticalSeeds) {
  TestingPrefServiceSimple prefs;
  VariationsSeedStore::RegisterPrefs(prefs.registry());
  prefs.SetString(prefs::kVariationsSafeCompressedSeed, "some seed");
  prefs.SetString(prefs::kVariationsCompressedSeed,
                  kIdenticalToSafeSeedSentinel);
  prefs.SetTime(prefs::kVariationsLastFetchTime, WrapTime(1));
  prefs.SetTime(prefs::kVariationsSafeSeedFetchTime, WrapTime(0));

  base::Time start_time = WrapTime(10);
  TestVariationsSeedStore seed_store(&prefs);
  seed_store.RecordLastFetchTime(WrapTime(11));

  // Verify that the last fetch time was updated.
  const base::Time last_fetch_time =
      prefs.GetTime(prefs::kVariationsLastFetchTime);
  EXPECT_EQ(WrapTime(11), last_fetch_time);
  EXPECT_GE(last_fetch_time, start_time);

  // Verify that the safe seed's fetch time *was* also updated.
  EXPECT_EQ(last_fetch_time,
            prefs.GetTime(prefs::kVariationsSafeSeedFetchTime));
}

TEST(VariationsSeedStoreTest, GetLatestSerialNumber_LoadsInitialValue) {
  // Store good seed data to test if loading from prefs works.
  const VariationsSeed seed = CreateTestSeed();
  const std::string base64_seed = SerializeSeedBase64(seed);
  const std::string base64_seed_signature = "a completely ignored signature";

  TestingPrefServiceSimple prefs;
  VariationsSeedStore::RegisterPrefs(prefs.registry());
  prefs.SetString(prefs::kVariationsCompressedSeed, base64_seed);
  prefs.SetString(prefs::kVariationsSeedSignature, base64_seed_signature);

  TestVariationsSeedStore seed_store(&prefs);
  EXPECT_EQ("123", seed_store.GetLatestSerialNumber());
}

TEST(VariationsSeedStoreTest, GetLatestSerialNumber_EmptyWhenNoSeedIsSaved) {
  // Start with empty prefs.
  TestingPrefServiceSimple prefs;
  VariationsSeedStore::RegisterPrefs(prefs.registry());

  TestVariationsSeedStore seed_store(&prefs);
  EXPECT_EQ(std::string(), seed_store.GetLatestSerialNumber());
}

TEST(VariationsSeedStoreTest, GetLatestSerialNumber_ClearsPrefsOnFailure) {
  // Store corrupted seed data to test that prefs are cleared when loading
  // fails.
  TestingPrefServiceSimple prefs;
  VariationsSeedStore::RegisterPrefs(prefs.registry());
  prefs.SetString(prefs::kVariationsCompressedSeed, "complete garbage");
  prefs.SetString(prefs::kVariationsSeedSignature, "an unused signature");

  TestVariationsSeedStore seed_store(&prefs);
  EXPECT_EQ(std::string(), seed_store.GetLatestSerialNumber());
  EXPECT_TRUE(PrefHasDefaultValue(prefs, prefs::kVariationsCompressedSeed));
}

// Verifies that GetTimeForStudyDateChecks() returns the server timestamp for
// when the regular seed was fetched,|kVariationsSeedDate|, when the time is
// more recent than the build time.
TEST(VariationsSeedStoreTest, RegularSeedTimeReturned) {
  TestingPrefServiceSimple prefs;
  VariationsSeedStore::RegisterPrefs(prefs.registry());
  const base::Time seed_fetch_time = base::GetBuildTime() + base::Days(4);
  prefs.SetTime(prefs::kVariationsSeedDate, seed_fetch_time);

  TestVariationsSeedStore seed_store(&prefs);
  EXPECT_EQ(seed_store.GetTimeForStudyDateChecks(/*is_safe_seed=*/false),
            seed_fetch_time);
}

// Verifies that GetTimeForStudyDateChecks() returns the server timestamp for
// when the safe seed was fetched, |kVariationsSafeSeedDate|, when the time is
// more recent than the build time.
TEST(VariationsSeedStoreTest, SafeSeedTimeReturned) {
  TestingPrefServiceSimple prefs;
  VariationsSeedStore::RegisterPrefs(prefs.registry());
  const base::Time safe_seed_fetch_time = base::GetBuildTime() + base::Days(7);
  prefs.SetTime(prefs::kVariationsSafeSeedDate, safe_seed_fetch_time);

  TestVariationsSeedStore seed_store(&prefs);
  EXPECT_EQ(seed_store.GetTimeForStudyDateChecks(/*is_safe_seed=*/true),
            safe_seed_fetch_time);
}

// Verifies that GetTimeForStudyDateChecks() returns the build time when it is
// more recent than |kVariationsSeedDate|.
TEST(VariationsSeedStoreTest, BuildTimeReturnedForRegularSeed) {
  TestingPrefServiceSimple prefs;
  VariationsSeedStore::RegisterPrefs(prefs.registry());
  const base::Time seed_fetch_time = base::GetBuildTime() - base::Days(2);
  prefs.SetTime(prefs::kVariationsSeedDate, seed_fetch_time);

  TestVariationsSeedStore seed_store(&prefs);
  EXPECT_EQ(seed_store.GetTimeForStudyDateChecks(/*is_safe_seed=*/false),
            base::GetBuildTime());
}

// Verifies that GetTimeForStudyDateChecks() returns the build time when it is
// more recent than |kVariationsSafeSeedDate|.
TEST(VariationsSeedStoreTest, BuildTimeReturnedForSafeSeed) {
  TestingPrefServiceSimple prefs;
  VariationsSeedStore::RegisterPrefs(prefs.registry());
  const base::Time safe_seed_fetch_time = base::GetBuildTime() - base::Days(3);
  prefs.SetTime(prefs::kVariationsSeedDate, safe_seed_fetch_time);

  TestVariationsSeedStore seed_store(&prefs);
  EXPECT_EQ(seed_store.GetTimeForStudyDateChecks(/*is_safe_seed=*/true),
            base::GetBuildTime());
}

// Verifies that GetTimeForStudyDateChecks() returns the build time when the
// seed time is null.
TEST(VariationsSeedStoreTest, BuildTimeReturnedForNullSeedTimes) {
  TestingPrefServiceSimple prefs;
  VariationsSeedStore::RegisterPrefs(prefs.registry());
  ASSERT_TRUE(prefs.GetTime(prefs::kVariationsSeedDate).is_null());

  TestVariationsSeedStore seed_store(&prefs);
  EXPECT_EQ(seed_store.GetTimeForStudyDateChecks(/*is_safe_seed=*/false),
            base::GetBuildTime());

  ASSERT_TRUE(prefs.GetTime(prefs::kVariationsSafeSeedDate).is_null());
  EXPECT_EQ(seed_store.GetTimeForStudyDateChecks(/*is_safe_seed=*/true),
            base::GetBuildTime());
}

#if BUILDFLAG(IS_ANDROID)
TEST(VariationsSeedStoreTest, ImportFirstRunJavaSeed) {
  const std::string test_seed_data = "raw_seed_data_test";
  const std::string test_seed_signature = "seed_signature_test";
  const std::string test_seed_country = "seed_country_code_test";
  const int64_t test_response_date = 1234567890;
  const bool test_is_gzip_compressed = true;
  android::SetJavaFirstRunPrefsForTesting(test_seed_data, test_seed_signature,
                                          test_seed_country, test_response_date,
                                          test_is_gzip_compressed);

  auto seed = android::GetVariationsFirstRunSeed();
  EXPECT_EQ(test_seed_data, seed->data);
  EXPECT_EQ(test_seed_signature, seed->signature);
  EXPECT_EQ(test_seed_country, seed->country);
  EXPECT_EQ(test_response_date, seed->date.InMillisecondsSinceUnixEpoch());
  EXPECT_EQ(test_is_gzip_compressed, seed->is_gzip_compressed);

  android::ClearJavaFirstRunPrefs();
  seed = android::GetVariationsFirstRunSeed();
  EXPECT_EQ("", seed->data);
  EXPECT_EQ("", seed->signature);
  EXPECT_EQ("", seed->country);
  EXPECT_EQ(0, seed->date.InMillisecondsSinceUnixEpoch());
  EXPECT_FALSE(seed->is_gzip_compressed);
}

class VariationsSeedStoreFirstRunPrefsTest
    : public ::testing::TestWithParam<bool> {};

INSTANTIATE_TEST_SUITE_P(VariationsSeedStoreTest,
                         VariationsSeedStoreFirstRunPrefsTest,
                         ::testing::Bool());

TEST_P(VariationsSeedStoreFirstRunPrefsTest, FirstRunPrefsAllowed) {
  bool use_first_run_prefs = GetParam();

  const std::string test_seed_data = "raw_seed_data_test";
  const std::string test_seed_signature = "seed_signature_test";
  const std::string test_seed_country = "seed_country_code_test";
  const int64_t test_response_date = 1234567890;
  const bool test_is_gzip_compressed = true;
  android::SetJavaFirstRunPrefsForTesting(test_seed_data, test_seed_signature,
                                          test_seed_country, test_response_date,
                                          test_is_gzip_compressed);

  const VariationsSeed test_seed = CreateTestSeed();
  const std::string seed_data = SerializeSeed(test_seed);
  const std::string base64_seed_data = SerializeSeedBase64(test_seed);
  auto seed = std::make_unique<SeedResponse>();
  seed->data = seed_data;
  seed->signature = "java_seed_signature";
  seed->country = "java_seed_country";
  seed->date = base::Time::FromMillisecondsSinceUnixEpoch(test_response_date) +
               base::Days(1);
  seed->is_gzip_compressed = false;

  TestingPrefServiceSimple prefs;
  VariationsSeedStore::RegisterPrefs(prefs.registry());
  TestVariationsSeedStore seed_store(&prefs, /*initial_seed=*/std::move(seed),
                                     use_first_run_prefs);

  seed = android::GetVariationsFirstRunSeed();

  // VariationsSeedStore must not modify Java prefs at all.
  EXPECT_EQ(test_seed_data, seed->data);
  EXPECT_EQ(test_seed_signature, seed->signature);
  EXPECT_EQ(test_seed_country, seed->country);
  EXPECT_EQ(test_response_date, seed->date.InMillisecondsSinceUnixEpoch());
  EXPECT_EQ(test_is_gzip_compressed, seed->is_gzip_compressed);
  if (use_first_run_prefs) {
    EXPECT_TRUE(android::HasMarkedPrefsForTesting());
  } else {
    EXPECT_FALSE(android::HasMarkedPrefsForTesting());
  }

  // Seed should be stored in prefs.
  EXPECT_FALSE(PrefHasDefaultValue(prefs, prefs::kVariationsCompressedSeed));
  EXPECT_EQ(base64_seed_data,
            prefs.GetString(prefs::kVariationsCompressedSeed));
}
#endif  // BUILDFLAG(IS_ANDROID)

#if BUILDFLAG(IS_CHROMEOS_ASH)
const featured::SeedDetails CreateDummySafeSeed(
    ClientFilterableState* client_state,
    base::Time fetch_time_to_store) {
  featured::SeedDetails expected_seed;
  expected_seed.set_b64_compressed_data(kTestSeedData.base64_compressed_data);
  expected_seed.set_signature(kTestSeedData.base64_signature);
  expected_seed.set_milestone(92);
  expected_seed.set_locale(client_state->locale);
  expected_seed.set_date(
      client_state->reference_date.ToDeltaSinceWindowsEpoch().InMilliseconds());
  expected_seed.set_permanent_consistency_country(
      client_state->permanent_consistency_country);
  expected_seed.set_session_consistency_country(
      client_state->session_consistency_country);
  expected_seed.set_fetch_time(
      fetch_time_to_store.ToDeltaSinceWindowsEpoch().InMilliseconds());
  return expected_seed;
}

// Checks that |platform_data| and |expected_data| deserialize to the same
// VariationsSeed proto.
// |platform_data| and |expected_data| are base64_compressed forms of seed data.
void ExpectSeedData(const std::string& platform_data,
                    const std::string& expected_data) {
  std::string decoded_platform_data;
  EXPECT_TRUE(base::Base64Decode(platform_data, &decoded_platform_data));
  std::string uncompressed_decoded_platform_data;
  EXPECT_TRUE(compression::GzipUncompress(decoded_platform_data,
                                          &uncompressed_decoded_platform_data));
  VariationsSeed platform_seed;
  EXPECT_TRUE(
      platform_seed.ParseFromString(uncompressed_decoded_platform_data));

  std::string decoded_expected_data;
  EXPECT_TRUE(base::Base64Decode(expected_data, &decoded_expected_data));
  std::string uncompressed_decoded_expected_data;
  EXPECT_TRUE(compression::GzipUncompress(decoded_expected_data,
                                          &uncompressed_decoded_expected_data));
  VariationsSeed expected_seed;
  EXPECT_TRUE(
      expected_seed.ParseFromString(uncompressed_decoded_expected_data));

  EXPECT_THAT(platform_seed, EqualsProto(expected_seed));
}

// Manually verifying each field in featured::SeedDetails rather than using
// EqualsProto is necessary because the
// featured::SeedDetails::b64_compressed_data field may be different between
// |platform| and |expected| even if the data unserializes to the same
// VariationsSeed. This could be caused by implementation differences between
// different versions of compression::GzipCompress.
//
// To accurately compare two featured::SeedDetails protos, the
// `b64_compressed_data` should be deserialized into a VariationsSeed proto and
// the two VariationsSeed protos should be compared.
void ExpectSafeSeed(const featured::SeedDetails& platform,
                    const featured::SeedDetails expected) {
  ExpectSeedData(platform.b64_compressed_data(),
                 expected.b64_compressed_data());
  EXPECT_EQ(platform.locale(), expected.locale());
  EXPECT_EQ(platform.milestone(), expected.milestone());
  EXPECT_EQ(platform.permanent_consistency_country(),
            expected.permanent_consistency_country());
  EXPECT_EQ(platform.session_consistency_country(),
            expected.session_consistency_country());
  EXPECT_EQ(platform.signature(), expected.signature());
  EXPECT_EQ(platform.date(), expected.date());
  EXPECT_EQ(platform.fetch_time(), expected.fetch_time());
}

TEST(VariationsSeedStoreTest, SendSafeSeedToPlatform_SucceedFirstAttempt) {
  TestingPrefServiceSimple prefs;
  VariationsSeedStore::RegisterPrefs(prefs.registry());
  SignatureVerifyingVariationsSeedStore seed_store(&prefs);

  ash::featured::FeaturedClient::InitializeFake();
  ash::featured::FakeFeaturedClient* client =
      ash::featured::FakeFeaturedClient::Get();
  client->AddResponse(true);

  std::unique_ptr<ClientFilterableState> client_state =
      CreateDummyClientFilterableState();
  base::Time now = base::Time::Now();
  const base::Time fetch_time_to_store = now - base::Hours(1);
  featured::SeedDetails expected_platform_seed =
      CreateDummySafeSeed(client_state.get(), fetch_time_to_store);
  std::string expected_seed_data;
  ASSERT_TRUE(base::Base64Decode(kTestSeedData.base64_uncompressed_data,
                                 &expected_seed_data));

  // Verify that storing the safe seed succeeded.
  EXPECT_TRUE(seed_store.StoreSafeSeed(
      expected_seed_data, expected_platform_seed.signature(),
      expected_platform_seed.milestone(), *client_state, fetch_time_to_store));

  // Verify that the validated safe seed was received on Platform.
  ExpectSafeSeed(client->latest_safe_seed(), expected_platform_seed);
  EXPECT_EQ(client->handle_seed_fetched_attempts(), 1);

  ash::featured::FeaturedClient::Shutdown();
}

TEST(VariationsSeedStoreTest, SendSafeSeedToPlatform_FailFirstAttempt) {
  TestingPrefServiceSimple prefs;
  VariationsSeedStore::RegisterPrefs(prefs.registry());
  SignatureVerifyingVariationsSeedStore seed_store(&prefs);

  ash::featured::FeaturedClient::InitializeFake();
  ash::featured::FakeFeaturedClient* client =
      ash::featured::FakeFeaturedClient::Get();
  client->AddResponse(false);
  client->AddResponse(true);

  std::unique_ptr<ClientFilterableState> client_state =
      CreateDummyClientFilterableState();
  base::Time now = base::Time::Now();
  const base::Time fetch_time_to_store = now - base::Hours(1);
  featured::SeedDetails expected_platform_seed =
      CreateDummySafeSeed(client_state.get(), fetch_time_to_store);
  std::string expected_seed_data;
  ASSERT_TRUE(base::Base64Decode(kTestSeedData.base64_uncompressed_data,
                                 &expected_seed_data));

  // Verify that storing the safe seed succeeded.
  EXPECT_TRUE(seed_store.StoreSafeSeed(
      expected_seed_data, expected_platform_seed.signature(),
      expected_platform_seed.milestone(), *client_state, fetch_time_to_store));

  // Verify that the validated safe seed was received on Platform.
  ExpectSafeSeed(client->latest_safe_seed(), expected_platform_seed);
  EXPECT_EQ(client->handle_seed_fetched_attempts(), 2);

  ash::featured::FeaturedClient::Shutdown();
}

TEST(VariationsSeedStoreTest, SendSafeSeedToPlatform_FailTwoAttempts) {
  TestingPrefServiceSimple prefs;
  VariationsSeedStore::RegisterPrefs(prefs.registry());
  SignatureVerifyingVariationsSeedStore seed_store(&prefs);

  ash::featured::FeaturedClient::InitializeFake();
  ash::featured::FakeFeaturedClient* client =
      ash::featured::FakeFeaturedClient::Get();
  client->AddResponse(false);
  client->AddResponse(false);

  std::unique_ptr<ClientFilterableState> client_state =
      CreateDummyClientFilterableState();
  base::Time now = base::Time::Now();
  const base::Time fetch_time_to_store = now - base::Hours(1);
  featured::SeedDetails seed =
      CreateDummySafeSeed(client_state.get(), fetch_time_to_store);
  std::string seed_data;
  ASSERT_TRUE(
      base::Base64Decode(kTestSeedData.base64_uncompressed_data, &seed_data));

  // Verify that storing the safe seed succeeded.
  EXPECT_TRUE(seed_store.StoreSafeSeed(seed_data, seed.signature(),
                                       seed.milestone(), *client_state,
                                       fetch_time_to_store));

  // Verify that the validated safe seed was not received on Platform.
  featured::SeedDetails empty_seed;
  EXPECT_THAT(client->latest_safe_seed(), EqualsProto(empty_seed));
  EXPECT_EQ(client->handle_seed_fetched_attempts(), 2);

  ash::featured::FeaturedClient::Shutdown();
}
#endif  // BUILDFLAG(IS_CHROMEOS_ASH)

}  // namespace variations
