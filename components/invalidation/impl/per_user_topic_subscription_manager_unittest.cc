// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/invalidation/impl/per_user_topic_subscription_manager.h"

#include "base/barrier_callback.h"
#include "base/functional/bind.h"
#include "base/json/json_string_value_serializer.h"
#include "base/json/json_writer.h"
#include "base/run_loop.h"
#include "base/strings/stringprintf.h"
#include "base/test/metrics/histogram_tester.h"
#include "base/test/task_environment.h"
#include "base/values.h"
#include "components/invalidation/impl/profile_identity_provider.h"
#include "components/invalidation/public/invalidation_util.h"
#include "components/prefs/testing_pref_service.h"
#include "components/signin/public/identity_manager/identity_test_environment.h"
#include "net/http/http_status_code.h"
#include "services/data_decoder/public/cpp/test_support/in_process_data_decoder.h"
#include "services/network/public/mojom/url_response_head.mojom.h"
#include "services/network/test/test_url_loader_factory.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "url/gurl.h"

using testing::_;
using testing::Contains;
using testing::Eq;
using testing::NiceMock;
using testing::Not;
using testing::SizeIs;

namespace invalidation {

namespace {

size_t kInvalidationTopicsCount = 5;

const char kInvalidationRegistrationScope[] =
    "https://firebaseperusertopics-pa.googleapis.com";

const char kProjectId[] = "8181035976";

const char kTypeSubscribedForInvalidation[] =
    "invalidation.per_sender_registered_for_invalidation";

const char kActiveRegistrationTokens[] =
    "invalidation.per_sender_active_registration_tokens";

const char kFakeInstanceIdToken[] = "fake_instance_id_token";

class MockIdentityDiagnosticsObserver
    : public signin::IdentityManager::DiagnosticsObserver {
 public:
  MOCK_METHOD3(OnAccessTokenRequested,
               void(const CoreAccountId&,
                    const std::string&,
                    const signin::ScopeSet&));
  MOCK_METHOD2(OnAccessTokenRemovedFromCache,
               void(const CoreAccountId&, const signin::ScopeSet&));
};

std::string IndexToName(size_t index) {
  char name[2] = "a";
  name[0] += static_cast<char>(index);
  return name;
}

TopicMap GetSequenceOfTopicsStartingAt(size_t start, size_t count) {
  TopicMap topics;
  for (size_t i = start; i < start + count; ++i) {
    topics.emplace(IndexToName(i), TopicMetadata{false});
  }
  return topics;
}

TopicMap GetSequenceOfTopics(size_t count) {
  return GetSequenceOfTopicsStartingAt(0, count);
}

TopicSet TopicSetFromTopics(const TopicMap& topics) {
  TopicSet topic_set;
  for (auto& topic : topics) {
    topic_set.insert(topic.first);
  }
  return topic_set;
}

network::mojom::URLResponseHeadPtr CreateHeadersForTest(int responce_code) {
  auto head = network::mojom::URLResponseHead::New();
  head->headers = new net::HttpResponseHeaders(base::StringPrintf(
      "HTTP/1.1 %d OK\nContent-type: text/html\n\n", responce_code));
  head->mime_type = "text/html";
  return head;
}

GURL FullSubscriptionUrl(const std::string& token) {
  return GURL(base::StringPrintf(
      "%s/v1/perusertopics/%s/rel/topics/?subscriber_token=%s",
      kInvalidationRegistrationScope, kProjectId, token.c_str()));
}

GURL FullUnSubscriptionUrlForTopic(const std::string& topic) {
  return GURL(base::StringPrintf(
      "%s/v1/perusertopics/%s/rel/topics/%s?subscriber_token=%s",
      kInvalidationRegistrationScope, kProjectId, topic.c_str(),
      kFakeInstanceIdToken));
}

network::URLLoaderCompletionStatus CreateStatusForTest(
    int status,
    const std::string& response_body) {
  network::URLLoaderCompletionStatus response_status(status);
  response_status.decoded_body_length = response_body.size();
  return response_status;
}

}  // namespace

class RegistrationManagerStateObserver
    : public PerUserTopicSubscriptionManager::Observer {
 public:
  void OnSubscriptionChannelStateChanged(
      SubscriptionChannelState state) override {
    state_ = state;
    if (run_loop_) {
      run_loop_->Quit();
    }
  }

  void OnSubscriptionRequestStarted(Topic topic) override {
    subscription_requests_started_++;
    if (run_loop_) {
      run_loop_->Quit();
    }
  }

  void OnSubscriptionRequestFinished(Topic topic, Status code) override {
    subscription_requests_finished_++;
    if (run_loop_) {
      run_loop_->Quit();
    }
  }

  void WaitForState(SubscriptionChannelState expected_state) {
    while (state_ != expected_state) {
      run_loop_ = std::make_unique<base::RunLoop>();
      run_loop_->Run();
      run_loop_.reset();
    }
  }

  void WaitForSubscriptionRequestsStarted(int num_subscription_requests) {
    while (subscription_requests_started_ < num_subscription_requests) {
      run_loop_ = std::make_unique<base::RunLoop>();
      run_loop_->Run();
      run_loop_.reset();
    }
    subscription_requests_started_ = 0;
    subscription_requests_started_ = 0;
  }

  void WaitForSubscriptionRequestsFinished(int num_subscription_requests) {
    while (subscription_requests_finished_ < num_subscription_requests) {
      run_loop_ = std::make_unique<base::RunLoop>();
      run_loop_->Run();
      run_loop_.reset();
    }
    subscription_requests_started_ = 0;
    subscription_requests_finished_ = 0;
  }

 private:
  SubscriptionChannelState state_ = SubscriptionChannelState::NOT_STARTED;
  std::unique_ptr<base::RunLoop> run_loop_;
  int subscription_requests_started_ = 0;
  int subscription_requests_finished_ = 0;
};

class PerUserTopicSubscriptionManagerTest : public testing::Test {
 protected:
  PerUserTopicSubscriptionManagerTest() = default;
  ~PerUserTopicSubscriptionManagerTest() override = default;

