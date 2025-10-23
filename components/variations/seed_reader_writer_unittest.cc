// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/variations/seed_reader_writer.h"

#include "base/base64.h"
#include "base/files/file_util.h"
#include "base/files/scoped_temp_dir.h"
#include "base/test/bind.h"
#include "base/test/metrics/histogram_tester.h"
#include "base/test/mock_entropy_provider.h"
#include "base/test/scoped_feature_list.h"
#include "base/test/task_environment.h"
#include "base/threading/thread.h"
#include "base/timer/mock_timer.h"
#include "base/version_info/channel.h"
#include "components/prefs/testing_pref_service.h"
#include "components/variations/metrics.h"
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
const base::FilePath::CharType kOldSeedFilename[] =
    FILE_PATH_LITERAL("OldTestSeed");

// Used for clients that do not participate in SeedFiles experiment.
constexpr char kNoGroup[] = "";

std::string CreateTooLargeData() {
  return std::string(SeedReaderWriter::MaxUncompressedSeedSizeForTesting() + 1,
                     'A');
}

// Creates a string of size equal to the maximum uncompressed seed size. The
// data is not valid, it should only be used for testing size limits.
std::string CreateValidSizeData() {
  return std::string(SeedReaderWriter::MaxUncompressedSeedSizeForTesting(),
                     'A');
}

// Compresses `data` using Gzip compression.
std::string Gzip(const std::string& data) {
  std::string compressed;
  CHECK(compression::GzipCompress(data, &compressed));
  return compressed;
}

// Creates and serializes a test VariationsSeed.
std::string CreateVariationsSeed() {
  VariationsSeed seed;
  seed.add_study()->set_name("TestStudy");
  std::string serialized_seed;
  seed.SerializeToString(&serialized_seed);
  return serialized_seed;
}

// Creates, serializes, and then Gzip compresses a test seed.
std::string CreateCompressedVariationsSeed() {
  return Gzip(CreateVariationsSeed());
}

// Creates a test StoredSeedInfo to be stored in a seed file.
StoredSeedInfo CreateStoredSeedInfo() {
  StoredSeedInfo stored_seed_info;
  stored_seed_info.set_data(CreateVariationsSeed());
  stored_seed_info.set_signature("signature");
  stored_seed_info.set_milestone(92);
  stored_seed_info.set_session_country_code("us");
  stored_seed_info.set_seed_date(1234);
  return stored_seed_info;
}

// Serializes and compresses a test StoredSeedInfo.
std::string Compress(const StoredSeedInfo& stored_seed_info) {
  return SeedReaderWriter::CompressForSeedFileForTesting(
      stored_seed_info.SerializeAsString());
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
    temp_old_seed_file_path_ = temp_dir_.GetPath().Append(kOldSeedFilename);
  }
  ~SeedReaderWriterTestBase() = default;

 protected:
  base::test::ScopedFeatureList scoped_feature_list_;
  base::test::TaskEnvironment task_environment_;
  base::FilePath temp_seed_file_path_;
  base::FilePath temp_old_seed_file_path_;
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
class ExpectedFieldTrialGroupCanaryDevTest
    : public ExpectedFieldTrialGroupChannelsTest {};
class ExpectedFieldTrialGroupBetaStableUnknownTest
    : public ExpectedFieldTrialGroupChannelsTest {};

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
      kOldSeedFilename, GetParam().seed_fields_prefs, GetParam().channel,
      entropy_providers_.get(), /*histogram_suffix=*/"",
      file_writer_thread_.task_runner());
  EXPECT_THAT(base::FieldTrialList::FindFullName(kSeedFileTrial), IsEmpty());
}

// If no entropy provider given, client is not assigned a group.
TEST_P(ExpectedFieldTrialGroupAllChannelsTest, NoEntropyProvider) {
  SeedReaderWriter seed_reader_writer(
      &local_state_, /*seed_file_dir=*/temp_dir_.GetPath(), kSeedFilename,
      kOldSeedFilename, GetParam().seed_fields_prefs, GetParam().channel,
      /*entropy_providers=*/nullptr, /*histogram_suffix=*/"",
      file_writer_thread_.task_runner());
  EXPECT_THAT(base::FieldTrialList::FindFullName(kSeedFileTrial), IsEmpty());
}

INSTANTIATE_TEST_SUITE_P(
    All,
    ExpectedFieldTrialGroupCanaryDevTest,
    ::testing::ConvertGenerator<ExpectedFieldTrialGroupTestParams::TupleT>(
        ::testing::Combine(::testing::Values(kRegularSeedFieldsPrefs,
                                             kSafeSeedFieldsPrefs),
                           ::testing::Values(version_info::Channel::CANARY,
                                             version_info::Channel::DEV))));

// If channel is canary or dev, client is assigned a group.
TEST_P(ExpectedFieldTrialGroupCanaryDevTest, AssignedGroup) {
  SeedReaderWriter seed_reader_writer(
      &local_state_, /*seed_file_dir=*/temp_dir_.GetPath(), kSeedFilename,
      kOldSeedFilename, GetParam().seed_fields_prefs, GetParam().channel,
      entropy_providers_.get(), /*histogram_suffix=*/"",
      file_writer_thread_.task_runner());
  EXPECT_THAT(base::FieldTrialList::FindFullName(kSeedFileTrial),
              ::testing::AnyOf(kControlGroup, kSeedFilesGroup));
}

INSTANTIATE_TEST_SUITE_P(
    All,
    ExpectedFieldTrialGroupBetaStableUnknownTest,
    ::testing::ConvertGenerator<ExpectedFieldTrialGroupTestParams::TupleT>(
        ::testing::Combine(::testing::Values(kRegularSeedFieldsPrefs,
                                             kSafeSeedFieldsPrefs),
                           ::testing::Values(version_info::Channel::BETA,
                                             version_info::Channel::STABLE,
                                             version_info::Channel::UNKNOWN))));

// If channel is beta, stable, or unknown, client is not assigned a group.
TEST_P(ExpectedFieldTrialGroupBetaStableUnknownTest, NotAssignedGroup) {
  SeedReaderWriter seed_reader_writer(
      &local_state_, /*seed_file_dir=*/temp_dir_.GetPath(), kSeedFilename,
      kOldSeedFilename, GetParam().seed_fields_prefs, GetParam().channel,
      entropy_providers_.get(), /*histogram_suffix=*/"",
      file_writer_thread_.task_runner());
  EXPECT_THAT(base::FieldTrialList::FindFullName(kSeedFileTrial), IsEmpty());
}

class SeedReaderWriterGroupTest
    : public SeedReaderWriterTestBase,
      public TestWithParam<SeedReaderWriterTestParams> {
 public:
  SeedReaderWriterGroupTest() {
    SetUpSeedFileTrial(std::string(GetParam().field_trial_group));
    std::string_view seed_data_field = GetParam().seed_fields_prefs.seed;
    histogram_suffix_ =
        base::Contains(seed_data_field, "Safe") ? "Safe" : "Latest";
  }

  std::string_view GetHistogramSuffix() const { return histogram_suffix_; }

 private:
  std::string histogram_suffix_;
};

class SeedReaderWriterSeedFilesGroupTest : public SeedReaderWriterGroupTest {
 public:
  StoredSeedInfo ReadStoredSeedInfo() {
    std::string seed_file_data;
    CHECK(base::ReadFileToString(temp_seed_file_path_, &seed_file_data));
    std::string uncompressed_contents;
    CHECK(SeedReaderWriter::UncompressFromSeedFileForTesting(
        seed_file_data, &uncompressed_contents));
    StoredSeedInfo stored_seed_info;
    CHECK(stored_seed_info.ParseFromString(uncompressed_contents));
    return stored_seed_info;
  }
};
class SeedReaderWriterLocalStateGroupsTest : public SeedReaderWriterGroupTest {
};

// Verifies clients in SeedFiles group write seeds to a seed file.
TEST_P(SeedReaderWriterSeedFilesGroupTest, WriteSeed) {
  ASSERT_EQ(base::FieldTrialList::FindFullName(kSeedFileTrial),
            GetParam().field_trial_group);

  EXPECT_FALSE(base::PathExists(temp_seed_file_path_));

  // Initialize seed_reader_writer with test thread and timer.
  SeedReaderWriter seed_reader_writer(
      &local_state_, /*seed_file_dir=*/temp_dir_.GetPath(), kSeedFilename,
      kOldSeedFilename, GetParam().seed_fields_prefs, GetParam().channel,
      entropy_providers_.get(), /*histogram_suffix=*/"",
      file_writer_thread_.task_runner());
  seed_reader_writer.SetTimerForTesting(&timer_);

  // Create and store seed.
  const std::string seed_data = CreateVariationsSeed();
  const base::Time seed_date = base::Time::Now();
  const base::Time fetch_time = base::Time::Now();
  seed_reader_writer.StoreValidatedSeedInfo(ValidatedSeedInfo{
      .seed_data = seed_data,
      .signature = "signature",
      .milestone = 2,
      .seed_date = seed_date,
      .client_fetch_time = fetch_time,
      .session_country_code = "us",
  });

  // Force write.
  timer_.Fire();
  file_writer_thread_.FlushForTesting();

  // Verify that a seed was written to a seed file.
  std::string seed_file_data;
  ASSERT_TRUE(base::ReadFileToString(temp_seed_file_path_, &seed_file_data));
  EXPECT_THAT(seed_file_data, Not(IsEmpty()));

  // Verify that the seed data is loaded correctly.
  std::string stored_seed_data;
  std::string stored_signature;
  LoadSeedResult read_seed_result = seed_reader_writer.ReadSeedDataOnStartup(
      &stored_seed_data, &stored_signature);
  EXPECT_EQ(read_seed_result, LoadSeedResult::kSuccess);
  EXPECT_EQ(stored_seed_data, seed_data);
  EXPECT_EQ(stored_signature, "signature");
  EXPECT_EQ(seed_reader_writer.GetSeedInfo().signature, "signature");
  EXPECT_EQ(seed_reader_writer.GetSeedInfo().milestone, 2);
  EXPECT_EQ(seed_reader_writer.GetSeedInfo().seed_date, seed_date);
  EXPECT_EQ(seed_reader_writer.GetSeedInfo().client_fetch_time, fetch_time);
}

