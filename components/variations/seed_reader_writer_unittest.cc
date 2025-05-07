// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/variations/seed_reader_writer.h"

#include "base/base64.h"
#include "base/files/file_util.h"
#include "base/files/scoped_temp_dir.h"
#include "base/test/metrics/histogram_tester.h"
#include "base/test/mock_entropy_provider.h"
#include "base/test/scoped_feature_list.h"
#include "base/test/task_environment.h"
#include "base/threading/thread.h"
#include "base/timer/mock_timer.h"
#include "base/version_info/channel.h"
#include "components/prefs/testing_pref_service.h"
#include "components/variations/pref_names.h"
#include "components/variations/variations_seed_store.h"
#include "components/variations/variations_test_utils.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/zlib/google/compression_utils.h"

namespace variations {
namespace {

using ::testing::IsEmpty;
using ::testing::TestWithParam;
using ::testing::Values;

const base::FilePath::CharType kSeedFilename[] = FILE_PATH_LITERAL("TestSeed");

// Used for clients that do not participate in SeedFiles experiment.
constexpr char kNoGroup[] = "";

// Compresses `data` using Gzip compression.
std::string Gzip(const std::string& data) {
  std::string compressed;
  CHECK(compression::GzipCompress(data, &compressed));
  return compressed;
}

// Creates, serializes, and then Gzip compresses a test seed.
std::string CreateCompressedVariationsSeed() {
  VariationsSeed seed;
  seed.add_study()->set_name("TestStudy");
  std::string serialized_seed;
  seed.SerializeToString(&serialized_seed);
  return Gzip(serialized_seed);
}

struct SeedReaderWriterTestParams {
  using TupleT =
      std::tuple<SeedFieldsPrefs, std::string_view, version_info::Channel>;

  SeedReaderWriterTestParams(SeedFieldsPrefs seed_fields_prefs,
                             std::string_view field_trial_group,
                             version_info::Channel channel)
      : seed_fields_prefs(seed_fields_prefs),
        field_trial_group(field_trial_group),
        channel(channel) {}

  explicit SeedReaderWriterTestParams(const TupleT& t)
      : SeedReaderWriterTestParams(std::get<0>(t),
                                   std::get<1>(t),
                                   std::get<2>(t)) {}

  SeedFieldsPrefs seed_fields_prefs;
  std::string_view field_trial_group;
  version_info::Channel channel;
};

struct ExpectedFieldTrialGroupTestParams {
  using TupleT = std::tuple<SeedFieldsPrefs, version_info::Channel>;

  ExpectedFieldTrialGroupTestParams(
      variations::SeedFieldsPrefs seed_fields_prefs,
      version_info::Channel channel)
      : seed_fields_prefs(seed_fields_prefs), channel(channel) {}

  explicit ExpectedFieldTrialGroupTestParams(const TupleT& t)
      : ExpectedFieldTrialGroupTestParams(std::get<0>(t), std::get<1>(t)) {}

  variations::SeedFieldsPrefs seed_fields_prefs;
  version_info::Channel channel;
};

class SeedReaderWriterTestBase {
 public:
  SeedReaderWriterTestBase()
      : file_writer_thread_("SeedReaderWriter Test thread"),
        entropy_providers_(std::make_unique<const MockEntropyProviders>(
            MockEntropyProviders::Results{.low_entropy =
                                              kAlwaysUseLastGroup})) {
    VariationsSeedStore::RegisterPrefs(local_state_.registry());
    scoped_feature_list_.InitWithEmptyFeatureAndFieldTrialLists();
    file_writer_thread_.Start();
    CHECK(temp_dir_.CreateUniqueTempDir());
    temp_seed_file_path_ = temp_dir_.GetPath().Append(kSeedFilename);
  }
  ~SeedReaderWriterTestBase() = default;

