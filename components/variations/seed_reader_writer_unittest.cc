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
  using TupleT = std::tuple<std::string_view, std::string_view>;

  SeedReaderWriterTestParams(std::string_view seed_pref,
                             std::string_view field_trial_group)
      : seed_pref(seed_pref), field_trial_group(field_trial_group) {}

  explicit SeedReaderWriterTestParams(const TupleT& t)
      : SeedReaderWriterTestParams(std::get<0>(t), std::get<1>(t)) {}

  std::string_view seed_pref;
  std::string_view field_trial_group;
};

class SeedReaderWriterTest : public TestWithParam<SeedReaderWriterTestParams> {
 public:
  SeedReaderWriterTest() : file_writer_thread_("SeedReaderWriter Test thread") {
    scoped_feature_list_.InitWithEmptyFeatureAndFieldTrialLists();
    file_writer_thread_.Start();
    CHECK(temp_dir_.CreateUniqueTempDir());
    SetUpSeedFileTrial(std::string(GetParam().field_trial_group));
    temp_seed_file_path_ = temp_dir_.GetPath().Append(kSeedFilename);
    VariationsSeedStore::RegisterPrefs(local_state_.registry());
  }
  ~SeedReaderWriterTest() override = default;

 protected:
  base::test::ScopedFeatureList scoped_feature_list_;
  base::test::TaskEnvironment task_environment_;
  base::FilePath temp_seed_file_path_;
  base::Thread file_writer_thread_;
  base::ScopedTempDir temp_dir_;
  TestingPrefServiceSimple local_state_;
  base::MockOneShotTimer timer_;
};

class SeedReaderWriterSeedFilesGroupTest : public SeedReaderWriterTest {};
class SeedReaderWriterControlAndLocalStateOnlyGroupTest
    : public SeedReaderWriterTest {};
class SeedReaderWriterAllGroupsTest : public SeedReaderWriterTest {};

// Verifies clients in SeedFiles group write seeds to Local State and the
// seed file.
TEST_P(SeedReaderWriterSeedFilesGroupTest, WriteSeed) {
  ASSERT_EQ(base::FieldTrialList::FindFullName(kSeedFileTrial),
            GetParam().field_trial_group);
  // Initialize seed_reader_writer with test thread and timer.
  SeedReaderWriter seed_reader_writer(
      &local_state_, /*seed_file_dir=*/temp_dir_.GetPath(), kSeedFilename,
      GetParam().seed_pref, file_writer_thread_.task_runner());
  seed_reader_writer.SetTimerForTesting(&timer_);

  // Create and store seed.
  const std::string compressed_seed = CreateCompressedVariationsSeed();
  const std::string base64_compressed_seed =
      base::Base64Encode(compressed_seed);
  seed_reader_writer.StoreValidatedSeed(compressed_seed,
                                        base64_compressed_seed);

  // Force write.
  timer_.Fire();
  file_writer_thread_.FlushForTesting();

  // Verify that a seed was written to both Local State and a seed file.
  std::string seed_file_data;
  ASSERT_TRUE(base::ReadFileToString(temp_seed_file_path_, &seed_file_data));
  EXPECT_EQ(seed_file_data, compressed_seed);
  EXPECT_EQ(local_state_.GetString(GetParam().seed_pref),
            base64_compressed_seed);
}

// Verifies that a seed is cleared from both Local State and the seed file for
// clients in the SeedFiles group.
TEST_P(SeedReaderWriterSeedFilesGroupTest, ClearSeed) {
  ASSERT_EQ(base::FieldTrialList::FindFullName(kSeedFileTrial),
            GetParam().field_trial_group);
  // Initialize seed_reader_writer with test thread and timer.
  SeedReaderWriter seed_reader_writer(
      &local_state_, /*seed_file_dir=*/temp_dir_.GetPath(), kSeedFilename,
      GetParam().seed_pref, file_writer_thread_.task_runner());
  seed_reader_writer.SetTimerForTesting(&timer_);

  // Create and store seed.
  const std::string compressed_seed = CreateCompressedVariationsSeed();
  ASSERT_TRUE(base::WriteFile(temp_seed_file_path_, compressed_seed));
  local_state_.SetString(GetParam().seed_pref,
                         base::Base64Encode(compressed_seed));

  // Clear seed and force write.
  seed_reader_writer.ClearSeed();
  timer_.Fire();
  file_writer_thread_.FlushForTesting();

  // Verify seed cleared correctly in both Local State prefs and a seed file.
  std::string seed_file_data;
  ASSERT_TRUE(base::ReadFileToString(temp_seed_file_path_, &seed_file_data));
  EXPECT_THAT(seed_file_data, IsEmpty());
  EXPECT_THAT(local_state_.GetString(GetParam().seed_pref), IsEmpty());
}

