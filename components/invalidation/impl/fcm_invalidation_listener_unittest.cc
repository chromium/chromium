// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <string>
#include <vector>

#include "base/bind.h"
#include "base/run_loop.h"
#include "base/stl_util.h"
#include "base/strings/string_number_conversions.h"
#include "base/test/task_environment.h"
#include "base/threading/thread_task_runner_handle.h"
#include "components/invalidation/impl/fake_invalidation_state_tracker.h"
#include "components/invalidation/impl/fcm_invalidation_listener.h"
#include "components/invalidation/impl/per_user_topic_registration_manager.h"
#include "components/invalidation/impl/push_client_channel.h"
#include "components/invalidation/impl/unacked_invalidation_set_test_util.h"
#include "components/invalidation/public/invalidation_util.h"
#include "components/invalidation/public/invalidator_state.h"
#include "components/invalidation/public/object_id_invalidation_map.h"
#include "components/invalidation/public/topic_invalidation_map.h"
#include "google/cacheinvalidation/include/types.h"
#include "jingle/notifier/listener/fake_push_client.h"
#include "services/data_decoder/public/cpp/test_support/in_process_data_decoder.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace syncer {

namespace {

using invalidation::ObjectId;

const char kPayload1[] = "payload1";
const char kPayload2[] = "payload2";

const int64_t kVersion1 = 1LL;
const int64_t kVersion2 = 2LL;

class TestFCMSyncNetworkChannel : public FCMSyncNetworkChannel {
 public:
  void StartListening() override {}
  void StopListening() override {}
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

class MockRegistrationManager : public PerUserTopicRegistrationManager {
 public:
  MockRegistrationManager()
      : PerUserTopicRegistrationManager(
            nullptr /* identity_provider */,
            nullptr /* pref_service */,
            nullptr /* loader_factory */,
            "fake_sender_id",
            false) {
    ON_CALL(*this, LookupRegisteredPublicTopicByPrivateTopic)
        .WillByDefault(testing::ReturnArg<0>());
  }
  ~MockRegistrationManager() override {}
  MOCK_METHOD2(UpdateRegisteredTopics,
               void(const Topics& topics, const std::string& token));
  MOCK_METHOD0(Init, void());
  MOCK_CONST_METHOD1(LookupRegisteredPublicTopicByPrivateTopic,
                     base::Optional<Topic>(const std::string& private_topic));
};

class FCMInvalidationListenerTest : public testing::Test {
 protected:
  FCMInvalidationListenerTest()
      : kBookmarksTopic_("BOOKMARK"),
        kPreferencesTopic_("PREFERENCE"),
        kExtensionsTopic_("EXTENSION"),
        kAppsTopic_("APP"),
        fcm_sync_network_channel_(new TestFCMSyncNetworkChannel()),
        listener_(base::WrapUnique(fcm_sync_network_channel_)),
        fake_delegate_(&listener_) {}

  void SetUp() override {
    StartListener();

    registred_topics_.emplace(kBookmarksTopic_, TopicMetadata{false});
    registred_topics_.emplace(kPreferencesTopic_, TopicMetadata{true});
    listener_.UpdateRegisteredTopics(registred_topics_);
  }

  void TearDown() override {}

  void StartListener() {
    std::unique_ptr<MockRegistrationManager> mock_registration_manager =
        std::make_unique<MockRegistrationManager>();
    registration_manager_ = mock_registration_manager.get();
    listener_.Start(&fake_delegate_, std::move(mock_registration_manager));
  }

