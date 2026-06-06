// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/signin/core/browser/account_preview_metrics_recorder.h"

#include <optional>
#include <string>

#include "base/strings/strcat.h"
#include "base/strings/string_number_conversions.h"
#include "base/test/metrics/histogram_tester.h"
#include "base/test/task_environment.h"
#include "components/metrics/profile_metrics_service.h"
#include "components/prefs/testing_pref_service.h"
#include "components/signin/core/browser/account_metrics_id_allocator.h"
#include "components/signin/core/browser/account_preview_data.h"
#include "components/signin/public/base/signin_prefs.h"
#include "components/signin/public/identity_manager/identity_test_environment.h"
#include "components/signin/public/identity_manager/signin_constants.h"
#include "components/sync/base/data_type.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace signin {

class AccountPreviewMetricsRecorderTest : public testing::Test {
 public:
  AccountPreviewMetricsRecorderTest() {
    SigninPrefs::RegisterProfilePrefs(pref_service_.registry());
  }

  PrefService* pref_service() { return &pref_service_; }
  IdentityManager* identity_manager() {
    return identity_test_env_.identity_manager();
  }
  IdentityTestEnvironment* identity_test_env() { return &identity_test_env_; }

 protected:
  base::test::TaskEnvironment task_environment_;
  TestingPrefServiceSimple pref_service_;
  IdentityTestEnvironment identity_test_env_;
  metrics::ProfileMetricsService profile_metrics_service_{1};
};

TEST_F(AccountPreviewMetricsRecorderTest, RecordMetrics) {
  base::HistogramTester histogram_tester;

  AccountInfo account_info =
      identity_test_env()->MakeAccountAvailable("user@example.com");
  GaiaId gaia_id = account_info.gaia;

  account_info = AccountInfo::Builder(account_info)
                     .SetHostedDomain(signin::constants::kNoHostedDomainFound)
                     .SetIsChildAccount(signin::Tribool::kFalse)
                     .Build();
  identity_test_env()->UpdateAccountInfoForAccount(account_info);

  SigninPrefs signin_prefs(*pref_service());
  signin::GetOrAllocateAccountMetricsId(signin_prefs, gaia_id);

  AccountPreviewMetricsRecorder recorder(*pref_service(), *identity_manager(),
                                         profile_metrics_service_);

  AccountPreviewData data;
  data.counts[syncer::APPS] = 1;
  data.counts[syncer::DEVICE_INFO] = 12;
  data.counts[syncer::AUTOFILL] = 2;
  data.counts[syncer::AUTOFILL_WALLET_CREDENTIAL] = 3;
  data.counts[syncer::BOOKMARKS] = 4;
  data.counts[syncer::EXTENSIONS] = 5;
  data.counts[syncer::PASSWORDS] = 6;
  data.counts[syncer::PREFERENCES] = 7;
  data.counts[syncer::READING_LIST] = 8;
  data.counts[syncer::SESSIONS] = 9;
  data.counts[syncer::THEMES] = 10;
  data.counts[syncer::AUTOFILL_WALLET_METADATA] = 11;

  recorder.RecordMetrics(gaia_id, data);

  std::string_view prefix =
      "Signin.SmartAccountSelection.OnSyncPreviewFetched.";
  std::string_view account_suffix = ".Account0";

  histogram_tester.ExpectUniqueSample(
      base::StrCat({prefix, "IsManaged", account_suffix}), false, 1);
  histogram_tester.ExpectUniqueSample(
      base::StrCat({prefix, "IsSupervised", account_suffix}), false, 1);
  histogram_tester.ExpectUniqueSample(
      base::StrCat({prefix, "IsPrimary", account_suffix}), false, 1);

  histogram_tester.ExpectUniqueSample(
      base::StrCat({prefix, "APP", account_suffix}), 1, 1);
  histogram_tester.ExpectUniqueSample(
      base::StrCat({prefix, "DEVICE_INFO", account_suffix}), 12, 1);
  histogram_tester.ExpectUniqueSample(
      base::StrCat({prefix, "APP", account_suffix}) + ".Profile1", 1, 1);
  histogram_tester.ExpectUniqueSample(
      base::StrCat({prefix, "AUTOFILL", account_suffix}), 2, 1);
  histogram_tester.ExpectUniqueSample(
      base::StrCat({prefix, "AUTOFILL_WALLET_CREDENTIAL", account_suffix}), 3,
      1);
  histogram_tester.ExpectUniqueSample(
      base::StrCat({prefix, "BOOKMARK", account_suffix}), 4, 1);
  histogram_tester.ExpectUniqueSample(
      base::StrCat({prefix, "EXTENSION", account_suffix}), 5, 1);
  histogram_tester.ExpectUniqueSample(
      base::StrCat({prefix, "PASSWORD", account_suffix}), 6, 1);
  histogram_tester.ExpectUniqueSample(
      base::StrCat({prefix, "PREFERENCE", account_suffix}), 7, 1);
  histogram_tester.ExpectUniqueSample(
      base::StrCat({prefix, "READING_LIST", account_suffix}), 8, 1);
  histogram_tester.ExpectUniqueSample(
      base::StrCat({prefix, "SESSION", account_suffix}), 9, 1);
  histogram_tester.ExpectUniqueSample(
      base::StrCat({prefix, "THEME", account_suffix}), 10, 1);
  histogram_tester.ExpectUniqueSample(
      base::StrCat({prefix, "WALLET_METADATA", account_suffix}), 11, 1);
}

