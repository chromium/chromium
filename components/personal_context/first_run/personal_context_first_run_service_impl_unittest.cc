// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/personal_context/first_run/personal_context_first_run_service_impl.h"

#include <memory>

#include "base/functional/bind.h"
#include "base/test/bind.h"
#include "base/test/metrics/histogram_tester.h"
#include "base/test/scoped_feature_list.h"
#include "base/test/task_environment.h"
#include "base/test/test_future.h"
#include "build/build_config.h"
#include "components/personal_context/core/personal_context_debug_features.h"
#include "components/personal_context/core/personal_context_eligibility_service.h"
#include "components/personal_context/core/personal_context_features.h"
#include "components/personal_context/core/personal_context_prefs.h"
#include "components/personal_context/core/personal_context_types.h"
#include "components/prefs/pref_registry_simple.h"
#include "components/prefs/testing_pref_service.h"
#include "components/signin/public/identity_manager/identity_test_environment.h"
#include "components/signin/public/identity_manager/identity_test_utils.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace personal_context {
namespace {

using ::testing::_;
using ::testing::Return;

class MockPersonalContextEligibilityService
    : public PersonalContextEligibilityService {
 public:
  MOCK_METHOD(void, AddObserver, (Observer*), (override));
  MOCK_METHOD(void, RemoveObserver, (Observer*), (override));
  MOCK_METHOD(PersonalContextEligibilityState,
              GetEligibilityState,
              (),
              (override));
  MOCK_METHOD(std::optional<PersonalContextNonEligibilityReason>,
              GetNonEligibilityReason,
              (),
              (const, override));
};

class PersonalContextFirstRunServiceImplTest : public testing::Test {
 public:
  PersonalContextFirstRunServiceImplTest() {
    prefs::RegisterProfilePrefs(pref_service_.registry());

    service_ = std::make_unique<PersonalContextFirstRunServiceImpl>(
        &eligibility_service_, &pref_service_,
        identity_test_env_.identity_manager());
  }

  void SignIn(const std::string& email) {
    identity_test_env_.MakePrimaryAccountAvailable(
        email, signin::ConsentLevel::kSignin);
  }

  TestingPrefServiceSimple* pref_service() { return &pref_service_; }

  MockPersonalContextEligibilityService* eligibility_service() {
    return &eligibility_service_;
  }

  PersonalContextFirstRunServiceImpl* service() { return service_.get(); }

  signin::IdentityTestEnvironment* identity_test_env() {
    return &identity_test_env_;
  }

 private:
  base::test::TaskEnvironment task_environment_;

  TestingPrefServiceSimple pref_service_;
  MockPersonalContextEligibilityService eligibility_service_;
  signin::IdentityTestEnvironment identity_test_env_;
  std::unique_ptr<PersonalContextFirstRunServiceImpl> service_;
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

