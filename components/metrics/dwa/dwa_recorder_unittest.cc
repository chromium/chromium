// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/metrics/dwa/dwa_recorder.h"

#include <unordered_map>
#include <vector>

#include "base/metrics/field_trial.h"
#include "base/metrics/metrics_hashes.h"
#include "base/test/scoped_feature_list.h"
#include "components/metrics/dwa/dwa_entry_builder.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace metrics::dwa {
namespace {

// TODO(b/375239908): Increase the code readability of the following helper
// conversion functions.

// Converts `dwa_events` into vector of DWA event_hashes.
std::vector<uint64_t> GetDwaEventHashesFromDwaEvents(
    const std::vector<::dwa::DeidentifiedWebAnalyticsEvent>& dwa_events) {
  std::vector<uint64_t> dwa_event_hashes;
  dwa_event_hashes.reserve(dwa_events.size());
  for (const auto& dwa_event : dwa_events) {
    dwa_event_hashes.push_back(dwa_event.event_hash());
  }
  return dwa_event_hashes;
}

// Converts |repeated_ptr_field_metrics| into an unordered_map of metrics. Each
// metric in metrics consists of a name_hash (key) and value (value).
std::unordered_map<uint64_t, int64_t> RepeatedPtrFieldMetricToUnorderedMap(
    const google::protobuf::RepeatedPtrField<
        ::dwa::DeidentifiedWebAnalyticsEvent::ContentMetric::EntryMetrics::
            Metric>& repeated_ptr_field_metrics) {
  std::unordered_map<uint64_t, int64_t> metrics;
  metrics.reserve(repeated_ptr_field_metrics.size());
  for (const auto& metric : repeated_ptr_field_metrics) {
    metrics[metric.name_hash()] = metric.value();
  }
  return metrics;
}

// Converts |repeated_ptr_field_content_metric| into a vector of contents. Each
// content in contents is a std::pair of content_type (first) and content_hash
// (second).
std::vector<
    std::pair<::dwa::DeidentifiedWebAnalyticsEvent::ContentMetric::ContentType,
              uint64_t>>
RepeatedPtrFieldContentMetricToVector(
    const google::protobuf::RepeatedPtrField<
        ::dwa::DeidentifiedWebAnalyticsEvent::ContentMetric>&
        repeated_ptr_field_content_metric) {
  std::vector<std::pair<
      ::dwa::DeidentifiedWebAnalyticsEvent::ContentMetric::ContentType,
      uint64_t>>
      contents;
  contents.reserve(repeated_ptr_field_content_metric.size());
  for (const auto& content : repeated_ptr_field_content_metric) {
    contents.emplace_back(content.content_type(), content.content_hash());
  }
  return contents;
}

// Converts |repeated_ptr_field_field_trials| into vector of field trial groups.
// Each field trial group consist of a pair of field trial name hash (first) and
// group name hash (second).
std::vector<std::pair<uint32_t, uint32_t>> RepeatedPtrFieldFieldTrialsToVector(
    const google::protobuf::RepeatedPtrField<
        ::metrics::SystemProfileProto::FieldTrial>&
        repeated_ptr_field_field_trials) {
  std::vector<std::pair<uint32_t, uint32_t>> field_trial_groups;
  field_trial_groups.reserve(repeated_ptr_field_field_trials.size());
  for (const auto& field_trial : repeated_ptr_field_field_trials) {
    field_trial_groups.emplace_back(field_trial.name_id(),
                                    field_trial.group_id());
  }
  return field_trial_groups;
}

}  // namespace

class DwaRecorderTestBase : public testing::Test {
 public:
  explicit DwaRecorderTestBase(bool enable_feature) {
    if (enable_feature) {
      scoped_feature_list_.InitAndEnableFeature(kDwaFeature);
    } else {
      scoped_feature_list_.InitAndDisableFeature(kDwaFeature);
    }
    recorder_ = DwaRecorder::Get();
    recorder_->Purge();
    recorder_->EnableRecording();
  }
  ~DwaRecorderTestBase() override = default;

  DwaRecorder* GetRecorder() { return recorder_; }

 private:
  raw_ptr<DwaRecorder> recorder_;
  base::test::ScopedFeatureList scoped_feature_list_;
};