  void StopClient() {
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

  Topics GetRegisteredTopics() const {
    return listener_.GetRegisteredIdsForTest();
  }

  void RegisterAndFireInvalidate(const Topic& topic,
                                 const std::string version,
                                 const std::string& payload) {
    FireInvalidate(topic, version, payload);
  }

  void FireInvalidate(const Topic& topic,
                      const std::string version,
                      const std::string& payload) {
    listener_.Invalidate(payload, topic, topic, version);
  }

  void EnableNotifications() {
    fcm_sync_network_channel_->NotifyChannelStateChange(
        FcmChannelState::ENABLED);
    listener_.InformTokenReceived("token");
  }

  void DisableNotifications(FcmChannelState state) {
    fcm_sync_network_channel_->NotifyChannelStateChange(state);
  }

  const Topic kBookmarksTopic_;
  const Topic kPreferencesTopic_;
  const Topic kExtensionsTopic_;
  const Topic kAppsTopic_;

  Topics registred_topics_;

 private:
  base::test::SingleThreadTaskEnvironment task_environment_;
  data_decoder::test::InProcessDataDecoder in_process_data_decoder_;
  FCMSyncNetworkChannel* fcm_sync_network_channel_;
  MockRegistrationManager* registration_manager_;

 protected:
  // A derrived test needs direct access to this.
  FakeInvalidationStateTracker fake_tracker_;

  // Tests need to access these directly.
  FCMInvalidationListener listener_;

 private:
  FakeDelegate fake_delegate_;
};

// Invalidation tests.

// Fire an invalidation without a payload.  It should be processed,
// the payload should remain empty, and the version should be updated.
TEST_F(FCMInvalidationListenerTest, InvalidateNoPayload) {
  const Topic& topic = kBookmarksTopic_;

  RegisterAndFireInvalidate(topic, base::NumberToString(kVersion1),
                            std::string());

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

  RegisterAndFireInvalidate(topic, base::NumberToString(kVersion1),
                            std::string());

  ASSERT_EQ(1U, GetInvalidationCount(topic));
  ASSERT_FALSE(IsUnknownVersion(topic));
  EXPECT_EQ(kVersion1, GetVersion(topic));
  EXPECT_EQ("", GetPayload(topic));
}

// Fire an invalidation with a payload.  It should be processed, and
// both the payload and the version should be updated.
TEST_F(FCMInvalidationListenerTest, InvalidateWithPayload) {
  const Topic& topic = kPreferencesTopic_;

  RegisterAndFireInvalidate(topic, base::NumberToString(kVersion1), kPayload1);

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
    RegisterAndFireInvalidate(topic, base::NumberToString(i), kPayload1);
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
  Topics topics;
  topics.emplace(topic, TopicMetadata{false});

  EXPECT_EQ(0U, GetInvalidationCount(topic));

  FireInvalidate(topic, base::NumberToString(kVersion1), kPayload1);

  ASSERT_EQ(0U, GetInvalidationCount(topic));

  EnableNotifications();
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
  Topics topics;
  topics.emplace(topic, TopicMetadata{false});

  EXPECT_EQ(0U, GetInvalidationCount(topic));

  int64_t initial_version = kVersion1;
  for (int64_t i = initial_version; i < initial_version + kRepeatCount; ++i) {
    FireInvalidate(topic, base::NumberToString(i), kPayload1);
  }

  EnableNotifications();
  listener_.UpdateRegisteredTopics(topics);

  ASSERT_EQ(UnackedInvalidationSet::kMaxBufferedInvalidations,
            GetInvalidationCount(topic));
}

// Fire an invalidation, then fire another one with a lower version.  Both
// should be received.
TEST_F(FCMInvalidationListenerTest, InvalidateVersion) {
  const Topic& topic = kPreferencesTopic_;

  RegisterAndFireInvalidate(topic, base::NumberToString(kVersion2), kPayload2);

  ASSERT_EQ(1U, GetInvalidationCount(topic));
  ASSERT_FALSE(IsUnknownVersion(topic));
  EXPECT_EQ(kVersion2, GetVersion(topic));
  EXPECT_EQ(kPayload2, GetPayload(topic));

  FireInvalidate(topic, base::NumberToString(kVersion1), kPayload1);

  ASSERT_EQ(2U, GetInvalidationCount(topic));
  ASSERT_FALSE(IsUnknownVersion(topic));

  EXPECT_EQ(kVersion1, GetVersion(topic));
  EXPECT_EQ(kPayload1, GetPayload(topic));
}

// Test a simple scenario for multiple IDs.
TEST_F(FCMInvalidationListenerTest, InvalidateMultipleIds) {
  RegisterAndFireInvalidate(kBookmarksTopic_, "3", std::string());
  ASSERT_EQ(1U, GetInvalidationCount(kBookmarksTopic_));
  ASSERT_FALSE(IsUnknownVersion(kBookmarksTopic_));
  EXPECT_EQ(3, GetVersion(kBookmarksTopic_));
  EXPECT_EQ("", GetPayload(kBookmarksTopic_));

  // kExtensionId is not registered, so the invalidation should not get through.
  FireInvalidate(kExtensionsTopic_, "2", std::string());
  ASSERT_EQ(0U, GetInvalidationCount(kExtensionsTopic_));
}

// Disable notifications, then enable them.
TEST_F(FCMInvalidationListenerTest, ReEnableNotifications) {
  DisableNotifications(FcmChannelState::NO_INSTANCE_ID_TOKEN);

  EXPECT_EQ(TRANSIENT_INVALIDATION_ERROR, GetInvalidatorState());

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
