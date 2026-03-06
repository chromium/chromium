// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/password_manager/core/browser/actor_login/internal/actor_login_get_credentials_helper.h"

#include <memory>
#include <optional>
#include <string>

#include "base/memory/weak_ptr.h"
#include "base/run_loop.h"
#include "base/task/sequenced_task_runner.h"
#include "base/test/gmock_callback_support.h"
#include "base/test/run_until.h"
#include "base/test/task_environment.h"
#include "base/test/test_future.h"
#include "components/password_manager/core/browser/actor_login/actor_login_types.h"
#include "components/password_manager/core/browser/actor_login/internal/actor_login_metrics_helper_interface.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace actor_login {

namespace {

class MockActorLoginMetricsHelper : public ActorLoginMetricsHelperInterface {
 public:
  MOCK_METHOD(void, RecordDeduplicationOccurred, (bool), (override));
};

class FakeCredentialsFetcher : public ActorLoginCredentialsFetcher {
 public:
  explicit FakeCredentialsFetcher(
      std::vector<Credential> credentials,
      ActorLoginCredentialsFetcher::Status status =
          ActorLoginCredentialsFetcher::Status::kSuccess)
      : credentials_(std::move(credentials)), status_(status) {}

  void Fetch(FetchResultCallback callback) override {
    base::SequencedTaskRunner::GetCurrentDefault()->PostTask(
        FROM_HERE, base::BindOnce(std::move(callback), std::move(credentials_),
                                  std::move(status_)));
  }

 private:
  std::vector<Credential> credentials_;
  ActorLoginCredentialsFetcher::Status status_;
};

}  // namespace

class ActorLoginGetCredentialsHelperTest : public ::testing::Test {
 public:
  ActorLoginGetCredentialsHelperTest() = default;

 protected:
  base::test::TaskEnvironment task_environment_;
  testing::NiceMock<MockActorLoginMetricsHelper> metrics_helper_;
};

TEST_F(ActorLoginGetCredentialsHelperTest,
       TwoFetchers_BothReturnEmptyCredentials_ReturnsEmptyCredentials) {
  base::test::TestFuture<CredentialsOrError> future;
  std::vector<std::unique_ptr<ActorLoginCredentialsFetcher>> fetchers;
  fetchers.push_back(
      std::make_unique<FakeCredentialsFetcher>(std::vector<Credential>()));
  fetchers.push_back(
      std::make_unique<FakeCredentialsFetcher>(std::vector<Credential>()));

  auto helper = std::make_unique<ActorLoginGetCredentialsHelper>(
      std::move(fetchers), &metrics_helper_, future.GetCallback());

  ASSERT_TRUE(future.Wait());
  ASSERT_TRUE(future.Get().has_value());
  EXPECT_TRUE(future.Get().value().empty());
}

TEST_F(ActorLoginGetCredentialsHelperTest,
       TwoFetchers_BothReturnDifferentCredentials_ReturnsAllCredentials) {
  std::vector<Credential> credentials1;
  Credential user1;
  user1.type = CredentialType::kPassword;
  user1.username = u"user1";
  credentials1.push_back(user1);
  std::vector<Credential> credentials2;
  Credential user2;
  user2.type = CredentialType::kFederated;
  user2.username = u"user2";
  credentials2.push_back(user2);

  base::test::TestFuture<CredentialsOrError> future;
  std::vector<std::unique_ptr<ActorLoginCredentialsFetcher>> fetchers;
  fetchers.push_back(std::make_unique<FakeCredentialsFetcher>(credentials1));
  fetchers.push_back(std::make_unique<FakeCredentialsFetcher>(credentials2));
  auto helper = std::make_unique<ActorLoginGetCredentialsHelper>(
      std::move(fetchers), &metrics_helper_, future.GetCallback());

  ASSERT_TRUE(future.Wait());
  ASSERT_TRUE(future.Get().has_value());
  // Federated credentials come first.
  EXPECT_THAT(future.Get().value(), testing::ElementsAre(user2, user1));
}

