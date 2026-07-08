// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/personal_context/first_run/personal_context_first_run_service_impl.h"

#include <memory>

#include "base/functional/bind.h"
#include "base/test/bind.h"
#include "base/test/scoped_feature_list.h"
#include "base/test/task_environment.h"
#include "base/test/test_future.h"
#include "build/build_config.h"
#include "components/personal_context/core/personal_context_debug_features.h"
#include "components/personal_context/core/personal_context_enablement_service.h"
#include "components/personal_context/core/personal_context_features.h"
#include "components/personal_context/core/personal_context_prefs.h"
#include "components/personal_context/core/personal_context_types.h"
#include "components/prefs/testing_pref_service.h"
#include "components/signin/public/identity_manager/identity_test_environment.h"
#include "components/signin/public/identity_manager/identity_test_utils.h"
#include "content/public/browser/web_contents.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace personal_context {
namespace {

using ::testing::_;
using ::testing::Return;

class MockPersonalContextFirstRunClient : public PersonalContextFirstRunClient {
 public:
  MOCK_METHOD(void,
              ShowNotice,
              (content::WebContents*,
               FirstRunInvocationSource,
               base::OnceCallback<void(NoticeResult)>),
              (override));
};

class MockPersonalContextEnablementService
    : public PersonalContextEnablementService {
 public:
  MOCK_METHOD(void, AddObserver, (Observer*), (override));
  MOCK_METHOD(void, RemoveObserver, (Observer*), (override));
  MOCK_METHOD(PersonalContextEnablementState,
              GetEnablementState,
              (),
              (override));
};

class PersonalContextFirstRunServiceImplTest : public testing::Test {
 public:
  PersonalContextFirstRunServiceImplTest() {
    prefs::RegisterProfilePrefs(pref_service_.registry());

    auto client = std::make_unique<MockPersonalContextFirstRunClient>();
    client_ = client.get();

    service_ = std::make_unique<PersonalContextFirstRunServiceImpl>(
        std::move(client), &enablement_service_, &pref_service_,
        identity_test_env_.identity_manager());
  }

  void SignIn(const std::string& email) {
    identity_test_env_.MakePrimaryAccountAvailable(
        email, signin::ConsentLevel::kSignin);
  }

  TestingPrefServiceSimple* pref_service() { return &pref_service_; }

  MockPersonalContextFirstRunClient* client() { return client_; }

  MockPersonalContextEnablementService* enablement_service() {
    return &enablement_service_;
  }

  PersonalContextFirstRunServiceImpl* service() { return service_.get(); }

  signin::IdentityTestEnvironment* identity_test_env() {
    return &identity_test_env_;
  }

 private:
  base::test::TaskEnvironment task_environment_;

