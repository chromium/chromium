// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <string>
#include <vector>

#include "base/message_loop/message_loop.h"
#include "base/run_loop.h"
#include "base/stl_util.h"
#include "base/threading/thread_task_runner_handle.h"
#include "components/invalidation/impl/fake_invalidation_state_tracker.h"
#include "components/invalidation/impl/fcm_invalidation_listener.h"
#include "components/invalidation/impl/json_unsafe_parser.h"
#include "components/invalidation/impl/per_user_topic_registration_manager.h"
#include "components/invalidation/impl/push_client_channel.h"
#include "components/invalidation/impl/unacked_invalidation_set_test_util.h"
#include "components/invalidation/public/invalidation_util.h"
#include "components/invalidation/public/invalidator_state.h"
#include "components/invalidation/public/object_id_invalidation_map.h"
#include "components/invalidation/public/topic_invalidation_map.h"
#include "google/cacheinvalidation/include/types.h"
#include "jingle/notifier/listener/fake_push_client.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace syncer {

namespace {

using invalidation::ObjectId;

const char kPayload1[] = "payload1";
const char kPayload2[] = "payload2";

const int64_t kVersion1 = 1LL;
const int64_t kVersion2 = 2LL;

// Fake invalidation::InvalidationClient implementation that keeps
// track of registered topics and acked handles.
class FakeInvalidationClient : public InvalidationClient {
 public:
  FakeInvalidationClient() : started_(false) {}
  ~FakeInvalidationClient() override {}

  // invalidation::InvalidationClient implementation.

  void Start() override { started_ = true; }

  void Stop() override { started_ = false; }

 private:
  bool started_;
};

// Fake delegate that keeps track of invalidation counts, payloads,
// and state.
class FakeDelegate : public FCMInvalidationListener::Delegate {
 public:
  explicit FakeDelegate(FCMInvalidationListener* listener)
      : state_(TRANSIENT_INVALIDATION_ERROR) {}
  ~FakeDelegate() override {}

  size_t GetInvalidationCount(const Topic& topic) const {
    auto it = invalidations_.find(topic);
    if (it == invalidations_.end()) {
      return 0;
    } else {
      return it->second.size();
    }
  }

  int64_t GetVersion(const Topic& topic) const {
    auto it = invalidations_.find(topic);
    if (it == invalidations_.end()) {
      ADD_FAILURE() << "No invalidations for topic " << topic;
      return 0;
    } else {
      return it->second.back().version();
    }
  }

  std::string GetPayload(const Topic& topic) const {
    auto it = invalidations_.find(topic);
    if (it == invalidations_.end()) {
      ADD_FAILURE() << "No invalidations for topic " << topic;
      return nullptr;
    } else {
      return it->second.back().payload();
    }
  }

  bool IsUnknownVersion(const Topic& topic) const {
    auto it = invalidations_.find(topic);
    if (it == invalidations_.end()) {
      ADD_FAILURE() << "No invalidations for topic " << topic;
      return false;
    } else {
      return it->second.back().is_unknown_version();
    }
  }

  bool StartsWithUnknownVersion(const Topic& topic) const {
    auto it = invalidations_.find(topic);
    if (it == invalidations_.end()) {
      ADD_FAILURE() << "No invalidations for topic " << topic;
      return false;
    } else {
      return it->second.front().is_unknown_version();
    }
  }

  InvalidatorState GetInvalidatorState() const { return state_; }

  void AcknowledgeNthInvalidation(const Topic& topic, size_t n) {
    List& list = invalidations_[topic];
    auto it = list.begin() + n;
    it->Acknowledge();
  }

  void AcknowledgeAll(const Topic& topic) {
    List& list = invalidations_[topic];
    for (auto it = list.begin(); it != list.end(); ++it) {
      it->Acknowledge();
    }
  }

  void DropNthInvalidation(const Topic& topic, size_t n) {
    List& list = invalidations_[topic];
    auto it = list.begin() + n;
    it->Drop();
    dropped_invalidations_map_.erase(topic);
    dropped_invalidations_map_.insert(std::make_pair(topic, *it));
  }

  void RecoverFromDropEvent(const Topic& topic) {
    auto it = dropped_invalidations_map_.find(topic);
    if (it != dropped_invalidations_map_.end()) {
      it->second.Acknowledge();
      dropped_invalidations_map_.erase(it);
    }
  }