// Verifies clients in SeedFiles group read seeds from the seed file.
TEST_P(SeedReaderWriterSeedFilesGroupTest, ReadSeedFileBasedSeed) {
  ASSERT_EQ(base::FieldTrialList::FindFullName(kSeedFileTrial),
            GetParam().field_trial_group);
  // Create and store seed.
  const std::string compressed_seed = CreateCompressedVariationsSeed();
  ASSERT_TRUE(base::WriteFile(temp_seed_file_path_, compressed_seed));
  local_state_.SetString(GetParam().seed_pref, "unused seed");

  // Initialize seed_reader_writer with test thread.
  base::HistogramTester histogram_tester;
  SeedReaderWriter seed_reader_writer(
      &local_state_, /*seed_file_dir=*/temp_dir_.GetPath(), kSeedFilename,
      GetParam().seed_pref, file_writer_thread_.task_runner());

  // Ensure seed data loaded from seed file.
  ASSERT_EQ(compressed_seed, seed_reader_writer.GetSeedData());
  histogram_tester.ExpectUniqueSample(
      base::StrCat(
          {"Variations.SeedFileRead.",
           base::Contains(GetParam().seed_pref, "Safe") ? "Safe" : "Latest"}),
      /*sample=*/1, /*expected_bucket_count=*/1);
}

// Verifies clients in SeedFiles group do not crash if reading empty seed file.
TEST_P(SeedReaderWriterSeedFilesGroupTest, ReadEmptySeedFile) {
  ASSERT_EQ(base::FieldTrialList::FindFullName(kSeedFileTrial),
            GetParam().field_trial_group);
  // Create and store seed.
  const std::string compressed_seed = CreateCompressedVariationsSeed();
  ASSERT_TRUE(base::WriteFile(temp_seed_file_path_, ""));
  local_state_.SetString(GetParam().seed_pref, "unused seed");

  // Initialize seed_reader_writer with test thread.
  base::HistogramTester histogram_tester;
  SeedReaderWriter seed_reader_writer(
      &local_state_, /*seed_file_dir=*/temp_dir_.GetPath(), kSeedFilename,
      GetParam().seed_pref, file_writer_thread_.task_runner());

  // Ensure read failed due to seed file not existing.
  histogram_tester.ExpectUniqueSample(
      base::StrCat(
          {"Variations.SeedFileRead.",
           base::Contains(GetParam().seed_pref, "Safe") ? "Safe" : "Latest"}),
      /*sample=*/1, /*expected_bucket_count=*/1);

  // Ensure seed data from local state prefs is loaded and decoded.
  ASSERT_EQ("", seed_reader_writer.GetSeedData());
}

// Verifies clients in SeedFiles group read seeds from local state prefs if no
// seed file found.
TEST_P(SeedReaderWriterSeedFilesGroupTest, ReadMissingSeedFile) {
  ASSERT_EQ(base::FieldTrialList::FindFullName(kSeedFileTrial),
            GetParam().field_trial_group);
  // Create and store seed.
  const std::string compressed_seed = CreateCompressedVariationsSeed();
  local_state_.SetString(GetParam().seed_pref,
                         base::Base64Encode(compressed_seed));

  // Initialize seed_reader_writer with test thread.
  base::HistogramTester histogram_tester;
  SeedReaderWriter seed_reader_writer(
      &local_state_, /*seed_file_dir=*/temp_dir_.GetPath(), kSeedFilename,
      GetParam().seed_pref, file_writer_thread_.task_runner());

  // Ensure read failed due to seed file not existing.
  histogram_tester.ExpectUniqueSample(
      base::StrCat(
          {"Variations.SeedFileRead.",
           base::Contains(GetParam().seed_pref, "Safe") ? "Safe" : "Latest"}),
      /*sample=*/0, /*expected_bucket_count=*/1);

  // Ensure seed data from local state prefs is loaded and decoded.
  ASSERT_EQ(compressed_seed, seed_reader_writer.GetSeedData());
}

INSTANTIATE_TEST_SUITE_P(
    All,
    SeedReaderWriterSeedFilesGroupTest,
    ::testing::ConvertGenerator<SeedReaderWriterTestParams::TupleT>(
        ::testing::Combine(
            ::testing::Values(prefs::kVariationsCompressedSeed,
                              prefs::kVariationsSafeCompressedSeed),
            ::testing::Values(kSeedFilesGroup))));

// Verifies clients in the control group and those using local state only write
// seeds only to Local State.
TEST_P(SeedReaderWriterControlAndLocalStateOnlyGroupTest, WriteSeed) {
  ASSERT_EQ(base::FieldTrialList::FindFullName(kSeedFileTrial),
            GetParam().field_trial_group);
  // Initialize seed_reader_writer with test thread and timer.
  SeedReaderWriter seed_reader_writer(
      &local_state_, /*seed_file_dir=*/temp_dir_.GetPath(), kSeedFilename,
      GetParam().seed_pref, file_writer_thread_.task_runner());
  seed_reader_writer.SetTimerForTesting(&timer_);

  // Create and store seed.
  const std::string compressed_seed = CreateCompressedVariationsSeed();
  const std::string base64_compressed_seed =
      base::Base64Encode(compressed_seed);
  seed_reader_writer.StoreValidatedSeed(compressed_seed,
                                        base64_compressed_seed);

  // Ensure there's no pending write.
  EXPECT_FALSE(timer_.IsRunning());

  // Verify seed stored correctly, should only be found in Local State prefs.
  EXPECT_FALSE(base::PathExists(temp_seed_file_path_));
  EXPECT_EQ(local_state_.GetString(GetParam().seed_pref),
            base64_compressed_seed);
}