  auto service = std::make_unique<PersonalContextFirstRunServiceImpl>(
      eligibility_service(), pref_service(),
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

  auto service = std::make_unique<PersonalContextFirstRunServiceImpl>(
      eligibility_service(), pref_service(),
      identity_test_env()->identity_manager());

  EXPECT_FALSE(pref_service()->GetBoolean(
      prefs::kPersonalContextAmbientAutofillNoticeShouldBeShown));
  EXPECT_FALSE(pref_service()->GetBoolean(
      prefs::kPersonalContextAtMemoryNoticeShouldBeShown));
  EXPECT_FALSE(pref_service()->GetBoolean(
      prefs::kPersonalContextInAutofillSettingsToggleStatus));
}

TEST_F(PersonalContextFirstRunServiceImplTest,
       MarkPersonalContextAmbientAutofillNoticeAsAcknowledgedSetsPrefs) {
  pref_service()->SetBoolean(
      prefs::kPersonalContextAmbientAutofillNoticeShouldBeShown, true);
  EXPECT_TRUE(pref_service()
                  ->GetTime(prefs::kAmbientAutofillNoticeAcknowledgedTimestamp)
                  .is_null());

  service()->MarkPersonalContextAmbientAutofillNoticeAsAcknowledged();

  EXPECT_FALSE(pref_service()->GetBoolean(
      prefs::kPersonalContextAmbientAutofillNoticeShouldBeShown));
  EXPECT_FALSE(pref_service()
                   ->GetTime(prefs::kAmbientAutofillNoticeAcknowledgedTimestamp)
                   .is_null());
}

TEST_F(PersonalContextFirstRunServiceImplTest,
       ShouldShowPersonalContextAmbientAutofillNotice) {
  // Test kEligible (and prefs true by default)
  EXPECT_CALL(*eligibility_service(), GetEligibilityState())
      .WillOnce(Return(PersonalContextEligibilityState::kEligible));
  EXPECT_TRUE(service()->ShouldShowPersonalContextAmbientAutofillNotice());

  // Test kEligible (with pref false)
  EXPECT_CALL(*eligibility_service(), GetEligibilityState())
      .WillOnce(Return(PersonalContextEligibilityState::kEligible));
  pref_service()->SetBoolean(
      prefs::kPersonalContextAmbientAutofillNoticeShouldBeShown, false);
  EXPECT_FALSE(service()->ShouldShowPersonalContextAmbientAutofillNotice());

  // Test kDisabledNotEligible (should be false)
  EXPECT_CALL(*eligibility_service(), GetEligibilityState())
      .WillOnce(Return(PersonalContextEligibilityState::kDisabledNotEligible));
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
  // Test kEligible (and prefs true by default)
  EXPECT_CALL(*eligibility_service(), GetEligibilityState())
      .WillOnce(Return(PersonalContextEligibilityState::kEligible));
  EXPECT_TRUE(service()->ShouldShowPersonalContextAtMemoryNotice());

  // Test kEligible (with pref false)
  EXPECT_CALL(*eligibility_service(), GetEligibilityState())
      .WillOnce(Return(PersonalContextEligibilityState::kEligible));
  pref_service()->SetBoolean(prefs::kPersonalContextAtMemoryNoticeShouldBeShown,
                             false);
  EXPECT_FALSE(service()->ShouldShowPersonalContextAtMemoryNotice());

  // Test kDisabledNotEligible (should be false)
  EXPECT_CALL(*eligibility_service(), GetEligibilityState())
      .WillOnce(Return(PersonalContextEligibilityState::kDisabledNotEligible));
  EXPECT_FALSE(service()->ShouldShowPersonalContextAtMemoryNotice());
}

TEST_F(PersonalContextFirstRunServiceImplTest,
       RecordsAmbientAutofillNoticeImpression) {
  EXPECT_EQ(pref_service()->GetInteger(
                prefs::kPersonalContextAmbientAutofillNoticeImpressionCount),
            0);

  // First impression in session 0 should increment.
  service()->RecordAmbientAutofillNoticeImpression(0);
  EXPECT_EQ(pref_service()->GetInteger(
                prefs::kPersonalContextAmbientAutofillNoticeImpressionCount),
            1);

  // Second impression in same session 0 should NOT increment.
  service()->RecordAmbientAutofillNoticeImpression(0);
  EXPECT_EQ(pref_service()->GetInteger(
                prefs::kPersonalContextAmbientAutofillNoticeImpressionCount),
            1);

  // Impression in new session 1 should increment.
  service()->RecordAmbientAutofillNoticeImpression(1);
  EXPECT_EQ(pref_service()->GetInteger(
                prefs::kPersonalContextAmbientAutofillNoticeImpressionCount),
            2);
}

TEST_F(PersonalContextFirstRunServiceImplTest,
       RecordsAtMemoryNoticeImpression) {
  EXPECT_EQ(pref_service()->GetInteger(
                prefs::kPersonalContextAtMemoryNoticeImpressionCount),
            0);

  // First impression in session 0 should increment
  service()->RecordAtMemoryNoticeImpression(0);
  EXPECT_EQ(pref_service()->GetInteger(
                prefs::kPersonalContextAtMemoryNoticeImpressionCount),
            1);

  // Second impression in same session 0 should NOT increment.
  service()->RecordAtMemoryNoticeImpression(0);
  EXPECT_EQ(pref_service()->GetInteger(
                prefs::kPersonalContextAtMemoryNoticeImpressionCount),
            1);

  // Impression in new session 1 should increment.
  service()->RecordAtMemoryNoticeImpression(1);
  EXPECT_EQ(pref_service()->GetInteger(
                prefs::kPersonalContextAtMemoryNoticeImpressionCount),
            2);
}

TEST_F(PersonalContextFirstRunServiceImplTest,
       AmbientAutofillNoticeAcknowledgementLogsAndClears) {
  pref_service()->SetInteger(
      prefs::kPersonalContextAmbientAutofillNoticeImpressionCount, 3);

  base::HistogramTester histogram_tester;
  service()->MarkPersonalContextAmbientAutofillNoticeAsAcknowledged();

  histogram_tester.ExpectUniqueSample(
      "PersonalContext.NoticeImpressionsBeforeAck.AmbientAutofill", 3, 1);
  EXPECT_EQ(pref_service()->GetInteger(
                prefs::kPersonalContextAmbientAutofillNoticeImpressionCount),
            0);
}

TEST_F(PersonalContextFirstRunServiceImplTest,
       AtMemoryNoticeAcknowledgementLogsAndClears) {
  pref_service()->SetInteger(
      prefs::kPersonalContextAtMemoryNoticeImpressionCount, 4);
  pref_service()->SetInteger(
      prefs::kPersonalContextAmbientAutofillNoticeImpressionCount, 2);

  base::HistogramTester histogram_tester;
  service()->MarkPersonalContextInAtMemoryNoticeAsAcknowledged();

  histogram_tester.ExpectUniqueSample(
      "PersonalContext.NoticeImpressionsBeforeAck.AtMemory", 4, 1);
  histogram_tester.ExpectUniqueSample(
      "PersonalContext.NoticeImpressionsBeforeImplicitAck.AmbientAutofill", 2,
      1);
  histogram_tester.ExpectTotalCount(
      "PersonalContext.NoticeImpressionsBeforeAck.AmbientAutofill", 0);

  EXPECT_EQ(pref_service()->GetInteger(
                prefs::kPersonalContextAtMemoryNoticeImpressionCount),
            0);
  EXPECT_EQ(pref_service()->GetInteger(
                prefs::kPersonalContextAmbientAutofillNoticeImpressionCount),
            0);
}

TEST_F(PersonalContextFirstRunServiceImplTest,
       AtMemoryNoticeAcknowledgementLogsImplicitAmbientAutofillIfZero) {
  pref_service()->SetInteger(
      prefs::kPersonalContextAtMemoryNoticeImpressionCount, 4);
  pref_service()->SetInteger(
      prefs::kPersonalContextAmbientAutofillNoticeImpressionCount, 0);

  base::HistogramTester histogram_tester;
  service()->MarkPersonalContextInAtMemoryNoticeAsAcknowledged();

  histogram_tester.ExpectUniqueSample(
      "PersonalContext.NoticeImpressionsBeforeAck.AtMemory", 4, 1);
  histogram_tester.ExpectUniqueSample(
      "PersonalContext.NoticeImpressionsBeforeImplicitAck.AmbientAutofill", 0,
      1);
  histogram_tester.ExpectTotalCount(
      "PersonalContext.NoticeImpressionsBeforeAck.AmbientAutofill", 0);

  EXPECT_EQ(pref_service()->GetInteger(
                prefs::kPersonalContextAtMemoryNoticeImpressionCount),
            0);
}

}  // namespace
}  // namespace personal_context
