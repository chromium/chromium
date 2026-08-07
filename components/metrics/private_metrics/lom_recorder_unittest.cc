// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/metrics/private_metrics/lom_recorder.h"

#include <limits>
#include <utility>
#include <vector>

#include "base/memory/raw_ptr.h"
#include "base/metrics/metrics_hashes.h"
#include "base/test/scoped_feature_list.h"
#include "components/metrics/private_metrics/private_metrics_features.h"
#include "components/metrics/private_metrics/puma_histogram_functions.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/metrics_proto/private_metrics/private_user_metrics.pb.h"

namespace metrics::private_metrics {
namespace {

struct ExpectedBucket {
  int64_t min;
  int64_t max;
  int64_t count;
};

const ::private_metrics::ProfileKeyedHistogramEvent* FindProfileEvent(
    const std::vector<::private_metrics::ProfileKeyedHistogramEvent>& events,
    uint64_t profile_id) {
  for (const auto& event : events) {
    if (event.profile_id() == profile_id) {
      return &event;
    }
  }
  return nullptr;
}

const metrics::HistogramEventProto* FindEvent(
    const std::vector<::private_metrics::ProfileKeyedHistogramEvent>& events,
    uint64_t profile_id,
    uint64_t name_hash) {
  const auto* profile_event = FindProfileEvent(events, profile_id);
  if (!profile_event) {
    return nullptr;
  }
  for (const auto& event : profile_event->histogram_events()) {
    if (event.name_hash() == name_hash) {
      return &event;
    }
  }
  return nullptr;
}

const metrics::HistogramEventProto::Bucket* FindBucket(
    const metrics::HistogramEventProto& event,
    int64_t min,
    int64_t max) {
  for (int i = 0; i < event.bucket_size(); ++i) {
    const auto& bucket = event.bucket(i);
    if (bucket.min() == min && bucket.max() == max) {
      return &bucket;
    }
  }
  return nullptr;
}

void ExpectEvent(
    const std::vector<::private_metrics::ProfileKeyedHistogramEvent>& events,
    uint64_t profile_id,
    uint64_t name_hash,
    int64_t expected_sum,
    const std::vector<ExpectedBucket>& expected_buckets) {
  const auto* event = FindEvent(events, profile_id, name_hash);
  ASSERT_NE(event, nullptr) << "Event not found for profile_id=" << profile_id
                            << ", name_hash=" << name_hash;
  EXPECT_EQ(event->sum(), expected_sum);
  ASSERT_EQ(event->bucket_size(), static_cast<int>(expected_buckets.size()));
  for (const auto& eb : expected_buckets) {
    const auto* bucket = FindBucket(*event, eb.min, eb.max);
    ASSERT_NE(bucket, nullptr)
        << "Bucket [" << eb.min << ", " << eb.max << ") not found";
    EXPECT_EQ(bucket->count(), eb.count);
  }
}

}  // namespace

class LomRecorderTestBase : public testing::Test {
 public:
  explicit LomRecorderTestBase(bool enable_feature) {
    if (enable_feature) {
      scoped_feature_list_.InitAndEnableFeature(kLomFeature);
    } else {
      scoped_feature_list_.InitAndDisableFeature(kLomFeature);
    }
    recorder_ = LomRecorder::Get();
    recorder_->TakeHistogramEvents();
  }
  ~LomRecorderTestBase() override { recorder_->TakeHistogramEvents(); }

  LomRecorder* GetRecorder() { return recorder_; }

