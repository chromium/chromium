// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/browser_actuator/internal/transport_session_registry_impl.h"

#include <memory>

#include "base/memory/weak_ptr.h"
#include "base/test/metrics/histogram_tester.h"
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

// Verifies that `Browser.Actuator.SessionRegistry.ExistingSessionsCount`
// records the number of currently active transport sessions immediately prior
// to attempting to create a new session.
TEST(TransportSessionRegistryImplTest,
     ExistingSessionsCountHistogram_RecordedOnSessionCreation) {
  base::HistogramTester histogram_tester;
  MockTransportChannel channel;
  TransportSessionRegistryImpl registry(channel.GetWeakPtr(),
                                        /*max_concurrent_sessions=*/2);

  histogram_tester.ExpectTotalCount(
      "Browser.Actuator.SessionRegistry.ExistingSessionsCount", 0);

  // 1st session creation: registry has 0 existing sessions.
  registry.GetOrCreateSession("session_1");
  histogram_tester.ExpectUniqueSample(
      "Browser.Actuator.SessionRegistry.ExistingSessionsCount", /*sample=*/0,
      /*expected_bucket_count=*/1);

  // 2nd session creation: registry has 1 existing session.
  registry.GetOrCreateSession("session_2");
  histogram_tester.ExpectBucketCount(
      "Browser.Actuator.SessionRegistry.ExistingSessionsCount", /*sample=*/1,
      /*expected_bucket_count=*/1);
  histogram_tester.ExpectTotalCount(
      "Browser.Actuator.SessionRegistry.ExistingSessionsCount", 2);
}

// Verifies that attempting to create a new session when the registry has
// reached `max_concurrent_sessions` records `ExistingSessionsCount` with the
// count at the limit, even though the creation is rejected.
TEST(TransportSessionRegistryImplTest,
     ExistingSessionsCountHistogram_RecordedWhenLimitExceeded) {
  base::HistogramTester histogram_tester;
  MockTransportChannel channel;
  TransportSessionRegistryImpl registry(channel.GetWeakPtr(),
                                        /*max_concurrent_sessions=*/2);

  // Fill up registry to the max capacity (2 sessions).
  registry.GetOrCreateSession("session_1");
  registry.GetOrCreateSession("session_2");

  // Attempting to create a 3rd session exceeds the limit and is rejected.
  // The histogram should still record that 2 sessions currently exist.
  EXPECT_EQ(registry.GetOrCreateSession("session_3"), nullptr);
  histogram_tester.ExpectBucketCount(
      "Browser.Actuator.SessionRegistry.ExistingSessionsCount", /*sample=*/2,
      /*expected_bucket_count=*/1);
  histogram_tester.ExpectTotalCount(
      "Browser.Actuator.SessionRegistry.ExistingSessionsCount", 3);
}

// Verifies that `Browser.Actuator.SessionRegistry.SessionLimitReached` records
// `false` when a new session is created within the allowed limit, and `true`
// when the session request is rejected because the maximum limit is reached.
TEST(TransportSessionRegistryImplTest,
     SessionLimitReachedHistogram_RecordsSuccessAndRejection) {
  base::HistogramTester histogram_tester;
  MockTransportChannel channel;
  TransportSessionRegistryImpl registry(channel.GetWeakPtr(),
                                        /*max_concurrent_sessions=*/2);

  histogram_tester.ExpectTotalCount(
      "Browser.Actuator.SessionRegistry.SessionLimitReached", 0);

  // 1st and 2nd session creations succeed (limit not reached -> false).
  registry.GetOrCreateSession("session_1");
  histogram_tester.ExpectBucketCount(
      "Browser.Actuator.SessionRegistry.SessionLimitReached", /*sample=*/false,
      /*expected_bucket_count=*/1);

  registry.GetOrCreateSession("session_2");
  histogram_tester.ExpectBucketCount(
      "Browser.Actuator.SessionRegistry.SessionLimitReached", /*sample=*/false,
      /*expected_bucket_count=*/2);

  // 3rd session creation is rejected (limit reached -> true).
  EXPECT_EQ(registry.GetOrCreateSession("session_3"), nullptr);
  histogram_tester.ExpectBucketCount(
      "Browser.Actuator.SessionRegistry.SessionLimitReached", /*sample=*/true,
      /*expected_bucket_count=*/1);
  histogram_tester.ExpectTotalCount(
      "Browser.Actuator.SessionRegistry.SessionLimitReached", 3);
}