// Verifies that a seed is cleared from Local State and that seed file is
// deleted if present for clients in the control group and those using local
// state only.
TEST_P(SeedReaderWriterControlAndLocalStateOnlyGroupTest, ClearSeed) {
  ASSERT_EQ(base::FieldTrialList::FindFullName(kSeedFileTrial),
            GetParam().field_trial_group);
  // Initialize seed_reader_writer with test thread and timer.
  SeedReaderWriter seed_reader_writer(
      &local_state_, /*seed_file_dir=*/temp_dir_.GetPath(), kSeedFilename,
      GetParam().seed_pref, file_writer_thread_.task_runner());

  // Create and store seed.
  const std::string compressed_seed = CreateCompressedVariationsSeed();
  ASSERT_TRUE(base::WriteFile(temp_seed_file_path_, compressed_seed));
  local_state_.SetString(GetParam().seed_pref,
                         base::Base64Encode(compressed_seed));

  // Clear seed and force file delete.
  seed_reader_writer.ClearSeed();
  file_writer_thread_.FlushForTesting();

  // Verify seed cleared correctly in Local State prefs and that seed file is
  // deleted.
  EXPECT_THAT(local_state_.GetString(GetParam().seed_pref), IsEmpty());
  EXPECT_FALSE(base::PathExists(temp_seed_file_path_));
}

// Verifies clients in the control group and those using local state only read
// seeds from Local State.
TEST_P(SeedReaderWriterControlAndLocalStateOnlyGroupTest,
       ReadLocalStateBasedSeed) {
  ASSERT_EQ(base::FieldTrialList::FindFullName(kSeedFileTrial),
            GetParam().field_trial_group);
  // Create and store seed.
  ASSERT_TRUE(base::WriteFile(temp_seed_file_path_, "unused seed"));
  local_state_.SetString(GetParam().seed_pref,
                         base::Base64Encode(CreateCompressedVariationsSeed()));

  // Initialize seed_reader_writer with test thread.
  base::HistogramTester histogram_tester;
  SeedReaderWriter seed_reader_writer(
      &local_state_, /*seed_file_dir=*/temp_dir_.GetPath(), kSeedFilename,
      GetParam().seed_pref, file_writer_thread_.task_runner());

  // Ensure seed data loaded from prefs, not seed file.
  ASSERT_EQ(local_state_.GetString(GetParam().seed_pref),
            seed_reader_writer.GetSeedData());
  histogram_tester.ExpectTotalCount(
      base::StrCat(
          {"Variations.SeedFileRead.",
           base::Contains(GetParam().seed_pref, "Safe") ? "Safe" : "Latest"}),
      /*expected_count=*/0);
}

INSTANTIATE_TEST_SUITE_P(
    All,
    SeedReaderWriterControlAndLocalStateOnlyGroupTest,
    ::testing::ConvertGenerator<SeedReaderWriterTestParams::TupleT>(
        ::testing::Combine(
            ::testing::Values(prefs::kVariationsCompressedSeed,
                              prefs::kVariationsSafeCompressedSeed),
            ::testing::Values(kControlGroup, kDefaultGroup, kNoGroup))));

// Verifies that writing seeds with an empty path for `seed_file_dir` does not
// cause a crash.
TEST_P(SeedReaderWriterAllGroupsTest, EmptySeedFilePathIsValid) {
  ASSERT_EQ(base::FieldTrialList::FindFullName(kSeedFileTrial),
            GetParam().field_trial_group);
  // Initialize seed_reader_writer with test thread and timer and an empty file
  // path.
  SeedReaderWriter seed_reader_writer(&local_state_,
                                      /*seed_file_dir=*/base::FilePath(),
                                      kSeedFilename, GetParam().seed_pref,
                                      file_writer_thread_.task_runner());
  seed_reader_writer.SetTimerForTesting(&timer_);

  // Create and store seed.
  const std::string compressed_seed = CreateCompressedVariationsSeed();
  const std::string base64_compressed_seed =
      base::Base64Encode(compressed_seed);
  seed_reader_writer.StoreValidatedSeed(compressed_seed,
                                        base64_compressed_seed);

  // Ensure there's no pending write.
  EXPECT_FALSE(timer_.IsRunning());

  // Verify seed stored correctly, should only be found in Local State prefs.
  EXPECT_EQ(local_state_.GetString(GetParam().seed_pref),
            base64_compressed_seed);
}

INSTANTIATE_TEST_SUITE_P(
    All,
    SeedReaderWriterAllGroupsTest,
    ::testing::ConvertGenerator<SeedReaderWriterTestParams::TupleT>(
        ::testing::Combine(
            ::testing::Values(prefs::kVariationsCompressedSeed,
                              prefs::kVariationsSafeCompressedSeed),
            ::testing::Values(kSeedFilesGroup,
                              kControlGroup,
                              kDefaultGroup,
                              kNoGroup))));
}  // namespace
}  // namespace variations
