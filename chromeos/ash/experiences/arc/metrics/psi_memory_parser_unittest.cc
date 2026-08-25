// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chromeos/ash/experiences/arc/metrics/psi_memory_parser.h"

#include <memory>

#include "base/metrics/statistics_recorder.h"
#include "base/test/metrics/histogram_tester.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace arc {

namespace {

// Just as the kernel outputs.
const char kFileContents1[] =
    "some avg10=23.10 avg60=5.06 avg300=15.10 total=417963\n"
    "full avg10=9.00 avg60=19.20 avg300=3.23 total=205933\n";

// Number of decimals not consistent, slightly malformed - but acceptable.
const char kFileContents2[] =
    "some avg10=24 avg60=5.06 avg300=15.10 total=417963\n"
    "full avg10=9.2 avg60=19.20 avg300=3.23 total=205933\n";

}  // namespace

class PSIMemoryParserTest : public testing::Test {
 public:
  PSIMemoryParserTest() = default;
  ~PSIMemoryParserTest() override = default;

  void Init(base::TimeDelta period) {
    cit_ = std::make_unique<PSIMemoryParser>(period);
  }

  base::TimeDelta GetPeriod() { return cit_->GetPeriod(); }
  base::HistogramTester& Histograms() { return histogram_tester_; }
  std::unique_ptr<PSIMemoryParser>& Cit() { return cit_; }
  const std::string& GetMetricPrefix() { return cit_->metric_prefix_; }

  void KillCit() { cit_.reset(); }

 private:
  std::unique_ptr<PSIMemoryParser> cit_;
  base::HistogramTester histogram_tester_;
};

TEST_F(PSIMemoryParserTest, CustomInterval) {
  Init(base::Seconds(60));

  EXPECT_EQ(base::Seconds(60), GetPeriod());
}

TEST_F(PSIMemoryParserTest, InvalidInterval) {
  Init(base::Seconds(15));

  EXPECT_EQ(base::Seconds(10), GetPeriod());
}

TEST_F(PSIMemoryParserTest, InternalsA) {
  Init(base::Seconds(10));

  std::string testContent1 = "prefix" + GetMetricPrefix() + "9.37 suffix";
  EXPECT_EQ(base::Seconds(10), GetPeriod());

  size_t s = 0;
  size_t e = 0;

  EXPECT_EQ(false, internal::FindMiddleString(testContent1, 0, "nothere",
                                              "suffix", &s, &e));

  EXPECT_EQ(false, internal::FindMiddleString(testContent1, 0, "prefix",
                                              "notthere", &s, &e));

  EXPECT_EQ(true, internal::FindMiddleString(testContent1, 0, "prefix",
                                             "suffix", &s, &e));
  EXPECT_EQ(6u, s);
  EXPECT_EQ(17u, e);

  EXPECT_EQ(937, Cit()->GetMetricValue(testContent1, s, e));

  std::string testContent2 = "extra " + testContent1;
  EXPECT_EQ(true, internal::FindMiddleString(testContent2, 0, "prefix",
                                             "suffix", &s, &e));
  EXPECT_EQ(12u, s);
  EXPECT_EQ(23u, e);

  EXPECT_EQ(937, Cit()->GetMetricValue(testContent2, s, e));
}

TEST_F(PSIMemoryParserTest, InternalsB) {
  Init(base::Seconds(300));

  int msome;
  int mfull;
  ParsePSIMemStatus stat;

  stat = Cit()->ParseMetrics(kFileContents1, &msome, &mfull);

  EXPECT_EQ(ParsePSIMemStatus::kSuccess, stat);
  EXPECT_EQ(1510, msome);
  EXPECT_EQ(323, mfull);
}

TEST_F(PSIMemoryParserTest, InternalsC) {
  Init(base::Seconds(60));

  int msome;
  int mfull;
  ParsePSIMemStatus stat;

  stat = Cit()->ParseMetrics(kFileContents1, &msome, &mfull);

  EXPECT_EQ(ParsePSIMemStatus::kSuccess, stat);
  EXPECT_EQ(506, msome);
  EXPECT_EQ(1920, mfull);
}

TEST_F(PSIMemoryParserTest, InternalsD) {
  Init(base::Seconds(10));

  int msome;
  int mfull;
  ParsePSIMemStatus stat;

  stat = Cit()->ParseMetrics(kFileContents1, &msome, &mfull);

  EXPECT_EQ(ParsePSIMemStatus::kSuccess, stat);
  EXPECT_EQ(2310, msome);
  EXPECT_EQ(900, mfull);
}

TEST_F(PSIMemoryParserTest, InternalsE) {
  Init(base::Seconds(10));

  int msome;
  int mfull;
  ParsePSIMemStatus stat;

  stat = Cit()->ParseMetrics(kFileContents2, &msome, &mfull);

  EXPECT_EQ(ParsePSIMemStatus::kSuccess, stat);
  EXPECT_EQ(2400, msome);
  EXPECT_EQ(920, mfull);
}

TEST_F(PSIMemoryParserTest, ParseResultCounter) {
  Init(base::Seconds(10));

  Cit()->LogParseStatus(ParsePSIMemStatus::kSuccess);
  Cit()->LogParseStatus(ParsePSIMemStatus::kInvalidMetricFormat);
  Cit()->LogParseStatus(ParsePSIMemStatus::kInvalidMetricFormat);

  Histograms().ExpectBucketCount("ChromeOS.CWP.ParsePSIMemory",
                                 ParsePSIMemStatus::kSuccess, 1);
  Histograms().ExpectBucketCount("ChromeOS.CWP.ParsePSIMemory",
                                 ParsePSIMemStatus::kInvalidMetricFormat, 2);
}

}  // namespace arc
