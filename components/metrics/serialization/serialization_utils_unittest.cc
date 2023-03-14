// Copyright 2014 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/metrics/serialization/serialization_utils.h"

#include <stddef.h>
#include <stdint.h>

#include "base/check.h"
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
  TestSerialization(MetricSample::CrashSample("test").get());
}

TEST_F(SerializationUtilsTest, HistogramSerializeTest) {
  TestSerialization(
      MetricSample::HistogramSample("myhist", 13, 1, 100, 10).get());
}

TEST_F(SerializationUtilsTest, LinearSerializeTest) {
  TestSerialization(
      MetricSample::LinearHistogramSample("linearhist", 12, 30).get());
}

TEST_F(SerializationUtilsTest, SparseSerializeTest) {
  TestSerialization(MetricSample::SparseHistogramSample("mysparse", 30).get());
}

TEST_F(SerializationUtilsTest, UserActionSerializeTest) {
  TestSerialization(MetricSample::UserActionSample("myaction").get());
}

TEST_F(SerializationUtilsTest, IllegalNameAreFilteredTest) {
  std::unique_ptr<MetricSample> sample1 =
      MetricSample::SparseHistogramSample("no space", 10);
  std::unique_ptr<MetricSample> sample2 = MetricSample::LinearHistogramSample(
      base::StringPrintf("here%cbhe", '\0'), 1, 3);

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
      base::StringPrintf("sparsehistogram%cname foo%c", '\0', '\0'));
  EXPECT_EQ(nullptr, MetricSample::ParseSparseHistogram(input).get());
}

TEST_F(SerializationUtilsTest, MessageSeparatedByZero) {
  std::unique_ptr<MetricSample> crash = MetricSample::CrashSample("mycrash");

  SerializationUtils::WriteMetricToFile(*crash.get(), filename());
  int64_t size = 0;
  ASSERT_TRUE(base::GetFileSize(filepath(), &size));
  // 4 bytes for the size
  // 5 bytes for crash
  // 7 bytes for mycrash
  // 2 bytes for the \0
  // -> total of 18
  EXPECT_EQ(size, 18);
}

TEST_F(SerializationUtilsTest, MessagesTooLongAreDiscardedTest) {
  // Creates a message that is bigger than the maximum allowed size.
  // As we are adding extra character (crash, \0s, etc), if the name is
  // kMessageMaxLength long, it will be too long.
  std::string name(SerializationUtils::kMessageMaxLength, 'c');

  std::unique_ptr<MetricSample> crash = MetricSample::CrashSample(name);
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
  test_file.WriteAtCurrentPos(reinterpret_cast<const char*>(&message_size),
                              sizeof(message_size));
  test_file.WriteAtCurrentPos(message.c_str(), message.length());
  test_file.Close();

  std::unique_ptr<MetricSample> crash = MetricSample::CrashSample("test");
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

TEST_F(SerializationUtilsTest, WriteReadTest) {
  std::unique_ptr<MetricSample> hist =
      MetricSample::HistogramSample("myhist", 1, 2, 3, 4);
  std::unique_ptr<MetricSample> crash = MetricSample::CrashSample("mycrash");
  std::unique_ptr<MetricSample> lhist =
      MetricSample::LinearHistogramSample("linear", 1, 10);
  std::unique_ptr<MetricSample> shist =
      MetricSample::SparseHistogramSample("mysparse", 30);
  std::unique_ptr<MetricSample> action =
      MetricSample::UserActionSample("myaction");

  SerializationUtils::WriteMetricToFile(*hist.get(), filename());
  SerializationUtils::WriteMetricToFile(*crash.get(), filename());
  SerializationUtils::WriteMetricToFile(*lhist.get(), filename());
  SerializationUtils::WriteMetricToFile(*shist.get(), filename());
  SerializationUtils::WriteMetricToFile(*action.get(), filename());
  std::vector<std::unique_ptr<MetricSample>> vect;
  SerializationUtils::ReadAndTruncateMetricsFromFile(filename(), &vect);
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

TEST_F(SerializationUtilsTest, TooManyMessagesTest) {
  std::unique_ptr<MetricSample> hist =
      MetricSample::HistogramSample("myhist", 1, 2, 3, 4);

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

}  // namespace
}  // namespace metrics
