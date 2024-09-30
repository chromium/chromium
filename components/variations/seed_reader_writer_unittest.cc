// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/variations/seed_reader_writer.h"

#include "base/files/file_util.h"
#include "base/files/scoped_temp_dir.h"
#include "base/test/task_environment.h"
#include "base/threading/thread.h"
#include "base/timer/mock_timer.h"
#include "components/prefs/testing_pref_service.h"
#include "components/variations/pref_names.h"
#include "components/variations/variations_seed_store.h"
#include "components/variations/variations_test_utils.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/zlib/google/compression_utils.h"

namespace variations {
namespace {

using ::testing::TestWithParam;
using ::testing::Values;

const base::FilePath::CharType kSeedFilename[] = FILE_PATH_LITERAL("TestSeed");

VariationsSeed CreateTestSeed() {
  VariationsSeed seed;
  seed.add_study()->set_name("TestStudy");
  return seed;
}

std::string SerializeSeed(const VariationsSeed& seed) {
  std::string serialized_seed;
  seed.SerializeToString(&serialized_seed);
  return serialized_seed;
}

class SeedReaderWriterTest : public TestWithParam<version_info::Channel> {
 public:
  SeedReaderWriterTest() : file_writer_thread_("SeedReaderWriter Test thread") {
    file_writer_thread_.Start();
    if (!temp_dir_.CreateUniqueTempDir()) {
      ADD_FAILURE() << "Failed to create temp directory.";
    }
    VariationsSeedStore::RegisterPrefs(local_state_.registry());
  }
  ~SeedReaderWriterTest() override = default;