class DwaRecorderEnabledTest : public DwaRecorderTestBase {
 public:
  DwaRecorderEnabledTest() : DwaRecorderTestBase(/*enable_feature=*/true) {}
};

class DwaRecorderDisabledTest : public DwaRecorderTestBase {
 public:
  DwaRecorderDisabledTest() : DwaRecorderTestBase(/*enable_feature=*/false) {}
};

TEST_F(DwaRecorderEnabledTest, ValidateHasEntriesWhenEntryIsAdded) {
  ::dwa::DwaEntryBuilder builder("Kangaroo.Jumped");
  builder.SetContent("https://adtech.com");
  builder.SetMetric("Length", 5);
  builder.Record(GetRecorder());

  EXPECT_TRUE(GetRecorder()->HasEntries());
}

TEST_F(DwaRecorderEnabledTest, ValidateEntriesWhenRecordingIsDisabled) {
  GetRecorder()->DisableRecording();

  ::dwa::DwaEntryBuilder builder("Kangaroo.Jumped");
  builder.SetContent("https://adtech.com");
  builder.SetMetric("Length", 5);
  builder.Record(GetRecorder());

  EXPECT_FALSE(GetRecorder()->HasEntries());
}

TEST_F(DwaRecorderEnabledTest, ValidateRecorderRecordsForVaryingMetrics) {
  ::dwa::DwaEntryBuilder builder_1("Kangaroo.Jumped");
  builder_1.SetContent("https://adtech.com");
  builder_1.SetMetric("Length", 5);
  builder_1.SetMetric("Width", 10);
  builder_1.Record(GetRecorder());

  ::dwa::DwaEntryBuilder builder_2("Kangaroo.Jumped");
  builder_2.SetContent("https://adtech.com");
  builder_2.SetMetric("Length", 3);
  builder_2.SetMetric("Width", 12);
  builder_2.Record(GetRecorder());

  EXPECT_TRUE(GetRecorder()->HasEntries());

  auto dwa_events = GetRecorder()->TakeDwaEvents();
  EXPECT_FALSE(dwa_events.empty());
  ASSERT_EQ(dwa_events.size(), 1u);

  EXPECT_EQ(dwa_events[0].event_hash(),
            base::HashMetricName("Kangaroo.Jumped"));
  ASSERT_EQ(dwa_events[0].content_metrics().size(), 1);
  EXPECT_EQ(
      dwa_events[0].content_metrics(0).content_type(),
      ::dwa::DeidentifiedWebAnalyticsEvent::ContentMetric::CONTENT_TYPE_URL);
  EXPECT_EQ(dwa_events[0].content_metrics(0).content_hash(),
            base::HashMetricName("adtech.com"));
  ASSERT_EQ(dwa_events[0].content_metrics(0).metrics().size(), 2);

  EXPECT_EQ(dwa_events[0].content_metrics(0).metrics(0).metric().size(), 2);
  EXPECT_THAT(RepeatedPtrFieldMetricToUnorderedMap(
                  dwa_events[0].content_metrics(0).metrics(0).metric()),
              testing::UnorderedElementsAre(
                  testing::Pair(base::HashMetricName("Length"), 5),
                  testing::Pair(base::HashMetricName("Width"), 10)));

  EXPECT_EQ(dwa_events[0].content_metrics(0).metrics(1).metric().size(), 2);
  EXPECT_THAT(RepeatedPtrFieldMetricToUnorderedMap(
                  dwa_events[0].content_metrics(0).metrics(1).metric()),
              testing::UnorderedElementsAre(
                  testing::Pair(base::HashMetricName("Length"), 3),
                  testing::Pair(base::HashMetricName("Width"), 12)));
}

