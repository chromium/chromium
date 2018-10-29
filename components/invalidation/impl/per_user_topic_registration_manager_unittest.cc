// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/invalidation/impl/per_user_topic_registration_manager.h"

#include "base/json/json_string_value_serializer.h"
#include "base/json/json_writer.h"
#include "base/macros.h"
#include "base/message_loop/message_loop.h"
#include "base/stl_util.h"
#include "base/strings/stringprintf.h"
#include "base/values.h"
#include "components/invalidation/impl/json_unsafe_parser.h"
#include "components/invalidation/impl/profile_identity_provider.h"
#include "components/invalidation/public/invalidation_util.h"
#include "components/prefs/testing_pref_service.h"
#include "net/http/http_status_code.h"
#include "services/identity/public/cpp/identity_test_environment.h"
#include "services/network/test/test_url_loader_factory.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "url/gurl.h"

namespace syncer {

namespace {

size_t kInvalidationObjectIdsCount = 5;

const char kInvalidationRegistrationScope[] =
    "https://firebaseperusertopics-pa.googleapis.com";

const char kProjectId[] = "8181035976";

const char kTypeRegisteredForInvalidation[] =
    "invalidation.registered_for_invalidation";

const char kActiveRegistrationToken[] =
    "invalidation.active_registration_token";

const char kFakeInstanceIdToken[] = "fake_instance_id_token";

std::string IndexToName(size_t index) {
  char name[2] = "a";
  name[0] += static_cast<char>(index);
  return name;
}

TopicSet GetSequenceOfTopicsStartingAt(size_t start, size_t count) {
  TopicSet ids;
  for (size_t i = start; i < start + count; ++i)
    ids.insert(IndexToName(i));
  return ids;
}

TopicSet GetSequenceOfTopics(size_t count) {
  return GetSequenceOfTopicsStartingAt(0, count);
}

network::ResourceResponseHead CreateHeadersForTest(int responce_code) {
  network::ResourceResponseHead head;
  head.headers = new net::HttpResponseHeaders(base::StringPrintf(
      "HTTP/1.1 %d OK\nContent-type: text/html\n\n", responce_code));
  head.mime_type = "text/html";
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

};  // namespace

class PerUserTopicRegistrationManagerTest : public testing::Test {
 protected:
  PerUserTopicRegistrationManagerTest() {}

  ~PerUserTopicRegistrationManagerTest() override {}

  void SetUp() override {
    PerUserTopicRegistrationManager::RegisterProfilePrefs(
        pref_service_.registry());
    AccountInfo account =
        identity_test_env_.MakePrimaryAccountAvailable("example@gmail.com");
    identity_test_env_.SetAutomaticIssueOfAccessTokens(true);
    identity_provider_ =
        std::make_unique<invalidation::ProfileIdentityProvider>(
            identity_test_env_.identity_manager());
    identity_provider_->SetActiveAccountId(account.account_id);
  }

  std::unique_ptr<PerUserTopicRegistrationManager> BuildRegistrationManager() {
    auto reg_manager = std::make_unique<PerUserTopicRegistrationManager>(
        identity_provider_.get(), &pref_service_, url_loader_factory(),
        base::BindRepeating(&syncer::JsonUnsafeParser::Parse));
    reg_manager->Init();
    return reg_manager;
  }

  network::TestURLLoaderFactory* url_loader_factory() {
    return &url_loader_factory_;
  }

  TestingPrefServiceSimple* pref_service() { return &pref_service_; }

  void AddCorrectSubscriptionResponce(
      const std::string& private_topic = std::string(),
      const std::string& token = kFakeInstanceIdToken) {
    std::unique_ptr<base::DictionaryValue> value(new base::DictionaryValue());
    value->SetString("privateTopicName",
                     private_topic.empty() ? "test-pr" : private_topic.c_str());
    std::string serialized_response;
    JSONStringValueSerializer serializer(&serialized_response);
    serializer.Serialize(*value);
    url_loader_factory()->AddResponse(
        FullSubscriptionUrl(token), CreateHeadersForTest(net::HTTP_OK),
        serialized_response, CreateStatusForTest(net::OK, serialized_response));
  }

  void AddCorrectUnSubscriptionResponceForTopic(const std::string& topic) {
    url_loader_factory()->AddResponse(
        FullUnSubscriptionUrlForTopic(topic),
        CreateHeadersForTest(net::HTTP_OK), std::string() /* response_body */,
        CreateStatusForTest(net::OK, std::string() /* response_body */));
  }

 private:
  base::MessageLoop message_loop_;
  network::TestURLLoaderFactory url_loader_factory_;
  TestingPrefServiceSimple pref_service_;

  identity::IdentityTestEnvironment identity_test_env_;
  std::unique_ptr<invalidation::ProfileIdentityProvider> identity_provider_;