// Verifies that a seed is cleared from a seed file for clients in the SeedFiles
// group.
TEST_P(SeedReaderWriterSeedFilesGroupTest, ClearSeed) {
  ASSERT_EQ(base::FieldTrialList::FindFullName(kSeedFileTrial),
            GetParam().field_trial_group);
  // Create and store seed info in the Seed File.
  const StoredSeedInfo stored_seed_info = CreateStoredSeedInfo();
  ASSERT_TRUE(
      base::WriteFile(temp_seed_file_path_, Compress(stored_seed_info)));

  // Initialize seed_reader_writer with test thread and timer.
  SeedReaderWriter seed_reader_writer(
      &local_state_, /*seed_file_dir=*/temp_dir_.GetPath(), kSeedFilename,
      kOldSeedFilename, GetParam().seed_fields_prefs, GetParam().channel,
      entropy_providers_.get(), /*histogram_suffix=*/"",
      file_writer_thread_.task_runner());
  seed_reader_writer.SetTimerForTesting(&timer_);

  // Verify seed was loaded correctly.
  std::string stored_seed_data;
  std::string stored_signature;
  LoadSeedResult read_seed_result = seed_reader_writer.ReadSeedDataOnStartup(
      &stored_seed_data, &stored_signature);
  ASSERT_EQ(read_seed_result, LoadSeedResult::kSuccess);
  ASSERT_THAT(stored_seed_data, Not(IsEmpty()));
  ASSERT_THAT(stored_signature, Not(IsEmpty()));
  auto seed_info = seed_reader_writer.GetSeedInfo();
  ASSERT_THAT(seed_info.signature, Not(IsEmpty()));
  ASSERT_NE(seed_info.milestone, 0);
  ASSERT_FALSE(seed_info.seed_date.is_null());
  ASSERT_THAT(seed_info.session_country_code, Not(IsEmpty()));

  // Clear seed and force write.
  seed_reader_writer.ClearSeedInfo();
  timer_.Fire();
  file_writer_thread_.FlushForTesting();

  // Returned seed data should be empty.
  LoadSeedResult result_after_clear = seed_reader_writer.ReadSeedDataOnStartup(
      &stored_seed_data, &stored_signature);
  EXPECT_EQ(result_after_clear, LoadSeedResult::kEmpty);
  EXPECT_THAT(seed_reader_writer.GetSeedInfo().signature, IsEmpty());
  EXPECT_EQ(seed_reader_writer.GetSeedInfo().milestone, 0);
  EXPECT_TRUE(seed_reader_writer.GetSeedInfo().seed_date.is_null());
  // Session country code is not cleared.
  EXPECT_THAT(seed_reader_writer.GetSeedInfo().session_country_code,
              Not(IsEmpty()));

  // Verify that the seed info in the seed file is cleared.
  StoredSeedInfo cleared_seed_info = ReadStoredSeedInfo();
  EXPECT_THAT(cleared_seed_info.data(), IsEmpty());
  EXPECT_THAT(cleared_seed_info.signature(), IsEmpty());
  EXPECT_EQ(cleared_seed_info.milestone(), 0u);
  EXPECT_EQ(cleared_seed_info.seed_date(), 0);
  EXPECT_EQ(cleared_seed_info.client_fetch_time(), 0);
  // Session country code is not cleared.
  EXPECT_THAT(cleared_seed_info.session_country_code(), Not(IsEmpty()));
}

// Verifies that session country code is cleared from a seed file for clients in
// the SeedFiles group.
TEST_P(SeedReaderWriterSeedFilesGroupTest, ClearSessionCountryCode) {
  ASSERT_EQ(base::FieldTrialList::FindFullName(kSeedFileTrial),
            GetParam().field_trial_group);
  local_state_.SetString(GetParam().seed_fields_prefs.session_country_code,
                         "us");
  // Initialize seed_reader_writer with test thread and timer.
  SeedReaderWriter seed_reader_writer(
      &local_state_, /*seed_file_dir=*/temp_dir_.GetPath(), kSeedFilename,
      kOldSeedFilename, GetParam().seed_fields_prefs, GetParam().channel,
      entropy_providers_.get(), /*histogram_suffix=*/"",
      file_writer_thread_.task_runner());

  ASSERT_THAT(seed_reader_writer.GetSeedInfo().session_country_code,
              Not(IsEmpty()));

  seed_reader_writer.ClearSessionCountry();

  // Session country code is cleared.
  EXPECT_THAT(seed_reader_writer.GetSeedInfo().session_country_code, IsEmpty());
  // Local state pref should be cleared.
  EXPECT_THAT(
      local_state_.GetString(GetParam().seed_fields_prefs.session_country_code),
      IsEmpty());
}

// Verifies clients in SeedFiles group read seeds from the seed file.
TEST_P(SeedReaderWriterSeedFilesGroupTest, ReadSeedFileBasedSeed) {
  ASSERT_EQ(base::FieldTrialList::FindFullName(kSeedFileTrial),
            GetParam().field_trial_group);
  // Create and store seed.
  const StoredSeedInfo stored_seed_info = CreateStoredSeedInfo();
  ASSERT_TRUE(
      base::WriteFile(temp_seed_file_path_, Compress(stored_seed_info)));

  std::string_view histogram_suffix = GetHistogramSuffix();

  // Initialize seed_reader_writer with test thread.
  base::HistogramTester histogram_tester;
  SeedReaderWriter seed_reader_writer(
      &local_state_, /*seed_file_dir=*/temp_dir_.GetPath(), kSeedFilename,
      kOldSeedFilename, GetParam().seed_fields_prefs, GetParam().channel,
      entropy_providers_.get(), histogram_suffix,
      file_writer_thread_.task_runner());

  // Ensure seed data loaded from seed file.
  std::string stored_seed_data;
  std::string stored_signature;
  LoadSeedResult read_seed_result = seed_reader_writer.ReadSeedDataOnStartup(
      &stored_seed_data, &stored_signature);
  ASSERT_EQ(read_seed_result, LoadSeedResult::kSuccess);
  EXPECT_EQ(stored_seed_data, stored_seed_info.data());
  EXPECT_EQ(stored_signature, stored_seed_info.signature());
  histogram_tester.ExpectUniqueSample(
      base::StrCat({"Variations.SeedFileRead.", histogram_suffix}),
      /*sample=*/1, /*expected_bucket_count=*/1);
}

// Verifies clients in SeedFiles group do not crash if reading empty seed file.
TEST_P(SeedReaderWriterSeedFilesGroupTest, ReadEmptySeedFile) {
  ASSERT_EQ(base::FieldTrialList::FindFullName(kSeedFileTrial),
            GetParam().field_trial_group);
  // Create and store seed info.
  ASSERT_TRUE(
      base::WriteFile(temp_seed_file_path_, Compress(StoredSeedInfo())));
  const std::string_view seed_data_field = GetParam().seed_fields_prefs.seed;
  local_state_.SetString(seed_data_field, "unused seed");

  std::string_view histogram_suffix = GetHistogramSuffix();

  // Initialize seed_reader_writer with test thread.
  base::HistogramTester histogram_tester;
  SeedReaderWriter seed_reader_writer(
      &local_state_, /*seed_file_dir=*/temp_dir_.GetPath(), kSeedFilename,
      kOldSeedFilename, GetParam().seed_fields_prefs, GetParam().channel,
      entropy_providers_.get(), histogram_suffix,
      file_writer_thread_.task_runner());

  histogram_tester.ExpectUniqueSample(
      base::StrCat({"Variations.SeedFileRead.", histogram_suffix}),
      /*sample=*/1, /*expected_bucket_count=*/1);

  // Ensure seed data loaded from seed file.
  std::string stored_seed_data;
  LoadSeedResult read_seed_result =
      seed_reader_writer.ReadSeedDataOnStartup(&stored_seed_data, nullptr);
  ASSERT_EQ(read_seed_result, LoadSeedResult::kEmpty);
}

// Verifies clients in SeedFiles group read seeds from the old seed file if the
// seed file is not found.
TEST_P(SeedReaderWriterSeedFilesGroupTest, FallbackToOldSeedFile) {
  ASSERT_EQ(base::FieldTrialList::FindFullName(kSeedFileTrial),
            GetParam().field_trial_group);
  // Create and store seed.
  const std::string compressed_seed = CreateCompressedVariationsSeed();
  base::WriteFile(temp_old_seed_file_path_, compressed_seed);

  ASSERT_TRUE(base::PathExists(temp_old_seed_file_path_));
  ASSERT_FALSE(base::PathExists(temp_seed_file_path_));
  std::string_view histogram_suffix = GetHistogramSuffix();

  // Initialize seed_reader_writer with test thread.
  base::HistogramTester histogram_tester;
  SeedReaderWriter seed_reader_writer(
      &local_state_, /*seed_file_dir=*/temp_dir_.GetPath(), kSeedFilename,
      kOldSeedFilename, GetParam().seed_fields_prefs, GetParam().channel,
      entropy_providers_.get(), histogram_suffix,
      file_writer_thread_.task_runner());

  // Ensure read failed due to seed file not existing.
  histogram_tester.ExpectUniqueSample(
      base::StrCat({"Variations.SeedFileRead.", histogram_suffix}),
      /*sample=*/0, /*expected_bucket_count=*/1);

  // Ensure seed data from local state prefs is loaded and decoded.
  std::string stored_seed_data;
  LoadSeedResult read_result =
      seed_reader_writer.ReadSeedDataOnStartup(&stored_seed_data, nullptr);
  ASSERT_EQ(read_result, LoadSeedResult::kSuccess);
  EXPECT_EQ(stored_seed_data, CreateVariationsSeed());
}