TEST_F(ActorLoginGetCredentialsHelperTest,
       OneFetcher_ReturnsMultipleCredentials_MergesOnSameUsername) {
  std::vector<Credential> credentials;
  Credential password_credential;
  password_credential.type = CredentialType::kPassword;
  password_credential.username = u"user";
  credentials.push_back(password_credential);
  Credential federated_credential;
  federated_credential.type = CredentialType::kFederated;
  federated_credential.username = u"user";
  credentials.push_back(federated_credential);

  base::test::TestFuture<CredentialsOrError> future;
  std::vector<std::unique_ptr<ActorLoginCredentialsFetcher>> fetchers;
  fetchers.push_back(std::make_unique<FakeCredentialsFetcher>(credentials));
  auto helper = std::make_unique<ActorLoginGetCredentialsHelper>(
      std::move(fetchers), &metrics_helper_, future.GetCallback());

  ASSERT_TRUE(future.Wait());
  ASSERT_TRUE(future.Get().has_value());
  EXPECT_THAT(future.Get().value(), testing::ElementsAre(federated_credential));
}

TEST_F(
    ActorLoginGetCredentialsHelperTest,
    OneFetcher_ReturnsMultipleCredentials_MergesOnSameUsernameAndKeepsOtherUsernames) {
  std::vector<Credential> credentials;
  // User 1: Password only
  Credential user1_password;
  user1_password.type = CredentialType::kPassword;
  user1_password.username = u"user1";
  credentials.push_back(user1_password);
  // User 2: Federated only
  Credential user2_federated;
  user2_federated.type = CredentialType::kFederated;
  user2_federated.username = u"user2";
  credentials.push_back(user2_federated);
  // User 3: Both
  Credential user3_password;
  user3_password.type = CredentialType::kPassword;
  user3_password.username = u"user3";
  credentials.push_back(user3_password);
  Credential user3_federated;
  user3_federated.type = CredentialType::kFederated;
  user3_federated.username = u"user3";
  credentials.push_back(user3_federated);

  base::test::TestFuture<CredentialsOrError> future;
  std::vector<std::unique_ptr<ActorLoginCredentialsFetcher>> fetchers;
  fetchers.push_back(std::make_unique<FakeCredentialsFetcher>(credentials));
  auto helper = std::make_unique<ActorLoginGetCredentialsHelper>(
      std::move(fetchers), &metrics_helper_, future.GetCallback());

  ASSERT_TRUE(future.Wait());
  ASSERT_TRUE(future.Get().has_value());
  // Federated credentials come first.
  EXPECT_THAT(
      future.Get().value(),
      testing::ElementsAre(user2_federated, user3_federated, user1_password));
}

TEST_F(ActorLoginGetCredentialsHelperTest,
       TwoFetchers_BothReturnCredentials_MergesOnSameUsername) {
  // Fetcher 1 has Password for "user1"
  std::vector<Credential> credentials1;
  Credential user1_password;
  user1_password.type = CredentialType::kPassword;
  user1_password.username = u"user1";
  credentials1.push_back(user1_password);
  // Fetcher 2 has Federated for "user1"
  std::vector<Credential> credentials2;
  Credential user1_federated;
  user1_federated.type = CredentialType::kFederated;
  user1_federated.username = u"user1";
  credentials2.push_back(user1_federated);

  base::test::TestFuture<CredentialsOrError> future;
  std::vector<std::unique_ptr<ActorLoginCredentialsFetcher>> fetchers;
  fetchers.push_back(std::make_unique<FakeCredentialsFetcher>(credentials1));
  fetchers.push_back(std::make_unique<FakeCredentialsFetcher>(credentials2));
  auto helper = std::make_unique<ActorLoginGetCredentialsHelper>(
      std::move(fetchers), &metrics_helper_, future.GetCallback());

  ASSERT_TRUE(future.Wait());
  ASSERT_TRUE(future.Get().has_value());
  EXPECT_THAT(future.Get().value(), testing::ElementsAre(user1_federated));
}