  void SetUp() override {
    PerUserTopicSubscriptionManager::RegisterProfilePrefs(
        pref_service_.registry());
    AccountInfo account = identity_test_env_.MakePrimaryAccountAvailable(
        "example@gmail.com", signin::ConsentLevel::kSync);
    identity_test_env_.SetAutomaticIssueOfAccessTokens(true);
    identity_provider_ = std::make_unique<ProfileIdentityProvider>(
        identity_test_env_.identity_manager());
  }

  std::unique_ptr<PerUserTopicSubscriptionManager> BuildRegistrationManager() {
    auto reg_manager = std::make_unique<PerUserTopicSubscriptionManager>(
        identity_provider_.get(), &pref_service_, url_loader_factory(),
        kProjectId);
    reg_manager->Init();
    reg_manager->AddObserver(&state_observer_);
    return reg_manager;
  }

  network::TestURLLoaderFactory* url_loader_factory() {
    return &url_loader_factory_;
  }

  TestingPrefServiceSimple* pref_service() { return &pref_service_; }

  const base::Value::Dict& GetSubscribedTopics() const {
    const base::Value::Dict* subscribed_topics =
        pref_service_.GetDict(kTypeSubscribedForInvalidation)
            .FindDict(kProjectId);
    DCHECK(subscribed_topics);
    return *subscribed_topics;
  }

  void WaitForState(SubscriptionChannelState expected_state) {
    state_observer_.WaitForState(expected_state);
  }

  void WaitForSubscriptionRequestsStarted(int num_subscription_requests) {
    state_observer_.WaitForSubscriptionRequestsStarted(
        num_subscription_requests);
  }

  void WaitForSubscriptionRequestsFinished(int num_subscription_requests) {
    state_observer_.WaitForSubscriptionRequestsFinished(
        num_subscription_requests);
  }

  void WaitForTopics(const PerUserTopicSubscriptionManager& manager,
                     const TopicMap& expected_topics) {
    while (manager.GetSubscribedTopicsForTest() !=
           TopicSetFromTopics(expected_topics)) {
      pref_service()->user_prefs_store()->WaitUntilValueChanges(
          kTypeSubscribedForInvalidation);
    }
  }

  void AddCorrectSubscriptionResponce(
      const std::string& private_topic = std::string(),
      const std::string& token = kFakeInstanceIdToken,
      int http_responce_code = net::HTTP_OK) {
    base::Value::Dict value;
    value.Set("privateTopicName",
              private_topic.empty() ? "test-pr" : private_topic.c_str());
    std::string serialized_response;
    JSONStringValueSerializer serializer(&serialized_response);
    serializer.Serialize(value);
    url_loader_factory()->AddResponse(
        FullSubscriptionUrl(token), CreateHeadersForTest(http_responce_code),
        serialized_response, CreateStatusForTest(net::OK, serialized_response));
  }

  void AddCorrectUnSubscriptionResponceForTopic(const std::string& topic) {
    url_loader_factory()->AddResponse(
        FullUnSubscriptionUrlForTopic(topic),
        CreateHeadersForTest(net::HTTP_OK), std::string() /* response_body */,
        CreateStatusForTest(net::OK, std::string() /* response_body */));
  }

  void FastForwardTimeBy(base::TimeDelta delta) {
    task_environment_.FastForwardBy(delta);
  }

  signin::IdentityTestEnvironment* identity_test_env() {
    return &identity_test_env_;
  }

 private:
  base::test::TaskEnvironment task_environment_{
      base::test::TaskEnvironment::TimeSource::MOCK_TIME};
  data_decoder::test::InProcessDataDecoder in_process_data_decoder_;
  network::TestURLLoaderFactory url_loader_factory_;
  TestingPrefServiceSimple pref_service_;

