// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/metrics/dwa/dwa_entry_builder.h"

#include <string>
#include <unordered_set>

#include "base/metrics/metrics_hashes.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace dwa {

// Tests that calling `SetMetric` repeatedly on an `DwaEntryBuilder` updates the
// value stored for the metric.
TEST(DwaEntryBuilderTest, BuilderAllowsUpdatingMetrics) {
  DwaEntryBuilder builder("Kangaroo.Jumped");
  builder.SetMetric("Length", 4);
  EXPECT_THAT(
      (*builder.GetEntryForTesting())->metrics,
      testing::ElementsAre(testing::Pair(base::HashMetricName("Length"), 4)));

  builder.SetMetric("Length", 5);
  EXPECT_THAT(
      (*builder.GetEntryForTesting())->metrics,
      testing::ElementsAre(testing::Pair(base::HashMetricName("Length"), 5)));
}

// Tests that calling `AddStudy` repeatedly on an `DwaEntryBuilder` updates the
// value stored for the study.
TEST(DwaEntryBuilderTest, BuilderAllowsAddingStudiesOfInterest) {
  DwaEntryBuilder builder("Kangaroo.Jumped");
  builder.AddToStudiesOfInterest("Study1");
  EXPECT_THAT((*builder.GetEntryForTesting())->studies_of_interest,
              testing::ElementsAre(testing::Pair("Study1", true)));

  builder.AddToStudiesOfInterest("Study2");
  EXPECT_THAT((*builder.GetEntryForTesting())->studies_of_interest,
              testing::ElementsAre(testing::Pair("Study1", true),
                                   testing::Pair("Study2", true)));

  builder.AddToStudiesOfInterest("Study1");
  EXPECT_THAT((*builder.GetEntryForTesting())->studies_of_interest,
              testing::ElementsAre(testing::Pair("Study1", true),
                                   testing::Pair("Study2", true)));
}

TEST(DwaEntryBuilderTest, BuilderAllowsSettingContent) {
  DwaEntryBuilder builder("Kangaroo.Jumped");
  builder.SetContent("https://google.com/content");
  EXPECT_THAT((*builder.GetEntryForTesting())->content_hash,
              testing::Eq(base::HashMetricName("google.com")));

  // Invalid content are expected to be hashed as empty strings.
  builder.SetContent("content_");
  EXPECT_THAT((*builder.GetEntryForTesting())->content_hash,
              testing::Eq(base::HashMetricName("")));
}

TEST(DwaEntryBuilderTest, SanitizeContent) {
  struct {
    std::string input;
    std::string expected_output;
  } test_cases[] = {
      {"https://google.com", "google.com"},
      {"https://google.com/", "google.com"},
      {"https://google.com/mail", "google.com"},
      {"https://google.com/file.html", "google.com"},
      {"https://www.google.com/mail", "google.com"},
      {"https://..google.com/file.html", "google.com"},
      {"https://www.google.co.uk/mail", "google.co.uk"},
      {"http://www.google.co.uk/mail", "google.co.uk"},
      {"http://www.google.co.uk/mail/", "google.co.uk"},
      {"http://www.google.co.uk/xyz/index.html", "google.co.uk"},
      {"mail://www.google.co.uk/mail", "google.co.uk"},
      {"https://google.com./file.html", "google.com."},
      {"https://a.b.co.uk/file.html", "b.co.uk"},
      {"http://foo.bar/file.html", "foo.bar"},
      {"google.co.uk/", ""},
      {"google.co.uk", ""},
      {"google.co.uk/mail", ""},
      {"www.google.co.uk/mail/", ""},
      {"file:///C:/bar.html", ""},
      {"http://foo.com../file.html", ""},
      {"http://192.168.0.1/file.html", ""},
      {"http://bar/file.html", ""},
      {"http://co.uk/file.html", ""},
  };

  for (const auto& test_case : test_cases) {
    EXPECT_THAT(DwaEntryBuilder::SanitizeContent(test_case.input),
                testing::Eq(test_case.expected_output));
  }
}

}  // namespace dwa