 protected:
  base::test::ScopedFeatureList scoped_feature_list_;
  base::test::TaskEnvironment task_environment_;
  base::FilePath temp_seed_file_path_;
  base::Thread file_writer_thread_;
  base::ScopedTempDir temp_dir_;
  TestingPrefServiceSimple local_state_;
  base::MockOneShotTimer timer_;
  std::unique_ptr<const MockEntropyProviders> entropy_providers_;
};

class ExpectedFieldTrialGroupChannelsTest
    : public SeedReaderWriterTestBase,
      public TestWithParam<ExpectedFieldTrialGroupTestParams> {};

class ExpectedFieldTrialGroupAllChannelsTest
    : public ExpectedFieldTrialGroupChannelsTest {};
class ExpectedFieldTrialGroupPreStableTest
    : public ExpectedFieldTrialGroupChannelsTest {};
class ExpectedFieldTrialGroupStableTest
    : public SeedReaderWriterTestBase,
      public TestWithParam<SeedFieldsPrefs> {};
class ExpectedFieldTrialGroupUnknownTest
    : public SeedReaderWriterTestBase,
      public TestWithParam<SeedFieldsPrefs> {};

INSTANTIATE_TEST_SUITE_P(
    All,
    ExpectedFieldTrialGroupAllChannelsTest,
    ::testing::ConvertGenerator<ExpectedFieldTrialGroupTestParams::TupleT>(
        ::testing::Combine(::testing::Values(kRegularSeedFieldsPrefs,
                                             kSafeSeedFieldsPrefs),
                           ::testing::Values(version_info::Channel::UNKNOWN,
                                             version_info::Channel::CANARY,
                                             version_info::Channel::DEV,
                                             version_info::Channel::BETA,
                                             version_info::Channel::STABLE))));

// If empty seed file dir given, client is not assigned a group.
TEST_P(ExpectedFieldTrialGroupAllChannelsTest, NoSeedFileDir) {
  SeedReaderWriter seed_reader_writer(
      &local_state_, /*seed_file_dir=*/base::FilePath(), kSeedFilename,
      GetParam().seed_fields_prefs, GetParam().channel,
      entropy_providers_.get(), file_writer_thread_.task_runner());
  EXPECT_THAT(base::FieldTrialList::FindFullName(kSeedFileTrial), IsEmpty());
}

// If no entropy provider given, client is not assigned a group.
TEST_P(ExpectedFieldTrialGroupAllChannelsTest, NoEntropyProvider) {
  SeedReaderWriter seed_reader_writer(
      &local_state_, /*seed_file_dir=*/temp_dir_.GetPath(), kSeedFilename,
      GetParam().seed_fields_prefs, GetParam().channel,
      /*entropy_providers=*/nullptr, file_writer_thread_.task_runner());
  EXPECT_THAT(base::FieldTrialList::FindFullName(kSeedFileTrial), IsEmpty());
}

INSTANTIATE_TEST_SUITE_P(
    All,
    ExpectedFieldTrialGroupPreStableTest,
    ::testing::ConvertGenerator<ExpectedFieldTrialGroupTestParams::TupleT>(
        ::testing::Combine(::testing::Values(kRegularSeedFieldsPrefs,
                                             kSafeSeedFieldsPrefs),
                           ::testing::Values(version_info::Channel::CANARY,
                                             version_info::Channel::DEV,
                                             version_info::Channel::BETA))));

// If channel is pre-stable, client is assigned a group.
TEST_P(ExpectedFieldTrialGroupPreStableTest, PreStable) {
  SeedReaderWriter seed_reader_writer(
      &local_state_, /*seed_file_dir=*/temp_dir_.GetPath(), kSeedFilename,
      GetParam().seed_fields_prefs, GetParam().channel,
      entropy_providers_.get(), file_writer_thread_.task_runner());
  EXPECT_THAT(base::FieldTrialList::FindFullName(kSeedFileTrial),
              ::testing::AnyOf(kControlGroup, kSeedFilesGroup));
}

INSTANTIATE_TEST_SUITE_P(All,
                         ExpectedFieldTrialGroupStableTest,
                         ::testing::Values(kRegularSeedFieldsPrefs,
                                           kSafeSeedFieldsPrefs));

// If channel is stable, trial has been registered.
TEST_P(ExpectedFieldTrialGroupStableTest, Stable) {
  SeedReaderWriter seed_reader_writer(
      &local_state_, /*seed_file_dir=*/temp_dir_.GetPath(), kSeedFilename,
      GetParam(), version_info::Channel::STABLE, entropy_providers_.get(),
      file_writer_thread_.task_runner());
  EXPECT_TRUE(base::FieldTrialList::TrialExists(kSeedFileTrial));
}

INSTANTIATE_TEST_SUITE_P(All,
                         ExpectedFieldTrialGroupUnknownTest,
                         ::testing::Values(kRegularSeedFieldsPrefs,
                                           kSafeSeedFieldsPrefs));

// If channel is unknown, client is not assigned a group.
TEST_P(ExpectedFieldTrialGroupUnknownTest, Unknown) {
  SeedReaderWriter seed_reader_writer(
      &local_state_, /*seed_file_dir=*/temp_dir_.GetPath(), kSeedFilename,
      GetParam(), version_info::Channel::UNKNOWN, entropy_providers_.get(),
      file_writer_thread_.task_runner());
  EXPECT_THAT(base::FieldTrialList::FindFullName(kSeedFileTrial), IsEmpty());
}

class SeedReaderWriterGroupTest
    : public SeedReaderWriterTestBase,
      public TestWithParam<SeedReaderWriterTestParams> {
 public:
  SeedReaderWriterGroupTest() {
    SetUpSeedFileTrial(std::string(GetParam().field_trial_group));
  }
};
class SeedReaderWriterSeedFilesGroupTest : public SeedReaderWriterGroupTest {};
class SeedReaderWriterLocalStateGroupsTest : public SeedReaderWriterGroupTest {
};
class SeedReaderWriterAllGroupsTest : public SeedReaderWriterGroupTest {};

// Verifies clients in SeedFiles group write seeds to a seed file.
TEST_P(SeedReaderWriterSeedFilesGroupTest, WriteSeed) {
  ASSERT_EQ(base::FieldTrialList::FindFullName(kSeedFileTrial),
            GetParam().field_trial_group);
  // Initialize seed_reader_writer with test thread and timer.
  SeedReaderWriter seed_reader_writer(
      &local_state_, /*seed_file_dir=*/temp_dir_.GetPath(), kSeedFilename,
      GetParam().seed_fields_prefs, GetParam().channel,
      entropy_providers_.get(), file_writer_thread_.task_runner());
  seed_reader_writer.SetTimerForTesting(&timer_);

  // Create and store seed.
  const std::string compressed_seed = CreateCompressedVariationsSeed();
  const std::string base64_compressed_seed =
      base::Base64Encode(compressed_seed);
  seed_reader_writer.StoreValidatedSeedInfo(
      ValidatedSeedInfo{.compressed_seed_data = compressed_seed,
                        .base64_seed_data = base64_compressed_seed,
                        .signature = "signature",
                        .milestone = 2});

  // Force write.
  timer_.Fire();
  file_writer_thread_.FlushForTesting();

  // Verify that a seed was written to a seed file.
  std::string seed_file_data;
  ASSERT_TRUE(base::ReadFileToString(temp_seed_file_path_, &seed_file_data));
  EXPECT_EQ(seed_file_data, compressed_seed);
}

// Verifies that a seed is cleared from a seed file for clients in the SeedFiles
// group.
TEST_P(SeedReaderWriterSeedFilesGroupTest, ClearSeed) {
  ASSERT_EQ(base::FieldTrialList::FindFullName(kSeedFileTrial),
            GetParam().field_trial_group);
  // Initialize seed_reader_writer with test thread and timer.
  SeedReaderWriter seed_reader_writer(
      &local_state_, /*seed_file_dir=*/temp_dir_.GetPath(), kSeedFilename,
      GetParam().seed_fields_prefs, GetParam().channel,
      entropy_providers_.get(), file_writer_thread_.task_runner());
  seed_reader_writer.SetTimerForTesting(&timer_);

  // Create and store seed.
  const std::string compressed_seed = CreateCompressedVariationsSeed();
  ASSERT_TRUE(base::WriteFile(temp_seed_file_path_, compressed_seed));

  // Clear seed and force write.
  seed_reader_writer.ClearSeedInfo();
  timer_.Fire();
  file_writer_thread_.FlushForTesting();

  // Verify seed cleared correctly in a seed file.
  std::string seed_file_data;
  ASSERT_TRUE(base::ReadFileToString(temp_seed_file_path_, &seed_file_data));
  EXPECT_THAT(seed_file_data, IsEmpty());
}

// Verifies clients in SeedFiles group read seeds from the seed file.
TEST_P(SeedReaderWriterSeedFilesGroupTest, ReadSeedFileBasedSeed) {
  ASSERT_EQ(base::FieldTrialList::FindFullName(kSeedFileTrial),
            GetParam().field_trial_group);
  // Create and store seed.
  const std::string compressed_seed = CreateCompressedVariationsSeed();
  ASSERT_TRUE(base::WriteFile(temp_seed_file_path_, compressed_seed));
  const std::string_view seed_data_field = GetParam().seed_fields_prefs.seed;
  local_state_.SetString(seed_data_field, "unused seed");

  // Initialize seed_reader_writer with test thread.
  base::HistogramTester histogram_tester;
  SeedReaderWriter seed_reader_writer(
      &local_state_, /*seed_file_dir=*/temp_dir_.GetPath(), kSeedFilename,
      GetParam().seed_fields_prefs, GetParam().channel,
      entropy_providers_.get(), file_writer_thread_.task_runner());

  // Ensure seed data loaded from seed file.
  ASSERT_EQ(StoredSeed::StorageFormat::kCompressed,
            seed_reader_writer.GetSeedData().storage_format);
  ASSERT_EQ(compressed_seed, seed_reader_writer.GetSeedData().data);
  histogram_tester.ExpectUniqueSample(
      base::StrCat(
          {"Variations.SeedFileRead.",
           base::Contains(seed_data_field, "Safe") ? "Safe" : "Latest"}),
      /*sample=*/1, /*expected_bucket_count=*/1);
}

// Verifies clients in SeedFiles group do not crash if reading empty seed file.
TEST_P(SeedReaderWriterSeedFilesGroupTest, ReadEmptySeedFile) {
  ASSERT_EQ(base::FieldTrialList::FindFullName(kSeedFileTrial),
            GetParam().field_trial_group);
  // Create and store seed.
  const std::string compressed_seed = CreateCompressedVariationsSeed();
  ASSERT_TRUE(base::WriteFile(temp_seed_file_path_, ""));
  const std::string_view seed_data_field = GetParam().seed_fields_prefs.seed;
  local_state_.SetString(seed_data_field, "unused seed");

  // Initialize seed_reader_writer with test thread.
  base::HistogramTester histogram_tester;
  SeedReaderWriter seed_reader_writer(
      &local_state_, /*seed_file_dir=*/temp_dir_.GetPath(), kSeedFilename,
      GetParam().seed_fields_prefs, GetParam().channel,
      entropy_providers_.get(), file_writer_thread_.task_runner());

  histogram_tester.ExpectUniqueSample(
      base::StrCat(
          {"Variations.SeedFileRead.",
           base::Contains(seed_data_field, "Safe") ? "Safe" : "Latest"}),
      /*sample=*/1, /*expected_bucket_count=*/1);

  // Ensure seed data loaded from seed file.
  ASSERT_EQ(StoredSeed::StorageFormat::kCompressed,
            seed_reader_writer.GetSeedData().storage_format);
  ASSERT_EQ("", seed_reader_writer.GetSeedData().data);
}

// Verifies clients in SeedFiles group read seeds from local state prefs if no
// seed file found.
TEST_P(SeedReaderWriterSeedFilesGroupTest, ReadMissingSeedFile) {
  ASSERT_EQ(base::FieldTrialList::FindFullName(kSeedFileTrial),
            GetParam().field_trial_group);
  // Create and store seed.
  const std::string compressed_seed = CreateCompressedVariationsSeed();
  const std::string_view seed_data_field = GetParam().seed_fields_prefs.seed;
  local_state_.SetString(seed_data_field, base::Base64Encode(compressed_seed));

  // Initialize seed_reader_writer with test thread.
  base::HistogramTester histogram_tester;
  SeedReaderWriter seed_reader_writer(
      &local_state_, /*seed_file_dir=*/temp_dir_.GetPath(), kSeedFilename,
      GetParam().seed_fields_prefs, GetParam().channel,
      entropy_providers_.get(), file_writer_thread_.task_runner());

  // Ensure read failed due to seed file not existing.
  histogram_tester.ExpectUniqueSample(
      base::StrCat(
          {"Variations.SeedFileRead.",
           base::Contains(seed_data_field, "Safe") ? "Safe" : "Latest"}),
      /*sample=*/0, /*expected_bucket_count=*/1);

  // Ensure seed data from local state prefs is loaded and decoded.
  ASSERT_EQ(StoredSeed::StorageFormat::kCompressed,
            seed_reader_writer.GetSeedData().storage_format);
  ASSERT_EQ(compressed_seed, seed_reader_writer.GetSeedData().data);
}

INSTANTIATE_TEST_SUITE_P(
    All,
    SeedReaderWriterSeedFilesGroupTest,
    ::testing::ConvertGenerator<SeedReaderWriterTestParams::TupleT>(
        ::testing::Combine(::testing::Values(kRegularSeedFieldsPrefs,
                                             kSafeSeedFieldsPrefs),
                           ::testing::Values(kSeedFilesGroup),
                           ::testing::Values(version_info::Channel::CANARY,
                                             version_info::Channel::DEV,
                                             version_info::Channel::BETA))));

// Verifies clients using local state to store seeds write seeds to Local State.
TEST_P(SeedReaderWriterLocalStateGroupsTest, WriteSeed) {
  ASSERT_EQ(base::FieldTrialList::FindFullName(kSeedFileTrial),
            GetParam().field_trial_group);
  // Initialize seed_reader_writer with test thread and timer.
  SeedReaderWriter seed_reader_writer(
      &local_state_, /*seed_file_dir=*/temp_dir_.GetPath(), kSeedFilename,
      GetParam().seed_fields_prefs, GetParam().channel,
      entropy_providers_.get(), file_writer_thread_.task_runner());
  seed_reader_writer.SetTimerForTesting(&timer_);

  // Create and store seed.
  const std::string compressed_seed = CreateCompressedVariationsSeed();
  const std::string base64_compressed_seed =
      base::Base64Encode(compressed_seed);
  seed_reader_writer.StoreValidatedSeedInfo(
      ValidatedSeedInfo{.compressed_seed_data = compressed_seed,
                        .base64_seed_data = base64_compressed_seed,
                        .signature = "signature",
                        .milestone = 2});

  // Ensure there's no pending write.
  EXPECT_FALSE(timer_.IsRunning());

  // Verify seed stored correctly, should only be found in Local State prefs.
  EXPECT_FALSE(base::PathExists(temp_seed_file_path_));
  EXPECT_EQ(local_state_.GetString(GetParam().seed_fields_prefs.seed),
            base64_compressed_seed);
}

// Verifies that a seed is cleared from Local State and that seed file is
// deleted if present for clients using local state to store seeds.
TEST_P(SeedReaderWriterLocalStateGroupsTest, ClearSeed) {
  ASSERT_EQ(base::FieldTrialList::FindFullName(kSeedFileTrial),
            GetParam().field_trial_group);
  // Initialize seed_reader_writer with test thread and timer.
  SeedReaderWriter seed_reader_writer(
      &local_state_, /*seed_file_dir=*/temp_dir_.GetPath(), kSeedFilename,
      GetParam().seed_fields_prefs, GetParam().channel,
      entropy_providers_.get(), file_writer_thread_.task_runner());

  // Create and store seed.
  const std::string compressed_seed = CreateCompressedVariationsSeed();
  ASSERT_TRUE(base::WriteFile(temp_seed_file_path_, compressed_seed));
  local_state_.SetString(GetParam().seed_fields_prefs.seed,
                         base::Base64Encode(compressed_seed));

  // Clear seed and force file delete.
  seed_reader_writer.ClearSeedInfo();
  file_writer_thread_.FlushForTesting();

  // Verify seed cleared correctly in Local State prefs and that seed file is
  // deleted.
  EXPECT_THAT(local_state_.GetString(GetParam().seed_fields_prefs.seed),
              IsEmpty());
  EXPECT_FALSE(base::PathExists(temp_seed_file_path_));
}

// Verifies clients using local state to store seeds read seeds from local
// state.
TEST_P(SeedReaderWriterLocalStateGroupsTest, ReadLocalStateBasedSeed) {
  ASSERT_EQ(base::FieldTrialList::FindFullName(kSeedFileTrial),
            GetParam().field_trial_group);
  // Create and store seed.
  ASSERT_TRUE(base::WriteFile(temp_seed_file_path_, "unused seed"));
  const std::string_view seed_data_field = GetParam().seed_fields_prefs.seed;
  local_state_.SetString(seed_data_field,
                         base::Base64Encode(CreateCompressedVariationsSeed()));

  // Initialize seed_reader_writer with test thread.
  base::HistogramTester histogram_tester;
  SeedReaderWriter seed_reader_writer(
      &local_state_, /*seed_file_dir=*/temp_dir_.GetPath(), kSeedFilename,
      GetParam().seed_fields_prefs, GetParam().channel,
      entropy_providers_.get(), file_writer_thread_.task_runner());

  // Ensure seed data loaded from prefs, not seed file.
  ASSERT_EQ(StoredSeed::StorageFormat::kCompressedAndBase64Encoded,
            seed_reader_writer.GetSeedData().storage_format);
  ASSERT_EQ(local_state_.GetString(GetParam().seed_fields_prefs.seed),
            seed_reader_writer.GetSeedData().data);
  histogram_tester.ExpectTotalCount(
      base::StrCat(
          {"Variations.SeedFileRead.",
           base::Contains(seed_data_field, "Safe") ? "Safe" : "Latest"}),
      /*expected_count=*/0);
}

// Verifies that writing seeds with an empty path for `seed_file_dir` does not
// cause a crash.
TEST_P(SeedReaderWriterLocalStateGroupsTest, EmptySeedFilePathIsValid) {
  ASSERT_EQ(base::FieldTrialList::FindFullName(kSeedFileTrial),
            GetParam().field_trial_group);
  // Initialize seed_reader_writer with test thread and timer and an empty file
  // path.
  SeedReaderWriter seed_reader_writer(
      &local_state_,
      /*seed_file_dir=*/base::FilePath(), kSeedFilename,
      GetParam().seed_fields_prefs, GetParam().channel,
      entropy_providers_.get(), file_writer_thread_.task_runner());
  seed_reader_writer.SetTimerForTesting(&timer_);

  // Create and store seed.
  const std::string compressed_seed = CreateCompressedVariationsSeed();
  const std::string base64_compressed_seed =
      base::Base64Encode(compressed_seed);
  seed_reader_writer.StoreValidatedSeedInfo(
      ValidatedSeedInfo{.compressed_seed_data = compressed_seed,
                        .base64_seed_data = base64_compressed_seed,
                        .signature = "signature",
                        .milestone = 2});

  // Ensure there's no pending write.
  EXPECT_FALSE(timer_.IsRunning());

  // Verify seed stored correctly, should only be found in Local State prefs.
  EXPECT_EQ(local_state_.GetString(GetParam().seed_fields_prefs.seed),
            base64_compressed_seed);
}

INSTANTIATE_TEST_SUITE_P(
    NoGroup,
    SeedReaderWriterLocalStateGroupsTest,
    ::testing::ConvertGenerator<SeedReaderWriterTestParams::TupleT>(
        ::testing::Combine(::testing::Values(kRegularSeedFieldsPrefs,
                                             kSafeSeedFieldsPrefs),
                           ::testing::Values(kNoGroup),
                           ::testing::Values(version_info::Channel::UNKNOWN))));

INSTANTIATE_TEST_SUITE_P(
    ControlAndDefaultGroup,
    SeedReaderWriterLocalStateGroupsTest,
    ::testing::ConvertGenerator<SeedReaderWriterTestParams::TupleT>(
        ::testing::Combine(::testing::Values(kRegularSeedFieldsPrefs,
                                             kSafeSeedFieldsPrefs),
                           ::testing::Values(kControlGroup, kDefaultGroup),
                           ::testing::Values(version_info::Channel::UNKNOWN,
                                             version_info::Channel::CANARY,
                                             version_info::Channel::DEV,
                                             version_info::Channel::BETA,
                                             version_info::Channel::STABLE))));
}  // namespace
}  // namespace variations
