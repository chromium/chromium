// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <memory>
#include <utility>
#include <vector>

#include "base/feature_list.h"
#include "base/strings/string_util.h"
#include "base/strings/utf_string_conversions.h"
#include "base/test/metrics/histogram_tester.h"
#include "base/test/metrics/user_action_tester.h"
#include "base/test/task_environment.h"
#include "base/test/test_mock_time_task_runner.h"
#include "base/time/time.h"
#include "build/build_config.h"
#include "components/autofill/core/common/form_field_data.h"
#include "components/password_manager/core/browser/form_fetcher_impl.h"
#include "components/password_manager/core/browser/http_auth_manager_impl.h"
#include "components/password_manager/core/browser/password_form.h"
#include "components/password_manager/core/browser/password_form_manager.h"
#include "components/password_manager/core/browser/password_form_manager_for_ui.h"
#include "components/password_manager/core/browser/password_manager_driver.h"
#include "components/password_manager/core/browser/password_store/mock_password_store_interface.h"
#include "components/password_manager/core/browser/password_store/mock_smart_bubble_stats_store.h"
#include "components/password_manager/core/browser/password_store/password_store_consumer.h"
#include "components/password_manager/core/browser/password_store/password_store_interface.h"
#include "components/password_manager/core/browser/stub_password_manager_client.h"
#include "components/password_manager/core/common/password_manager_pref_names.h"
#include "components/prefs/pref_registry_simple.h"
#include "components/prefs/pref_service.h"
#include "components/prefs/testing_pref_service.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

#if BUILDFLAG(IS_MAC) || BUILDFLAG(IS_LINUX)
#include "components/os_crypt/sync/os_crypt_mocker.h"
#endif

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
  MOCK_METHOD(bool,
              IsSavingAndFillingEnabled,
              (const GURL&),
              (const, override));
  MOCK_METHOD(bool, IsFillingEnabled, (const GURL&), (const, override));
  MOCK_METHOD(void,
              AutofillHttpAuth,
              (const PasswordForm&, const PasswordFormManagerForUI*),
              (override));
  MOCK_METHOD(PasswordStoreInterface*,
              GetProfilePasswordStore,
              (),
              (const, override));
  MOCK_METHOD(PasswordStoreInterface*,
              GetAccountPasswordStore,
              (),
              (const, override));
  MOCK_METHOD(void, PromptUserToSaveOrUpdatePasswordPtr, (), ());
  MOCK_METHOD(PrefService*, GetPrefs, (), (const, override));

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

  MockHttpAuthObserver(const MockHttpAuthObserver&) = delete;
  MockHttpAuthObserver& operator=(const MockHttpAuthObserver&) = delete;

  MOCK_METHOD0(OnLoginModelDestroying, void());
  MOCK_METHOD2(OnAutofillDataAvailable,
               void(const std::u16string&, const std::u16string&));
};

ACTION_P(InvokeEmptyConsumerWithForms, store) {
  arg0->OnGetPasswordStoreResultsOrErrorFrom(store,
                                             std::vector<PasswordForm>());
}
}  // namespace

// The boolean param determines the presence of the "account" PasswordStore.
class HttpAuthManagerTest : public testing::Test,
                            public testing::WithParamInterface<bool> {
 public:
  HttpAuthManagerTest() = default;
  ~HttpAuthManagerTest() override = default;

 protected:
  void SetUp() override {
    store_ = new testing::StrictMock<MockPasswordStoreInterface>;

    if (GetParam()) {
      account_store_ = new testing::NiceMock<MockPasswordStoreInterface>;

      // Most tests don't really need the account store, but it'll still get
      // queried by MultiStoreFormFetcher, so it needs to return something to
      // its consumers. Let the account store return empty results by default,
      // so that not every test has to set this up individually. Individual
      // tests that do cover the account store can still override this.
      ON_CALL(*account_store_, GetLogins(_, _))
          .WillByDefault(
              WithArg<1>(InvokeEmptyConsumerWithForms(account_store_.get())));
    }

#if BUILDFLAG(IS_MAC) || BUILDFLAG(IS_LINUX)
    OSCryptMocker::SetUp();
    pref_service_.registry()->RegisterIntegerPref(
        password_manager::prefs::kRelaunchChromeBubbleDismissedCounter, 0);
#endif

    ON_CALL(client_, GetProfilePasswordStore())
        .WillByDefault(Return(store_.get()));
    ON_CALL(client_, GetAccountPasswordStore())
        .WillByDefault(Return(account_store_.get()));
    EXPECT_CALL(*store_, GetSmartBubbleStatsStore)
        .WillRepeatedly(Return(&smart_bubble_stats_store_));
    ON_CALL(client_, GetPrefs()).WillByDefault(Return(&pref_service_));

    httpauth_manager_ = std::make_unique<HttpAuthManagerImpl>(&client_);

    EXPECT_CALL(*store_, IsAbleToSavePasswords()).WillRepeatedly(Return(true));

    ON_CALL(client_, AutofillHttpAuth(_, _))
        .WillByDefault(
            Invoke(httpauth_manager_.get(), &HttpAuthManagerImpl::Autofill));
  }

  HttpAuthManagerImpl* httpauth_manager() { return httpauth_manager_.get(); }

  base::test::TaskEnvironment task_environment_;
  scoped_refptr<MockPasswordStoreInterface> store_;
  scoped_refptr<MockPasswordStoreInterface> account_store_;
  TestingPrefServiceSimple pref_service_;
  testing::NiceMock<MockPasswordManagerClient> client_;
  testing::NiceMock<MockSmartBubbleStatsStore> smart_bubble_stats_store_;
  std::unique_ptr<HttpAuthManagerImpl> httpauth_manager_;
};