 private:
  raw_ptr<LomRecorder> recorder_;
  base::test::ScopedFeatureList scoped_feature_list_;
};

class LomRecorderEnabledTest : public LomRecorderTestBase {
 public:
  LomRecorderEnabledTest() : LomRecorderTestBase(/*enable_feature=*/true) {}
};

class LomRecorderDisabledTest : public LomRecorderTestBase {
 public:
  LomRecorderDisabledTest() : LomRecorderTestBase(/*enable_feature=*/false) {}
};

TEST_F(LomRecorderEnabledTest, ValidateRecorderRecords) {
  constexpr uint64_t kProfileId1 = 12345u;

  GetRecorder()->RecordBoolean(PumaType::kRc, "Test.Boolean", true);
  GetRecorder()->RecordBoolean(PumaType::kRc, "Test.Boolean", false);
  GetRecorder()->RecordExactLinear(PumaType::kRc, "Test.Linear", 2, 10);
  GetRecorder()->RecordExactLinear(PumaType::kRc, "Test.Linear", 3, 10);
  GetRecorder()->RecordExactLinear(PumaType::kRc, "Test.Linear", 3, 10);
  GetRecorder()->RecordExactLinear(PumaType::kRc, "Test.Linear", 4, 10);
  GetRecorder()->RecordExactLinear(PumaType::kRc, "Test.Linear2", 7, 10000);
  GetRecorder()->RecordExactLinear(PumaType::kRc, "Test.Linear2", 9, 10000);
  GetRecorder()->RecordExactLinear(PumaType::kRc, "Test.Linear2", 7001, 10000);
  GetRecorder()->RecordBoolean(PumaType::kRc, "Test.ProfileBoolean", true,
                               kProfileId1);

  // Global helper function should also route to LomRecorder when enabled.
  PumaHistogramBoolean(PumaType::kRc, "Test.GlobalBoolean", true);

  // Non-Rc type should be ignored and discarded.
  GetRecorder()->RecordBoolean(static_cast<PumaType>(999), "Test.NonRc", true);

  auto events = GetRecorder()->TakeHistogramEvents();
  ASSERT_EQ(events.size(), 2u);

  ExpectEvent(events, /*profile_id=*/0u, base::HashMetricName("Test.Boolean"),
              /*expected_sum=*/1,
              {{std::numeric_limits<int64_t>::min(), 1, 1}, {1, 2, 1}});
  ExpectEvent(events, /*profile_id=*/0u, base::HashMetricName("Test.Linear"),
              /*expected_sum=*/12, {{2, 3, 1}, {3, 4, 2}, {4, 5, 1}});
  ExpectEvent(events, /*profile_id=*/0u, base::HashMetricName("Test.Linear2"),
              /*expected_sum=*/7017, {{7, 8, 1}, {9, 10, 1}, {7001, 7002, 1}});
  ExpectEvent(events, kProfileId1, base::HashMetricName("Test.ProfileBoolean"),
              /*expected_sum=*/1, {{1, 2, 1}});
  ExpectEvent(events, /*profile_id=*/0u,
              base::HashMetricName("Test.GlobalBoolean"), /*expected_sum=*/1,
              {{1, 2, 1}});

  // Taking again should be empty.
  EXPECT_TRUE(GetRecorder()->TakeHistogramEvents().empty());
}

TEST_F(LomRecorderEnabledTest, ValidateProfileId) {
  constexpr uint64_t kProfileIdA = 11111u;
  constexpr uint64_t kProfileIdB = 22222u;

  // Record the same metric name under three different profile configurations.
  GetRecorder()->RecordBoolean(PumaType::kRc, "Test.MultiProfile", true);
  GetRecorder()->RecordBoolean(PumaType::kRc, "Test.MultiProfile", true);
  GetRecorder()->RecordBoolean(PumaType::kRc, "Test.MultiProfile", false,
                               kProfileIdA);
  GetRecorder()->RecordBoolean(PumaType::kRc, "Test.MultiProfile", true,
                               kProfileIdB);
  GetRecorder()->RecordBoolean(PumaType::kRc, "Test.MultiProfile", true,
                               kProfileIdB);
  GetRecorder()->RecordBoolean(PumaType::kRc, "Test.MultiProfile", false,
                               kProfileIdB);

  auto events = GetRecorder()->TakeHistogramEvents();
  ASSERT_EQ(events.size(), 3u);

  uint64_t metric_hash = base::HashMetricName("Test.MultiProfile");

  ExpectEvent(events, /*profile_id=*/0u, metric_hash, /*expected_sum=*/2,
              {{1, 2, 2}});
  ExpectEvent(events, kProfileIdA, metric_hash,
              /*expected_sum=*/0,
              {{std::numeric_limits<int64_t>::min(), 1, 1}});
  ExpectEvent(events, kProfileIdB, metric_hash,
              /*expected_sum=*/2,
              {{std::numeric_limits<int64_t>::min(), 1, 1}, {1, 2, 2}});
}

TEST_F(LomRecorderDisabledTest, FeatureDisabled) {
  // When kLomFeature is disabled, global helper functions should not record to
  // LomRecorder.
  PumaHistogramBoolean(PumaType::kRc, "Test.GlobalBoolean", true);
  EXPECT_TRUE(GetRecorder()->TakeHistogramEvents().empty());
}

}  // namespace metrics::private_metrics
