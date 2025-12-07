// Copyright 2014 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/variations/variations_seed_store.h"

#include <memory>
#include <utility>

#include "base/base64.h"
#include "base/build_time.h"
#include "base/files/file_util.h"
#include "base/files/scoped_temp_dir.h"
#include "base/functional/callback_helpers.h"
#include "base/run_loop.h"
#include "base/test/bind.h"
#include "base/test/metrics/histogram_tester.h"
#include "base/test/mock_entropy_provider.h"
#include "base/test/scoped_command_line.h"
#include "base/test/scoped_feature_list.h"
#include "base/test/task_environment.h"
#include "base/threading/thread.h"
#include "base/time/time.h"
#include "base/timer/mock_timer.h"
#include "base/version.h"
#include "build/build_config.h"
#include "components/prefs/testing_pref_service.h"
#include "components/variations/client_filterable_state.h"
#include "components/variations/entropy_provider.h"
#include "components/variations/pref_names.h"
#include "components/variations/proto/stored_seed_info.pb.h"
#include "components/variations/proto/study.pb.h"
#include "components/variations/proto/variations_seed.pb.h"
#include "components/variations/seed_reader_writer.h"
#include "components/variations/variations_safe_seed_store_local_state.h"
#include "components/variations/variations_switches.h"
#include "components/variations/variations_test_utils.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/zlib/google/compression_utils.h"

#if BUILDFLAG(IS_ANDROID)
#include "components/variations/android/variations_seed_bridge.h"
#endif  // BUILDFLAG(IS_ANDROID)

namespace variations {
namespace {

using ::testing::IsEmpty;
using ::testing::Not;

// File used by SeedReaderWriter to store a latest seed.
const base::FilePath::CharType kSeedFilename[] =
    FILE_PATH_LITERAL("VariationsSeedV2");
const base::FilePath::CharType kOldSeedFilename[] =
    FILE_PATH_LITERAL("VariationsSeedV1");
const base::FilePath::CharType kSafeSeedFilename[] =
    FILE_PATH_LITERAL("VariationsSafeSeedV2");

// Used for clients that do not participate in SeedFiles experiment.
constexpr char kNoGroup[] = "";

// Returns a seed that is larger than the maximum uncompressed seed size.
std::string CreateTooLargeData() {
  return std::string(SeedReaderWriter::MaxUncompressedSeedSizeForTesting() + 1,
                     'A');
}

class TestVariationsSeedStore : public VariationsSeedStore {
 public:
  explicit TestVariationsSeedStore(
      PrefService* local_state,
      base::FilePath seed_file_dir = base::FilePath(),
      bool signature_verification_needed = false,
      std::unique_ptr<SeedResponse> initial_seed = nullptr,
      bool use_first_run_prefs = true,
      version_info::Channel channel = version_info::Channel::UNKNOWN,
      std::unique_ptr<const EntropyProviders> entropy_providers =
          std::make_unique<const MockEntropyProviders>(
              MockEntropyProviders::Results{
                  .low_entropy = kAlwaysUseLastGroup}))
      : VariationsSeedStore(local_state,
                            std::move(initial_seed),
                            signature_verification_needed,
                            std::make_unique<VariationsSafeSeedStoreLocalState>(
                                local_state,
                                seed_file_dir,
                                channel,
                                entropy_providers.get()),
                            channel,
                            seed_file_dir,
                            entropy_providers.get(),
                            use_first_run_prefs) {}
  ~TestVariationsSeedStore() override = default;

  void SetSerialNumberForTesting(std::string_view serial_number) {
    StoreLatestSerialNumber(serial_number);
  }
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
  EXPECT_TRUE(compression::GzipCompress(data, &compressed));
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

// Returns true if a local state seed should be used.
bool ShouldUseLocalStateSeed() {
  return base::FieldTrialList::FindFullName(kSeedFileTrial) != kSeedFilesGroup;
}

// Loads the seed from the seed store and returns true if successful.
bool MakeSeedStoreLoadStoredSeed(TestVariationsSeedStore& seed_store) {
  VariationsSeed seed;
  std::string seed_data;
  std::string seed_signature;
  return seed_store.LoadSeedSync(&seed, &seed_data, &seed_signature);
}

// Gets the seed data from the seed store.
SeedInfo GetSeedInfo(TestVariationsSeedStore& seed_store) {
  return seed_store.GetSeedReaderWriterForTesting()->GetSeedInfo();
}

// Gets the safe seed data from the seed store.
SeedInfo GetSafeSeedInfo(TestVariationsSeedStore& seed_store) {
  return seed_store.GetSafeSeedReaderWriterForTesting()->GetSeedInfo();
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

// Sets all seed-related prefs to non-default values. Also, sets seed-file-based
// seeds to non-default values using |seed_store| for the seed file experiments
// treatment-group clients. Used to verify whether pref values were cleared.
void SetAllSeedsAndSeedPrefsToNonDefaultValues(
    PrefService* prefs,
    TestVariationsSeedStore& seed_store) {
  const base::Time now = base::Time::Now();
  const base::TimeDelta delta = base::Days(1);

  //  Update the latest seed in memory. This is done for the Local-State-based
  //  seed OR the seed-file-based seed depending on the seed file trial group to
  //  which the client belongs.
  seed_store.GetSeedReaderWriterForTesting()->StoreValidatedSeedInfo(
      ValidatedSeedInfo{
          .seed_data = "coffee",
          .signature = "tea",
          .milestone = 1,
          .seed_date = now - delta * 1,
          .client_fetch_time = now,
          .session_country_code = "us",
      });
  seed_store.SetSerialNumberForTesting("123");

  //  Update the safe seed in memory. This is done for the Local-State-based
  //  seed OR the seed-file-based seed depending on the seed file trial group to
  //  which the client belongs.
  seed_store.GetSafeSeedReaderWriterForTesting()->StoreValidatedSeedInfo(
      ValidatedSeedInfo{
          .seed_data = "ketchup",
          .signature = "mustard",
          .milestone = 90,
          .seed_date = now - delta * 2,
          .client_fetch_time = now - delta * 3,
          .session_country_code = "gt",
          .permanent_country_code = "mx",
      });
  prefs->SetString(prefs::kVariationsSafeSeedLocale, "en-MX");
}

// Checks whether the given pref has its default value in |prefs|.
bool PrefHasDefaultValue(const TestingPrefServiceSimple& prefs,
                         const char* pref_name) {
  return prefs.FindPreference(pref_name)->IsDefaultValue();
}

void CheckRegularSeedAndSeedPrefsAreSet(const TestingPrefServiceSimple& prefs,
                                        TestVariationsSeedStore& seed_store) {
  std::string stored_seed_data;
  std::string stored_seed_signature;
  EXPECT_EQ(seed_store.GetSeedReaderWriterForTesting()->ReadSeedDataOnStartup(
                &stored_seed_data, &stored_seed_signature),
            LoadSeedResult::kSuccess);
  EXPECT_THAT(stored_seed_data, Not(IsEmpty()));
  SeedInfo stored_seed_info = GetSeedInfo(seed_store);
  EXPECT_THAT(stored_seed_info.signature, Not(IsEmpty()));
  EXPECT_NE(stored_seed_info.milestone, 0);
  EXPECT_NE(stored_seed_info.seed_date, base::Time());
  EXPECT_NE(stored_seed_info.client_fetch_time, base::Time());
  if (ShouldUseLocalStateSeed()) {
    EXPECT_FALSE(PrefHasDefaultValue(prefs, prefs::kVariationsCompressedSeed));
    EXPECT_FALSE(PrefHasDefaultValue(prefs, prefs::kVariationsSeedSignature));
    EXPECT_FALSE(PrefHasDefaultValue(prefs, prefs::kVariationsSeedMilestone));
    EXPECT_FALSE(PrefHasDefaultValue(prefs, prefs::kVariationsLastFetchTime));
    EXPECT_FALSE(PrefHasDefaultValue(prefs, prefs::kVariationsSeedDate));
  }
}

void CheckRegularSeedAndSeedPrefsAreCleared(
    const TestingPrefServiceSimple& prefs,
    TestVariationsSeedStore& seed_store) {
  std::string stored_seed_data;
  EXPECT_EQ(seed_store.GetSeedReaderWriterForTesting()->ReadSeedDataOnStartup(
                &stored_seed_data, nullptr),
            LoadSeedResult::kEmpty);
  SeedInfo stored_seed_info = GetSeedInfo(seed_store);
  EXPECT_THAT(stored_seed_info.signature, IsEmpty());
  EXPECT_EQ(stored_seed_info.milestone, 0);
  EXPECT_EQ(stored_seed_info.seed_date, base::Time());
  EXPECT_EQ(stored_seed_info.client_fetch_time, base::Time());
  if (ShouldUseLocalStateSeed()) {
    EXPECT_TRUE(PrefHasDefaultValue(prefs, prefs::kVariationsCompressedSeed));
    EXPECT_TRUE(PrefHasDefaultValue(prefs, prefs::kVariationsSeedSignature));
    EXPECT_TRUE(PrefHasDefaultValue(prefs, prefs::kVariationsSeedMilestone));
    EXPECT_TRUE(PrefHasDefaultValue(prefs, prefs::kVariationsLastFetchTime));
    EXPECT_TRUE(PrefHasDefaultValue(prefs, prefs::kVariationsSeedDate));
  }

  // The serial number should be cleared when the seed is cleared.
  EXPECT_THAT(seed_store.GetLatestSerialNumber(), IsEmpty());
}

void CheckSafeSeedAndSeedPrefsAreSet(const TestingPrefServiceSimple& prefs,
                                     TestVariationsSeedStore& seed_store) {
  std::string stored_seed_data;
  std::string stored_seed_signature;
  EXPECT_EQ(
      seed_store.GetSafeSeedReaderWriterForTesting()->ReadSeedDataOnStartup(
          &stored_seed_data, &stored_seed_signature),
      LoadSeedResult::kSuccess);
  EXPECT_THAT(stored_seed_data, Not(IsEmpty()));
  EXPECT_THAT(stored_seed_signature, Not(IsEmpty()));
  SeedInfo stored_seed_info = GetSafeSeedInfo(seed_store);
  EXPECT_THAT(stored_seed_info.signature, Not(IsEmpty()));
  EXPECT_NE(stored_seed_info.milestone, 0);
  EXPECT_NE(stored_seed_info.seed_date, base::Time());
  EXPECT_NE(stored_seed_info.client_fetch_time, base::Time());
  if (ShouldUseLocalStateSeed()) {
    EXPECT_FALSE(
        PrefHasDefaultValue(prefs, prefs::kVariationsSafeCompressedSeed));
    EXPECT_FALSE(
        PrefHasDefaultValue(prefs, prefs::kVariationsSafeSeedSignature));
    EXPECT_FALSE(
        PrefHasDefaultValue(prefs, prefs::kVariationsSafeSeedMilestone));
    EXPECT_FALSE(PrefHasDefaultValue(prefs, prefs::kVariationsSafeSeedDate));
    EXPECT_FALSE(
        PrefHasDefaultValue(prefs, prefs::kVariationsSafeSeedFetchTime));
    EXPECT_FALSE(PrefHasDefaultValue(
        prefs, prefs::kVariationsSafeSeedPermanentConsistencyCountry));
    EXPECT_FALSE(PrefHasDefaultValue(
        prefs, prefs::kVariationsSafeSeedSessionConsistencyCountry));
  }
  EXPECT_FALSE(PrefHasDefaultValue(prefs, prefs::kVariationsSafeSeedLocale));
}

void CheckSafeSeedAndSeedPrefsAreCleared(const TestingPrefServiceSimple& prefs,
                                         TestVariationsSeedStore& seed_store) {
  std::string stored_seed_data;
  EXPECT_EQ(
      seed_store.GetSafeSeedReaderWriterForTesting()->ReadSeedDataOnStartup(
          &stored_seed_data, nullptr),
      LoadSeedResult::kEmpty);
  SeedInfo stored_seed = GetSafeSeedInfo(seed_store);
  EXPECT_THAT(stored_seed.signature, IsEmpty());
  EXPECT_EQ(stored_seed.milestone, 0);
  EXPECT_EQ(stored_seed.seed_date, base::Time());
  EXPECT_EQ(stored_seed.client_fetch_time, base::Time());
  if (ShouldUseLocalStateSeed()) {
    EXPECT_TRUE(
        PrefHasDefaultValue(prefs, prefs::kVariationsSafeCompressedSeed));
    EXPECT_TRUE(
        PrefHasDefaultValue(prefs, prefs::kVariationsSafeSeedSignature));
    EXPECT_TRUE(
        PrefHasDefaultValue(prefs, prefs::kVariationsSafeSeedMilestone));
    EXPECT_TRUE(PrefHasDefaultValue(prefs, prefs::kVariationsSafeSeedDate));
    EXPECT_TRUE(
        PrefHasDefaultValue(prefs, prefs::kVariationsSafeSeedFetchTime));
    EXPECT_TRUE(PrefHasDefaultValue(
        prefs, prefs::kVariationsSafeSeedPermanentConsistencyCountry));
    EXPECT_TRUE(PrefHasDefaultValue(
        prefs, prefs::kVariationsSafeSeedSessionConsistencyCountry));
  }
  EXPECT_TRUE(PrefHasDefaultValue(prefs, prefs::kVariationsSafeSeedLocale));
}
}  // namespace

class VariationsSeedStoreTest : public ::testing::Test {
 private:
  base::test::TaskEnvironment task_environment_;
};

TEST_F(VariationsSeedStoreTest, ApplyDeltaPatch) {
  std::string output;
  ASSERT_TRUE(VariationsSeedStore::ApplyDeltaPatch(
      kSeedDeltaTestData.GetInitialSeedData(),
      kSeedDeltaTestData.GetDeltaData(), &output));
  EXPECT_EQ(kSeedDeltaTestData.GetNewSeedData(), output);
}

class SeedStoreGroupTestBase : public ::testing::Test {
 public:
  explicit SeedStoreGroupTestBase(const SeedFieldsPrefs& seed_fields_prefs,
                                  std::string_view field_trial_group)
      : file_writer_thread_("SeedReaderWriter Test thread") {
    scoped_feature_list_.InitWithEmptyFeatureAndFieldTrialLists();
    file_writer_thread_.Start();
    CHECK(temp_dir_.CreateUniqueTempDir());
    temp_seed_file_path_ = temp_dir_.GetPath().Append(kSeedFilename);

    VariationsSeedStore::RegisterPrefs(prefs_.registry());
    SetUpSeedFileTrial(std::string(field_trial_group));

    const std::string_view seed_data_field = seed_fields_prefs.seed;
    std::string histogram_suffix =
        base::Contains(seed_data_field, "Safe") ? "Safe" : "Latest";

    // Initialize |seed_reader_writer_|.
    seed_reader_writer_ = std::make_unique<SeedReaderWriter>(
        &prefs_, temp_dir_.GetPath(), kSeedFilename, kOldSeedFilename,
        seed_fields_prefs, version_info::Channel::UNKNOWN,
        std::make_unique<const MockEntropyProviders>(
            MockEntropyProviders::Results{.low_entropy = kAlwaysUseLastGroup})
            .get(),
        histogram_suffix, file_writer_thread_.task_runner());
    seed_reader_writer_->SetTimerForTesting(&timer_);
  }

  ~SeedStoreGroupTestBase() override = default;

