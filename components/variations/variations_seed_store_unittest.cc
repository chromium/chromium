// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/variations/variations_seed_store.h"

#include <memory>
#include <utility>

#include "base/base64.h"
#include "base/macros.h"
#include "base/test/metrics/histogram_tester.h"
#include "base/time/time.h"
#include "base/version.h"
#include "build/build_config.h"
#include "components/prefs/testing_pref_service.h"
#include "components/variations/client_filterable_state.h"
#include "components/variations/pref_names.h"
#include "components/variations/proto/study.pb.h"
#include "components/variations/proto/variations_seed.pb.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/zlib/google/compression_utils.h"

#if defined(OS_ANDROID)
#include "components/variations/android/variations_seed_bridge.h"
#endif  // OS_ANDROID

namespace variations {
namespace {

// The below seed and signature pair were generated using the server's private
// key.
const char kUncompressedBase64SeedData[] =
    "CigxZDI5NDY0ZmIzZDc4ZmYxNTU2ZTViNTUxYzY0NDdjYmM3NGU1ZmQwEr0BCh9VTUEtVW5p"
    "Zm9ybWl0eS1UcmlhbC0xMC1QZXJjZW50GICckqUFOAFCB2RlZmF1bHRKCwoHZGVmYXVsdBAB"
    "SgwKCGdyb3VwXzAxEAFKDAoIZ3JvdXBfMDIQAUoMCghncm91cF8wMxABSgwKCGdyb3VwXzA0"
    "EAFKDAoIZ3JvdXBfMDUQAUoMCghncm91cF8wNhABSgwKCGdyb3VwXzA3EAFKDAoIZ3JvdXBf"
    "MDgQAUoMCghncm91cF8wORAB";
const char kBase64SeedSignature[] =
    "MEQCIDD1IVxjzWYncun+9IGzqYjZvqxxujQEayJULTlbTGA/AiAr0oVmEgVUQZBYq5VLOSvy"
    "96JkMYgzTkHPwbv7K/CmgA==";

// The sentinel value that may be stored as the latest variations seed value in
// prefs to indicate that the latest seed is identical to the safe seed.
// Note: This constant is intentionally duplicated in the test because it is
// persisted to disk. In order to maintain backward-compatibility, it's
// important that code continue to correctly handle this specific constant, even
// if the constant used internally in the implementation changes.
constexpr char kIdenticalToSafeSeedSentinel[] = "safe_seed_content";

class TestVariationsSeedStore : public VariationsSeedStore {
 public:
  explicit TestVariationsSeedStore(PrefService* local_state)
      : VariationsSeedStore(local_state) {}
  ~TestVariationsSeedStore() override {}

  bool StoreSeedForTesting(const std::string& seed_data) {
    return StoreSeedData(seed_data, std::string(), std::string(),
                         base::Time::Now(), false, false, false, nullptr);
  }

  bool SignatureVerificationEnabled() override { return false; }

 private:
  DISALLOW_COPY_AND_ASSIGN(TestVariationsSeedStore);
};

// Signature verification is disabled on Android and iOS for performance
// reasons. This class re-enables it for tests, which don't mind the (small)
// performance penalty.
class SignatureVerifyingVariationsSeedStore : public VariationsSeedStore {
 public:
  explicit SignatureVerifyingVariationsSeedStore(PrefService* local_state)
      : VariationsSeedStore(local_state) {}
  ~SignatureVerifyingVariationsSeedStore() override {}

  bool SignatureVerificationEnabled() override { return true; }

