// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/autofill/core/common/autofill_prefs.h"

#include "base/json/values_util.h"
#include "base/test/metrics/histogram_tester.h"
#include "base/test/values_test_util.h"
#include "base/time/time.h"
#include "base/values.h"
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

TEST_F(AutofillProfilePrefsTest, DeduplicateEmailVerificationState) {
  // Calling on an empty pref is a safe no-op.
  EXPECT_TRUE(pref_service()->GetDict(kAutofillEmailVerificationState).empty());
  DeduplicateEmailVerificationState(pref_service());
  EXPECT_TRUE(pref_service()->GetDict(kAutofillEmailVerificationState).empty());

  // Setup initial state with mixed case, duplicates with varying
  // timestamps/permissions, and corrupted entries.
  pref_service()->SetDict(kAutofillEmailVerificationState,
                          base::test::ParseJsonDict(R"({
    "already.normalized@example.com": {
      "allowed": true,
      "issuer_site": "https://issuer-normal.com",
      "timestamp": "1000"
    },
    "SingleMixedCase@Example.COM": {
      "allowed": true,
      "issuer_site": "https://issuer-single.com",
      "timestamp": "1000"
    },
    "merge_user1@example.com": {
      "allowed": false,
      "issuer_site": "https://old-issuer1.com",
      "timestamp": "1000"
    },
    "MERGE_USER1@EXAMPLE.COM": {
      "allowed": true,
      "issuer_site": "https://new-issuer1.com",
      "timestamp": "2000"
    },
    "MERGE_USER2@EXAMPLE.COM": {
      "allowed": true,
      "issuer_site": "https://old-issuer2.com",
      "timestamp": "1000"
    },
    "merge_user2@example.com": {
      "allowed": false,
      "issuer_site": "https://new-issuer2.com",
      "timestamp": "2000"
    },
    "merge_user3@example.com": {
      "allowed": false,
      "issuer_site": "https://old-issuer3.com",
      "timestamp": "1000"
    },
    "Merge_User3@Example.Com": {
      "allowed": false,
      "issuer_site": "https://new-issuer3.com",
      "timestamp": "2000"
    },
    "corrupted_string": "not_a_dict",
    "corrupted_int": 42
  })"));

  // Trigger migration via MigrateDeprecatedAutofillPrefs on profile startup.
  MigrateDeprecatedAutofillPrefs(pref_service());

  base::DictValue expected_state = base::test::ParseJsonDict(R"({
    "already.normalized@example.com": {
      "allowed": true,
      "issuer_site": "https://issuer-normal.com",
      "timestamp": "1000"
    },
    "singlemixedcase@example.com": {
      "allowed": true,
      "issuer_site": "https://issuer-single.com",
      "timestamp": "1000"
    },
    "merge_user1@example.com": {
      "allowed": true,
      "issuer_site": "https://new-issuer1.com",
      "timestamp": "2000"
    },
    "merge_user2@example.com": {
      "allowed": true,
      "issuer_site": "https://new-issuer2.com",
      "timestamp": "2000"
    },
    "merge_user3@example.com": {
      "allowed": false,
      "issuer_site": "https://new-issuer3.com",
      "timestamp": "2000"
    }
  })");

  EXPECT_EQ(pref_service()->GetDict(kAutofillEmailVerificationState),
            expected_state);

  // Idempotence: Running deduplication again on already normalized state is a
  // no-op.
  DeduplicateEmailVerificationState(pref_service());
  EXPECT_EQ(pref_service()->GetDict(kAutofillEmailVerificationState),
            expected_state);
}

TEST_F(AutofillProfilePrefsTest, SetHasShownWalletReminderNotice) {
  EXPECT_FALSE(HasShownWalletReminderNotice(pref_service()));

  SetHasShownWalletReminderNotice(pref_service());
  EXPECT_TRUE(HasShownWalletReminderNotice(pref_service()));
}

}  // namespace

}  // namespace autofill::prefs
