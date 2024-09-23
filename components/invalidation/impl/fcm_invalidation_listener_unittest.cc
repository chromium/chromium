// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <map>
#include <memory>
#include <set>
#include <string>
#include <vector>

#include "base/memory/ptr_util.h"
#include "base/memory/raw_ptr.h"
#include "base/strings/string_number_conversions.h"
#include "base/test/task_environment.h"
#include "components/invalidation/impl/fcm_invalidation_listener.h"
#include "components/invalidation/impl/per_user_topic_subscription_manager.h"
#include "components/invalidation/impl/status.h"
#include "components/invalidation/public/invalidation.h"
#include "components/invalidation/public/invalidation_util.h"
#include "components/invalidation/public/invalidator_state.h"
#include "services/data_decoder/public/cpp/test_support/in_process_data_decoder.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

using testing::IsEmpty;
using testing::UnorderedElementsAre;

namespace invalidation {

namespace {

const char kPayload1[] = "payload1";
const char kPayload2[] = "payload2";

const int64_t kVersion1 = 1LL;
const int64_t kVersion2 = 2LL;

class TestFCMSyncNetworkChannel : public FCMSyncNetworkChannel {
 public:
  void StartListening() override {}
  void StopListening() override {}

  using FCMSyncNetworkChannel::DeliverIncomingMessage;
  using FCMSyncNetworkChannel::DeliverToken;
  using FCMSyncNetworkChannel::NotifyChannelStateChange;
};

// Fake delegate that keeps track of invalidation counts, payloads,
// and state.
class FakeDelegate : public FCMInvalidationListener::Delegate {
 public:
  explicit FakeDelegate(FCMInvalidationListener* listener) {}
  ~FakeDelegate() override = default;

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
      return "";
    } else {
      return it->second.back().payload();
    }
  }

  InvalidatorState GetInvalidatorState() const { return state_; }

  const std::set<Topic>& GetSuccessfullySubscribedTopics() const {
    return successfully_subscribed_topics_;
  }

  // FCMInvalidationListener::Delegate implementation.
  std::optional<Invalidation> OnInvalidate(
      const Invalidation& invalidation) override {
    invalidations_[invalidation.topic()].push_back(invalidation);
    if (shall_dispatch_) {
      return std::nullopt;
    }
    return invalidation;
  }

  void OnInvalidatorStateChange(InvalidatorState state) override {
    state_ = state;
  }

  void OnSuccessfullySubscribed(const Topic& topic) override {
    successfully_subscribed_topics_.insert(topic);
  }

  bool shall_dispatch_ = true;

 private:
  typedef std::vector<Invalidation> List;
  typedef std::map<Topic, List> Map;
  typedef std::map<Topic, Invalidation> DropMap;

  Map invalidations_;
  InvalidatorState state_ = InvalidatorState::kDisabled;
  DropMap dropped_invalidations_map_;
  std::set<Topic> successfully_subscribed_topics_;
};

class MockSubscriptionManager : public PerUserTopicSubscriptionManager {
 public:
  MockSubscriptionManager()
      : PerUserTopicSubscriptionManager(nullptr /* identity_provider */,
                                        nullptr /* pref_service */,
                                        nullptr /* loader_factory */,
                                        "fake_sender_id") {
    ON_CALL(*this, LookupSubscribedPublicTopicByPrivateTopic)
        .WillByDefault(testing::ReturnArg<0>());
  }
  ~MockSubscriptionManager() override = default;
  MOCK_METHOD2(UpdateSubscribedTopics,
               void(const TopicMap& topics, const std::string& token));
  MOCK_METHOD0(Init, void());
  MOCK_CONST_METHOD1(LookupSubscribedPublicTopicByPrivateTopic,
                     std::optional<Topic>(const std::string& private_topic));
  void NotifySubscriptionRequestFinished(
      Topic topic,
      PerUserTopicSubscriptionManager::RequestType request_type,
      Status code) {
    PerUserTopicSubscriptionManager::NotifySubscriptionRequestFinished(
        topic, request_type, code);
  }
};

class FCMInvalidationListenerTest : public testing::Test {
 protected:
  FCMInvalidationListenerTest()
      : kBookmarksTopic_("BOOKMARK"),
        kPreferencesTopic_("PREFERENCE"),
        kExtensionsTopic_("EXTENSION"),
        kAppsTopic_("APP"),
        fcm_sync_network_channel_(new TestFCMSyncNetworkChannel()),
        listener_(base::WrapUnique(fcm_sync_network_channel_.get())),
        fake_delegate_(&listener_) {}

  void SetUp() override {
    StartListener();

    TopicMap initial_topics;
    initial_topics.emplace(kBookmarksTopic_, TopicMetadata{false});
    initial_topics.emplace(kPreferencesTopic_, TopicMetadata{true});
    listener_.UpdateInterestedTopics(initial_topics);
  }

  void TearDown() override {}