  // FCMInvalidationListener::Delegate implementation.
  void OnInvalidate(const TopicInvalidationMap& invalidation_map) override {
    TopicSet topics = invalidation_map.GetTopics();
    for (const auto& topic : topics) {
      const SingleObjectInvalidationSet& incoming =
          invalidation_map.ForTopic(topic);
      List& list = invalidations_[topic];
      list.insert(list.end(), incoming.begin(), incoming.end());
    }
  }

  void OnInvalidatorStateChange(InvalidatorState state) override {
    state_ = state;
  }

 private:
  typedef std::vector<Invalidation> List;
  typedef std::map<Topic, List> Map;
  typedef std::map<Topic, Invalidation> DropMap;

  Map invalidations_;
  InvalidatorState state_;
  DropMap dropped_invalidations_map_;
};

std::unique_ptr<InvalidationClient> CreateFakeInvalidationClient(
    FakeInvalidationClient** fake_invalidation_client,
    NetworkChannel* network_channel,
    Logger* logger,
    InvalidationListener* listener) {
  std::unique_ptr<FakeInvalidationClient> fake_client =
      std::make_unique<FakeInvalidationClient>();
  *fake_invalidation_client = fake_client.get();
  return fake_client;
}

class MockRegistrationManager : public PerUserTopicRegistrationManager {
 public:
  MockRegistrationManager()
      : PerUserTopicRegistrationManager(
            nullptr /* identity_provider */,
            nullptr /* pref_service */,
            nullptr /* loader_factory */,
            base::BindRepeating(&syncer::JsonUnsafeParser::Parse)) {}
  ~MockRegistrationManager() override {}
  MOCK_METHOD2(UpdateRegisteredTopics,
               void(const TopicSet& topics, const std::string& token));
  MOCK_METHOD0(Init, void());
};

class FCMInvalidationListenerTest : public testing::Test {
 protected:
  FCMInvalidationListenerTest()
      : kBookmarksTopic_("BOOKMARK"),
        kPreferencesTopic_("PREFERENCE"),
        kExtensionsTopic_("EXTENSION"),
        kAppsTopic_("APP"),
        fcm_sync_network_channel_(new FCMSyncNetworkChannel()),
        fake_invalidation_client_(nullptr),
        listener_(base::WrapUnique(fcm_sync_network_channel_)),
        fake_delegate_(&listener_) {}

  void SetUp() override {
    StartClient();

    registred_topics_.insert(kBookmarksTopic_);
    registred_topics_.insert(kPreferencesTopic_);
    listener_.UpdateRegisteredTopics(registred_topics_);
  }

  void TearDown() override { StopClient(); }

  // Restart client without re-registering topics.
  void RestartClient() {
    StopClient();
    StartClient();
  }

  void StartClient() {
    fake_invalidation_client_ = nullptr;
    std::unique_ptr<MockRegistrationManager> mock_registration_manager =
        std::make_unique<MockRegistrationManager>();
    registration_manager_ = mock_registration_manager.get();
    listener_.Start(base::BindOnce(&CreateFakeInvalidationClient,
                                   &fake_invalidation_client_),
                    &fake_delegate_, std::move(mock_registration_manager));
    DCHECK(fake_invalidation_client_);
  }

  void StopClient() {
    // listener_.StopForTest() stops the invalidation scheduler, which
    // deletes any pending tasks without running them.  Some tasks
    // "run and delete" another task, so they must be run in order to
    // avoid leaking the inner task.  listener_.StopForTest() does not
    // schedule any tasks, so it's both necessary and sufficient to
    // drain the task queue before calling it.
    fake_invalidation_client_ = nullptr;
    listener_.StopForTest();
  }

  size_t GetInvalidationCount(const Topic& topic) const {
    return fake_delegate_.GetInvalidationCount(topic);
  }

  std::string GetPayload(const Topic& topic) const {
    return fake_delegate_.GetPayload(topic);
  }

  int64_t GetVersion(const Topic& topic) const {
    return fake_delegate_.GetVersion(topic);
  }

  bool IsUnknownVersion(const Topic& topic) const {
    return fake_delegate_.IsUnknownVersion(topic);
  }