TEST_P(HttpAuthManagerTest, HttpAuthFilling) {
  EXPECT_CALL(client_, IsFillingEnabled(_)).WillRepeatedly(Return(true));

  PasswordForm observed_form;
  observed_form.scheme = PasswordForm::Scheme::kBasic;
  observed_form.url = GURL("http://proxy.com/");
  observed_form.signon_realm = "proxy.com/realm";

  PasswordForm stored_form = observed_form;
  stored_form.username_value = u"user";
  stored_form.password_value = u"1234";

  MockHttpAuthObserver observer;

  base::WeakPtr<PasswordStoreConsumer> consumer;
  EXPECT_CALL(*store_, GetLogins(_, _)).WillOnce(SaveArg<1>(&consumer));
  httpauth_manager()->SetObserverAndDeliverCredentials(&observer,
                                                       observed_form);
  EXPECT_CALL(observer, OnAutofillDataAvailable(std::u16string(u"user"),
                                                std::u16string(u"1234")));
  ASSERT_TRUE(consumer);
  std::vector<PasswordForm> result;
  result.push_back(stored_form);
  consumer->OnGetPasswordStoreResultsOrErrorFrom(store_.get(),
                                                 std::move(result));
  testing::Mock::VerifyAndClearExpectations(&store_);
  httpauth_manager()->DetachObserver(&observer);
}

TEST_P(HttpAuthManagerTest, HttpAuthSaving) {
  for (bool filling_and_saving_enabled : {true, false}) {
    SCOPED_TRACE(testing::Message("filling_and_saving_enabled=")
                 << filling_and_saving_enabled);

    EXPECT_CALL(client_, IsSavingAndFillingEnabled(_))
        .WillRepeatedly(Return(filling_and_saving_enabled));
    EXPECT_CALL(client_, IsFillingEnabled)
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
    submitted_form.username_value = u"user";
    submitted_form.password_value = u"1234";
    httpauth_manager()->OnPasswordFormSubmitted(submitted_form);
    httpauth_manager()->OnPasswordFormDismissed();

    // Expect save prompt on successful submission.
    EXPECT_CALL(client_, PromptUserToSaveOrUpdatePasswordPtr())
        .Times(filling_and_saving_enabled ? 1 : 0);
    httpauth_manager()->OnDidFinishMainFrameNavigation();
    testing::Mock::VerifyAndClearExpectations(&client_);
    httpauth_manager()->DetachObserver(&observer);
  }
}

TEST_P(HttpAuthManagerTest, UpdateLastUsedTimeWhenSubmittingSavedCredentials) {
  EXPECT_CALL(client_, IsSavingAndFillingEnabled).WillRepeatedly(Return(true));
  EXPECT_CALL(client_, IsFillingEnabled).WillRepeatedly(Return(true));

  PasswordForm observed_form;
  observed_form.scheme = PasswordForm::Scheme::kBasic;
  observed_form.url = GURL("http://proxy.com/");
  observed_form.signon_realm = "proxy.com/realm";

  PasswordForm stored_form = observed_form;
  stored_form.username_value = u"user";
  stored_form.password_value = u"1234";
  stored_form.in_store = PasswordForm::Store::kProfileStore;
  stored_form.date_last_used = base::Time::Now() - base::Days(1);
  stored_form.match_type = PasswordForm::MatchType::kExact;

  MockHttpAuthObserver observer;

  base::WeakPtr<PasswordStoreConsumer> consumer;
  EXPECT_CALL(*store_, GetLogins).WillOnce(SaveArg<1>(&consumer));
  httpauth_manager()->SetObserverAndDeliverCredentials(&observer,
                                                       observed_form);
  ASSERT_TRUE(consumer);
  std::vector<PasswordForm> result;
  result.push_back(stored_form);
  consumer->OnGetPasswordStoreResultsOrErrorFrom(store_.get(),
                                                 std::move(result));

  // Emulate that http auth credentials submitted.
  httpauth_manager()->OnPasswordFormSubmitted(stored_form);
  httpauth_manager()->OnPasswordFormDismissed();
  PasswordForm expected_updated_form;
  EXPECT_CALL(*store_, UpdateLogin)
      .WillOnce(SaveArg<0>(&expected_updated_form));
  httpauth_manager()->OnDidFinishMainFrameNavigation();
  // `date_last_used` should have been updated to a more recent value.
  EXPECT_GT(expected_updated_form.date_last_used, stored_form.date_last_used);
  testing::Mock::VerifyAndClearExpectations(&store_);
  httpauth_manager()->DetachObserver(&observer);
}