 protected:
  base::test::ScopedFeatureList scoped_feature_list_;
  base::test::TaskEnvironment task_environment_;
  base::Thread file_writer_thread_;
  base::ScopedTempDir temp_dir_;
  base::MockOneShotTimer timer_;
  base::FilePath temp_seed_file_path_;
  TestingPrefServiceSimple prefs_;
  std::unique_ptr<SeedReaderWriter> seed_reader_writer_;
};

class LoadSeedDataGroupTest
    : public SeedStoreGroupTestBase,
      public ::testing::WithParamInterface<std::string_view> {
 public:
  LoadSeedDataGroupTest()
      : SeedStoreGroupTestBase(kRegularSeedFieldsPrefs, GetParam()) {}
  ~LoadSeedDataGroupTest() override = default;
};


class LoadSeedDataControlAndDefaultGroupsTest : public LoadSeedDataGroupTest {};
INSTANTIATE_TEST_SUITE_P(All,
                         LoadSeedDataControlAndDefaultGroupsTest,
                         ::testing::Values(kControlGroup,
                                           kDefaultGroup,
                                           kNoGroup));

TEST_P(LoadSeedDataControlAndDefaultGroupsTest, LoadSeed_ValidSeed) {
  // Store good seed data to test if loading from prefs works.
  const std::string seed_data = SerializeSeed(CreateTestSeed());
  const std::string base64_seed = GzipAndBase64Encode(seed_data);
  const std::string compressed_seed = Gzip(seed_data);
  const std::string base64_seed_signature = "a test signature, ignored.";

  TestVariationsSeedStore seed_store(&prefs_, temp_dir_.GetPath());
  ASSERT_EQ(base::FieldTrialList::FindFullName(kSeedFileTrial), GetParam());
  seed_store.GetSeedReaderWriterForTesting()->StoreValidatedSeedInfo(
      ValidatedSeedInfo{
          .seed_data = seed_data,
          .signature = base64_seed_signature,
          .milestone = 1,
          .seed_date = base::Time::Now(),
          .client_fetch_time = base::Time::Now(),
          .session_country_code = "us",
      });
  const std::string expected_seed =
      GetParam() == kSeedFilesGroup ? compressed_seed : base64_seed;

  base::HistogramTester histogram_tester;
  VariationsSeed loaded_seed;
  std::string loaded_seed_data;
  std::string loaded_base64_seed_signature;
  // Check that loading a seed works correctly.
  ASSERT_TRUE(seed_store.LoadSeedSync(&loaded_seed, &loaded_seed_data,
                                      &loaded_base64_seed_signature));

  // Verify metrics.
  histogram_tester.ExpectUniqueSample("Variations.SeedLoadResult",
                                      LoadSeedResult::kSuccess, 1);

  // Check that the loaded data is the same as the original.
  EXPECT_EQ(seed_data, SerializeSeed(loaded_seed));
  EXPECT_EQ(seed_data, loaded_seed_data);
  EXPECT_EQ(base64_seed_signature, loaded_base64_seed_signature);
  // Make sure the seed data hasn't been changed.
  if (ShouldUseLocalStateSeed()) {
    EXPECT_EQ(base64_seed, prefs_.GetString(prefs::kVariationsCompressedSeed));
  }
}

TEST_P(LoadSeedDataControlAndDefaultGroupsTest, LoadSeed_InvalidSignature) {
  const std::string seed_data = SerializeSeed(CreateTestSeed());

  // Loading a valid seed with an invalid signature should return false and
  // clear seeds and associated prefs when signature verification is enabled.
  TestVariationsSeedStore seed_store(&prefs_, temp_dir_.GetPath(),
                                     /*signature_verification_needed=*/true);
  ASSERT_EQ(base::FieldTrialList::FindFullName(kSeedFileTrial), GetParam());
  SetAllSeedsAndSeedPrefsToNonDefaultValues(&prefs_, seed_store);
  seed_store.GetSeedReaderWriterForTesting()->StoreValidatedSeedInfo(
      ValidatedSeedInfo{
          .seed_data = seed_data,
          .signature = "a deeply compromised signature.",
          .milestone = 1,
          .seed_date = base::Time::Now(),
          .client_fetch_time = base::Time::Now(),
          .session_country_code = "us",
      });

  base::HistogramTester histogram_tester;
  VariationsSeed loaded_seed;
  std::string loaded_seed_data;
  std::string loaded_base64_seed_signature;
  ASSERT_FALSE(seed_store.LoadSeedSync(&loaded_seed, &loaded_seed_data,
                                       &loaded_base64_seed_signature));

  // Verify metrics and prefs.
  histogram_tester.ExpectUniqueSample("Variations.SeedLoadResult",
                                      LoadSeedResult::kInvalidSignature, 1);
  CheckRegularSeedAndSeedPrefsAreCleared(prefs_, seed_store);
  CheckSafeSeedAndSeedPrefsAreSet(prefs_, seed_store);

  // Verify session country is not cleared.
  EXPECT_THAT(GetSeedInfo(seed_store).session_country_code, Not(IsEmpty()));
}

TEST_P(LoadSeedDataControlAndDefaultGroupsTest, LoadSeed_InvalidProto) {
  // Loading a valid seed with an invalid signature should return false and
  // clear seeds and associated prefs when signature verification is enabled.
  TestVariationsSeedStore seed_store(&prefs_, temp_dir_.GetPath());
  ASSERT_EQ(base::FieldTrialList::FindFullName(kSeedFileTrial), GetParam());
  SetAllSeedsAndSeedPrefsToNonDefaultValues(&prefs_, seed_store);
  seed_store.GetSeedReaderWriterForTesting()->StoreValidatedSeedInfo(
      ValidatedSeedInfo{
          .seed_data = "invalid proto",
          .signature = "ignored signature",
          .milestone = 1,
          .seed_date = base::Time::Now(),
          .client_fetch_time = base::Time::Now(),
          .session_country_code = "us",
      });
  base::HistogramTester histogram_tester;
  VariationsSeed loaded_seed;
  std::string loaded_seed_data;
  std::string loaded_base64_seed_signature;
  ASSERT_FALSE(seed_store.LoadSeedSync(&loaded_seed, &loaded_seed_data,
                                       &loaded_base64_seed_signature));

  // Verify metrics and prefs.
  histogram_tester.ExpectUniqueSample("Variations.SeedLoadResult",
                                      LoadSeedResult::kCorruptProtobuf, 1);
  CheckRegularSeedAndSeedPrefsAreCleared(prefs_, seed_store);
  CheckSafeSeedAndSeedPrefsAreSet(prefs_, seed_store);

  // Verify session country is not cleared.
  EXPECT_THAT(GetSeedInfo(seed_store).session_country_code, Not(IsEmpty()));
}

TEST_P(LoadSeedDataControlAndDefaultGroupsTest, LoadSeed_RejectEmptySignature) {
  const std::string seed_data = SerializeSeed(CreateTestSeed());

  // Loading a valid seed with an empty signature should fail and clear seeds
  // and associated prefs when signature verification is enabled.
  TestVariationsSeedStore seed_store(&prefs_, temp_dir_.GetPath(),
                                     /*signature_verification_needed=*/true);
  ASSERT_EQ(base::FieldTrialList::FindFullName(kSeedFileTrial), GetParam());
  SetAllSeedsAndSeedPrefsToNonDefaultValues(&prefs_, seed_store);
  seed_store.GetSeedReaderWriterForTesting()->StoreValidatedSeedInfo(
      ValidatedSeedInfo{
          .seed_data = seed_data,
          .signature = "",
          .milestone = 1,
          .seed_date = base::Time::Now(),
          .client_fetch_time = base::Time::Now(),
          .session_country_code = "us",
      });

  base::HistogramTester histogram_tester;
  VariationsSeed loaded_seed;
  std::string loaded_seed_data;
  std::string loaded_base64_seed_signature;
  ASSERT_FALSE(seed_store.LoadSeedSync(&loaded_seed, &loaded_seed_data,
                                       &loaded_base64_seed_signature));

  // Verify metrics and prefs.
  histogram_tester.ExpectUniqueSample("Variations.SeedLoadResult",
                                      LoadSeedResult::kInvalidSignature, 1);
  CheckRegularSeedAndSeedPrefsAreCleared(prefs_, seed_store);
  CheckSafeSeedAndSeedPrefsAreSet(prefs_, seed_store);
}

TEST_P(LoadSeedDataControlAndDefaultGroupsTest, LoadSeed_AcceptEmptySignature) {
  const std::string seed_data = SerializeSeed(CreateTestSeed());

  // Loading a valid seed with an empty signature should succeed iff
  // switches::kAcceptEmptySeedSignatureForTesting is on the command line.
  base::test::ScopedCommandLine scoped_command_line;
  scoped_command_line.GetProcessCommandLine()->AppendSwitch(
      switches::kAcceptEmptySeedSignatureForTesting);

  TestVariationsSeedStore seed_store(&prefs_, temp_dir_.GetPath(),
                                     /*signature_verification_needed=*/true);
  ASSERT_EQ(base::FieldTrialList::FindFullName(kSeedFileTrial), GetParam());
  SetAllSeedsAndSeedPrefsToNonDefaultValues(&prefs_, seed_store);
  seed_store.GetSeedReaderWriterForTesting()->StoreValidatedSeedInfo(
      ValidatedSeedInfo{
          .seed_data = seed_data,
          .signature = "",
          .milestone = 1,
          .seed_date = base::Time::Now(),
          .client_fetch_time = base::Time::Now(),
          .session_country_code = "us",
      });

  base::HistogramTester histogram_tester;
  VariationsSeed loaded_seed;
  std::string loaded_seed_data;
  std::string loaded_base64_seed_signature;
  ASSERT_TRUE(seed_store.LoadSeedSync(&loaded_seed, &loaded_seed_data,
                                      &loaded_base64_seed_signature));

  // Verify metrics and prefs.
  histogram_tester.ExpectUniqueSample("Variations.SeedLoadResult",
                                      LoadSeedResult::kSuccess, 1);
  std::string stored_seed_data;
  LoadSeedResult result =
      seed_store.GetSeedReaderWriterForTesting()->ReadSeedDataOnStartup(
          &stored_seed_data, nullptr);
  EXPECT_EQ(result, LoadSeedResult::kSuccess);
  EXPECT_EQ(stored_seed_data, seed_data);
  const SeedInfo stored_seed = GetSeedInfo(seed_store);
  EXPECT_THAT(stored_seed.signature, IsEmpty());
  EXPECT_NE(stored_seed.milestone, 0);
  if (ShouldUseLocalStateSeed()) {
    EXPECT_FALSE(PrefHasDefaultValue(prefs_, prefs::kVariationsCompressedSeed));
    EXPECT_FALSE(PrefHasDefaultValue(prefs_, prefs::kVariationsSeedSignature));
    EXPECT_FALSE(PrefHasDefaultValue(prefs_, prefs::kVariationsSeedMilestone));
  }
  EXPECT_FALSE(PrefHasDefaultValue(prefs_, prefs::kVariationsLastFetchTime));
  EXPECT_FALSE(PrefHasDefaultValue(prefs_, prefs::kVariationsSeedDate));
  CheckSafeSeedAndSeedPrefsAreSet(prefs_, seed_store);
}

TEST_P(LoadSeedDataControlAndDefaultGroupsTest, LoadSeed_EmptySeed) {
  // Loading an empty seed should return false.
  TestVariationsSeedStore seed_store(&prefs_, temp_dir_.GetPath());
  ASSERT_EQ(base::FieldTrialList::FindFullName(kSeedFileTrial), GetParam());
  std::string stored_seed_data;
  LoadSeedResult result =
      seed_store.GetSeedReaderWriterForTesting()->ReadSeedDataOnStartup(
          &stored_seed_data, nullptr);
  ASSERT_EQ(result, LoadSeedResult::kEmpty);

  base::HistogramTester histogram_tester;
  VariationsSeed loaded_seed;
  std::string loaded_seed_data;
  std::string loaded_base64_seed_signature;
  ASSERT_FALSE(seed_store.LoadSeedSync(&loaded_seed, &loaded_seed_data,
                                       &loaded_base64_seed_signature));

  // Verify metrics.
  histogram_tester.ExpectUniqueSample("Variations.SeedLoadResult",
                                      LoadSeedResult::kEmpty, 1);
}

TEST_P(LoadSeedDataControlAndDefaultGroupsTest, LoadSeed_IdenticalToSafeSeed) {
  // Store good seed data for safe seed, and store a sentinel value for the
  // latest seed, to verify that loading via the alias works.
  const std::string seed_data = SerializeSeed(CreateTestSeed());
  const std::string base64_seed_signature = "a test signature, ignored.";

  TestVariationsSeedStore seed_store(&prefs_, temp_dir_.GetPath());
  ASSERT_EQ(base::FieldTrialList::FindFullName(kSeedFileTrial), GetParam());
  seed_store.GetSeedReaderWriterForTesting()->StoreValidatedSeedInfo(
      ValidatedSeedInfo{
          .seed_data = kIdenticalToSafeSeedSentinel,
          .signature = base64_seed_signature,
          .milestone = 2,
          .seed_date = base::Time::Now(),
          .client_fetch_time = base::Time::Now(),
          .session_country_code = "us",
      });
  seed_store.GetSafeSeedReaderWriterForTesting()->StoreValidatedSeedInfo(
      ValidatedSeedInfo{
          .seed_data = seed_data,
          .signature = base64_seed_signature,
          .milestone = 1,
          .seed_date = base::Time::Now() - base::Days(1),
          .client_fetch_time = base::Time::Now() - base::Days(1),
          .session_country_code = "us",
          .permanent_country_code = "us",
      });

  base::HistogramTester histogram_tester;
  VariationsSeed loaded_seed;
  std::string loaded_seed_data;
  std::string loaded_base64_seed_signature;
  // Check that loading the seed works correctly.
  ASSERT_TRUE(seed_store.LoadSeedSync(&loaded_seed, &loaded_seed_data,
                                      &loaded_base64_seed_signature));

  // Verify metrics.
  histogram_tester.ExpectUniqueSample("Variations.SeedLoadResult",
                                      LoadSeedResult::kSuccess, 1);

  // Check that the loaded data is the same as the original.
  EXPECT_EQ(seed_data, SerializeSeed(loaded_seed));
  EXPECT_EQ(seed_data, loaded_seed_data);
  EXPECT_EQ(base64_seed_signature, loaded_base64_seed_signature);
}

TEST_P(LoadSeedDataControlAndDefaultGroupsTest, LoadSeed_CorruptGzip) {
  // Loading a corrupted compressed seed should return false.
  TestVariationsSeedStore seed_store(&prefs_, temp_dir_.GetPath());
  ASSERT_EQ(base::FieldTrialList::FindFullName(kSeedFileTrial), GetParam());
  SetAllSeedsAndSeedPrefsToNonDefaultValues(&prefs_, seed_store);
  std::string compressed_seed = Gzip("seed data");
  // Flip some bits to corrupt the data
  compressed_seed[5] ^= 0xFF;
  compressed_seed[10] ^= 0xFF;
  seed_store.GetSeedReaderWriterForTesting()->StoreValidatedSeedInfo(
      ValidatedSeedInfo{
          .seed_data = "this will be overwritten",
          .signature = "ignored signature",
          .milestone = 1,
          .seed_date = base::Time::Now(),
          .client_fetch_time = base::Time::Now(),
          .session_country_code = "us",
      });
  // Data is stored in base64 format in local state.
  if (ShouldUseLocalStateSeed()) {
    std::string base64_compressed_seed = base::Base64Encode(compressed_seed);
    seed_store.GetSeedReaderWriterForTesting()->StoreRawSeedForTesting(
        base64_compressed_seed);
  } else {
    seed_store.GetSeedReaderWriterForTesting()->StoreRawSeedForTesting(
        compressed_seed);
  }

  base::HistogramTester histogram_tester;
  ASSERT_FALSE(MakeSeedStoreLoadStoredSeed(seed_store));

  // Verify metrics and prefs.
  histogram_tester.ExpectUniqueSample("Variations.SeedLoadResult",
                                      LoadSeedResult::kCorruptGzip, 1);
  CheckRegularSeedAndSeedPrefsAreCleared(prefs_, seed_store);
  CheckSafeSeedAndSeedPrefsAreSet(prefs_, seed_store);

  // Verify session country is not cleared.
  EXPECT_THAT(GetSeedInfo(seed_store).session_country_code, Not(IsEmpty()));
}

TEST_P(LoadSeedDataControlAndDefaultGroupsTest,
       LoadSeed_ExceedsUncompressedSizeLimit) {
  // Loading a seed that exceeds the uncompressed size should return false.
  TestVariationsSeedStore seed_store(&prefs_, temp_dir_.GetPath());
  ASSERT_EQ(base::FieldTrialList::FindFullName(kSeedFileTrial), GetParam());
  SetAllSeedsAndSeedPrefsToNonDefaultValues(&prefs_, seed_store);
  seed_store.GetSeedReaderWriterForTesting()->StoreValidatedSeedInfo(
      ValidatedSeedInfo{
          .seed_data = CreateTooLargeData(),
          .signature = "ignored signature",
          .milestone = 1,
          .seed_date = base::Time::Now(),
          .client_fetch_time = base::Time::Now(),
          .session_country_code = "us",
      });

  base::HistogramTester histogram_tester;
  VariationsSeed loaded_seed;
  std::string loaded_seed_data;
  std::string loaded_base64_seed_signature;
  ASSERT_FALSE(seed_store.LoadSeedSync(&loaded_seed, &loaded_seed_data,
                                       &loaded_base64_seed_signature));

  // Verify metrics and prefs.
  histogram_tester.ExpectUniqueSample(
      "Variations.SeedLoadResult",
      LoadSeedResult::kExceedsUncompressedSizeLimit, 1);
  CheckRegularSeedAndSeedPrefsAreCleared(prefs_, seed_store);
  CheckSafeSeedAndSeedPrefsAreSet(prefs_, seed_store);

  // Verify session country is not cleared.
  EXPECT_THAT(GetSeedInfo(seed_store).session_country_code, Not(IsEmpty()));
}

TEST_P(LoadSeedDataControlAndDefaultGroupsTest, LoadSeedAsync_ValidSeed) {
  // Store good seed data to test if loading from prefs works.
  const std::string seed_data = SerializeSeed(CreateTestSeed());
  const std::string base64_seed_signature = "a test signature, ignored.";

  TestVariationsSeedStore seed_store(&prefs_, temp_dir_.GetPath());
  ASSERT_EQ(base::FieldTrialList::FindFullName(kSeedFileTrial), GetParam());
  seed_store.GetSeedReaderWriterForTesting()->StoreValidatedSeedInfo(
      ValidatedSeedInfo{
          .seed_data = seed_data,
          .signature = base64_seed_signature,
          .milestone = 1,
          .seed_date = base::Time::Now(),
          .client_fetch_time = base::Time::Now(),
          .session_country_code = "us",
      });

  base::HistogramTester histogram_tester;
  VariationsSeed loaded_seed;
  // Check that loading a seed works correctly.
  base::RunLoop run_loop;
  seed_store.LoadSeed(base::BindLambdaForTesting(
      [&seed_data, &base64_seed_signature, &run_loop](
          std::string loaded_seed_data, std::string loaded_seed_signature,
          bool success, VariationsSeed seed) {
        EXPECT_TRUE(success);
        EXPECT_EQ(loaded_seed_data, seed_data);
        EXPECT_EQ(loaded_seed_signature, base64_seed_signature);
        EXPECT_EQ(seed.SerializeAsString(), seed_data);
        run_loop.Quit();
      }));
  run_loop.Run();

  // Verify metrics.
  histogram_tester.ExpectUniqueSample("Variations.SeedLoadResult",
                                      LoadSeedResult::kSuccess, 1);
}

TEST_P(LoadSeedDataControlAndDefaultGroupsTest,
       LoadSeedAsync_InvalidSignature) {
  const std::string seed_data = SerializeSeed(CreateTestSeed());

  // Loading a valid seed with an invalid signature should return false and
  // clear seeds and associated prefs when signature verification is enabled.
  TestVariationsSeedStore seed_store(&prefs_, temp_dir_.GetPath(),
                                     /*signature_verification_needed=*/true);
  ASSERT_EQ(base::FieldTrialList::FindFullName(kSeedFileTrial), GetParam());
  SetAllSeedsAndSeedPrefsToNonDefaultValues(&prefs_, seed_store);
  seed_store.GetSeedReaderWriterForTesting()->StoreValidatedSeedInfo(
      ValidatedSeedInfo{
          .seed_data = seed_data,
          .signature = "a deeply compromised signature.",
          .milestone = 1,
          .seed_date = base::Time::Now(),
          .client_fetch_time = base::Time::Now(),
          .session_country_code = "us",
      });

  base::HistogramTester histogram_tester;
  VariationsSeed loaded_seed;
  // Check that loading a seed fails.
  base::RunLoop run_loop;
  seed_store.LoadSeed(base::BindLambdaForTesting(
      [&run_loop](std::string seed_data, std::string seed_signature,
                  bool success, VariationsSeed seed) {
        EXPECT_FALSE(success);
        run_loop.Quit();
      }));
  run_loop.Run();

  // Verify metrics and prefs.
  histogram_tester.ExpectUniqueSample("Variations.SeedLoadResult",
                                      LoadSeedResult::kInvalidSignature, 1);
  CheckRegularSeedAndSeedPrefsAreCleared(prefs_, seed_store);
  CheckSafeSeedAndSeedPrefsAreSet(prefs_, seed_store);

  // Verify session country is not cleared.
  EXPECT_THAT(GetSeedInfo(seed_store).session_country_code, Not(IsEmpty()));
}

TEST_P(LoadSeedDataControlAndDefaultGroupsTest, LoadSeedAsync_EmptySeed) {
  // Loading an empty seed should return false.
  TestVariationsSeedStore seed_store(&prefs_, temp_dir_.GetPath());
  ASSERT_EQ(base::FieldTrialList::FindFullName(kSeedFileTrial), GetParam());
  std::string stored_seed_data;
  LoadSeedResult result =
      seed_store.GetSeedReaderWriterForTesting()->ReadSeedDataOnStartup(
          &stored_seed_data, nullptr);
  ASSERT_EQ(result, LoadSeedResult::kEmpty);

  base::HistogramTester histogram_tester;
  VariationsSeed loaded_seed;

  // Verify that loading a seed fails.
  base::RunLoop run_loop;
  seed_store.LoadSeed(base::BindLambdaForTesting(
      [&run_loop](std::string seed_data, std::string seed_signature,
                  bool success, VariationsSeed seed) {
        EXPECT_FALSE(success);
        run_loop.Quit();
      }));
  run_loop.Run();

  // Verify metrics.
  histogram_tester.ExpectUniqueSample("Variations.SeedLoadResult",
                                      LoadSeedResult::kEmpty, 1);
}

// Coverage for base64 decoding issues is N/A to treatment-group clients because
// they don't use base64 encoding.
TEST_P(LoadSeedDataControlAndDefaultGroupsTest,
       LoadSeed_Base64DecodingFailure) {
  // Loading a non-base64-encoded seed should return false.
  TestVariationsSeedStore seed_store(&prefs_, temp_dir_.GetPath());
  ASSERT_EQ(base::FieldTrialList::FindFullName(kSeedFileTrial), GetParam());
  SetAllSeedsAndSeedPrefsToNonDefaultValues(&prefs_, seed_store);
  seed_store.GetSeedReaderWriterForTesting()->StoreValidatedSeedInfo(
      ValidatedSeedInfo{
          .seed_data = "this will be overwritten",
          .signature = "ignored signature",
          .milestone = 1,
          .seed_date = base::Time::Now(),
          .client_fetch_time = base::Time::Now(),
          .session_country_code = "us",
      });
  seed_store.GetSeedReaderWriterForTesting()->StoreRawSeedForTesting(
      "this is not base64");

  base::HistogramTester histogram_tester;
  ASSERT_FALSE(MakeSeedStoreLoadStoredSeed(seed_store));

  // Verify metrics and prefs.
  histogram_tester.ExpectUniqueSample("Variations.SeedLoadResult",
                                      LoadSeedResult::kCorruptBase64, 1);
  CheckRegularSeedAndSeedPrefsAreCleared(prefs_, seed_store);
  CheckSafeSeedAndSeedPrefsAreSet(prefs_, seed_store);

  // Verify session country is not cleared.
  EXPECT_THAT(GetSeedInfo(seed_store).session_country_code, Not(IsEmpty()));
}

class LoadSeedDataSeedFilesGroupBase : public SeedStoreGroupTestBase {
 public:
  explicit LoadSeedDataSeedFilesGroupBase(
      const SeedFieldsPrefs& seed_fields_prefs)
      : SeedStoreGroupTestBase(seed_fields_prefs, kSeedFilesGroup) {}

