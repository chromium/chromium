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
#include "components/personal_context/core/personal_context_enablement_service.h"
#include "components/personal_context/core/personal_context_features.h"
#include "components/personal_context/core/personal_context_prefs.h"
#include "components/personal_context/core/personal_context_types.h"
#include "components/prefs/testing_pref_service.h"
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
        std::move(client), &enablement_service_, &pref_service_);
  }

  TestingPrefServiceSimple* pref_service() { return &pref_service_; }

  MockPersonalContextFirstRunClient* client() { return client_; }

  MockPersonalContextEnablementService* enablement_service() {
    return &enablement_service_;
  }

  PersonalContextFirstRunServiceImpl* service() { return service_.get(); }

 private:
  base::test::TaskEnvironment task_environment_;
  TestingPrefServiceSimple pref_service_;
  MockPersonalContextEnablementService enablement_service_;
  std::unique_ptr<PersonalContextFirstRunServiceImpl> service_;
  raw_ptr<MockPersonalContextFirstRunClient> client_ = nullptr;
};

TEST_F(PersonalContextFirstRunServiceImplTest, SetsPrefOnAcknowledge) {
  EXPECT_CALL(*enablement_service(), GetEnablementState())
      .WillOnce(
          Return(PersonalContextEnablementState::kDisabledShouldShowNotice));

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
      prefs::kPersonalContextInAutofillNoticeShouldBeShown));
}

TEST_F(PersonalContextFirstRunServiceImplTest, DoesNotSetPrefOnDismiss) {
  EXPECT_CALL(*enablement_service(), GetEnablementState())
      .WillOnce(
          Return(PersonalContextEnablementState::kDisabledShouldShowNotice));

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
      prefs::kPersonalContextInAutofillNoticeShouldBeShown));
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
      .WillOnce(Return(PersonalContextEnablementState::kEnabled));

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

  service()->MaybeTriggerFirstRun(nullptr, FirstRunInvocationSource::kAutofill,
                                  base::DoNothing());
}

TEST_F(PersonalContextFirstRunServiceImplTest, TriggersWhenShouldShowNotice) {
  EXPECT_CALL(*enablement_service(), GetEnablementState())
      .WillOnce(
          Return(PersonalContextEnablementState::kDisabledShouldShowNotice));

  EXPECT_CALL(*client(), ShowNotice).Times(1);

  service()->MaybeTriggerFirstRun(nullptr, FirstRunInvocationSource::kAutofill,
                                  base::DoNothing());
}

TEST_F(PersonalContextFirstRunServiceImplTest,
       MarkPersonalContextInAutofillNoticeAsAcknowledgedSetsPrefs) {
  pref_service()->SetBoolean(
      prefs::kPersonalContextInAutofillNoticeShouldBeShown, true);

  service()->MarkPersonalContextInAutofillNoticeAsAcknowledged();

  EXPECT_FALSE(pref_service()->GetBoolean(
      prefs::kPersonalContextInAutofillNoticeShouldBeShown));
}

TEST_F(PersonalContextFirstRunServiceImplTest,
       ShouldShowPersonalContextAutofillNotice_FeatureDisabled) {
  base::test::ScopedFeatureList scoped_feature_list;
  scoped_feature_list.InitAndDisableFeature(
      features::kPersonalContextFirstRunNoticePhase2);

  EXPECT_CALL(*enablement_service(), GetEnablementState())
      .WillRepeatedly(
          Return(PersonalContextEnablementState::kDisabledShouldShowNotice));

  EXPECT_FALSE(service()->ShouldShowPersonalContextAutofillNotice());
}

TEST_F(PersonalContextFirstRunServiceImplTest,
       ShouldShowPersonalContextAutofillNotice_FeatureEnabled) {
  base::test::ScopedFeatureList scoped_feature_list;
  scoped_feature_list.InitAndEnableFeature(
      features::kPersonalContextFirstRunNoticePhase2);

  // Test kDisabledShouldShowNotice
  EXPECT_CALL(*enablement_service(), GetEnablementState())
      .WillOnce(
          Return(PersonalContextEnablementState::kDisabledShouldShowNotice));
  EXPECT_TRUE(service()->ShouldShowPersonalContextAutofillNotice());

  // Test kEnabledShouldShowNotice
  EXPECT_CALL(*enablement_service(), GetEnablementState())
      .WillOnce(
          Return(PersonalContextEnablementState::kEnabledShouldShowNotice));
  EXPECT_TRUE(service()->ShouldShowPersonalContextAutofillNotice());

  // Test kEnabled (should be false)
  EXPECT_CALL(*enablement_service(), GetEnablementState())
      .WillOnce(Return(PersonalContextEnablementState::kEnabled));
  EXPECT_FALSE(service()->ShouldShowPersonalContextAutofillNotice());

  // Test kDisabledNotEligible (should be false)
  EXPECT_CALL(*enablement_service(), GetEnablementState())
      .WillOnce(Return(PersonalContextEnablementState::kDisabledNotEligible));
  EXPECT_FALSE(service()->ShouldShowPersonalContextAutofillNotice());
}

}  // namespace
}  // namespace personal_context
