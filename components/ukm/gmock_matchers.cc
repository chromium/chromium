// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/ukm/gmock_matchers.h"

#include <ostream>
#include <string>

#include "components/ukm/test_ukm_recorder.h"
#include "services/metrics/public/mojom/ukm_interface.mojom.h"

namespace ukm::testing {
namespace {
class UkmMetricMatcher {
 public:
  using is_gtest_matcher = void;

  explicit UkmMetricMatcher(std::string_view metric_name)
      : metric_name_(metric_name) {}

  bool MatchAndExplain(const mojom::UkmEntry* entry, std::ostream* os) const {
    CHECK(entry);
    return ukm::TestUkmRecorder::EntryHasMetric(entry, metric_name_);
  }

  void DescribeTo(std::ostream* os) const {
    *os << "has the UKM metric " << metric_name_;
  }

  void DescribeNegationTo(std::ostream* os) const {
    *os << "doesn't have the UKM metric " << metric_name_;
  }

 private:
  std::string metric_name_;
};

class UkmMetricValueMatcher {
 public:
  using is_gtest_matcher = void;

  UkmMetricValueMatcher(std::string_view metric_name, int64_t value)
      : metric_name_(metric_name), value_(value) {}

  bool MatchAndExplain(const mojom::UkmEntry* entry, std::ostream* os) const {
    CHECK(entry);
    auto* metric_value =
        ukm::TestUkmRecorder::GetEntryMetric(entry, metric_name_);
    if (!metric_value) {
      if (os) {
        *os << "metric " << metric_name_ << " not found";
      }
      return false;
    }
    if (value_ != *metric_value) {
      if (os) {
        *os << "metric " << metric_name_ << " with incorrect value"
            << " - got: " << *metric_value << " - expected: " << value_;
      }
      return false;
    }
    return true;
  }

  void DescribeTo(std::ostream* os) const {
    *os << "has the UKM metric " << metric_name_ << " with the specific value "
        << value_;
  }

  void DescribeNegationTo(std::ostream* os) const {
    *os << "doesn't have the UKM metric " << metric_name_
        << " with the specific value " << value_;
  }

 private:
  std::string metric_name_;
  int64_t value_;
};
}  // namespace

::testing::Matcher<const mojom::UkmEntry*> HasMetric(
    std::string_view metric_name) {
  return UkmMetricMatcher(metric_name);
}

::testing::Matcher<const mojom::UkmEntry*> HasMetricWithValue(
    std::string_view metric_name,
    int64_t value) {
  return UkmMetricValueMatcher(metric_name, value);
}

}  // namespace ukm::testing
