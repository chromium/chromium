// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/password_manager/core/browser/actor_login/internal/actor_login_get_credentials_helper.h"

#include <memory>
#include <optional>
#include <string>

#include "base/test/task_environment.h"
#include "base/test/test_future.h"
#include "components/autofill/core/common/autofill_test_utils.h"
#include "components/autofill/core/common/form_data.h"
#include "components/password_manager/core/browser/actor_login/test/actor_login_test_util.h"
#include "components/password_manager/core/browser/fake_form_fetcher.h"
#include "components/password_manager/core/browser/form_fetcher.h"
#include "components/password_manager/core/browser/mock_password_form_cache.h"
#include "components/password_manager/core/browser/mock_password_manager.h"
#include "components/password_manager/core/browser/password_form.h"
#include "components/password_manager/core/browser/password_form_manager.h"
#include "components/password_manager/core/browser/password_save_manager_impl.h"
#include "components/password_manager/core/browser/password_store/test_password_store.h"
#include "components/password_manager/core/browser/stub_password_manager_client.h"
#include "components/password_manager/core/browser/stub_password_manager_driver.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "url/gurl.h"

namespace actor_login {

using password_manager::PasswordForm;
using password_manager::PasswordFormManager;
using testing::NiceMock;
using testing::Return;
using testing::ReturnRef;
using testing::UnorderedElementsAre;

namespace {

class FakePasswordManagerClient
    : public password_manager::StubPasswordManagerClient {
 public:
  MOCK_METHOD(password_manager::PasswordManagerInterface*,
              GetPasswordManager,
              (),
              (override, const));

  FakePasswordManagerClient() {
    profile_store_ = base::MakeRefCounted<password_manager::TestPasswordStore>(
        password_manager::IsAccountStore(false));
    account_store_ = base::MakeRefCounted<password_manager::TestPasswordStore>(
        password_manager::IsAccountStore(true));
  }
  ~FakePasswordManagerClient() override = default;

  scoped_refptr<password_manager::TestPasswordStore> profile_store() {
    return profile_store_;
  }
  scoped_refptr<password_manager::TestPasswordStore> account_store() {
    return account_store_;
  }

 private:
  // PasswordManagerClient:
  password_manager::PasswordStoreInterface* GetProfilePasswordStore()
      const override {
    return profile_store_.get();
  }
  password_manager::PasswordStoreInterface* GetAccountPasswordStore()
      const override {
    return account_store_.get();
  }
  scoped_refptr<password_manager::TestPasswordStore> profile_store_;
  scoped_refptr<password_manager::TestPasswordStore> account_store_;
};

class MockPasswordManagerDriver
    : public password_manager::StubPasswordManagerDriver {
 public:
  MOCK_METHOD(const url::Origin&,
              GetLastCommittedOrigin,
              (),
              (const, override));
  MOCK_METHOD(bool, IsInPrimaryMainFrame, (), (const, override));
  MOCK_METHOD(password_manager::PasswordManagerInterface*,
              GetPasswordManager,
              (),
              (override));
};
}  // namespace

class ActorLoginGetCredentialsHelperTest : public ::testing::Test {
 public:
  ActorLoginGetCredentialsHelperTest() = default;

  void SetUp() override {
    client_.profile_store()->Init(/*affiliated_match_helper=*/nullptr);
    client_.account_store()->Init(/*affiliated_match_helper=*/nullptr);
    ON_CALL(password_manager_, GetPasswordFormCache())
        .WillByDefault(Return(&form_cache_));
    ON_CALL(password_manager_, GetClient()).WillByDefault(Return(&client_));
    ON_CALL(client_, GetPasswordManager)
        .WillByDefault(Return(&password_manager_));
  }

  void TearDown() override {
    client_.profile_store()->ShutdownOnUIThread();
    client_.account_store()->ShutdownOnUIThread();
  }

 protected:
  FakePasswordManagerClient* client() { return &client_; }
  password_manager::FakeFormFetcher* form_fetcher() { return &form_fetcher_; }
  password_manager::MockPasswordManager* password_manager() {
    return &password_manager_;
  }
  NiceMock<MockPasswordManagerDriver>& driver() { return driver_; }
  NiceMock<password_manager::MockPasswordFormCache>& form_cache() {
    return form_cache_;
  }