  void StartListener() {
    auto mock_subscription_manager =
        std::make_unique<MockSubscriptionManager>();
    subscription_manager_ = mock_subscription_manager.get();
    listener_.Start(&fake_delegate_, std::move(mock_subscription_manager));
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

  InvalidatorState GetInvalidatorState() {
    return fake_delegate_.GetInvalidatorState();
  }

  const std::set<Topic>& GetSuccessfullySubscribedTopics() {
    return fake_delegate_.GetSuccessfullySubscribedTopics();
  }

  void FireInvalidate(const Topic& topic,
                      int64_t version,
                      const std::string& payload) {
    fcm_sync_network_channel_->DeliverIncomingMessage(payload, topic, topic,
                                                      version);
  }

  void NotifySubscriptionRequestFinished(
      Topic topic,
      PerUserTopicSubscriptionManager::RequestType request_type,
      Status code) {
    subscription_manager_->NotifySubscriptionRequestFinished(
        topic, request_type, code);
  }

  void EnableNotifications() {
    fcm_sync_network_channel_->NotifyChannelStateChange(
        FcmChannelState::ENABLED);
    fcm_sync_network_channel_->DeliverToken("token");
  }

  void DisableNotifications(FcmChannelState state) {
    fcm_sync_network_channel_->NotifyChannelStateChange(state);
  }

  const Topic kBookmarksTopic_;
  const Topic kPreferencesTopic_;
  const Topic kExtensionsTopic_;
  const Topic kAppsTopic_;

 private:
  base::test::SingleThreadTaskEnvironment task_environment_;
  data_decoder::test::InProcessDataDecoder in_process_data_decoder_;
  raw_ptr<TestFCMSyncNetworkChannel, DanglingUntriaged>
      fcm_sync_network_channel_;
  raw_ptr<MockSubscriptionManager, DanglingUntriaged> subscription_manager_;

 protected:
  // Tests need to access these directly.
  FCMInvalidationListener listener_;

  FakeDelegate fake_delegate_;
};

// Invalidation tests.

// Fire an invalidation without a payload.  It should be processed,
// the payload should remain empty, and the version should be updated.
TEST_F(FCMInvalidationListenerTest, InvalidateNoPayload) {
  const Topic& topic = kBookmarksTopic_;

  FireInvalidate(topic, kVersion1, std::string());

  ASSERT_EQ(1U, GetInvalidationCount(topic));
  EXPECT_EQ(kVersion1, GetVersion(topic));
  EXPECT_EQ("", GetPayload(topic));
}

// Fire an invalidation with an empty payload.  It should be
// processed, the payload should remain empty, and the version should
// be updated.
TEST_F(FCMInvalidationListenerTest, InvalidateEmptyPayload) {
  const Topic& topic = kBookmarksTopic_;

  FireInvalidate(topic, kVersion1, std::string());

  ASSERT_EQ(1U, GetInvalidationCount(topic));
  EXPECT_EQ(kVersion1, GetVersion(topic));
  EXPECT_EQ("", GetPayload(topic));
}

// Fire an invalidation with a payload.  It should be processed, and
// both the payload and the version should be updated.
TEST_F(FCMInvalidationListenerTest, InvalidateWithPayload) {
  const Topic& topic = kPreferencesTopic_;

  FireInvalidate(topic, kVersion1, kPayload1);

  ASSERT_EQ(1U, GetInvalidationCount(topic));
  EXPECT_EQ(kVersion1, GetVersion(topic));
  EXPECT_EQ(kPayload1, GetPayload(topic));
}

// Fire ten invalidations in a row.  All should be received.
TEST_F(FCMInvalidationListenerTest, ManyInvalidations_NoDrop) {
  const int kRepeatCount = 10;
  const Topic& topic = kPreferencesTopic_;
  int64_t initial_version = kVersion1;
  for (int64_t i = initial_version; i < initial_version + kRepeatCount; ++i) {
    FireInvalidate(topic, i, kPayload1);
  }
  ASSERT_EQ(static_cast<size_t>(kRepeatCount), GetInvalidationCount(topic));
  EXPECT_EQ(kPayload1, GetPayload(topic));
  EXPECT_EQ(initial_version + kRepeatCount - 1, GetVersion(topic));
}

// Fire an invalidation for an unregistered topic with a payload. It should
// still be processed, and both the payload and the version should be updated.
TEST_F(FCMInvalidationListenerTest, InvalidateBeforeRegistration_Simple) {
  const Topic kUnregisteredId = "unregistered";
  const Topic& topic = kUnregisteredId;
  TopicMap topics;
  topics.emplace(topic, TopicMetadata{false});

  EXPECT_EQ(0U, GetInvalidationCount(topic));

  FireInvalidate(topic, kVersion1, kPayload1);

  ASSERT_EQ(0U, GetInvalidationCount(topic));

  EnableNotifications();
  listener_.UpdateInterestedTopics(topics);

  ASSERT_EQ(1U, GetInvalidationCount(topic));
  EXPECT_EQ(kVersion1, GetVersion(topic));
  EXPECT_EQ(kPayload1, GetPayload(topic));
}

// Upon registration, cached invalidations are emitted.
// Successfully dispatched invalidations are removed from the cache.
TEST_F(FCMInvalidationListenerTest, RegisterTwice_DispatchFromCacheOnce) {
  EnableNotifications();

  const Topic kTopic = "my_topic";
  TopicMap topics{{kTopic, TopicMetadata{false}}};

  // An invalidation from an unregistered topic does not get emitted.
  EXPECT_EQ(0U, GetInvalidationCount(kTopic));
  FireInvalidate(kTopic, kVersion1, kPayload1);
  ASSERT_EQ(0U, GetInvalidationCount(kTopic));

  // Once we register, the cache invalidation gets emitted.
  // It will remain cached if the dispatch fails. Subsequent updates of
  // interested topics will cause the invalidation to be emitted again.
  fake_delegate_.shall_dispatch_ = false;
  listener_.UpdateInterestedTopics(topics);
  ASSERT_EQ(1U, GetInvalidationCount(kTopic));
  listener_.UpdateInterestedTopics(topics);
  ASSERT_EQ(2U, GetInvalidationCount(kTopic));

  // However, once the dispatch succeeds, the emitted invalidation is removed
  // from the cache and will not be emitted again.
  fake_delegate_.shall_dispatch_ = true;
  listener_.UpdateInterestedTopics(topics);
  ASSERT_EQ(3U, GetInvalidationCount(kTopic));
  listener_.UpdateInterestedTopics(topics);
  ASSERT_EQ(3U, GetInvalidationCount(kTopic));
}

// Fire a couple of invalidations before any topic registers. For each topic,
// all but the invalidation with the highest version number will be dropped.
TEST_F(FCMInvalidationListenerTest, InvalidateBeforeRegistration_Drop) {
  const int kRepeatCount = 10;
  const Topic kTopicA = "unregistered topic a";
  const Topic kTopicB = "unregistered topic b";
  TopicMap topics;
  topics.emplace(kTopicA, TopicMetadata{false});
  topics.emplace(kTopicB, TopicMetadata{false});

  EXPECT_EQ(0U, GetInvalidationCount(kTopicA));
  EXPECT_EQ(0U, GetInvalidationCount(kTopicB));

  const int64_t initial_version = kVersion1;
  const int64_t max_version = initial_version + kRepeatCount;
  for (int64_t i = initial_version; i <= initial_version + kRepeatCount; ++i) {
    FireInvalidate(kTopicA, i, kPayload1);
    FireInvalidate(kTopicB, i, kPayload1);
  }

  EnableNotifications();
  listener_.UpdateInterestedTopics(topics);

  EXPECT_EQ(1U, GetInvalidationCount(kTopicA));
  EXPECT_EQ(max_version, GetVersion(kTopicA));

  EXPECT_EQ(1U, GetInvalidationCount(kTopicB));
  EXPECT_EQ(max_version, GetVersion(kTopicB));
}

// Fire an invalidation, then fire another one with a lower version.  Both
// should be received.
TEST_F(FCMInvalidationListenerTest, InvalidateVersion) {
  const Topic& topic = kPreferencesTopic_;

  FireInvalidate(topic, kVersion2, kPayload2);

  ASSERT_EQ(1U, GetInvalidationCount(topic));
  EXPECT_EQ(kVersion2, GetVersion(topic));
  EXPECT_EQ(kPayload2, GetPayload(topic));

  FireInvalidate(topic, kVersion1, kPayload1);

  ASSERT_EQ(2U, GetInvalidationCount(topic));

  EXPECT_EQ(kVersion1, GetVersion(topic));
  EXPECT_EQ(kPayload1, GetPayload(topic));
}

// Test a simple scenario for multiple IDs.
TEST_F(FCMInvalidationListenerTest, InvalidateMultipleIds) {
  FireInvalidate(kBookmarksTopic_, 3, std::string());
  ASSERT_EQ(1U, GetInvalidationCount(kBookmarksTopic_));
  EXPECT_EQ(3, GetVersion(kBookmarksTopic_));
  EXPECT_EQ("", GetPayload(kBookmarksTopic_));

  // kExtensionId is not registered, so the invalidation should not get through.
  FireInvalidate(kExtensionsTopic_, 2, std::string());
  ASSERT_EQ(0U, GetInvalidationCount(kExtensionsTopic_));
}

TEST_F(FCMInvalidationListenerTest, EmitSuccessfullySubscribedNotification) {
  const Topic& topic = kPreferencesTopic_;

  EXPECT_THAT(GetSuccessfullySubscribedTopics(), IsEmpty());

  NotifySubscriptionRequestFinished(
      topic, PerUserTopicSubscriptionManager::RequestType::kSubscribe,
      Status(StatusCode::SUCCESS, /*message=*/std::string()));

  EXPECT_THAT(GetSuccessfullySubscribedTopics(), UnorderedElementsAre(topic));
}

}  // namespace

}  // namespace invalidation