// Verifies that looking up or retrieving an existing session from the registry
// does not record any new samples to either histogram, since no new session
// creation was attempted.
TEST(TransportSessionRegistryImplTest,
     Histograms_NotRecordedWhenRetrievingExistingSession) {
  base::HistogramTester histogram_tester;
  MockTransportChannel channel;
  TransportSessionRegistryImpl registry(channel.GetWeakPtr(),
                                        /*max_concurrent_sessions=*/2);

  // Create initial session.
  registry.GetOrCreateSession("session_1");
  histogram_tester.ExpectTotalCount(
      "Browser.Actuator.SessionRegistry.ExistingSessionsCount", 1);
  histogram_tester.ExpectTotalCount(
      "Browser.Actuator.SessionRegistry.SessionLimitReached", 1);

  // Requesting the existing session again should retrieve it without recording
  // new histogram samples.
  TransportSessionImpl* session1 = registry.GetOrCreateSession("session_1");
  ASSERT_NE(session1, nullptr);
  EXPECT_EQ(session1->GetSessionId(), "session_1");

  histogram_tester.ExpectTotalCount(
      "Browser.Actuator.SessionRegistry.ExistingSessionsCount", 1);
  histogram_tester.ExpectTotalCount(
      "Browser.Actuator.SessionRegistry.SessionLimitReached", 1);
}

// Verifies that after an active session is destroyed to free capacity, creating
// a subsequent new session records the updated session count and `false` for
// limit reached.
TEST(TransportSessionRegistryImplTest,
     Histograms_RecordedAfterDestroyingSessionFreesCapacity) {
  base::HistogramTester histogram_tester;
  MockTransportChannel channel;
  TransportSessionRegistryImpl registry(channel.GetWeakPtr(),
                                        /*max_concurrent_sessions=*/2);

  // Fill registry to capacity.
  registry.GetOrCreateSession("session_1");
  registry.GetOrCreateSession("session_2");

  // Destroying session_1 reduces active sessions from 2 to 1.
  registry.DestroySession("session_1");

  // Creating session_3 should now succeed:
  // - ExistingSessionsCount records 1 (1 active session remained).
  // - SessionLimitReached records false.
  TransportSessionImpl* session3 = registry.GetOrCreateSession("session_3");
  ASSERT_NE(session3, nullptr);
  EXPECT_EQ(session3->GetSessionId(), "session_3");

  histogram_tester.ExpectBucketCount(
      "Browser.Actuator.SessionRegistry.ExistingSessionsCount", /*sample=*/1,
      /*expected_bucket_count=*/2);
  histogram_tester.ExpectBucketCount(
      "Browser.Actuator.SessionRegistry.SessionLimitReached", /*sample=*/false,
      /*expected_bucket_count=*/3);
}

namespace {

class MockSessionRegistryObserver : public TransportSessionRegistry::Observer {
 public:
  MOCK_METHOD(void,
              OnSessionRegistered,
              (TransportSession * session),
              (override));
};

}  // namespace

TEST(TransportSessionRegistryImplTest, NotifiesObserversOnSessionRegistered) {
  MockTransportChannel channel;
  TransportSessionRegistryImpl registry(channel.GetWeakPtr());
  MockSessionRegistryObserver observer;
  registry.AddObserver(&observer);

  TransportSession* created_session = nullptr;
  EXPECT_CALL(observer, OnSessionRegistered)
      .WillOnce(testing::SaveArg<0>(&created_session));

  TransportSessionImpl* session1 = registry.GetOrCreateSession("session_1");

  EXPECT_EQ(created_session, session1);
}

TEST(TransportSessionRegistryImplTest,
     DoesNotNotifyObserversWhenGettingExistingSession) {
  MockTransportChannel channel;
  TransportSessionRegistryImpl registry(channel.GetWeakPtr());
  MockSessionRegistryObserver observer;
  registry.AddObserver(&observer);
  TransportSessionImpl* session1 = registry.GetOrCreateSession("session_1");

  EXPECT_CALL(observer, OnSessionRegistered).Times(0);
  TransportSessionImpl* retrieved_session =
      registry.GetOrCreateSession("session_1");

  EXPECT_EQ(retrieved_session, session1);
}

TEST(TransportSessionRegistryImplTest, DoesNotNotifyRemovedObservers) {
  MockTransportChannel channel;
  TransportSessionRegistryImpl registry(channel.GetWeakPtr());
  MockSessionRegistryObserver observer;
  registry.AddObserver(&observer);
  registry.RemoveObserver(&observer);

  EXPECT_CALL(observer, OnSessionRegistered).Times(0);
  registry.GetOrCreateSession("session_1");
}

}  // namespace
}  // namespace browser_actuator
