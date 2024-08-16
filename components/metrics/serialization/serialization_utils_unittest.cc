// Copyright 2014 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/metrics/serialization/serialization_utils.h"

#include <stddef.h>
#include <stdint.h>

#include "base/check.h"
#include "base/containers/span.h"
#include "base/files/file.h"
#include "base/files/file_util.h"
#include "base/files/scoped_temp_dir.h"
#include "base/strings/stringprintf.h"
#include "base/test/metrics/histogram_tester.h"
#include "components/metrics/serialization/metric_sample.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace metrics {
namespace {

using ::testing::IsEmpty;

class SerializationUtilsTest : public testing::Test {
 protected:
  SerializationUtilsTest() {
    bool success = temporary_dir_.CreateUniqueTempDir();
    if (success) {
      base::FilePath dir_path = temporary_dir_.GetPath();
      filename_ = dir_path.value() + "chromeossampletest";
      filepath_ = base::FilePath(filename_);
    }
  }

  void SetUp() override { base::DeleteFile(filepath_); }

  void TestSerialization(MetricSample* sample) {
    std::string serialized(sample->ToString());
    ASSERT_EQ('\0', serialized.back());
    std::unique_ptr<MetricSample> deserialized =
        SerializationUtils::ParseSample(serialized);
    ASSERT_TRUE(deserialized);
    EXPECT_TRUE(sample->IsEqual(*deserialized.get()));
  }

  const std::string& filename() const { return filename_; }
  const base::FilePath& filepath() const { return filepath_; }