  void WriteRegularSeedToFile(std::string_view seed_data) {
    ASSERT_TRUE(
        base::WriteFile(temp_dir_.GetPath().Append(kSeedFilename), seed_data));
  }

  void WriteSafeSeedToFile(std::string_view seed_data) {
    ASSERT_TRUE(base::WriteFile(temp_dir_.GetPath().Append(kSafeSeedFilename),
                                seed_data));
  }

  void WriteTestRegularSeedToFile() {
    WriteRegularSeedToFile(SeedReaderWriter::CompressForSeedFileForTesting(
        CreateTestStoredSeedInfo().SerializeAsString()));
  }

  void WriteTestSafeSeedToFile() {
    WriteSafeSeedToFile(SeedReaderWriter::CompressForSeedFileForTesting(
        CreateTestStoredSeedInfo().SerializeAsString()));
  }

  StoredSeedInfo CreateTestStoredSeedInfo() {
    StoredSeedInfo stored_seed_info;
    stored_seed_info.set_data("seed data");
    stored_seed_info.set_signature("a test signature, ignored.");
    stored_seed_info.set_milestone(1);
    stored_seed_info.set_seed_date(12345);
    stored_seed_info.set_client_fetch_time(12345);
    stored_seed_info.set_session_country_code("us");
    stored_seed_info.set_permanent_country_code("us");
    return stored_seed_info;
  }
};

class LoadSeedDataSeedFilesGroupTest : public LoadSeedDataSeedFilesGroupBase {
 public:
  LoadSeedDataSeedFilesGroupTest()
      : LoadSeedDataSeedFilesGroupBase(kRegularSeedFieldsPrefs) {}
};

TEST_F(LoadSeedDataSeedFilesGroupTest, LoadSeed_ValidSeed) {
  ASSERT_EQ(base::FieldTrialList::FindFullName(kSeedFileTrial),
            kSeedFilesGroup);

  const std::string serialized_seed = SerializeSeed(CreateTestSeed());

  StoredSeedInfo stored_seed_info;
  stored_seed_info.set_data(serialized_seed);
  stored_seed_info.set_signature("a test signature, ignored.");
  stored_seed_info.set_milestone(1);
  stored_seed_info.set_seed_date(12345);
  stored_seed_info.set_client_fetch_time(12345);
  stored_seed_info.set_session_country_code("us");
  stored_seed_info.set_permanent_country_code("us");
  WriteRegularSeedToFile(SeedReaderWriter::CompressForSeedFileForTesting(
      stored_seed_info.SerializeAsString()));

  base::HistogramTester histogram_tester;
  TestVariationsSeedStore seed_store(
      /*local_state=*/&prefs_, /*seed_file_dir=*/temp_dir_.GetPath(),
      /*signature_verification_needed=*/false, /*initial_seed=*/nullptr,
      /*use_first_run_prefs=*/true,
      /*channel=*/version_info::Channel::DEV);

  // Verify metrics.
  histogram_tester.ExpectUniqueSample("Variations.SeedFileReadResult.Latest",
                                      LoadSeedResult::kSuccess, 1);

  VariationsSeed loaded_seed;
  std::string loaded_seed_data;
  std::string loaded_seed_signature;
  ASSERT_TRUE(seed_store.LoadSeedSync(&loaded_seed, &loaded_seed_data,
                                      &loaded_seed_signature));

  // Check that the loaded data is the same as the original.
  EXPECT_EQ(loaded_seed.SerializeAsString(), serialized_seed);
  EXPECT_EQ(loaded_seed_data, serialized_seed);
  EXPECT_EQ(loaded_seed_signature, stored_seed_info.signature());
}

TEST_F(LoadSeedDataSeedFilesGroupTest, LoadSeed_InvalidSignature) {
  ASSERT_EQ(base::FieldTrialList::FindFullName(kSeedFileTrial),
            kSeedFilesGroup);
  const std::string serialized_seed = SerializeSeed(CreateTestSeed());
  StoredSeedInfo stored_seed_info;
  stored_seed_info.set_data(serialized_seed);
  stored_seed_info.set_signature("a deeply compromised signature.");
  stored_seed_info.set_milestone(1);
  stored_seed_info.set_seed_date(12345);
  stored_seed_info.set_client_fetch_time(12345);
  stored_seed_info.set_session_country_code("us");
  stored_seed_info.set_permanent_country_code("us");
  WriteRegularSeedToFile(SeedReaderWriter::CompressForSeedFileForTesting(
      stored_seed_info.SerializeAsString()));
  WriteTestSafeSeedToFile();

  prefs_.SetString(prefs::kVariationsSafeSeedLocale, "en-US");

  base::HistogramTester histogram_tester;
  TestVariationsSeedStore seed_store(
      /*local_state=*/&prefs_, /*seed_file_dir=*/temp_dir_.GetPath(),
      /*signature_verification_needed=*/true, /*initial_seed=*/nullptr,
      /*use_first_run_prefs=*/true,
      /*channel=*/version_info::Channel::DEV);

  // The seed file is read successfully.
  histogram_tester.ExpectUniqueSample("Variations.SeedFileReadResult.Latest",
                                      LoadSeedResult::kSuccess, 1);

  VariationsSeed loaded_seed;
  std::string loaded_seed_data;
  std::string loaded_seed_signature;
  // But loading the seed fails because of the invalid signature.
  ASSERT_FALSE(seed_store.LoadSeedSync(&loaded_seed, &loaded_seed_data,
                                       &loaded_seed_signature));
  histogram_tester.ExpectUniqueSample("Variations.SeedLoadResult",
                                      LoadSeedResult::kInvalidSignature, 1);

  CheckRegularSeedAndSeedPrefsAreCleared(prefs_, seed_store);
  CheckSafeSeedAndSeedPrefsAreSet(prefs_, seed_store);

  // Verify session country is not cleared.
  EXPECT_THAT(GetSeedInfo(seed_store).session_country_code, Not(IsEmpty()));
}

TEST_F(LoadSeedDataSeedFilesGroupTest, LoadSeed_InvalidStoredSeedInfoProto) {
  ASSERT_EQ(base::FieldTrialList::FindFullName(kSeedFileTrial),
            kSeedFilesGroup);
  WriteRegularSeedToFile(SeedReaderWriter::CompressForSeedFileForTesting(
      "fun fact: this is not a proto"));
  WriteTestSafeSeedToFile();

  prefs_.SetString(prefs::kVariationsSafeSeedLocale, "en-US");

  base::HistogramTester histogram_tester;
  TestVariationsSeedStore seed_store(
      /*local_state=*/&prefs_, /*seed_file_dir=*/temp_dir_.GetPath(),
      /*signature_verification_needed=*/false, /*initial_seed=*/nullptr,
      /*use_first_run_prefs=*/true,
      /*channel=*/version_info::Channel::DEV);

  // Error when parsing the StoredSeedInfo proto.
  histogram_tester.ExpectUniqueSample(
      "Variations.SeedFileReadResult.Latest",
      LoadSeedResult::kSeedInfoParseToProtoError, 1);

  VariationsSeed loaded_seed;
  std::string loaded_seed_data;
  std::string loaded_seed_signature;
  // The seed is empty, so loading the seed fails.
  ASSERT_FALSE(seed_store.LoadSeedSync(&loaded_seed, &loaded_seed_data,
                                       &loaded_seed_signature));

  CheckRegularSeedAndSeedPrefsAreCleared(prefs_, seed_store);
  CheckSafeSeedAndSeedPrefsAreSet(prefs_, seed_store);
}

TEST_F(LoadSeedDataSeedFilesGroupTest, LoadSeed_InvalidSeedDataProto) {
  ASSERT_EQ(base::FieldTrialList::FindFullName(kSeedFileTrial),
            kSeedFilesGroup);
  StoredSeedInfo stored_seed_info;
  stored_seed_info.set_data("fun fact: this is not a proto");
  stored_seed_info.set_signature("a test signature, ignored.");
  stored_seed_info.set_milestone(1);
  stored_seed_info.set_seed_date(12345);
  stored_seed_info.set_client_fetch_time(12345);
  stored_seed_info.set_session_country_code("us");
  stored_seed_info.set_permanent_country_code("us");
  WriteRegularSeedToFile(SeedReaderWriter::CompressForSeedFileForTesting(
      stored_seed_info.SerializeAsString()));
  WriteTestSafeSeedToFile();

  prefs_.SetString(prefs::kVariationsSafeSeedLocale, "en-US");

  base::HistogramTester histogram_tester;
  TestVariationsSeedStore seed_store(
      /*local_state=*/&prefs_, /*seed_file_dir=*/temp_dir_.GetPath(),
      /*signature_verification_needed=*/false, /*initial_seed=*/nullptr,
      /*use_first_run_prefs=*/true,
      /*channel=*/version_info::Channel::DEV);

  // The seed file is read successfully.
  histogram_tester.ExpectUniqueSample("Variations.SeedFileReadResult.Latest",
                                      LoadSeedResult::kSuccess, 1);

  VariationsSeed loaded_seed;
  std::string loaded_seed_data;
  std::string loaded_seed_signature;
  // But loading the seed fails because of the invalid seed data.
  ASSERT_FALSE(seed_store.LoadSeedSync(&loaded_seed, &loaded_seed_data,
                                       &loaded_seed_signature));

  histogram_tester.ExpectUniqueSample("Variations.SeedLoadResult",
                                      LoadSeedResult::kCorruptProtobuf, 1);
  CheckRegularSeedAndSeedPrefsAreCleared(prefs_, seed_store);
  CheckSafeSeedAndSeedPrefsAreSet(prefs_, seed_store);

  // Verify session country is not cleared.
  EXPECT_THAT(GetSeedInfo(seed_store).session_country_code, Not(IsEmpty()));
}

TEST_F(LoadSeedDataSeedFilesGroupTest, LoadSeed_RejectEmptySignature) {
  ASSERT_EQ(base::FieldTrialList::FindFullName(kSeedFileTrial),
            kSeedFilesGroup);
  StoredSeedInfo stored_seed_info;
  stored_seed_info.set_data(SerializeSeed(CreateTestSeed()));
  stored_seed_info.set_signature("");
  stored_seed_info.set_milestone(1);
  stored_seed_info.set_seed_date(12345);
  stored_seed_info.set_client_fetch_time(12345);
  stored_seed_info.set_session_country_code("us");
  stored_seed_info.set_permanent_country_code("us");
  WriteRegularSeedToFile(SeedReaderWriter::CompressForSeedFileForTesting(
      stored_seed_info.SerializeAsString()));
  WriteTestSafeSeedToFile();

  prefs_.SetString(prefs::kVariationsSafeSeedLocale, "en-US");

  base::HistogramTester histogram_tester;
  TestVariationsSeedStore seed_store(
      /*local_state=*/&prefs_, /*seed_file_dir=*/temp_dir_.GetPath(),
      /*signature_verification_needed=*/true, /*initial_seed=*/nullptr,
      /*use_first_run_prefs=*/true,
      /*channel=*/version_info::Channel::DEV);

  // The seed file is read successfully.
  histogram_tester.ExpectUniqueSample("Variations.SeedFileReadResult.Latest",
                                      LoadSeedResult::kSuccess, 1);

  VariationsSeed loaded_seed;
  std::string loaded_seed_data;
  std::string loaded_seed_signature;
  // But loading the seed fails because of the empty signature.
  ASSERT_FALSE(seed_store.LoadSeedSync(&loaded_seed, &loaded_seed_data,
                                       &loaded_seed_signature));

  histogram_tester.ExpectUniqueSample("Variations.SeedLoadResult",
                                      LoadSeedResult::kInvalidSignature, 1);
  CheckRegularSeedAndSeedPrefsAreCleared(prefs_, seed_store);
  CheckSafeSeedAndSeedPrefsAreSet(prefs_, seed_store);

  // Verify session country is not cleared.
  EXPECT_THAT(GetSeedInfo(seed_store).session_country_code, Not(IsEmpty()));
}

TEST_F(LoadSeedDataSeedFilesGroupTest, LoadSeed_AcceptEmptySignature) {
  ASSERT_EQ(base::FieldTrialList::FindFullName(kSeedFileTrial),
            kSeedFilesGroup);

  // Loading a valid seed with an empty signature should succeed iff
  // switches::kAcceptEmptySeedSignatureForTesting is on the command line.
  base::test::ScopedCommandLine scoped_command_line;
  scoped_command_line.GetProcessCommandLine()->AppendSwitch(
      switches::kAcceptEmptySeedSignatureForTesting);

  StoredSeedInfo stored_seed_info;
  stored_seed_info.set_data(SerializeSeed(CreateTestSeed()));
  stored_seed_info.set_signature("");
  stored_seed_info.set_milestone(1);
  stored_seed_info.set_seed_date(12345);
  stored_seed_info.set_client_fetch_time(12345);
  stored_seed_info.set_session_country_code("us");
  stored_seed_info.set_permanent_country_code("us");
  WriteRegularSeedToFile(SeedReaderWriter::CompressForSeedFileForTesting(
      stored_seed_info.SerializeAsString()));
  WriteTestSafeSeedToFile();

  prefs_.SetString(prefs::kVariationsSafeSeedLocale, "en-US");

  base::HistogramTester histogram_tester;
  TestVariationsSeedStore seed_store(
      /*local_state=*/&prefs_, /*seed_file_dir=*/temp_dir_.GetPath(),
      /*signature_verification_needed=*/true, /*initial_seed=*/nullptr,
      /*use_first_run_prefs=*/true,
      /*channel=*/version_info::Channel::DEV);

  histogram_tester.ExpectUniqueSample("Variations.SeedFileReadResult.Latest",
                                      LoadSeedResult::kSuccess, 1);

  VariationsSeed loaded_seed;
  std::string loaded_seed_data;
  std::string loaded_seed_signature;
  // Loading the seed succeeds even though the signature is empty.
  ASSERT_TRUE(seed_store.LoadSeedSync(&loaded_seed, &loaded_seed_data,
                                      &loaded_seed_signature));

  histogram_tester.ExpectUniqueSample("Variations.SeedLoadResult",
                                      LoadSeedResult::kSuccess, 1);
  CheckSafeSeedAndSeedPrefsAreSet(prefs_, seed_store);
}

TEST_F(LoadSeedDataSeedFilesGroupTest, LoadSeed_IdenticalToSafeSeed) {
  ASSERT_EQ(base::FieldTrialList::FindFullName(kSeedFileTrial),
            kSeedFilesGroup);
  StoredSeedInfo stored_seed_info;
  stored_seed_info.set_data(kIdenticalToSafeSeedSentinel);
  stored_seed_info.set_signature("a test signature, ignored.");
  stored_seed_info.set_milestone(1);
  stored_seed_info.set_seed_date(12345);
  stored_seed_info.set_client_fetch_time(12345);
  stored_seed_info.set_session_country_code("us");
  stored_seed_info.set_permanent_country_code("us");
  WriteRegularSeedToFile(SeedReaderWriter::CompressForSeedFileForTesting(
      stored_seed_info.SerializeAsString()));
  std::string serialized_seed = SerializeSeed(CreateTestSeed());
  stored_seed_info.set_data(serialized_seed);
  WriteSafeSeedToFile(SeedReaderWriter::CompressForSeedFileForTesting(
      stored_seed_info.SerializeAsString()));

  prefs_.SetString(prefs::kVariationsSafeSeedLocale, "en-US");

  base::HistogramTester histogram_tester;
  TestVariationsSeedStore seed_store(
      /*local_state=*/&prefs_, /*seed_file_dir=*/temp_dir_.GetPath(),
      /*signature_verification_needed=*/false, /*initial_seed=*/nullptr,
      /*use_first_run_prefs=*/true,
      /*channel=*/version_info::Channel::DEV);

  histogram_tester.ExpectUniqueSample("Variations.SeedFileReadResult.Latest",
                                      LoadSeedResult::kSuccess, 1);

  VariationsSeed loaded_seed;
  std::string loaded_seed_data;
  std::string loaded_seed_signature;
  // Loading the seed succeeds. It has the same data as the safe seed.
  ASSERT_TRUE(seed_store.LoadSeedSync(&loaded_seed, &loaded_seed_data,
                                      &loaded_seed_signature));
  EXPECT_EQ(loaded_seed_data, serialized_seed);
  EXPECT_EQ(loaded_seed.SerializeAsString(), serialized_seed);

  histogram_tester.ExpectUniqueSample("Variations.SeedLoadResult",
                                      LoadSeedResult::kSuccess, 1);
  CheckRegularSeedAndSeedPrefsAreSet(prefs_, seed_store);
  CheckSafeSeedAndSeedPrefsAreSet(prefs_, seed_store);
}

TEST_F(LoadSeedDataSeedFilesGroupTest, LoadSeed_CorruptGzip) {
  ASSERT_EQ(base::FieldTrialList::FindFullName(kSeedFileTrial),
            kSeedFilesGroup);
  std::string compressed_seed_info =
      SeedReaderWriter::CompressForSeedFileForTesting("seed data");
  // Flip some bits to corrupt the data
  compressed_seed_info[5] ^= 0xFF;
  compressed_seed_info[10] ^= 0xFF;

  WriteRegularSeedToFile(compressed_seed_info);
  WriteTestSafeSeedToFile();

  prefs_.SetString(prefs::kVariationsSafeSeedLocale, "en-US");

  base::HistogramTester histogram_tester;
  TestVariationsSeedStore seed_store(
      /*local_state=*/&prefs_, /*seed_file_dir=*/temp_dir_.GetPath(),
      /*signature_verification_needed=*/false, /*initial_seed=*/nullptr,
      /*use_first_run_prefs=*/true,
      /*channel=*/version_info::Channel::DEV);

  {
#if BUILDFLAG(IS_ANDROID)
    LoadSeedResult expected_result = LoadSeedResult::kCorruptGzip;
#else
    LoadSeedResult expected_result = LoadSeedResult::kCorruptZstd;
#endif
    histogram_tester.ExpectUniqueSample("Variations.SeedFileReadResult.Latest",
                                        expected_result, 1);
  }

  VariationsSeed loaded_seed;
  std::string loaded_seed_data;
  std::string loaded_seed_signature;
  ASSERT_FALSE(seed_store.LoadSeedSync(&loaded_seed, &loaded_seed_data,
                                       &loaded_seed_signature));

  histogram_tester.ExpectUniqueSample("Variations.SeedLoadResult",
                                      LoadSeedResult::kEmpty, 1);
  CheckRegularSeedAndSeedPrefsAreCleared(prefs_, seed_store);
  CheckSafeSeedAndSeedPrefsAreSet(prefs_, seed_store);
}

TEST_F(LoadSeedDataSeedFilesGroupTest, LoadSeed_ExceedsUncompressedSizeLimit) {
  ASSERT_EQ(base::FieldTrialList::FindFullName(kSeedFileTrial),
            kSeedFilesGroup);
  WriteRegularSeedToFile(
      SeedReaderWriter::CompressForSeedFileForTesting(CreateTooLargeData()));
  WriteTestSafeSeedToFile();

  prefs_.SetString(prefs::kVariationsSafeSeedLocale, "en-US");

  base::HistogramTester histogram_tester;
  TestVariationsSeedStore seed_store(
      /*local_state=*/&prefs_, /*seed_file_dir=*/temp_dir_.GetPath(),
      /*signature_verification_needed=*/false, /*initial_seed=*/nullptr,
      /*use_first_run_prefs=*/true,
      /*channel=*/version_info::Channel::DEV);

  histogram_tester.ExpectUniqueSample(
      "Variations.SeedFileReadResult.Latest",
      LoadSeedResult::kExceedsUncompressedSizeLimit, 1);

  VariationsSeed loaded_seed;
  std::string loaded_seed_data;
  std::string loaded_seed_signature;
  ASSERT_FALSE(seed_store.LoadSeedSync(&loaded_seed, &loaded_seed_data,
                                       &loaded_seed_signature));

  histogram_tester.ExpectUniqueSample("Variations.SeedLoadResult",
                                      LoadSeedResult::kEmpty, 1);
  CheckRegularSeedAndSeedPrefsAreCleared(prefs_, seed_store);
  CheckSafeSeedAndSeedPrefsAreSet(prefs_, seed_store);
}

TEST_F(LoadSeedDataSeedFilesGroupTest, LoadSeedAsync_ValidSeed) {
  ASSERT_EQ(base::FieldTrialList::FindFullName(kSeedFileTrial),
            kSeedFilesGroup);

  const std::string serialized_seed = SerializeSeed(CreateTestSeed());
  StoredSeedInfo stored_seed_info;
  stored_seed_info.set_data(serialized_seed);
  stored_seed_info.set_signature("a test signature, ignored.");
  stored_seed_info.set_milestone(1);
  stored_seed_info.set_seed_date(12345);
  stored_seed_info.set_client_fetch_time(12345);
  stored_seed_info.set_session_country_code("us");
  stored_seed_info.set_permanent_country_code("us");
  WriteRegularSeedToFile(SeedReaderWriter::CompressForSeedFileForTesting(
      stored_seed_info.SerializeAsString()));
  WriteTestSafeSeedToFile();

  prefs_.SetString(prefs::kVariationsSafeSeedLocale, "en-US");

  base::HistogramTester histogram_tester;
  TestVariationsSeedStore seed_store(
      /*local_state=*/&prefs_, /*seed_file_dir=*/temp_dir_.GetPath(),
      /*signature_verification_needed=*/false, /*initial_seed=*/nullptr,
      /*use_first_run_prefs=*/true,
      /*channel=*/version_info::Channel::DEV);

  histogram_tester.ExpectUniqueSample("Variations.SeedFileReadResult.Latest",
                                      LoadSeedResult::kSuccess, 1);

  VariationsSeed loaded_seed;
  // Check that loading a seed works correctly.
  base::RunLoop run_loop;
  seed_store.LoadSeed(base::BindLambdaForTesting(
      [&run_loop, &stored_seed_info](std::string loaded_seed_data,
                                     std::string loaded_seed_signature,
                                     bool success, VariationsSeed seed) {
        EXPECT_TRUE(success);
        EXPECT_EQ(loaded_seed_data, stored_seed_info.data());
        EXPECT_EQ(loaded_seed_signature, stored_seed_info.signature());
        EXPECT_EQ(seed.SerializeAsString(), stored_seed_info.data());
        run_loop.Quit();
      }));
  run_loop.Run();

  // Verify metrics.
  histogram_tester.ExpectUniqueSample("Variations.SeedLoadResult",
                                      LoadSeedResult::kSuccess, 1);
}

TEST_F(LoadSeedDataSeedFilesGroupTest, LoadSeedAsync_InvalidSignature) {
  ASSERT_EQ(base::FieldTrialList::FindFullName(kSeedFileTrial),
            kSeedFilesGroup);

  WriteTestRegularSeedToFile();
  WriteTestSafeSeedToFile();

  prefs_.SetString(prefs::kVariationsSafeSeedLocale, "en-US");

  base::HistogramTester histogram_tester;
  TestVariationsSeedStore seed_store(
      /*local_state=*/&prefs_, /*seed_file_dir=*/temp_dir_.GetPath(),
      /*signature_verification_needed=*/true, /*initial_seed=*/nullptr,
      /*use_first_run_prefs=*/true,
      /*channel=*/version_info::Channel::DEV);

  histogram_tester.ExpectUniqueSample("Variations.SeedFileReadResult.Latest",
                                      LoadSeedResult::kSuccess, 1);

  VariationsSeed loaded_seed;
  // Check that loading a seed fails because the signature is invalid.
  base::RunLoop run_loop;
  seed_store.LoadSeed(base::BindLambdaForTesting(
      [&run_loop](std::string loaded_seed_data,
                  std::string loaded_seed_signature, bool success,
                  VariationsSeed seed) {
        EXPECT_FALSE(success);
        run_loop.Quit();
      }));
  run_loop.Run();

  // Verify metrics.
  histogram_tester.ExpectUniqueSample("Variations.SeedLoadResult",
                                      LoadSeedResult::kInvalidSignature, 1);
}

struct StoreSeedDataTestParams {
  using TupleT = std::tuple<bool, std::string_view>;
  const bool require_synchronous_stores;
  std::string_view field_trial_group;

