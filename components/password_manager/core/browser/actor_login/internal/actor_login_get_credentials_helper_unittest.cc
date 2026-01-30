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
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace actor_login {

namespace {

class FakeStatus : public ActorLoginCredentialsFetcher::Status {
 public:
  explicit FakeStatus(std::optional<ActorLoginError> error) : error_(error) {}
  std::optional<ActorLoginError> GetGlobalError() const override {
    return error_;
  }

 private:
  std::optional<ActorLoginError> error_;
};

class FakeCredentialsFetcher : public ActorLoginCredentialsFetcher {
 public:
  explicit FakeCredentialsFetcher(
      std::vector<Credential> credentials,
      std::optional<ActorLoginError> error = std::nullopt)
      : credentials_(std::move(credentials)), error_(error) {}

  void Fetch(FetchResultCallback callback) override {
    std::unique_ptr<Status> status =
        error_ ? std::make_unique<FakeStatus>(error_) : nullptr;
    base::SequencedTaskRunner::GetCurrentDefault()->PostTask(
        FROM_HERE, base::BindOnce(std::move(callback), std::move(credentials_),
                                  std::move(status)));
  }

 private:
  std::vector<Credential> credentials_;
  std::optional<ActorLoginError> error_;
};

}  // namespace

class ActorLoginGetCredentialsHelperTest : public ::testing::Test {
 public:
  ActorLoginGetCredentialsHelperTest() = default;

 protected:
  base::test::TaskEnvironment task_environment_;
};

TEST_F(ActorLoginGetCredentialsHelperTest, GetCredentialsEmpty) {
  base::test::TestFuture<CredentialsOrError> future;
  std::vector<std::unique_ptr<ActorLoginCredentialsFetcher>> fetchers;
  fetchers.push_back(
      std::make_unique<FakeCredentialsFetcher>(std::vector<Credential>()));
  fetchers.push_back(
      std::make_unique<FakeCredentialsFetcher>(std::vector<Credential>()));

  auto helper = std::make_unique<ActorLoginGetCredentialsHelper>(
      std::move(fetchers), future.GetCallback());

  ASSERT_TRUE(future.Wait());
  ASSERT_TRUE(future.Get().has_value());
  EXPECT_TRUE(future.Get().value().empty());
}

TEST_F(ActorLoginGetCredentialsHelperTest, GetCredentialsFromMultipleFetchers) {
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
      std::move(fetchers), future.GetCallback());

  ASSERT_TRUE(future.Wait());
  ASSERT_TRUE(future.Get().has_value());
  EXPECT_THAT(future.Get().value(),
              testing::UnorderedElementsAre(user1, user2));
}

TEST_F(ActorLoginGetCredentialsHelperTest,
       GetCredentialsReturnsAllCredentialsForSameUsername) {
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
      std::move(fetchers), future.GetCallback());

  ASSERT_TRUE(future.Wait());
  ASSERT_TRUE(future.Get().has_value());
  EXPECT_THAT(
      future.Get().value(),
      testing::UnorderedElementsAre(password_credential, federated_credential));
}

TEST_F(ActorLoginGetCredentialsHelperTest,
       GetCredentialsReturnsAllCredentials) {
  std::vector<Credential> credentials;
  // User 1: Password only
  Credential user1_pass;
  user1_pass.type = CredentialType::kPassword;
  user1_pass.username = u"user1";
  credentials.push_back(user1_pass);
  // User 2: Federated only
  Credential user2_fed;
  user2_fed.type = CredentialType::kFederated;
  user2_fed.username = u"user2";
  credentials.push_back(user2_fed);
  // User 3: Both
  Credential user3_pass;
  user3_pass.type = CredentialType::kPassword;
  user3_pass.username = u"user3";
  credentials.push_back(user3_pass);
  Credential user3_fed;
  user3_fed.type = CredentialType::kFederated;
  user3_fed.username = u"user3";
  credentials.push_back(user3_fed);

  base::test::TestFuture<CredentialsOrError> future;
  std::vector<std::unique_ptr<ActorLoginCredentialsFetcher>> fetchers;
  fetchers.push_back(std::make_unique<FakeCredentialsFetcher>(credentials));
  auto helper = std::make_unique<ActorLoginGetCredentialsHelper>(
      std::move(fetchers), future.GetCallback());

  ASSERT_TRUE(future.Wait());
  ASSERT_TRUE(future.Get().has_value());
  EXPECT_THAT(future.Get().value(),
              testing::UnorderedElementsAre(user1_pass, user2_fed, user3_pass,
                                            user3_fed));
}

TEST_F(ActorLoginGetCredentialsHelperTest,
       GetCredentialsReturnsAllCredentialsFromDifferentFetchers) {
  // Fetcher 1 has Password for "user1"
  std::vector<Credential> credentials1;
  Credential user1_pass;
  user1_pass.type = CredentialType::kPassword;
  user1_pass.username = u"user1";
  credentials1.push_back(user1_pass);
  // Fetcher 2 has Federated for "user1"
  std::vector<Credential> credentials2;
  Credential user1_fed;
  user1_fed.type = CredentialType::kFederated;
  user1_fed.username = u"user1";
  credentials2.push_back(user1_fed);

  base::test::TestFuture<CredentialsOrError> future;
  std::vector<std::unique_ptr<ActorLoginCredentialsFetcher>> fetchers;
  fetchers.push_back(std::make_unique<FakeCredentialsFetcher>(credentials1));
  fetchers.push_back(std::make_unique<FakeCredentialsFetcher>(credentials2));
  auto helper = std::make_unique<ActorLoginGetCredentialsHelper>(
      std::move(fetchers), future.GetCallback());

  ASSERT_TRUE(future.Wait());
  ASSERT_TRUE(future.Get().has_value());
  EXPECT_THAT(future.Get().value(),
              testing::UnorderedElementsAre(user1_pass, user1_fed));
}

// This test case only makes sense as long as we treat "filling not allowed" in
// the password fetcher as a global error.
TEST_F(ActorLoginGetCredentialsHelperTest,
       GetCredentialsReturnsErrorIfFetcherReturnsGlobalError) {
  std::vector<Credential> credentials;
  Credential user1;
  user1.type = CredentialType::kPassword;
  user1.username = u"user1";
  credentials.push_back(user1);

  base::test::TestFuture<CredentialsOrError> future;
  // First fetcher returns success.
  // Second fetcher returns error.
  std::vector<std::unique_ptr<ActorLoginCredentialsFetcher>> fetchers;
  fetchers.push_back(std::make_unique<FakeCredentialsFetcher>(credentials));
  fetchers.push_back(std::make_unique<FakeCredentialsFetcher>(
      std::vector<Credential>(), ActorLoginError::kFillingNotAllowed));

  auto helper = std::make_unique<ActorLoginGetCredentialsHelper>(
      std::move(fetchers), future.GetCallback());

  ASSERT_TRUE(future.Wait());
  ASSERT_FALSE(future.Get().has_value());
  EXPECT_EQ(future.Get().error(), ActorLoginError::kFillingNotAllowed);
}

}  // namespace actor_login
