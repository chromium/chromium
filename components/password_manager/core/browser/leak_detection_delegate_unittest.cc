// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <utility>

#include "base/strings/utf_string_conversions.h"
#include "base/test/metrics/histogram_tester.h"
#include "base/test/scoped_feature_list.h"
#include "base/test/task_environment.h"
#include "build/build_config.h"
#include "components/password_manager/core/browser/form_parsing/form_parser.h"
#include "components/password_manager/core/browser/leak_detection/leak_detection_check.h"
#include "components/password_manager/core/browser/leak_detection/mock_leak_detection_check_factory.h"
#include "components/password_manager/core/browser/leak_detection_delegate.h"
#include "components/password_manager/core/browser/leak_detection_dialog_utils.h"
#include "components/password_manager/core/browser/mock_password_store.h"
#include "components/password_manager/core/browser/password_form.h"
#include "components/password_manager/core/browser/password_store_consumer.h"
#include "components/password_manager/core/browser/stub_password_manager_client.h"
#include "components/password_manager/core/common/password_manager_features.h"
#include "components/password_manager/core/common/password_manager_pref_names.h"
#include "components/prefs/pref_registry_simple.h"
#include "components/prefs/pref_service.h"
#include "components/prefs/testing_pref_service.h"
#include "components/safe_browsing/core/common/features.h"
#include "components/safe_browsing/core/common/safe_browsing_prefs.h"
#include "services/network/public/cpp/shared_url_loader_factory.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace password_manager {
namespace {

using base::ASCIIToUTF16;
using testing::_;
using testing::ByMove;
using testing::Eq;
using testing::NiceMock;
using testing::Return;
using testing::WithArg;

PasswordForm CreateTestForm() {
  PasswordForm form;
  form.url = GURL("http://www.example.com/a/LoginAuth");
  form.username_value = u"Adam";
  form.password_value = u"p4ssword";
  form.signon_realm = "http://www.example.com/";
  return form;
}

class MockPasswordManagerClient : public StubPasswordManagerClient {
 public:
  MockPasswordManagerClient() = default;
  ~MockPasswordManagerClient() override = default;

  MOCK_METHOD(bool, IsIncognito, (), (const override));
  MOCK_METHOD(PrefService*, GetPrefs, (), (const override));
  MOCK_METHOD(void,
              NotifyUserCredentialsWereLeaked,
              (password_manager::CredentialLeakType,
               const GURL&,
               const std::u16string&),
              (override));
  MOCK_METHOD(PasswordStore*, GetProfilePasswordStore, (), (const override));
};

class MockLeakDetectionCheck : public LeakDetectionCheck {
 public:
  MOCK_METHOD(void,
              Start,
              (const GURL&, std::u16string, std::u16string),
              (override));
};

}  // namespace

class LeakDetectionDelegateTest : public testing::Test {
 public:
  LeakDetectionDelegateTest() {
    mock_store_->Init(nullptr);
    auto mock_factory =
        std::make_unique<testing::StrictMock<MockLeakDetectionCheckFactory>>();
    mock_factory_ = mock_factory.get();
    delegate_.set_leak_factory(std::move(mock_factory));
    pref_service_->registry()->RegisterBooleanPref(
        password_manager::prefs::kPasswordLeakDetectionEnabled, true);
    pref_service_->registry()->RegisterBooleanPref(
        ::prefs::kSafeBrowsingEnabled, true);
    pref_service_->registry()->RegisterBooleanPref(
        ::prefs::kSafeBrowsingEnhanced, false);
    ON_CALL(client_, GetPrefs()).WillByDefault(Return(pref_service()));
  }

  ~LeakDetectionDelegateTest() override { mock_store_->ShutdownOnUIThread(); }

  MockPasswordManagerClient& client() { return client_; }
  MockLeakDetectionCheckFactory& factory() { return *mock_factory_; }
  LeakDetectionDelegate& delegate() { return delegate_; }
  MockPasswordStore* store() { return mock_store_.get(); }
  PrefService* pref_service() { return pref_service_.get(); }

  void WaitForPasswordStore() { task_environment_.RunUntilIdle(); }

  void SetSBState(safe_browsing::SafeBrowsingState state) {
    switch (state) {
      case safe_browsing::ENHANCED_PROTECTION:
        pref_service_->SetBoolean(::prefs::kSafeBrowsingEnhanced, true);
        pref_service_->SetBoolean(::prefs::kSafeBrowsingEnabled, true);
        break;
      case safe_browsing::STANDARD_PROTECTION:
        pref_service_->SetBoolean(::prefs::kSafeBrowsingEnhanced, false);
        pref_service_->SetBoolean(::prefs::kSafeBrowsingEnabled, true);
        break;
      case safe_browsing::NO_SAFE_BROWSING:
      default:
        pref_service_->SetBoolean(::prefs::kSafeBrowsingEnhanced, false);
        pref_service_->SetBoolean(::prefs::kSafeBrowsingEnabled, false);
        break;
    }
  }

