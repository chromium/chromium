// Copyright 2022 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <utility>

#include "base/memory/raw_ptr.h"
#include "base/test/scoped_feature_list.h"
#include "base/test/task_environment.h"
#include "base/time/time.h"
#include "components/autofill_assistant/browser/features.h"
#include "components/autofill_assistant/browser/save_password_leak_detection_delegate.h"
#include "components/password_manager/core/browser/leak_detection/leak_detection_check.h"
#include "components/password_manager/core/browser/leak_detection/mock_leak_detection_check_factory.h"
#include "components/password_manager/core/browser/leak_detection_delegate.h"
#include "components/password_manager/core/browser/password_form.h"
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

namespace autofill_assistant {
namespace {

using testing::_;
using testing::ByMove;
using testing::NiceMock;
using testing::Return;
using testing::StrictMock;

using password_manager::LeakDetectionDelegateInterface;
using password_manager::MockLeakDetectionCheckFactory;
using password_manager::PasswordForm;

constexpr base::TimeDelta kLeakDetectionTimeout = base::Seconds(1);

PasswordForm CreateTestForm() {
  PasswordForm form;
  form.url = GURL("http://www.example.com/a/LoginAuth");
  form.username_value = u"Adam";
  form.password_value = u"p4ssword";
  form.signon_realm = "http://www.example.com/";
  return form;
}

class MockPasswordManagerClient
    : public password_manager::StubPasswordManagerClient {
 public:
  MockPasswordManagerClient() = default;
  ~MockPasswordManagerClient() override = default;

  MOCK_METHOD(bool, IsIncognito, (), (const override));
  MOCK_METHOD(PrefService*, GetPrefs, (), (const override));
};

class MockLeakDetectionCheck : public password_manager::LeakDetectionCheck {
 public:
  MOCK_METHOD(void,
              Start,
              (const GURL&, std::u16string, std::u16string),
              (override));
};

class MockReceiver {
 public:
  MOCK_METHOD(void, OnCallback, (LeakDetectionStatus, bool));
};

}  // namespace

class SavePasswordLeakDetectionDelegateTest : public testing::Test {
 public:
  SavePasswordLeakDetectionDelegateTest() {
    features_.InitWithFeatures(
        {features::kAutofillAssistantAPCLeakCheckOnSaveSubmittedPassword}, {});

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

  ~SavePasswordLeakDetectionDelegateTest() override = default;

  MockPasswordManagerClient& client() { return client_; }
  MockLeakDetectionCheckFactory& factory() { return *mock_factory_; }
  SavePasswordLeakDetectionDelegate& delegate() { return delegate_; }
  PrefService* pref_service() { return pref_service_.get(); }
  MockReceiver& receiver() { return receiver_; }

  void SetLeakDetectionEnabled(bool is_on) {
    pref_service_->SetBoolean(
        password_manager::prefs::kPasswordLeakDetectionEnabled, is_on);
  }

  void SimulateTimeout() {
    task_environment_.FastForwardBy(kLeakDetectionTimeout + base::Seconds(1));
  }

  void StartLeakCheck(const PasswordForm& form) {
    delegate().StartLeakCheck(form,
                              base::BindOnce(&MockReceiver::OnCallback,
                                             base::Unretained(&receiver())),
                              kLeakDetectionTimeout);
  }

 private:
  base::test::ScopedFeatureList features_;
  base::test::TaskEnvironment task_environment_{
      base::test::TaskEnvironment::TimeSource::MOCK_TIME};
  testing::NiceMock<MockPasswordManagerClient> client_;
  raw_ptr<MockLeakDetectionCheckFactory> mock_factory_ = nullptr;
  SavePasswordLeakDetectionDelegate delegate_{&client_};
  std::unique_ptr<TestingPrefServiceSimple> pref_service_ =
      std::make_unique<TestingPrefServiceSimple>();
  testing::StrictMock<MockReceiver> receiver_;
};

TEST_F(SavePasswordLeakDetectionDelegateTest, InIncognito) {
  const PasswordForm form = CreateTestForm();
  EXPECT_CALL(client(), IsIncognito).WillOnce(Return(true));
  EXPECT_CALL(factory(), TryCreateLeakCheck).Times(0);

  EXPECT_CALL(
      receiver(),
      OnCallback(LeakDetectionStatus(LeakDetectionStatusCode::INCOGNITO_MODE),
                 false));

  StartLeakCheck(form);
}

TEST_F(SavePasswordLeakDetectionDelegateTest, SafeBrowsingOff) {
  pref_service()->SetBoolean(::prefs::kSafeBrowsingEnabled, false);

  const PasswordForm form = CreateTestForm();
  EXPECT_CALL(factory(), TryCreateLeakCheck).Times(0);

  EXPECT_CALL(receiver(),
              OnCallback(LeakDetectionStatus(LeakDetectionStatusCode::DISABLED),
                         false));

  StartLeakCheck(form);
}

TEST_F(SavePasswordLeakDetectionDelegateTest, APCLeakCheckDisabled) {
  base::test::ScopedFeatureList feature_override;
  feature_override.InitWithFeatures(
      {}, {features::kAutofillAssistantAPCLeakCheckOnSaveSubmittedPassword});
  const PasswordForm form = CreateTestForm();

  auto check_instance = std::make_unique<MockLeakDetectionCheck>();
  EXPECT_CALL(factory(), TryCreateLeakCheck).Times(0);

  EXPECT_CALL(
      receiver(),
      OnCallback(LeakDetectionStatus(LeakDetectionStatusCode::DISABLED_FOR_APC),
                 false));
  StartLeakCheck(form);

  EXPECT_FALSE(delegate().leak_check());
}

TEST_F(SavePasswordLeakDetectionDelegateTest, UsernameIsEmpty) {
  PasswordForm form = CreateTestForm();
  form.username_value.clear();
  EXPECT_CALL(factory(), TryCreateLeakCheck).Times(0);

  EXPECT_CALL(receiver(), OnCallback(LeakDetectionStatus(LeakDetectionStatus(
                                         LeakDetectionStatusCode::NO_USERNAME)),
                                     false));

  StartLeakCheck(form);
}

TEST_F(SavePasswordLeakDetectionDelegateTest, StartCheck) {
  SetLeakDetectionEnabled(true);
  const PasswordForm form = CreateTestForm();
  EXPECT_CALL(client(), IsIncognito).WillOnce(Return(false));

  auto check_instance = std::make_unique<MockLeakDetectionCheck>();
  EXPECT_CALL(*check_instance,
              Start(form.url, form.username_value, form.password_value));
  EXPECT_CALL(factory(), TryCreateLeakCheck(&delegate(), _, _, _))
      .WillOnce(Return(ByMove(std::move(check_instance))));

  StartLeakCheck(form);

  EXPECT_TRUE(delegate().leak_check());
  EXPECT_TRUE(password_manager::CanStartLeakCheck(*pref_service()));
}

TEST_F(SavePasswordLeakDetectionDelegateTest,
       LeakDetectionDoneWithFalseResult) {
  LeakDetectionDelegateInterface* delegate_interface = &delegate();
  const PasswordForm form = CreateTestForm();

  EXPECT_CALL(factory(), TryCreateLeakCheck)
      .WillOnce(
          Return(ByMove(std::make_unique<NiceMock<MockLeakDetectionCheck>>())));

  EXPECT_CALL(
      receiver(),
      OnCallback(LeakDetectionStatus(LeakDetectionStatusCode::SUCCESS), false));
  StartLeakCheck(form);

  delegate_interface->OnLeakDetectionDone(
      /*is_leaked=*/false, form.url, form.username_value, form.password_value);
}

TEST_F(SavePasswordLeakDetectionDelegateTest, LeakDetectionDoneWithTrueResult) {
  LeakDetectionDelegateInterface* delegate_interface = &delegate();
  const PasswordForm form = CreateTestForm();

  EXPECT_CALL(factory(), TryCreateLeakCheck)
      .WillOnce(
          Return(ByMove(std::make_unique<NiceMock<MockLeakDetectionCheck>>())));

  EXPECT_CALL(
      receiver(),
      OnCallback(LeakDetectionStatus(LeakDetectionStatusCode::SUCCESS), true));
  StartLeakCheck(form);

  delegate_interface->OnLeakDetectionDone(
      /*is_leaked=*/true, form.url, form.username_value, form.password_value);
}

TEST_F(SavePasswordLeakDetectionDelegateTest, LeakDetectionDoneWithError) {
  LeakDetectionDelegateInterface* delegate_interface = &delegate();
  const PasswordForm form = CreateTestForm();

  EXPECT_CALL(factory(), TryCreateLeakCheck)
      .WillOnce(
          Return(ByMove(std::make_unique<NiceMock<MockLeakDetectionCheck>>())));

  LeakDetectionStatus expected_status(
      password_manager::LeakDetectionError::kNetworkError);
  EXPECT_CALL(receiver(), OnCallback(expected_status, false));
  StartLeakCheck(form);

  delegate_interface->OnError(expected_status.execution_error.value());
}

TEST_F(SavePasswordLeakDetectionDelegateTest, Timeout) {
  const PasswordForm form = CreateTestForm();

  EXPECT_CALL(factory(), TryCreateLeakCheck)
      .WillOnce(
          Return(ByMove(std::make_unique<NiceMock<MockLeakDetectionCheck>>())));

  EXPECT_CALL(
      receiver(),
      OnCallback(LeakDetectionStatus(LeakDetectionStatusCode::TIMEOUT), false));
  StartLeakCheck(form);
  SimulateTimeout();
}

}  // namespace autofill_assistant