TEST_F(ActorLoginGetCredentialsHelperTest,
       TwoFetchers_BothReturnCredentials_MergesPermissionsForSameUsername) {
  std::vector<Credential> credentials;
  Credential user1_no_permission;
  user1_no_permission.username = u"user1";
  user1_no_permission.has_persistent_permission = false;
  credentials.push_back(user1_no_permission);

  Credential user1_permission;
  user1_permission.username = u"user1";
  user1_permission.has_persistent_permission = true;
  credentials.push_back(user1_permission);

  base::test::TestFuture<CredentialsOrError> future;
  std::vector<std::unique_ptr<ActorLoginCredentialsFetcher>> fetchers;
  fetchers.push_back(std::make_unique<FakeCredentialsFetcher>(credentials));
  auto helper = std::make_unique<ActorLoginGetCredentialsHelper>(
      std::move(fetchers), &metrics_helper_, future.GetCallback());

  ASSERT_TRUE(future.Wait());
  ASSERT_TRUE(future.Get().has_value());
  ASSERT_EQ(future.Get().value().size(), 1u);
  EXPECT_TRUE(future.Get().value()[0].has_persistent_permission);
}

TEST_F(
    ActorLoginGetCredentialsHelperTest,
    OneFetcher_ReturnsMultipleCredentials_OnePermission_ReturnsOnlyCredentialWithPermission) {
  std::vector<Credential> credentials;
  Credential user1;
  user1.username = u"user1";
  user1.has_persistent_permission = true;
  credentials.push_back(user1);

  Credential user2;
  user2.username = u"user2";
  user2.has_persistent_permission = false;
  credentials.push_back(user2);

  base::test::TestFuture<CredentialsOrError> future;
  std::vector<std::unique_ptr<ActorLoginCredentialsFetcher>> fetchers;
  fetchers.push_back(std::make_unique<FakeCredentialsFetcher>(credentials));
  auto helper = std::make_unique<ActorLoginGetCredentialsHelper>(
      std::move(fetchers), &metrics_helper_, future.GetCallback());

  ASSERT_TRUE(future.Wait());
  ASSERT_TRUE(future.Get().has_value());
  EXPECT_THAT(future.Get().value(), testing::ElementsAre(user1));
}

TEST_F(
    ActorLoginGetCredentialsHelperTest,
    OneFetcher_ReturnsMultipleCredentials_MultiplePermissions_DropsAllPermissions) {
  std::vector<Credential> credentials;
  Credential user1;
  user1.username = u"user1";
  user1.has_persistent_permission = true;
  credentials.push_back(user1);

  Credential user2;
  user2.username = u"user2";
  user2.has_persistent_permission = true;
  credentials.push_back(user2);

  base::test::TestFuture<CredentialsOrError> future;
  std::vector<std::unique_ptr<ActorLoginCredentialsFetcher>> fetchers;
  fetchers.push_back(std::make_unique<FakeCredentialsFetcher>(credentials));
  auto helper = std::make_unique<ActorLoginGetCredentialsHelper>(
      std::move(fetchers), &metrics_helper_, future.GetCallback());

  ASSERT_TRUE(future.Wait());
  ASSERT_TRUE(future.Get().has_value());
  EXPECT_EQ(future.Get().value().size(), 2u);
  for (const Credential& credential : future.Get().value()) {
    EXPECT_FALSE(credential.has_persistent_permission);
  }
}

// This test case only makes sense as long as we treat "filling not allowed" in
// the password fetcher as a global error.
TEST_F(
    ActorLoginGetCredentialsHelperTest,
    TwoFetchers_OneReturnsFillingNotAllowed_OneReturnsEmptyCredentials_ReturnsError) {
  base::test::TestFuture<CredentialsOrError> future;
  // First fetcher returns success.
  // Second fetcher returns error.
  std::vector<std::unique_ptr<ActorLoginCredentialsFetcher>> fetchers;
  fetchers.push_back(
      std::make_unique<FakeCredentialsFetcher>(std::vector<Credential>()));
  fetchers.push_back(std::make_unique<FakeCredentialsFetcher>(
      std::vector<Credential>(),
      ActorLoginCredentialsFetcher::Status::kFillingNotAllowed));

  auto helper = std::make_unique<ActorLoginGetCredentialsHelper>(
      std::move(fetchers), &metrics_helper_, future.GetCallback());

  ASSERT_TRUE(future.Wait());
  ASSERT_FALSE(future.Get().has_value());
  EXPECT_EQ(future.Get().error(), ActorLoginError::kFillingNotAllowed);
}