 protected:
  base::Thread file_writer_thread_;
  base::ScopedTempDir temp_dir_;
  TestingPrefServiceSimple local_state_;
  base::MockOneShotTimer timer_;
};

class SeedReaderWriterPreStableTest : public SeedReaderWriterTest {};

class SeedReaderWriterStableAndUnknownTest : public SeedReaderWriterTest {};

// Verifies that pre-stable clients write latest seeds to Local State and the
// new seed file.
TEST_P(SeedReaderWriterPreStableTest, WriteSeed) {
  base::FilePath temp_seed_file_path =
      SeedReaderWriter::GetFilePath(temp_dir_.GetPath(), kSeedFilename);

  // Initialize seed_reader_writer with test thread and timer.
  SeedReaderWriter seed_reader_writer(&local_state_, temp_seed_file_path,
                                      GetParam(),
                                      file_writer_thread_.task_runner());
  seed_reader_writer.SetTimerForTesting(&timer_);

  // Create and store seed. Note that the data format written here is contrived.
  const std::string serialized_seed = SerializeSeed(CreateTestSeed());
  seed_reader_writer.StoreValidatedSeed(serialized_seed);

  // Force write.
  timer_.Fire();
  file_writer_thread_.FlushForTesting();

  // Verify seed stored correctly, should be found in both prefs and the seed
  // file.
  std::string seed_file_data;
  EXPECT_TRUE(base::ReadFileToString(temp_seed_file_path, &seed_file_data));
  EXPECT_EQ(seed_file_data, serialized_seed);
  EXPECT_EQ(local_state_.GetString(prefs::kVariationsCompressedSeed),
            serialized_seed);
}

// Verifies that pre-stable clients clear latest seeds in Local State and the
// new seed file.
TEST_P(SeedReaderWriterPreStableTest, ClearSeed) {
  base::FilePath temp_seed_file_path =
      SeedReaderWriter::GetFilePath(temp_dir_.GetPath(), kSeedFilename);

  // Initialize seed_reader_writer with test thread and timer.
  SeedReaderWriter seed_reader_writer(&local_state_, temp_seed_file_path,
                                      GetParam(),
                                      file_writer_thread_.task_runner());
  seed_reader_writer.SetTimerForTesting(&timer_);

  // Create and store seed. Note that the data format written here is contrived.
  const std::string serialized_seed = SerializeSeed(CreateTestSeed());
  seed_reader_writer.StoreValidatedSeed(serialized_seed);

  // Force write.
  timer_.Fire();
  file_writer_thread_.FlushForTesting();

  // Verify seed stored correctly.
  std::string seed_file_data;
  EXPECT_TRUE(base::ReadFileToString(temp_seed_file_path, &seed_file_data));
  EXPECT_EQ(seed_file_data, serialized_seed);
  EXPECT_EQ(local_state_.GetString(prefs::kVariationsCompressedSeed),
            serialized_seed);

  // Clear seed and force write.
  seed_reader_writer.ClearSeed();
  timer_.Fire();
  file_writer_thread_.FlushForTesting();

  // Verify seed cleared correctly in both prefs and the seed file.
  EXPECT_TRUE(base::ReadFileToString(temp_seed_file_path, &seed_file_data));
  EXPECT_EQ(seed_file_data, std::string());
  EXPECT_EQ(local_state_.GetString(prefs::kVariationsCompressedSeed),
            std::string());
}

INSTANTIATE_TEST_SUITE_P(All,
                         SeedReaderWriterPreStableTest,
                         Values(version_info::Channel::CANARY,
                                version_info::Channel::DEV,
                                version_info::Channel::BETA));

// Verifies that stable and unknown channel clients write latest seeds only to
// Local State.
TEST_P(SeedReaderWriterStableAndUnknownTest, WriteSeed) {
  base::FilePath temp_seed_file_path =
      SeedReaderWriter::GetFilePath(temp_dir_.GetPath(), kSeedFilename);

  // Initialize seed_reader_writer with test thread and timer.
  SeedReaderWriter seed_reader_writer(&local_state_, temp_seed_file_path,
                                      GetParam(),
                                      file_writer_thread_.task_runner());
  seed_reader_writer.SetTimerForTesting(&timer_);

  // Create and store seed. Note that the data format written here is contrived.
  const std::string serialized_seed = SerializeSeed(CreateTestSeed());
  seed_reader_writer.StoreValidatedSeed(serialized_seed);

  // Ensure there's no pending write.
  EXPECT_FALSE(seed_reader_writer.HasPendingWriteForTesting());
  ASSERT_FALSE(timer_.IsRunning());

  // Verify seed stored correctly, should only be found in prefs.
  std::string seed_file_data;
  EXPECT_FALSE(base::ReadFileToString(temp_seed_file_path, &seed_file_data));
  EXPECT_EQ(local_state_.GetString(prefs::kVariationsCompressedSeed),
            serialized_seed);
}

// Verifies that stable and unknown channel clients clear latest seeds only in
// Local State.
TEST_P(SeedReaderWriterStableAndUnknownTest, ClearSeed) {
  base::FilePath temp_seed_file_path =
      SeedReaderWriter::GetFilePath(temp_dir_.GetPath(), kSeedFilename);

  // Initialize seed_reader_writer with test thread and timer.
  SeedReaderWriter seed_reader_writer(&local_state_, temp_seed_file_path,
                                      GetParam(),
                                      file_writer_thread_.task_runner());
  seed_reader_writer.SetTimerForTesting(&timer_);

  // Create and store seed. Note that the data format written here is contrived.
  const std::string serialized_seed = SerializeSeed(CreateTestSeed());
  seed_reader_writer.StoreValidatedSeed(serialized_seed);

  // Verify seed stored correctly.
  EXPECT_EQ(local_state_.GetString(prefs::kVariationsCompressedSeed),
            serialized_seed);

  // Clear seed and ensure there's no pending write.
  seed_reader_writer.ClearSeed();
  EXPECT_FALSE(seed_reader_writer.HasPendingWriteForTesting());
  ASSERT_FALSE(timer_.IsRunning());

  // Verify seed cleared correctly in prefs.
  std::string seed_file_data;
  EXPECT_FALSE(base::ReadFileToString(temp_seed_file_path, &seed_file_data));
  EXPECT_EQ(local_state_.GetString(prefs::kVariationsCompressedSeed),
            std::string());
}

INSTANTIATE_TEST_SUITE_P(All,
                         SeedReaderWriterStableAndUnknownTest,
                         Values(version_info::Channel::STABLE,
                                version_info::Channel::UNKNOWN));

// Verifies that writing latest seeds with an empty path for `seed_file_path`
// does not cause a crash.
TEST_P(SeedReaderWriterTest, EmptySeedFilePathIsValid) {
  base::FilePath temp_seed_file_path =
      SeedReaderWriter::GetFilePath(temp_dir_.GetPath(), kSeedFilename);

  // Initialize seed_reader_writer with test thread and timer and an empty file
  // path.
  SeedReaderWriter seed_reader_writer(&local_state_,
                                      /*seed_file_path=*/base::FilePath(),
                                      GetParam(),
                                      file_writer_thread_.task_runner());
  seed_reader_writer.SetTimerForTesting(&timer_);

  // Create and store seed. Note that the data format written here is contrived.
  const std::string serialized_seed = SerializeSeed(CreateTestSeed());
  seed_reader_writer.StoreValidatedSeed(serialized_seed);

  // Ensure there's no pending write.
  EXPECT_FALSE(seed_reader_writer.HasPendingWriteForTesting());
  ASSERT_FALSE(timer_.IsRunning());

  // Verify seed stored correctly, should only be found in prefs.
  EXPECT_EQ(local_state_.GetString(prefs::kVariationsCompressedSeed),
            serialized_seed);
}
INSTANTIATE_TEST_SUITE_P(All,
                         SeedReaderWriterTest,
                         Values(version_info::Channel::CANARY,
                                version_info::Channel::DEV,
                                version_info::Channel::BETA,
                                version_info::Channel::STABLE,
                                version_info::Channel::UNKNOWN));
}  // namespace
}  // namespace variations