  void SetLeakDetectionEnabled(bool is_on) {
    pref_service_->SetBoolean(
        password_manager::prefs::kPasswordLeakDetectionEnabled, is_on);
  }

  void ExpectPasswords(std::vector<PasswordForm> password_forms) {
    EXPECT_CALL(*mock_store_, GetAutofillableLogins)
        .WillOnce(testing::WithArg<0>([password_forms](
                                          PasswordStoreConsumer* consumer) {
          std::vector<std::unique_ptr<PasswordForm>> results;
          for (auto& form : password_forms)
            results.push_back(std::make_unique<PasswordForm>(std::move(form)));
          base::ThreadTaskRunnerHandle::Get()->PostTask(
              FROM_HERE,
              base::BindOnce(&PasswordStoreConsumer::OnGetPasswordStoreResults,
                             consumer->GetWeakPtr(), std::move(results)));
        }));
  }

 private:
  base::test::TaskEnvironment task_environment_{
      base::test::TaskEnvironment::TimeSource::MOCK_TIME};
  testing::NiceMock<MockPasswordManagerClient> client_;
  MockLeakDetectionCheckFactory* mock_factory_ = nullptr;
  // TODO(crbug.com/1218413): Use StrickMock after MockPasswordStore is replaced
  // with the MockPasswordStoreInterface.
  scoped_refptr<MockPasswordStore> mock_store_ =
      base::MakeRefCounted<testing::NiceMock<MockPasswordStore>>();
  LeakDetectionDelegate delegate_{&client_};
  std::unique_ptr<TestingPrefServiceSimple> pref_service_ =
      std::make_unique<TestingPrefServiceSimple>();
};

TEST_F(LeakDetectionDelegateTest, InIncognito) {
  const PasswordForm form = CreateTestForm();
  EXPECT_CALL(client(), IsIncognito).WillOnce(Return(true));
  EXPECT_CALL(factory(), TryCreateLeakCheck).Times(0);
  delegate().StartLeakCheck(form);

  EXPECT_FALSE(delegate().leak_check());
}

TEST_F(LeakDetectionDelegateTest, SafeBrowsingOff) {
  pref_service()->SetBoolean(::prefs::kSafeBrowsingEnabled, false);

  EXPECT_CALL(factory(), TryCreateLeakCheck).Times(0);
  delegate().StartLeakCheck(CreateTestForm());

  EXPECT_FALSE(delegate().leak_check());
}

TEST_F(LeakDetectionDelegateTest, UsernameIsEmpty) {
  PasswordForm form = CreateTestForm();
  form.username_value.clear();

  EXPECT_CALL(factory(), TryCreateLeakCheck).Times(0);
  delegate().StartLeakCheck(form);

  EXPECT_FALSE(delegate().leak_check());
}

TEST_F(LeakDetectionDelegateTest, StartCheck) {
  SetLeakDetectionEnabled(true);
  const PasswordForm form = CreateTestForm();
  EXPECT_CALL(client(), IsIncognito).WillOnce(Return(false));
  auto check_instance = std::make_unique<MockLeakDetectionCheck>();
  EXPECT_CALL(*check_instance,
              Start(form.url, form.username_value, form.password_value));
  EXPECT_CALL(factory(), TryCreateLeakCheck(&delegate(), _, _))
      .WillOnce(Return(ByMove(std::move(check_instance))));
  delegate().StartLeakCheck(form);

  EXPECT_TRUE(delegate().leak_check());
}

TEST_F(LeakDetectionDelegateTest, DoNotStartCheck) {
  SetLeakDetectionEnabled(false);
  const PasswordForm form = CreateTestForm();
  EXPECT_CALL(client(), IsIncognito).WillOnce(Return(false));
  auto check_instance = std::make_unique<MockLeakDetectionCheck>();
  EXPECT_CALL(factory(), TryCreateLeakCheck).Times(0);
  delegate().StartLeakCheck(form);

  EXPECT_FALSE(delegate().leak_check());
}

