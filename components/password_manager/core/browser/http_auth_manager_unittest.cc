// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <memory>
#include <utility>
#include <vector>

#include "base/feature_list.h"
#include "base/macros.h"
#include "base/optional.h"
#include "base/strings/string_util.h"
#include "base/strings/utf_string_conversions.h"
#include "base/test/metrics/histogram_tester.h"
#include "base/test/metrics/user_action_tester.h"
#include "base/test/scoped_feature_list.h"
#include "base/test/task_environment.h"
#include "base/test/test_mock_time_task_runner.h"
#include "build/build_config.h"
#include "components/autofill/core/common/form_field_data.h"
#include "components/password_manager/core/browser/form_fetcher_impl.h"
#include "components/password_manager/core/browser/http_auth_manager_impl.h"
#include "components/password_manager/core/browser/mock_password_store.h"
#include "components/password_manager/core/browser/password_form_manager.h"
#include "components/password_manager/core/browser/password_form_manager_for_ui.h"
#include "components/password_manager/core/browser/password_manager_driver.h"
#include "components/password_manager/core/browser/password_store.h"
#include "components/password_manager/core/browser/password_store_consumer.h"
#include "components/password_manager/core/browser/stub_password_manager_client.h"
#include "components/password_manager/core/common/password_manager_features.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

using base::ASCIIToUTF16;
using base::TestMockTimeTaskRunner;
using testing::_;
using testing::AnyNumber;
using testing::Invoke;
using testing::Mock;
using testing::Return;
using testing::ReturnRef;
using testing::SaveArg;
using testing::WithArg;

namespace password_manager {

namespace {

class MockPasswordManagerClient : public StubPasswordManagerClient {
 public:
  MOCK_CONST_METHOD1(IsSavingAndFillingEnabled, bool(const GURL&));
  MOCK_CONST_METHOD1(IsFillingEnabled, bool(const GURL&));
  MOCK_METHOD2(AutofillHttpAuth,
               void(const PasswordForm&, const PasswordFormManagerForUI*));
  MOCK_CONST_METHOD0(GetProfilePasswordStore, PasswordStore*());
  MOCK_CONST_METHOD0(GetAccountPasswordStore, PasswordStore*());
  MOCK_METHOD0(PromptUserToSaveOrUpdatePasswordPtr, void());

  // Workaround for std::unique_ptr<> lacking a copy constructor.
  bool PromptUserToSaveOrUpdatePassword(
      std::unique_ptr<PasswordFormManagerForUI> manager,
      bool update_password) override {
    PromptUserToSaveOrUpdatePasswordPtr();
    return true;
  }
};

class MockHttpAuthObserver : public HttpAuthObserver {
 public:
  MockHttpAuthObserver() = default;

  MOCK_METHOD0(OnLoginModelDestroying, void());
  MOCK_METHOD2(OnAutofillDataAvailable,
               void(const base::string16&, const base::string16&));

  DISALLOW_COPY_AND_ASSIGN(MockHttpAuthObserver);
};

ACTION_P(InvokeEmptyConsumerWithForms, store) {
  arg0->OnGetPasswordStoreResultsFrom(
      store, std::vector<std::unique_ptr<PasswordForm>>());
}
}  // namespace

class HttpAuthManagerTest : public testing::Test {
 public:
  HttpAuthManagerTest() = default;
  ~HttpAuthManagerTest() override = default;

 protected:
  void SetUp() override {
    store_ = new testing::StrictMock<MockPasswordStore>;
    ASSERT_TRUE(store_->Init(/*prefs=*/nullptr));

    if (base::FeatureList::IsEnabled(
            features::kEnablePasswordsAccountStorage)) {
      account_store_ = new testing::NiceMock<MockPasswordStore>;
      ASSERT_TRUE(account_store_->Init(/*prefs=*/nullptr));

      // Most tests don't really need the account store, but it'll still get
      // queried by MultiStoreFormFetcher, so it needs to return something to
      // its consumers. Let the account store return empty results by default,
      // so that not every test has to set this up individually. Individual
      // tests that do cover the account store can still override this.
      ON_CALL(*account_store_, GetLogins(_, _))
          .WillByDefault(
              WithArg<1>(InvokeEmptyConsumerWithForms(account_store_.get())));
    }

    ON_CALL(client_, GetProfilePasswordStore())
        .WillByDefault(Return(store_.get()));
    ON_CALL(client_, GetAccountPasswordStore())
        .WillByDefault(Return(account_store_.get()));

    EXPECT_CALL(*store_, GetSiteStatsImpl(_)).Times(AnyNumber());

    httpauth_manager_ = std::make_unique<HttpAuthManagerImpl>(&client_);

    EXPECT_CALL(*store_, IsAbleToSavePasswords()).WillRepeatedly(Return(true));

    ON_CALL(client_, AutofillHttpAuth(_, _))
        .WillByDefault(
            Invoke(httpauth_manager_.get(), &HttpAuthManagerImpl::Autofill));
  }

  void TearDown() override {
    if (account_store_) {
      account_store_->ShutdownOnUIThread();
      account_store_ = nullptr;
    }
    store_->ShutdownOnUIThread();
    store_ = nullptr;
  }

  HttpAuthManagerImpl* httpauth_manager() { return httpauth_manager_.get(); }