  DISALLOW_COPY_AND_ASSIGN(PerUserTopicRegistrationManagerTest);
};

TEST_F(PerUserTopicRegistrationManagerTest,
       EmptyPrivateTopicShouldNotUpdateRegisteredTopics) {
  TopicSet ids = GetSequenceOfTopics(kInvalidationObjectIdsCount);

  auto per_user_topic_registration_manager = BuildRegistrationManager();

  EXPECT_TRUE(per_user_topic_registration_manager->GetRegisteredIds().empty());

  // Empty response body should result in no succesfull registrations.
  std::string response_body;

  url_loader_factory()->AddResponse(
      FullSubscriptionUrl(kFakeInstanceIdToken),
      CreateHeadersForTest(net::HTTP_OK), response_body,
      CreateStatusForTest(net::OK, response_body));

  per_user_topic_registration_manager->UpdateRegisteredTopics(
      ids, kFakeInstanceIdToken);
  base::RunLoop().RunUntilIdle();

  // The response didn't contain non-empty topic name. So nothing was
  // registered.
  EXPECT_TRUE(per_user_topic_registration_manager->GetRegisteredIds().empty());
}

TEST_F(PerUserTopicRegistrationManagerTest, ShouldUpdateRegisteredTopics) {
  TopicSet ids = GetSequenceOfTopics(kInvalidationObjectIdsCount);

  auto per_user_topic_registration_manager = BuildRegistrationManager();

  EXPECT_TRUE(per_user_topic_registration_manager->GetRegisteredIds().empty());

  AddCorrectSubscriptionResponce();

  per_user_topic_registration_manager->UpdateRegisteredTopics(
      ids, kFakeInstanceIdToken);
  base::RunLoop().RunUntilIdle();

  EXPECT_EQ(ids, per_user_topic_registration_manager->GetRegisteredIds());

  for (const auto& id : ids) {
    const base::DictionaryValue* topics =
        pref_service()->GetDictionary(kTypeRegisteredForInvalidation);
    const base::Value* private_topic_value =
        topics->FindKeyOfType(id, base::Value::Type::STRING);
    ASSERT_NE(private_topic_value, nullptr);
  }
}

TEST_F(PerUserTopicRegistrationManagerTest,
       ShouldDisableIdsAndDeleteFromPrefs) {
  TopicSet ids = GetSequenceOfTopics(kInvalidationObjectIdsCount);

  AddCorrectSubscriptionResponce();

  auto per_user_topic_registration_manager = BuildRegistrationManager();
  EXPECT_TRUE(per_user_topic_registration_manager->GetRegisteredIds().empty());

  per_user_topic_registration_manager->UpdateRegisteredTopics(
      ids, kFakeInstanceIdToken);
  base::RunLoop().RunUntilIdle();
  EXPECT_EQ(ids, per_user_topic_registration_manager->GetRegisteredIds());

  // Disable some ids.
  TopicSet disabled_ids = GetSequenceOfTopics(3);
  TopicSet enabled_ids =
      GetSequenceOfTopicsStartingAt(3, kInvalidationObjectIdsCount - 3);
  for (const auto& id : disabled_ids)
    AddCorrectUnSubscriptionResponceForTopic(id);

  per_user_topic_registration_manager->UpdateRegisteredTopics(
      enabled_ids, kFakeInstanceIdToken);
  base::RunLoop().RunUntilIdle();

  // ids were disabled, check that they're not in the prefs.
  for (const auto& id : disabled_ids) {
    const base::DictionaryValue* topics =
        pref_service()->GetDictionary(kTypeRegisteredForInvalidation);
    const base::Value* private_topic_value = topics->FindKey(id);
    ASSERT_EQ(private_topic_value, nullptr);
  }

  // Check that enable ids are still in the prefs.
  for (const auto& id : enabled_ids) {
    const base::DictionaryValue* topics =
        pref_service()->GetDictionary(kTypeRegisteredForInvalidation);
    const base::Value* private_topic_value =
        topics->FindKeyOfType(id, base::Value::Type::STRING);
    ASSERT_NE(private_topic_value, nullptr);
  }
}

TEST_F(PerUserTopicRegistrationManagerTest,
       ShouldDropSavedTopicsOnTokenChange) {
  TopicSet ids = GetSequenceOfTopics(kInvalidationObjectIdsCount);

  auto per_user_topic_registration_manager = BuildRegistrationManager();

  EXPECT_TRUE(per_user_topic_registration_manager->GetRegisteredIds().empty());

  AddCorrectSubscriptionResponce("old-token-topic");

  per_user_topic_registration_manager->UpdateRegisteredTopics(
      ids, kFakeInstanceIdToken);
  base::RunLoop().RunUntilIdle();

  EXPECT_EQ(ids, per_user_topic_registration_manager->GetRegisteredIds());

  for (const auto& id : ids) {
    const base::DictionaryValue* topics =
        pref_service()->GetDictionary(kTypeRegisteredForInvalidation);
    const base::Value* private_topic_value =
        topics->FindKeyOfType(id, base::Value::Type::STRING);
    ASSERT_NE(private_topic_value, nullptr);
    std::string private_topic;
    private_topic_value->GetAsString(&private_topic);
    EXPECT_EQ(private_topic, "old-token-topic");
  }

  EXPECT_EQ(kFakeInstanceIdToken,
            pref_service()->GetString(kActiveRegistrationToken));

  std::string token = "new-fake-token";
  AddCorrectSubscriptionResponce("new-token-topic", token);

  per_user_topic_registration_manager->UpdateRegisteredTopics(ids, token);
  base::RunLoop().RunUntilIdle();

  EXPECT_EQ(token, pref_service()->GetString(kActiveRegistrationToken));
  EXPECT_EQ(ids, per_user_topic_registration_manager->GetRegisteredIds());

  for (const auto& id : ids) {
    const base::DictionaryValue* topics =
        pref_service()->GetDictionary(kTypeRegisteredForInvalidation);
    const base::Value* private_topic_value =
        topics->FindKeyOfType(id, base::Value::Type::STRING);
    ASSERT_NE(private_topic_value, nullptr);
    std::string private_topic;
    private_topic_value->GetAsString(&private_topic);
    EXPECT_EQ(private_topic, "new-token-topic");
  }
}

}  // namespace syncer