TEST_F(AccountPreviewMetricsRecorderTest, RecordMetricsSupervised) {
  base::HistogramTester histogram_tester;

  AccountInfo account_info =
      identity_test_env()->MakeAccountAvailable("supervised@example.com");
  GaiaId gaia_id = account_info.gaia;

  account_info = AccountInfo::Builder(account_info)
                     .SetHostedDomain(signin::constants::kNoHostedDomainFound)
                     .SetIsChildAccount(signin::Tribool::kTrue)
                     .Build();
  identity_test_env()->UpdateAccountInfoForAccount(account_info);

  SigninPrefs signin_prefs(*pref_service());
  signin::GetOrAllocateAccountMetricsId(signin_prefs, gaia_id);

  AccountPreviewMetricsRecorder recorder(*pref_service(), *identity_manager(),
                                         profile_metrics_service_);

  AccountPreviewData data;
  recorder.RecordMetrics(gaia_id, data);

  std::string_view prefix =
      "Signin.SmartAccountSelection.OnSyncPreviewFetched.";
  std::string_view account_suffix = ".Account0";

  histogram_tester.ExpectUniqueSample(
      base::StrCat({prefix, "IsSupervised", account_suffix}), true, 1);
}

TEST_F(AccountPreviewMetricsRecorderTest, DropAccountsAboveFive) {
  base::HistogramTester histogram_tester;

  SigninPrefs signin_prefs(*pref_service());
  for (int i = 0; i < 5; ++i) {
    signin::GetOrAllocateAccountMetricsId(
        signin_prefs, GaiaId("gaia_id_" + base::NumberToString(i)));
  }

  AccountInfo account_info =
      identity_test_env()->MakeAccountAvailable("user5@example.com");
  GaiaId gaia_id = account_info.gaia;

  account_info = AccountInfo::Builder(account_info)
                     .SetHostedDomain(signin::constants::kNoHostedDomainFound)
                     .Build();
  identity_test_env()->UpdateAccountInfoForAccount(account_info);

  AccountPreviewMetricsRecorder recorder(*pref_service(), *identity_manager(),
                                         profile_metrics_service_);

  AccountPreviewData data;
  recorder.RecordMetrics(gaia_id, data);

  std::string_view prefix =
      "Signin.SmartAccountSelection.OnSyncPreviewFetched.";
  EXPECT_TRUE(histogram_tester.GetTotalCountsForPrefix(prefix).empty());
}

TEST_F(AccountPreviewMetricsRecorderTest, RecordMetricsProfileOverflow) {
  base::HistogramTester histogram_tester;

  AccountInfo account_info =
      identity_test_env()->MakeAccountAvailable("user@example.com");
  GaiaId gaia_id = account_info.gaia;

  account_info = AccountInfo::Builder(account_info)
                     .SetHostedDomain(signin::constants::kNoHostedDomainFound)
                     .SetIsChildAccount(signin::Tribool::kFalse)
                     .Build();
  identity_test_env()->UpdateAccountInfoForAccount(account_info);

  SigninPrefs signin_prefs(*pref_service());
  signin::GetOrAllocateAccountMetricsId(signin_prefs, gaia_id);

  metrics::ProfileMetricsService overflow_service(25);
  AccountPreviewMetricsRecorder recorder(*pref_service(), *identity_manager(),
                                         overflow_service);

  AccountPreviewData data;
  data.counts[syncer::APPS] = 1;
  data.counts[syncer::DEVICE_INFO] = 12;

  recorder.RecordMetrics(gaia_id, data);

  std::string_view prefix =
      "Signin.SmartAccountSelection.OnSyncPreviewFetched.";
  std::string_view account_suffix = ".Account0";

  histogram_tester.ExpectUniqueSample(
      base::StrCat({prefix, "APP", account_suffix}), 1, 1);
  histogram_tester.ExpectUniqueSample(
      base::StrCat({prefix, "DEVICE_INFO", account_suffix}), 12, 1);
  histogram_tester.ExpectUniqueSample(
      base::StrCat({prefix, "APP", account_suffix, ".Profile20Plus"}), 1, 1);
}

}  // namespace signin
