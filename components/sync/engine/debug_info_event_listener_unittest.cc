// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/sync/engine/debug_info_event_listener.h"

#include "components/sync/protocol/client_debug_info.pb.h"
#include "components/sync/protocol/sync_enums.pb.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace syncer {

using DebugInfoEventListenerTest = testing::Test;

TEST_F(DebugInfoEventListenerTest, VerifyEventsAdded) {
  DebugInfoEventListener debug_info_event_listener;
  debug_info_event_listener.CreateAndAddEvent(
      sync_pb::SyncEnums::INITIALIZATION_COMPLETE);
  ASSERT_EQ(debug_info_event_listener.events_.size(), 1U);
  const sync_pb::DebugEventInfo& debug_info =
      debug_info_event_listener.events_.back();
  ASSERT_TRUE(debug_info.has_singleton_event());
  ASSERT_EQ(debug_info.singleton_event(),
            sync_pb::SyncEnums::INITIALIZATION_COMPLETE);
}

TEST_F(DebugInfoEventListenerTest, VerifyQueueSize) {
  DebugInfoEventListener debug_info_event_listener;
  for (size_t i = 0; i < 2 * DebugInfoEventListener::kMaxEvents; ++i) {
    debug_info_event_listener.CreateAndAddEvent(
        sync_pb::SyncEnums::INITIALIZATION_COMPLETE);
  }
  sync_pb::DebugInfo debug_info = debug_info_event_listener.GetDebugInfo();
  debug_info_event_listener.ClearDebugInfo();
  ASSERT_TRUE(debug_info.events_dropped());
  ASSERT_EQ(static_cast<int>(DebugInfoEventListener::kMaxEvents),
            debug_info.events_size());
}

TEST_F(DebugInfoEventListenerTest, VerifyGetEvents) {
  DebugInfoEventListener debug_info_event_listener;
  debug_info_event_listener.CreateAndAddEvent(
      sync_pb::SyncEnums::INITIALIZATION_COMPLETE);
  ASSERT_EQ(debug_info_event_listener.events_.size(), 1U);
  sync_pb::DebugInfo debug_info = debug_info_event_listener.GetDebugInfo();
  ASSERT_EQ(debug_info_event_listener.events_.size(), 1U);
  ASSERT_EQ(debug_info.events_size(), 1);
  ASSERT_TRUE(debug_info.events(0).has_singleton_event());
  ASSERT_EQ(debug_info.events(0).singleton_event(),
            sync_pb::SyncEnums::INITIALIZATION_COMPLETE);
}

TEST_F(DebugInfoEventListenerTest, VerifyClearEvents) {
  DebugInfoEventListener debug_info_event_listener;
  debug_info_event_listener.CreateAndAddEvent(
      sync_pb::SyncEnums::INITIALIZATION_COMPLETE);
  ASSERT_EQ(debug_info_event_listener.events_.size(), 1U);
  debug_info_event_listener.ClearDebugInfo();
  ASSERT_EQ(debug_info_event_listener.events_.size(), 0U);
}

}  // namespace syncer