TEST_F(
    ActorLoginGetCredentialsHelperTest,
    TwoFetchers_OneReturnsFillingNotAllowed_OneReturnsCredentials_ReturnsCredentials) {
  std::vector<Credential> credentials;
  Credential user1;
  user1.type = CredentialType::kPassword;
  user1.username = u"user1";
  credentials.push_back(user1);

  base::test::TestFuture<CredentialsOrError> future;
  // First fetcher returns success with credentials.
  // Second fetcher returns error.
  std::vector<std::unique_ptr<ActorLoginCredentialsFetcher>> fetchers;
  fetchers.push_back(std::make_unique<FakeCredentialsFetcher>(credentials));
  fetchers.push_back(std::make_unique<FakeCredentialsFetcher>(
      std::vector<Credential>(),
      ActorLoginCredentialsFetcher::Status::kFillingNotAllowed));

  auto helper = std::make_unique<ActorLoginGetCredentialsHelper>(
      std::move(fetchers), &metrics_helper_, future.GetCallback());

  ASSERT_TRUE(future.Wait());
  ASSERT_TRUE(future.Get().has_value());
  EXPECT_THAT(future.Get().value(), testing::UnorderedElementsAre(user1));
}

TEST_F(ActorLoginGetCredentialsHelperTest,
       OneFetcher_ReturnsMultipleCredentials_RanksCredentials) {
  std::vector<Credential> credentials;
  Credential user1;
  user1.type = CredentialType::kPassword;
  user1.username = u"user1";
  credentials.push_back(user1);

  Credential user2;
  user2.type = CredentialType::kPassword;
  user2.username = u"user2";
  credentials.push_back(user2);

  Credential user3;
  user3.type = CredentialType::kFederated;
  user3.username = u"user3";
  credentials.push_back(user3);

  Credential user4;
  user4.type = CredentialType::kFederated;
  user4.username = u"user4";
  credentials.push_back(user4);

  base::test::TestFuture<CredentialsOrError> future;
  std::vector<std::unique_ptr<ActorLoginCredentialsFetcher>> fetchers;
  fetchers.push_back(std::make_unique<FakeCredentialsFetcher>(credentials));
  auto helper = std::make_unique<ActorLoginGetCredentialsHelper>(
      std::move(fetchers), &metrics_helper_, future.GetCallback());

  ASSERT_TRUE(future.Wait());
  ASSERT_TRUE(future.Get().has_value());
  // Federated credentials come first and maintain their relative order.
  // Password credentials come after and maintain their relative order.
  EXPECT_THAT(future.Get().value(),
              testing::ElementsAre(user3, user4, user1, user2));
}

TEST_F(ActorLoginGetCredentialsHelperTest, RecordDeduplicationMetrics) {
  std::vector<Credential> credentials;
  Credential user1_password;
  user1_password.type = CredentialType::kPassword;
  user1_password.username = u"user1";
  credentials.push_back(user1_password);
  Credential user1_federated;
  user1_federated.type = CredentialType::kFederated;
  user1_federated.username = u"user1";
  credentials.push_back(user1_federated);

  EXPECT_CALL(metrics_helper_, RecordDeduplicationOccurred(true)).Times(1);

  base::test::TestFuture<CredentialsOrError> future;
  std::vector<std::unique_ptr<ActorLoginCredentialsFetcher>> fetchers;
  fetchers.push_back(std::make_unique<FakeCredentialsFetcher>(credentials));
  auto helper = std::make_unique<ActorLoginGetCredentialsHelper>(
      std::move(fetchers), &metrics_helper_, future.GetCallback());

  ASSERT_TRUE(future.Wait());
}

}  // namespace actor_login