  explicit StoreSeedDataTestParams(const TupleT& t)
      : require_synchronous_stores(std::get<0>(t)),
        field_trial_group(std::get<1>(t)) {}
};

class StoreSeedDataGroupTest
    : public SeedStoreGroupTestBase,
      public ::testing::WithParamInterface<StoreSeedDataTestParams> {
 public:
  StoreSeedDataGroupTest()
      : SeedStoreGroupTestBase(kRegularSeedFieldsPrefs,
                               GetParam().field_trial_group) {}
  ~StoreSeedDataGroupTest() override = default;

  bool RequireSynchronousStores() const {
    return GetParam().require_synchronous_stores;
  }

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
        /*done_callback=*/base::BindOnce(
            &StoreSeedDataGroupTest::OnSeedStoreResult, base::Unretained(this),
            run_loop.QuitClosure()),
        seed_data, /*base64_seed_signature=*/std::string(), params.country_code,
        base::Time::Now(), params.is_delta_compressed,
        params.is_gzip_compressed, RequireSynchronousStores());
    // If we're testing synchronous stores, we shouldn't issue a Run() call so
    // that the test verifies that the operation completed synchronously.
    if (!RequireSynchronousStores()) {
      run_loop.Run();
    }
    return store_success_;
  }

  VariationsSeed GetStoredSeed() { return stored_seed_; }

 protected:
  bool store_success_ = false;
  VariationsSeed stored_seed_;