 private:
  std::string filename_;
  base::ScopedTempDir temporary_dir_;
  base::FilePath filepath_;
};

TEST_F(SerializationUtilsTest, CrashSerializeTest) {
  // Should work with both 1 and non-1 values
  TestSerialization(MetricSample::CrashSample("test", /*num_samples=*/1).get());
  TestSerialization(
      MetricSample::CrashSample("test", /*num_samples=*/10).get());
}

TEST_F(SerializationUtilsTest, HistogramSerializeTest) {
  TestSerialization(MetricSample::HistogramSample(
                        "myhist", /*sample=*/13, /*min=*/1, /*max=*/100,
                        /*bucket_count=*/10, /*num_samples=*/1)
                        .get());
  TestSerialization(MetricSample::HistogramSample(
                        "myhist", /*sample=*/13, /*min=*/1, /*max=*/100,
                        /*bucket_count=*/10, /*num_samples=*/2)
                        .get());
}

TEST_F(SerializationUtilsTest, LinearSerializeTest) {
  TestSerialization(
      MetricSample::LinearHistogramSample("linearhist", /*sample=*/12,
                                          /*max=*/30, /*num_samples=*/1)
          .get());
  TestSerialization(
      MetricSample::LinearHistogramSample("linearhist", /*sample=*/12,
                                          /*max=*/30, /*num_samples=*/10)
          .get());
}

TEST_F(SerializationUtilsTest, SparseSerializeTest) {
  TestSerialization(MetricSample::SparseHistogramSample(
                        "mysparse", /*sample=*/30, /*num_samples=*/1)
                        .get());
  TestSerialization(MetricSample::SparseHistogramSample(
                        "mysparse", /*sample=*/30, /*num_samples=*/10)
                        .get());
}

TEST_F(SerializationUtilsTest, UserActionSerializeTest) {
  TestSerialization(
      MetricSample::UserActionSample("myaction", /*num_samples=*/1).get());
  TestSerialization(
      MetricSample::UserActionSample("myaction", /*num_samples=*/10).get());
}

TEST_F(SerializationUtilsTest, InvalidCrashSerialize) {
  // No name
  EXPECT_EQ(nullptr, MetricSample::ParseCrash(""));
  // Empty name
  EXPECT_EQ(nullptr, MetricSample::ParseCrash(" "));
  // num_samples is not a number
  EXPECT_EQ(nullptr, MetricSample::ParseCrash("kernel asdf"));
  // Too many numbers
  EXPECT_EQ(nullptr, MetricSample::ParseCrash("kernel 1 2"));
  // Negative num_samples
  EXPECT_EQ(nullptr, MetricSample::ParseCrash("kernel -1"));
}

TEST_F(SerializationUtilsTest, InvalidHistogramSample) {
  // Too few parts
  EXPECT_EQ(nullptr, MetricSample::ParseHistogram("hist 1 2 3"));
  // Too many parts
  EXPECT_EQ(nullptr, MetricSample::ParseHistogram("hist 1 2 3 4 5 6"));
  // Empty hist name
  EXPECT_EQ(nullptr, MetricSample::ParseHistogram(" 1 2 3 4 5"));
  // sample is not a number
  EXPECT_EQ(nullptr, MetricSample::ParseHistogram("hist a 2 3 4 5"));
  // min is not a number
  EXPECT_EQ(nullptr, MetricSample::ParseHistogram("hist 1 a 3 4 5"));
  // max is not a number
  EXPECT_EQ(nullptr, MetricSample::ParseHistogram("hist 1 2 a 4 5"));
  // buckets is not a number
  EXPECT_EQ(nullptr, MetricSample::ParseHistogram("hist 1 2 3 a 5"));
  // num_samples is not a number
  EXPECT_EQ(nullptr, MetricSample::ParseHistogram("hist 1 2 3 4 a"));
  // Negative num_samples
  EXPECT_EQ(nullptr, MetricSample::ParseHistogram("hist 1 2 3 4 -1"));
}

TEST_F(SerializationUtilsTest, InvalidSparseHistogramSample) {
  // Too few fields
  EXPECT_EQ(nullptr, MetricSample::ParseSparseHistogram("name"));
  // Too many fields
  EXPECT_EQ(nullptr, MetricSample::ParseSparseHistogram("name 1 2 3"));
  // No name
  EXPECT_EQ(nullptr, MetricSample::ParseSparseHistogram(" 1 2"));
  // Invalid sample
  EXPECT_EQ(nullptr, MetricSample::ParseSparseHistogram("name a 2"));
  // Invalid num_samples
  EXPECT_EQ(nullptr, MetricSample::ParseSparseHistogram("name 1 a"));
  // Negative num_samples
  EXPECT_EQ(nullptr, MetricSample::ParseSparseHistogram("name 1 -1"));
}

TEST_F(SerializationUtilsTest, InvalidLinearHistogramSample) {
  // Too few fields
  EXPECT_EQ(nullptr, MetricSample::ParseLinearHistogram("name 1"));
  // Too many fields
  EXPECT_EQ(nullptr, MetricSample::ParseLinearHistogram("name 1 2 3 4"));
  // No name
  EXPECT_EQ(nullptr, MetricSample::ParseLinearHistogram(" 1 2 3"));
  // Invalid sample
  EXPECT_EQ(nullptr, MetricSample::ParseLinearHistogram("name a 2 3"));
  // Invalid max
  EXPECT_EQ(nullptr, MetricSample::ParseLinearHistogram("name 1 a 3"));
  // Invalid num_samples
  EXPECT_EQ(nullptr, MetricSample::ParseLinearHistogram("name 1 2 a"));
  // Negative num_samples
  EXPECT_EQ(nullptr, MetricSample::ParseLinearHistogram("name 1 2 -1"));
}

TEST_F(SerializationUtilsTest, InvalidUserAction) {
  // Too few fields
  EXPECT_EQ(nullptr, MetricSample::ParseUserAction(""));
  // Too many fields
  EXPECT_EQ(nullptr, MetricSample::ParseUserAction("name 1 2"));
  // No name
  EXPECT_EQ(nullptr, MetricSample::ParseUserAction(" 1"));
  // Invalid num_samples
  EXPECT_EQ(nullptr, MetricSample::ParseUserAction("name a"));
  // Negative num_samples
  EXPECT_EQ(nullptr, MetricSample::ParseUserAction("name -1"));
}

TEST_F(SerializationUtilsTest, IllegalNameAreFilteredTest) {
  std::unique_ptr<MetricSample> sample1 = MetricSample::SparseHistogramSample(
      "no space", /*sample=*/10, /*num_samples=*/1);
  std::unique_ptr<MetricSample> sample2 = MetricSample::LinearHistogramSample(
      base::StringPrintf("here%cbhe", '\0'), /*sample=*/1, /*max=*/3,
      /*num_samples=*/2);

  EXPECT_FALSE(
      SerializationUtils::WriteMetricToFile(*sample1.get(), filename()));
  EXPECT_FALSE(
      SerializationUtils::WriteMetricToFile(*sample2.get(), filename()));
  int64_t size = 0;

  ASSERT_TRUE(!PathExists(filepath()) || base::GetFileSize(filepath(), &size));

  EXPECT_EQ(0, size);
}

TEST_F(SerializationUtilsTest, BadInputIsCaughtTest) {
  std::string input(
      base::StringPrintf("sparsehistogram%cname foo%c1", '\0', '\0'));
  EXPECT_EQ(nullptr, MetricSample::ParseSparseHistogram(input).get());
}

TEST_F(SerializationUtilsTest, MessageSeparatedByZero) {
  std::unique_ptr<MetricSample> crash =
      MetricSample::CrashSample("mycrash", /*num_samples=*/10);

  SerializationUtils::WriteMetricToFile(*crash.get(), filename());
  int64_t size = 0;
  ASSERT_TRUE(base::GetFileSize(filepath(), &size));
  // 4 bytes for the size
  // 5 bytes for crash
  // 1 byte for \0
  // 7 bytes for mycrash
  // 3 bytes for " 10"
  // 1 byte for \0
  // -> total of 21
  EXPECT_EQ(size, 21);
}

TEST_F(SerializationUtilsTest, MessagesTooLongAreDiscardedTest) {
  // Creates a message that is bigger than the maximum allowed size.
  // As we are adding extra character (crash, \0s, etc), if the name is
  // kMessageMaxLength long, it will be too long.
  std::string name(SerializationUtils::kMessageMaxLength, 'c');

  std::unique_ptr<MetricSample> crash =
      MetricSample::CrashSample(name, /*num_samples=*/10);
  EXPECT_FALSE(SerializationUtils::WriteMetricToFile(*crash.get(), filename()));
  int64_t size = 0;
  ASSERT_TRUE(base::GetFileSize(filepath(), &size));
  EXPECT_EQ(0, size);
}

TEST_F(SerializationUtilsTest, ReadLongMessageTest) {
  base::File test_file(filepath(),
                       base::File::FLAG_OPEN_ALWAYS | base::File::FLAG_APPEND);
  std::string message(SerializationUtils::kMessageMaxLength + 1, 'c');

  int32_t message_size = message.length() + sizeof(int32_t);
  test_file.WriteAtCurrentPos(base::byte_span_from_ref(message_size));
  test_file.WriteAtCurrentPos(base::as_byte_span(message));
  test_file.Close();

  std::unique_ptr<MetricSample> crash =
      MetricSample::CrashSample("test", /*num_samples=*/10);
  SerializationUtils::WriteMetricToFile(*crash.get(), filename());

  std::vector<std::unique_ptr<MetricSample>> samples;
  SerializationUtils::ReadAndTruncateMetricsFromFile(filename(), &samples);
  ASSERT_EQ(size_t(1), samples.size());
  ASSERT_TRUE(samples[0].get() != nullptr);
  EXPECT_TRUE(crash->IsEqual(*samples[0]));
}

TEST_F(SerializationUtilsTest, NegativeLengthTest) {
  // This input is specifically constructed to yield a single crash sample when
  // parsed by a buggy version of the code but fails to parse and doesn't yield
  // samples when parsed by a correct implementation.
  constexpr uint8_t kInput[] = {
      // Length indicating that next length field is the negative one below.
      // This sample is invalid as it contains more than three null bytes.
      0x14,
      0x00,
      0x00,
      0x00,
      // Encoding of a valid crash sample.
      0x0c,
      0x00,
      0x00,
      0x00,
      0x63,
      0x72,
      0x61,
      0x73,
      0x68,
      0x00,
      0x61,
      0x00,
      // Invalid sample that jumps past the negative length bytes below.
      0x08,
      0x00,
      0x00,
      0x00,
      // This is -16 in two's complement interpretation, pointing to the valid
      // crash sample before.
      0xf0,
      0xff,
      0xff,
      0xff,
  };
  ASSERT_TRUE(base::WriteFile(filepath(), base::make_span(kInput)));

  std::vector<std::unique_ptr<MetricSample>> samples;
  SerializationUtils::ReadAndTruncateMetricsFromFile(filename(), &samples);
  ASSERT_EQ(0U, samples.size());
}

TEST_F(SerializationUtilsTest, WriteReadTest_TruncateFile) {
  std::unique_ptr<MetricSample> hist = MetricSample::HistogramSample(
      "myhist", /*sample=*/1, /*min=*/2, /*max=*/3, /*bucket_count=*/4,
      /*num_samples=*/5);
  std::unique_ptr<MetricSample> crash =
      MetricSample::CrashSample("mycrash", /*num_samples=*/10);
  std::unique_ptr<MetricSample> lhist = MetricSample::LinearHistogramSample(
      "linear", /*sample=*/1, /*max=*/10, /*num_samples=*/10);
  std::unique_ptr<MetricSample> shist = MetricSample::SparseHistogramSample(
      "mysparse", /*sample=*/30, /*num_samples=*/10);
  std::unique_ptr<MetricSample> action =
      MetricSample::UserActionSample("myaction", /*num_samples=*/1);

  SerializationUtils::WriteMetricToFile(*hist.get(), filename());
  SerializationUtils::WriteMetricToFile(*crash.get(), filename());
  SerializationUtils::WriteMetricToFile(*lhist.get(), filename());
  SerializationUtils::WriteMetricToFile(*shist.get(), filename());
  SerializationUtils::WriteMetricToFile(*action.get(), filename());
  std::vector<std::unique_ptr<MetricSample>> vect;
  SerializationUtils::ReadAndTruncateMetricsFromFile(filename(), &vect);
  // NOTE: Should *not* have an entry for each repeated sample.
  ASSERT_EQ(vect.size(), size_t(5));
  for (auto& sample : vect) {
    ASSERT_NE(nullptr, sample.get());
  }
  EXPECT_TRUE(hist->IsEqual(*vect[0]));
  EXPECT_TRUE(crash->IsEqual(*vect[1]));
  EXPECT_TRUE(lhist->IsEqual(*vect[2]));
  EXPECT_TRUE(shist->IsEqual(*vect[3]));
  EXPECT_TRUE(action->IsEqual(*vect[4]));

  int64_t size = 0;
  ASSERT_TRUE(base::GetFileSize(filepath(), &size));
  ASSERT_EQ(0, size);
}

TEST_F(SerializationUtilsTest, WriteReadTest_DeleteFile) {
  std::unique_ptr<MetricSample> hist = MetricSample::HistogramSample(
      "myhist", /*sample=*/1, /*min=*/2, /*max=*/3, /*bucket_count=*/4,
      /*num_samples=*/5);
  std::unique_ptr<MetricSample> crash =
      MetricSample::CrashSample("mycrash", /*num_samples=*/10);
  std::unique_ptr<MetricSample> lhist = MetricSample::LinearHistogramSample(
      "linear", /*sample=*/1, /*max=*/10, /*num_samples=*/10);
  std::unique_ptr<MetricSample> shist = MetricSample::SparseHistogramSample(
      "mysparse", /*sample=*/30, /*num_samples=*/10);
  std::unique_ptr<MetricSample> action =
      MetricSample::UserActionSample("myaction", /*num_samples=*/1);

  SerializationUtils::WriteMetricToFile(*hist.get(), filename());
  SerializationUtils::WriteMetricToFile(*crash.get(), filename());
  SerializationUtils::WriteMetricToFile(*lhist.get(), filename());
  SerializationUtils::WriteMetricToFile(*shist.get(), filename());
  SerializationUtils::WriteMetricToFile(*action.get(), filename());
  std::vector<std::unique_ptr<MetricSample>> vect;
  SerializationUtils::ReadAndDeleteMetricsFromFile(filename(), &vect);
  // NOTE: Should *not* have an entry for each repeated sample.
  ASSERT_EQ(vect.size(), size_t(5));
  for (auto& sample : vect) {
    ASSERT_NE(nullptr, sample.get());
  }
  EXPECT_TRUE(hist->IsEqual(*vect[0]));
  EXPECT_TRUE(crash->IsEqual(*vect[1]));
  EXPECT_TRUE(lhist->IsEqual(*vect[2]));
  EXPECT_TRUE(shist->IsEqual(*vect[3]));
  EXPECT_TRUE(action->IsEqual(*vect[4]));

  EXPECT_FALSE(base::PathExists(filepath()));
}

TEST_F(SerializationUtilsTest, TooManyMessagesTest) {
  std::unique_ptr<MetricSample> hist = MetricSample::HistogramSample(
      "myhist", /*sample=*/1, /*min=*/2, /*max=*/3, /*bucket_count=*/4,
      /*num_samples=*/5);

  constexpr int kDiscardedSamples = 50000;
  for (int i = 0;
       i < SerializationUtils::kMaxMessagesPerRead + kDiscardedSamples; i++) {
    SerializationUtils::WriteMetricToFile(*hist.get(), filename());
  }

  std::vector<std::unique_ptr<MetricSample>> vect;
  SerializationUtils::ReadAndTruncateMetricsFromFile(filename(), &vect);
  ASSERT_EQ(SerializationUtils::kMaxMessagesPerRead,
            static_cast<int>(vect.size()));
  for (auto& sample : vect) {
    ASSERT_NE(nullptr, sample.get());
    EXPECT_TRUE(hist->IsEqual(*sample));
  }

  int64_t size = 0;
  ASSERT_TRUE(base::GetFileSize(filepath(), &size));
  ASSERT_EQ(0, size);
}

TEST_F(SerializationUtilsTest, ReadEmptyFile) {
  {
    // Create a zero-length file and then close file descriptor.
    base::File file(filepath(),
                    base::File::FLAG_CREATE | base::File::FLAG_WRITE);
    ASSERT_TRUE(file.IsValid());
  }

  std::vector<std::unique_ptr<MetricSample>> vect;
  SerializationUtils::ReadAndTruncateMetricsFromFile(filename(), &vect);
  EXPECT_THAT(vect, IsEmpty());
}

TEST_F(SerializationUtilsTest, ReadNonExistentFile) {
  base::DeleteFile(filepath());  // Ensure non-existance.
  base::HistogramTester histogram_tester;
  std::vector<std::unique_ptr<MetricSample>> vect;
  SerializationUtils::ReadAndTruncateMetricsFromFile(filename(), &vect);
  EXPECT_THAT(vect, IsEmpty());
}

TEST_F(SerializationUtilsTest, ParseInvalidType) {
  // Verify that parsing of an invalid sample type fails.
  EXPECT_EQ(nullptr, SerializationUtils::ParseSample(base::StringPrintf(
                         "not_a_type%cvalue%c", '\0', '\0')));
}

}  // namespace
}  // namespace metrics