// Verifies clients in SeedFiles group read seeds from local state prefs if no
// seed file found.
TEST_P(SeedReaderWriterSeedFilesGroupTest, FallbackToLocalState) {
  ASSERT_EQ(base::FieldTrialList::FindFullName(kSeedFileTrial),
            GetParam().field_trial_group);
  // Create and store seed.
  const std::string compressed_seed = CreateCompressedVariationsSeed();
  const std::string_view seed_data_field = GetParam().seed_fields_prefs.seed;
  local_state_.SetString(seed_data_field, base::Base64Encode(compressed_seed));

  ASSERT_FALSE(base::PathExists(temp_seed_file_path_));
  ASSERT_FALSE(base::PathExists(temp_old_seed_file_path_));

  std::string_view histogram_suffix = GetHistogramSuffix();

  // Initialize seed_reader_writer with test thread.
  base::HistogramTester histogram_tester;
  SeedReaderWriter seed_reader_writer(
      &local_state_, /*seed_file_dir=*/temp_dir_.GetPath(), kSeedFilename,
      kOldSeedFilename, GetParam().seed_fields_prefs, GetParam().channel,
      entropy_providers_.get(), histogram_suffix,
      file_writer_thread_.task_runner());

  // Ensure read failed due to seed file not existing.
  histogram_tester.ExpectUniqueSample(
      base::StrCat({"Variations.SeedFileRead.", histogram_suffix}),
      /*sample=*/0, /*expected_bucket_count=*/1);

  // Ensure seed data from local state prefs is loaded and decoded.
  std::string stored_seed_data;
  LoadSeedResult read_result =
      seed_reader_writer.ReadSeedDataOnStartup(&stored_seed_data, nullptr);
  ASSERT_EQ(read_result, LoadSeedResult::kSuccess);
  EXPECT_EQ(stored_seed_data, CreateVariationsSeed());
}

TEST_P(SeedReaderWriterSeedFilesGroupTest, ReadMissingSeedFileEmptyLocalState) {
  ASSERT_EQ(base::FieldTrialList::FindFullName(kSeedFileTrial),
            GetParam().field_trial_group);
  // Create and store seed.
  const std::string_view seed_data_field = GetParam().seed_fields_prefs.seed;
  local_state_.ClearPref(seed_data_field);

  std::string_view histogram_suffix = GetHistogramSuffix();

  // Initialize seed_reader_writer with test thread.
  base::HistogramTester histogram_tester;
  SeedReaderWriter seed_reader_writer(
      &local_state_, /*seed_file_dir=*/temp_dir_.GetPath(), kSeedFilename,
      kOldSeedFilename, GetParam().seed_fields_prefs, GetParam().channel,
      entropy_providers_.get(), histogram_suffix,
      file_writer_thread_.task_runner());

  // Ensure read failed due to seed file not existing.
  histogram_tester.ExpectUniqueSample(
      base::StrCat({"Variations.SeedFileRead.", histogram_suffix}),
      /*sample=*/0, /*expected_bucket_count=*/1);

  // Ensure seed data from local state prefs is loaded and decoded.
  std::string stored_seed_data;
  LoadSeedResult read_result =
      seed_reader_writer.ReadSeedDataOnStartup(&stored_seed_data, nullptr);
  EXPECT_EQ(read_result, LoadSeedResult::kEmpty);
}

TEST_P(SeedReaderWriterSeedFilesGroupTest,
       ReadMissingSeedFileEmptyCorruptGzip) {
  ASSERT_EQ(base::FieldTrialList::FindFullName(kSeedFileTrial),
            GetParam().field_trial_group);
  // Create and store seed.
  std::string compressed_seed = CreateCompressedVariationsSeed();
  const std::string_view seed_data_field = GetParam().seed_fields_prefs.seed;
  compressed_seed[5] ^= 0xFF;
  compressed_seed[10] ^= 0xFF;
  local_state_.SetString(seed_data_field, base::Base64Encode(compressed_seed));

  std::string_view histogram_suffix = GetHistogramSuffix();

  // Initialize seed_reader_writer with test thread.
  base::HistogramTester histogram_tester;
  SeedReaderWriter seed_reader_writer(
      &local_state_, /*seed_file_dir=*/temp_dir_.GetPath(), kSeedFilename,
      kOldSeedFilename, GetParam().seed_fields_prefs, GetParam().channel,
      entropy_providers_.get(), histogram_suffix,
      file_writer_thread_.task_runner());

  // Ensure read failed due to seed file not existing.
  histogram_tester.ExpectUniqueSample(
      base::StrCat({"Variations.SeedFileRead.", histogram_suffix}),
      /*sample=*/0, /*expected_bucket_count=*/1);

  // Ensure seed data from local state prefs is loaded and decoded.
  std::string stored_seed_data;
  LoadSeedResult read_result =
      seed_reader_writer.ReadSeedDataOnStartup(&stored_seed_data, nullptr);
  EXPECT_EQ(read_result, LoadSeedResult::kEmpty);
}

TEST_P(SeedReaderWriterSeedFilesGroupTest,
       ReadMissingSeedFileEmptyInvalidBase64) {
  ASSERT_EQ(base::FieldTrialList::FindFullName(kSeedFileTrial),
            GetParam().field_trial_group);
  // Create and store seed.
  const std::string_view seed_data_field = GetParam().seed_fields_prefs.seed;
  local_state_.SetString(seed_data_field, "invalid base64");

  std::string_view histogram_suffix = GetHistogramSuffix();

  // Initialize seed_reader_writer with test thread.
  base::HistogramTester histogram_tester;
  SeedReaderWriter seed_reader_writer(
      &local_state_, /*seed_file_dir=*/temp_dir_.GetPath(), kSeedFilename,
      kOldSeedFilename, GetParam().seed_fields_prefs, GetParam().channel,
      entropy_providers_.get(), histogram_suffix,
      file_writer_thread_.task_runner());

  // Ensure read failed due to seed file not existing.
  histogram_tester.ExpectUniqueSample(
      base::StrCat({"Variations.SeedFileRead.", histogram_suffix}),
      /*sample=*/0, /*expected_bucket_count=*/1);

  // Ensure seed data from local state prefs is loaded and decoded.
  std::string stored_seed_data;
  LoadSeedResult read_result =
      seed_reader_writer.ReadSeedDataOnStartup(&stored_seed_data, nullptr);
  EXPECT_EQ(read_result, LoadSeedResult::kEmpty);
}

TEST_P(SeedReaderWriterSeedFilesGroupTest, ReadSeedData) {
  ASSERT_EQ(base::FieldTrialList::FindFullName(kSeedFileTrial),
            GetParam().field_trial_group);

  SeedReaderWriter seed_reader_writer(
      &local_state_, /*seed_file_dir=*/temp_dir_.GetPath(), kSeedFilename,
      kOldSeedFilename, GetParam().seed_fields_prefs, GetParam().channel,
      entropy_providers_.get(), /*histogram_suffix=*/"",
      file_writer_thread_.task_runner());
  seed_reader_writer.SetTimerForTesting(&timer_);

  // Create and store seed.
  const std::string seed_data = CreateVariationsSeed();
  const std::string signature = "completely valid signature";
  seed_reader_writer.StoreValidatedSeedInfo(ValidatedSeedInfo{
      .seed_data = seed_data,
      .signature = signature,
  });

  std::string read_seed_data;
  std::string base64_seed_signature;
  LoadSeedResult result = seed_reader_writer.ReadSeedDataOnStartup(
      &read_seed_data, &base64_seed_signature);
  EXPECT_EQ(result, LoadSeedResult::kSuccess);
  EXPECT_EQ(read_seed_data, seed_data);
  EXPECT_EQ(base64_seed_signature, signature);
}

TEST_P(SeedReaderWriterSeedFilesGroupTest, ReadSeedDataCorruptGzip) {
  ASSERT_EQ(base::FieldTrialList::FindFullName(kSeedFileTrial),
            GetParam().field_trial_group);

  // Create and store a corrupt seed. When load on startup, it should fail and
  // keep the seed data in memory empty.
  std::string compressed_seed_info = Compress(CreateStoredSeedInfo());
  compressed_seed_info[5] ^= 0xFF;
  compressed_seed_info[10] ^= 0xFF;
  ASSERT_TRUE(base::WriteFile(temp_seed_file_path_, compressed_seed_info));

  SeedReaderWriter seed_reader_writer(
      &local_state_, /*seed_file_dir=*/temp_dir_.GetPath(), kSeedFilename,
      kOldSeedFilename, GetParam().seed_fields_prefs, GetParam().channel,
      entropy_providers_.get(), /*histogram_suffix=*/"",
      file_writer_thread_.task_runner());
  seed_reader_writer.SetTimerForTesting(&timer_);

  std::string seed_data;
  std::string base64_seed_signature;
  LoadSeedResult result = seed_reader_writer.ReadSeedDataOnStartup(
      &seed_data, &base64_seed_signature);
  EXPECT_EQ(result, LoadSeedResult::kEmpty);
}

TEST_P(SeedReaderWriterSeedFilesGroupTest, ReadSeedDataExceedsSizeLimit) {
  ASSERT_EQ(base::FieldTrialList::FindFullName(kSeedFileTrial),
            GetParam().field_trial_group);

  // Create and store a "too large" seed. When load on startup, it should fail
  // and keep the seed data in memory empty.
  std::string compressed_seed_info =
      SeedReaderWriter::CompressForSeedFileForTesting(CreateTooLargeData());
  ASSERT_TRUE(base::WriteFile(temp_seed_file_path_, compressed_seed_info));

  std::string_view histogram_suffix = GetHistogramSuffix();

  base::HistogramTester histogram_tester;
  SeedReaderWriter seed_reader_writer(
      &local_state_, /*seed_file_dir=*/temp_dir_.GetPath(), kSeedFilename,
      kOldSeedFilename, GetParam().seed_fields_prefs, GetParam().channel,
      entropy_providers_.get(), histogram_suffix,
      file_writer_thread_.task_runner());
  seed_reader_writer.SetTimerForTesting(&timer_);

  histogram_tester.ExpectUniqueSample(
      base::StrCat({"Variations.SeedFileReadResult.", histogram_suffix}),
      /*sample=*/LoadSeedResult::kExceedsUncompressedSizeLimit,
      /*expected_bucket_count=*/1);

  std::string seed_data;
  std::string base64_seed_signature;
  LoadSeedResult result = seed_reader_writer.ReadSeedDataOnStartup(
      &seed_data, &base64_seed_signature);
  EXPECT_EQ(result, LoadSeedResult::kEmpty);
}

