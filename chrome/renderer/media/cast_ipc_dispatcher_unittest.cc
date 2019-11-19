// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/renderer/media/cast_ipc_dispatcher.h"

#include "base/test/simple_test_tick_clock.h"
#include "base/test/task_environment.h"
#include "base/threading/thread_task_runner_handle.h"
#include "chrome/common/cast_messages.h"
#include "ipc/ipc_message_macros.h"
#include "media/cast/logging/logging_defines.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace {

class CastIPCDispatcherTest : public testing::Test {
 public:
  CastIPCDispatcherTest() {
    dispatcher_ = new CastIPCDispatcher(base::ThreadTaskRunnerHandle::Get());
  }

 protected:
  void FakeSend(const IPC::Message& message) {
    EXPECT_TRUE(dispatcher_->OnMessageReceived(message));
  }

  scoped_refptr<CastIPCDispatcher> dispatcher_;
  base::test::SingleThreadTaskEnvironment task_environment_{
      base::test::SingleThreadTaskEnvironment::MainThreadType::IO};
};

TEST_F(CastIPCDispatcherTest, RawEvents) {
  const int kChannelId = 17;

  media::cast::PacketEvent packet_event;
  packet_event.rtp_timestamp =
      media::cast::RtpTimeTicks().Expand(UINT32_C(100));
  packet_event.max_packet_id = 10;
  packet_event.packet_id = 5;
  packet_event.size = 512;
  packet_event.timestamp = base::SimpleTestTickClock().NowTicks();
  packet_event.type = media::cast::PACKET_SENT_TO_NETWORK;
  packet_event.media_type = media::cast::VIDEO_EVENT;
  std::vector<media::cast::PacketEvent> packet_events;
  packet_events.push_back(packet_event);

  media::cast::FrameEvent frame_event;
  frame_event.rtp_timestamp = media::cast::RtpTimeTicks().Expand(UINT32_C(100));
  frame_event.frame_id = media::cast::FrameId::first() + 5;
  frame_event.size = 512;
  frame_event.timestamp = base::SimpleTestTickClock().NowTicks();
  frame_event.media_type = media::cast::VIDEO_EVENT;
  std::vector<media::cast::FrameEvent> frame_events;
  frame_events.push_back(frame_event);

  packet_events.push_back(packet_event);
  CastMsg_RawEvents raw_events_msg(kChannelId, packet_events,
                                   frame_events);

  FakeSend(raw_events_msg);
}

}  // namespace
