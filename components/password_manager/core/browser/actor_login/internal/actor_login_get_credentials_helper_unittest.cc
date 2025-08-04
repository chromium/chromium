// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/password_manager/core/browser/actor_login/internal/actor_login_get_credentials_helper.h"

#include "base/test/task_environment.h"
#include "base/test/test_future.h"
#include "components/password_manager/core/browser/password_form.h"
#include "components/password_manager/core/browser/password_store/test_password_store.h"
#include "components/password_manager/core/browser/stub_password_manager_client.h"
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

}  // namespace

class ActorLoginGetCredentialsHelperTest : public ::testing::Test {
 public:
  ActorLoginGetCredentialsHelperTest() = default;

  void SetUp() override {
    client_.profile_store()->Init(/*prefs=*/nullptr,
                                  /*affiliated_match_helper=*/nullptr);
    client_.account_store()->Init(/*prefs=*/nullptr,
                                  /*affiliated_match_helper=*/nullptr);
  }

  void TearDown() override {
    client_.profile_store()->ShutdownOnUIThread();
    client_.account_store()->ShutdownOnUIThread();
  }

 protected:
  FakePasswordManagerClient* client() { return &client_; }

 private:
  base::test::TaskEnvironment task_environment_;
  FakePasswordManagerClient client_;
};

TEST_F(ActorLoginGetCredentialsHelperTest, GetCredentialsSuccess) {
  base::test::TestFuture<CredentialsOrError> future;
  ActorLoginGetCredentialsHelper helper(GURL("https://example.com"), client(),
                                        future.GetCallback());

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
  ActorLoginGetCredentialsHelper helper(GURL("https://foo.com"), client(),
                                        future.GetCallback());

  ASSERT_TRUE(future.Get().has_value());
  ASSERT_EQ(future.Get().value().size(), 1u);
  EXPECT_EQ(future.Get().value()[0].username, u"foo_username");
  EXPECT_EQ(future.Get().value()[0].type, kPassword);
  EXPECT_EQ(future.Get().value()[0].source_site_or_app, u"https://foo.com/");
  // This is a temporary default value, to be used until searching for
  // the signin form is implemented.
  EXPECT_TRUE(future.Get().value()[0].immediatelyAvailableToLogin);
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
  ActorLoginGetCredentialsHelper helper(GURL("https://foo.com"), client(),
                                        future.GetCallback());

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

}  // namespace actor_login