TEST_P(SeedReaderWriterSeedFilesGroupTest, ReadSeedData_LimitSizeSeed) {
  ASSERT_EQ(base::FieldTrialList::FindFullName(kSeedFileTrial),
            GetParam().field_trial_group);

  // Create and store a "correct size seed". When load on startup, it shouldn't
  // fail because of incorrect size.
  std::string compressed_seed_info =
      SeedReaderWriter::CompressForSeedFileForTesting(CreateValidSizeData());
  ASSERT_TRUE(base::WriteFile(temp_seed_file_path_, compressed_seed_info));

  std::string_view histogram_suffix = GetHistogramSuffix();

  base::HistogramTester histogram_tester;
  SeedReaderWriter seed_reader_writer(
      &local_state_, /*seed_file_dir=*/temp_dir_.GetPath(), kSeedFilename,
      kOldSeedFilename, GetParam().seed_fields_prefs, GetParam().channel,
      entropy_providers_.get(), histogram_suffix,
      file_writer_thread_.task_runner());
  seed_reader_writer.SetTimerForTesting(&timer_);

  histogram_tester.ExpectBucketCount(
      base::StrCat({"Variations.SeedFileReadResult.", histogram_suffix}),
      /*sample=*/LoadSeedResult::kExceedsUncompressedSizeLimit,
      /*expected_count=*/0);

  std::string seed_data;
  std::string base64_seed_signature;
  LoadSeedResult result = seed_reader_writer.ReadSeedDataOnStartup(
      &seed_data, &base64_seed_signature);
  // Because the data is invalid, the seed data should be empty.
  EXPECT_EQ(result, LoadSeedResult::kEmpty);
}

TEST_P(SeedReaderWriterSeedFilesGroupTest, ReadSeedDataCallback) {
  ASSERT_EQ(base::FieldTrialList::FindFullName(kSeedFileTrial),
            GetParam().field_trial_group);

  SeedReaderWriter seed_reader_writer(
      &local_state_, /*seed_file_dir=*/temp_dir_.GetPath(), kSeedFilename,
      kOldSeedFilename, GetParam().seed_fields_prefs, GetParam().channel,
      entropy_providers_.get(), /*histogram_suffix=*/"",
      file_writer_thread_.task_runner());
  seed_reader_writer.SetTimerForTesting(&timer_);

  // Create and store seed.
  const std::string seed_data = CreateVariationsSeed();
  const std::string signature = "completely valid signature";
  seed_reader_writer.StoreValidatedSeedInfo(ValidatedSeedInfo{
      .seed_data = seed_data,
      .signature = signature,
  });

  // Read seed data and verify result.
  base::RunLoop run_loop;
  SeedReaderWriter::ReadSeedDataResult result;
  auto lambda_cb = base::BindLambdaForTesting(
      [&result, &run_loop](SeedReaderWriter::ReadSeedDataResult read_result) {
        result = std::move(read_result);
        run_loop.Quit();
      });
  seed_reader_writer.ReadSeedData(lambda_cb);
  run_loop.Run();

  EXPECT_EQ(result.result, LoadSeedResult::kSuccess);
  EXPECT_EQ(result.seed_data, seed_data);
  EXPECT_EQ(result.signature, signature);
}

TEST_P(SeedReaderWriterSeedFilesGroupTest, ReadSeedDataCallbackCorruptGzip) {
  ASSERT_EQ(base::FieldTrialList::FindFullName(kSeedFileTrial),
            GetParam().field_trial_group);

  // Create and store a seed. The seed loaded on startup should be valid for the
  // test to be meaningful, otherwise an empty seed will be stored in memory and
  // never be purged.
  std::string compressed_seed_info = Compress(CreateStoredSeedInfo());
  ASSERT_TRUE(base::WriteFile(temp_seed_file_path_, compressed_seed_info));

  // Initialize seed_reader_writer with test thread and timer.
  SeedReaderWriter seed_reader_writer(
      &local_state_,
      /*seed_file_dir=*/temp_dir_.GetPath(), kSeedFilename, kOldSeedFilename,
      GetParam().seed_fields_prefs, GetParam().channel,
      entropy_providers_.get(), /*histogram_suffix=*/"",
      file_writer_thread_.task_runner());
  seed_reader_writer.SetTimerForTesting(&timer_);

  // Allow the seed data to be purged from memory to simulate the case where the
  // seed data is read from the file.
  file_writer_thread_.FlushForTesting();
  seed_reader_writer.AllowToPurgeSeedDataFromMemory();
  base::RunLoop().RunUntilIdle();
  ASSERT_FALSE(seed_reader_writer.stored_seed_data_for_testing().has_value());

  // Simulate a corrupt gzip file by toggling bits in the compressed seed info.
  // This can happen when storing a new seed to the file after a fetch.
  compressed_seed_info[5] ^= 0xFF;
  compressed_seed_info[10] ^= 0xFF;
  ASSERT_TRUE(base::WriteFile(temp_seed_file_path_, compressed_seed_info));

  // Read seed data and verify result.
  base::RunLoop run_loop;
  LoadSeedResult load_result;
  auto lambda_cb = base::BindLambdaForTesting(
      [&load_result,
       &run_loop](SeedReaderWriter::ReadSeedDataResult read_result) {
        load_result = read_result.result;
        run_loop.Quit();
      });
  seed_reader_writer.ReadSeedData(lambda_cb);
  run_loop.Run();
#if BUILDFLAG(IS_ANDROID)
  EXPECT_EQ(load_result, LoadSeedResult::kCorruptGzip);
#else   // !BUILDFLAG(IS_ANDROID)
  EXPECT_EQ(load_result, LoadSeedResult::kCorruptZstd);
#endif  // BUILDFLAG(IS_ANDROID)
}

TEST_P(SeedReaderWriterSeedFilesGroupTest,
       ReadSeedDataCallbackExceedsSizeLimit) {
  ASSERT_EQ(base::FieldTrialList::FindFullName(kSeedFileTrial),
            GetParam().field_trial_group);

  // Create and store a seed. The seed loaded on startup should be valid for the
  // test to be meaningful, otherwise an empty seed will be stored in memory and
  // never be purged.
  std::string compressed_seed_info = Compress(CreateStoredSeedInfo());
  ASSERT_TRUE(base::WriteFile(temp_seed_file_path_, compressed_seed_info));

  SeedReaderWriter seed_reader_writer(
      &local_state_, /*seed_file_dir=*/temp_dir_.GetPath(), kSeedFilename,
      kOldSeedFilename, GetParam().seed_fields_prefs, GetParam().channel,
      entropy_providers_.get(), /*histogram_suffix=*/"",
      file_writer_thread_.task_runner());
  seed_reader_writer.SetTimerForTesting(&timer_);

  // Simulate a "too large" seed by writing a very large seed to the file.
  std::string corrupt_compressed_seed_info =
      SeedReaderWriter::CompressForSeedFileForTesting(CreateTooLargeData());
  ASSERT_TRUE(
      base::WriteFile(temp_seed_file_path_, corrupt_compressed_seed_info));

  // Allow the seed data to be purged from memory to simulate the case where the
  // seed data is read from the file.
  file_writer_thread_.FlushForTesting();
  base::RunLoop().RunUntilIdle();
  seed_reader_writer.AllowToPurgeSeedDataFromMemory();
  ASSERT_FALSE(seed_reader_writer.stored_seed_data_for_testing().has_value());

  // Read seed data and verify result.
  base::RunLoop run_loop;
  LoadSeedResult load_result;
  auto lambda_cb = base::BindLambdaForTesting(
      [&load_result,
       &run_loop](SeedReaderWriter::ReadSeedDataResult read_result) {
        load_result = read_result.result;
        run_loop.Quit();
      });
  seed_reader_writer.ReadSeedData(lambda_cb);
  run_loop.Run();
  EXPECT_EQ(load_result, LoadSeedResult::kExceedsUncompressedSizeLimit);
}

TEST_P(SeedReaderWriterSeedFilesGroupTest, ReadSeedDataCallback_LimitSizeSeed) {
  ASSERT_EQ(base::FieldTrialList::FindFullName(kSeedFileTrial),
            GetParam().field_trial_group);

  // Create and store a seed. The seed loaded on startup should be valid for the
  // test to be meaningful, otherwise an empty seed will be stored in memory and
  // never be purged.
  std::string compressed_seed_info = Compress(CreateStoredSeedInfo());
  ASSERT_TRUE(base::WriteFile(temp_seed_file_path_, compressed_seed_info));

  SeedReaderWriter seed_reader_writer(
      &local_state_, /*seed_file_dir=*/temp_dir_.GetPath(), kSeedFilename,
      kOldSeedFilename, GetParam().seed_fields_prefs, GetParam().channel,
      entropy_providers_.get(), /*histogram_suffix=*/"",
      file_writer_thread_.task_runner());
  seed_reader_writer.SetTimerForTesting(&timer_);

  // Simulate a "too large" seed by writing a very large seed to the file.
  std::string corrupt_compressed_seed_info =
      SeedReaderWriter::CompressForSeedFileForTesting(CreateValidSizeData());
  ASSERT_TRUE(
      base::WriteFile(temp_seed_file_path_, corrupt_compressed_seed_info));

  // Allow the seed data to be purged from memory to simulate the case where the
  // seed data is read from the file.
  file_writer_thread_.FlushForTesting();
  base::RunLoop().RunUntilIdle();
  seed_reader_writer.AllowToPurgeSeedDataFromMemory();
  ASSERT_FALSE(seed_reader_writer.stored_seed_data_for_testing().has_value());

  // Read seed data and verify result.
  base::RunLoop run_loop;
  LoadSeedResult load_result;
  auto lambda_cb = base::BindLambdaForTesting(
      [&load_result,
       &run_loop](SeedReaderWriter::ReadSeedDataResult read_result) {
        load_result = read_result.result;
        run_loop.Quit();
      });
  seed_reader_writer.ReadSeedData(lambda_cb);
  run_loop.Run();
  // Because the seed is invalid, the load result should not be success.
  EXPECT_NE(load_result, LoadSeedResult::kSuccess);
}