  bool StartsWithUnknownVersion(const Topic& topic) const {
    return fake_delegate_.StartsWithUnknownVersion(topic);
  }

  void AcknowledgeNthInvalidation(const Topic& topic, size_t n) {
    fake_delegate_.AcknowledgeNthInvalidation(topic, n);
  }

  void DropNthInvalidation(const Topic& topic, size_t n) {
    return fake_delegate_.DropNthInvalidation(topic, n);
  }

  void RecoverFromDropEvent(const Topic& topic) {
    return fake_delegate_.RecoverFromDropEvent(topic);
  }

  InvalidatorState GetInvalidatorState() {
    return fake_delegate_.GetInvalidatorState();
  }

  void AcknowledgeAll(const Topic& topic) {
    fake_delegate_.AcknowledgeAll(topic);
  }

  TopicSet GetRegisteredTopics() const {
    return listener_.GetRegisteredIdsForTest();
  }

  void RegisterAndFireInvalidate(const Topic& topic,
                                 int64_t version,
                                 const std::string& payload) {
    FireInvalidate(topic, version, payload);
  }

  void FireInvalidate(const Topic& topic,
                      int64_t version,
                      const std::string& payload) {
    listener_.Invalidate(fake_invalidation_client_, payload, topic, topic,
                         version);
  }

  void EnableNotifications() {
    fcm_sync_network_channel_->NotifyChannelStateChange(INVALIDATIONS_ENABLED);
    listener_.InformTokenRecieved(fake_invalidation_client_, "token");
  }

  void DisableNotifications(InvalidatorState state) {
    fcm_sync_network_channel_->NotifyChannelStateChange(state);
  }

  const Topic kBookmarksTopic_;
  const Topic kPreferencesTopic_;
  const Topic kExtensionsTopic_;
  const Topic kAppsTopic_;

  TopicSet registred_topics_;

 private:
  base::MessageLoop message_loop_;
  FCMSyncNetworkChannel* fcm_sync_network_channel_;
  MockRegistrationManager* registration_manager_;

 protected:
  // A derrived test needs direct access to this.
  FakeInvalidationStateTracker fake_tracker_;

  // Tests need to access these directly.
  FakeInvalidationClient* fake_invalidation_client_;
  FCMInvalidationListener listener_;

