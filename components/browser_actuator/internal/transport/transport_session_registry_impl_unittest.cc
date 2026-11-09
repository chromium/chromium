// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/browser_actuator/internal/transport/transport_session_registry_impl.h"

#include <memory>
#include <string>

#include "base/memory/weak_ptr.h"
#include "base/test/metrics/histogram_tester.h"
#include "base/test/scoped_feature_list.h"
#include "base/test/task_environment.h"
#include "base/time/time.h"
#include "components/browser_actuator/internal/transport/features.h"
#include "components/browser_actuator/internal/transport/transport_handler_factory_registry_impl.h"
#include "components/browser_actuator/internal/transport/transport_session_impl.h"
#include "components/browser_actuator/public/transport/common.h"
#include "components/browser_actuator/test_support/transport/mock_transport_channel.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace browser_actuator::transport {
namespace {

TEST(TransportSessionRegistryImplTest, GetSession) {
  MockTransportChannel channel;
  TransportSessionRegistryImpl registry(channel.GetWeakPtr());

  EXPECT_EQ(registry.GetSession("session_1"), nullptr);

  TransportSessionImpl* created_session =
      registry.CreateSession("session_1", InitIngressSource::kUnspecified);
  EXPECT_EQ(registry.GetSession("session_1"), created_session);
}

TEST(TransportSessionRegistryImplTest, GetAllSessionImpls) {
  MockTransportChannel channel;
  TransportSessionRegistryImpl registry(channel.GetWeakPtr());

  EXPECT_TRUE(registry.GetAllSessionImpls().empty());

  TransportSessionImpl* session1 =
      registry.CreateSession("session_1", InitIngressSource::kUnspecified);
  TransportSessionImpl* session2 =
      registry.CreateSession("session_2", InitIngressSource::kUnspecified);

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
      registry.CreateSession("session_1", InitIngressSource::kUnspecified);
  EXPECT_EQ(registry.GetSessionImpl("session_1"), created_session);
}

TEST(TransportSessionRegistryImplTest, CreateSession) {
  MockTransportChannel channel;
  TransportSessionRegistryImpl registry(channel.GetWeakPtr());

  TransportSessionImpl* session1 =
      registry.CreateSession("session_1", InitIngressSource::kUnspecified);
  ASSERT_NE(session1, nullptr);
  EXPECT_EQ(session1->GetSessionId(), "session_1");

  TransportSessionImpl* session1_again =
      registry.CreateSession("session_1", InitIngressSource::kUnspecified);
  EXPECT_EQ(session1_again, nullptr);
  EXPECT_EQ(registry.GetSessionImpl("session_1"), session1);

  TransportSessionImpl* session2 =
      registry.CreateSession("session_2", InitIngressSource::kUnspecified);
  ASSERT_NE(session2, nullptr);
  EXPECT_NE(session1, session2);
}

TEST(TransportSessionRegistryImplTest, DestroySession) {
  MockTransportChannel channel;
  TransportSessionRegistryImpl registry(channel.GetWeakPtr());

  TransportSessionImpl* session =
      registry.CreateSession("session_1", InitIngressSource::kUnspecified);
  ASSERT_NE(session, nullptr);

  registry.DestroySession("session_1");
  EXPECT_EQ(registry.GetSession("session_1"), nullptr);
}

TEST(TransportSessionRegistryImplTest, Clear) {
  MockTransportChannel channel;
  TransportSessionRegistryImpl registry(channel.GetWeakPtr());

  registry.CreateSession("session_1", InitIngressSource::kUnspecified);
  registry.CreateSession("session_2", InitIngressSource::kUnspecified);

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

  TransportSessionImpl* session1 =
      registry.CreateSession("session_1", InitIngressSource::kUnspecified);
  ASSERT_NE(session1, nullptr);
  TransportSessionImpl* session2 =
      registry.CreateSession("session_2", InitIngressSource::kUnspecified);
  ASSERT_NE(session2, nullptr);

  // 3rd session exceeds the limit of 2 and should be rejected.
  TransportSessionImpl* session3 =
      registry.CreateSession("session_3", InitIngressSource::kUnspecified);
  EXPECT_EQ(session3, nullptr);

  // Existing sessions can still be retrieved without error.
  EXPECT_EQ(registry.GetSessionImpl("session_1"), session1);

  // Destroying a session frees up capacity.
  registry.DestroySession("session_1");
  TransportSessionImpl* session3_retry =
      registry.CreateSession("session_3", InitIngressSource::kUnspecified);
  ASSERT_NE(session3_retry, nullptr);
  EXPECT_EQ(session3_retry->GetSessionId(), "session_3");
}

// Verifies that
// `Browser.Actuator.Transport.SessionRegistry.ExistingSessionsCount` records
// the number of currently active transport sessions immediately prior to
// attempting to create a new session.
TEST(TransportSessionRegistryImplTest,
     ExistingSessionsCountHistogram_RecordedOnSessionCreation) {
  base::HistogramTester histogram_tester;
  MockTransportChannel channel;
  TransportSessionRegistryImpl registry(channel.GetWeakPtr(),
                                        /*max_concurrent_sessions=*/2);

  histogram_tester.ExpectTotalCount(
      "Browser.Actuator.Transport.SessionRegistry.ExistingSessionsCount", 0);

  // 1st session creation: registry has 0 existing sessions.
  registry.CreateSession("session_1", InitIngressSource::kUnspecified);
  histogram_tester.ExpectUniqueSample(
      "Browser.Actuator.Transport.SessionRegistry.ExistingSessionsCount",
      /*sample=*/0,
      /*expected_count=*/1);

  // 2nd session creation: registry has 1 existing session.
  registry.CreateSession("session_2", InitIngressSource::kUnspecified);
  histogram_tester.ExpectBucketCount(
      "Browser.Actuator.Transport.SessionRegistry.ExistingSessionsCount",
      /*sample=*/1,
      /*expected_count=*/1);
  histogram_tester.ExpectTotalCount(
      "Browser.Actuator.Transport.SessionRegistry.ExistingSessionsCount", 2);
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
  registry.CreateSession("session_1", InitIngressSource::kUnspecified);
  registry.CreateSession("session_2", InitIngressSource::kUnspecified);

  // Attempting to create a 3rd session exceeds the limit and is rejected.
  // The histogram should still record that 2 sessions currently exist.
  EXPECT_EQ(
      registry.CreateSession("session_3", InitIngressSource::kUnspecified),
      nullptr);
  histogram_tester.ExpectBucketCount(
      "Browser.Actuator.Transport.SessionRegistry.ExistingSessionsCount",
      /*sample=*/2,
      /*expected_count=*/1);
  histogram_tester.ExpectTotalCount(
      "Browser.Actuator.Transport.SessionRegistry.ExistingSessionsCount", 3);
}

// Verifies that
// `Browser.Actuator.Transport.SessionRegistry.SessionLimitReached` records
// `false` when a new session is created within the allowed limit, and `true`
// when the session request is rejected because the maximum limit is reached.
TEST(TransportSessionRegistryImplTest,
     SessionLimitReachedHistogram_RecordsSuccessAndRejection) {
  base::HistogramTester histogram_tester;
  MockTransportChannel channel;
  TransportSessionRegistryImpl registry(channel.GetWeakPtr(),
                                        /*max_concurrent_sessions=*/2);

  histogram_tester.ExpectTotalCount(
      "Browser.Actuator.Transport.SessionRegistry.SessionLimitReached", 0);

  // 1st and 2nd session creations succeed (limit not reached -> false).
  registry.CreateSession("session_1", InitIngressSource::kUnspecified);
  histogram_tester.ExpectBucketCount(
      "Browser.Actuator.Transport.SessionRegistry.SessionLimitReached",
      /*sample=*/false,
      /*expected_count=*/1);

  registry.CreateSession("session_2", InitIngressSource::kUnspecified);
  histogram_tester.ExpectBucketCount(
      "Browser.Actuator.Transport.SessionRegistry.SessionLimitReached",
      /*sample=*/false,
      /*expected_count=*/2);

  // 3rd session creation is rejected (limit reached -> true).
  EXPECT_EQ(
      registry.CreateSession("session_3", InitIngressSource::kUnspecified),
      nullptr);
  histogram_tester.ExpectBucketCount(
      "Browser.Actuator.Transport.SessionRegistry.SessionLimitReached",
      /*sample=*/true,
      /*expected_count=*/1);
  histogram_tester.ExpectTotalCount(
      "Browser.Actuator.Transport.SessionRegistry.SessionLimitReached", 3);
}

// Verifies that looking up an existing session or attempting to create a
// duplicate session in the registry does not record any new samples to either
// histogram.
TEST(TransportSessionRegistryImplTest,
     Histograms_NotRecordedWhenRetrievingExistingSession) {
  base::HistogramTester histogram_tester;
  MockTransportChannel channel;
  TransportSessionRegistryImpl registry(channel.GetWeakPtr(),
                                        /*max_concurrent_sessions=*/2);

  // Create initial session.
  registry.CreateSession("session_1", InitIngressSource::kUnspecified);
  histogram_tester.ExpectTotalCount(
      "Browser.Actuator.Transport.SessionRegistry.ExistingSessionsCount", 1);
  histogram_tester.ExpectTotalCount(
      "Browser.Actuator.Transport.SessionRegistry.SessionLimitReached", 1);

  // Attempting to create the existing session again or retrieving it should not
  // record new histogram samples.
  EXPECT_EQ(
      registry.CreateSession("session_1", InitIngressSource::kUnspecified),
      nullptr);
  TransportSessionImpl* session1 = registry.GetSessionImpl("session_1");
  ASSERT_NE(session1, nullptr);
  EXPECT_EQ(session1->GetSessionId(), "session_1");

  histogram_tester.ExpectTotalCount(
      "Browser.Actuator.Transport.SessionRegistry.ExistingSessionsCount", 1);
  histogram_tester.ExpectTotalCount(
      "Browser.Actuator.Transport.SessionRegistry.SessionLimitReached", 1);
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
  registry.CreateSession("session_1", InitIngressSource::kUnspecified);
  registry.CreateSession("session_2", InitIngressSource::kUnspecified);

  // Destroying session_1 reduces active sessions from 2 to 1.
  registry.DestroySession("session_1");

  // Creating session_3 should now succeed:
  // - ExistingSessionsCount records 1 (1 active session remained).
  // - SessionLimitReached records false.
  TransportSessionImpl* session3 =
      registry.CreateSession("session_3", InitIngressSource::kUnspecified);
  ASSERT_NE(session3, nullptr);
  EXPECT_EQ(session3->GetSessionId(), "session_3");

  histogram_tester.ExpectBucketCount(
      "Browser.Actuator.Transport.SessionRegistry.ExistingSessionsCount",
      /*sample=*/1,
      /*expected_count=*/2);
  histogram_tester.ExpectBucketCount(
      "Browser.Actuator.Transport.SessionRegistry.SessionLimitReached",
      /*sample=*/false,
      /*expected_count=*/3);
}

// Verifies that `Browser.Actuator.Transport.InitIngressSource` records the
// provided `InitIngressSource` once for each newly created session.
TEST(TransportSessionRegistryImplTest,
     InitIngressSourceHistogram_RecordedOnSessionCreation) {
  base::HistogramTester histogram_tester;
  MockTransportChannel channel;
  TransportSessionRegistryImpl registry(channel.GetWeakPtr(),
                                        /*max_concurrent_sessions=*/10);

  histogram_tester.ExpectTotalCount(
      "Browser.Actuator.Transport.InitIngressSource", 0);

  ASSERT_NE(registry.CreateSession("session_fcm", InitIngressSource::kFcmPush),
            nullptr);
  ASSERT_NE(registry.CreateSession("session_stream",
                                   InitIngressSource::kWatchSessionsStream),
            nullptr);
  ASSERT_NE(registry.CreateSession("session_local",
                                   InitIngressSource::kLocalChromeIngest),
            nullptr);
  ASSERT_NE(registry.CreateSession("session_unspecified",
                                   InitIngressSource::kUnspecified),
            nullptr);

  histogram_tester.ExpectBucketCount(
      "Browser.Actuator.Transport.InitIngressSource",
      InitIngressSource::kFcmPush, 1);
  histogram_tester.ExpectBucketCount(
      "Browser.Actuator.Transport.InitIngressSource",
      InitIngressSource::kWatchSessionsStream, 1);
  histogram_tester.ExpectBucketCount(
      "Browser.Actuator.Transport.InitIngressSource",
      InitIngressSource::kLocalChromeIngest, 1);
  histogram_tester.ExpectBucketCount(
      "Browser.Actuator.Transport.InitIngressSource",
      InitIngressSource::kUnspecified, 1);
  histogram_tester.ExpectTotalCount(
      "Browser.Actuator.Transport.InitIngressSource", 4);
}

// Verifies that a duplicate `CreateSession()` or a `GetSession()` lookup for an
// already-active session does not record to
// `Browser.Actuator.Transport.InitIngressSource`.
TEST(TransportSessionRegistryImplTest,
     InitIngressSourceHistogram_NotRecordedWhenSessionAlreadyActive) {
  base::HistogramTester histogram_tester;
  MockTransportChannel channel;
  TransportSessionRegistryImpl registry(channel.GetWeakPtr());

  TransportSessionImpl* session =
      registry.CreateSession("session_1", InitIngressSource::kFcmPush);
  ASSERT_NE(session, nullptr);
  histogram_tester.ExpectUniqueSample(
      "Browser.Actuator.Transport.InitIngressSource",
      InitIngressSource::kFcmPush, 1);

  EXPECT_EQ(registry.CreateSession("session_1",
                                   InitIngressSource::kWatchSessionsStream),
            nullptr);
  EXPECT_EQ(registry.GetSession("session_1"), session);
  histogram_tester.ExpectUniqueSample(
      "Browser.Actuator.Transport.InitIngressSource",
      InitIngressSource::kFcmPush, 1);
}

// Verifies that `Browser.Actuator.Transport.InitIngressSource` is not recorded
// when session creation fails because the concurrent session limit is reached.
TEST(TransportSessionRegistryImplTest,
     InitIngressSourceHistogram_NotRecordedWhenSessionLimitExceeded) {
  base::HistogramTester histogram_tester;
  MockTransportChannel channel;
  TransportSessionRegistryImpl registry(channel.GetWeakPtr(),
                                        /*max_concurrent_sessions=*/1);

  ASSERT_NE(registry.CreateSession("session_1", InitIngressSource::kFcmPush),
            nullptr);
  EXPECT_EQ(registry.CreateSession("session_2",
                                   InitIngressSource::kWatchSessionsStream),
            nullptr);
  histogram_tester.ExpectUniqueSample(
      "Browser.Actuator.Transport.InitIngressSource",
      InitIngressSource::kFcmPush, 1);
}

namespace {

class MockSessionRegistryObserver : public TransportSessionRegistry::Observer {
 public:
  MOCK_METHOD(void,
              OnSessionRegistered,
              (TransportSession * session),
              (override));
  MOCK_METHOD(void,
              OnSessionRemoved,
              (std::string_view session_id),
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

  TransportSessionImpl* session1 =
      registry.CreateSession("session_1", InitIngressSource::kUnspecified);

  EXPECT_EQ(created_session, session1);
}

TEST(TransportSessionRegistryImplTest, NotifiesObserversOnSessionRemoved) {
  MockTransportChannel channel;
  TransportSessionRegistryImpl registry(channel.GetWeakPtr());
  MockSessionRegistryObserver observer;
  registry.AddObserver(&observer);

  EXPECT_CALL(observer, OnSessionRegistered).Times(1);
  ASSERT_NE(
      registry.CreateSession("session_1", InitIngressSource::kUnspecified),
      nullptr);

  EXPECT_CALL(observer, OnSessionRemoved(std::string_view("session_1")))
      .Times(1);
  registry.DestroySession("session_1");

  // Destroying a non-existent or already-destroyed session does not notify.
  EXPECT_CALL(observer, OnSessionRemoved).Times(0);
  registry.DestroySession("session_1");
}

TEST(TransportSessionRegistryImplTest,
     DoesNotNotifyObserversWhenGettingExistingSession) {
  MockTransportChannel channel;
  TransportSessionRegistryImpl registry(channel.GetWeakPtr());
  MockSessionRegistryObserver observer;
  registry.AddObserver(&observer);
  TransportSessionImpl* session1 =
      registry.CreateSession("session_1", InitIngressSource::kUnspecified);

  EXPECT_CALL(observer, OnSessionRegistered).Times(0);
  EXPECT_EQ(
      registry.CreateSession("session_1", InitIngressSource::kUnspecified),
      nullptr);
  TransportSessionImpl* retrieved_session =
      registry.GetSessionImpl("session_1");

  EXPECT_EQ(retrieved_session, session1);
}

TEST(TransportSessionRegistryImplTest, DoesNotNotifyRemovedObservers) {
  MockTransportChannel channel;
  TransportSessionRegistryImpl registry(channel.GetWeakPtr());
  MockSessionRegistryObserver observer;
  registry.AddObserver(&observer);
  registry.RemoveObserver(&observer);

  EXPECT_CALL(observer, OnSessionRegistered).Times(0);
  registry.CreateSession("session_1", InitIngressSource::kUnspecified);
}

TEST(TransportSessionRegistryImplTest,
     ReadsMaxConcurrentSessionsFeatureParamDynamically) {
  MockTransportChannel channel;
  {
    TransportSessionRegistryImpl default_registry(channel.GetWeakPtr());
    EXPECT_EQ(default_registry.max_concurrent_sessions(), 10u);
  }
  {
    base::test::ScopedFeatureList feature_list;
    feature_list.InitAndEnableFeatureWithParameters(
        kBrowserActuatorProtoStreamTransport,
        {{kMaxTransportSessions.name, "5"}});
    TransportSessionRegistryImpl overridden_registry(channel.GetWeakPtr());
    EXPECT_EQ(overridden_registry.max_concurrent_sessions(), 5u);
  }
}

TEST(TransportSessionRegistryImplTest,
     EvictsIdleSessionsWhenCreatingNewSession) {
  base::test::TaskEnvironment task_environment(
      base::test::TaskEnvironment::TimeSource::MOCK_TIME);
  MockTransportChannel channel;
  TransportHandlerFactoryRegistryImpl handler_registry;
  EXPECT_CALL(channel, GetHandlerFactoryRegistry())
      .WillRepeatedly(testing::Return(&handler_registry));
  TransportSessionRegistryImpl registry(channel.GetWeakPtr(),
                                        /*max_concurrent_sessions=*/2);

  ASSERT_NE(
      registry.CreateSession("session_1", InitIngressSource::kUnspecified),
      nullptr);
  task_environment.FastForwardBy(base::Minutes(10));
  TransportSessionImpl* session2 =
      registry.CreateSession("session_2", InitIngressSource::kUnspecified);
  ASSERT_NE(session2, nullptr);

  // Advance 25 minutes and record activity on `session_2` via `OnMessage()`,
  // then advance another 10 minutes: `session_1` has been idle for 45m (>= 30m
  // default TTL), while `session_2` is 35m past `start_time()` (>= 30m TTL) but
  // only 10m past its `OnMessage()` activity (< 30m TTL). Creating `session_3`
  // evicts `session_1` and succeeds despite `max_concurrent_sessions` == 2.
  task_environment.FastForwardBy(base::Minutes(25));
  session2->OnMessage(PayloadType::kControl, "");
  task_environment.FastForwardBy(base::Minutes(10));
  EXPECT_NE(
      registry.CreateSession("session_3", InitIngressSource::kUnspecified),
      nullptr);
  EXPECT_EQ(registry.GetSession("session_1"), nullptr);
  EXPECT_NE(registry.GetSession("session_2"), nullptr);

  // Advance 30 minutes so both `session_2` and `session_3` expire. All expired
  // sessions must be erased from the registry before `OnSessionRemoved()` fires
  // for any of them.
  MockSessionRegistryObserver observer;
  registry.AddObserver(&observer);
  EXPECT_CALL(observer, OnSessionRemoved)
      .Times(2)
      .WillRepeatedly([&registry](std::string_view) {
        EXPECT_TRUE(registry.GetAllSessionImpls().empty());
      });
  EXPECT_CALL(observer, OnSessionRegistered).Times(1);

  task_environment.FastForwardBy(base::Minutes(30));
  EXPECT_NE(
      registry.CreateSession("session_4", InitIngressSource::kUnspecified),
      nullptr);
  registry.RemoveObserver(&observer);
}

TEST(TransportSessionRegistryImplTest, RejectsEmptyOrOversizedSessionId) {
  MockTransportChannel channel;
  TransportSessionRegistryImpl registry(channel.GetWeakPtr());

  EXPECT_EQ(registry.CreateSession("", InitIngressSource::kUnspecified),
            nullptr);
  EXPECT_EQ(registry.CreateSession(
                std::string(
                    TransportSessionRegistryImpl::kMaxSessionIdLength + 1, 'a'),
                InitIngressSource::kUnspecified),
            nullptr);
  EXPECT_NE(
      registry.CreateSession(
          std::string(TransportSessionRegistryImpl::kMaxSessionIdLength, 'a'),
          InitIngressSource::kUnspecified),
      nullptr);
}

}  // namespace
}  // namespace browser_actuator::transport