INSTANTIATE_TEST_SUITE_P(
    All,
    SeedReaderWriterSeedFilesGroupTest,
    ::testing::ConvertGenerator<SeedReaderWriterTestParams::TupleT>(
        ::testing::Combine(::testing::Values(kRegularSeedFieldsPrefs,
                                             kSafeSeedFieldsPrefs),
                           ::testing::Values(kSeedFilesGroup),
                           ::testing::Values(version_info::Channel::CANARY,
                                             version_info::Channel::DEV))));

// Verifies clients using local state to store seeds write seeds to Local State.
TEST_P(SeedReaderWriterLocalStateGroupsTest, WriteSeed) {
  ASSERT_EQ(base::FieldTrialList::FindFullName(kSeedFileTrial),
            GetParam().field_trial_group);
  // Initialize seed_reader_writer with test thread and timer.
  SeedReaderWriter seed_reader_writer(
      &local_state_, /*seed_file_dir=*/temp_dir_.GetPath(), kSeedFilename,
      kOldSeedFilename, GetParam().seed_fields_prefs, GetParam().channel,
      entropy_providers_.get(), /*histogram_suffix=*/"",
      file_writer_thread_.task_runner());
  seed_reader_writer.SetTimerForTesting(&timer_);

  // Create and store seed.
  const std::string seed_data = CreateVariationsSeed();
  const base::Time seed_date = base::Time::Now();
  const base::Time fetch_time = base::Time::Now();
  seed_reader_writer.StoreValidatedSeedInfo(ValidatedSeedInfo{
      .seed_data = seed_data,
      .signature = "signature",
      .milestone = 2,
      .seed_date = seed_date,
      .client_fetch_time = fetch_time,
      .session_country_code = "us",
  });

  // Ensure there's no pending write.
  EXPECT_FALSE(timer_.IsRunning());

  // Verify seed stored correctly, should only be found in Local State prefs.
  EXPECT_FALSE(base::PathExists(temp_seed_file_path_));
  const std::string compressed_seed = Gzip(seed_data);
  const std::string base64_compressed_seed =
      base::Base64Encode(compressed_seed);
  EXPECT_EQ(local_state_.GetString(GetParam().seed_fields_prefs.seed),
            base64_compressed_seed);
  EXPECT_EQ(local_state_.GetString(GetParam().seed_fields_prefs.signature),
            "signature");
  EXPECT_EQ(local_state_.GetInteger(GetParam().seed_fields_prefs.milestone), 2);
  EXPECT_EQ(local_state_.GetTime(GetParam().seed_fields_prefs.seed_date),
            seed_date);
  EXPECT_EQ(
      local_state_.GetTime(GetParam().seed_fields_prefs.client_fetch_time),
      fetch_time);
}

// Verifies that a seed is cleared from Local State and that seed file is
// deleted if present for clients using local state to store seeds.
TEST_P(SeedReaderWriterLocalStateGroupsTest, ClearSeed) {
  ASSERT_EQ(base::FieldTrialList::FindFullName(kSeedFileTrial),
            GetParam().field_trial_group);
  // Initialize seed_reader_writer with test thread and timer.
  SeedReaderWriter seed_reader_writer(
      &local_state_, /*seed_file_dir=*/temp_dir_.GetPath(), kSeedFilename,
      kOldSeedFilename, GetParam().seed_fields_prefs, GetParam().channel,
      entropy_providers_.get(), /*histogram_suffix=*/"",
      file_writer_thread_.task_runner());

  // Create and store seed.
  const std::string compressed_seed = CreateCompressedVariationsSeed();
  ASSERT_TRUE(base::WriteFile(temp_seed_file_path_, compressed_seed));
  local_state_.SetString(GetParam().seed_fields_prefs.seed,
                         base::Base64Encode(compressed_seed));
  local_state_.SetString(GetParam().seed_fields_prefs.signature, "signature");
  local_state_.SetInteger(GetParam().seed_fields_prefs.milestone, 92);
  local_state_.SetTime(GetParam().seed_fields_prefs.seed_date,
                       base::Time::Now());
  local_state_.SetString(GetParam().seed_fields_prefs.session_country_code,
                         "us");

  // Clear seed and force file delete.
  seed_reader_writer.ClearSeedInfo();
  file_writer_thread_.FlushForTesting();

  // Returned seed data should be empty.
  std::string stored_seed_data;
  LoadSeedResult read_result =
      seed_reader_writer.ReadSeedDataOnStartup(&stored_seed_data, nullptr);
  EXPECT_EQ(read_result, LoadSeedResult::kEmpty);
  EXPECT_THAT(seed_reader_writer.GetSeedInfo().signature, IsEmpty());
  EXPECT_EQ(seed_reader_writer.GetSeedInfo().milestone, 0);
  EXPECT_TRUE(seed_reader_writer.GetSeedInfo().seed_date.is_null());
  // Session country code is not cleared.
  EXPECT_THAT(seed_reader_writer.GetSeedInfo().session_country_code,
              Not(IsEmpty()));

  // Verify seed cleared correctly in Local State prefs and that seed file is
  // deleted.
  EXPECT_THAT(local_state_.GetString(GetParam().seed_fields_prefs.seed),
              IsEmpty());
  EXPECT_THAT(local_state_.GetString(GetParam().seed_fields_prefs.signature),
              IsEmpty());
  EXPECT_EQ(local_state_.GetInteger(GetParam().seed_fields_prefs.milestone), 0);
  EXPECT_EQ(local_state_.GetTime(GetParam().seed_fields_prefs.seed_date),
            base::Time());
  EXPECT_EQ(
      local_state_.GetTime(GetParam().seed_fields_prefs.client_fetch_time),
      base::Time());
  // Session country code is not cleared.
  EXPECT_THAT(
      local_state_.GetString(GetParam().seed_fields_prefs.session_country_code),
      Not(IsEmpty()));
  EXPECT_FALSE(base::PathExists(temp_seed_file_path_));
}

// Verifies that session country code is cleared from Local State for clients
// using local state to store seeds.
TEST_P(SeedReaderWriterLocalStateGroupsTest, ClearSessionCountryCode) {
  ASSERT_EQ(base::FieldTrialList::FindFullName(kSeedFileTrial),
            GetParam().field_trial_group);
  // Initialize seed_reader_writer with test thread and timer.
  SeedReaderWriter seed_reader_writer(
      &local_state_, /*seed_file_dir=*/temp_dir_.GetPath(), kSeedFilename,
      kOldSeedFilename, GetParam().seed_fields_prefs, GetParam().channel,
      entropy_providers_.get(), /*histogram_suffix=*/"",
      file_writer_thread_.task_runner());

  // Create and store seed.
  local_state_.SetString(GetParam().seed_fields_prefs.session_country_code,
                         "us");

  // Clear seed and force file delete.
  seed_reader_writer.ClearSessionCountry();
  file_writer_thread_.FlushForTesting();

  EXPECT_THAT(seed_reader_writer.GetSeedInfo().session_country_code, IsEmpty());
  EXPECT_THAT(
      local_state_.GetString(GetParam().seed_fields_prefs.session_country_code),
      IsEmpty());
}

// Verifies clients using local state to store seeds read seeds from local
// state.
TEST_P(SeedReaderWriterLocalStateGroupsTest, ReadLocalStateBasedSeed) {
  ASSERT_EQ(base::FieldTrialList::FindFullName(kSeedFileTrial),
            GetParam().field_trial_group);
  // Create and store seed.
  const std::string_view seed_data_field = GetParam().seed_fields_prefs.seed;
  local_state_.SetString(seed_data_field,
                         base::Base64Encode(CreateCompressedVariationsSeed()));

  // Initialize seed_reader_writer with test thread.
  base::HistogramTester histogram_tester;
  SeedReaderWriter seed_reader_writer(
      &local_state_, /*seed_file_dir=*/temp_dir_.GetPath(), kSeedFilename,
      kOldSeedFilename, GetParam().seed_fields_prefs, GetParam().channel,
      entropy_providers_.get(), /*histogram_suffix=*/"",
      file_writer_thread_.task_runner());

  // Ensure seed data loaded from prefs, not seed file.
  std::string stored_seed_data;
  LoadSeedResult read_result =
      seed_reader_writer.ReadSeedDataOnStartup(&stored_seed_data, nullptr);
  EXPECT_EQ(read_result, LoadSeedResult::kSuccess);
  EXPECT_EQ(stored_seed_data, CreateVariationsSeed());
  histogram_tester.ExpectTotalCount(
      base::StrCat({"Variations.SeedFileRead.", GetHistogramSuffix()}),
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
      /*seed_file_dir=*/base::FilePath(), kSeedFilename, kOldSeedFilename,
      GetParam().seed_fields_prefs, GetParam().channel,
      entropy_providers_.get(), /*histogram_suffix=*/"",
      file_writer_thread_.task_runner());
  seed_reader_writer.SetTimerForTesting(&timer_);

  // Create and store seed.
  const std::string seed_data = CreateVariationsSeed();
  const base::Time seed_date = base::Time::Now();
  const base::Time fetch_time = base::Time::Now();
  seed_reader_writer.StoreValidatedSeedInfo(ValidatedSeedInfo{
      .seed_data = seed_data,
      .signature = "signature",
      .milestone = 2,
      .seed_date = seed_date,
      .client_fetch_time = fetch_time,
  });

  // Ensure there's no pending write.
  EXPECT_FALSE(timer_.IsRunning());

  // Verify seed stored correctly, should only be found in Local State prefs.
  const std::string base64_compressed_seed =
      base::Base64Encode(Gzip(seed_data));
  EXPECT_EQ(local_state_.GetString(GetParam().seed_fields_prefs.seed),
            base64_compressed_seed);
  EXPECT_EQ(local_state_.GetString(GetParam().seed_fields_prefs.signature),
            "signature");
  EXPECT_EQ(local_state_.GetInteger(GetParam().seed_fields_prefs.milestone), 2);
  EXPECT_EQ(local_state_.GetTime(GetParam().seed_fields_prefs.seed_date),
            seed_date);
  EXPECT_EQ(
      local_state_.GetTime(GetParam().seed_fields_prefs.client_fetch_time),
      fetch_time);
}