TEST_F(LeakDetectionDelegateTest, StartCheckWithStandardProtection) {
  SetSBState(safe_browsing::STANDARD_PROTECTION);
  SetLeakDetectionEnabled(true);
  const PasswordForm form = CreateTestForm();
  EXPECT_CALL(client(), IsIncognito).WillOnce(Return(false));
  auto check_instance = std::make_unique<MockLeakDetectionCheck>();
  EXPECT_CALL(*check_instance,
              Start(form.url, form.username_value, form.password_value));
  EXPECT_CALL(factory(), TryCreateLeakCheck(&delegate(), _, _))
      .WillOnce(Return(ByMove(std::move(check_instance))));
  delegate().StartLeakCheck(form);

  EXPECT_TRUE(delegate().leak_check());
  EXPECT_TRUE(CanStartLeakCheck(*pref_service()));
}

TEST_F(LeakDetectionDelegateTest, StartCheckWithEnhancedProtection) {
  SetSBState(safe_browsing::ENHANCED_PROTECTION);
  SetLeakDetectionEnabled(false);
  const PasswordForm form = CreateTestForm();
  EXPECT_CALL(client(), IsIncognito).WillOnce(Return(false));
  auto check_instance = std::make_unique<MockLeakDetectionCheck>();
  EXPECT_CALL(*check_instance,
              Start(form.url, form.username_value, form.password_value));
  EXPECT_CALL(factory(), TryCreateLeakCheck(&delegate(), _, _))
      .WillOnce(Return(ByMove(std::move(check_instance))));
  delegate().StartLeakCheck(form);

  EXPECT_TRUE(delegate().leak_check());
  EXPECT_TRUE(CanStartLeakCheck(*pref_service()));
}

TEST_F(LeakDetectionDelegateTest, DoNotStartCheckWithoutSafeBrowsing) {
  SetSBState(safe_browsing::NO_SAFE_BROWSING);
  SetLeakDetectionEnabled(true);
  const PasswordForm form = CreateTestForm();
  EXPECT_CALL(client(), IsIncognito).WillOnce(Return(false));
  auto check_instance = std::make_unique<MockLeakDetectionCheck>();
  EXPECT_CALL(factory(), TryCreateLeakCheck).Times(0);
  delegate().StartLeakCheck(form);

  EXPECT_FALSE(delegate().leak_check());
  EXPECT_FALSE(CanStartLeakCheck(*pref_service()));
}

TEST_F(LeakDetectionDelegateTest, DoNotStartLeakCheckIfLeakCheckIsOff) {
  SetSBState(safe_browsing::STANDARD_PROTECTION);
  SetLeakDetectionEnabled(false);
  const PasswordForm form = CreateTestForm();
  EXPECT_CALL(client(), IsIncognito).WillOnce(Return(false));
  EXPECT_CALL(factory(), TryCreateLeakCheck).Times(0);
  auto check_instance = std::make_unique<MockLeakDetectionCheck>();
  delegate().StartLeakCheck(form);

  EXPECT_FALSE(delegate().leak_check());
  EXPECT_FALSE(CanStartLeakCheck(*pref_service()));
}

TEST_F(LeakDetectionDelegateTest, LeakDetectionDoneWithFalseResult) {
  base::HistogramTester histogram_tester;
  LeakDetectionDelegateInterface* delegate_interface = &delegate();
  const PasswordForm form = CreateTestForm();

  EXPECT_CALL(factory(), TryCreateLeakCheck)
      .WillOnce(
          Return(ByMove(std::make_unique<NiceMock<MockLeakDetectionCheck>>())));
  delegate().StartLeakCheck(form);

  EXPECT_CALL(client(), NotifyUserCredentialsWereLeaked).Times(0);
  delegate_interface->OnLeakDetectionDone(
      /*is_leaked=*/false, form.url, form.username_value, form.password_value);
  histogram_tester.ExpectTotalCount(
      "PasswordManager.LeakDetection.NotifyIsLeakedTime", 0);
}

TEST_F(LeakDetectionDelegateTest,
       LeakDetectionWithForcedDialogAfterEverySuccessfulSubmission) {
  base::test::ScopedFeatureList scoped_feature_list;
  scoped_feature_list.InitAndEnableFeatureWithParameters(
      features::kPasswordChange,
      {{features::kPasswordChangeWithForcedDialogAfterEverySuccessfulSubmission,
        "true"}});

  EXPECT_TRUE(base::GetFieldTrialParamByFeatureAsBool(
      password_manager::features::kPasswordChange,
      password_manager::features::
          kPasswordChangeWithForcedDialogAfterEverySuccessfulSubmission,
      false));

  LeakDetectionDelegateInterface* delegate_interface = &delegate();
  const PasswordForm form = CreateTestForm();

  EXPECT_CALL(client(), GetProfilePasswordStore())
      .WillRepeatedly(testing::Return(store()));
  ExpectPasswords({});
  EXPECT_CALL(factory(), TryCreateLeakCheck)
      .WillOnce(
          Return(ByMove(std::make_unique<NiceMock<MockLeakDetectionCheck>>())));
  delegate().StartLeakCheck(form);

  EXPECT_CALL(client(),
              NotifyUserCredentialsWereLeaked(
                  password_manager::CreateLeakType(
                      IsSaved(false), IsReused(false), IsSyncing(false)),
                  form.url, form.username_value));

  delegate_interface->OnLeakDetectionDone(
      /*is_leaked=*/false, form.url, form.username_value, form.password_value);
  WaitForPasswordStore();
}