 private:
  FakeDelegate fake_delegate_;
};

// Invalidation tests.

// Fire an invalidation without a payload.  It should be processed,
// the payload should remain empty, and the version should be updated.
TEST_F(FCMInvalidationListenerTest, InvalidateNoPayload) {
  const Topic& topic = kBookmarksTopic_;

  RegisterAndFireInvalidate(topic, kVersion1, std::string());

  ASSERT_EQ(1U, GetInvalidationCount(topic));
  ASSERT_FALSE(IsUnknownVersion(topic));
  EXPECT_EQ(kVersion1, GetVersion(topic));
  EXPECT_EQ("", GetPayload(topic));
}

// Fire an invalidation with an empty payload.  It should be
// processed, the payload should remain empty, and the version should
// be updated.
TEST_F(FCMInvalidationListenerTest, InvalidateEmptyPayload) {
  const Topic& topic = kBookmarksTopic_;

  RegisterAndFireInvalidate(topic, kVersion1, std::string());

  ASSERT_EQ(1U, GetInvalidationCount(topic));
  ASSERT_FALSE(IsUnknownVersion(topic));
  EXPECT_EQ(kVersion1, GetVersion(topic));
  EXPECT_EQ("", GetPayload(topic));
}

// Fire an invalidation with a payload.  It should be processed, and
// both the payload and the version should be updated.
TEST_F(FCMInvalidationListenerTest, InvalidateWithPayload) {
  const Topic& topic = kPreferencesTopic_;

  RegisterAndFireInvalidate(topic, kVersion1, kPayload1);

  ASSERT_EQ(1U, GetInvalidationCount(topic));
  ASSERT_FALSE(IsUnknownVersion(topic));
  EXPECT_EQ(kVersion1, GetVersion(topic));
  EXPECT_EQ(kPayload1, GetPayload(topic));
}

// Fire ten invalidations in a row.  All should be received.
TEST_F(FCMInvalidationListenerTest, ManyInvalidations_NoDrop) {
  const int kRepeatCount = 10;
  const Topic& topic = kPreferencesTopic_;
  int64_t initial_version = kVersion1;
  for (int64_t i = initial_version; i < initial_version + kRepeatCount; ++i) {
    RegisterAndFireInvalidate(topic, i, kPayload1);
  }
  ASSERT_EQ(static_cast<size_t>(kRepeatCount), GetInvalidationCount(topic));
  ASSERT_FALSE(IsUnknownVersion(topic));
  EXPECT_EQ(kPayload1, GetPayload(topic));
  EXPECT_EQ(initial_version + kRepeatCount - 1, GetVersion(topic));
}

// Fire an invalidation for an unregistered object topic with a payload.  It
// should still be processed, and both the payload and the version should be
// updated.
TEST_F(FCMInvalidationListenerTest, InvalidateBeforeRegistration_Simple) {
  const Topic kUnregisteredId = "unregistered";
  const Topic& topic = kUnregisteredId;
  TopicSet topics;
  topics.insert(topic);

  EXPECT_EQ(0U, GetInvalidationCount(topic));

  FireInvalidate(topic, kVersion1, kPayload1);

  ASSERT_EQ(0U, GetInvalidationCount(topic));

  EnableNotifications();
  listener_.Ready(fake_invalidation_client_);
  listener_.UpdateRegisteredTopics(topics);

  ASSERT_EQ(1U, GetInvalidationCount(topic));
  ASSERT_FALSE(IsUnknownVersion(topic));
  EXPECT_EQ(kVersion1, GetVersion(topic));
  EXPECT_EQ(kPayload1, GetPayload(topic));
}

// Fire ten invalidations before an object registers.  Some invalidations will
// be dropped an replaced with an unknown version invalidation.
TEST_F(FCMInvalidationListenerTest, InvalidateBeforeRegistration_Drop) {
  const int kRepeatCount =
      UnackedInvalidationSet::kMaxBufferedInvalidations + 1;
  const Topic kUnregisteredId("unregistered");
  const Topic& topic = kUnregisteredId;
  TopicSet topics;
  topics.insert(topic);

  EXPECT_EQ(0U, GetInvalidationCount(topic));

  int64_t initial_version = kVersion1;
  for (int64_t i = initial_version; i < initial_version + kRepeatCount; ++i) {
    FireInvalidate(topic, i, kPayload1);
  }

  EnableNotifications();
  listener_.Ready(fake_invalidation_client_);
  listener_.UpdateRegisteredTopics(topics);

  ASSERT_EQ(UnackedInvalidationSet::kMaxBufferedInvalidations,
            GetInvalidationCount(topic));
}

// Fire an invalidation, then fire another one with a lower version.  Both
// should be received.
TEST_F(FCMInvalidationListenerTest, InvalidateVersion) {
  const Topic& topic = kPreferencesTopic_;

  RegisterAndFireInvalidate(topic, kVersion2, kPayload2);

  ASSERT_EQ(1U, GetInvalidationCount(topic));
  ASSERT_FALSE(IsUnknownVersion(topic));
  EXPECT_EQ(kVersion2, GetVersion(topic));
  EXPECT_EQ(kPayload2, GetPayload(topic));

  FireInvalidate(topic, kVersion1, kPayload1);

  ASSERT_EQ(2U, GetInvalidationCount(topic));
  ASSERT_FALSE(IsUnknownVersion(topic));

  EXPECT_EQ(kVersion1, GetVersion(topic));
  EXPECT_EQ(kPayload1, GetPayload(topic));
}

// Test a simple scenario for multiple IDs.
TEST_F(FCMInvalidationListenerTest, InvalidateMultipleIds) {
  RegisterAndFireInvalidate(kBookmarksTopic_, 3, std::string());
  ASSERT_EQ(1U, GetInvalidationCount(kBookmarksTopic_));
  ASSERT_FALSE(IsUnknownVersion(kBookmarksTopic_));
  EXPECT_EQ(3, GetVersion(kBookmarksTopic_));
  EXPECT_EQ("", GetPayload(kBookmarksTopic_));

  // kExtensionId is not registered, so the invalidation should not get through.
  FireInvalidate(kExtensionsTopic_, 2, std::string());
  ASSERT_EQ(0U, GetInvalidationCount(kExtensionsTopic_));
}

// Without readying the client, disable notifications, then enable
// them.  The listener should still think notifications are disabled.
TEST_F(FCMInvalidationListenerTest, EnableNotificationsNotReady) {
  EXPECT_EQ(TRANSIENT_INVALIDATION_ERROR, GetInvalidatorState());

  DisableNotifications(TRANSIENT_INVALIDATION_ERROR);

  EXPECT_EQ(TRANSIENT_INVALIDATION_ERROR, GetInvalidatorState());

  EnableNotifications();

  EXPECT_EQ(TRANSIENT_INVALIDATION_ERROR, GetInvalidatorState());
}

// Enable notifications then Ready the invalidation client.  The
// delegate should then be ready.
TEST_F(FCMInvalidationListenerTest, EnableNotificationsThenReady) {
  EXPECT_EQ(TRANSIENT_INVALIDATION_ERROR, GetInvalidatorState());

  EnableNotifications();

  EXPECT_EQ(TRANSIENT_INVALIDATION_ERROR, GetInvalidatorState());

  listener_.Ready(fake_invalidation_client_);

  EXPECT_EQ(INVALIDATIONS_ENABLED, GetInvalidatorState());
}

// Ready the invalidation client then enable notifications.  The
// delegate should then be ready.
TEST_F(FCMInvalidationListenerTest, ReadyThenEnableNotifications) {
  EXPECT_EQ(TRANSIENT_INVALIDATION_ERROR, GetInvalidatorState());

  listener_.Ready(fake_invalidation_client_);

  EXPECT_EQ(TRANSIENT_INVALIDATION_ERROR, GetInvalidatorState());

  EnableNotifications();

  EXPECT_EQ(INVALIDATIONS_ENABLED, GetInvalidatorState());
}

// Enable notifications and ready the client.  Then disable
// notifications with an auth error and re-enable notifications.  The
// delegate should go into an auth error mode and then back out.
TEST_F(FCMInvalidationListenerTest, PushClientAuthError) {
  EnableNotifications();
  listener_.Ready(fake_invalidation_client_);

  EXPECT_EQ(INVALIDATIONS_ENABLED, GetInvalidatorState());

  DisableNotifications(INVALIDATION_CREDENTIALS_REJECTED);

  EXPECT_EQ(INVALIDATION_CREDENTIALS_REJECTED, GetInvalidatorState());

  EnableNotifications();

  EXPECT_EQ(INVALIDATIONS_ENABLED, GetInvalidatorState());
}

// A variant of FCMInvalidationListenerTest that starts with some initial
// state.  We make not attempt to abstract away the contents of this state.  The
// tests that make use of this harness depend on its implementation details.
class FCMInvalidationListenerTest_WithInitialState
    : public FCMInvalidationListenerTest {
 public:
  void SetUp() override {
    UnackedInvalidationSet bm_state(ConvertTopicToId(kBookmarksTopic_));
    UnackedInvalidationSet ext_state(ConvertTopicToId(kExtensionsTopic_));

    Invalidation bm_unknown =
        Invalidation::InitUnknownVersion(ConvertTopicToId(kBookmarksTopic_));
    Invalidation bm_v100 =
        Invalidation::Init(ConvertTopicToId(kBookmarksTopic_), 100, "hundred");
    bm_state.Add(bm_unknown);
    bm_state.Add(bm_v100);

    Invalidation ext_v10 =
        Invalidation::Init(ConvertTopicToId(kExtensionsTopic_), 10, "ten");
    Invalidation ext_v20 =
        Invalidation::Init(ConvertTopicToId(kExtensionsTopic_), 20, "twenty");
    ext_state.Add(ext_v10);
    ext_state.Add(ext_v20);

    initial_state.insert(
        std::make_pair(ConvertTopicToId(kBookmarksTopic_), bm_state));
    initial_state.insert(
        std::make_pair(ConvertTopicToId(kExtensionsTopic_), ext_state));

    fake_tracker_.SetSavedInvalidations(initial_state);

    FCMInvalidationListenerTest::SetUp();
  }

  UnackedInvalidationsMap initial_state;
};

}  // namespace

}  // namespace syncer