TEST_P(SeedReaderWriterLocalStateGroupsTest, ReadSeedData) {
  ASSERT_EQ(base::FieldTrialList::FindFullName(kSeedFileTrial),
            GetParam().field_trial_group);
  // Initialize seed_reader_writer with test thread and timer and an empty file
  // path.
  SeedReaderWriter seed_reader_writer(
      &local_state_,
      /*seed_file_dir=*/base::FilePath(), kSeedFilename, kOldSeedFilename,
      GetParam().seed_fields_prefs, GetParam().channel,
      entropy_providers_.get(), /*histogram_suffix=*/"",
      file_writer_thread_.task_runner());
  seed_reader_writer.SetTimerForTesting(&timer_);

  // Create and store seed.
  const std::string seed_data = CreateVariationsSeed();
  seed_reader_writer.StoreValidatedSeedInfo(ValidatedSeedInfo{
      .seed_data = seed_data,
      .signature = "seed signature",
  });

  std::string read_seed_data;
  std::string base64_seed_signature;
  LoadSeedResult result = seed_reader_writer.ReadSeedDataOnStartup(
      &read_seed_data, &base64_seed_signature);
  EXPECT_EQ(result, LoadSeedResult::kSuccess);
  EXPECT_EQ(read_seed_data, seed_data);
  EXPECT_EQ(base64_seed_signature, "seed signature");
}

TEST_P(SeedReaderWriterLocalStateGroupsTest, ReadSeedDataCorruptBase64) {
  ASSERT_EQ(base::FieldTrialList::FindFullName(kSeedFileTrial),
            GetParam().field_trial_group);
  // Initialize seed_reader_writer with test thread and timer and an empty file
  // path.
  SeedReaderWriter seed_reader_writer(
      &local_state_,
      /*seed_file_dir=*/base::FilePath(), kSeedFilename, kOldSeedFilename,
      GetParam().seed_fields_prefs, GetParam().channel,
      entropy_providers_.get(), /*histogram_suffix=*/"",
      file_writer_thread_.task_runner());
  seed_reader_writer.SetTimerForTesting(&timer_);

  seed_reader_writer.StoreRawSeedForTesting("invalid base64");

  std::string seed_data;
  std::string base64_seed_signature;
  LoadSeedResult result = seed_reader_writer.ReadSeedDataOnStartup(
      &seed_data, &base64_seed_signature);
  EXPECT_EQ(result, LoadSeedResult::kCorruptBase64);
}

TEST_P(SeedReaderWriterLocalStateGroupsTest, ReadSeedDataCorruptGzip) {
  ASSERT_EQ(base::FieldTrialList::FindFullName(kSeedFileTrial),
            GetParam().field_trial_group);
  // Initialize seed_reader_writer with test thread and timer and an empty file
  // path.
  SeedReaderWriter seed_reader_writer(
      &local_state_,
      /*seed_file_dir=*/base::FilePath(), kSeedFilename, kOldSeedFilename,
      GetParam().seed_fields_prefs, GetParam().channel,
      entropy_providers_.get(), /*histogram_suffix=*/"",
      file_writer_thread_.task_runner());
  seed_reader_writer.SetTimerForTesting(&timer_);

  std::string compressed_seed = CreateCompressedVariationsSeed();
  compressed_seed[5] ^= 0xFF;
  compressed_seed[10] ^= 0xFF;
  const std::string base64_compressed_seed =
      base::Base64Encode(compressed_seed);
  seed_reader_writer.StoreRawSeedForTesting(base64_compressed_seed);

  std::string seed_data;
  std::string base64_seed_signature;
  LoadSeedResult result = seed_reader_writer.ReadSeedDataOnStartup(
      &seed_data, &base64_seed_signature);
  EXPECT_EQ(result, LoadSeedResult::kCorruptGzip);
}

TEST_P(SeedReaderWriterLocalStateGroupsTest, ReadSeedDataExceedsSizeLimit) {
  ASSERT_EQ(base::FieldTrialList::FindFullName(kSeedFileTrial),
            GetParam().field_trial_group);
  // Initialize seed_reader_writer with test thread and timer and an empty file
  // path.
  SeedReaderWriter seed_reader_writer(
      &local_state_,
      /*seed_file_dir=*/base::FilePath(), kSeedFilename, kOldSeedFilename,
      GetParam().seed_fields_prefs, GetParam().channel,
      entropy_providers_.get(), /*histogram_suffix=*/"",
      file_writer_thread_.task_runner());
  seed_reader_writer.SetTimerForTesting(&timer_);

  const std::string base64_compressed_seed =
      base::Base64Encode(Gzip(CreateTooLargeData()));
  seed_reader_writer.StoreRawSeedForTesting(base64_compressed_seed);

  std::string seed_data;
  std::string base64_seed_signature;
  LoadSeedResult result = seed_reader_writer.ReadSeedDataOnStartup(
      &seed_data, &base64_seed_signature);
  EXPECT_EQ(result, LoadSeedResult::kExceedsUncompressedSizeLimit);
}

TEST_P(SeedReaderWriterLocalStateGroupsTest, ReadSeedData_LimitSizeSeed) {
  ASSERT_EQ(base::FieldTrialList::FindFullName(kSeedFileTrial),
            GetParam().field_trial_group);
  // Initialize seed_reader_writer with test thread and timer and an empty file
  // path.
  SeedReaderWriter seed_reader_writer(
      &local_state_,
      /*seed_file_dir=*/base::FilePath(), kSeedFilename, kOldSeedFilename,
      GetParam().seed_fields_prefs, GetParam().channel,
      entropy_providers_.get(), /*histogram_suffix=*/"",
      file_writer_thread_.task_runner());
  seed_reader_writer.SetTimerForTesting(&timer_);

  const std::string base64_compressed_seed =
      base::Base64Encode(Gzip(CreateValidSizeData()));
  seed_reader_writer.StoreRawSeedForTesting(base64_compressed_seed);

  std::string seed_data;
  std::string base64_seed_signature;
  LoadSeedResult result = seed_reader_writer.ReadSeedDataOnStartup(
      &seed_data, &base64_seed_signature);
  EXPECT_NE(result, LoadSeedResult::kExceedsUncompressedSizeLimit);
  // Because the seed is not validated at this point, it should not be
  // considered valid.
  EXPECT_EQ(result, LoadSeedResult::kSuccess);
}

TEST_P(SeedReaderWriterLocalStateGroupsTest, ReadSeedDataCallback) {
  ASSERT_EQ(base::FieldTrialList::FindFullName(kSeedFileTrial),
            GetParam().field_trial_group);
  // Initialize seed_reader_writer with test thread and timer and an empty file
  // path.
  SeedReaderWriter seed_reader_writer(
      &local_state_,
      /*seed_file_dir=*/base::FilePath(), kSeedFilename, kOldSeedFilename,
      GetParam().seed_fields_prefs, GetParam().channel,
      entropy_providers_.get(), /*histogram_suffix=*/"",
      file_writer_thread_.task_runner());
  seed_reader_writer.SetTimerForTesting(&timer_);

  // Create and store seed.
  const std::string seed_data = CreateVariationsSeed();
  const std::string signature = "seed signature";
  seed_reader_writer.StoreValidatedSeedInfo(ValidatedSeedInfo{
      .seed_data = seed_data,
      .signature = signature,
  });

  SeedReaderWriter::ReadSeedDataResult read_result;
  auto lambda_cb = base::BindLambdaForTesting(
      [&read_result](SeedReaderWriter::ReadSeedDataResult result) {
        read_result = result;
      });
  seed_reader_writer.ReadSeedData(lambda_cb);

  EXPECT_EQ(read_result.result, LoadSeedResult::kSuccess);
  EXPECT_EQ(read_result.seed_data, seed_data);
  EXPECT_EQ(read_result.signature, signature);
}

TEST_P(SeedReaderWriterLocalStateGroupsTest,
       ReadSeedDataCallbackCorruptBase64) {
  ASSERT_EQ(base::FieldTrialList::FindFullName(kSeedFileTrial),
            GetParam().field_trial_group);
  // Initialize seed_reader_writer with test thread and timer and an empty file
  // path.
  SeedReaderWriter seed_reader_writer(
      &local_state_,
      /*seed_file_dir=*/base::FilePath(), kSeedFilename, kOldSeedFilename,
      GetParam().seed_fields_prefs, GetParam().channel,
      entropy_providers_.get(), /*histogram_suffix=*/"",
      file_writer_thread_.task_runner());
  seed_reader_writer.SetTimerForTesting(&timer_);

  seed_reader_writer.StoreRawSeedForTesting("invalid base64");

  LoadSeedResult load_result;
  auto lambda_cb = base::BindLambdaForTesting(
      [&load_result](SeedReaderWriter::ReadSeedDataResult read_result) {
        load_result = read_result.result;
      });
  seed_reader_writer.ReadSeedData(lambda_cb);
  EXPECT_EQ(load_result, LoadSeedResult::kCorruptBase64);
}

TEST_P(SeedReaderWriterLocalStateGroupsTest, ReadSeedDataCallbackCorruptGzip) {
  ASSERT_EQ(base::FieldTrialList::FindFullName(kSeedFileTrial),
            GetParam().field_trial_group);
  // Initialize seed_reader_writer with test thread and timer and an empty file
  // path.
  SeedReaderWriter seed_reader_writer(
      &local_state_,
      /*seed_file_dir=*/base::FilePath(), kSeedFilename, kOldSeedFilename,
      GetParam().seed_fields_prefs, GetParam().channel,
      entropy_providers_.get(), /*histogram_suffix=*/"",
      file_writer_thread_.task_runner());
  seed_reader_writer.SetTimerForTesting(&timer_);

  std::string compressed_seed = CreateCompressedVariationsSeed();
  compressed_seed[5] ^= 0xFF;
  compressed_seed[10] ^= 0xFF;
  const std::string base64_compressed_seed =
      base::Base64Encode(compressed_seed);
  seed_reader_writer.StoreRawSeedForTesting(base64_compressed_seed);

  LoadSeedResult load_result;
  auto lambda_cb = base::BindLambdaForTesting(
      [&load_result](SeedReaderWriter::ReadSeedDataResult read_result) {
        load_result = read_result.result;
      });
  seed_reader_writer.ReadSeedData(lambda_cb);
  EXPECT_EQ(load_result, LoadSeedResult::kCorruptGzip);
}