  signin::IdentityTestEnvironment identity_test_env_;
  std::unique_ptr<ProfileIdentityProvider> identity_provider_;

  RegistrationManagerStateObserver state_observer_;
};

TEST_F(PerUserTopicSubscriptionManagerTest,
       EmptyPrivateTopicShouldNotUpdateSubscribedTopics) {
  auto topics = GetSequenceOfTopics(kInvalidationTopicsCount);

  auto per_user_topic_subscription_manager = BuildRegistrationManager();

  EXPECT_TRUE(per_user_topic_subscription_manager->GetSubscribedTopicsForTest()
                  .empty());

  // Empty response body should result in no successful registrations.
  std::string response_body;

  url_loader_factory()->AddResponse(
      FullSubscriptionUrl(kFakeInstanceIdToken),
      CreateHeadersForTest(net::HTTP_OK), response_body,
      CreateStatusForTest(net::OK, response_body));

  per_user_topic_subscription_manager->UpdateSubscribedTopics(
      topics, kFakeInstanceIdToken);
  WaitForSubscriptionRequestsFinished(kInvalidationTopicsCount);

  // The response didn't contain non-empty topic name. So nothing was
  // registered.
  EXPECT_TRUE(per_user_topic_subscription_manager->GetSubscribedTopicsForTest()
                  .empty());
}

TEST_F(PerUserTopicSubscriptionManagerTest, ShouldUpdateSubscribedTopics) {
  auto topics = GetSequenceOfTopics(kInvalidationTopicsCount);

  auto per_user_topic_subscription_manager = BuildRegistrationManager();
  ASSERT_TRUE(per_user_topic_subscription_manager->GetSubscribedTopicsForTest()
                  .empty());

  AddCorrectSubscriptionResponce();

  per_user_topic_subscription_manager->UpdateSubscribedTopics(
      topics, kFakeInstanceIdToken);
  WaitForTopics(*per_user_topic_subscription_manager, topics);
  EXPECT_TRUE(
      per_user_topic_subscription_manager->HaveAllRequestsFinishedForTest());

  for (const auto& topic : topics) {
    const base::Value::Dict& subscribed_topics = GetSubscribedTopics();
    const std::string* private_topic_value =
        subscribed_topics.FindString(topic.first);
    ASSERT_NE(private_topic_value, nullptr);
  }
}

TEST_F(PerUserTopicSubscriptionManagerTest,
       ShouldNotRequestAccessTokenWhileHaveNoRequests) {
  NiceMock<MockIdentityDiagnosticsObserver> identity_observer;
  identity_test_env()->identity_manager()->AddDiagnosticsObserver(
      &identity_observer);

  EXPECT_CALL(identity_observer, OnAccessTokenRequested(_, _, _)).Times(0);
  BuildRegistrationManager()->UpdateSubscribedTopics({}, kFakeInstanceIdToken);

  identity_test_env()->identity_manager()->RemoveDiagnosticsObserver(
      &identity_observer);
}

TEST_F(PerUserTopicSubscriptionManagerTest, ShouldRepeatRequestsOnFailure) {
  // For this test, we want to manually control when access tokens are returned.
  identity_test_env()->SetAutomaticIssueOfAccessTokens(false);

  NiceMock<MockIdentityDiagnosticsObserver> identity_observer;
  identity_test_env()->identity_manager()->AddDiagnosticsObserver(
      &identity_observer);

  auto topics = GetSequenceOfTopics(kInvalidationTopicsCount);

  auto per_user_topic_subscription_manager = BuildRegistrationManager();
  ASSERT_TRUE(per_user_topic_subscription_manager->GetSubscribedTopicsForTest()
                  .empty());

  // The first subscription attempt will fail.
  AddCorrectSubscriptionResponce(
      /*private_topic=*/std::string(), kFakeInstanceIdToken,
      net::HTTP_INTERNAL_SERVER_ERROR);
  // Since this is a generic failure, not an auth error, the existing access
  // token should *not* get invalidated.
  EXPECT_CALL(identity_observer, OnAccessTokenRemovedFromCache(_, _)).Times(0);

  per_user_topic_subscription_manager->UpdateSubscribedTopics(
      topics, kFakeInstanceIdToken);
  // This should have resulted in a request for an access token. Return one.
  identity_test_env()->WaitForAccessTokenRequestIfNecessaryAndRespondWithToken(
      "access_token", base::Time::Max());

  // Wait for all of the subscription requests to finish, but not for the
  // retries.
  WaitForSubscriptionRequestsFinished(kInvalidationTopicsCount);

  // Since the subscriptions failed, the requests should still be pending.
  EXPECT_TRUE(per_user_topic_subscription_manager->GetSubscribedTopicsForTest()
                  .empty());
  EXPECT_FALSE(
      per_user_topic_subscription_manager->HaveAllRequestsFinishedForTest());

  // The second attempt will succeed.
  AddCorrectSubscriptionResponce();

  // Repeating subscriptions shouldn't bypass backoff.
  // This should have resulted in a request for an access token. Return one.
  per_user_topic_subscription_manager->UpdateSubscribedTopics(
      topics, kFakeInstanceIdToken);
  identity_test_env()->WaitForAccessTokenRequestIfNecessaryAndRespondWithToken(
      "access_token", base::Time::Max());

  // Initial backoff is 2 seconds with 20% jitter, so the minimum possible delay
  // is 1600ms. Advance time to just before that; nothing should have changed
  // yet.
  FastForwardTimeBy(base::Milliseconds(1500));
  EXPECT_TRUE(per_user_topic_subscription_manager->GetSubscribedTopicsForTest()
                  .empty());
  EXPECT_FALSE(
      per_user_topic_subscription_manager->HaveAllRequestsFinishedForTest());

  // The maximum backoff is 2 seconds; advance to just past that.
  // Access token should be refreshed in order to avoid requests with expired
  // access token.
  EXPECT_CALL(identity_observer, OnAccessTokenRequested(_, _, _));
  FastForwardTimeBy(base::Milliseconds(600));
  identity_test_env()->WaitForAccessTokenRequestIfNecessaryAndRespondWithToken(
      "access_token", base::Time::Max());

  // Wait for all of the subscription requests to finish.
  WaitForSubscriptionRequestsFinished(kInvalidationTopicsCount);

  // Now all subscriptions should have finished.
  EXPECT_FALSE(per_user_topic_subscription_manager->GetSubscribedTopicsForTest()
                   .empty());
  EXPECT_TRUE(
      per_user_topic_subscription_manager->HaveAllRequestsFinishedForTest());

  identity_test_env()->identity_manager()->RemoveDiagnosticsObserver(
      &identity_observer);
}

TEST_F(PerUserTopicSubscriptionManagerTest, ShouldNotRepeatOngoingRequests) {
  auto topics = GetSequenceOfTopics(kInvalidationTopicsCount);

  auto per_user_topic_subscription_manager = BuildRegistrationManager();

  per_user_topic_subscription_manager->UpdateSubscribedTopics(
      topics, kFakeInstanceIdToken);
  // Wait for the subscription requests to begin. No response was set, so they
  // will not finish yet.
  WaitForSubscriptionRequestsStarted(kInvalidationTopicsCount);
  // The requests are not finished, so there should be one pending request per
  // invalidation topic.
  // Check pending_requests() size instead of NumPending(), because
  // NumPending() filters out cancelled requests.
  ASSERT_THAT(*url_loader_factory()->pending_requests(),
              SizeIs(kInvalidationTopicsCount));

  per_user_topic_subscription_manager->UpdateSubscribedTopics(
      topics, kFakeInstanceIdToken);

  // No changes in wanted subscriptions or access token, so there should still
  // be only one pending request per invalidation topic.
  // Check pending_requests() size instead of NumPending(), because
  // NumPending() filters out cancelled requests.
  EXPECT_THAT(*url_loader_factory()->pending_requests(),
              SizeIs(kInvalidationTopicsCount));
}

TEST_F(PerUserTopicSubscriptionManagerTest,
       ShouldRepeatAccessTokenRequestOnFailure) {
  // For this test, we want to manually control when access tokens are returned.
  identity_test_env()->SetAutomaticIssueOfAccessTokens(false);

  NiceMock<MockIdentityDiagnosticsObserver> identity_observer;
  identity_test_env()->identity_manager()->AddDiagnosticsObserver(
      &identity_observer);

  auto topics = GetSequenceOfTopics(kInvalidationTopicsCount);

  auto per_user_topic_subscription_manager = BuildRegistrationManager();
  ASSERT_TRUE(per_user_topic_subscription_manager->GetSubscribedTopicsForTest()
                  .empty());

  // Emulate failure on first access token request.
  EXPECT_CALL(identity_observer, OnAccessTokenRequested(_, _, _));
  per_user_topic_subscription_manager->UpdateSubscribedTopics(
      topics, kFakeInstanceIdToken);
  identity_test_env()->WaitForAccessTokenRequestIfNecessaryAndRespondWithError(
      GoogleServiceAuthError(GoogleServiceAuthError::CONNECTION_FAILED));
  testing::Mock::VerifyAndClearExpectations(&identity_observer);

  // Initial backoff is 2 seconds with 20% jitter, so the minimum possible delay
  // is 1600ms. Advance time to just before that; nothing should have changed
  // yet.
  EXPECT_CALL(identity_observer, OnAccessTokenRequested(_, _, _)).Times(0);
  // UpdateSubscribedTopics() call shouldn't lead to backoff bypassing.
  per_user_topic_subscription_manager->UpdateSubscribedTopics(
      topics, kFakeInstanceIdToken);
  FastForwardTimeBy(base::Milliseconds(1500));
  testing::Mock::VerifyAndClearExpectations(&identity_observer);

  // The maximum backoff is 2 seconds; advance to just past that. Now access
  // token should be requested.
  EXPECT_CALL(identity_observer, OnAccessTokenRequested(_, _, _));
  FastForwardTimeBy(base::Milliseconds(600));
  testing::Mock::VerifyAndClearExpectations(&identity_observer);

  // Add valid responses to access token and subscription requests and ensure
  // that subscriptions finished.
  identity_test_env()->WaitForAccessTokenRequestIfNecessaryAndRespondWithToken(
      "valid_access_token", base::Time::Max());
  AddCorrectSubscriptionResponce();
  WaitForSubscriptionRequestsFinished(kInvalidationTopicsCount);
  EXPECT_FALSE(per_user_topic_subscription_manager->GetSubscribedTopicsForTest()
                   .empty());
  EXPECT_TRUE(
      per_user_topic_subscription_manager->HaveAllRequestsFinishedForTest());

  identity_test_env()->identity_manager()->RemoveDiagnosticsObserver(
      &identity_observer);
}

TEST_F(PerUserTopicSubscriptionManagerTest,
       ShouldInvalidateAccessTokenOnUnauthorized) {
  // For this test, we need to manually control when access tokens are returned.
  identity_test_env()->SetAutomaticIssueOfAccessTokens(false);

  NiceMock<MockIdentityDiagnosticsObserver> identity_observer;
  identity_test_env()->identity_manager()->AddDiagnosticsObserver(
      &identity_observer);

  auto topics = GetSequenceOfTopics(kInvalidationTopicsCount);

  auto per_user_topic_subscription_manager = BuildRegistrationManager();
  ASSERT_TRUE(per_user_topic_subscription_manager->GetSubscribedTopicsForTest()
                  .empty());

  // The first subscription attempt will fail with an "unauthorized" error.
  AddCorrectSubscriptionResponce(
      /*private_topic=*/std::string(), kFakeInstanceIdToken,
      net::HTTP_UNAUTHORIZED);
  // This error should result in invalidating the access token.
  EXPECT_CALL(identity_observer, OnAccessTokenRemovedFromCache(_, _));

  per_user_topic_subscription_manager->UpdateSubscribedTopics(
      topics, kFakeInstanceIdToken);
  // This should have resulted in a request for an access token. Return one
  // (which is considered invalid, e.g. already expired).
  identity_test_env()->WaitForAccessTokenRequestIfNecessaryAndRespondWithToken(
      "invalid_access_token", base::Time::Now());

  // Now the subscription requests should be scheduled.
  ASSERT_FALSE(
      per_user_topic_subscription_manager->HaveAllRequestsFinishedForTest());

  // Wait for the subscription requests to happen.
  WaitForSubscriptionRequestsFinished(kInvalidationTopicsCount);

  // Since the subscriptions failed, the requests should still be pending.
  ASSERT_FALSE(
      per_user_topic_subscription_manager->HaveAllRequestsFinishedForTest());

  // A new access token should have been requested. Serving one will trigger
  // another subscription attempt; let this one succeed.
  AddCorrectSubscriptionResponce();
  EXPECT_CALL(identity_observer, OnAccessTokenRemovedFromCache(_, _)).Times(0);
  identity_test_env()->WaitForAccessTokenRequestIfNecessaryAndRespondWithToken(
      "valid_access_token", base::Time::Max());
  WaitForSubscriptionRequestsFinished(kInvalidationTopicsCount);

  EXPECT_FALSE(per_user_topic_subscription_manager->GetSubscribedTopicsForTest()
                   .empty());
  EXPECT_TRUE(
      per_user_topic_subscription_manager->HaveAllRequestsFinishedForTest());

  identity_test_env()->identity_manager()->RemoveDiagnosticsObserver(
      &identity_observer);
}

TEST_F(PerUserTopicSubscriptionManagerTest,
       ShouldInvalidateAccessTokenOnlyOnce) {
  // For this test, we need to manually control when access tokens are returned.
  identity_test_env()->SetAutomaticIssueOfAccessTokens(false);

  NiceMock<MockIdentityDiagnosticsObserver> identity_observer;
  identity_test_env()->identity_manager()->AddDiagnosticsObserver(
      &identity_observer);

  auto topics = GetSequenceOfTopics(kInvalidationTopicsCount);

  auto per_user_topic_subscription_manager = BuildRegistrationManager();
  ASSERT_TRUE(per_user_topic_subscription_manager->GetSubscribedTopicsForTest()
                  .empty());

  // The first subscription attempt will fail with an "unauthorized" error.
  AddCorrectSubscriptionResponce(
      /*private_topic=*/std::string(), kFakeInstanceIdToken,
      net::HTTP_UNAUTHORIZED);
  // This error should result in invalidating the access token.
  EXPECT_CALL(identity_observer, OnAccessTokenRemovedFromCache(_, _));

  per_user_topic_subscription_manager->UpdateSubscribedTopics(
      topics, kFakeInstanceIdToken);
  // This should have resulted in a request for an access token. Return one
  // (which is considered invalid, e.g. already expired).
  identity_test_env()->WaitForAccessTokenRequestIfNecessaryAndRespondWithToken(
      "invalid_access_token", base::Time::Now());

  // Now the subscription requests should be scheduled.
  ASSERT_FALSE(
      per_user_topic_subscription_manager->HaveAllRequestsFinishedForTest());

  // Wait for the subscription requests to happen.
  WaitForSubscriptionRequestsFinished(kInvalidationTopicsCount);

  // Since the subscriptions failed, the requests should still be pending.
  ASSERT_FALSE(
      per_user_topic_subscription_manager->HaveAllRequestsFinishedForTest());

  // At this point, the old access token should have been invalidated and a new
  // one requested. The new one should *not* get invalidated.
  EXPECT_CALL(identity_observer, OnAccessTokenRemovedFromCache(_, _)).Times(0);
  // Serving a new access token will trigger another subscription attempt, but
  // it'll fail again with the same error.
  identity_test_env()->WaitForAccessTokenRequestIfNecessaryAndRespondWithToken(
      "invalid_access_token_2", base::Time::Max());
  WaitForSubscriptionRequestsFinished(kInvalidationTopicsCount);

  // On the second auth failure, we should have given up - no new access token
  // request should have happened, and all the pending subscriptions should have
  // been dropped, even though still no topics are subscribed.
  EXPECT_FALSE(identity_test_env()->IsAccessTokenRequestPending());
  EXPECT_TRUE(per_user_topic_subscription_manager->GetSubscribedTopicsForTest()
                  .empty());
  EXPECT_TRUE(
      per_user_topic_subscription_manager->HaveAllRequestsFinishedForTest());

  identity_test_env()->identity_manager()->RemoveDiagnosticsObserver(
      &identity_observer);
}

TEST_F(PerUserTopicSubscriptionManagerTest,
       ShouldNotRepeatRequestsOnForbidden) {
  auto topics = GetSequenceOfTopics(kInvalidationTopicsCount);

  auto per_user_topic_subscription_manager = BuildRegistrationManager();
  ASSERT_TRUE(per_user_topic_subscription_manager->GetSubscribedTopicsForTest()
                  .empty());

  AddCorrectSubscriptionResponce(
      /*private_topic=*/std::string(), kFakeInstanceIdToken,
      net::HTTP_FORBIDDEN);

  per_user_topic_subscription_manager->UpdateSubscribedTopics(
      topics, kFakeInstanceIdToken);
  WaitForSubscriptionRequestsFinished(kInvalidationTopicsCount);

  EXPECT_TRUE(per_user_topic_subscription_manager->GetSubscribedTopicsForTest()
                  .empty());
  EXPECT_TRUE(
      per_user_topic_subscription_manager->HaveAllRequestsFinishedForTest());
}

TEST_F(PerUserTopicSubscriptionManagerTest,
       ShouldDisableTopicsAndDeleteFromPrefs) {
  auto topics = GetSequenceOfTopics(kInvalidationTopicsCount);

  AddCorrectSubscriptionResponce();

  auto per_user_topic_subscription_manager = BuildRegistrationManager();
  EXPECT_TRUE(per_user_topic_subscription_manager->GetSubscribedTopicsForTest()
                  .empty());

  per_user_topic_subscription_manager->UpdateSubscribedTopics(
      topics, kFakeInstanceIdToken);
  WaitForSubscriptionRequestsFinished(kInvalidationTopicsCount);
  EXPECT_EQ(TopicSetFromTopics(topics),
            per_user_topic_subscription_manager->GetSubscribedTopicsForTest());

  // Disable some topics.
  auto disabled_topics = GetSequenceOfTopics(3);
  auto enabled_topics =
      GetSequenceOfTopicsStartingAt(3, kInvalidationTopicsCount - 3);
  for (const auto& topic : disabled_topics)
    AddCorrectUnSubscriptionResponceForTopic(topic.first);

  per_user_topic_subscription_manager->UpdateSubscribedTopics(
      enabled_topics, kFakeInstanceIdToken);
  WaitForSubscriptionRequestsFinished(kInvalidationTopicsCount - 3);

  // Topics were disabled, check that they're not in the prefs.
  for (const auto& topic : disabled_topics) {
    const base::Value::Dict& subscribed_topics = GetSubscribedTopics();
    const base::Value* private_topic_value =
        subscribed_topics.Find(topic.first);
    ASSERT_EQ(private_topic_value, nullptr);
  }

  // Check that enable topics are still in the prefs.
  for (const auto& topic : enabled_topics) {
    const base::Value::Dict& subscribed_topics = GetSubscribedTopics();
    const std::string* private_topic_value =
        subscribed_topics.FindString(topic.first);
    ASSERT_NE(private_topic_value, nullptr);
  }
}

TEST_F(PerUserTopicSubscriptionManagerTest,
       ShouldDropSavedTopicsOnTokenChange) {
  auto topics = GetSequenceOfTopics(kInvalidationTopicsCount);

  auto per_user_topic_subscription_manager = BuildRegistrationManager();

  EXPECT_TRUE(per_user_topic_subscription_manager->GetSubscribedTopicsForTest()
                  .empty());

  AddCorrectSubscriptionResponce("old-token-topic");

  per_user_topic_subscription_manager->UpdateSubscribedTopics(
      topics, kFakeInstanceIdToken);
  WaitForTopics(*per_user_topic_subscription_manager, topics);

  for (const auto& topic : topics) {
    const base::Value::Dict& subscribed_topics = GetSubscribedTopics();
    const std::string* private_topic_value =
        subscribed_topics.FindString(topic.first);
    ASSERT_NE(private_topic_value, nullptr);
    EXPECT_EQ("old-token-topic", *private_topic_value);
  }

  EXPECT_EQ(kFakeInstanceIdToken, *pref_service()
                                       ->GetDict(kActiveRegistrationTokens)
                                       .FindString(kProjectId));

  std::string token = "new-fake-token";
  AddCorrectSubscriptionResponce("new-token-topic", token);

  per_user_topic_subscription_manager->UpdateSubscribedTopics(topics, token);

  WaitForTopics(*per_user_topic_subscription_manager, topics);
  EXPECT_EQ(token, *pref_service()
                        ->GetDict(kActiveRegistrationTokens)
                        .FindString(kProjectId));

  for (const auto& topic : topics) {
    const base::Value::Dict& subscribed_topics = GetSubscribedTopics();
    const std::string* private_topic_value =
        subscribed_topics.FindString(topic.first);
    ASSERT_NE(private_topic_value, nullptr);
    EXPECT_EQ("new-token-topic", *private_topic_value);
  }
}

TEST_F(PerUserTopicSubscriptionManagerTest,
       ShouldDeleteTopicsFromPrefsWhenRequestFails) {
  auto topics = GetSequenceOfTopics(kInvalidationTopicsCount);

  AddCorrectSubscriptionResponce();

  auto per_user_topic_subscription_manager = BuildRegistrationManager();
  EXPECT_TRUE(per_user_topic_subscription_manager->GetSubscribedTopicsForTest()
                  .empty());

  per_user_topic_subscription_manager->UpdateSubscribedTopics(
      topics, kFakeInstanceIdToken);
  WaitForTopics(*per_user_topic_subscription_manager, topics);

  // Disable some topics.
  auto disabled_topics = GetSequenceOfTopics(3);
  auto enabled_topics =
      GetSequenceOfTopicsStartingAt(3, kInvalidationTopicsCount - 3);
  // Without configuring the response, the request will not happen.
  per_user_topic_subscription_manager->UpdateSubscribedTopics(
      enabled_topics, kFakeInstanceIdToken);
  WaitForSubscriptionRequestsFinished(kInvalidationTopicsCount);

  // Topics should still be removed from prefs.
  for (const auto& topic : disabled_topics) {
    const base::Value::Dict& subscribed_topics = GetSubscribedTopics();
    const base::Value* private_topic_value =
        subscribed_topics.Find(topic.first);
    ASSERT_EQ(private_topic_value, nullptr);
  }

  // Check that enable topics are still in the prefs.
  for (const auto& topic : enabled_topics) {
    const base::Value::Dict& subscribed_topics = GetSubscribedTopics();
    const std::string* private_topic_value =
        subscribed_topics.FindString(topic.first);
    ASSERT_NE(private_topic_value, nullptr);
  }
}

TEST_F(PerUserTopicSubscriptionManagerTest,
       ShouldChangeStatusToEnabledWhenHasNoPendingSubscription) {
  BuildRegistrationManager()->UpdateSubscribedTopics(/*topics=*/{},
                                                     kFakeInstanceIdToken);
  WaitForState(SubscriptionChannelState::ENABLED);
}

TEST_F(PerUserTopicSubscriptionManagerTest,
       ShouldChangeStatusToDisabledWhenTopicsRegistrationFailed) {
  auto topics = GetSequenceOfTopics(kInvalidationTopicsCount);

  AddCorrectSubscriptionResponce();

  auto per_user_topic_subscription_manager = BuildRegistrationManager();
  ASSERT_TRUE(per_user_topic_subscription_manager->GetSubscribedTopicsForTest()
                  .empty());

  per_user_topic_subscription_manager->UpdateSubscribedTopics(
      topics, kFakeInstanceIdToken);
  WaitForState(SubscriptionChannelState::ENABLED);
  EXPECT_EQ(TopicSetFromTopics(topics),
            per_user_topic_subscription_manager->GetSubscribedTopicsForTest());

  // Disable some topics.
  auto disabled_topics = GetSequenceOfTopics(3);
  auto enabled_topics =
      GetSequenceOfTopicsStartingAt(3, kInvalidationTopicsCount - 3);
  per_user_topic_subscription_manager->UpdateSubscribedTopics(
      enabled_topics, kFakeInstanceIdToken);
  WaitForSubscriptionRequestsFinished(kInvalidationTopicsCount);

  // Clear previously configured correct response. So next requests will fail.
  url_loader_factory()->ClearResponses();
  url_loader_factory()->AddResponse(
      FullSubscriptionUrl(kFakeInstanceIdToken).spec(),
      std::string() /* content */, net::HTTP_NOT_FOUND);

  per_user_topic_subscription_manager->UpdateSubscribedTopics(
      topics, kFakeInstanceIdToken);
  WaitForState(SubscriptionChannelState::SUBSCRIPTION_FAILURE);

  // Configure correct response and retry.
  AddCorrectSubscriptionResponce();
  per_user_topic_subscription_manager->UpdateSubscribedTopics(
      topics, kFakeInstanceIdToken);
  WaitForState(SubscriptionChannelState::ENABLED);
}

TEST_F(PerUserTopicSubscriptionManagerTest, ShouldRecordTokenStateHistogram) {
  const char kTokenStateHistogram[] =
      "FCMInvalidations.TokenStateOnRegistrationRequest2";
  enum class TokenStateOnSubscriptionRequest {
    kTokenWasEmpty = 0,
    kTokenUnchanged = 1,
    kTokenChanged = 2,
    kTokenCleared = 3,
  };

  const TopicMap topics = GetSequenceOfTopics(kInvalidationTopicsCount);
  auto per_user_topic_subscription_manager = BuildRegistrationManager();

  // Subscribe to some topics (and provide an InstanceID token).
  {
    base::HistogramTester histograms;

    AddCorrectSubscriptionResponce(/*private_topic=*/"", "original_token");
    per_user_topic_subscription_manager->UpdateSubscribedTopics(
        topics, "original_token");
    WaitForSubscriptionRequestsFinished(kInvalidationTopicsCount);

    histograms.ExpectUniqueSample(
        kTokenStateHistogram, TokenStateOnSubscriptionRequest::kTokenWasEmpty,
        1);
  }

  ASSERT_EQ(TopicSetFromTopics(topics),
            per_user_topic_subscription_manager->GetSubscribedTopicsForTest());
  ASSERT_TRUE(
      per_user_topic_subscription_manager->HaveAllRequestsFinishedForTest());

  // Call UpdateSubscribedTopics again with the same token.
  {
    base::HistogramTester histograms;

    per_user_topic_subscription_manager->UpdateSubscribedTopics(
        topics, "original_token");

    // Nothing happens, so no need to wait for anything.

    histograms.ExpectUniqueSample(
        kTokenStateHistogram, TokenStateOnSubscriptionRequest::kTokenUnchanged,
        1);
  }

  // Topic subscriptions are unchanged.
  ASSERT_EQ(TopicSetFromTopics(topics),
            per_user_topic_subscription_manager->GetSubscribedTopicsForTest());
  ASSERT_TRUE(
      per_user_topic_subscription_manager->HaveAllRequestsFinishedForTest());

  // Call UpdateSubscribedTopics again, but now with a different token.
  {
    base::HistogramTester histograms;

    AddCorrectSubscriptionResponce(/*private_topic=*/"", "different_token");
    per_user_topic_subscription_manager->UpdateSubscribedTopics(
        topics, "different_token");
    WaitForSubscriptionRequestsFinished(kInvalidationTopicsCount);

    histograms.ExpectUniqueSample(
        kTokenStateHistogram, TokenStateOnSubscriptionRequest::kTokenChanged,
        1);
  }

  // Topic subscriptions are still the same (all topics were re-subscribed).
  ASSERT_EQ(TopicSetFromTopics(topics),
            per_user_topic_subscription_manager->GetSubscribedTopicsForTest());
  ASSERT_TRUE(
      per_user_topic_subscription_manager->HaveAllRequestsFinishedForTest());

  // Call ClearInstanceIDToken.
  {
    base::HistogramTester histograms;

    per_user_topic_subscription_manager->ClearInstanceIDToken();
    histograms.ExpectUniqueSample(
        kTokenStateHistogram, TokenStateOnSubscriptionRequest::kTokenCleared,
        1);
  }

  // Topic subscriptions are gone now.
  ASSERT_TRUE(per_user_topic_subscription_manager->GetSubscribedTopicsForTest()
                  .empty());
  ASSERT_TRUE(
      per_user_topic_subscription_manager->HaveAllRequestsFinishedForTest());
}

}  // namespace invalidation
