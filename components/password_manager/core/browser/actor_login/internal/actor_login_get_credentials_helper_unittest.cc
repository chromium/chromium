// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/password_manager/core/browser/actor_login/internal/actor_login_get_credentials_helper.h"

#include "base/test/task_environment.h"
#include "base/test/test_future.h"
#include "components/autofill/core/common/autofill_test_utils.h"
#include "components/autofill/core/common/form_data.h"
#include "components/os_crypt/sync/os_crypt_mocker.h"
#include "components/password_manager/core/browser/actor_login/test/actor_login_test_util.h"
#include "components/password_manager/core/browser/fake_form_fetcher.h"
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

namespace {

class FakePasswordManagerClient
    : public password_manager::StubPasswordManagerClient {
 public:
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
    // Used by `PasswordFormManager`.
    OSCryptMocker::SetUp();

    client_.profile_store()->Init(/*affiliated_match_helper=*/nullptr);
    client_.account_store()->Init(/*affiliated_match_helper=*/nullptr);
    ON_CALL(password_manager_, GetPasswordFormCache())
        .WillByDefault(testing::Return(&form_cache_));
    ON_CALL(password_manager_, GetClient())
        .WillByDefault(testing::Return(&client_));
  }

  void TearDown() override {
    client_.profile_store()->ShutdownOnUIThread();
    client_.account_store()->ShutdownOnUIThread();

    OSCryptMocker::TearDown();
  }

 protected:
  FakePasswordManagerClient* client() { return &client_; }
  password_manager::MockPasswordManager* password_manager() {
    return &password_manager_;
  }
  testing::NiceMock<password_manager::MockPasswordFormCache> form_cache_;

 private:
  base::test::TaskEnvironment task_environment_;
  autofill::test::AutofillUnitTestEnvironment autofill_test_environment_{
      {.disable_server_communication = true}};
  FakePasswordManagerClient client_;
  testing::NiceMock<password_manager::MockPasswordManager> password_manager_;
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
  password_manager::PasswordForm form1;
  form1.url = GURL("https://foo.com");
  form1.signon_realm = form1.url.spec();
  form1.username_value = u"foo_username";
  form1.password_value = u"foo_password";
  client()->profile_store()->AddLogin(form1);

  password_manager::PasswordForm form2;
  form2.url = GURL("https://bar.com");
  form2.signon_realm = form2.url.spec();
  form2.username_value = u"bar_username";
  form2.password_value = u"bar_password";
  client()->account_store()->AddLogin(form2);

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
}

TEST_F(ActorLoginGetCredentialsHelperTest, GetCredentialsFromAllStores) {
  password_manager::PasswordForm form1;
  form1.url = GURL("https://foo.com");
  form1.signon_realm = form1.url.spec();
  form1.username_value = u"foo_username";
  form1.password_value = u"foo_password";
  client()->profile_store()->AddLogin(form1);

  password_manager::PasswordForm form2;
  form2.url = GURL("https://foo.com");
  form2.signon_realm = form2.url.spec();
  form2.username_value = u"bar_username";
  form2.password_value = u"bar_password";
  client()->account_store()->AddLogin(form2);

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
              testing::UnorderedElementsAre(u"foo_username", u"bar_username"));
}

TEST_F(ActorLoginGetCredentialsHelperTest, ImmediatelyAvailableToLogin) {
  const GURL kUrl("https://foo.com");
  const url::Origin kOrigin = url::Origin::Create(kUrl);

  password_manager::PasswordForm saved_form;
  saved_form.url = kUrl;
  saved_form.signon_realm = kUrl.spec();
  saved_form.username_value = u"foo_username";
  saved_form.password_value = u"foo_password";
  saved_form.match_type = password_manager::PasswordForm::MatchType::kExact;
  client()->profile_store()->AddLogin(saved_form);

  // To make GetSigninFormManager return a non-nullptr value, we need to
  // populate the PasswordFormCache with a PasswordFormManager that represents
  // a sign-in form.
  testing::NiceMock<MockPasswordManagerDriver> driver;
  ON_CALL(driver, GetLastCommittedOrigin)
      .WillByDefault(testing::ReturnRef(kOrigin));
  ON_CALL(driver, IsInPrimaryMainFrame).WillByDefault(testing::Return(true));
  ON_CALL(driver, GetPasswordManager)
      .WillByDefault(testing::Return(password_manager()));

  autofill::FormData form_data = actor_login::CreateSigninFormData(kUrl);
  password_manager::FakeFormFetcher form_fetcher;
  form_fetcher.SetBestMatches({saved_form});

  auto form_manager = std::make_unique<password_manager::PasswordFormManager>(
      client(), driver.AsWeakPtr(), form_data, &form_fetcher,
      std::make_unique<password_manager::PasswordSaveManagerImpl>(client()),
      /*metrics_recorder=*/nullptr);
  // Force form parsing, otherwise there will be no parsed observed form.
  form_manager->DisableFillingServerPredictionsForTesting();
  form_fetcher.NotifyFetchCompleted();

  std::vector<std::unique_ptr<password_manager::PasswordFormManager>>
      form_managers;
  form_managers.push_back(std::move(form_manager));

  EXPECT_CALL(form_cache_, GetFormManagers)
      .WillOnce(testing::Return(base::span(form_managers)));

  base::test::TestFuture<CredentialsOrError> future;
  ActorLoginGetCredentialsHelper helper(url::Origin::Create(kUrl), client(),
                                        password_manager(),
                                        future.GetCallback());

  // `FakeFormFetcher::AddConsumer` implementation differs from production,
  // therefore additional manual call to NotifyFetchCompleted is needed
  // after helper above gets registered as observer of `FakeFormFetcher`.
  // Otherwise helper will never know that `FakeFormFetcher` already fetched
  // credentials and this test will crash.
  form_fetcher.NotifyFetchCompleted();

  ASSERT_TRUE(future.Get().has_value());
  const auto& credentials = future.Get().value();
  ASSERT_EQ(credentials.size(), 1u);
  EXPECT_EQ(credentials[0].username, u"foo_username");
  EXPECT_TRUE(credentials[0].immediatelyAvailableToLogin);
}

}  // namespace actor_login
