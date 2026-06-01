// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/autofill/core/browser/integrators/personal_context/personal_context_autofill_util.h"

#include "components/personal_context/core/personal_context_enablement_service.h"
#include "components/personal_context/core/personal_context_prefs.h"
#include "components/personal_context/core/personal_context_types.h"
#include "components/prefs/testing_pref_service.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace autofill {

namespace {

using personal_context::PersonalContextEnablementService;
using personal_context::PersonalContextEnablementState;

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

}  // namespace

TEST(PersonalContextAutofillUtilTest,
     ShouldShowPersonalContextAutofillSetting) {
  using enum PersonalContextEnablementState;
  MockPersonalContextEnablementService service;

  auto check_state = [&](PersonalContextEnablementState state) {
    EXPECT_CALL(service, GetEnablementState()).WillOnce(testing::Return(state));
    return ShouldShowPersonalContextAutofillSetting(&service);
  };

  EXPECT_FALSE(check_state(kDisabledNotEligible));
  EXPECT_TRUE(check_state(kDisabledShouldShowNotice));
  EXPECT_FALSE(check_state(kDisabledNeedsOptIn));
  EXPECT_TRUE(check_state(kDisabledViaPersonalIntelligenceInAutofillToggle));
  EXPECT_TRUE(check_state(kEnabledShouldShowNotice));
  EXPECT_TRUE(check_state(kEnabled));

  EXPECT_FALSE(ShouldShowPersonalContextAutofillSetting(nullptr));
}

TEST(PersonalContextAutofillUtilTest,
     PersonalContextInAutofillSettingFlippedOn) {
  TestingPrefServiceSimple pref_service;
  personal_context::prefs::RegisterProfilePrefs(pref_service.registry());

  pref_service.SetBoolean(
      personal_context::prefs::kPersonalContextInAutofillNoticeShouldBeShown,
      true);
  pref_service.SetBoolean(
      personal_context::prefs::kPersonalContextInAutofillNoticeHasBeenShown,
      false);

  PersonalContextInAutofillSettingFlippedOn(&pref_service);

  EXPECT_FALSE(pref_service.GetBoolean(
      personal_context::prefs::kPersonalContextInAutofillNoticeShouldBeShown));
  EXPECT_TRUE(pref_service.GetBoolean(
      personal_context::prefs::kPersonalContextInAutofillNoticeHasBeenShown));
}

}  // namespace autofill
