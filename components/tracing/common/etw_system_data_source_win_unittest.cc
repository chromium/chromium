// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifdef UNSAFE_BUFFERS_BUILD
// TODO(crbug.com/40285824): Remove this and convert code to safer constructs.
#pragma allow_unsafe_buffers
#endif

#include "components/tracing/common/etw_system_data_source_win.h"

#include <vector>

#include "base/containers/span.h"
#include "base/logging.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/perfetto/include/perfetto/protozero/scattered_heap_buffer.h"
#include "third_party/perfetto/protos/perfetto/trace/etw/etw.pbzero.h"
#include "third_party/perfetto/protos/perfetto/trace/etw/etw_event.pbzero.h"

namespace tracing {

TEST(EtwSystemDataSource, DecodeCSwitchEvent_Empty) {
  std::vector<uint8_t> data = {};
  protozero::HeapBuffered<perfetto::protos::pbzero::EtwTraceEvent> event;
  EXPECT_FALSE(
      EtwSystemDataSource::DecodeCSwitchEvent({data.data(), 0U}, *event.get()));
}

TEST(EtwSystemDataSource, DecodeCSwitchEvent_TooShort) {
  std::vector<uint8_t> data(0x00, 23);
  protozero::HeapBuffered<perfetto::protos::pbzero::EtwTraceEvent> event;
  EXPECT_FALSE(EtwSystemDataSource::DecodeCSwitchEvent(
      {data.data(), data.size()}, *event.get()));
}

TEST(EtwSystemDataSource, DecodeCSwitchEvent) {
  std::vector<uint8_t> data = {
      0x55, 0x00, 0x00, 0x00,  // new_thread_id
      0x44, 0x00, 0x00, 0x00,  // old_thread_id
      0x01,                    // new_thread_priority
      0x02,                    // old_thread_priority
      0x03,                    // previous_c_state
      0x42,                    // SpareByte
      36,                      // old_thread_wait_reason = WR_RUNDOWN
      1,                       // old_thread_wait_mode = USER_MODE
      7,                       // old_thread_state = DEFERRED_READY
      0x04,                    // old_thread_wait_ideal_processor
      0x05, 0x00, 0x00, 0x00,  // new_thread_wait_time
      0x42, 0x42, 0x42, 0x42,  // Reserved
  };
  protozero::HeapBuffered<perfetto::protos::pbzero::EtwTraceEvent> event;
  ASSERT_TRUE(EtwSystemDataSource::DecodeCSwitchEvent(
      {data.data(), data.size()}, *event.get()));

  auto serialized = event.SerializeAsString();
  perfetto::protos::pbzero::EtwTraceEvent::Decoder decoded_event(serialized);

  ASSERT_TRUE(decoded_event.has_c_switch());
  perfetto::protos::pbzero::CSwitchEtwEvent::Decoder c_switch(
      decoded_event.c_switch());
  EXPECT_EQ(0x55u, c_switch.new_thread_id());
  EXPECT_EQ(0x44u, c_switch.old_thread_id());
  EXPECT_EQ(0x01, c_switch.new_thread_priority());
  EXPECT_EQ(0x02, c_switch.old_thread_priority());
  EXPECT_EQ(0x03u, c_switch.previous_c_state());
  EXPECT_EQ(perfetto::protos::pbzero::CSwitchEtwEvent::WR_RUNDOWN,
            c_switch.old_thread_wait_reason());
  EXPECT_EQ(perfetto::protos::pbzero::CSwitchEtwEvent::USER_MODE,
            c_switch.old_thread_wait_mode());
  EXPECT_EQ(perfetto::protos::pbzero::CSwitchEtwEvent::DEFERRED_READY,
            c_switch.old_thread_state());
  EXPECT_EQ(0x04, c_switch.old_thread_wait_ideal_processor());
  EXPECT_EQ(0x05u, c_switch.new_thread_wait_time());
}

}  // namespace tracing