 private:
  DISALLOW_COPY_AND_ASSIGN(SignatureVerifyingVariationsSeedStore);
};

// Creates a base::Time object from the corresponding raw value. The specific
// implementation is not important; it's only important that distinct inputs map
// to distinct outputs.
base::Time WrapTime(int64_t time) {
  return base::Time::FromDeltaSinceWindowsEpoch(
      base::TimeDelta::FromMicroseconds(time));
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
      std::make_unique<ClientFilterableState>(base::OnceCallback<bool()>());
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
std::string Compress(const std::string& data) {
  std::string compressed;
  const bool result = compression::GzipCompress(data, &compressed);
  EXPECT_TRUE(result);
  return compressed;
}

// Serializes |seed| to compressed base64-encoded protobuf binary format.
std::string SerializeSeedBase64(const VariationsSeed& seed) {
  std::string serialized_seed = SerializeSeed(seed);
  std::string base64_serialized_seed;
  base::Base64Encode(Compress(serialized_seed), &base64_serialized_seed);
  return base64_serialized_seed;
}

// Sets all seed-related prefs to non-default values. Used to verify whether
// pref values were cleared.
void SetAllSeedPrefsToNonDefaultValues(PrefService* prefs) {
  prefs->SetString(prefs::kVariationsCompressedSeed, "a");
  prefs->SetString(prefs::kVariationsSafeCompressedSeed, "b");
  prefs->SetString(prefs::kVariationsSafeSeedLocale, "c");
  prefs->SetString(prefs::kVariationsSafeSeedPermanentConsistencyCountry, "d");
  prefs->SetString(prefs::kVariationsSafeSeedSessionConsistencyCountry, "e");
  prefs->SetString(prefs::kVariationsSafeSeedSignature, "f");
  prefs->SetString(prefs::kVariationsSeedSignature, "g");
  const base::Time now = base::Time::Now();
  const base::TimeDelta delta = base::TimeDelta::FromDays(1);
  prefs->SetTime(prefs::kVariationsSafeSeedDate, now - delta);
  prefs->SetTime(prefs::kVariationsSafeSeedFetchTime, now - delta * 2);
  prefs->SetTime(prefs::kVariationsSeedDate, now - delta * 3);
}

// Checks whether the pref with name |pref_name| is at its default value in
// |prefs|.
bool PrefHasDefaultValue(const TestingPrefServiceSimple& prefs,
                         const char* pref_name) {
  return prefs.FindPreference(pref_name)->IsDefaultValue();
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

  VariationsSeed loaded_seed;
  std::string loaded_seed_data;
  std::string loaded_base64_seed_signature;
  // Check that loading a seed works correctly.
  EXPECT_TRUE(seed_store.LoadSeed(&loaded_seed, &loaded_seed_data,
                                  &loaded_base64_seed_signature));

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

  // Loading an invalid seed should return false and clear all associated prefs.
  TestVariationsSeedStore seed_store(&prefs);
  VariationsSeed loaded_seed;
  std::string loaded_seed_data;
  std::string loaded_base64_seed_signature;
  EXPECT_FALSE(seed_store.LoadSeed(&loaded_seed, &loaded_seed_data,
                                   &loaded_base64_seed_signature));
  EXPECT_TRUE(PrefHasDefaultValue(prefs, prefs::kVariationsCompressedSeed));
  EXPECT_TRUE(PrefHasDefaultValue(prefs, prefs::kVariationsSeedDate));
  EXPECT_TRUE(PrefHasDefaultValue(prefs, prefs::kVariationsSeedSignature));

  // However, only the latest seed prefs should be cleared; the safe seed prefs
  // should not be modified.
  EXPECT_FALSE(
      PrefHasDefaultValue(prefs, prefs::kVariationsSafeCompressedSeed));
  EXPECT_FALSE(PrefHasDefaultValue(prefs, prefs::kVariationsSafeSeedDate));
  EXPECT_FALSE(PrefHasDefaultValue(prefs, prefs::kVariationsSafeSeedFetchTime));
  EXPECT_FALSE(PrefHasDefaultValue(prefs, prefs::kVariationsSafeSeedLocale));
  EXPECT_FALSE(PrefHasDefaultValue(
      prefs, prefs::kVariationsSafeSeedPermanentConsistencyCountry));
  EXPECT_FALSE(PrefHasDefaultValue(
      prefs, prefs::kVariationsSafeSeedSessionConsistencyCountry));
  EXPECT_FALSE(PrefHasDefaultValue(prefs, prefs::kVariationsSafeSeedSignature));
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
  VariationsSeed loaded_seed;
  std::string loaded_seed_data;
  std::string loaded_base64_seed_signature;
  EXPECT_FALSE(seed_store.LoadSeed(&loaded_seed, &loaded_seed_data,
                                   &loaded_base64_seed_signature));
  EXPECT_TRUE(PrefHasDefaultValue(prefs, prefs::kVariationsCompressedSeed));
  EXPECT_TRUE(PrefHasDefaultValue(prefs, prefs::kVariationsSeedDate));
  EXPECT_TRUE(PrefHasDefaultValue(prefs, prefs::kVariationsSeedSignature));

  // However, only the latest seed prefs should be cleared; the safe seed prefs
  // should not be modified.
  EXPECT_FALSE(
      PrefHasDefaultValue(prefs, prefs::kVariationsSafeCompressedSeed));
  EXPECT_FALSE(PrefHasDefaultValue(prefs, prefs::kVariationsSafeSeedDate));
  EXPECT_FALSE(PrefHasDefaultValue(prefs, prefs::kVariationsSafeSeedFetchTime));
  EXPECT_FALSE(PrefHasDefaultValue(prefs, prefs::kVariationsSafeSeedLocale));
  EXPECT_FALSE(PrefHasDefaultValue(
      prefs, prefs::kVariationsSafeSeedPermanentConsistencyCountry));
  EXPECT_FALSE(PrefHasDefaultValue(
      prefs, prefs::kVariationsSafeSeedSessionConsistencyCountry));
  EXPECT_FALSE(PrefHasDefaultValue(prefs, prefs::kVariationsSafeSeedSignature));
}

TEST(VariationsSeedStoreTest, LoadSeed_EmptySeed) {
  TestingPrefServiceSimple prefs;
  VariationsSeedStore::RegisterPrefs(prefs.registry());

  // Loading an empty seed should return false.
  TestVariationsSeedStore seed_store(&prefs);
  VariationsSeed loaded_seed;
  std::string loaded_seed_data;
  std::string loaded_base64_seed_signature;
  EXPECT_FALSE(seed_store.LoadSeed(&loaded_seed, &loaded_seed_data,
                                   &loaded_base64_seed_signature));
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

  VariationsSeed loaded_seed;
  std::string loaded_seed_data;
  std::string loaded_base64_seed_signature;
  // Check that loading the seed works correctly.
  EXPECT_TRUE(seed_store.LoadSeed(&loaded_seed, &loaded_seed_data,
                                  &loaded_base64_seed_signature));

  // Check that the loaded data is the same as the original.
  EXPECT_EQ(SerializeSeed(seed), SerializeSeed(loaded_seed));
  EXPECT_EQ(SerializeSeed(seed), loaded_seed_data);
  EXPECT_EQ(base64_seed_signature, loaded_base64_seed_signature);
}

TEST(VariationsSeedStoreTest, StoreSeedData) {
  const VariationsSeed seed = CreateTestSeed();
  const std::string serialized_seed = SerializeSeed(seed);

  TestingPrefServiceSimple prefs;
  VariationsSeedStore::RegisterPrefs(prefs.registry());

  TestVariationsSeedStore seed_store(&prefs);

  EXPECT_TRUE(seed_store.StoreSeedForTesting(serialized_seed));
  // Make sure the pref was actually set.
  EXPECT_FALSE(PrefHasDefaultValue(prefs, prefs::kVariationsCompressedSeed));

  std::string loaded_compressed_seed =
      prefs.GetString(prefs::kVariationsCompressedSeed);
  std::string decoded_compressed_seed;
  ASSERT_TRUE(base::Base64Decode(loaded_compressed_seed,
                                 &decoded_compressed_seed));
  // Make sure the stored seed from pref is the same as the seed we created.
  EXPECT_EQ(Compress(serialized_seed), decoded_compressed_seed);

  // Check if trying to store a bad seed leaves the pref unchanged.
  prefs.ClearPref(prefs::kVariationsCompressedSeed);
  EXPECT_FALSE(seed_store.StoreSeedForTesting("should fail"));
  EXPECT_TRUE(PrefHasDefaultValue(prefs, prefs::kVariationsCompressedSeed));
}

TEST(VariationsSeedStoreTest, StoreSeedData_ParsedSeed) {
  const VariationsSeed seed = CreateTestSeed();
  const std::string serialized_seed = SerializeSeed(seed);

  TestingPrefServiceSimple prefs;
  VariationsSeedStore::RegisterPrefs(prefs.registry());
  TestVariationsSeedStore seed_store(&prefs);

  VariationsSeed parsed_seed;
  EXPECT_TRUE(seed_store.StoreSeedData(serialized_seed, std::string(),
                                       std::string(), base::Time::Now(), false,
                                       false, false, &parsed_seed));
  EXPECT_EQ(serialized_seed, SerializeSeed(parsed_seed));
}

TEST(VariationsSeedStoreTest, StoreSeedData_CountryCode) {
  TestingPrefServiceSimple prefs;
  VariationsSeedStore::RegisterPrefs(prefs.registry());
  TestVariationsSeedStore seed_store(&prefs);

  // Test with a valid header value.
  std::string seed = SerializeSeed(CreateTestSeed());
  EXPECT_TRUE(seed_store.StoreSeedData(seed, std::string(), "test_country",
                                       base::Time::Now(), false, false, false,
                                       nullptr));
  EXPECT_EQ("test_country", prefs.GetString(prefs::kVariationsCountry));

  // Test with no country code specified - which should preserve the old value.
  EXPECT_TRUE(seed_store.StoreSeedData(seed, std::string(), std::string(),
                                       base::Time::Now(), false, false, false,
                                       nullptr));
  EXPECT_EQ("test_country", prefs.GetString(prefs::kVariationsCountry));
}

TEST(VariationsSeedStoreTest, StoreSeedData_GzippedSeed) {
  const VariationsSeed seed = CreateTestSeed();
  const std::string serialized_seed = SerializeSeed(seed);
  std::string compressed_seed;
  ASSERT_TRUE(compression::GzipCompress(serialized_seed, &compressed_seed));

  TestingPrefServiceSimple prefs;
  VariationsSeedStore::RegisterPrefs(prefs.registry());
  TestVariationsSeedStore seed_store(&prefs);

  VariationsSeed parsed_seed;
  EXPECT_TRUE(seed_store.StoreSeedData(compressed_seed, std::string(),
                                       std::string(), base::Time::Now(), false,
                                       true, false, &parsed_seed));
  EXPECT_EQ(serialized_seed, SerializeSeed(parsed_seed));
}

TEST(VariationsSeedStoreTest, StoreSeedData_IdenticalToSafeSeed) {
  const VariationsSeed seed = CreateTestSeed();
  const std::string serialized_seed = SerializeSeed(seed);
  const std::string base64_seed = SerializeSeedBase64(seed);

  TestingPrefServiceSimple prefs;
  VariationsSeedStore::RegisterPrefs(prefs.registry());
  prefs.SetString(prefs::kVariationsSafeCompressedSeed, base64_seed);

  TestVariationsSeedStore seed_store(&prefs);
  EXPECT_TRUE(seed_store.StoreSeedForTesting(serialized_seed));

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

TEST(VariationsSeedStoreTest, LoadSafeSeed_ValidSeed) {
  // Store good seed data to test if loading from prefs works.
  const VariationsSeed seed = CreateTestSeed();
  const std::string base64_seed = SerializeSeedBase64(seed);
  const std::string base64_seed_signature = "a test signature, ignored.";
  const base::Time reference_date = base::Time::Now();
  const base::Time fetch_time = reference_date - base::TimeDelta::FromDays(3);
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

  TestVariationsSeedStore seed_store(&prefs);
  VariationsSeed loaded_seed;
  std::unique_ptr<ClientFilterableState> client_state =
      CreateTestClientFilterableState();
  base::Time loaded_fetch_time;
  EXPECT_TRUE(seed_store.LoadSafeSeed(&loaded_seed, client_state.get(),
                                      &loaded_fetch_time));

  // Check that the loaded data is the same as the original.
  EXPECT_EQ(SerializeSeed(seed), SerializeSeed(loaded_seed));
  EXPECT_EQ(locale, client_state->locale);
  EXPECT_EQ(reference_date, client_state->reference_date);
  EXPECT_EQ(permanent_consistency_country,
            client_state->permanent_consistency_country);
  EXPECT_EQ(session_consistency_country,
            client_state->session_consistency_country);
  EXPECT_EQ(fetch_time, loaded_fetch_time);

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

TEST(VariationsSeedStoreTest, LoadSafeSeed_InvalidSeed) {
  TestingPrefServiceSimple prefs;
  VariationsSeedStore::RegisterPrefs(prefs.registry());
  SetAllSeedPrefsToNonDefaultValues(&prefs);
  prefs.SetString(prefs::kVariationsSafeCompressedSeed, "this should fail");

  // Loading an invalid seed should return false and clear all associated prefs.
  TestVariationsSeedStore seed_store(&prefs);
  VariationsSeed loaded_seed;
  std::unique_ptr<ClientFilterableState> client_state =
      CreateTestClientFilterableState();
  base::Time fetch_time;
  EXPECT_FALSE(
      seed_store.LoadSafeSeed(&loaded_seed, client_state.get(), &fetch_time));
  EXPECT_TRUE(PrefHasDefaultValue(prefs, prefs::kVariationsSafeCompressedSeed));
  EXPECT_TRUE(PrefHasDefaultValue(prefs, prefs::kVariationsSafeSeedDate));
  EXPECT_TRUE(PrefHasDefaultValue(prefs, prefs::kVariationsSafeSeedFetchTime));
  EXPECT_TRUE(PrefHasDefaultValue(prefs, prefs::kVariationsSafeSeedLocale));
  EXPECT_TRUE(PrefHasDefaultValue(
      prefs, prefs::kVariationsSafeSeedPermanentConsistencyCountry));
  EXPECT_TRUE(PrefHasDefaultValue(
      prefs, prefs::kVariationsSafeSeedSessionConsistencyCountry));
  EXPECT_TRUE(PrefHasDefaultValue(prefs, prefs::kVariationsSafeSeedSignature));

  // However, only the safe seed prefs should be cleared; the latest seed prefs
  // should not be modified.
  EXPECT_FALSE(PrefHasDefaultValue(prefs, prefs::kVariationsCompressedSeed));
  EXPECT_FALSE(PrefHasDefaultValue(prefs, prefs::kVariationsSeedDate));
  EXPECT_FALSE(PrefHasDefaultValue(prefs, prefs::kVariationsSeedSignature));

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

  // Loading a valid seed with an invalid signature should return false and
  // clear all associated prefs when signature verification is enabled.
  SignatureVerifyingVariationsSeedStore seed_store(&prefs);
  VariationsSeed loaded_seed;
  std::unique_ptr<ClientFilterableState> client_state =
      CreateTestClientFilterableState();
  base::Time fetch_time;
  EXPECT_FALSE(
      seed_store.LoadSafeSeed(&loaded_seed, client_state.get(), &fetch_time));
  EXPECT_TRUE(PrefHasDefaultValue(prefs, prefs::kVariationsSafeCompressedSeed));
  EXPECT_TRUE(PrefHasDefaultValue(prefs, prefs::kVariationsSafeSeedDate));
  EXPECT_TRUE(PrefHasDefaultValue(prefs, prefs::kVariationsSafeSeedFetchTime));
  EXPECT_TRUE(PrefHasDefaultValue(prefs, prefs::kVariationsSafeSeedLocale));
  EXPECT_TRUE(PrefHasDefaultValue(
      prefs, prefs::kVariationsSafeSeedPermanentConsistencyCountry));
  EXPECT_TRUE(PrefHasDefaultValue(
      prefs, prefs::kVariationsSafeSeedSessionConsistencyCountry));
  EXPECT_TRUE(PrefHasDefaultValue(prefs, prefs::kVariationsSafeSeedSignature));

  // However, only the safe seed prefs should be cleared; the latest seed prefs
  // should not be modified.
  EXPECT_FALSE(PrefHasDefaultValue(prefs, prefs::kVariationsCompressedSeed));
  EXPECT_FALSE(PrefHasDefaultValue(prefs, prefs::kVariationsSeedDate));
  EXPECT_FALSE(PrefHasDefaultValue(prefs, prefs::kVariationsSeedSignature));

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

  // Loading an empty seed should return false.
  TestVariationsSeedStore seed_store(&prefs);
  VariationsSeed loaded_seed;
  ClientFilterableState client_state({});
  base::Time fetch_time;
  EXPECT_FALSE(
      seed_store.LoadSafeSeed(&loaded_seed, &client_state, &fetch_time));
}

TEST(VariationsSeedStoreTest, StoreSafeSeed_ValidSeed) {
  const VariationsSeed seed = CreateTestSeed();
  const std::string serialized_seed = SerializeSeed(seed);
  const std::string signature = "a completely ignored signature";
  ClientFilterableState client_state({});
  client_state.locale = "en-US";
  client_state.reference_date = WrapTime(12345);
  client_state.session_consistency_country = "US";
  client_state.permanent_consistency_country = "CA";
  const base::Time fetch_time = WrapTime(99999);

  TestingPrefServiceSimple prefs;
  VariationsSeedStore::RegisterPrefs(prefs.registry());
  TestVariationsSeedStore seed_store(&prefs);

  base::HistogramTester histogram_tester;
  EXPECT_TRUE(seed_store.StoreSafeSeed(serialized_seed, signature, client_state,
                                       fetch_time));

  // Verify the stored data.
  std::string loaded_compressed_seed =
      prefs.GetString(prefs::kVariationsSafeCompressedSeed);
  std::string decoded_compressed_seed;
  ASSERT_TRUE(
      base::Base64Decode(loaded_compressed_seed, &decoded_compressed_seed));
  EXPECT_EQ(Compress(serialized_seed), decoded_compressed_seed);
  EXPECT_EQ(signature, prefs.GetString(prefs::kVariationsSafeSeedSignature));
  EXPECT_EQ("en-US", prefs.GetString(prefs::kVariationsSafeSeedLocale));
  EXPECT_EQ(WrapTime(12345), prefs.GetTime(prefs::kVariationsSafeSeedDate));
  EXPECT_EQ(WrapTime(99999),
            prefs.GetTime(prefs::kVariationsSafeSeedFetchTime));
  EXPECT_EQ("US", prefs.GetString(
                      prefs::kVariationsSafeSeedSessionConsistencyCountry));
  EXPECT_EQ("CA", prefs.GetString(
                      prefs::kVariationsSafeSeedPermanentConsistencyCountry));

  // Verify metrics.
  histogram_tester.ExpectUniqueSample(
      "Variations.SafeMode.StoreSafeSeed.Result", StoreSeedResult::SUCCESS, 1);
}

TEST(VariationsSeedStoreTest, StoreSafeSeed_EmptySeed) {
  const std::string serialized_seed;
  const std::string signature = "a completely ignored signature";
  ClientFilterableState client_state({});
  client_state.locale = "en-US";
  client_state.reference_date = WrapTime(54321);
  client_state.session_consistency_country = "US";
  client_state.permanent_consistency_country = "CA";
  base::Time fetch_time = WrapTime(99999);

  TestingPrefServiceSimple prefs;
  VariationsSeedStore::RegisterPrefs(prefs.registry());
  prefs.SetString(prefs::kVariationsSafeCompressedSeed, "a seed");
  prefs.SetString(prefs::kVariationsSafeSeedSignature, "a signature");
  prefs.SetString(prefs::kVariationsSafeSeedLocale, "en-US");
  prefs.SetString(prefs::kVariationsSafeSeedPermanentConsistencyCountry, "CA");
  prefs.SetString(prefs::kVariationsSafeSeedSessionConsistencyCountry, "US");
  prefs.SetTime(prefs::kVariationsSafeSeedDate, WrapTime(12345));
  prefs.SetTime(prefs::kVariationsSafeSeedFetchTime, WrapTime(34567));

  TestVariationsSeedStore seed_store(&prefs);

  base::HistogramTester histogram_tester;
  EXPECT_FALSE(seed_store.StoreSafeSeed(serialized_seed, signature,
                                        client_state, fetch_time));

  // Verify that none of the prefs were overwritten.
  EXPECT_EQ("a seed", prefs.GetString(prefs::kVariationsSafeCompressedSeed));
  EXPECT_EQ("a signature",
            prefs.GetString(prefs::kVariationsSafeSeedSignature));
  EXPECT_EQ("en-US", prefs.GetString(prefs::kVariationsSafeSeedLocale));
  EXPECT_EQ("CA", prefs.GetString(
                      prefs::kVariationsSafeSeedPermanentConsistencyCountry));
  EXPECT_EQ("US", prefs.GetString(
                      prefs::kVariationsSafeSeedSessionConsistencyCountry));
  EXPECT_EQ(WrapTime(12345), prefs.GetTime(prefs::kVariationsSafeSeedDate));
  EXPECT_EQ(WrapTime(34567),
            prefs.GetTime(prefs::kVariationsSafeSeedFetchTime));

  // Verify metrics.
  histogram_tester.ExpectUniqueSample(
      "Variations.SafeMode.StoreSafeSeed.Result",
      StoreSeedResult::FAILED_EMPTY_GZIP_CONTENTS, 1);
}

TEST(VariationsSeedStoreTest, StoreSafeSeed_InvalidSeed) {
  const std::string serialized_seed = "a nonsense seed";
  const std::string signature = "a completely ignored signature";
  ClientFilterableState client_state({});
  client_state.locale = "en-US";
  client_state.reference_date = WrapTime(12345);
  client_state.session_consistency_country = "US";
  client_state.permanent_consistency_country = "CA";
  base::Time fetch_time = WrapTime(54321);

  TestingPrefServiceSimple prefs;
  VariationsSeedStore::RegisterPrefs(prefs.registry());
  prefs.SetString(prefs::kVariationsSafeCompressedSeed, "a previous seed");
  prefs.SetString(prefs::kVariationsSafeSeedSignature, "a previous signature");
  prefs.SetString(prefs::kVariationsSafeSeedLocale, "en-CA");
  prefs.SetString(prefs::kVariationsSafeSeedPermanentConsistencyCountry, "IN");
  prefs.SetString(prefs::kVariationsSafeSeedSessionConsistencyCountry, "MX");
  prefs.SetTime(prefs::kVariationsSafeSeedDate, WrapTime(67890));
  prefs.SetTime(prefs::kVariationsSafeSeedFetchTime, WrapTime(13579));

  SignatureVerifyingVariationsSeedStore seed_store(&prefs);

  base::HistogramTester histogram_tester;
  EXPECT_FALSE(seed_store.StoreSafeSeed(serialized_seed, signature,
                                        client_state, fetch_time));

  // Verify that none of the prefs were overwritten.
  EXPECT_EQ("a previous seed",
            prefs.GetString(prefs::kVariationsSafeCompressedSeed));
  EXPECT_EQ("a previous signature",
            prefs.GetString(prefs::kVariationsSafeSeedSignature));
  EXPECT_EQ("en-CA", prefs.GetString(prefs::kVariationsSafeSeedLocale));
  EXPECT_EQ("IN", prefs.GetString(
                      prefs::kVariationsSafeSeedPermanentConsistencyCountry));
  EXPECT_EQ("MX", prefs.GetString(
                      prefs::kVariationsSafeSeedSessionConsistencyCountry));
  EXPECT_EQ(WrapTime(67890), prefs.GetTime(prefs::kVariationsSafeSeedDate));
  EXPECT_EQ(WrapTime(13579),
            prefs.GetTime(prefs::kVariationsSafeSeedFetchTime));

  // Verify metrics.
  histogram_tester.ExpectUniqueSample(
      "Variations.SafeMode.StoreSafeSeed.Result", StoreSeedResult::FAILED_PARSE,
      1);
}

TEST(VariationsSeedStoreTest, StoreSafeSeed_InvalidSignature) {
  const VariationsSeed seed = CreateTestSeed();
  const std::string serialized_seed = SerializeSeed(seed);
  // A valid signature, but for a different seed.
  const std::string signature = kBase64SeedSignature;
  ClientFilterableState client_state({});
  client_state.locale = "en-US";
  client_state.reference_date = WrapTime(12345);
  client_state.session_consistency_country = "US";
  client_state.permanent_consistency_country = "CA";
  const base::Time fetch_time = WrapTime(34567);

  TestingPrefServiceSimple prefs;
  VariationsSeedStore::RegisterPrefs(prefs.registry());
  prefs.SetString(prefs::kVariationsSafeCompressedSeed, "a previous seed");
  prefs.SetString(prefs::kVariationsSafeSeedSignature, "a previous signature");
  prefs.SetString(prefs::kVariationsSafeSeedLocale, "en-CA");
  prefs.SetString(prefs::kVariationsSafeSeedPermanentConsistencyCountry, "IN");
  prefs.SetString(prefs::kVariationsSafeSeedSessionConsistencyCountry, "MX");
  prefs.SetTime(prefs::kVariationsSafeSeedDate, WrapTime(67890));
  prefs.SetTime(prefs::kVariationsSafeSeedFetchTime, WrapTime(24680));

  SignatureVerifyingVariationsSeedStore seed_store(&prefs);

  base::HistogramTester histogram_tester;
  EXPECT_FALSE(seed_store.StoreSafeSeed(serialized_seed, signature,
                                        client_state, fetch_time));

  // Verify that none of the prefs were overwritten.
  EXPECT_EQ("a previous seed",
            prefs.GetString(prefs::kVariationsSafeCompressedSeed));
  EXPECT_EQ("a previous signature",
            prefs.GetString(prefs::kVariationsSafeSeedSignature));
  EXPECT_EQ("en-CA", prefs.GetString(prefs::kVariationsSafeSeedLocale));
  EXPECT_EQ("IN", prefs.GetString(
                      prefs::kVariationsSafeSeedPermanentConsistencyCountry));
  EXPECT_EQ("MX", prefs.GetString(
                      prefs::kVariationsSafeSeedSessionConsistencyCountry));
  EXPECT_EQ(WrapTime(67890), prefs.GetTime(prefs::kVariationsSafeSeedDate));
  EXPECT_EQ(WrapTime(24680),
            prefs.GetTime(prefs::kVariationsSafeSeedFetchTime));

  // Verify metrics.
  histogram_tester.ExpectUniqueSample(
      "Variations.SafeMode.StoreSafeSeed.Result",
      StoreSeedResult::FAILED_SIGNATURE, 1);
  histogram_tester.ExpectUniqueSample(
      "Variations.SafeMode.StoreSafeSeed.SignatureValidity",
      VerifySignatureResult::INVALID_SEED, 1);
}

TEST(VariationsSeedStoreTest, StoreSafeSeed_ValidSignature) {
  std::string serialized_seed;
  ASSERT_TRUE(
      base::Base64Decode(kUncompressedBase64SeedData, &serialized_seed));
  const std::string signature = kBase64SeedSignature;
  ClientFilterableState client_state({});
  client_state.locale = "en-US";
  client_state.reference_date = WrapTime(12345);
  client_state.session_consistency_country = "US";
  client_state.permanent_consistency_country = "CA";
  const base::Time fetch_time = WrapTime(34567);

  TestingPrefServiceSimple prefs;
  VariationsSeedStore::RegisterPrefs(prefs.registry());
  SignatureVerifyingVariationsSeedStore seed_store(&prefs);

  base::HistogramTester histogram_tester;
  EXPECT_TRUE(seed_store.StoreSafeSeed(serialized_seed, signature, client_state,
                                       fetch_time));

  // Verify the stored data.
  std::string loaded_compressed_seed =
      prefs.GetString(prefs::kVariationsSafeCompressedSeed);
  std::string decoded_compressed_seed;
  ASSERT_TRUE(
      base::Base64Decode(loaded_compressed_seed, &decoded_compressed_seed));
  EXPECT_EQ(Compress(serialized_seed), decoded_compressed_seed);
  EXPECT_EQ(signature, prefs.GetString(prefs::kVariationsSafeSeedSignature));
  EXPECT_EQ("en-US", prefs.GetString(prefs::kVariationsSafeSeedLocale));
  EXPECT_EQ(WrapTime(12345), prefs.GetTime(prefs::kVariationsSafeSeedDate));
  EXPECT_EQ(WrapTime(34567),
            prefs.GetTime(prefs::kVariationsSafeSeedFetchTime));
  EXPECT_EQ("US", prefs.GetString(
                      prefs::kVariationsSafeSeedSessionConsistencyCountry));
  EXPECT_EQ("CA", prefs.GetString(
                      prefs::kVariationsSafeSeedPermanentConsistencyCountry));

  // Verify metrics.
  histogram_tester.ExpectUniqueSample(
      "Variations.SafeMode.StoreSafeSeed.Result", StoreSeedResult::SUCCESS, 1);
  histogram_tester.ExpectUniqueSample(
      "Variations.SafeMode.StoreSafeSeed.SignatureValidity",
      VerifySignatureResult::VALID_SIGNATURE, 1);
}

TEST(VariationsSeedStoreTest, StoreSafeSeed_IdenticalToLatestSeed) {
  const VariationsSeed seed = CreateTestSeed();
  const std::string serialized_seed = SerializeSeed(seed);
  const std::string base64_seed = SerializeSeedBase64(seed);
  const std::string signature = "a completely ignored signature";
  ClientFilterableState unused_client_state({});
  const base::Time fetch_time = WrapTime(12345);

  TestingPrefServiceSimple prefs;
  VariationsSeedStore::RegisterPrefs(prefs.registry());
  prefs.SetString(prefs::kVariationsCompressedSeed, base64_seed);
  prefs.SetTime(prefs::kVariationsLastFetchTime, WrapTime(99999));

  TestVariationsSeedStore seed_store(&prefs);
  base::HistogramTester histogram_tester;
  EXPECT_TRUE(seed_store.StoreSafeSeed(serialized_seed, signature,
                                       unused_client_state, fetch_time));

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
  base::Time loaded_fetch_time;
  EXPECT_TRUE(seed_store.LoadSafeSeed(&loaded_safe_seed, &unused_client_state,
                                      &loaded_fetch_time));
  EXPECT_EQ(SerializeSeed(seed), SerializeSeed(loaded_safe_seed));
  EXPECT_EQ(WrapTime(99999), loaded_fetch_time);

  // Verify metrics.
  histogram_tester.ExpectUniqueSample(
      "Variations.SafeMode.StoreSafeSeed.Result", StoreSeedResult::SUCCESS, 1);
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
  ClientFilterableState unused_client_state({});

  TestingPrefServiceSimple prefs;
  VariationsSeedStore::RegisterPrefs(prefs.registry());
  prefs.SetString(prefs::kVariationsSafeCompressedSeed, base64_old_seed);
  prefs.SetString(prefs::kVariationsCompressedSeed,
                  kIdenticalToSafeSeedSentinel);

  TestVariationsSeedStore seed_store(&prefs);
  base::HistogramTester histogram_tester;
  EXPECT_TRUE(seed_store.StoreSafeSeed(serialized_new_seed, signature,
                                       unused_client_state, fetch_time));

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
  base::Time loaded_fetch_time;
  EXPECT_TRUE(seed_store.LoadSafeSeed(&loaded_safe_seed, &unused_client_state,
                                      &loaded_fetch_time));
  EXPECT_EQ(SerializeSeed(new_seed), SerializeSeed(loaded_safe_seed));
  EXPECT_EQ(fetch_time, loaded_fetch_time);

  // Verify metrics.
  histogram_tester.ExpectUniqueSample(
      "Variations.SafeMode.StoreSafeSeed.Result", StoreSeedResult::SUCCESS, 1);
}

TEST(VariationsSeedStoreTest, StoreSeedData_GzippedEmptySeed) {
  std::string empty_seed;
  std::string compressed_seed;
  ASSERT_TRUE(compression::GzipCompress(empty_seed, &compressed_seed));

  TestingPrefServiceSimple prefs;
  VariationsSeedStore::RegisterPrefs(prefs.registry());
  TestVariationsSeedStore seed_store(&prefs);

  VariationsSeed parsed_seed;
  EXPECT_FALSE(seed_store.StoreSeedData(compressed_seed, std::string(),
                                        std::string(), base::Time::Now(), false,
                                        true, false, &parsed_seed));
}

TEST(VariationsSeedStoreTest, VerifySeedSignature) {
  // A valid seed and signature pair generated using the server's private key.
  const std::string uncompressed_base64_seed_data = kUncompressedBase64SeedData;
  const std::string base64_seed_signature = kBase64SeedSignature;

  std::string seed_data;
  ASSERT_TRUE(base::Base64Decode(uncompressed_base64_seed_data, &seed_data));
  VariationsSeed seed;
  ASSERT_TRUE(seed.ParseFromString(seed_data));
  std::string base64_seed_data = SerializeSeedBase64(seed);

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
    std::string base64_seed_signature;
    EXPECT_TRUE(seed_store.LoadSeed(&seed, &seed_data, &base64_seed_signature));
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
    std::string base64_seed_signature;
    EXPECT_FALSE(
        seed_store.LoadSeed(&seed, &seed_data, &base64_seed_signature));
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
    std::string seed_data;
    std::string base64_seed_signature;
    EXPECT_FALSE(
        seed_store.LoadSeed(&seed, &seed_data, &base64_seed_signature));
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
    std::string base64_seed_signature;
    EXPECT_FALSE(
        seed_store.LoadSeed(&seed, &seed_data, &base64_seed_signature));
    histogram_tester.ExpectUniqueSample(
        "Variations.LoadSeedSignature",
        static_cast<base::HistogramBase::Sample>(
            VerifySignatureResult::INVALID_SEED),
        1);
  }

  // Using a different seed should not match the signature.
  {
    VariationsSeed wrong_seed;
    ASSERT_TRUE(wrong_seed.ParseFromString(seed_data));
    (*wrong_seed.mutable_study(0)->mutable_name())[0] = 'x';
    std::string base64_wrong_seed_data = SerializeSeedBase64(wrong_seed);

    prefs.SetString(prefs::kVariationsCompressedSeed, base64_wrong_seed_data);
    prefs.SetString(prefs::kVariationsSeedSignature, base64_seed_signature);
    SignatureVerifyingVariationsSeedStore seed_store(&prefs);

    base::HistogramTester histogram_tester;
    std::string seed_data;
    std::string base64_seed_signature;
    EXPECT_FALSE(
        seed_store.LoadSeed(&seed, &seed_data, &base64_seed_signature));
    histogram_tester.ExpectUniqueSample(
        "Variations.LoadSeedSignature",
        static_cast<base::HistogramBase::Sample>(
            VerifySignatureResult::INVALID_SEED),
        1);
  }
}

TEST(VariationsSeedStoreTest, ApplyDeltaPatch) {
  // Sample seeds and the server produced delta between them to verify that the
  // client code is able to decode the deltas produced by the server.
  const std::string base64_before_seed_data =
      "CigxN2E4ZGJiOTI4ODI0ZGU3ZDU2MGUyODRlODY1ZDllYzg2NzU1MTE0ElgKDFVNQVN0YWJp"
      "bGl0eRjEyomgBTgBQgtTZXBhcmF0ZUxvZ0oLCgdEZWZhdWx0EABKDwoLU2VwYXJhdGVMb2cQ"
      "ZFIVEgszNC4wLjE4MDEuMCAAIAEgAiADEkQKIFVNQS1Vbmlmb3JtaXR5LVRyaWFsLTEwMC1Q"
      "ZXJjZW50GIDjhcAFOAFCCGdyb3VwXzAxSgwKCGdyb3VwXzAxEAFgARJPCh9VTUEtVW5pZm9y"
      "bWl0eS1UcmlhbC01MC1QZXJjZW50GIDjhcAFOAFCB2RlZmF1bHRKDAoIZ3JvdXBfMDEQAUoL"
      "CgdkZWZhdWx0EAFgAQ==";
  const std::string base64_after_seed_data =
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

  std::string before_seed_data;
  std::string after_seed_data;
  std::string delta_data;
  EXPECT_TRUE(base::Base64Decode(base64_before_seed_data, &before_seed_data));
  EXPECT_TRUE(base::Base64Decode(base64_after_seed_data, &after_seed_data));
  EXPECT_TRUE(base::Base64Decode(base64_delta_data, &delta_data));

  std::string output;
  EXPECT_TRUE(VariationsSeedStore::ApplyDeltaPatch(before_seed_data, delta_data,
                                                   &output));
  EXPECT_EQ(after_seed_data, output);
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

// Verifies that the cached serial number is correctly updated when a new seed
// is saved.
TEST(VariationsSeedStoreTest, GetLatestSerialNumber_UpdatedWithNewStoredSeed) {
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
  seed_store.StoreSeedForTesting(SerializeSeed(new_seed));
  EXPECT_EQ("456", seed_store.GetLatestSerialNumber());
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

#if defined(OS_ANDROID)
TEST(VariationsSeedStoreTest, ImportFirstRunJavaSeed) {
  const std::string test_seed_data = "raw_seed_data_test";
  const std::string test_seed_signature = "seed_signature_test";
  const std::string test_seed_country = "seed_country_code_test";
  const long test_response_date = 1234567890;
  const bool test_is_gzip_compressed = true;
  android::SetJavaFirstRunPrefsForTesting(test_seed_data, test_seed_signature,
                                          test_seed_country, test_response_date,
                                          test_is_gzip_compressed);

  auto seed = android::GetVariationsFirstRunSeed();
  EXPECT_EQ(test_seed_data,          seed->data);
  EXPECT_EQ(test_seed_signature,     seed->signature);
  EXPECT_EQ(test_seed_country,       seed->country);
  EXPECT_EQ(test_response_date,      seed->date);
  EXPECT_EQ(test_is_gzip_compressed, seed->is_gzip_compressed);

  android::ClearJavaFirstRunPrefs();
  seed = android::GetVariationsFirstRunSeed();
  EXPECT_EQ("", seed->data);
  EXPECT_EQ("", seed->signature);
  EXPECT_EQ("", seed->country);
  EXPECT_EQ(0, seed->date);
  EXPECT_FALSE(seed->is_gzip_compressed);
}
#endif  // OS_ANDROID

}  // namespace variations