  std::unique_ptr<PasswordFormManager> CreateFormManager() {
    return CreateFormManager(kOrigin,
                             /*is_in_main_frame=*/true,
                             actor_login::CreateSigninFormData(kUrl), client(),
                             driver(), form_fetcher());
  }

  std::unique_ptr<PasswordFormManager> CreateFormManager(
      const url::Origin& origin,
      bool is_in_main_frame,
      const autofill::FormData& form_data,
      password_manager::PasswordManagerClient* client,
      MockPasswordManagerDriver& driver,
      password_manager::FormFetcher* form_fetcher) {
    ON_CALL(driver, GetLastCommittedOrigin).WillByDefault(ReturnRef(origin));
    ON_CALL(driver, IsInPrimaryMainFrame)
        .WillByDefault(Return(is_in_main_frame));
    ON_CALL(driver, GetPasswordManager)
        .WillByDefault(Return(password_manager()));

    auto form_manager = std::make_unique<PasswordFormManager>(
        client, driver.AsWeakPtr(), form_data, form_fetcher,
        std::make_unique<password_manager::PasswordSaveManagerImpl>(client),
        /*metrics_recorder=*/nullptr);
    form_manager->DisableFillingServerPredictionsForTesting();
    return form_manager;
  }

  PasswordForm CreatePasswordForm(
      const std::string& url,
      const std::u16string& username,
      const std::u16string& password,
      PasswordForm::MatchType match_type = PasswordForm::MatchType::kExact) {
    PasswordForm form;
    form.url = GURL(url);
    form.signon_realm = form.url.spec();
    form.username_value = username;
    form.password_value = password;
    form.match_type = match_type;
    return form;
  }

  void AddFormManager(std::unique_ptr<PasswordFormManager> manager) {
    form_managers_.push_back(std::move(manager));

    ON_CALL(form_cache_, GetFormManagers)
        .WillByDefault(Return(base::span(form_managers_)));
  }

  void SetBestMatches(std::vector<PasswordForm> best_matches) {
    form_fetcher_.SetBestMatches(best_matches);
    form_fetcher_.NotifyFetchCompleted();
  }

  const GURL kUrl = GURL("https://foo.com");
  const url::Origin kOrigin = url::Origin::Create(kUrl);