  base::test::TaskEnvironment task_environment_;
  scoped_refptr<MockPasswordStore> store_;
  scoped_refptr<MockPasswordStore> account_store_;
  testing::NiceMock<MockPasswordManagerClient> client_;
  std::unique_ptr<HttpAuthManagerImpl> httpauth_manager_;
};

TEST_F(HttpAuthManagerTest, HttpAuthFilling) {
  for (bool filling_enabled : {false, true}) {
    SCOPED_TRACE(testing::Message("filling_enabled=") << filling_enabled);
    EXPECT_CALL(client_, IsFillingEnabled(_))
        .WillRepeatedly(Return(filling_enabled));

    PasswordForm observed_form;
    observed_form.scheme = PasswordForm::Scheme::kBasic;
    observed_form.url = GURL("http://proxy.com/");
    observed_form.signon_realm = "proxy.com/realm";

    PasswordForm stored_form = observed_form;
    stored_form.username_value = ASCIIToUTF16("user");
    stored_form.password_value = ASCIIToUTF16("1234");

    MockHttpAuthObserver observer;

    PasswordStoreConsumer* consumer = nullptr;
    EXPECT_CALL(*store_, GetLogins(_, _)).WillOnce(SaveArg<1>(&consumer));
    httpauth_manager()->SetObserverAndDeliverCredentials(&observer,
                                                         observed_form);
    EXPECT_CALL(observer, OnAutofillDataAvailable(ASCIIToUTF16("user"),
                                                  ASCIIToUTF16("1234")))
        .Times(filling_enabled);
    ASSERT_TRUE(consumer);
    std::vector<std::unique_ptr<PasswordForm>> result;
    result.push_back(std::make_unique<PasswordForm>(stored_form));
    consumer->OnGetPasswordStoreResultsFrom(store_.get(), std::move(result));
    testing::Mock::VerifyAndClearExpectations(&store_);
    httpauth_manager()->DetachObserver(&observer);
  }
}

TEST_F(HttpAuthManagerTest, HttpAuthSaving) {
  for (bool filling_and_saving_enabled : {true, false}) {
    SCOPED_TRACE(testing::Message("filling_and_saving_enabled=")
                 << filling_and_saving_enabled);

    EXPECT_CALL(client_, IsSavingAndFillingEnabled(_))
        .WillRepeatedly(Return(filling_and_saving_enabled));
    PasswordForm observed_form;
    observed_form.scheme = PasswordForm::Scheme::kBasic;
    observed_form.url = GURL("http://proxy.com/");
    observed_form.signon_realm = "proxy.com/realm";

    MockHttpAuthObserver observer;
    EXPECT_CALL(*store_, GetLogins(_, _))
        .WillRepeatedly(WithArg<1>(InvokeEmptyConsumerWithForms(store_.get())));

    // Initiate creating a form manager.
    httpauth_manager()->SetObserverAndDeliverCredentials(&observer,
                                                         observed_form);
    // Emulate that http auth credentials submitted.
    PasswordForm submitted_form = observed_form;
    submitted_form.username_value = ASCIIToUTF16("user");
    submitted_form.password_value = ASCIIToUTF16("1234");
    httpauth_manager()->OnPasswordFormSubmitted(submitted_form);
    httpauth_manager()->OnPasswordFormDismissed();

    // Expect save prompt on successful submission.
    std::unique_ptr<PasswordFormManagerForUI> form_manager_to_save;
    EXPECT_CALL(client_, PromptUserToSaveOrUpdatePasswordPtr())
        .Times(filling_and_saving_enabled ? 1 : 0);
    httpauth_manager()->OnDidFinishMainFrameNavigation();
    testing::Mock::VerifyAndClearExpectations(&client_);
    httpauth_manager()->DetachObserver(&observer);
  }
}

TEST_F(HttpAuthManagerTest, NavigationWithoutSubmission) {
  EXPECT_CALL(client_, IsSavingAndFillingEnabled(_))
      .WillRepeatedly(Return(true));
  PasswordForm observed_form;
  observed_form.scheme = PasswordForm::Scheme::kBasic;
  observed_form.url = GURL("http://proxy.com/");
  observed_form.signon_realm = "proxy.com/realm";

  MockHttpAuthObserver observer;
  EXPECT_CALL(*store_, GetLogins(_, _))
      .WillRepeatedly(WithArg<1>(InvokeEmptyConsumerWithForms(store_.get())));

  // Initiate creating a form manager.
  httpauth_manager()->SetObserverAndDeliverCredentials(&observer,
                                                       observed_form);

  // Expect no prompt, since no submission was happened.
  EXPECT_CALL(client_, PromptUserToSaveOrUpdatePasswordPtr()).Times(0);
  httpauth_manager()->OnDidFinishMainFrameNavigation();
  httpauth_manager()->DetachObserver(&observer);
}

TEST_F(HttpAuthManagerTest, NavigationWhenMatchingNotReady) {
  EXPECT_CALL(client_, IsSavingAndFillingEnabled).WillRepeatedly(Return(true));
  PasswordForm observed_form;
  observed_form.scheme = PasswordForm::Scheme::kBasic;
  observed_form.url = GURL("http://proxy.com/");
  observed_form.signon_realm = "proxy.com/realm";

  MockHttpAuthObserver observer;
  // The password store is queried but it's slow and won't respond.
  EXPECT_CALL(*store_, GetLogins);
  // Initiate creating a form manager.
  httpauth_manager()->SetObserverAndDeliverCredentials(&observer,
                                                       observed_form);

  PasswordForm submitted_form = observed_form;
  submitted_form.username_value = ASCIIToUTF16("user");
  submitted_form.password_value = ASCIIToUTF16("1234");
  httpauth_manager()->OnPasswordFormSubmitted(submitted_form);
  httpauth_manager()->OnPasswordFormDismissed();

  // Expect no prompt as the password store didn't reply.
  EXPECT_CALL(client_, PromptUserToSaveOrUpdatePasswordPtr()).Times(0);
  httpauth_manager()->OnDidFinishMainFrameNavigation();
  httpauth_manager()->DetachObserver(&observer);
}

}  // namespace password_manager