TEST_F(DwaRecorderEnabledTest, ValidateRecorderRecordsForVaryingContent) {
  ::dwa::DwaEntryBuilder builder_1("Kangaroo.Jumped");
  builder_1.SetContent("https://adtech.com");
  builder_1.SetMetric("Latency", 10);
  builder_1.Record(GetRecorder());

  ::dwa::DwaEntryBuilder builder_2("Kangaroo.Jumped");
  builder_2.SetContent("https://adtech.com");
  builder_2.SetMetric("Latency", 12);
  builder_2.Record(GetRecorder());

  ::dwa::DwaEntryBuilder builder_3("Kangaroo.Jumped");
  builder_3.SetContent("https://adtech2.com");
  builder_3.SetMetric("Latency", 14);
  builder_3.Record(GetRecorder());

  EXPECT_TRUE(GetRecorder()->HasEntries());

  auto dwa_events = GetRecorder()->TakeDwaEvents();
  EXPECT_FALSE(dwa_events.empty());
  ASSERT_EQ(dwa_events.size(), 1u);

  EXPECT_EQ(dwa_events[0].event_hash(),
            base::HashMetricName("Kangaroo.Jumped"));
  ASSERT_EQ(dwa_events[0].content_metrics().size(), 2);

  EXPECT_THAT(
      RepeatedPtrFieldContentMetricToVector(dwa_events[0].content_metrics()),
      testing::UnorderedElementsAre(
          testing::Pair(::dwa::DeidentifiedWebAnalyticsEvent::ContentMetric::
                            CONTENT_TYPE_URL,
                        base::HashMetricName("adtech.com")),
          testing::Pair(::dwa::DeidentifiedWebAnalyticsEvent::ContentMetric::
                            CONTENT_TYPE_URL,
                        base::HashMetricName("adtech2.com"))));
}

TEST_F(DwaRecorderEnabledTest, ValidateRecorderRecordsForVaryingEvents) {
  ::dwa::DwaEntryBuilder builder_1("Kangaroo.Jumped");
  builder_1.SetContent("https://adtech.com");
  builder_1.SetMetric("Latency", 10);
  builder_1.Record(GetRecorder());

  ::dwa::DwaEntryBuilder builder_2("Kangaroo.Jumped");
  builder_2.SetContent("https://adtech.com");
  builder_2.SetMetric("Latency", 12);
  builder_2.Record(GetRecorder());

  ::dwa::DwaEntryBuilder builder_3("Frog.Leaped");
  builder_3.SetContent("https://adtech.com");
  builder_3.SetMetric("Latency", 14);
  builder_3.Record(GetRecorder());

  EXPECT_TRUE(GetRecorder()->HasEntries());

  auto dwa_events = GetRecorder()->TakeDwaEvents();
  EXPECT_FALSE(dwa_events.empty());
  ASSERT_EQ(dwa_events.size(), 2u);

  EXPECT_THAT(
      GetDwaEventHashesFromDwaEvents(dwa_events),
      testing::UnorderedElementsAre(base::HashMetricName("Kangaroo.Jumped"),
                                    base::HashMetricName("Frog.Leaped")));
}

TEST_F(DwaRecorderEnabledTest, ValidateRecorderRecordsEventsWithFieldTrials) {
  base::FieldTrialList::CreateFieldTrial("test_trial_1", "test_group_2")
      ->Activate();
  base::FieldTrialList::CreateFieldTrial("test_trial_2", "test_group_1")
      ->Activate();
  base::FieldTrialList::CreateFieldTrial("test_trial_3", "test_group_8")
      ->Activate();

  ::dwa::DwaEntryBuilder builder_1("Kangaroo.Jumped");
  builder_1.SetContent("https://adtech.com");
  builder_1.AddToStudiesOfInterest("test_trial_1");
  builder_1.AddToStudiesOfInterest("test_trial_2");
  builder_1.SetMetric("Latency", 10);
  builder_1.Record(GetRecorder());

  auto dwa_events = GetRecorder()->TakeDwaEvents();
  EXPECT_FALSE(dwa_events.empty());
  ASSERT_EQ(dwa_events.size(), 1u);

  EXPECT_THAT(RepeatedPtrFieldFieldTrialsToVector(dwa_events[0].field_trials()),
              testing::UnorderedElementsAre(
                  testing::Pair(base::HashFieldTrialName("test_trial_1"),
                                base::HashFieldTrialName("test_group_2")),
                  testing::Pair(base::HashFieldTrialName("test_trial_2"),
                                base::HashFieldTrialName("test_group_1"))));
}

TEST_F(DwaRecorderDisabledTest, FeatureDisabled) {
  ::dwa::DwaEntryBuilder builder("Kangaroo.Jumped");
  builder.SetContent("https://adtech.com");
  builder.SetMetric("Length", 5);
  builder.Record(GetRecorder());

  EXPECT_FALSE(GetRecorder()->HasEntries());
}

}  // namespace metrics::dwa