TEST_P(SeedReaderWriterLocalStateGroupsTest,
       ReadSeedDataCallbackExceedsSizeLimit) {
  ASSERT_EQ(base::FieldTrialList::FindFullName(kSeedFileTrial),
            GetParam().field_trial_group);
  // Initialize seed_reader_writer with test thread and timer and an empty file
  // path.
  SeedReaderWriter seed_reader_writer(
      &local_state_,
      /*seed_file_dir=*/base::FilePath(), kSeedFilename, kOldSeedFilename,
      GetParam().seed_fields_prefs, GetParam().channel,
      entropy_providers_.get(), /*histogram_suffix=*/"",
      file_writer_thread_.task_runner());
  seed_reader_writer.SetTimerForTesting(&timer_);

  const std::string base64_compressed_seed =
      base::Base64Encode(Gzip(CreateTooLargeData()));
  seed_reader_writer.StoreRawSeedForTesting(base64_compressed_seed);

  base::RunLoop run_loop;
  LoadSeedResult load_result;
  auto lambda_cb = base::BindLambdaForTesting(
      [&load_result,
       &run_loop](SeedReaderWriter::ReadSeedDataResult read_result) {
        load_result = read_result.result;
        run_loop.Quit();
      });
  seed_reader_writer.ReadSeedData(lambda_cb);
  run_loop.Run();
  EXPECT_EQ(load_result, LoadSeedResult::kExceedsUncompressedSizeLimit);
}

TEST_P(SeedReaderWriterLocalStateGroupsTest,
       ReadSeedDataCallback_LimitSizeSeed) {
  ASSERT_EQ(base::FieldTrialList::FindFullName(kSeedFileTrial),
            GetParam().field_trial_group);
  // Initialize seed_reader_writer with test thread and timer and an empty file
  // path.
  SeedReaderWriter seed_reader_writer(
      &local_state_,
      /*seed_file_dir=*/base::FilePath(), kSeedFilename, kOldSeedFilename,
      GetParam().seed_fields_prefs, GetParam().channel,
      entropy_providers_.get(), /*histogram_suffix=*/"",
      file_writer_thread_.task_runner());
  seed_reader_writer.SetTimerForTesting(&timer_);

  const std::string base64_compressed_seed =
      base::Base64Encode(Gzip(CreateValidSizeData()));
  seed_reader_writer.StoreRawSeedForTesting(base64_compressed_seed);

  base::RunLoop run_loop;
  LoadSeedResult load_result;
  auto lambda_cb = base::BindLambdaForTesting(
      [&load_result,
       &run_loop](SeedReaderWriter::ReadSeedDataResult read_result) {
        load_result = read_result.result;
        run_loop.Quit();
      });
  seed_reader_writer.ReadSeedData(lambda_cb);
  run_loop.Run();
  // Because the seed is not validated at this point, it should not be
  // considered valid.
  EXPECT_EQ(load_result, LoadSeedResult::kSuccess);
}

TEST_P(SeedReaderWriterLocalStateGroupsTest, MigrateFromSeedFileToLocalState) {
  ASSERT_EQ(base::FieldTrialList::FindFullName(kSeedFileTrial),
            GetParam().field_trial_group);
  // Write seed to seed file.
  const std::string variations_seed = CreateVariationsSeed();
  const std::string compressed_seed = Gzip(variations_seed);
  ASSERT_FALSE(base::PathExists(temp_seed_file_path_));
  ASSERT_TRUE(base::WriteFile(temp_old_seed_file_path_, compressed_seed));

  // Initialize seed_reader_writer with test thread and timer.
  SeedReaderWriter seed_reader_writer(
      &local_state_, /*seed_file_dir=*/temp_dir_.GetPath(), kSeedFilename,
      kOldSeedFilename, GetParam().seed_fields_prefs, GetParam().channel,
      entropy_providers_.get(), /*histogram_suffix=*/"",
      file_writer_thread_.task_runner());
  file_writer_thread_.FlushForTesting();

  // Verify that the seed was written into local state.
  std::string encoded_seed = base::Base64Encode(compressed_seed);
  EXPECT_EQ(local_state_.GetString(GetParam().seed_fields_prefs.seed),
            encoded_seed);

  // Verify that the old seed file was deleted.
  EXPECT_FALSE(base::PathExists(temp_old_seed_file_path_));
  // Verify that the seed file was not created.
  EXPECT_FALSE(base::PathExists(temp_seed_file_path_));

  // Verify that the seed data is loaded correctly.
  std::string stored_seed_data;
  std::string stored_signature;
  LoadSeedResult read_seed_result = seed_reader_writer.ReadSeedDataOnStartup(
      &stored_seed_data, &stored_signature);
  EXPECT_EQ(read_seed_result, LoadSeedResult::kSuccess);
  EXPECT_EQ(stored_seed_data, variations_seed);
}

TEST_P(SeedReaderWriterLocalStateGroupsTest,
       MigrateFromSeedFileToLocalStateWithSameSeedSentinel) {
  ASSERT_EQ(base::FieldTrialList::FindFullName(kSeedFileTrial),
            GetParam().field_trial_group);
  // Write seed to seed file.
  const std::string compressed_seed = kIdenticalToSafeSeedSentinel;
  ASSERT_TRUE(base::WriteFile(temp_old_seed_file_path_, compressed_seed));

  // Initialize seed_reader_writer with test thread and timer.
  SeedReaderWriter seed_reader_writer(
      &local_state_, /*seed_file_dir=*/temp_dir_.GetPath(), kSeedFilename,
      kOldSeedFilename, GetParam().seed_fields_prefs, GetParam().channel,
      entropy_providers_.get(), /*histogram_suffix=*/"",
      file_writer_thread_.task_runner());
  file_writer_thread_.FlushForTesting();

  // Verify that the seed was written into local state.
  std::string encoded_seed = base::Base64Encode(compressed_seed);
  EXPECT_EQ(local_state_.GetString(GetParam().seed_fields_prefs.seed),
            kIdenticalToSafeSeedSentinel);

  // Verify that the seed file was deleted.
  EXPECT_FALSE(base::PathExists(temp_old_seed_file_path_));

  // Verify that the seed data is loaded correctly.
  std::string stored_seed_data;
  std::string stored_signature;
  LoadSeedResult read_seed_result = seed_reader_writer.ReadSeedDataOnStartup(
      &stored_seed_data, &stored_signature);
  EXPECT_EQ(read_seed_result, LoadSeedResult::kSuccess);
  EXPECT_EQ(stored_seed_data, kIdenticalToSafeSeedSentinel);
}

// If no seed file exists, the seed in local state should not be overwritten.
TEST_P(SeedReaderWriterLocalStateGroupsTest, NoSeedFile) {
  ASSERT_EQ(base::FieldTrialList::FindFullName(kSeedFileTrial),
            GetParam().field_trial_group);
  // No seed file.
  ASSERT_FALSE(base::PathExists(temp_old_seed_file_path_));
  // Seed in local state that shouldn't be overwritten.
  const std::string variations_seed = CreateVariationsSeed();
  const std::string encoded_seed = base::Base64Encode(Gzip(variations_seed));
  local_state_.SetString(GetParam().seed_fields_prefs.seed, encoded_seed);
  ASSERT_EQ(local_state_.GetString(GetParam().seed_fields_prefs.seed),
            encoded_seed);

  // Initialize seed_reader_writer with test thread and timer.
  SeedReaderWriter seed_reader_writer(
      &local_state_, /*seed_file_dir=*/temp_dir_.GetPath(), kSeedFilename,
      kOldSeedFilename, GetParam().seed_fields_prefs, GetParam().channel,
      entropy_providers_.get(), /*histogram_suffix=*/"",
      file_writer_thread_.task_runner());
  file_writer_thread_.FlushForTesting();

  // Verify that the seed was not overwritten.
  EXPECT_EQ(local_state_.GetString(GetParam().seed_fields_prefs.seed),
            encoded_seed);
  // Verify that the seed file was not created.
  EXPECT_FALSE(base::PathExists(temp_old_seed_file_path_));

  // Verify that the seed data is loaded correctly.
  std::string stored_seed_data;
  std::string stored_signature;
  LoadSeedResult read_seed_result = seed_reader_writer.ReadSeedDataOnStartup(
      &stored_seed_data, &stored_signature);
  EXPECT_EQ(read_seed_result, LoadSeedResult::kSuccess);
  EXPECT_EQ(stored_seed_data, variations_seed);
}

INSTANTIATE_TEST_SUITE_P(
    NoGroup,
    SeedReaderWriterLocalStateGroupsTest,
    ::testing::ConvertGenerator<SeedReaderWriterTestParams::TupleT>(
        ::testing::Combine(::testing::Values(kRegularSeedFieldsPrefs,
                                             kSafeSeedFieldsPrefs),
                           ::testing::Values(kNoGroup),
                           ::testing::Values(version_info::Channel::UNKNOWN,
                                             version_info::Channel::STABLE,
                                             version_info::Channel::BETA))));

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

class SeedReaderWriterAllGroupsTest : public SeedReaderWriterGroupTest {};

INSTANTIATE_TEST_SUITE_P(
    AllGroups,
    SeedReaderWriterAllGroupsTest,
    ::testing::ConvertGenerator<SeedReaderWriterTestParams::TupleT>(
        ::testing::Combine(
            ::testing::Values(kRegularSeedFieldsPrefs, kSafeSeedFieldsPrefs),
            ::testing::Values(kControlGroup, kDefaultGroup, kSeedFilesGroup),
            ::testing::Values(version_info::Channel::UNKNOWN,
                              version_info::Channel::CANARY,
                              version_info::Channel::DEV,
                              version_info::Channel::BETA,
                              version_info::Channel::STABLE))));

