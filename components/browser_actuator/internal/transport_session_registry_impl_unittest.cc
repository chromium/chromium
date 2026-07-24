// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/browser_actuator/internal/transport_session_registry_impl.h"

#include <memory>

#include "base/memory/weak_ptr.h"
#include "components/browser_actuator/internal/transport_session_impl.h"
#include "components/browser_actuator/test_support/mock_transport_channel.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace browser_actuator {
namespace {

TEST(TransportSessionRegistryImplTest, GetSession) {
  MockTransportChannel channel;
  TransportSessionRegistryImpl registry(channel.GetWeakPtr());

  EXPECT_EQ(registry.GetSession("session_1"), nullptr);

  TransportSessionImpl* created_session =
      registry.GetOrCreateSession("session_1");
  EXPECT_EQ(registry.GetSession("session_1"), created_session);
}

TEST(TransportSessionRegistryImplTest, GetAllSessionImpls) {
  MockTransportChannel channel;
  TransportSessionRegistryImpl registry(channel.GetWeakPtr());

  EXPECT_TRUE(registry.GetAllSessionImpls().empty());

  TransportSessionImpl* session1 = registry.GetOrCreateSession("session_1");
  TransportSessionImpl* session2 = registry.GetOrCreateSession("session_2");

  std::vector<TransportSessionImpl*> active_sessions =
      registry.GetAllSessionImpls();
  EXPECT_EQ(active_sessions.size(), 2u);
  EXPECT_THAT(active_sessions,
              testing::UnorderedElementsAre(session1, session2));

  registry.DestroySession("session_1");
  active_sessions = registry.GetAllSessionImpls();
  EXPECT_EQ(active_sessions.size(), 1u);
  EXPECT_EQ(active_sessions[0], session2);

  registry.Clear();
  EXPECT_TRUE(registry.GetAllSessionImpls().empty());
}

TEST(TransportSessionRegistryImplTest, GetSessionImpl) {
  MockTransportChannel channel;
  TransportSessionRegistryImpl registry(channel.GetWeakPtr());

  EXPECT_EQ(registry.GetSessionImpl("session_1"), nullptr);

  TransportSessionImpl* created_session =
      registry.GetOrCreateSession("session_1");
  EXPECT_EQ(registry.GetSessionImpl("session_1"), created_session);
}

TEST(TransportSessionRegistryImplTest, GetOrCreateSession) {
  MockTransportChannel channel;
  TransportSessionRegistryImpl registry(channel.GetWeakPtr());

  TransportSessionImpl* session1 = registry.GetOrCreateSession("session_1");
  ASSERT_NE(session1, nullptr);
  EXPECT_EQ(session1->GetSessionId(), "session_1");

  TransportSessionImpl* session1_again =
      registry.GetOrCreateSession("session_1");
  EXPECT_EQ(session1, session1_again);

  TransportSessionImpl* session2 = registry.GetOrCreateSession("session_2");
  ASSERT_NE(session2, nullptr);
  EXPECT_NE(session1, session2);
}

TEST(TransportSessionRegistryImplTest, DestroySession) {
  MockTransportChannel channel;
  TransportSessionRegistryImpl registry(channel.GetWeakPtr());

  TransportSessionImpl* session = registry.GetOrCreateSession("session_1");
  ASSERT_NE(session, nullptr);

  registry.DestroySession("session_1");
  EXPECT_EQ(registry.GetSession("session_1"), nullptr);
}

TEST(TransportSessionRegistryImplTest, Clear) {
  MockTransportChannel channel;
  TransportSessionRegistryImpl registry(channel.GetWeakPtr());

  registry.GetOrCreateSession("session_1");
  registry.GetOrCreateSession("session_2");

  registry.Clear();
  EXPECT_EQ(registry.GetSession("session_1"), nullptr);
  EXPECT_EQ(registry.GetSession("session_2"), nullptr);
}

TEST(TransportSessionRegistryImplTest, GetWeakPtr) {
  MockTransportChannel channel;
  auto registry =
      std::make_unique<TransportSessionRegistryImpl>(channel.GetWeakPtr());
  base::WeakPtr<TransportSessionRegistryImpl> weak_registry =
      registry->GetWeakPtr();

  EXPECT_NE(weak_registry.get(), nullptr);
  registry.reset();
  EXPECT_EQ(weak_registry.get(), nullptr);
}

TEST(TransportSessionRegistryImplTest, EnforcesMaxConcurrentSessions) {
  MockTransportChannel channel;
  TransportSessionRegistryImpl registry(channel.GetWeakPtr(),
                                        /*max_concurrent_sessions=*/2);
  EXPECT_EQ(registry.max_concurrent_sessions(), 2u);

  TransportSessionImpl* session1 = registry.GetOrCreateSession("session_1");
  ASSERT_NE(session1, nullptr);
  TransportSessionImpl* session2 = registry.GetOrCreateSession("session_2");
  ASSERT_NE(session2, nullptr);

  // 3rd session exceeds the limit of 2 and should be rejected.
  TransportSessionImpl* session3 = registry.GetOrCreateSession("session_3");
  EXPECT_EQ(session3, nullptr);

  // Existing sessions can still be retrieved without error.
  EXPECT_EQ(registry.GetOrCreateSession("session_1"), session1);

  // Destroying a session frees up capacity.
  registry.DestroySession("session_1");
  TransportSessionImpl* session3_retry =
      registry.GetOrCreateSession("session_3");
  ASSERT_NE(session3_retry, nullptr);
  EXPECT_EQ(session3_retry->GetSessionId(), "session_3");
}

}  // namespace
}  // namespace browser_actuator