TEST_F(LeakDetectionDelegateTest, LeakDetectionDoneWithTrueResult) {
  base::HistogramTester histogram_tester;
  LeakDetectionDelegateInterface* delegate_interface = &delegate();
  const PasswordForm form = CreateTestForm();

  EXPECT_CALL(client(), GetProfilePasswordStore())
      .WillRepeatedly(testing::Return(store()));
  ExpectPasswords({});
  EXPECT_CALL(factory(), TryCreateLeakCheck)
      .WillOnce(
          Return(ByMove(std::make_unique<NiceMock<MockLeakDetectionCheck>>())));
  delegate().StartLeakCheck(form);

  EXPECT_CALL(client(),
              NotifyUserCredentialsWereLeaked(
                  password_manager::CreateLeakType(
                      IsSaved(false), IsReused(false), IsSyncing(false)),
                  form.url, form.username_value));
  delegate_interface->OnLeakDetectionDone(
      /*is_leaked=*/true, form.url, form.username_value, form.password_value);
  WaitForPasswordStore();
  histogram_tester.ExpectTotalCount(
      "PasswordManager.LeakDetection.NotifyIsLeakedTime", 1);
}

TEST_F(LeakDetectionDelegateTest, LeakHistoryAddCredentials) {
  LeakDetectionDelegateInterface* delegate_interface = &delegate();
  PasswordForm form = CreateTestForm();

  EXPECT_CALL(client(), GetProfilePasswordStore())
      .WillRepeatedly(testing::Return(store()));
  ExpectPasswords({form});
  EXPECT_CALL(factory(), TryCreateLeakCheck)
      .WillOnce(
          Return(ByMove(std::make_unique<NiceMock<MockLeakDetectionCheck>>())));
  delegate().StartLeakCheck(form);

  EXPECT_CALL(client(), NotifyUserCredentialsWereLeaked(_, form.url,
                                                        form.username_value));
  delegate_interface->OnLeakDetectionDone(
      /*is_leaked=*/true, form.url, form.username_value, form.password_value);

  // The expected form should have a leaked entry.
  form.password_issues.insert_or_assign(
      InsecureType::kLeaked,
      InsecurityMetadata(base::Time::Now(), IsMuted(false)));
  EXPECT_CALL(*store(), UpdateLogin(form));
  WaitForPasswordStore();
}

// crbug.com/1083937 regression
TEST_F(LeakDetectionDelegateTest, CallStartTwice) {
  EXPECT_CALL(client(), GetProfilePasswordStore())
      .WillRepeatedly(testing::Return(store()));
  ExpectPasswords({});
  auto check_instance = std::make_unique<NiceMock<MockLeakDetectionCheck>>();
  EXPECT_CALL(factory(), TryCreateLeakCheck(&delegate(), _, _))
      .WillOnce(Return(ByMove(std::move(check_instance))));
  PasswordForm form = CreateTestForm();
  delegate().StartLeakCheck(form);
  ASSERT_TRUE(delegate().leak_check());

  // The delegate analyses the password store after this call.
  LeakDetectionDelegateInterface* delegate_interface = &delegate();
  delegate_interface->OnLeakDetectionDone(
      /*is_leaked=*/true, form.url, form.username_value, form.password_value);

  // Start the check again on another form in the mean time.
  check_instance = std::make_unique<NiceMock<MockLeakDetectionCheck>>();
  ExpectPasswords({});
  EXPECT_CALL(factory(), TryCreateLeakCheck(&delegate(), _, _))
      .WillOnce(Return(ByMove(std::move(check_instance))));
  form.username_value = u"username";
  form.password_value = u"password";
  delegate().StartLeakCheck(form);
  ASSERT_TRUE(delegate().leak_check());

  // Simulate the previous check is complete now.
  WaitForPasswordStore();

  // The second check is finishing and talking to the password store. It should
  // not crash.
  delegate_interface->OnLeakDetectionDone(
      /*is_leaked=*/true, form.url, form.username_value, form.password_value);
  WaitForPasswordStore();
}

}  // namespace password_manager