TEST_P(SeedReaderWriterAllGroupsTest, ReadSeedDataEmptySeedData) {
  ASSERT_EQ(base::FieldTrialList::FindFullName(kSeedFileTrial),
            GetParam().field_trial_group);
  // Initialize seed_reader_writer with test thread and timer.
  SeedReaderWriter seed_reader_writer(
      &local_state_, /*seed_file_dir=*/temp_dir_.GetPath(), kSeedFilename,
      kOldSeedFilename, GetParam().seed_fields_prefs, GetParam().channel,
      entropy_providers_.get(), /*histogram_suffix=*/"",
      file_writer_thread_.task_runner());
  seed_reader_writer.SetTimerForTesting(&timer_);

  // Create and store seed.
  seed_reader_writer.StoreValidatedSeedInfo(ValidatedSeedInfo{
      .seed_data = "",
      .signature = "ignored signature",
  });

  std::string seed_data;
  std::string base64_seed_signature;
  LoadSeedResult result = seed_reader_writer.ReadSeedDataOnStartup(
      &seed_data, &base64_seed_signature);
  EXPECT_EQ(result, LoadSeedResult::kEmpty);
}

TEST_P(SeedReaderWriterAllGroupsTest, ReadSeedDataSentinel) {
  ASSERT_EQ(base::FieldTrialList::FindFullName(kSeedFileTrial),
            GetParam().field_trial_group);
  // Initialize seed_reader_writer with test thread and timer.
  SeedReaderWriter seed_reader_writer(
      &local_state_, /*seed_file_dir=*/temp_dir_.GetPath(), kSeedFilename,
      kOldSeedFilename, GetParam().seed_fields_prefs, GetParam().channel,
      entropy_providers_.get(), /*histogram_suffix=*/"",
      file_writer_thread_.task_runner());
  seed_reader_writer.SetTimerForTesting(&timer_);

  // Create and store seed.
  seed_reader_writer.StoreValidatedSeedInfo(ValidatedSeedInfo{
      .seed_data = kIdenticalToSafeSeedSentinel,
      .signature = "ignored signature",
  });

  std::string seed_data;
  std::string base64_seed_signature;
  LoadSeedResult result = seed_reader_writer.ReadSeedDataOnStartup(
      &seed_data, &base64_seed_signature);
  EXPECT_EQ(result, LoadSeedResult::kSuccess);
  EXPECT_EQ(seed_data, kIdenticalToSafeSeedSentinel);
}

TEST_P(SeedReaderWriterSeedFilesGroupTest,
       ReadSeedDataAfterAllowToPurgeSeedDataFromMemory) {
  ASSERT_EQ(base::FieldTrialList::FindFullName(kSeedFileTrial),
            GetParam().field_trial_group);
  // Initialize seed_reader_writer with test thread and timer.
  SeedReaderWriter seed_reader_writer(
      &local_state_, /*seed_file_dir=*/temp_dir_.GetPath(), kSeedFilename,
      kOldSeedFilename, GetParam().seed_fields_prefs, GetParam().channel,
      entropy_providers_.get(), /*histogram_suffix=*/"",
      file_writer_thread_.task_runner());
  seed_reader_writer.SetTimerForTesting(&timer_);

  std::string seed_data = CreateVariationsSeed();
  std::string signature = "seed signature";
  // Store a seed
  ASSERT_EQ(seed_reader_writer.StoreValidatedSeedInfo(ValidatedSeedInfo{
                .seed_data = seed_data,
                .signature = signature,
            }),
            StoreSeedResult::kSuccess);
  // Fire the timer to write the seed data to disk.
  timer_.Fire();
  file_writer_thread_.FlushForTesting();
  base::RunLoop().RunUntilIdle();
  ASSERT_TRUE(seed_reader_writer.stored_seed_data_for_testing().has_value());

  {
    base::RunLoop run_loop;
    SeedReaderWriter::ReadSeedDataResult read_result;
    auto lambda_cb = base::BindLambdaForTesting(
        [&read_result, &run_loop](SeedReaderWriter::ReadSeedDataResult result) {
          read_result = std::move(result);
          run_loop.Quit();
        });
    seed_reader_writer.ReadSeedData(lambda_cb);
    run_loop.Run();
    ASSERT_EQ(read_result.result, LoadSeedResult::kSuccess);
    ASSERT_EQ(read_result.seed_data, seed_data);
    ASSERT_EQ(read_result.signature, signature);
    ASSERT_EQ(seed_reader_writer.stored_seed_data_for_testing(), seed_data);
  }

  // Remove the seed data from memory.
  seed_reader_writer.AllowToPurgeSeedDataFromMemory();
  ASSERT_FALSE(seed_reader_writer.stored_seed_data_for_testing().has_value());

  {
    // Read the seed data.
    base::RunLoop run_loop;
    SeedReaderWriter::ReadSeedDataResult read_result;
    auto lambda_cb = base::BindLambdaForTesting(
        [&read_result, &run_loop](SeedReaderWriter::ReadSeedDataResult result) {
          read_result = std::move(result);
          run_loop.Quit();
        });
    seed_reader_writer.ReadSeedData(lambda_cb);
    run_loop.Run();
    EXPECT_EQ(read_result.result, LoadSeedResult::kSuccess);
    EXPECT_EQ(read_result.seed_data, seed_data);
    EXPECT_EQ(read_result.signature, signature);
    EXPECT_FALSE(seed_reader_writer.stored_seed_data_for_testing().has_value());
  }
}

TEST_P(SeedReaderWriterSeedFilesGroupTest,
       WriteSeedDataAfterAllowToPurgeSeedDataFromMemory) {
  ASSERT_EQ(base::FieldTrialList::FindFullName(kSeedFileTrial),
            GetParam().field_trial_group);
  // Initialize seed_reader_writer with test thread and timer.
  SeedReaderWriter seed_reader_writer(
      &local_state_, /*seed_file_dir=*/temp_dir_.GetPath(), kSeedFilename,
      kOldSeedFilename, GetParam().seed_fields_prefs, GetParam().channel,
      entropy_providers_.get(), /*histogram_suffix=*/"",
      file_writer_thread_.task_runner());
  seed_reader_writer.SetTimerForTesting(&timer_);

  // Remove the seed data from memory.
  seed_reader_writer.AllowToPurgeSeedDataFromMemory();

  // Store a seed.
  std::string seed_data = CreateVariationsSeed();
  std::string signature = "seed signature";
  auto result = seed_reader_writer.StoreValidatedSeedInfo(ValidatedSeedInfo{
      .seed_data = seed_data,
      .signature = signature,
  });
  EXPECT_EQ(result, StoreSeedResult::kSuccess);
  timer_.Fire();
  file_writer_thread_.FlushForTesting();
  base::RunLoop().RunUntilIdle();
  ASSERT_FALSE(seed_reader_writer.stored_seed_data_for_testing().has_value());

  base::RunLoop run_loop;
  SeedReaderWriter::ReadSeedDataResult read_result;
  auto lambda_cb = base::BindLambdaForTesting(
      [&read_result, &run_loop](SeedReaderWriter::ReadSeedDataResult result) {
        read_result = std::move(result);
        run_loop.Quit();
      });
  seed_reader_writer.ReadSeedData(lambda_cb);
  run_loop.Run();
  ASSERT_EQ(read_result.result, LoadSeedResult::kSuccess);
  ASSERT_EQ(read_result.seed_data, seed_data);
  ASSERT_EQ(read_result.signature, signature);
  EXPECT_FALSE(seed_reader_writer.stored_seed_data_for_testing().has_value());
}

TEST_P(SeedReaderWriterSeedFilesGroupTest,
       EmptySeedIsKeptInMemoryAfterAllowToPurgeSeedDataFromMemory) {
  ASSERT_EQ(base::FieldTrialList::FindFullName(kSeedFileTrial),
            GetParam().field_trial_group);
  // Initialize seed_reader_writer with test thread and timer.
  SeedReaderWriter seed_reader_writer(
      &local_state_, /*seed_file_dir=*/temp_dir_.GetPath(), kSeedFilename,
      kOldSeedFilename, GetParam().seed_fields_prefs, GetParam().channel,
      entropy_providers_.get(), /*histogram_suffix=*/"",
      file_writer_thread_.task_runner());
  seed_reader_writer.SetTimerForTesting(&timer_);

  seed_reader_writer.AllowToPurgeSeedDataFromMemory();
  // Store a seed.
  std::string signature = "seed signature";
  auto result = seed_reader_writer.StoreValidatedSeedInfo(ValidatedSeedInfo{
      .seed_data = "",
      .signature = signature,
  });
  ASSERT_EQ(result, StoreSeedResult::kSuccess);
  timer_.Fire();
  file_writer_thread_.FlushForTesting();
  base::RunLoop().RunUntilIdle();

  EXPECT_TRUE(seed_reader_writer.stored_seed_data_for_testing().has_value());
  EXPECT_EQ(seed_reader_writer.stored_seed_data_for_testing().value(), "");
}

TEST_P(SeedReaderWriterSeedFilesGroupTest,
       SentinelIsKeptInMemoryAfterAllowToPurgeSeedDataFromMemory) {
  ASSERT_EQ(base::FieldTrialList::FindFullName(kSeedFileTrial),
            GetParam().field_trial_group);
  // Initialize seed_reader_writer with test thread and timer.
  SeedReaderWriter seed_reader_writer(
      &local_state_, /*seed_file_dir=*/temp_dir_.GetPath(), kSeedFilename,
      kOldSeedFilename, GetParam().seed_fields_prefs, GetParam().channel,
      entropy_providers_.get(), /*histogram_suffix=*/"",
      file_writer_thread_.task_runner());
  seed_reader_writer.SetTimerForTesting(&timer_);

  seed_reader_writer.AllowToPurgeSeedDataFromMemory();
  // Store a seed.
  std::string signature = "seed signature";
  auto result = seed_reader_writer.StoreValidatedSeedInfo(ValidatedSeedInfo{
      .seed_data = kIdenticalToSafeSeedSentinel,
      .signature = signature,
  });
  ASSERT_EQ(result, StoreSeedResult::kSuccess);
  timer_.Fire();
  file_writer_thread_.FlushForTesting();
  base::RunLoop().RunUntilIdle();

  EXPECT_TRUE(seed_reader_writer.stored_seed_data_for_testing().has_value());
  EXPECT_EQ(seed_reader_writer.stored_seed_data_for_testing().value(),
            kIdenticalToSafeSeedSentinel);
}

}  // namespace
}  // namespace variations