TEST_P(HttpAuthManagerTest, DontSaveEmptyPasswords) {
  EXPECT_CALL(client_, IsSavingAndFillingEnabled).WillRepeatedly(Return(true));
  EXPECT_CALL(client_, IsFillingEnabled).WillRepeatedly(Return(true));
  PasswordForm observed_form;
  observed_form.scheme = PasswordForm::Scheme::kBasic;
  observed_form.url = GURL("http://proxy.com/");
  observed_form.signon_realm = "proxy.com/realm";

  MockHttpAuthObserver observer;
  EXPECT_CALL(*store_, GetLogins)
      .WillOnce(WithArg<1>(InvokeEmptyConsumerWithForms(store_.get())));

  // Initiate creating a form manager.
  httpauth_manager()->SetObserverAndDeliverCredentials(&observer,
                                                       observed_form);
  // Emulate that http auth credentials submitted with an empty password.
  PasswordForm submitted_form = observed_form;
  submitted_form.username_value = u"user";
  submitted_form.password_value = std::u16string();
  httpauth_manager()->OnPasswordFormSubmitted(submitted_form);
  httpauth_manager()->OnPasswordFormDismissed();

  // Expect no save prompt on successful submission.
  EXPECT_CALL(client_, PromptUserToSaveOrUpdatePasswordPtr()).Times(0);
  httpauth_manager()->OnDidFinishMainFrameNavigation();
  testing::Mock::VerifyAndClearExpectations(&client_);
  httpauth_manager()->DetachObserver(&observer);
}

TEST_P(HttpAuthManagerTest, NavigationWithoutSubmission) {
  EXPECT_CALL(client_, IsSavingAndFillingEnabled(_))
      .WillRepeatedly(Return(true));
  EXPECT_CALL(client_, IsFillingEnabled).WillRepeatedly(Return(true));
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

TEST_P(HttpAuthManagerTest, NavigationWhenMatchingNotReady) {
  EXPECT_CALL(client_, IsSavingAndFillingEnabled).WillRepeatedly(Return(true));
  EXPECT_CALL(client_, IsFillingEnabled).WillRepeatedly(Return(true));
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
  submitted_form.username_value = u"user";
  submitted_form.password_value = u"1234";
  httpauth_manager()->OnPasswordFormSubmitted(submitted_form);
  httpauth_manager()->OnPasswordFormDismissed();

  // Expect no prompt as the password store didn't reply.
  EXPECT_CALL(client_, PromptUserToSaveOrUpdatePasswordPtr()).Times(0);
  httpauth_manager()->OnDidFinishMainFrameNavigation();
  httpauth_manager()->DetachObserver(&observer);
}

TEST_P(HttpAuthManagerTest, FillingDisabled) {
  EXPECT_CALL(client_, IsSavingAndFillingEnabled).WillRepeatedly(Return(false));
  EXPECT_CALL(client_, IsFillingEnabled).WillRepeatedly(Return(false));
  PasswordForm observed_form;
  observed_form.scheme = PasswordForm::Scheme::kBasic;
  observed_form.url = GURL("http://proxy.com/");
  observed_form.signon_realm = "proxy.com/realm";

  MockHttpAuthObserver observer;
  // The password store is not queried as the password manager is disabled.
  EXPECT_CALL(*store_, GetLogins).Times(0);
  // Initiate creating a form manager.
  httpauth_manager()->SetObserverAndDeliverCredentials(&observer,
                                                       observed_form);

  PasswordForm submitted_form = observed_form;
  submitted_form.username_value = u"user";
  submitted_form.password_value = u"1234";
  httpauth_manager()->OnPasswordFormSubmitted(submitted_form);
  httpauth_manager()->OnPasswordFormDismissed();

  // Expect no prompt.
  EXPECT_CALL(client_, PromptUserToSaveOrUpdatePasswordPtr()).Times(0);
  httpauth_manager()->OnDidFinishMainFrameNavigation();
  httpauth_manager()->DetachObserver(&observer);
}

INSTANTIATE_TEST_SUITE_P(, HttpAuthManagerTest, ::testing::Bool());

}  // namespace password_manager
