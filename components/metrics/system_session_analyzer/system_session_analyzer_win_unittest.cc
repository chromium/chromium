// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/metrics/system_session_analyzer/system_session_analyzer_win.h"

#include <algorithm>
#include <utility>
#include <vector>

#include "base/logging.h"
#include "base/time/time.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace metrics {

namespace {

const uint16_t kIdSessionStart = 6005U;
const uint16_t kIdSessionEnd = 6006U;
const uint16_t kIdSessionEndUnclean = 6008U;

}  // namespace

// Ensure the fetcher retrieves events.
// Due to https://crbug.com/1053451 which is caused by a Windows "bug" tracked
// by https://crbug.com/1053451 this cannot be made reliable. Running
// browser_tests can and does generate so much event logging (DCOM events) that
// none of the looked for events remain.
TEST(SystemSessionAnalyzerTest, DISABLED_FetchEvents) {
  SystemSessionAnalyzer analyzer(0);
  std::vector<SystemSessionAnalyzer::EventInfo> events;
  ASSERT_TRUE(analyzer.FetchEvents(1U, &events));
  EXPECT_EQ(1U, events.size());
}

// Ensure the fetcher's retrieved events conform to our expectations.
// Note: this test fails if the host system doesn't have at least 1 prior
// session.
TEST(SystemSessionAnalyzerTest, ValidateEvents) {
  SystemSessionAnalyzer analyzer(1U);
  auto is_session_unclean = analyzer.IsSessionUnclean(base::Time::Now());
  // If the system event log rate is high enough then there may not be enough
  // events to make a clean/unclean determination. Check for this situation and
  // don't treat it as a failure. See https://crbug.com/968440 and
  // https://crbug.com/1053451 for details.
  if (is_session_unclean == SystemSessionAnalyzer::INSUFFICIENT_DATA) {
    // This warning can be ignored, but it does mean that our clean/unclean
    // check did not give an answer.
    LOG(WARNING) << "Insufficient events found in ValidateEvents.";
  } else {
    EXPECT_EQ(SystemSessionAnalyzer::CLEAN, is_session_unclean)
        << "Extended error code is: "
        << static_cast<int>(analyzer.GetExtendedFailureStatus());
  }
}

// Stubs FetchEvents.
class StubSystemSessionAnalyzer : public SystemSessionAnalyzer {
 public:
  StubSystemSessionAnalyzer(uint32_t max_session_cnt)
      : SystemSessionAnalyzer(max_session_cnt) {}

  bool FetchEvents(size_t requested_events,
                   std::vector<EventInfo>* event_infos) override {
    DCHECK(event_infos);
    size_t num_to_copy = std::min(requested_events, events_.size());
    if (num_to_copy) {
      event_infos->clear();
      event_infos->insert(event_infos->begin(), events_.begin(),
                          events_.begin() + num_to_copy);
      events_.erase(events_.begin(), events_.begin() + num_to_copy);
    }

    return true;
  }

  void AddEvent(const EventInfo& info) { events_.push_back(info); }

 private:
  std::vector<EventInfo> events_;
};

TEST(SystemSessionAnalyzerTest, StandardCase) {
  StubSystemSessionAnalyzer analyzer(2U);

  base::Time time = base::Time::Now();
  analyzer.AddEvent({kIdSessionStart, time});
  analyzer.AddEvent({kIdSessionEndUnclean, time - base::Seconds(10)});
  analyzer.AddEvent({kIdSessionStart, time - base::Seconds(20)});
  analyzer.AddEvent({kIdSessionEnd, time - base::Seconds(22)});
  analyzer.AddEvent({kIdSessionStart, time - base::Seconds(28)});

  EXPECT_EQ(SystemSessionAnalyzer::OUTSIDE_RANGE,
            analyzer.IsSessionUnclean(time - base::Seconds(30)));
  EXPECT_EQ(SystemSessionAnalyzer::CLEAN,
            analyzer.IsSessionUnclean(time - base::Seconds(25)));
  EXPECT_EQ(SystemSessionAnalyzer::UNCLEAN,
            analyzer.IsSessionUnclean(time - base::Seconds(20)));
  EXPECT_EQ(SystemSessionAnalyzer::UNCLEAN,
            analyzer.IsSessionUnclean(time - base::Seconds(15)));
  EXPECT_EQ(SystemSessionAnalyzer::UNCLEAN,
            analyzer.IsSessionUnclean(time - base::Seconds(10)));
  EXPECT_EQ(SystemSessionAnalyzer::CLEAN,
            analyzer.IsSessionUnclean(time - base::Seconds(5)));
  EXPECT_EQ(SystemSessionAnalyzer::CLEAN,
            analyzer.IsSessionUnclean(time + base::Seconds(5)));
}

TEST(SystemSessionAnalyzerTest, NoEvent) {
  StubSystemSessionAnalyzer analyzer(0U);
  EXPECT_EQ(SystemSessionAnalyzer::INSUFFICIENT_DATA,
            analyzer.IsSessionUnclean(base::Time::Now()));
}

TEST(SystemSessionAnalyzerTest, TimeInversion) {
  StubSystemSessionAnalyzer analyzer(1U);

  base::Time time = base::Time::Now();
  analyzer.AddEvent({kIdSessionStart, time});
  analyzer.AddEvent({kIdSessionEnd, time + base::Seconds(1)});
  analyzer.AddEvent({kIdSessionStart, time - base::Seconds(1)});

  EXPECT_EQ(SystemSessionAnalyzer::INITIALIZE_FAILED,
            analyzer.IsSessionUnclean(base::Time::Now()));
}

TEST(SystemSessionAnalyzerTest, IdInversion) {
  StubSystemSessionAnalyzer analyzer(1U);

  base::Time time = base::Time::Now();
  analyzer.AddEvent({kIdSessionStart, time});
  analyzer.AddEvent({kIdSessionStart, time - base::Seconds(1)});
  analyzer.AddEvent({kIdSessionEnd, time - base::Seconds(2)});

  EXPECT_EQ(SystemSessionAnalyzer::INITIALIZE_FAILED,
            analyzer.IsSessionUnclean(base::Time::Now()));
}

}  // namespace metrics