 private:
  base::test::TaskEnvironment task_environment_;
  autofill::test::AutofillUnitTestEnvironment autofill_test_environment_{
      {.disable_server_communication = true}};
  FakePasswordManagerClient client_;
  NiceMock<password_manager::MockPasswordManager> password_manager_;
  password_manager::FakeFormFetcher form_fetcher_;
  NiceMock<MockPasswordManagerDriver> driver_;
  std::vector<std::unique_ptr<PasswordFormManager>> form_managers_;
  NiceMock<password_manager::MockPasswordFormCache> form_cache_;
};

TEST_F(ActorLoginGetCredentialsHelperTest, GetCredentialsSuccess) {
  base::test::TestFuture<CredentialsOrError> future;
  ActorLoginGetCredentialsHelper helper(
      url::Origin::Create(GURL("https://example.com")), client(),
      password_manager(), future.GetCallback());

  ASSERT_TRUE(future.Get().has_value());
  EXPECT_TRUE(future.Get().value().empty());
}

TEST_F(ActorLoginGetCredentialsHelperTest, GetCredentialsFiltersByDomain) {
  client()->profile_store()->AddLogin(
      CreatePasswordForm("https://foo.com", u"foo_username", u"foo_password"));
  client()->account_store()->AddLogin(
      CreatePasswordForm("https://bar.com", u"bar_username", u"bar_password"));

  base::test::TestFuture<CredentialsOrError> future;
  ActorLoginGetCredentialsHelper helper(
      url::Origin::Create(GURL("https://foo.com")), client(),
      password_manager(), future.GetCallback());

  ASSERT_TRUE(future.Get().has_value());
  const auto& credentials = future.Get().value();
  ASSERT_EQ(credentials.size(), 1u);
  EXPECT_EQ(credentials[0].username, u"foo_username");
  EXPECT_EQ(future.Get().value()[0].type, kPassword);
  EXPECT_EQ(credentials[0].source_site_or_app, u"https://foo.com/");
  EXPECT_EQ(future.Get().value()[0].request_origin,
            url::Origin::Create(GURL("https://foo.com")));
  EXPECT_FALSE(credentials[0].immediatelyAvailableToLogin);
  EXPECT_FALSE(credentials[0].has_persistent_permission);
}

TEST_F(ActorLoginGetCredentialsHelperTest, GetCredentialsFromAllStores) {
  client()->profile_store()->AddLogin(
      CreatePasswordForm("https://foo.com", u"foo_username", u"foo_password"));
  client()->account_store()->AddLogin(
      CreatePasswordForm("https://foo.com", u"bar_username", u"bar_password"));

  base::test::TestFuture<CredentialsOrError> future;
  ActorLoginGetCredentialsHelper helper(
      url::Origin::Create(GURL("https://foo.com")), client(),
      password_manager(), future.GetCallback());

  ASSERT_TRUE(future.Get().has_value());
  const auto& credentials = future.Get().value();
  ASSERT_EQ(credentials.size(), 2u);

  std::vector<std::u16string> usernames;
  for (const auto& credential : credentials) {
    usernames.push_back(credential.username);
  }
  EXPECT_THAT(usernames,
              UnorderedElementsAre(u"foo_username", u"bar_username"));
}

TEST_F(ActorLoginGetCredentialsHelperTest, ImmediatelyAvailableToLogin) {
  PasswordForm saved_form =
      CreatePasswordForm(kUrl.spec(), u"foo_username", u"foo_password");
  client()->profile_store()->AddLogin(saved_form);
  // To make GetSigninFormManager return a non-nullptr value, we need to
  // populate the PasswordFormCache with a PasswordFormManager that represents
  // a sign-in form.
  AddFormManager(CreateFormManager());
  SetBestMatches({saved_form});

  base::test::TestFuture<CredentialsOrError> future;
  ActorLoginGetCredentialsHelper helper(kOrigin, client(), password_manager(),
                                        future.GetCallback());
  // `FakeFormFetcher::AddConsumer` implementation differs from production,
  // therefore additional manual call to NotifyFetchCompleted is needed
  // after helper above gets registered as observer of `FakeFormFetcher`.
  // Otherwise helper will never know that `FakeFormFetcher` already fetched
  // credentials and this test will crash.
  form_fetcher()->NotifyFetchCompleted();

  ASSERT_TRUE(future.Get().has_value());
  const auto& credentials = future.Get().value();
  ASSERT_EQ(credentials.size(), 1u);
  EXPECT_EQ(credentials[0].username, u"foo_username");
  EXPECT_TRUE(credentials[0].immediatelyAvailableToLogin);
  EXPECT_FALSE(credentials[0].has_persistent_permission);
}

TEST_F(ActorLoginGetCredentialsHelperTest, GetCredentialsPrefersExactMatch) {
  PasswordForm psl_match =
      CreatePasswordForm("https://sub.foo.com", u"psl_username",
                         u"psl_password", PasswordForm::MatchType::kPSL);
  psl_match.actor_login_approved = true;
  PasswordForm affiliated_match = CreatePasswordForm(
      "https://m.foo.com", u"affiliated_username", u"affiliated_password",
      PasswordForm::MatchType::kAffiliated);
  affiliated_match.actor_login_approved = true;
  PasswordForm exact_match =
      CreatePasswordForm(kUrl.spec(), u"exact_username", u"exact_password");
  exact_match.actor_login_approved = true;
  AddFormManager(CreateFormManager());
  SetBestMatches({exact_match, affiliated_match, psl_match});

  base::test::TestFuture<CredentialsOrError> future;
  ActorLoginGetCredentialsHelper helper(kOrigin, client(), password_manager(),
                                        future.GetCallback());
  form_fetcher()->NotifyFetchCompleted();

  ASSERT_TRUE(future.Get().has_value());
  const auto& credentials = future.Get().value();
  ASSERT_EQ(credentials.size(), 1u);
  EXPECT_EQ(credentials[0].username, u"exact_username");
}

TEST_F(ActorLoginGetCredentialsHelperTest,
       GetCredentialsPrefersAffiliatedMatch) {
  PasswordForm psl_match =
      CreatePasswordForm("https://sub.foo.com", u"psl_username",
                         u"psl_password", PasswordForm::MatchType::kPSL);
  psl_match.actor_login_approved = true;
  PasswordForm affiliated_match = CreatePasswordForm(
      "https://m.foo.com", u"affiliated_username", u"affiliated_password",
      PasswordForm::MatchType::kAffiliated);
  affiliated_match.actor_login_approved = true;
  AddFormManager(CreateFormManager());
  SetBestMatches({affiliated_match, psl_match});

  base::test::TestFuture<CredentialsOrError> future;
  ActorLoginGetCredentialsHelper helper(kOrigin, client(), password_manager(),
                                        future.GetCallback());
  form_fetcher()->NotifyFetchCompleted();

  ASSERT_TRUE(future.Get().has_value());
  const auto& credentials = future.Get().value();
  ASSERT_EQ(credentials.size(), 1u);
  EXPECT_EQ(credentials[0].username, u"affiliated_username");
}

TEST_F(ActorLoginGetCredentialsHelperTest,
       GetCredentialsNoApprovedCredentials) {
  PasswordForm psl_match =
      CreatePasswordForm("https://sub.foo.com", u"psl_username",
                         u"psl_password", PasswordForm::MatchType::kPSL);
  PasswordForm affiliated_match = CreatePasswordForm(
      "https://m.foo.com", u"affiliated_username", u"affiliated_password",
      PasswordForm::MatchType::kAffiliated);
  AddFormManager(CreateFormManager());
  SetBestMatches({affiliated_match, psl_match});

  base::test::TestFuture<CredentialsOrError> future;
  ActorLoginGetCredentialsHelper helper(kOrigin, client(), password_manager(),
                                        future.GetCallback());
  form_fetcher()->NotifyFetchCompleted();

  ASSERT_TRUE(future.Get().has_value());
  const auto& credentials = future.Get().value();
  ASSERT_EQ(credentials.size(), 2u);
}

TEST_F(ActorLoginGetCredentialsHelperTest,
       GetCredentialsIgnoresWeakApprovedCredentials) {
  PasswordForm psl_match =
      CreatePasswordForm("https://sub.foo.com", u"psl_username",
                         u"psl_password", PasswordForm::MatchType::kPSL);
  psl_match.actor_login_approved = true;
  PasswordForm affiliated_match = CreatePasswordForm(
      "https://m.foo.com", u"affiliated_username", u"affiliated_password",
      PasswordForm::MatchType::kAffiliated);
  AddFormManager(CreateFormManager());
  SetBestMatches({affiliated_match, psl_match});

  base::test::TestFuture<CredentialsOrError> future;
  ActorLoginGetCredentialsHelper helper(kOrigin, client(), password_manager(),
                                        future.GetCallback());
  form_fetcher()->NotifyFetchCompleted();

  ASSERT_TRUE(future.Get().has_value());
  const auto& credentials = future.Get().value();
  ASSERT_EQ(credentials.size(), 2u);
}

TEST_F(ActorLoginGetCredentialsHelperTest,
       GetCredentialsReturnsSingleApprovedCredential) {
  PasswordForm psl_match =
      CreatePasswordForm("https://sub.foo.com", u"psl_username",
                         u"psl_password", PasswordForm::MatchType::kPSL);
  PasswordForm affiliated_match = CreatePasswordForm(
      "https://m.foo.com", u"affiliated_username", u"affiliated_password",
      PasswordForm::MatchType::kAffiliated);
  affiliated_match.actor_login_approved = true;
  PasswordForm exact_match =
      CreatePasswordForm(kUrl.spec(), u"exact_username", u"exact_password");
  AddFormManager(CreateFormManager());
  // The order is important, as PWM would rank them in this order and we still
  // want to return the affiliated match.
  SetBestMatches({exact_match, affiliated_match, psl_match});

  base::test::TestFuture<CredentialsOrError> future;
  ActorLoginGetCredentialsHelper helper(kOrigin, client(), password_manager(),
                                        future.GetCallback());
  form_fetcher()->NotifyFetchCompleted();

  ASSERT_TRUE(future.Get().has_value());
  const auto& credentials = future.Get().value();
  ASSERT_EQ(credentials.size(), 1u);
  EXPECT_EQ(credentials[0].username, u"affiliated_username");
  EXPECT_TRUE(credentials[0].has_persistent_permission);
}

}  // namespace actor_login
