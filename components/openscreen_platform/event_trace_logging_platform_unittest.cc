// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/openscreen_platform/event_trace_logging_platform.h"

#include <string>
#include <vector>

#include "base/test/task_environment.h"
#include "base/test/tracing/test_trace_processor.h"
#include "base/test/tracing/trace_test_utils.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/openscreen/src/platform/api/time.h"
#include "third_party/openscreen/src/platform/api/trace_event.h"
#include "third_party/openscreen/src/platform/base/error.h"
#include "third_party/openscreen/src/platform/base/trace_logging_types.h"

namespace openscreen_platform {
namespace {

using ::testing::ElementsAre;

using Row = std::vector<std::string>;

// Open Screen's clock counts whole microseconds.
openscreen::Clock::duration Milliseconds(int ms) {
  return openscreen::Clock::duration(ms * 1000);
}

openscreen::TraceEvent MakeFlowEvent(const char* name,
                                     openscreen::Clock::time_point start_time,
                                     uint64_t flow_id) {
  openscreen::TraceEvent event(openscreen::TraceCategory::kSender, start_time,
                               name, __FILE__, __LINE__);
  event.flow_ids.push_back(flow_id);
  return event;
}

class EventTraceLoggingPlatformTest : public ::testing::Test {
 protected:
  void StartTrace() {
    trace_processor_.StartTrace(kOpenscreenTraceLoggingCategory);
  }

  base::test::TestTraceProcessor::QueryResult StopAndQuery(
      const std::string& query) {
    const absl::Status status = trace_processor_.StopAndParseTrace();
    EXPECT_TRUE(status.ok()) << status.message();
    auto result = trace_processor_.RunQuery(query);
    EXPECT_TRUE(result.has_value()) << result.error();
    return result.value_or(base::test::TestTraceProcessor::QueryResult());
  }

  EventTraceLoggingPlatform& platform() { return platform_; }

 private:
  base::test::TaskEnvironment task_environment_;
  base::test::TracingEnvironment tracing_environment_;
  base::test::TestTraceProcessor trace_processor_;
  EventTraceLoggingPlatform platform_;
};

TEST_F(EventTraceLoggingPlatformTest, LogFlowLinksEventsWithTheSameId) {
  StartTrace();

  constexpr uint64_t kFlowId = 0x4000123400005678;
  const openscreen::Clock::time_point start =
      openscreen::Clock::now() - Milliseconds(10);

  platform().LogFlow(MakeFlowEvent("Frame.Begin", start, kFlowId),
                     openscreen::FlowType::kFlowBegin);
  platform().LogFlow(
      MakeFlowEvent("Frame.Step", start + Milliseconds(2), kFlowId),
      openscreen::FlowType::kFlowStep);
  platform().LogFlow(
      MakeFlowEvent("Frame.End", start + Milliseconds(3), kFlowId),
      openscreen::FlowType::kFlowEnd);

  // The end of the flow terminates it, so a later event with the same ID must
  // not be linked to it.
  platform().LogFlow(
      MakeFlowEvent("Frame.AfterEnd", start + Milliseconds(4), kFlowId),
      openscreen::FlowType::kFlowStep);

  // The deltas show that the events use the times that Open Screen recorded.
  EXPECT_THAT(StopAndQuery(R"(
        SELECT
          slice_out.name AS from_name,
          slice_in.name AS to_name,
          slice_in.ts - slice_out.ts AS delta
        FROM flow
        JOIN slice slice_out ON flow.slice_out = slice_out.id
        JOIN slice slice_in ON flow.slice_in = slice_in.id
        ORDER BY slice_out.ts
      )"),
              ElementsAre(Row{"from_name", "to_name", "delta"},
                          Row{"Frame.Begin", "Frame.Step", "2000000"},
                          Row{"Frame.Step", "Frame.End", "1000000"}));
}

TEST_F(EventTraceLoggingPlatformTest, LogFlowRecordsFlowIdAndArguments) {
  StartTrace();

  openscreen::TraceEvent event =
      MakeFlowEvent("Frame.Enqueued", openscreen::Clock::now(), 1234);
  event.arguments.emplace_back("frame_id", "42");
  event.result = openscreen::Error::Code::kParameterInvalid;
  platform().LogFlow(std::move(event), openscreen::FlowType::kFlowStep);

  EXPECT_THAT(
      StopAndQuery(R"(
        SELECT
          name,
          dur,
          EXTRACT_ARG(arg_set_id, 'debug.flow_id') AS flow_id,
          EXTRACT_ARG(arg_set_id, 'debug.frame_id') AS frame_id,
          EXTRACT_ARG(arg_set_id, 'debug.result') AS result
        FROM slice
      )"),
      ElementsAre(Row{"name", "dur", "flow_id", "frame_id", "result"},
                  Row{"Frame.Enqueued", "0", "1234", "42",
                      openscreen::ToString(
                          openscreen::Error::Code::kParameterInvalid)}));
}

}  // namespace
}  // namespace openscreen_platform
