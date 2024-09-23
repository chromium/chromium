// Copyright 2016 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/password_manager/core/browser/insecure_credentials_helper.h"

#include <string_view>

#include "base/functional/callback.h"
#include "base/test/task_environment.h"
#include "components/password_manager/core/browser/password_form.h"
#include "components/password_manager/core/browser/password_manager_test_utils.h"
#include "components/password_manager/core/browser/password_store/mock_password_store_interface.h"
#include "components/password_manager/core/browser/password_store/password_store_consumer.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace password_manager {

namespace {

using ::testing::_;

// Creates a form.
PasswordForm CreateForm(std::string_view signon_realm,
                        std::u16string_view username,
                        std::u16string_view password = std::u16string_view()) {
  PasswordForm form;
  form.signon_realm = std::string(signon_realm);
  form.username_value = std::u16string(username);
  form.password_value = std::u16string(password);
  return form;
}

MatchingReusedCredential MakeCredential(std::string_view signon_realm,
                                        std::u16string_view username) {
  MatchingReusedCredential credential;
  credential.signon_realm = std::string(signon_realm);
  credential.username = std::u16string(username);
  return credential;
}

}  // namespace

class InsecureCredentialsHelperTest : public testing::Test {
 public:
  InsecureCredentialsHelperTest()
      : store_(base::MakeRefCounted<MockPasswordStoreInterface>()) {}

  MockPasswordStoreInterface* store() { return store_.get(); }

  void ExpectGetLogins(const std::string& signon_realm) {
    PasswordFormDigest digest = {PasswordForm::Scheme::kHtml, signon_realm,
                                 GURL(signon_realm)};

    EXPECT_CALL(*store_, GetLogins(digest, _))
        .WillOnce(testing::WithArg<1>(
            [this](base::WeakPtr<PasswordStoreConsumer> consumer) {
              consumer_ = consumer;
            }));
  }

  void SimulateStoreRepliedWithResults(
      const std::vector<PasswordForm>& password_forms) {
    consumer_->OnGetPasswordStoreResultsOrErrorFrom(store_.get(),
                                                    password_forms);
  }

  void TearDown() override { store()->ShutdownOnUIThread(); }

 private:
  base::test::SingleThreadTaskEnvironment task_environment_{
      base::test::TaskEnvironment::TimeSource::MOCK_TIME};
  scoped_refptr<MockPasswordStoreInterface> store_;
  base::WeakPtr<PasswordStoreConsumer> consumer_;
};

TEST_F(InsecureCredentialsHelperTest, UpdateLoginCalledForTheRightFormAdd) {
  std::vector<PasswordForm> forms = {
      CreateForm("http://example.com", u"username1"),
      CreateForm("http://example.com", u"username2"),
      CreateForm("http://foo.com", u"username2"),
  };
  PasswordForm expected_form = forms[0];
  expected_form.password_issues[InsecureType::kPhished] = InsecurityMetadata(
      base::Time::Now(), IsMuted(false), TriggerBackendNotification(false));
  ExpectGetLogins("http://example.com");
  AddPhishedCredentials(store(),
                        MakeCredential("http://example.com", u"username1"));
  EXPECT_CALL(*store(), UpdateLogin(expected_form, _));
  SimulateStoreRepliedWithResults(forms);
}

TEST_F(InsecureCredentialsHelperTest, UpdateLoginCalledForTheRightFormRemove) {
  std::vector<PasswordForm> forms = {
      CreateForm("http://example.com", u"username1"),
      CreateForm("http://example.com", u"username2"),
      CreateForm("http://foo.com", u"username2"),
  };
  forms.at(0).password_issues[InsecureType::kPhished] = InsecurityMetadata(
      base::Time::Now(), IsMuted(false), TriggerBackendNotification(false));

  ExpectGetLogins("http://example.com");
  RemovePhishedCredentials(store(),
                           MakeCredential("http://example.com", u"username1"));
  EXPECT_CALL(*store(),
              UpdateLogin(CreateForm("http://example.com", u"username1"), _));
  SimulateStoreRepliedWithResults(forms);
}

TEST_F(InsecureCredentialsHelperTest, UpdateLoginCalledForAllMatchingFormsAdd) {
  std::vector<PasswordForm> forms = {
      CreateForm("http://example.com", u"username", u"password1"),
      CreateForm("http://example.com", u"username", u"password2"),
      CreateForm("http://foo.com", u"username", u"password2"),
  };
  ExpectGetLogins("http://example.com");
  AddPhishedCredentials(store(),
                        MakeCredential("http://example.com", u"username"));
  forms.at(0).password_issues[InsecureType::kPhished] = InsecurityMetadata(
      base::Time::Now(), IsMuted(false), TriggerBackendNotification(false));
  forms.at(1).password_issues[InsecureType::kPhished] = InsecurityMetadata(
      base::Time::Now(), IsMuted(false), TriggerBackendNotification(false));
  EXPECT_CALL(*store(), UpdateLogin(forms[1], _));
  EXPECT_CALL(*store(), UpdateLogin(forms[0], _));
  SimulateStoreRepliedWithResults(
      {CreateForm("http://example.com", u"username", u"password1"),
       CreateForm("http://example.com", u"username", u"password2")});
}

TEST_F(InsecureCredentialsHelperTest,
       UpdateLoginCalledForAllMatchingFormsRemove) {
  std::vector<PasswordForm> forms = {
      CreateForm("http://example.com", u"username", u"password1"),
      CreateForm("http://example.com", u"username", u"password2"),
      CreateForm("http://foo.com", u"username", u"password2"),
  };

  ExpectGetLogins("http://example.com");
  RemovePhishedCredentials(store(),
                           MakeCredential("http://example.com", u"username"));
  forms.at(0).password_issues[InsecureType::kPhished] = InsecurityMetadata();
  forms.at(1).password_issues[InsecureType::kPhished] = InsecurityMetadata();
  EXPECT_CALL(*store(), UpdateLogin(CreateForm("http://example.com",
                                               u"username", u"password2"),
                                    _));
  EXPECT_CALL(*store(), UpdateLogin(CreateForm("http://example.com",
                                               u"username", u"password1"),
                                    _));
  SimulateStoreRepliedWithResults(forms);
}

TEST_F(InsecureCredentialsHelperTest,
       UpdateCalledOnlyIfIssueWasNotPhishedBeforeAdd) {
  std::vector<PasswordForm> forms = {
      CreateForm("http://example.com", u"username1"),
      CreateForm("http://example.com", u"username2"),
  };

  ExpectGetLogins("http://example.com");
  AddPhishedCredentials(store(),
                        MakeCredential("http://example.com", u"username1"));
  forms.at(0).password_issues[InsecureType::kPhished] = InsecurityMetadata();
  EXPECT_CALL(*store(), UpdateLogin).Times(0);
  SimulateStoreRepliedWithResults(forms);
}

TEST_F(InsecureCredentialsHelperTest,
       UpdateCalledOnlyIfIssueWasPhishedBeforeRemove) {
  std::vector<PasswordForm> forms = {
      CreateForm("http://example.com", u"username1"),
      CreateForm("http://example.com", u"username2"),
  };

  ExpectGetLogins("http://example.com");
  RemovePhishedCredentials(store(),
                           MakeCredential("http://example.com", u"username1"));
  EXPECT_CALL(*store(), UpdateLogin).Times(0);
  SimulateStoreRepliedWithResults(forms);
}

}  // namespace password_manager
