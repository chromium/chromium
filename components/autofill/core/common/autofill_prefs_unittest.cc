// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/autofill/core/common/autofill_prefs.h"

#include "base/test/metrics/histogram_tester.h"
#include "components/sync_preferences/testing_pref_service_syncable.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace autofill::prefs {

namespace {

class AutofillProfilePrefsTest : public ::testing::Test {
 public:
  void SetUp() override { RegisterProfilePrefs(pref_service_.registry()); }

  PrefService* pref_service() { return &pref_service_; }

 private:
  sync_preferences::TestingPrefServiceSyncable pref_service_;
};

// Tests that `SetAutofillProfileEnabled` modifies the prefs and records a
// metric.
TEST_F(AutofillProfilePrefsTest, SetAutofillProfileEnabled) {
  constexpr int kOptIn = 0;
  constexpr int kOptOut = 1;
  ASSERT_TRUE(IsAutofillProfileEnabled(pref_service()));

  {
    base::HistogramTester histogram_tester;
    SetAutofillProfileEnabled(pref_service(), false);
    EXPECT_FALSE(IsAutofillProfileEnabled(pref_service()));
    histogram_tester.ExpectUniqueSample("Autofill.Address.IsEnabled.Change",
                                        kOptOut, 1);
  }

  {
    base::HistogramTester histogram_tester;
    SetAutofillProfileEnabled(pref_service(), true);
    EXPECT_TRUE(IsAutofillProfileEnabled(pref_service()));
    histogram_tester.ExpectUniqueSample("Autofill.Address.IsEnabled.Change",
                                        kOptIn, 1);
  }
}

// Tests that `SetAutofillProfileEnabled` does not emit a metric if there is no
// pref change.
TEST_F(AutofillProfilePrefsTest, SetAutofillProfileEnabledAsNoOp) {
  ASSERT_TRUE(IsAutofillProfileEnabled(pref_service()));

  base::HistogramTester histogram_tester;
  SetAutofillProfileEnabled(pref_service(), true);
  EXPECT_TRUE(IsAutofillProfileEnabled(pref_service()));
  histogram_tester.ExpectTotalCount("Autofill.Address.IsEnabled.Change", 0);
}

TEST_F(AutofillProfilePrefsTest, AutofillGmailOtpFillingEnabled_Default) {
  EXPECT_FALSE(IsAutofillGmailOtpFillingEnabled(pref_service()));
}

TEST_F(AutofillProfilePrefsTest, AutofillGmailOtpFillingEnabled_Set) {
  ASSERT_FALSE(IsAutofillGmailOtpFillingEnabled(pref_service()));
  {
    base::HistogramTester histogram_tester;
    SetAutofillGmailOtpFillingEnabled(pref_service(), true);
    EXPECT_TRUE(IsAutofillGmailOtpFillingEnabled(pref_service()));
    histogram_tester.ExpectUniqueSample("Autofill.GmailOtpOptIn.SettingsChange",
                                        true, 1);
  }
  {
    base::HistogramTester histogram_tester;
    SetAutofillGmailOtpFillingEnabled(pref_service(), false);
    EXPECT_FALSE(IsAutofillGmailOtpFillingEnabled(pref_service()));
    histogram_tester.ExpectUniqueSample("Autofill.GmailOtpOptIn.SettingsChange",
                                        false, 1);
  }
}

TEST_F(AutofillProfilePrefsTest, AutofillGmailOtpFillingEnabled_SetAsNoOp) {
  ASSERT_FALSE(IsAutofillGmailOtpFillingEnabled(pref_service()));
  base::HistogramTester histogram_tester;
  SetAutofillGmailOtpFillingEnabled(pref_service(), false);
  EXPECT_FALSE(IsAutofillGmailOtpFillingEnabled(pref_service()));
  histogram_tester.ExpectTotalCount("Autofill.GmailOtpOptIn.SettingsChange", 0);
}

TEST_F(AutofillProfilePrefsTest,
       AutofillGmailOtpFillingActivationDismissalTimestamp_Default) {
  EXPECT_TRUE(
      GetAutofillGmailOtpFillingActivationDismissalTimestamp(pref_service())
          .is_null());
}

TEST_F(AutofillProfilePrefsTest,
       AutofillGmailOtpFillingActivationDismissalTimestamp_Set) {
  base::Time now = base::Time::Now();
  ASSERT_FALSE(now.is_null());
  SetAutofillGmailOtpFillingActivationDismissalTimestamp(pref_service(), now);
  EXPECT_EQ(
      GetAutofillGmailOtpFillingActivationDismissalTimestamp(pref_service()),
      now);
}

}  // namespace

}  // namespace autofill::prefs