  TestingPrefServiceSimple pref_service_;
  MockPersonalContextEnablementService enablement_service_;
  signin::IdentityTestEnvironment identity_test_env_;
  std::unique_ptr<PersonalContextFirstRunServiceImpl> service_;
  raw_ptr<MockPersonalContextFirstRunClient> client_ = nullptr;
};

#if !BUILDFLAG(IS_CHROMEOS)  // Signing out does not work on ChromeOS.
TEST_F(PersonalContextFirstRunServiceImplTest, ClearsPrefOnSignout) {
  SignIn("test@gmail.com");
  pref_service()->SetBoolean(
      prefs::kPersonalContextAmbientAutofillNoticeShouldBeShown, false);
  pref_service()->SetBoolean(prefs::kPersonalContextAtMemoryNoticeShouldBeShown,
                             false);
  pref_service()->SetBoolean(
      prefs::kPersonalContextInAutofillSettingsToggleStatus, false);
  identity_test_env()->ClearPrimaryAccount();
  EXPECT_TRUE(pref_service()->GetBoolean(
      prefs::kPersonalContextAmbientAutofillNoticeShouldBeShown));
  EXPECT_TRUE(pref_service()->GetBoolean(
      prefs::kPersonalContextAtMemoryNoticeShouldBeShown));
  EXPECT_TRUE(pref_service()->GetBoolean(
      prefs::kPersonalContextInAutofillSettingsToggleStatus));
}
#endif  // !BUILDFLAG(IS_CHROMEOS)

TEST_F(PersonalContextFirstRunServiceImplTest, ResetsNoticePrefsOnStartup) {
  pref_service()->SetBoolean(
      prefs::kPersonalContextAmbientAutofillNoticeShouldBeShown, false);
  pref_service()->SetBoolean(prefs::kPersonalContextAtMemoryNoticeShouldBeShown,
                             false);
  pref_service()->SetBoolean(
      prefs::kPersonalContextInAutofillSettingsToggleStatus, false);

  base::test::ScopedFeatureList local_feature_list{
      features::debug::kPersonalContextResetNoticePrefsOnStartup};

  auto client = std::make_unique<MockPersonalContextFirstRunClient>();
  auto service = std::make_unique<PersonalContextFirstRunServiceImpl>(
      std::move(client), enablement_service(), pref_service(),
      identity_test_env()->identity_manager());

  EXPECT_TRUE(pref_service()->GetBoolean(
      prefs::kPersonalContextAmbientAutofillNoticeShouldBeShown));
  EXPECT_TRUE(pref_service()->GetBoolean(
      prefs::kPersonalContextAtMemoryNoticeShouldBeShown));
  EXPECT_TRUE(pref_service()->GetBoolean(
      prefs::kPersonalContextInAutofillSettingsToggleStatus));
}

TEST_F(PersonalContextFirstRunServiceImplTest,
       DoesNotResetNoticePrefsOnStartup) {
  pref_service()->SetBoolean(
      prefs::kPersonalContextAmbientAutofillNoticeShouldBeShown, false);
  pref_service()->SetBoolean(prefs::kPersonalContextAtMemoryNoticeShouldBeShown,
                             false);
  pref_service()->SetBoolean(
      prefs::kPersonalContextInAutofillSettingsToggleStatus, false);

  base::test::ScopedFeatureList local_feature_list;
  local_feature_list.InitAndDisableFeature(
      features::debug::kPersonalContextResetNoticePrefsOnStartup);

  auto client = std::make_unique<MockPersonalContextFirstRunClient>();
  auto service = std::make_unique<PersonalContextFirstRunServiceImpl>(
      std::move(client), enablement_service(), pref_service(),
      identity_test_env()->identity_manager());

  EXPECT_FALSE(pref_service()->GetBoolean(
      prefs::kPersonalContextAmbientAutofillNoticeShouldBeShown));
  EXPECT_FALSE(pref_service()->GetBoolean(
      prefs::kPersonalContextAtMemoryNoticeShouldBeShown));
  EXPECT_FALSE(pref_service()->GetBoolean(
      prefs::kPersonalContextInAutofillSettingsToggleStatus));
}

TEST_F(PersonalContextFirstRunServiceImplTest, SetsPrefOnAcknowledge) {
  EXPECT_CALL(*enablement_service(), GetEnablementState())
      .WillOnce(Return(PersonalContextEnablementState::kEnabled));

  EXPECT_CALL(*client(), ShowNotice)
      .WillOnce([](content::WebContents*, FirstRunInvocationSource,
                   base::OnceCallback<void(NoticeResult)> callback) {
        std::move(callback).Run(NoticeResult::kAcknowledged);
      });

  base::test::TestFuture<FirstRunTriggerResult> future;

  service()->MaybeTriggerFirstRun(nullptr, FirstRunInvocationSource::kAutofill,
                                  future.GetCallback());

  EXPECT_EQ(future.Get(), FirstRunTriggerResult::kSuccess);
  EXPECT_FALSE(pref_service()->GetBoolean(
      prefs::kPersonalContextAmbientAutofillNoticeShouldBeShown));
}

TEST_F(PersonalContextFirstRunServiceImplTest, DoesNotSetPrefOnDismiss) {
  EXPECT_CALL(*enablement_service(), GetEnablementState())
      .WillOnce(Return(PersonalContextEnablementState::kEnabled));

  EXPECT_CALL(*client(), ShowNotice)
      .WillOnce([](content::WebContents*, FirstRunInvocationSource,
                   base::OnceCallback<void(NoticeResult)> callback) {
        std::move(callback).Run(NoticeResult::kNotAcknowledged);
      });

  base::test::TestFuture<FirstRunTriggerResult> future;

  service()->MaybeTriggerFirstRun(nullptr, FirstRunInvocationSource::kAutofill,
                                  future.GetCallback());

  EXPECT_EQ(future.Get(), FirstRunTriggerResult::kSuccess);
  EXPECT_TRUE(pref_service()->GetBoolean(
      prefs::kPersonalContextAmbientAutofillNoticeShouldBeShown));
}

TEST_F(PersonalContextFirstRunServiceImplTest, DoesNotTriggerWhenNotEligible) {
  EXPECT_CALL(*enablement_service(), GetEnablementState())
      .WillOnce(Return(PersonalContextEnablementState::kDisabledNotEligible));

  EXPECT_CALL(*client(), ShowNotice).Times(0);

  base::test::TestFuture<FirstRunTriggerResult> future;

  service()->MaybeTriggerFirstRun(nullptr, FirstRunInvocationSource::kAutofill,
                                  future.GetCallback());

  EXPECT_EQ(future.Get(), FirstRunTriggerResult::kIgnoredNotEligible);
}

TEST_F(PersonalContextFirstRunServiceImplTest,
       DoesNotTriggerWhenAlreadyEnabled) {
  EXPECT_CALL(*enablement_service(), GetEnablementState())
      .WillRepeatedly(Return(PersonalContextEnablementState::kEnabled));
  pref_service()->SetBoolean(
      prefs::kPersonalContextAmbientAutofillNoticeShouldBeShown, false);

  EXPECT_CALL(*client(), ShowNotice).Times(0);

  base::test::TestFuture<FirstRunTriggerResult> future;

  service()->MaybeTriggerFirstRun(nullptr, FirstRunInvocationSource::kAutofill,
                                  future.GetCallback());

  EXPECT_EQ(future.Get(), FirstRunTriggerResult::kIgnoredAlreadyEnabled);
}

TEST_F(PersonalContextFirstRunServiceImplTest, DoesNotTriggerWhenNeedsOptIn) {
  EXPECT_CALL(*enablement_service(), GetEnablementState())
      .WillOnce(Return(PersonalContextEnablementState::kDisabledNeedsOptIn));

  EXPECT_CALL(*client(), ShowNotice).Times(0);

  base::test::TestFuture<FirstRunTriggerResult> future;
  service()->MaybeTriggerFirstRun(nullptr, FirstRunInvocationSource::kAutofill,
                                  future.GetCallback());
  EXPECT_EQ(future.Get(), FirstRunTriggerResult::kIgnoredNotEligible);
}

TEST_F(PersonalContextFirstRunServiceImplTest, TriggersWhenShouldShowNotice) {
  EXPECT_CALL(*enablement_service(), GetEnablementState())
      .WillOnce(Return(PersonalContextEnablementState::kEnabled));

  EXPECT_CALL(*client(), ShowNotice).Times(1);

  service()->MaybeTriggerFirstRun(nullptr, FirstRunInvocationSource::kAutofill,
                                  base::DoNothing());
}

TEST_F(PersonalContextFirstRunServiceImplTest,
       MarkPersonalContextAmbientAutofillNoticeAsAcknowledgedSetsPrefs) {
  pref_service()->SetBoolean(
      prefs::kPersonalContextAmbientAutofillNoticeShouldBeShown, true);

  service()->MarkPersonalContextAmbientAutofillNoticeAsAcknowledged();

  EXPECT_FALSE(pref_service()->GetBoolean(
      prefs::kPersonalContextAmbientAutofillNoticeShouldBeShown));
}

TEST_F(PersonalContextFirstRunServiceImplTest,
       ShouldShowPersonalContextAmbientAutofillNotice) {
  // Test kEnabled (and prefs true by default)
  EXPECT_CALL(*enablement_service(), GetEnablementState())
      .WillOnce(Return(PersonalContextEnablementState::kEnabled));
  EXPECT_TRUE(service()->ShouldShowPersonalContextAmbientAutofillNotice());

  // Test kEnabled (with pref false)
  EXPECT_CALL(*enablement_service(), GetEnablementState())
      .WillOnce(Return(PersonalContextEnablementState::kEnabled));
  pref_service()->SetBoolean(
      prefs::kPersonalContextAmbientAutofillNoticeShouldBeShown, false);
  EXPECT_FALSE(service()->ShouldShowPersonalContextAmbientAutofillNotice());

  // Test kDisabledNotEligible (should be false)
  EXPECT_CALL(*enablement_service(), GetEnablementState())
      .WillOnce(Return(PersonalContextEnablementState::kDisabledNotEligible));
  EXPECT_FALSE(service()->ShouldShowPersonalContextAmbientAutofillNotice());
}

TEST_F(PersonalContextFirstRunServiceImplTest,
       MarkPersonalContextInAtMemoryNoticeAsAcknowledgedSetsPrefs) {
  pref_service()->SetBoolean(
      prefs::kPersonalContextAmbientAutofillNoticeShouldBeShown, true);
  pref_service()->SetBoolean(prefs::kPersonalContextAtMemoryNoticeShouldBeShown,
                             true);

  service()->MarkPersonalContextInAtMemoryNoticeAsAcknowledged();

  EXPECT_FALSE(pref_service()->GetBoolean(
      prefs::kPersonalContextAmbientAutofillNoticeShouldBeShown));
  EXPECT_FALSE(pref_service()->GetBoolean(
      prefs::kPersonalContextAtMemoryNoticeShouldBeShown));
}

TEST_F(PersonalContextFirstRunServiceImplTest,
       ShouldShowPersonalContextAtMemoryNotice) {
  // Test kEnabled (and prefs true by default)
  EXPECT_CALL(*enablement_service(), GetEnablementState())
      .WillOnce(Return(PersonalContextEnablementState::kEnabled));
  EXPECT_TRUE(service()->ShouldShowPersonalContextAtMemoryNotice());

  // Test kEnabled (with pref false)
  EXPECT_CALL(*enablement_service(), GetEnablementState())
      .WillOnce(Return(PersonalContextEnablementState::kEnabled));
  pref_service()->SetBoolean(prefs::kPersonalContextAtMemoryNoticeShouldBeShown,
                             false);
  EXPECT_FALSE(service()->ShouldShowPersonalContextAtMemoryNotice());

  // Test kDisabledNotEligible (should be false)
  EXPECT_CALL(*enablement_service(), GetEnablementState())
      .WillOnce(Return(PersonalContextEnablementState::kDisabledNotEligible));
  EXPECT_FALSE(service()->ShouldShowPersonalContextAtMemoryNotice());
}

}  // namespace
}  // namespace personal_context