 private:
  void OnSeedStoreResult(base::RepeatingClosure quit_closure,
                         bool store_success,
                         VariationsSeed seed) {
    store_success_ = store_success;
    stored_seed_.Swap(&seed);
    quit_closure.Run();
  }
};

class StoreSeedDataSeedFilesGroupTest : public StoreSeedDataGroupTest {};

class StoreSeedDataControlAndLocalStateOnlyGroupTest
    : public StoreSeedDataGroupTest {};

class StoreSeedDataAllGroupsTest : public StoreSeedDataGroupTest {};

INSTANTIATE_TEST_SUITE_P(
    All,
    StoreSeedDataSeedFilesGroupTest,
    ::testing::ConvertGenerator<StoreSeedDataTestParams::TupleT>(
        ::testing::Combine(::testing::Bool(),
                           ::testing::Values(kSeedFilesGroup))));

// Verifies that clients in SeedFiles trial group write latest seeds to a seed
// file.
TEST_P(StoreSeedDataSeedFilesGroupTest, StoreSeedData) {
  // Initialize SeedStore with test local state prefs and SeedReaderWriter.
  TestVariationsSeedStore seed_store(&prefs_, temp_dir_.GetPath());
  ASSERT_EQ(base::FieldTrialList::FindFullName(kSeedFileTrial),
            GetParam().field_trial_group);
  seed_store.SetSeedReaderWriterForTesting(std::move(seed_reader_writer_));

  // Store seed and force write for SeedReaderWriter.
  const std::string serialized_seed = SerializeSeed(CreateTestSeed());
  ASSERT_TRUE(StoreSeedData(seed_store, serialized_seed));
  timer_.Fire();
  file_writer_thread_.FlushForTesting();
  base::RunLoop().RunUntilIdle();

  EXPECT_EQ(GetStoredSeed().SerializeAsString(), serialized_seed);
}

INSTANTIATE_TEST_SUITE_P(
    All,
    StoreSeedDataControlAndLocalStateOnlyGroupTest,
    ::testing::ConvertGenerator<StoreSeedDataTestParams::TupleT>(
        ::testing::Combine(
            ::testing::Bool(),
            ::testing::Values(kControlGroup, kDefaultGroup, kNoGroup))));

// Verifies that clients in the control group and those using local state only
// write latest seeds only to local state prefs.
TEST_P(StoreSeedDataControlAndLocalStateOnlyGroupTest, StoreSeedData) {
  // Initialize SeedStore with test local state prefs and SeedReaderWriter.
  TestVariationsSeedStore seed_store(&prefs_, temp_dir_.GetPath());
  ASSERT_EQ(base::FieldTrialList::FindFullName(kSeedFileTrial),
            GetParam().field_trial_group);
  seed_store.SetSeedReaderWriterForTesting(std::move(seed_reader_writer_));

  const std::string serialized_seed = SerializeSeed(CreateTestSeed());
  ASSERT_TRUE(StoreSeedData(seed_store, serialized_seed));

  // Make sure seed in local state prefs matches the one created.
  EXPECT_EQ(prefs_.GetString(prefs::kVariationsCompressedSeed),
            GzipAndBase64Encode(serialized_seed));

  // Check there's no pending write to a seed file and that it was not created.
  EXPECT_FALSE(timer_.IsRunning());
  EXPECT_FALSE(base::PathExists(temp_seed_file_path_));
}

INSTANTIATE_TEST_SUITE_P(
    All,
    StoreSeedDataAllGroupsTest,
    ::testing::ConvertGenerator<StoreSeedDataTestParams::TupleT>(
        ::testing::Combine(::testing::Bool(),
                           ::testing::Values(kSeedFilesGroup,
                                             kControlGroup,
                                             kDefaultGroup,
                                             kNoGroup))));

// Verifies that invalid latest seeds are not stored.
TEST_P(StoreSeedDataAllGroupsTest, StoreSeedData_InvalidSeed) {
  // Initialize SeedStore with test prefs and SeedReaderWriter.
  TestVariationsSeedStore seed_store(&prefs_, temp_dir_.GetPath());
  ASSERT_EQ(base::FieldTrialList::FindFullName(kSeedFileTrial),
            GetParam().field_trial_group);
  seed_store.SetSeedReaderWriterForTesting(std::move(seed_reader_writer_));

  // Check if trying to store a bad seed leaves the local state prefs unchanged.
  ASSERT_FALSE(StoreSeedData(seed_store, "should fail"));
  EXPECT_TRUE(PrefHasDefaultValue(prefs_, prefs::kVariationsCompressedSeed));

  // Check there's no pending write to a seed file and that it was not created.
  EXPECT_FALSE(timer_.IsRunning());
  EXPECT_FALSE(base::PathExists(temp_seed_file_path_));
}

TEST_P(StoreSeedDataAllGroupsTest, ParsedSeed) {
  TestVariationsSeedStore seed_store(&prefs_, temp_dir_.GetPath());
  ASSERT_EQ(base::FieldTrialList::FindFullName(kSeedFileTrial),
            GetParam().field_trial_group);

  const std::string serialized_seed = SerializeSeed(CreateTestSeed());
  ASSERT_TRUE(StoreSeedData(seed_store, serialized_seed));
  EXPECT_EQ(serialized_seed, SerializeSeed(stored_seed_));
}

TEST_P(StoreSeedDataAllGroupsTest, CountryCode) {
  TestVariationsSeedStore seed_store(&prefs_, temp_dir_.GetPath());
  ASSERT_EQ(base::FieldTrialList::FindFullName(kSeedFileTrial),
            GetParam().field_trial_group);

  // Test with a valid header value.
  std::string seed = SerializeSeed(CreateTestSeed());
  ASSERT_TRUE(
      StoreSeedData(seed_store, seed, {.country_code = "test_country"}));
  EXPECT_EQ("test_country", GetSeedInfo(seed_store).session_country_code);

  // Test with no country code specified - which should preserve the old value.
  ASSERT_TRUE(StoreSeedData(seed_store, seed));
  EXPECT_EQ("test_country", GetSeedInfo(seed_store).session_country_code);
}

TEST_P(StoreSeedDataAllGroupsTest, GzippedSeed) {
  TestVariationsSeedStore seed_store(&prefs_, temp_dir_.GetPath());
  ASSERT_EQ(base::FieldTrialList::FindFullName(kSeedFileTrial),
            GetParam().field_trial_group);

  const std::string serialized_seed = SerializeSeed(CreateTestSeed());
  ASSERT_TRUE(StoreSeedData(seed_store, Gzip(serialized_seed),
                            {.is_gzip_compressed = true}));
  EXPECT_EQ(serialized_seed, SerializeSeed(stored_seed_));
}

TEST_P(StoreSeedDataAllGroupsTest, GzippedEmptySeed) {
  TestVariationsSeedStore seed_store(&prefs_, temp_dir_.GetPath());
  ASSERT_EQ(base::FieldTrialList::FindFullName(kSeedFileTrial),
            GetParam().field_trial_group);

  store_success_ = true;
  EXPECT_FALSE(StoreSeedData(seed_store, Gzip(/*data=*/std::string()),
                             {.is_gzip_compressed = true}));
}

TEST_P(StoreSeedDataAllGroupsTest, DeltaCompressed) {
  TestVariationsSeedStore seed_store(&prefs_, temp_dir_.GetPath());
  ASSERT_EQ(base::FieldTrialList::FindFullName(kSeedFileTrial),
            GetParam().field_trial_group);
  seed_store.GetSeedReaderWriterForTesting()->StoreValidatedSeedInfo(
      ValidatedSeedInfo{
          .seed_data = kSeedDeltaTestData.GetInitialSeedData(),
          .signature = "ignored signature",
          .milestone = 1,
          .seed_date = base::Time::Now(),
          .client_fetch_time = base::Time::Now(),
          .session_country_code = "us",
      });

  ASSERT_TRUE(StoreSeedData(seed_store, kSeedDeltaTestData.GetDeltaData(),
                            {.is_delta_compressed = true}));
  EXPECT_EQ(kSeedDeltaTestData.GetNewSeedData(), SerializeSeed(stored_seed_));
}

TEST_P(StoreSeedDataAllGroupsTest, DeltaCompressedGzipped) {
  TestVariationsSeedStore seed_store(&prefs_, temp_dir_.GetPath());
  ASSERT_EQ(base::FieldTrialList::FindFullName(kSeedFileTrial),
            GetParam().field_trial_group);
  seed_store.GetSeedReaderWriterForTesting()->StoreValidatedSeedInfo(
      ValidatedSeedInfo{
          .seed_data = kSeedDeltaTestData.GetInitialSeedData(),
          .signature = "ignored signature",
          .milestone = 1,
          .seed_date = base::Time::Now(),
          .client_fetch_time = base::Time::Now(),
          .session_country_code = "us",
      });

  ASSERT_TRUE(StoreSeedData(seed_store, Gzip(kSeedDeltaTestData.GetDeltaData()),
                            {
                                .is_delta_compressed = true,
                                .is_gzip_compressed = true,
                            }));
  EXPECT_EQ(kSeedDeltaTestData.GetNewSeedData(), SerializeSeed(stored_seed_));
}

TEST_P(StoreSeedDataAllGroupsTest, DeltaButNoInitialSeed) {
  TestVariationsSeedStore seed_store(&prefs_, temp_dir_.GetPath());
  ASSERT_EQ(base::FieldTrialList::FindFullName(kSeedFileTrial),
            GetParam().field_trial_group);

  store_success_ = true;
  EXPECT_FALSE(StoreSeedData(seed_store,
                             Gzip(kSeedDeltaTestData.GetDeltaData()),
                             {
                                 .is_delta_compressed = true,
                                 .is_gzip_compressed = true,
                             }));
}

TEST_P(StoreSeedDataAllGroupsTest, BadDelta) {
  TestVariationsSeedStore seed_store(&prefs_, temp_dir_.GetPath());
  ASSERT_EQ(base::FieldTrialList::FindFullName(kSeedFileTrial),
            GetParam().field_trial_group);
  seed_store.GetSeedReaderWriterForTesting()->StoreValidatedSeedInfo(
      ValidatedSeedInfo{
          .seed_data = kSeedDeltaTestData.GetInitialSeedData(),
          .signature = "ignored signature",
          .milestone = 1,
          .seed_date = base::Time::Now(),
          .client_fetch_time = base::Time::Now(),
          .session_country_code = "us",
      });

  store_success_ = true;
  // Provide a gzipped delta, when gzip is not expected.
  EXPECT_FALSE(StoreSeedData(seed_store,
                             Gzip(kSeedDeltaTestData.GetDeltaData()),
                             {.is_delta_compressed = true}));
}

TEST_P(StoreSeedDataAllGroupsTest, IdenticalToSafeSeed) {
  const std::string serialized_seed = SerializeSeed(CreateTestSeed());

  TestVariationsSeedStore seed_store(&prefs_, temp_dir_.GetPath());
  ASSERT_EQ(base::FieldTrialList::FindFullName(kSeedFileTrial),
            GetParam().field_trial_group);
  seed_store.GetSafeSeedReaderWriterForTesting()->StoreValidatedSeedInfo(
      ValidatedSeedInfo{
          .seed_data = serialized_seed,
          .signature = "ignored signature",
          .milestone = 1,
          .seed_date = base::Time::Now(),
          .client_fetch_time = base::Time::Now(),
          .session_country_code = "us",
          .permanent_country_code = "us",
      });
  ASSERT_TRUE(StoreSeedData(seed_store, serialized_seed));

  // Verify that the pref has a sentinel value, rather than the full string.
  if (ShouldUseLocalStateSeed()) {
    EXPECT_EQ(kIdenticalToSafeSeedSentinel,
              prefs_.GetString(prefs::kVariationsCompressedSeed));
  }
  std::string stored_seed_data;
  auto read_result =
      seed_store.GetSeedReaderWriterForTesting()->ReadSeedDataOnStartup(
          &stored_seed_data, /*base64_seed_signature=*/nullptr);
  EXPECT_EQ(LoadSeedResult::kSuccess, read_result);
  EXPECT_EQ(kIdenticalToSafeSeedSentinel, stored_seed_data);

  // Verify that loading the stored seed returns the original seed value.
  VariationsSeed loaded_seed;
  std::string loaded_seed_data;
  std::string unused_loaded_base64_seed_signature;
  ASSERT_TRUE(seed_store.LoadSeedSync(&loaded_seed, &loaded_seed_data,
                                      &unused_loaded_base64_seed_signature));

  EXPECT_EQ(serialized_seed, SerializeSeed(loaded_seed));
  EXPECT_EQ(serialized_seed, loaded_seed_data);
}

// Verifies that the cached serial number is correctly updated when a new seed
// is saved.
TEST_P(StoreSeedDataAllGroupsTest,
       GetLatestSerialNumber_UpdatedWithNewStoredSeed) {
  // Call GetLatestSerialNumber() once to prime the cached value.
  TestVariationsSeedStore seed_store(&prefs_, temp_dir_.GetPath());
  ASSERT_EQ(base::FieldTrialList::FindFullName(kSeedFileTrial),
            GetParam().field_trial_group);
  ASSERT_THAT(seed_store.GetLatestSerialNumber(), IsEmpty());
  ASSERT_TRUE(StoreSeedData(seed_store, SerializeSeed(CreateTestSeed())));
  EXPECT_EQ("123", seed_store.GetLatestSerialNumber());

  VariationsSeed new_seed = CreateTestSeed();
  new_seed.set_serial_number("456");
  ASSERT_TRUE(StoreSeedData(seed_store, SerializeSeed(new_seed)));
  EXPECT_EQ("456", seed_store.GetLatestSerialNumber());
}

class LoadSafeSeedDataGroupTest
    : public SeedStoreGroupTestBase,
      public ::testing::WithParamInterface<std::string_view> {
 public:
  LoadSafeSeedDataGroupTest()
      : SeedStoreGroupTestBase(kSafeSeedFieldsPrefs, GetParam()) {}
  ~LoadSafeSeedDataGroupTest() override = default;
};

class LoadSafeSeedDataControlAndDefaultGroupsTest
    : public LoadSafeSeedDataGroupTest {};

INSTANTIATE_TEST_SUITE_P(All,
                         LoadSafeSeedDataControlAndDefaultGroupsTest,
                         ::testing::Values(kControlGroup,
                                           kDefaultGroup,
                                           kNoGroup));

TEST_P(LoadSafeSeedDataControlAndDefaultGroupsTest, LoadSafeSeed_ValidSeed) {
  // Store good seed data to test if loading from prefs works.
  const std::string serialized_seed = SerializeSeed(CreateTestSeed());
  const std::string b64_compressed_seed = GzipAndBase64Encode(serialized_seed);
  const base::Time reference_date = base::Time::Now();
  const std::string locale = "en-US";
  const std::string permanent_consistency_country = "us";
  const std::string session_consistency_country = "ca";

  // Attempt to load a valid safe seed.
  TestVariationsSeedStore seed_store(&prefs_, temp_dir_.GetPath());
  ASSERT_EQ(base::FieldTrialList::FindFullName(kSeedFileTrial), GetParam());
  seed_store.GetSafeSeedReaderWriterForTesting()->StoreValidatedSeedInfo(
      ValidatedSeedInfo{
          .seed_data = serialized_seed,
          .signature = "a test signature, ignored.",
          .milestone = 1,
          .seed_date = reference_date,
          .client_fetch_time = reference_date - base::Days(3),
          .session_country_code = session_consistency_country,
          .permanent_country_code = permanent_consistency_country,
      });
  prefs_.SetString(prefs::kVariationsSafeSeedLocale, locale);

  base::HistogramTester histogram_tester;
  VariationsSeed loaded_seed;
  std::unique_ptr<ClientFilterableState> client_state =
      CreateTestClientFilterableState();
  ASSERT_TRUE(seed_store.LoadSafeSeedSync(&loaded_seed, client_state.get()));

  // Verify metrics.
  histogram_tester.ExpectUniqueSample("Variations.SafeMode.LoadSafeSeed.Result",
                                      LoadSeedResult::kSuccess, 1);

  // Check that the loaded data is the same as the original.
  EXPECT_EQ(serialized_seed, SerializeSeed(loaded_seed));
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

  // Make sure the seed hasn't been changed.
  if (ShouldUseLocalStateSeed()) {
    EXPECT_EQ(b64_compressed_seed,
              prefs_.GetString(prefs::kVariationsSafeCompressedSeed));
  }
  std::string stored_seed_data;
  auto read_result =
      seed_store.GetSafeSeedReaderWriterForTesting()->ReadSeedDataOnStartup(
          &stored_seed_data, /*base64_seed_signature=*/nullptr);
  EXPECT_EQ(LoadSeedResult::kSuccess, read_result);
  EXPECT_EQ(serialized_seed, stored_seed_data);
}

TEST_P(LoadSafeSeedDataControlAndDefaultGroupsTest,
       LoadSafeSeed_InvalidSignature) {
  const std::string seed_data = SerializeSeed(CreateTestSeed());

  // Attempt to load a valid safe seed with an invalid signature while signature
  // verification is enabled.
  TestVariationsSeedStore seed_store(&prefs_, temp_dir_.GetPath(),
                                     /*signature_verification_needed=*/true);
  ASSERT_EQ(base::FieldTrialList::FindFullName(kSeedFileTrial), GetParam());
  SetAllSeedsAndSeedPrefsToNonDefaultValues(&prefs_, seed_store);
  seed_store.GetSafeSeedReaderWriterForTesting()->StoreValidatedSeedInfo(
      ValidatedSeedInfo{
          .seed_data = seed_data,
          .signature = "a deeply compromised signature.",
          .milestone = 1,
          .seed_date = base::Time::Now(),
          .client_fetch_time = base::Time::Now(),
          .session_country_code = "us",
          .permanent_country_code = "us",
      });

  base::HistogramTester histogram_tester;
  VariationsSeed loaded_seed;
  std::unique_ptr<ClientFilterableState> client_state =
      CreateTestClientFilterableState();
  ASSERT_FALSE(seed_store.LoadSafeSeedSync(&loaded_seed, client_state.get()));

  // Verify metrics and prefs.
  histogram_tester.ExpectUniqueSample("Variations.SafeMode.LoadSafeSeed.Result",
                                      LoadSeedResult::kInvalidSignature, 1);
  CheckSafeSeedAndSeedPrefsAreCleared(prefs_, seed_store);
  CheckRegularSeedAndSeedPrefsAreSet(prefs_, seed_store);

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

TEST_P(LoadSafeSeedDataControlAndDefaultGroupsTest, LoadSafeSeed_EmptySeed) {
  // Attempt to load an empty safe seed.
  TestVariationsSeedStore seed_store(&prefs_, temp_dir_.GetPath());
  ASSERT_EQ(base::FieldTrialList::FindFullName(kSeedFileTrial), GetParam());
  ASSERT_TRUE(
      PrefHasDefaultValue(prefs_, prefs::kVariationsSafeCompressedSeed));
  std::string stored_seed_data;
  auto read_result =
      seed_store.GetSafeSeedReaderWriterForTesting()->ReadSeedDataOnStartup(
          &stored_seed_data, /*base64_seed_signature=*/nullptr);
  EXPECT_EQ(read_result, LoadSeedResult::kEmpty);

  base::HistogramTester histogram_tester;
  VariationsSeed loaded_seed;
  std::unique_ptr<ClientFilterableState> client_state =
      CreateTestClientFilterableState();
  ASSERT_FALSE(seed_store.LoadSafeSeedSync(&loaded_seed, client_state.get()));

  // Verify metrics.
  histogram_tester.ExpectUniqueSample("Variations.SafeMode.LoadSafeSeed.Result",
                                      LoadSeedResult::kEmpty, 1);
}

TEST_P(LoadSafeSeedDataControlAndDefaultGroupsTest, LoadSafeSeed_CorruptGzip) {
  // Loading a corrupted compressed safe seed should return false.
  TestVariationsSeedStore seed_store(&prefs_, temp_dir_.GetPath());
  ASSERT_EQ(base::FieldTrialList::FindFullName(kSeedFileTrial), GetParam());
  SetAllSeedsAndSeedPrefsToNonDefaultValues(&prefs_, seed_store);
  std::string compressed_seed = Gzip("seed data");
  // Flip some bits to corrupt the data
  compressed_seed[5] ^= 0xFF;
  compressed_seed[10] ^= 0xFF;
  seed_store.GetSafeSeedReaderWriterForTesting()->StoreValidatedSeedInfo(
      ValidatedSeedInfo{
          .seed_data = "this will be overwritten",
          .signature = "ignored signature",
          .milestone = 1,
          .seed_date = base::Time::Now(),
          .client_fetch_time = base::Time::Now(),
          .session_country_code = "us",
          .permanent_country_code = "us",
      });
  // Data is stored in base64 format in local state.
  std::string base64_compressed_seed = base::Base64Encode(compressed_seed);
  seed_store.GetSafeSeedReaderWriterForTesting()->StoreRawSeedForTesting(
      base64_compressed_seed);

  base::HistogramTester histogram_tester;
  VariationsSeed loaded_seed;
  std::unique_ptr<ClientFilterableState> client_state =
      CreateTestClientFilterableState();
  ASSERT_FALSE(seed_store.LoadSafeSeedSync(&loaded_seed, client_state.get()));

  // Verify metrics and prefs.
  histogram_tester.ExpectUniqueSample("Variations.SafeMode.LoadSafeSeed.Result",
                                      LoadSeedResult::kCorruptGzip, 1);
  CheckSafeSeedAndSeedPrefsAreCleared(prefs_, seed_store);
  CheckRegularSeedAndSeedPrefsAreSet(prefs_, seed_store);

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

TEST_P(LoadSafeSeedDataControlAndDefaultGroupsTest,
       LoadSafeSeed_ExceedsUncompressedSizeLimit) {
  // Loading a safe seed that exceeds the uncompressed size should return false.
  TestVariationsSeedStore seed_store(&prefs_, temp_dir_.GetPath());
  ASSERT_EQ(base::FieldTrialList::FindFullName(kSeedFileTrial), GetParam());
  SetAllSeedsAndSeedPrefsToNonDefaultValues(&prefs_, seed_store);
  seed_store.GetSafeSeedReaderWriterForTesting()->StoreValidatedSeedInfo(
      ValidatedSeedInfo{
          .seed_data = CreateTooLargeData(),
          .signature = "ignored signature",
          .milestone = 1,
          .seed_date = base::Time::Now(),
          .client_fetch_time = base::Time::Now(),
          .session_country_code = "us",
          .permanent_country_code = "us",
      });

  base::HistogramTester histogram_tester;
  VariationsSeed loaded_seed;
  std::unique_ptr<ClientFilterableState> client_state =
      CreateTestClientFilterableState();
  ASSERT_FALSE(seed_store.LoadSafeSeedSync(&loaded_seed, client_state.get()));

  // Verify metrics and prefs.
  histogram_tester.ExpectUniqueSample(
      "Variations.SafeMode.LoadSafeSeed.Result",
      LoadSeedResult::kExceedsUncompressedSizeLimit, 1);
  CheckSafeSeedAndSeedPrefsAreCleared(prefs_, seed_store);
  CheckRegularSeedAndSeedPrefsAreSet(prefs_, seed_store);

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

// Coverage for base64 decoding issues is N/A to treatment-group clients because
// they don't use base64 encoding.
TEST_P(LoadSafeSeedDataControlAndDefaultGroupsTest,
       LoadSafeSeed_Base64DecodingFailure) {
  // Loading a non-base64-encoded safe seed should return false.
  TestVariationsSeedStore seed_store(&prefs_, temp_dir_.GetPath());
  ASSERT_EQ(base::FieldTrialList::FindFullName(kSeedFileTrial), GetParam());
  SetAllSeedsAndSeedPrefsToNonDefaultValues(&prefs_, seed_store);
  seed_store.GetSafeSeedReaderWriterForTesting()->StoreValidatedSeedInfo(
      ValidatedSeedInfo{
          .seed_data = "this will be overwritten",
          .signature = "ignored signature",
          .milestone = 1,
          .seed_date = base::Time::Now(),
          .client_fetch_time = base::Time::Now(),
          .session_country_code = "us",
          .permanent_country_code = "us",
      });
  seed_store.GetSafeSeedReaderWriterForTesting()->StoreRawSeedForTesting(
      "this is not base64");

  base::HistogramTester histogram_tester;
  VariationsSeed loaded_seed;
  std::unique_ptr<ClientFilterableState> client_state =
      CreateTestClientFilterableState();
  ASSERT_FALSE(seed_store.LoadSafeSeedSync(&loaded_seed, client_state.get()));

  // Verify metrics and prefs.
  histogram_tester.ExpectUniqueSample("Variations.SafeMode.LoadSafeSeed.Result",
                                      LoadSeedResult::kCorruptBase64, 1);
  CheckSafeSeedAndSeedPrefsAreCleared(prefs_, seed_store);
  CheckRegularSeedAndSeedPrefsAreSet(prefs_, seed_store);

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

class LoadSafeSeedDataSeedFilesGroupTest
    : public LoadSeedDataSeedFilesGroupBase {
 public:
  LoadSafeSeedDataSeedFilesGroupTest()
      : LoadSeedDataSeedFilesGroupBase(kSafeSeedFieldsPrefs) {}
};

TEST_F(LoadSafeSeedDataSeedFilesGroupTest, LoadSafeSeed_ValidSeed) {
  ASSERT_EQ(base::FieldTrialList::FindFullName(kSeedFileTrial),
            kSeedFilesGroup);

  const std::string serialized_seed = SerializeSeed(CreateTestSeed());
  const base::Time reference_date = base::Time::Now();
  const std::string locale = "en-US";
  const std::string permanent_consistency_country = "us";
  const std::string session_consistency_country = "ca";

  StoredSeedInfo stored_seed_info;
  stored_seed_info.set_data(serialized_seed);
  stored_seed_info.set_signature("a test signature, ignored.");
  stored_seed_info.set_milestone(1);
  stored_seed_info.set_seed_date(reference_date.ToInternalValue());
  stored_seed_info.set_client_fetch_time(
      (reference_date - base::Days(3)).ToInternalValue());
  stored_seed_info.set_session_country_code(session_consistency_country);
  stored_seed_info.set_permanent_country_code(permanent_consistency_country);
  WriteSafeSeedToFile(SeedReaderWriter::CompressForSeedFileForTesting(
      stored_seed_info.SerializeAsString()));

  prefs_.SetString(prefs::kVariationsSafeSeedLocale, locale);

  base::HistogramTester histogram_tester;
  TestVariationsSeedStore seed_store(
      /*local_state=*/&prefs_, /*seed_file_dir=*/temp_dir_.GetPath(),
      /*signature_verification_needed=*/false, /*initial_seed=*/nullptr,
      /*use_first_run_prefs=*/true,
      /*channel=*/version_info::Channel::DEV);

  // Verify metrics.
  histogram_tester.ExpectUniqueSample("Variations.SeedFileReadResult.Safe",
                                      LoadSeedResult::kSuccess, 1);

  VariationsSeed loaded_seed;
  std::unique_ptr<ClientFilterableState> client_state =
      CreateTestClientFilterableState();
  ASSERT_TRUE(seed_store.LoadSafeSeedSync(&loaded_seed, client_state.get()));

  // Check that the loaded data is the same as the original.
  EXPECT_EQ(serialized_seed, SerializeSeed(loaded_seed));
  EXPECT_EQ(locale, client_state->locale);
  // `reference_date` is truncated to microsecond precision when stored.
  EXPECT_EQ(base::Time::FromInternalValue(reference_date.ToInternalValue()),
            client_state->reference_date);
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

  // Make sure the seed hasn't been changed.
  std::string stored_seed_data;
  auto read_result =
      seed_store.GetSafeSeedReaderWriterForTesting()->ReadSeedDataOnStartup(
          &stored_seed_data, /*base64_seed_signature=*/nullptr);
  EXPECT_EQ(LoadSeedResult::kSuccess, read_result);
  EXPECT_EQ(serialized_seed, stored_seed_data);
}

TEST_F(LoadSafeSeedDataSeedFilesGroupTest, LoadSafeSeed_InvalidSignature) {
  ASSERT_EQ(base::FieldTrialList::FindFullName(kSeedFileTrial),
            kSeedFilesGroup);

  WriteTestRegularSeedToFile();
  const std::string seed_data = SerializeSeed(CreateTestSeed());
  StoredSeedInfo stored_seed_info;
  stored_seed_info.set_data(seed_data);
  stored_seed_info.set_signature("a deeply compromised signature.");
  stored_seed_info.set_milestone(1);
  stored_seed_info.set_seed_date(base::Time::Now().ToInternalValue());
  stored_seed_info.set_client_fetch_time(base::Time::Now().ToInternalValue());
  stored_seed_info.set_session_country_code("us");
  stored_seed_info.set_permanent_country_code("us");
  WriteSafeSeedToFile(SeedReaderWriter::CompressForSeedFileForTesting(
      stored_seed_info.SerializeAsString()));

  TestVariationsSeedStore seed_store(
      /*local_state=*/&prefs_, /*seed_file_dir=*/temp_dir_.GetPath(),
      /*signature_verification_needed=*/true, /*initial_seed=*/nullptr,
      /*use_first_run_prefs=*/true,
      /*channel=*/version_info::Channel::DEV);

  base::HistogramTester histogram_tester;
  VariationsSeed loaded_seed;
  std::unique_ptr<ClientFilterableState> client_state =
      CreateTestClientFilterableState();
  ASSERT_FALSE(seed_store.LoadSafeSeedSync(&loaded_seed, client_state.get()));

  // Verify metrics and prefs.
  histogram_tester.ExpectUniqueSample("Variations.SafeMode.LoadSafeSeed.Result",
                                      LoadSeedResult::kInvalidSignature, 1);
  CheckSafeSeedAndSeedPrefsAreCleared(prefs_, seed_store);
  CheckRegularSeedAndSeedPrefsAreSet(prefs_, seed_store);

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

TEST_F(LoadSafeSeedDataSeedFilesGroupTest, LoadSafeSeed_EmptySeedFile) {
  ASSERT_EQ(base::FieldTrialList::FindFullName(kSeedFileTrial),
            kSeedFilesGroup);

  WriteSafeSeedToFile(/*seed_data=*/"");

  base::HistogramTester histogram_tester;
  // Attempt to load an empty safe seed. The safe seed file is not created.
  TestVariationsSeedStore seed_store(/*local_state=*/&prefs_,
                                     /*seed_file_dir=*/temp_dir_.GetPath(),
                                     /*signature_verification_needed=*/false,
                                     /*initial_seed=*/nullptr,
                                     /*use_first_run_prefs=*/true,
                                     /*channel=*/version_info::Channel::DEV);
  // Verify metrics.
  histogram_tester.ExpectUniqueSample("Variations.SeedFileReadResult.Safe",
                                      LoadSeedResult::kEmpty, 1);
  std::string stored_seed_data;
  auto read_result =
      seed_store.GetSafeSeedReaderWriterForTesting()->ReadSeedDataOnStartup(
          &stored_seed_data, /*base64_seed_signature=*/nullptr);
  EXPECT_EQ(read_result, LoadSeedResult::kEmpty);

  VariationsSeed loaded_seed;
  std::unique_ptr<ClientFilterableState> client_state =
      CreateTestClientFilterableState();
  ASSERT_FALSE(seed_store.LoadSafeSeedSync(&loaded_seed, client_state.get()));
}

TEST_F(LoadSafeSeedDataSeedFilesGroupTest, LoadSafeSeed_NoSeedFile) {
  ASSERT_EQ(base::FieldTrialList::FindFullName(kSeedFileTrial),
            kSeedFilesGroup);

  base::HistogramTester histogram_tester;
  // Attempt to load an empty safe seed. The safe seed file is not created.
  TestVariationsSeedStore seed_store(/*local_state=*/&prefs_,
                                     /*seed_file_dir=*/temp_dir_.GetPath(),
                                     /*signature_verification_needed=*/false,
                                     /*initial_seed=*/nullptr,
                                     /*use_first_run_prefs=*/true,
                                     /*channel=*/version_info::Channel::DEV);
  // Verify metrics.
  histogram_tester.ExpectUniqueSample("Variations.SeedFileReadResult.Safe",
                                      LoadSeedResult::kErrorReadingFile, 1);
  std::string stored_seed_data;
  auto read_result =
      seed_store.GetSafeSeedReaderWriterForTesting()->ReadSeedDataOnStartup(
          &stored_seed_data, /*base64_seed_signature=*/nullptr);
  EXPECT_EQ(read_result, LoadSeedResult::kEmpty);

  VariationsSeed loaded_seed;
  std::unique_ptr<ClientFilterableState> client_state =
      CreateTestClientFilterableState();
  ASSERT_FALSE(seed_store.LoadSafeSeedSync(&loaded_seed, client_state.get()));
}

// Loading a corrupted compressed safe seed should return false.
TEST_F(LoadSafeSeedDataSeedFilesGroupTest, LoadSafeSeed_CorruptGzip) {
  ASSERT_EQ(base::FieldTrialList::FindFullName(kSeedFileTrial),
            kSeedFilesGroup);
  WriteTestRegularSeedToFile();
  std::string compressed_seed = SeedReaderWriter::CompressForSeedFileForTesting(
      "very valid seed info data");
  // Flip some bits to corrupt the data
  compressed_seed[5] ^= 0xFF;
  compressed_seed[10] ^= 0xFF;
  WriteSafeSeedToFile(compressed_seed);

  base::HistogramTester histogram_tester;
  TestVariationsSeedStore seed_store(/*local_state=*/&prefs_,
                                     /*seed_file_dir=*/temp_dir_.GetPath(),
                                     /*signature_verification_needed=*/false,
                                     /*initial_seed=*/nullptr,
                                     /*use_first_run_prefs=*/true,
                                     /*channel=*/version_info::Channel::DEV);

  // Verify metrics.
  {
#if BUILDFLAG(IS_ANDROID)
    LoadSeedResult expected_result = LoadSeedResult::kCorruptGzip;
#else
    LoadSeedResult expected_result = LoadSeedResult::kCorruptZstd;
#endif
    histogram_tester.ExpectUniqueSample("Variations.SeedFileReadResult.Safe",
                                        expected_result, 1);
  }

  // Verify that the seed is not loaded.
  VariationsSeed loaded_seed;
  std::unique_ptr<ClientFilterableState> client_state =
      CreateTestClientFilterableState();
  ASSERT_FALSE(seed_store.LoadSafeSeedSync(&loaded_seed, client_state.get()));

  // Verify prefs.
  CheckSafeSeedAndSeedPrefsAreCleared(prefs_, seed_store);
  CheckRegularSeedAndSeedPrefsAreSet(prefs_, seed_store);

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

TEST_F(LoadSafeSeedDataSeedFilesGroupTest,
       LoadSafeSeed_ExceedsUncompressedSizeLimit) {
  ASSERT_EQ(base::FieldTrialList::FindFullName(kSeedFileTrial),
            kSeedFilesGroup);

  WriteTestRegularSeedToFile();
  StoredSeedInfo stored_seed_info;
  stored_seed_info.set_data(CreateTooLargeData());
  stored_seed_info.set_signature("ignored signature");
  stored_seed_info.set_milestone(1);
  stored_seed_info.set_seed_date(base::Time::Now().ToInternalValue());
  stored_seed_info.set_client_fetch_time(base::Time::Now().ToInternalValue());
  stored_seed_info.set_session_country_code("us");
  stored_seed_info.set_permanent_country_code("us");
  WriteSafeSeedToFile(SeedReaderWriter::CompressForSeedFileForTesting(
      stored_seed_info.SerializeAsString()));

  base::HistogramTester histogram_tester;
  TestVariationsSeedStore seed_store(/*local_state=*/&prefs_,
                                     /*seed_file_dir=*/temp_dir_.GetPath(),
                                     /*signature_verification_needed=*/false,
                                     /*initial_seed=*/nullptr,
                                     /*use_first_run_prefs=*/true,
                                     /*channel=*/version_info::Channel::DEV);

  // Verify metrics.
  histogram_tester.ExpectUniqueSample(
      "Variations.SeedFileReadResult.Safe",
      LoadSeedResult::kExceedsUncompressedSizeLimit, 1);

  VariationsSeed loaded_seed;
  std::unique_ptr<ClientFilterableState> client_state =
      CreateTestClientFilterableState();
  ASSERT_FALSE(seed_store.LoadSafeSeedSync(&loaded_seed, client_state.get()));

  // Verify prefs.
  CheckSafeSeedAndSeedPrefsAreCleared(prefs_, seed_store);
  CheckRegularSeedAndSeedPrefsAreSet(prefs_, seed_store);

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

struct InvalidSafeSeedTestParams {
  const std::string test_name;
  const std::string seed;
  const std::string signature;
  StoreSeedResult store_seed_result;
  std::optional<VerifySignatureResult> verify_signature_result = std::nullopt;
};

struct StoreInvalidSafeSeedTestParams {
  using TupleT = std::tuple<InvalidSafeSeedTestParams, std::string_view>;

  InvalidSafeSeedTestParams invalid_params;
  std::string_view field_trial_group;

  explicit StoreInvalidSafeSeedTestParams(const TupleT& t)
      : invalid_params(std::get<0>(t)), field_trial_group(std::get<1>(t)) {}
};

class StoreInvalidSafeSeedTest
    : public SeedStoreGroupTestBase,
      public ::testing::WithParamInterface<StoreInvalidSafeSeedTestParams> {
 public:
  StoreInvalidSafeSeedTest()
      : SeedStoreGroupTestBase(kSafeSeedFieldsPrefs,
                               GetParam().field_trial_group) {}
  ~StoreInvalidSafeSeedTest() override = default;
};

INSTANTIATE_TEST_SUITE_P(
    All,
    StoreInvalidSafeSeedTest,
    ::testing::ConvertGenerator<
        StoreInvalidSafeSeedTestParams::TupleT>(::testing::Combine(
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
                .verify_signature_result =
                    VerifySignatureResult::kInvalidSeed}),
        ::testing::Values(kSeedFilesGroup,
                          kControlGroup,
                          kDefaultGroup,
                          kNoGroup))),
    [](const ::testing::TestParamInfo<StoreInvalidSafeSeedTestParams>& params) {
      if (params.param.field_trial_group.empty()) {
        return params.param.invalid_params.test_name + "_LocalStateOnly";
      }
      return params.param.invalid_params.test_name + "_" +
             std::string(params.param.field_trial_group);
    });

// Verify that attempting to store an invalid safe seed fails and does not
// modify Local State's safe-seed-related prefs or a seed file.
TEST_P(StoreInvalidSafeSeedTest, StoreSafeSeed) {
  // Set a safe seed in the seed file and local state prefs.
  const std::string expected_seed = "a seed";
  InvalidSafeSeedTestParams params = GetParam().invalid_params;
  const std::string seed_to_store = params.seed;
  prefs_.SetString(prefs::kVariationsSafeCompressedSeed, expected_seed);
  ASSERT_TRUE(base::WriteFile(temp_seed_file_path_, expected_seed));

  // Set associated safe seed local state prefs to their expected values.
  const std::string expected_signature = "a signature";
  prefs_.SetString(prefs::kVariationsSafeSeedSignature, expected_signature);

  const int expected_milestone = 90;
  prefs_.SetInteger(prefs::kVariationsSafeSeedMilestone, expected_milestone);

  const base::Time now = base::Time::Now();
  const base::Time expected_fetch_time = now - base::Hours(3);
  prefs_.SetTime(prefs::kVariationsSafeSeedFetchTime, expected_fetch_time);

  std::unique_ptr<ClientFilterableState> client_state =
      CreateTestClientFilterableState();

  const std::string expected_locale = "en-US";
  client_state->locale = "pt-PT";
  prefs_.SetString(prefs::kVariationsSafeSeedLocale, expected_locale);

  const std::string expected_permanent_consistency_country = "us";
  client_state->permanent_consistency_country = "CA";
  prefs_.SetString(prefs::kVariationsSafeSeedPermanentConsistencyCountry,
                   expected_permanent_consistency_country);

  const std::string expected_session_consistency_country = "BR";
  client_state->session_consistency_country = "PT";
  prefs_.SetString(prefs::kVariationsSafeSeedSessionConsistencyCountry,
                   expected_session_consistency_country);

  const base::Time expected_date = now - base::Days(2);
  client_state->reference_date = now - base::Days(1);
  prefs_.SetTime(prefs::kVariationsSafeSeedDate, expected_date);

  TestVariationsSeedStore seed_store(&prefs_, temp_dir_.GetPath(),
                                     /*signature_verification_needed=*/true);
  ASSERT_EQ(base::FieldTrialList::FindFullName(kSeedFileTrial),
            GetParam().field_trial_group);
  seed_store.SetSafeSeedReaderWriterForTesting(std::move(seed_reader_writer_));
  base::HistogramTester histogram_tester;

  // Verify that attempting to store an invalid seed fails.
  base::RunLoop run_loop;
  bool store_seed_result = false;
  seed_store.StoreSafeSeed(
      /*done_callback=*/base::BindLambdaForTesting(
          [&store_seed_result, &run_loop](bool result) {
            store_seed_result = result;
            run_loop.Quit();
          }),
      params.seed, params.signature,
      /*seed_milestone=*/91, *client_state,
      /*seed_fetch_time=*/now - base::Hours(1));
  run_loop.Run();
  ASSERT_FALSE(store_seed_result);

  // Verify that the seed file has no pending writes and was not overwritten.
  ASSERT_FALSE(timer_.IsRunning());
  std::string seed_file_data;
  ASSERT_TRUE(base::ReadFileToString(temp_seed_file_path_, &seed_file_data));
  EXPECT_EQ(seed_file_data, expected_seed);

  // Verify that none of the safe seed prefs were overwritten.
  if (ShouldUseLocalStateSeed()) {
    EXPECT_EQ(prefs_.GetString(prefs::kVariationsSafeCompressedSeed),
              expected_seed);
  }
  EXPECT_EQ(prefs_.GetString(prefs::kVariationsSafeSeedSignature),
            expected_signature);
  EXPECT_EQ(prefs_.GetString(prefs::kVariationsSafeSeedLocale),
            expected_locale);
  EXPECT_EQ(prefs_.GetInteger(prefs::kVariationsSafeSeedMilestone),
            expected_milestone);
  EXPECT_EQ(
      prefs_.GetString(prefs::kVariationsSafeSeedPermanentConsistencyCountry),
      expected_permanent_consistency_country);
  EXPECT_EQ(
      prefs_.GetString(prefs::kVariationsSafeSeedSessionConsistencyCountry),
      expected_session_consistency_country);
  EXPECT_EQ(prefs_.GetTime(prefs::kVariationsSafeSeedDate), expected_date);
  EXPECT_EQ(prefs_.GetTime(prefs::kVariationsSafeSeedFetchTime),
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

class StoreSafeSeedDataGroupTest
    : public SeedStoreGroupTestBase,
      public ::testing::WithParamInterface<StoreSeedDataTestParams> {
 public:
  StoreSafeSeedDataGroupTest()
      : SeedStoreGroupTestBase(kSafeSeedFieldsPrefs,
                               GetParam().field_trial_group) {}
  ~StoreSafeSeedDataGroupTest() override = default;
};

class StoreSafeSeedDataSeedFilesGroupTest : public StoreSafeSeedDataGroupTest {
};

class StoreSafeSeedDataControlAndLocalStateOnlyGroupTest
    : public StoreSafeSeedDataGroupTest {};

class StoreSafeSeedDataAllGroupsTest : public StoreSafeSeedDataGroupTest {};

INSTANTIATE_TEST_SUITE_P(
    All,
    StoreSafeSeedDataSeedFilesGroupTest,
    ::testing::ConvertGenerator<StoreSeedDataTestParams::TupleT>(
        ::testing::Combine(::testing::Bool(),
                           ::testing::Values(kSeedFilesGroup))));

TEST_P(StoreSafeSeedDataSeedFilesGroupTest, StoreSafeSeed_ValidSignature) {
  auto client_state = CreateDummyClientFilterableState();
  const std::string expected_locale = "en-US";
  client_state->locale = expected_locale;
  const base::Time now = base::Time::Now();
  const base::Time expected_date = now - base::Days(1);
  client_state->reference_date = expected_date;
  const std::string expected_permanent_consistency_country = "us";
  client_state->permanent_consistency_country =
      expected_permanent_consistency_country;
  const std::string expected_session_consistency_country = "CA";
  client_state->session_consistency_country =
      expected_session_consistency_country;

  TestVariationsSeedStore seed_store(&prefs_, temp_dir_.GetPath(),
                                     /*signature_verification_needed=*/true);
  ASSERT_EQ(base::FieldTrialList::FindFullName(kSeedFileTrial),
            GetParam().field_trial_group);
  base::HistogramTester histogram_tester;
  seed_store.SetSafeSeedReaderWriterForTesting(std::move(seed_reader_writer_));

  std::string expected_seed;
  ASSERT_TRUE(base::Base64Decode(kTestSeedData.base64_uncompressed_data,
                                 &expected_seed));
  const std::string expected_signature = kTestSeedData.base64_signature;
  const int expected_seed_milestone = 92;
  const base::Time expected_fetch_time = now - base::Hours(6);

  // Verify that storing the safe seed succeeded.
  {
    base::RunLoop run_loop;
    bool store_seed_result = false;
    seed_store.AllowToPurgeSeedsDataFromMemory();
    seed_store.StoreSafeSeed(
        /*done_callback=*/base::BindLambdaForTesting(
            [&store_seed_result, &run_loop](bool result) {
              store_seed_result = result;
              run_loop.Quit();
            }),
        expected_seed, expected_signature, expected_seed_milestone,
        *client_state, expected_fetch_time);
    run_loop.Run();
    // Force write for SeedReaderWriter.
    timer_.Fire();
    file_writer_thread_.FlushForTesting();
    base::RunLoop().RunUntilIdle();
    ASSERT_TRUE(store_seed_result);
  }

  {
    // Make sure seed is not stored in memory, so it can be read from the file.
    ASSERT_FALSE(seed_store.GetSafeSeedReaderWriterForTesting()
                     ->stored_seed_data_for_testing()
                     .has_value());
    // Make sure seed in seed file matches the one created.
    base::RunLoop run_loop;
    seed_store.LoadSafeSeed(base::BindLambdaForTesting(
        [&run_loop, &expected_seed](std::string loaded_seed_data,
                                    std::string loaded_seed_signature,
                                    bool success, VariationsSeed seed) {
          EXPECT_TRUE(success);
          EXPECT_EQ(loaded_seed_data, expected_seed);
          run_loop.Quit();
        }));
    run_loop.Run();
  }

  // Verify metrics.
  histogram_tester.ExpectUniqueSample(
      "Variations.SafeMode.StoreSafeSeed.Result", StoreSeedResult::kSuccess, 1);
  histogram_tester.ExpectUniqueSample(
      "Variations.SafeMode.StoreSafeSeed.SignatureValidity",
      VerifySignatureResult::kValidSignature, 1);
}

TEST_P(StoreSafeSeedDataSeedFilesGroupTest,
       StoreSafeSeed_PreviouslyIdenticalToLatestSeed) {
  // Create two distinct seeds: an old one saved as both the safe and the latest
  // seed value, and a new one that should overwrite only the stored safe seed
  // value.
  const std::string old_seed_data = SerializeSeed(CreateTestSeed());
  VariationsSeed new_seed = CreateTestSeed();
  new_seed.set_serial_number("12345678");
  const std::string new_seed_data = SerializeSeed(new_seed);
  ASSERT_NE(old_seed_data, new_seed_data);

  const std::string base64_old_seed = GzipAndBase64Encode(old_seed_data);
  const std::string compressed_old_seed = Gzip(old_seed_data);
  const base::Time fetch_time = WrapTime(12345);
  auto client_state = CreateDummyClientFilterableState();

  TestVariationsSeedStore seed_store(&prefs_, temp_dir_.GetPath());
  ASSERT_EQ(base::FieldTrialList::FindFullName(kSeedFileTrial),
            GetParam().field_trial_group);
  seed_store.GetSafeSeedReaderWriterForTesting()->StoreValidatedSeedInfo(
      ValidatedSeedInfo{
          .seed_data = old_seed_data,
          .signature = "a completely ignored signature",
          .milestone = 1,
          .seed_date = client_state->reference_date,
          .client_fetch_time = fetch_time - base::Hours(1),
          .session_country_code = "us",
          .permanent_country_code = "us",
      });
  seed_store.GetSeedReaderWriterForTesting()->StoreValidatedSeedInfo(
      ValidatedSeedInfo{
          .seed_data = kIdenticalToSafeSeedSentinel,
          .signature = "a completely ignored signature",
          .milestone = 1,
          .seed_date = client_state->reference_date,
          .client_fetch_time = fetch_time,
          .session_country_code = "us",
      });
  base::HistogramTester histogram_tester;
  base::RunLoop run_loop;
  bool store_seed_result = false;
  seed_store.StoreSafeSeed(
      /*done_callback=*/base::BindLambdaForTesting(
          [&store_seed_result, &run_loop](bool result) {
            store_seed_result = result;
            run_loop.Quit();
          }),
      new_seed_data, "a completely ignored signature",
      /*seed_milestone=*/92, *client_state, fetch_time);
  run_loop.Run();
  ASSERT_TRUE(store_seed_result);

  // Verify the latest seed value was copied before the safe seed was
  // overwritten.
  std::string stored_latest_seed;
  LoadSeedResult load_result =
      seed_store.GetSeedReaderWriterForTesting()->ReadSeedDataOnStartup(
          &stored_latest_seed, /*base64_seed_signature=*/nullptr);
  ASSERT_EQ(load_result, LoadSeedResult::kSuccess);
  ASSERT_EQ(stored_latest_seed, old_seed_data);
  // Verify that loading the stored seed returns the old seed value.
  VariationsSeed loaded_seed;
  std::string loaded_seed_data;
  std::string unused_loaded_base64_seed_signature;
  ASSERT_TRUE(seed_store.LoadSeedSync(&loaded_seed, &loaded_seed_data,
                                      &unused_loaded_base64_seed_signature));

  EXPECT_EQ(old_seed_data, SerializeSeed(loaded_seed));
  EXPECT_EQ(old_seed_data, loaded_seed_data);

  // Verify that the seed file indeed contains the new seed's serialized value.
  std::string stored_safe_seed;
  LoadSeedResult read_result =
      seed_store.GetSafeSeedReaderWriterForTesting()->ReadSeedDataOnStartup(
          &stored_safe_seed, /*base64_seed_signature=*/nullptr);
  ASSERT_EQ(read_result, LoadSeedResult::kSuccess);

  VariationsSeed loaded_safe_seed;
  ASSERT_TRUE(
      seed_store.LoadSafeSeedSync(&loaded_safe_seed, client_state.get()));
  EXPECT_EQ(SerializeSeed(new_seed), SerializeSeed(loaded_safe_seed));
  EXPECT_EQ(fetch_time, seed_store.GetSafeSeedFetchTime());

  // Verify metrics.
  histogram_tester.ExpectUniqueSample(
      "Variations.SafeMode.StoreSafeSeed.Result", StoreSeedResult::kSuccess, 1);
}

INSTANTIATE_TEST_SUITE_P(
    All,
    StoreSafeSeedDataControlAndLocalStateOnlyGroupTest,
    ::testing::ConvertGenerator<StoreSeedDataTestParams::TupleT>(
        ::testing::Combine(
            ::testing::Bool(),
            ::testing::Values(kControlGroup, kDefaultGroup, kNoGroup))));

TEST_P(StoreSafeSeedDataControlAndLocalStateOnlyGroupTest,
       StoreSafeSeed_ValidSignature) {
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
  const std::string expected_permanent_consistency_country = "us";
  client_state->permanent_consistency_country =
      expected_permanent_consistency_country;
  const std::string expected_session_consistency_country = "CA";
  client_state->session_consistency_country =
      expected_session_consistency_country;
  const base::Time expected_fetch_time = now - base::Hours(6);

  // Initialize SeedStore with test prefs and SeedReaderWriter.
  TestVariationsSeedStore seed_store(&prefs_, temp_dir_.GetPath(),
                                     /*signature_verification_needed=*/true);
  ASSERT_EQ(base::FieldTrialList::FindFullName(kSeedFileTrial),
            GetParam().field_trial_group);
  base::HistogramTester histogram_tester;
  seed_store.SetSafeSeedReaderWriterForTesting(std::move(seed_reader_writer_));

  // Verify that storing the safe seed succeeded.
  base::RunLoop run_loop;
  bool store_seed_result = false;
  seed_store.StoreSafeSeed(
      /*done_callback=*/base::BindLambdaForTesting(
          [&store_seed_result, &run_loop](bool result) {
            store_seed_result = result;
            run_loop.Quit();
          }),
      expected_seed, expected_signature, expected_seed_milestone, *client_state,
      expected_fetch_time);
  run_loop.Run();
  ASSERT_TRUE(store_seed_result);

  // Verify that the seed file has no pending or executed writes
  ASSERT_FALSE(timer_.IsRunning());
  EXPECT_FALSE(base::PathExists(temp_seed_file_path_));

  // Verify that safe-seed-related prefs were successfully stored.
  const std::string safe_seed =
      prefs_.GetString(prefs::kVariationsSafeCompressedSeed);
  std::string decoded_compressed_seed;
  ASSERT_TRUE(
      base::Base64Decode(prefs_.GetString(prefs::kVariationsSafeCompressedSeed),
                         &decoded_compressed_seed));
  EXPECT_EQ(Gzip(expected_seed), decoded_compressed_seed);
  EXPECT_EQ(prefs_.GetString(prefs::kVariationsSafeSeedSignature),
            expected_signature);
  EXPECT_EQ(prefs_.GetString(prefs::kVariationsSafeSeedLocale),
            expected_locale);
  EXPECT_EQ(prefs_.GetInteger(prefs::kVariationsSafeSeedMilestone),
            expected_seed_milestone);
  EXPECT_EQ(
      prefs_.GetString(prefs::kVariationsSafeSeedPermanentConsistencyCountry),
      expected_permanent_consistency_country);
  EXPECT_EQ(
      prefs_.GetString(prefs::kVariationsSafeSeedSessionConsistencyCountry),
      expected_session_consistency_country);
  EXPECT_EQ(prefs_.GetTime(prefs::kVariationsSafeSeedDate), expected_date);
  EXPECT_EQ(prefs_.GetTime(prefs::kVariationsSafeSeedFetchTime),
            expected_fetch_time);

  // Verify metrics.
  histogram_tester.ExpectUniqueSample(
      "Variations.SafeMode.StoreSafeSeed.Result", StoreSeedResult::kSuccess, 1);
  histogram_tester.ExpectUniqueSample(
      "Variations.SafeMode.StoreSafeSeed.SignatureValidity",
      VerifySignatureResult::kValidSignature, 1);
}

TEST_P(StoreSafeSeedDataControlAndLocalStateOnlyGroupTest,
       StoreSafeSeed_PreviouslyIdenticalToLatestSeed) {
  // Create two distinct seeds: an old one saved as both the safe and the latest
  // seed value, and a new one that should overwrite only the stored safe seed
  // value.
  const std::string old_seed_data = SerializeSeed(CreateTestSeed());
  VariationsSeed new_seed = CreateTestSeed();
  new_seed.set_serial_number("12345678");
  const std::string new_seed_data = SerializeSeed(new_seed);
  ASSERT_NE(old_seed_data, new_seed_data);

  const std::string base64_old_seed = GzipAndBase64Encode(old_seed_data);
  const std::string compressed_old_seed = Gzip(old_seed_data);
  const base::Time fetch_time = WrapTime(12345);
  auto client_state = CreateDummyClientFilterableState();

  TestVariationsSeedStore seed_store(&prefs_, temp_dir_.GetPath());
  ASSERT_EQ(base::FieldTrialList::FindFullName(kSeedFileTrial),
            GetParam().field_trial_group);
  seed_store.GetSafeSeedReaderWriterForTesting()->StoreValidatedSeedInfo(
      ValidatedSeedInfo{
          .seed_data = old_seed_data,
          .signature = "a completely ignored signature",
          .milestone = 1,
          .seed_date = client_state->reference_date,
          .client_fetch_time = fetch_time - base::Hours(1),
          .session_country_code = "us",
          .permanent_country_code = "us",
      });
  seed_store.GetSeedReaderWriterForTesting()->StoreValidatedSeedInfo(
      ValidatedSeedInfo{
          .seed_data = kIdenticalToSafeSeedSentinel,
          .signature = "a completely ignored signature",
          .milestone = 1,
          .seed_date = client_state->reference_date,
          .client_fetch_time = fetch_time,
          .session_country_code = "us",
      });
  base::HistogramTester histogram_tester;
  base::RunLoop run_loop;
  bool store_seed_result = false;
  seed_store.StoreSafeSeed(
      /*done_callback=*/base::BindLambdaForTesting(
          [&store_seed_result, &run_loop](bool result) {
            store_seed_result = result;
            run_loop.Quit();
          }),
      new_seed_data, "a completely ignored signature",
      /*seed_milestone=*/92, *client_state, fetch_time);
  run_loop.Run();
  ASSERT_TRUE(store_seed_result);

  // Verify the latest seed value was copied before the safe seed was
  // overwritten.
  EXPECT_EQ(base64_old_seed,
            prefs_.GetString(prefs::kVariationsCompressedSeed));
  std::string stored_latest_seed;
  LoadSeedResult read_result =
      seed_store.GetSeedReaderWriterForTesting()->ReadSeedDataOnStartup(
          &stored_latest_seed, /*base64_seed_signature=*/nullptr);
  ASSERT_EQ(read_result, LoadSeedResult::kSuccess);
  EXPECT_EQ(stored_latest_seed, old_seed_data);

  // Verify that loading the stored seed returns the old seed value.
  VariationsSeed loaded_seed;
  std::string loaded_seed_data;
  std::string unused_loaded_base64_seed_signature;
  ASSERT_TRUE(seed_store.LoadSeedSync(&loaded_seed, &loaded_seed_data,
                                      &unused_loaded_base64_seed_signature));

  EXPECT_EQ(old_seed_data, SerializeSeed(loaded_seed));
  EXPECT_EQ(old_seed_data, loaded_seed_data);

  // Verify that the safe seed prefs indeed contain the new seed's serialized
  // value.
  EXPECT_EQ(GzipAndBase64Encode(new_seed_data),
            prefs_.GetString(prefs::kVariationsSafeCompressedSeed));
  std::string stored_safe_seed;
  LoadSeedResult safe_seed_read_result =
      seed_store.GetSafeSeedReaderWriterForTesting()->ReadSeedDataOnStartup(
          &stored_safe_seed, /*base64_seed_signature=*/nullptr);
  ASSERT_EQ(safe_seed_read_result, LoadSeedResult::kSuccess);
  EXPECT_EQ(stored_safe_seed, new_seed_data);

  VariationsSeed loaded_safe_seed;
  ASSERT_TRUE(
      seed_store.LoadSafeSeedSync(&loaded_safe_seed, client_state.get()));
  EXPECT_EQ(SerializeSeed(new_seed), SerializeSeed(loaded_safe_seed));
  EXPECT_EQ(fetch_time, seed_store.GetSafeSeedFetchTime());

  // Verify metrics.
  histogram_tester.ExpectUniqueSample(
      "Variations.SafeMode.StoreSafeSeed.Result", StoreSeedResult::kSuccess, 1);
}

INSTANTIATE_TEST_SUITE_P(
    All,
    StoreSafeSeedDataAllGroupsTest,
    ::testing::ConvertGenerator<StoreSeedDataTestParams::TupleT>(
        ::testing::Combine(::testing::Bool(),
                           ::testing::Values(kSeedFilesGroup,
                                             kControlGroup,
                                             kDefaultGroup,
                                             kNoGroup))));

TEST_P(StoreSafeSeedDataAllGroupsTest, StoreSafeSeed_IdenticalToLatestSeed) {
  const VariationsSeed seed = CreateTestSeed();
  const std::string serialized_seed = SerializeSeed(seed);
  const std::string compressed_seed = Gzip(serialized_seed);
  const std::string base64_seed = SerializeSeedBase64(seed);
  auto client_state = CreateDummyClientFilterableState();
  const base::Time last_fetch_time = WrapTime(99999);

  TestVariationsSeedStore seed_store(&prefs_, temp_dir_.GetPath());
  ASSERT_EQ(base::FieldTrialList::FindFullName(kSeedFileTrial),
            GetParam().field_trial_group);
  seed_store.GetSeedReaderWriterForTesting()->StoreValidatedSeedInfo(
      ValidatedSeedInfo{
          .seed_data = serialized_seed,
          .signature = "ignored signature",
          .milestone = 92,
          .seed_date = client_state->reference_date,
          .client_fetch_time = last_fetch_time,
          .session_country_code = "us",
      });
  const std::string expected_seed =
      GetParam().field_trial_group == kSeedFilesGroup ? compressed_seed
                                                      : base64_seed;
  base::HistogramTester histogram_tester;
  base::RunLoop run_loop;
  bool store_seed_result = false;
  seed_store.StoreSafeSeed(
      /*done_callback=*/base::BindLambdaForTesting(
          [&store_seed_result, &run_loop](bool result) {
            store_seed_result = result;
            run_loop.Quit();
          }),
      serialized_seed, "a completely ignored signature", /*seed_milestone=*/92,
      *client_state, /*seed_fetch_time=*/WrapTime(12345));
  run_loop.Run();
  ASSERT_TRUE(store_seed_result);

  // Verify the latest seed value was migrated to a sentinel value, rather than
  // the full string.
  if (ShouldUseLocalStateSeed()) {
    EXPECT_EQ(kIdenticalToSafeSeedSentinel,
              prefs_.GetString(prefs::kVariationsCompressedSeed));
  }
  std::string stored_seed;
  LoadSeedResult read_result =
      seed_store.GetSeedReaderWriterForTesting()->ReadSeedDataOnStartup(
          &stored_seed, /*base64_seed_signature=*/nullptr);
  ASSERT_EQ(read_result, LoadSeedResult::kSuccess);
  EXPECT_EQ(stored_seed, kIdenticalToSafeSeedSentinel);

  // Verify that loading the stored seed returns the original seed value.
  VariationsSeed loaded_seed;
  std::string loaded_seed_data;
  std::string unused_loaded_base64_seed_signature;
  ASSERT_TRUE(seed_store.LoadSeedSync(&loaded_seed, &loaded_seed_data,
                                      &unused_loaded_base64_seed_signature));

  EXPECT_EQ(serialized_seed, SerializeSeed(loaded_seed));
  EXPECT_EQ(serialized_seed, loaded_seed_data);

  // Verify that the safe seed from prefs and SeedReaderWriter is unchanged
  // and that the last fetch time was copied from the latest seed.
  if (ShouldUseLocalStateSeed()) {
    EXPECT_EQ(base64_seed,
              prefs_.GetString(prefs::kVariationsSafeCompressedSeed));
  }
  std::string stored_safe_seed;
  LoadSeedResult safe_seed_read_result =
      seed_store.GetSafeSeedReaderWriterForTesting()->ReadSeedDataOnStartup(
          &stored_safe_seed, /*base64_seed_signature=*/nullptr);
  ASSERT_EQ(safe_seed_read_result, LoadSeedResult::kSuccess);
  EXPECT_EQ(stored_safe_seed, serialized_seed);

  VariationsSeed loaded_safe_seed;
  EXPECT_TRUE(
      seed_store.LoadSafeSeedSync(&loaded_safe_seed, client_state.get()));
  EXPECT_EQ(serialized_seed, SerializeSeed(loaded_safe_seed));
  EXPECT_EQ(last_fetch_time, seed_store.GetSafeSeedFetchTime());

  // Verify metrics.
  histogram_tester.ExpectUniqueSample(
      "Variations.SafeMode.StoreSafeSeed.Result", StoreSeedResult::kSuccess, 1);
}

class LoadSeedDataAllGroupsTest : public LoadSeedDataGroupTest {
 protected:
  void SetUp() override {
    ASSERT_TRUE(base::Base64Decode(kTestSeedData.base64_uncompressed_data,
                                   &seed_data_));
  }

  // Stores the seed data to the given seed store.
  // If |test_signature| is empty, the default test signature is used.
  // If |seed_data| is nullptr, the test's default seed data is used.
  void StoreValidatedSeed(
      TestVariationsSeedStore& seed_store,
      std::string_view test_signature = kTestSeedData.base64_signature,
      const std::string* seed_data = nullptr) {
    if (seed_data == nullptr) {
      seed_data = &seed_data_;
    }
    ASSERT_TRUE(seed_data != nullptr);
    VariationsSeed seed;
    ASSERT_TRUE(seed.ParseFromString(*seed_data));
    seed_store.GetSeedReaderWriterForTesting()->StoreValidatedSeedInfo(
        ValidatedSeedInfo{
            .seed_data = *seed_data,
            .signature = test_signature,
            .milestone = 1,
            .seed_date = base::Time::Now(),
            .client_fetch_time = base::Time::Now(),
            .session_country_code = "us",
        });
  }

  std::string seed_data_;
};

INSTANTIATE_TEST_SUITE_P(
    All,
    LoadSeedDataAllGroupsTest,
    ::testing::Values(kSeedFilesGroup, kControlGroup, kDefaultGroup, kNoGroup));

TEST_P(LoadSeedDataAllGroupsTest, VerifySeedSignatureSignatureIsValid) {
  TestVariationsSeedStore seed_store(&prefs_, temp_dir_.GetPath(),
                                     /*signature_verification_needed=*/true);
  ASSERT_EQ(base::FieldTrialList::FindFullName(kSeedFileTrial), GetParam());

  StoreValidatedSeed(seed_store);

  base::HistogramTester histogram_tester;
  ASSERT_TRUE(MakeSeedStoreLoadStoredSeed(seed_store));
  histogram_tester.ExpectUniqueSample("Variations.LoadSeedSignature",
                                      VerifySignatureResult::kValidSignature,
                                      1);
}

TEST_P(LoadSeedDataAllGroupsTest, VerifySeedSignatureSignatureIsMissing) {
  TestVariationsSeedStore seed_store(&prefs_, temp_dir_.GetPath(),
                                     /*signature_verification_needed=*/true);
  ASSERT_EQ(base::FieldTrialList::FindFullName(kSeedFileTrial), GetParam());

  StoreValidatedSeed(seed_store, /*test_signature=*/std::string());

  base::HistogramTester histogram_tester;
  ASSERT_FALSE(MakeSeedStoreLoadStoredSeed(seed_store));
  histogram_tester.ExpectUniqueSample("Variations.LoadSeedSignature",
                                      VerifySignatureResult::kMissingSignature,
                                      1);
}

TEST_P(LoadSeedDataAllGroupsTest,
       VerifySeedSignatureSignatureNotBase64Encoded) {
  TestVariationsSeedStore seed_store(&prefs_, temp_dir_.GetPath(),
                                     /*signature_verification_needed=*/true);
  ASSERT_EQ(base::FieldTrialList::FindFullName(kSeedFileTrial), GetParam());

  StoreValidatedSeed(seed_store,
                     /*test_signature=*/"not a base64-encoded string");

  base::HistogramTester histogram_tester;
  ASSERT_FALSE(MakeSeedStoreLoadStoredSeed(seed_store));
  histogram_tester.ExpectUniqueSample("Variations.LoadSeedSignature",
                                      VerifySignatureResult::kDecodeFailed, 1);
}

TEST_P(LoadSeedDataAllGroupsTest, VerifySeedSignatureSignatureDoesNotMatch) {
  // Using a different signature (e.g. the base64 seed data) should fail.
  // OpenSSL doesn't distinguish signature decode failure from the
  // signature not matching.
  VariationsSeed seed;
  ASSERT_TRUE(seed.ParseFromString(seed_data_));
  std::string base64_seed_data = SerializeSeedBase64(seed);

  TestVariationsSeedStore seed_store(&prefs_, temp_dir_.GetPath(),
                                     /*signature_verification_needed=*/true);
  ASSERT_EQ(base::FieldTrialList::FindFullName(kSeedFileTrial), GetParam());
  StoreValidatedSeed(seed_store, /*test_signature=*/base64_seed_data);

  base::HistogramTester histogram_tester;
  ASSERT_FALSE(MakeSeedStoreLoadStoredSeed(seed_store));
  histogram_tester.ExpectUniqueSample("Variations.LoadSeedSignature",
                                      VerifySignatureResult::kInvalidSeed, 1);
}

TEST_P(LoadSeedDataAllGroupsTest, VerifySeedSignatureSeedDoesNotMatch) {
  const std::string base64_seed_signature = kTestSeedData.base64_signature;

  VariationsSeed wrong_seed;
  ASSERT_TRUE(wrong_seed.ParseFromString(seed_data_));
  (*wrong_seed.mutable_study(0)->mutable_name())[0] = 'x';
  const std::string wrong_seed_data = SerializeSeed(wrong_seed);

  TestVariationsSeedStore seed_store(&prefs_, temp_dir_.GetPath(),
                                     /*signature_verification_needed=*/true);
  ASSERT_EQ(base::FieldTrialList::FindFullName(kSeedFileTrial), GetParam());
  StoreValidatedSeed(seed_store, /*test_signature=*/base64_seed_signature,
                     /*seed_data=*/&wrong_seed_data);

  base::HistogramTester histogram_tester;
  ASSERT_FALSE(MakeSeedStoreLoadStoredSeed(seed_store));
  histogram_tester.ExpectUniqueSample("Variations.LoadSeedSignature",
                                      VerifySignatureResult::kInvalidSeed, 1);
}

class VariationsSeedStoreTestAllGroups
    : public SeedStoreGroupTestBase,
      public ::testing::WithParamInterface<std::string_view> {
 public:
  explicit VariationsSeedStoreTestAllGroups()
      : SeedStoreGroupTestBase(kRegularSeedFieldsPrefs, GetParam()) {}
  ~VariationsSeedStoreTestAllGroups() override = default;
};

INSTANTIATE_TEST_SUITE_P(
    All,
    VariationsSeedStoreTestAllGroups,
    ::testing::Values(kSeedFilesGroup, kControlGroup, kDefaultGroup, kNoGroup));

TEST_P(VariationsSeedStoreTestAllGroups, LastFetchTime_DistinctSeeds) {
  base::Time start_time = WrapTime(10);
  TestVariationsSeedStore seed_store(&prefs_, temp_dir_.GetPath());
  ASSERT_EQ(base::FieldTrialList::FindFullName(kSeedFileTrial), GetParam());
  seed_store.GetSeedReaderWriterForTesting()->StoreValidatedSeedInfo(
      ValidatedSeedInfo{
          .seed_data = "one",
          .signature = "ignored signature",
          .milestone = 1,
          .seed_date = base::Time::Now(),
          .client_fetch_time = WrapTime(2),
          .session_country_code = "us",
      });
  seed_store.GetSafeSeedReaderWriterForTesting()->StoreValidatedSeedInfo(
      ValidatedSeedInfo{
          .seed_data = "not one",
          .signature = "ignored signature",
          .milestone = 2,
          .seed_date = base::Time::Now(),
          .client_fetch_time = WrapTime(1),
          .session_country_code = "us",
          .permanent_country_code = "us",
      });
  seed_store.RecordLastFetchTime(WrapTime(11));

  // Verify that the last fetch time was updated.
  const base::Time last_fetch_time = seed_store.GetLatestSeedFetchTime();
  EXPECT_EQ(WrapTime(11), last_fetch_time);
  EXPECT_GE(last_fetch_time, start_time);

  // Verify that the safe seed's fetch time was *not* updated.
  const base::Time safe_fetch_time = seed_store.GetSafeSeedFetchTime();
  EXPECT_EQ(WrapTime(1), safe_fetch_time);
}

TEST_P(VariationsSeedStoreTestAllGroups, LastFetchTime_IdenticalSeeds) {
  base::Time start_time = WrapTime(10);
  TestVariationsSeedStore seed_store(&prefs_, temp_dir_.GetPath());
  ASSERT_EQ(base::FieldTrialList::FindFullName(kSeedFileTrial), GetParam());
  seed_store.GetSeedReaderWriterForTesting()->StoreValidatedSeedInfo(
      ValidatedSeedInfo{
          .seed_data = kIdenticalToSafeSeedSentinel,
          .signature = "ignored signature",
          .milestone = 1,
          .seed_date = WrapTime(1),
          .client_fetch_time = WrapTime(1),
          .session_country_code = "us",
      });
  seed_store.GetSafeSeedReaderWriterForTesting()->StoreValidatedSeedInfo(
      ValidatedSeedInfo{
          .seed_data = "some seed",
          .signature = "ignored signature",
          .milestone = 1,
          .seed_date = WrapTime(1),
          .client_fetch_time = WrapTime(0),
          .session_country_code = "us",
          .permanent_country_code = "us",
      });
  seed_store.RecordLastFetchTime(WrapTime(11));

  // Verify that the last fetch time was updated.
  const base::Time last_fetch_time = seed_store.GetLatestSeedFetchTime();
  EXPECT_EQ(WrapTime(11), last_fetch_time);
  EXPECT_GE(last_fetch_time, start_time);

  // Verify that the safe seed's fetch time *was* also updated.
  const base::Time safe_fetch_time = seed_store.GetSafeSeedFetchTime();
  EXPECT_EQ(WrapTime(11), safe_fetch_time);
}

TEST_F(VariationsSeedStoreTest, GetLatestSerialNumber_EmptyWhenNoSeedIsSaved) {
  // Start with empty prefs.
  TestingPrefServiceSimple prefs;
  VariationsSeedStore::RegisterPrefs(prefs.registry());

  TestVariationsSeedStore seed_store(&prefs);
  EXPECT_EQ(std::string(), seed_store.GetLatestSerialNumber());
}

// Verifies that GetTimeForStudyDateChecks() returns the server timestamp for
// when the regular seed was fetched,|kVariationsSeedDate|, when the time is
// more recent than the build time.
// TODO(crbug.com/380465790): Store seed_fetch_time in seed file instead of
// local state when it's moved there.
TEST_F(VariationsSeedStoreTest, RegularSeedTimeReturned) {
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
// TODO(crbug.com/380465790): Store seed_fetch_time in seed file instead of
// local state when it's moved there.
TEST_F(VariationsSeedStoreTest, SafeSeedTimeReturned) {
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
// TODO(crbug.com/380465790): Store seed_fetch_time in seed file instead of
// local state when it's moved there.
TEST_F(VariationsSeedStoreTest, BuildTimeReturnedForRegularSeed) {
  TestingPrefServiceSimple prefs;
  VariationsSeedStore::RegisterPrefs(prefs.registry());
  prefs.SetTime(prefs::kVariationsSeedDate,
                base::GetBuildTime() - base::Days(2));

  TestVariationsSeedStore seed_store(&prefs);
  EXPECT_EQ(seed_store.GetTimeForStudyDateChecks(/*is_safe_seed=*/false),
            base::GetBuildTime());
}

// Verifies that GetTimeForStudyDateChecks() returns the build time when it is
// more recent than |kVariationsSafeSeedDate|.
// TODO(crbug.com/380465790): Store seed_fetch_time in seed file instead of
// local state when it's moved there.
TEST_F(VariationsSeedStoreTest, BuildTimeReturnedForSafeSeed) {
  TestingPrefServiceSimple prefs;
  VariationsSeedStore::RegisterPrefs(prefs.registry());
  prefs.SetTime(prefs::kVariationsSeedDate,
                base::GetBuildTime() - base::Days(3));

  TestVariationsSeedStore seed_store(&prefs);
  EXPECT_EQ(seed_store.GetTimeForStudyDateChecks(/*is_safe_seed=*/true),
            base::GetBuildTime());
}

// Verifies that GetTimeForStudyDateChecks() returns the build time when the
// seed time is null.
// TODO(crbug.com/380465790): Store seed_fetch_time in seed file instead of
// local state when it's moved there.
TEST_F(VariationsSeedStoreTest, BuildTimeReturnedForNullSeedTimes) {
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

struct DatesTestParams {
  base::Time old_seed_date;
  base::Time new_seed_date;
  UpdateSeedDateResult expected_result;
};

using VariationsSeedStoreTestAllGroupsDatesParams =
    std::tuple<std::string_view, DatesTestParams>;

// Class for testing the UpdateSeedDateAndLogDayChange() method. Test for all
// experiment groups. Test for different date combinations:
// - No old date.
// - New date is more recent than old date.
// - New date is the same as old date.
// - New date is older than old date.
class VariationsSeedStoreTestAllGroupsDates
    : public SeedStoreGroupTestBase,
      public ::testing::WithParamInterface<
          VariationsSeedStoreTestAllGroupsDatesParams> {
 public:
  explicit VariationsSeedStoreTestAllGroupsDates()
      : SeedStoreGroupTestBase(kRegularSeedFieldsPrefs,
                               std::get<0>(GetParam())) {}
  ~VariationsSeedStoreTestAllGroupsDates() override = default;
};

INSTANTIATE_TEST_SUITE_P(
    All,
    VariationsSeedStoreTestAllGroupsDates,
    ::testing::ConvertGenerator<VariationsSeedStoreTestAllGroupsDatesParams>(
        ::testing::Combine(
            ::testing::Values(kSeedFilesGroup,
                              kControlGroup,
                              kDefaultGroup,
                              kNoGroup),
            ::testing::Values(
                DatesTestParams{
                    .old_seed_date = base::Time(),
                    .new_seed_date = base::Time::Now(),
                    .expected_result = UpdateSeedDateResult::kNoOldDate,
                },
                DatesTestParams{
                    .old_seed_date = base::Time::Now() - base::Days(1),
                    .new_seed_date = base::Time::Now(),
                    .expected_result = UpdateSeedDateResult::kNewDay,
                },
                DatesTestParams{
                    .old_seed_date = base::Time::FromSecondsSinceUnixEpoch(5),
                    .new_seed_date = base::Time::FromSecondsSinceUnixEpoch(10),
                    .expected_result = UpdateSeedDateResult::kSameDay,
                },
                DatesTestParams{
                    .old_seed_date = base::Time::Now(),
                    .new_seed_date = base::Time::Now() - base::Days(1),
                    .expected_result = UpdateSeedDateResult::kNewDateIsOlder,
                }))));

// UpdateSeedDateAndLogDayChange() updates the seed date and logs the result.
TEST_P(VariationsSeedStoreTestAllGroupsDates, UpdateSeedDateAndLogDayChange) {
  TestingPrefServiceSimple prefs;
  VariationsSeedStore::RegisterPrefs(prefs.registry());
  TestVariationsSeedStore seed_store(&prefs);
  DatesTestParams params = std::get<1>(GetParam());
  if (!params.old_seed_date.is_null()) {
    seed_store.UpdateSeedDateAndLogDayChange(params.old_seed_date);
    const base::Time stored_seed_date = GetSeedInfo(seed_store).seed_date;
    ASSERT_EQ(stored_seed_date, params.old_seed_date);
  } else {
    ASSERT_TRUE(GetSeedInfo(seed_store).seed_date.is_null());
  }

  base::HistogramTester histogram_tester;
  seed_store.UpdateSeedDateAndLogDayChange(params.new_seed_date);

  // Verify that the seed date is updated.
  base::Time stored_seed_date = GetSeedInfo(seed_store).seed_date;
  EXPECT_EQ(stored_seed_date, params.new_seed_date);

  // Verify that the day change is logged.
  histogram_tester.ExpectUniqueSample("Variations.SeedDateChange",
                                      params.expected_result, 1);
}

#if BUILDFLAG(IS_ANDROID)
TEST_F(VariationsSeedStoreTest, ImportFirstRunJavaSeed) {
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
    : public ::testing::TestWithParam<bool> {
 private:
  base::test::TaskEnvironment task_environment_;
};

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
  auto seed = std::make_unique<SeedResponse>();
  seed->data = seed_data;
  seed->signature = "java_seed_signature";
  seed->country = "java_seed_country";
  seed->date = base::Time::FromMillisecondsSinceUnixEpoch(test_response_date) +
               base::Days(1);
  seed->is_gzip_compressed = false;

  TestingPrefServiceSimple prefs;
  VariationsSeedStore::RegisterPrefs(prefs.registry());
  TestVariationsSeedStore seed_store(&prefs,
                                     /*seed_file_dir=*/base::FilePath(),
                                     /*signature_verification_needed=*/false,
                                     /*initial_seed=*/std::move(seed),
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
  EXPECT_EQ(SerializeSeedBase64(test_seed),
            prefs.GetString(prefs::kVariationsCompressedSeed));
}
#endif  // BUILDFLAG(IS_ANDROID)

}  // namespace variations
