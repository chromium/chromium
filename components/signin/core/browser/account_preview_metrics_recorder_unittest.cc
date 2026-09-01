// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/signin/core/browser/account_preview_metrics_recorder.h"

#include <optional>
#include <string>

#include "base/strings/strcat.h"
#include "base/strings/string_number_conversions.h"
#include "base/test/metrics/histogram_tester.h"
#include "base/test/scoped_feature_list.h"
#include "base/test/task_environment.h"
#include "components/metrics/profile_metrics_service.h"
#include "components/prefs/testing_pref_service.h"
#include "components/signin/core/browser/account_metrics_id_allocator.h"
#include "components/signin/core/browser/account_preview_data.h"
#include "components/signin/core/browser/account_preview_data_service.h"
#include "components/signin/core/browser/account_preview_heuristic.h"
#include "components/signin/public/base/signin_pref_names.h"
#include "components/signin/public/base/signin_prefs.h"
#include "components/signin/public/base/signin_switches.h"
#include "components/signin/public/identity_manager/identity_test_environment.h"
#include "components/signin/public/identity_manager/signin_constants.h"
#include "components/sync/base/data_type.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace signin {

class AccountPreviewMetricsRecorderTest : public testing::Test {
 public:
  AccountPreviewMetricsRecorderTest() {
    SigninPrefs::RegisterProfilePrefs(pref_service_.registry());
    AccountPreviewDataService::RegisterProfilePrefs(pref_service_.registry());
  }

  PrefService* pref_service() { return &pref_service_; }
  IdentityManager* identity_manager() {
    return identity_test_env_.identity_manager();
  }
  IdentityTestEnvironment* identity_test_env() { return &identity_test_env_; }

 protected:
  base::test::ScopedFeatureList scoped_feature_list_{
      switches::kEnableAccountPreviewPreferredAccount};
  base::test::TaskEnvironment task_environment_{
      base::test::TaskEnvironment::TimeSource::MOCK_TIME};
  TestingPrefServiceSimple pref_service_;
  IdentityTestEnvironment identity_test_env_;
  metrics::ProfileMetricsService profile_metrics_service_{1};
};

TEST_F(AccountPreviewMetricsRecorderTest, RecordMetrics) {
  base::HistogramTester histogram_tester;

  AccountInfo account_info =
      identity_test_env()->MakeAccountAvailable("user@example.com");
  GaiaId gaia_id = account_info.GetGaiaId();

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
  GaiaId gaia_id = account_info.GetGaiaId();

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
  GaiaId gaia_id = account_info.GetGaiaId();

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
  GaiaId gaia_id = account_info.GetGaiaId();

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

TEST_F(AccountPreviewMetricsRecorderTest,
       RecordSelectionHeuristicScores_SignedOutProfile_SingleAccount) {
  base::HistogramTester histogram_tester;
  AccountPreviewMetricsRecorder recorder(*pref_service(), *identity_manager(),
                                         profile_metrics_service_);

  AccountPreviewData data;
  data.counts[syncer::PASSWORDS] = switches::kPasswordsQ3Threshold.Get();
  data.counts[syncer::BOOKMARKS] = switches::kBookmarksMedianThreshold.Get();
  // Passwords (Q3 -> score 8) + Bookmarks (Median -> score 4) = 12.

  AccountPreviewHeuristicContext ctx{
      .gaia_id = GaiaId("user0"),
      .preview_data = raw_ref(data),
  };

  std::vector<AccountPreviewHeuristicContext> accounts = {ctx};
  recorder.RecordSelectionHeuristicResult(
      accounts, ComputePreferredAccountForPromo(accounts));

  histogram_tester.ExpectUniqueSample(
      "Signin.SelectionHeuristic.Reason",
      AccountPreviewSelectionReason::kSyncDataScore, 1);
  histogram_tester.ExpectUniqueSample(
      "Signin.SelectionHeuristic.Reason.Profile1",
      AccountPreviewSelectionReason::kSyncDataScore, 1);
  histogram_tester.ExpectUniqueSample(
      "Signin.SelectionHeuristicScore.PreferredAccount.SingleAccount", 12, 1);
  histogram_tester.ExpectUniqueSample(
      "Signin.SelectionHeuristicScore.PreferredAccount.SingleAccount.Profile1",
      12, 1);
  histogram_tester.ExpectTotalCount(
      "Signin.SelectionHeuristicScore.PreferredAccount.MultipleAccounts", 0);
  histogram_tester.ExpectTotalCount(
      "Signin.SelectionHeuristicScore.PrimaryAccount.SingleAccount", 0);
  histogram_tester.ExpectTotalCount(
      "Signin.SelectionHeuristicScore.PrimaryAccount.MultipleAccounts", 0);
  histogram_tester.ExpectTotalCount(
      "Signin.SelectionHeuristicScore.IsPrimaryDifferentFromPreferred"
      ".SingleAccount",
      0);
  histogram_tester.ExpectTotalCount(
      "Signin.SelectionHeuristicScore.IsPrimaryDifferentFromPreferred"
      ".MultipleAccounts",
      0);
  histogram_tester.ExpectTotalCount(
      "Signin.SelectionHeuristicScore.OtherAccount", 0);
  histogram_tester.ExpectTotalCount(
      "Signin.SelectionHeuristicScore.OtherAccount.Profile1", 0);

  EXPECT_EQ(pref_service()->GetTime(
                prefs::kAccountPreviewSelectionHeuristicScoresLastRecordedPref),
            base::Time::Now());
}

TEST_F(AccountPreviewMetricsRecorderTest,
       RecordSelectionHeuristicScores_SignedInProfile_SingleAccount) {
  base::HistogramTester histogram_tester;
  AccountInfo primary_info = identity_test_env()->MakePrimaryAccountAvailable(
      "user0@gmail.com", ConsentLevel::kSignin);

  AccountPreviewMetricsRecorder recorder(*pref_service(), *identity_manager(),
                                         profile_metrics_service_);

  AccountPreviewData data;
  data.counts[syncer::PASSWORDS] = switches::kPasswordsQ3Threshold.Get();
  data.counts[syncer::BOOKMARKS] = switches::kBookmarksMedianThreshold.Get();
  // Score = 12

  AccountPreviewHeuristicContext ctx{
      .gaia_id = primary_info.gaia,
      .preview_data = raw_ref(data),
  };

  std::vector<AccountPreviewHeuristicContext> accounts = {ctx};
  recorder.RecordSelectionHeuristicResult(
      accounts, ComputePreferredAccountForPromo(accounts));

  histogram_tester.ExpectUniqueSample(
      "Signin.SelectionHeuristic.Reason",
      AccountPreviewSelectionReason::kSyncDataScore, 1);
  histogram_tester.ExpectUniqueSample(
      "Signin.SelectionHeuristic.Reason.Profile1",
      AccountPreviewSelectionReason::kSyncDataScore, 1);
  histogram_tester.ExpectUniqueSample(
      "Signin.SelectionHeuristicScore.PrimaryAccount.SingleAccount", 12, 1);
  histogram_tester.ExpectUniqueSample(
      "Signin.SelectionHeuristicScore.PrimaryAccount.SingleAccount.Profile1",
      12, 1);
  histogram_tester.ExpectUniqueSample(
      "Signin.SelectionHeuristicScore.PreferredAccount.SingleAccount", 12, 1);
  histogram_tester.ExpectUniqueSample(
      "Signin.SelectionHeuristicScore.PreferredAccount.SingleAccount.Profile1",
      12, 1);
  histogram_tester.ExpectUniqueSample(
      "Signin.SelectionHeuristicScore.IsPrimaryDifferentFromPreferred"
      ".SingleAccount",
      false, 1);
  histogram_tester.ExpectUniqueSample(
      "Signin.SelectionHeuristicScore.IsPrimaryDifferentFromPreferred"
      ".SingleAccount.Profile1",
      false, 1);
  histogram_tester.ExpectTotalCount(
      "Signin.SelectionHeuristicScore.OtherAccount", 0);
}

TEST_F(AccountPreviewMetricsRecorderTest,
       RecordSelectionHeuristicScores_SignedInProfile_PrimaryEqualsPreferred) {
  base::HistogramTester histogram_tester;
  AccountInfo primary_info = identity_test_env()->MakePrimaryAccountAvailable(
      "user0@gmail.com", ConsentLevel::kSignin);

  AccountPreviewMetricsRecorder recorder(*pref_service(), *identity_manager(),
                                         profile_metrics_service_);

  AccountPreviewData data0;
  data0.counts[syncer::PASSWORDS] = switches::kPasswordsQ3Threshold.Get();
  data0.counts[syncer::BOOKMARKS] = switches::kBookmarksMedianThreshold.Get();
  // Score = 8 + 4 = 12

  AccountPreviewData data1;
  data1.counts[syncer::PASSWORDS] = switches::kPasswordsMedianThreshold.Get();
  // Score = 4

  AccountPreviewData data2;
  data2.counts[syncer::PASSWORDS] = switches::kPasswordsMedianThreshold.Get();
  data2.counts[syncer::AUTOFILL] = switches::kAutofillQ1Threshold.Get();
  // Score = 4 + 2 = 6

  AccountPreviewHeuristicContext ctx0{
      .gaia_id = primary_info.gaia,
      .preview_data = raw_ref(data0),
  };
  AccountPreviewHeuristicContext ctx1{
      .gaia_id = GaiaId("user1"),
      .preview_data = raw_ref(data1),
  };
  AccountPreviewHeuristicContext ctx2{
      .gaia_id = GaiaId("user2"),
      .preview_data = raw_ref(data2),
  };

  std::vector<AccountPreviewHeuristicContext> accounts = {ctx0, ctx1, ctx2};
  recorder.RecordSelectionHeuristicResult(
      accounts, ComputePreferredAccountForPromo(accounts));

  histogram_tester.ExpectUniqueSample(
      "Signin.SelectionHeuristic.Reason",
      AccountPreviewSelectionReason::kSyncDataScore, 1);
  histogram_tester.ExpectUniqueSample(
      "Signin.SelectionHeuristic.Reason.Profile1",
      AccountPreviewSelectionReason::kSyncDataScore, 1);

  histogram_tester.ExpectUniqueSample(
      "Signin.SelectionHeuristicScore.PrimaryAccount.MultipleAccounts", 12, 1);
  histogram_tester.ExpectUniqueSample(
      "Signin.SelectionHeuristicScore.PrimaryAccount.MultipleAccounts.Profile1",
      12, 1);
  histogram_tester.ExpectUniqueSample(
      "Signin.SelectionHeuristicScore.PreferredAccount.MultipleAccounts", 12,
      1);
  histogram_tester.ExpectUniqueSample(
      "Signin.SelectionHeuristicScore.PreferredAccount.MultipleAccounts."
      "Profile1",
      12, 1);
  histogram_tester.ExpectUniqueSample(
      "Signin.SelectionHeuristicScore.IsPrimaryDifferentFromPreferred"
      ".MultipleAccounts",
      false, 1);
  histogram_tester.ExpectUniqueSample(
      "Signin.SelectionHeuristicScore.IsPrimaryDifferentFromPreferred"
      ".MultipleAccounts.Profile1",
      false, 1);
  histogram_tester.ExpectUniqueSample(
      "Signin.SelectionHeuristicScore.OtherAccount", 6, 1);
  histogram_tester.ExpectUniqueSample(
      "Signin.SelectionHeuristicScore.OtherAccount.Profile1", 6, 1);
}

TEST_F(
    AccountPreviewMetricsRecorderTest,
    RecordSelectionHeuristicScores_SignedInProfile_PrimaryDifferentFromPreferred_NoOther) {
  base::HistogramTester histogram_tester;
  AccountInfo primary_info = identity_test_env()->MakePrimaryAccountAvailable(
      "user0@gmail.com", ConsentLevel::kSignin);

  AccountPreviewMetricsRecorder recorder(*pref_service(), *identity_manager(),
                                         profile_metrics_service_);

  AccountPreviewData data0;
  data0.counts[syncer::PASSWORDS] = switches::kPasswordsQ1Threshold.Get();
  // Score = 2, single device

  AccountPreviewData data1;
  data1.counts[syncer::PASSWORDS] = switches::kPasswordsQ3Threshold.Get();
  data1.counts[syncer::BOOKMARKS] = switches::kBookmarksMedianThreshold.Get();
  DevicePreview device;
  device.form_factor =
      sync_pb::SyncEnums_DeviceFormFactor_DEVICE_FORM_FACTOR_PHONE;
  data1.devices.push_back(device);
  // Score = 12, cross device

  AccountPreviewHeuristicContext ctx0{
      .gaia_id = primary_info.gaia,
      .preview_data = raw_ref(data0),
  };
  AccountPreviewHeuristicContext ctx1{
      .gaia_id = GaiaId("user1"),
      .preview_data = raw_ref(data1),
  };

  std::vector<AccountPreviewHeuristicContext> accounts = {ctx0, ctx1};
  recorder.RecordSelectionHeuristicResult(
      accounts, ComputePreferredAccountForPromo(accounts));

  histogram_tester.ExpectUniqueSample(
      "Signin.SelectionHeuristic.Reason",
      AccountPreviewSelectionReason::kSyncDataScore, 1);
  histogram_tester.ExpectUniqueSample(
      "Signin.SelectionHeuristic.Reason.Profile1",
      AccountPreviewSelectionReason::kSyncDataScore, 1);
  histogram_tester.ExpectUniqueSample(
      "Signin.SelectionHeuristicScore.PrimaryAccount.MultipleAccounts", 2, 1);
  histogram_tester.ExpectUniqueSample(
      "Signin.SelectionHeuristicScore.PreferredAccount.MultipleAccounts", 12,
      1);
  histogram_tester.ExpectUniqueSample(
      "Signin.SelectionHeuristicScore.IsPrimaryDifferentFromPreferred"
      ".MultipleAccounts",
      true, 1);
  histogram_tester.ExpectUniqueSample(
      "Signin.SelectionHeuristicScore.IsPrimaryDifferentFromPreferred"
      ".MultipleAccounts.Profile1",
      true, 1);
  histogram_tester.ExpectTotalCount(
      "Signin.SelectionHeuristicScore.OtherAccount", 0);
}

TEST_F(
    AccountPreviewMetricsRecorderTest,
    RecordSelectionHeuristicScores_SignedInProfile_PrimaryDifferentFromPreferred_WithOther) {
  base::HistogramTester histogram_tester;
  AccountInfo primary_info = identity_test_env()->MakePrimaryAccountAvailable(
      "user0@gmail.com", ConsentLevel::kSignin);

  AccountPreviewMetricsRecorder recorder(*pref_service(), *identity_manager(),
                                         profile_metrics_service_);

  AccountPreviewData data0;
  data0.counts[syncer::PASSWORDS] = switches::kPasswordsQ1Threshold.Get();
  // Score = 2, single device

  AccountPreviewData data1;
  data1.counts[syncer::PASSWORDS] = switches::kPasswordsQ3Threshold.Get();
  data1.counts[syncer::BOOKMARKS] = switches::kBookmarksMedianThreshold.Get();
  DevicePreview device;
  device.form_factor =
      sync_pb::SyncEnums_DeviceFormFactor_DEVICE_FORM_FACTOR_PHONE;
  data1.devices.push_back(device);
  // Score = 12, cross device

  AccountPreviewData data2;
  data2.counts[syncer::PASSWORDS] = switches::kPasswordsMedianThreshold.Get();
  // Score = 4

  AccountPreviewData data3;
  data3.counts[syncer::PASSWORDS] = switches::kPasswordsMedianThreshold.Get();
  data3.counts[syncer::AUTOFILL] = switches::kAutofillQ1Threshold.Get();
  data3.counts[syncer::AUTOFILL_WALLET_METADATA] =
      switches::kAutofillWalletMetadataQ1Threshold.Get();
  // Score = 4 + 2 + 2 = 8

  AccountPreviewHeuristicContext ctx0{
      .gaia_id = primary_info.gaia,
      .preview_data = raw_ref(data0),
  };
  AccountPreviewHeuristicContext ctx1{
      .gaia_id = GaiaId("user1"),
      .preview_data = raw_ref(data1),
  };
  AccountPreviewHeuristicContext ctx2{
      .gaia_id = GaiaId("user2"),
      .preview_data = raw_ref(data2),
  };
  AccountPreviewHeuristicContext ctx3{
      .gaia_id = GaiaId("user3"),
      .preview_data = raw_ref(data3),
  };

  std::vector<AccountPreviewHeuristicContext> accounts = {ctx0, ctx1, ctx2,
                                                          ctx3};
  recorder.RecordSelectionHeuristicResult(
      accounts, ComputePreferredAccountForPromo(accounts));

  histogram_tester.ExpectUniqueSample(
      "Signin.SelectionHeuristic.Reason",
      AccountPreviewSelectionReason::kSyncDataScore, 1);
  histogram_tester.ExpectUniqueSample(
      "Signin.SelectionHeuristic.Reason.Profile1",
      AccountPreviewSelectionReason::kSyncDataScore, 1);
  histogram_tester.ExpectUniqueSample(
      "Signin.SelectionHeuristicScore.PrimaryAccount.MultipleAccounts", 2, 1);
  histogram_tester.ExpectUniqueSample(
      "Signin.SelectionHeuristicScore.PreferredAccount.MultipleAccounts", 12,
      1);
  histogram_tester.ExpectUniqueSample(
      "Signin.SelectionHeuristicScore.IsPrimaryDifferentFromPreferred"
      ".MultipleAccounts",
      true, 1);
  histogram_tester.ExpectUniqueSample(
      "Signin.SelectionHeuristicScore.IsPrimaryDifferentFromPreferred"
      ".MultipleAccounts.Profile1",
      true, 1);
  histogram_tester.ExpectUniqueSample(
      "Signin.SelectionHeuristicScore.OtherAccount", 8, 1);
}

TEST_F(AccountPreviewMetricsRecorderTest,
       RecordSelectionHeuristicScores_Priority1_NonRegularDefault) {
  base::HistogramTester histogram_tester;
  AccountPreviewMetricsRecorder recorder(*pref_service(), *identity_manager(),
                                         profile_metrics_service_);

  AccountPreviewData data0;
  AccountPreviewData data1;
  data1.counts[syncer::PASSWORDS] = switches::kPasswordsQ3Threshold.Get();

  AccountPreviewHeuristicContext ctx0{
      .gaia_id = GaiaId("user0"),
      .preview_data = raw_ref(data0),
      .is_managed = true,
  };
  AccountPreviewHeuristicContext ctx1{
      .gaia_id = GaiaId("user1"),
      .preview_data = raw_ref(data1),
  };

  std::vector<AccountPreviewHeuristicContext> accounts = {ctx0, ctx1};
  recorder.RecordSelectionHeuristicResult(
      accounts, ComputePreferredAccountForPromo(accounts));

  histogram_tester.ExpectUniqueSample(
      "Signin.SelectionHeuristic.Reason",
      AccountPreviewSelectionReason::kNonRegularDefault, 1);
  histogram_tester.ExpectUniqueSample(
      "Signin.SelectionHeuristic.Reason.Profile1",
      AccountPreviewSelectionReason::kNonRegularDefault, 1);
  histogram_tester.ExpectTotalCount(
      "Signin.SelectionHeuristicScore.PrimaryAccount.MultipleAccounts", 0);
  histogram_tester.ExpectTotalCount(
      "Signin.SelectionHeuristicScore.PrimaryAccount.SingleAccount", 0);
  histogram_tester.ExpectTotalCount(
      "Signin.SelectionHeuristicScore.PreferredAccount.MultipleAccounts", 0);
  histogram_tester.ExpectTotalCount(
      "Signin.SelectionHeuristicScore.PreferredAccount.SingleAccount", 0);
  histogram_tester.ExpectTotalCount(
      "Signin.SelectionHeuristicScore.IsPrimaryDifferentFromPreferred"
      ".MultipleAccounts",
      0);
  histogram_tester.ExpectTotalCount(
      "Signin.SelectionHeuristicScore.IsPrimaryDifferentFromPreferred"
      ".SingleAccount",
      0);
  histogram_tester.ExpectTotalCount(
      "Signin.SelectionHeuristicScore.OtherAccount", 0);
  EXPECT_EQ(pref_service()->GetTime(
                prefs::kAccountPreviewSelectionHeuristicScoresLastRecordedPref),
            base::Time::Now());
}

TEST_F(AccountPreviewMetricsRecorderTest,
       RecordSelectionHeuristicScores_Priority2_AGAAccount) {
  base::HistogramTester histogram_tester;
  AccountPreviewMetricsRecorder recorder(*pref_service(), *identity_manager(),
                                         profile_metrics_service_);

  AccountPreviewData data0;
  data0.counts[syncer::PASSWORDS] = switches::kPasswordsMedianThreshold.Get();

  AccountPreviewData data1;
  data1.counts[syncer::PASSWORDS] = switches::kPasswordsQ3Threshold.Get();

  AccountPreviewHeuristicContext ctx0{
      .gaia_id = GaiaId("user0"),
      .preview_data = raw_ref(data0),
  };
  AccountPreviewHeuristicContext ctx1{
      .gaia_id = GaiaId("user1"),
      .preview_data = raw_ref(data1),
      .is_external_app_primary = true,
  };

  std::vector<AccountPreviewHeuristicContext> accounts = {ctx0, ctx1};
  recorder.RecordSelectionHeuristicResult(
      accounts, ComputePreferredAccountForPromo(accounts));

  histogram_tester.ExpectUniqueSample(
      "Signin.SelectionHeuristic.Reason",
      AccountPreviewSelectionReason::kExternalAppPrimary, 1);
  histogram_tester.ExpectUniqueSample(
      "Signin.SelectionHeuristic.Reason.Profile1",
      AccountPreviewSelectionReason::kExternalAppPrimary, 1);
  histogram_tester.ExpectTotalCount(
      "Signin.SelectionHeuristicScore.PrimaryAccount.MultipleAccounts", 0);
  histogram_tester.ExpectTotalCount(
      "Signin.SelectionHeuristicScore.PrimaryAccount.SingleAccount", 0);
  histogram_tester.ExpectTotalCount(
      "Signin.SelectionHeuristicScore.PreferredAccount.MultipleAccounts", 0);
  histogram_tester.ExpectTotalCount(
      "Signin.SelectionHeuristicScore.PreferredAccount.SingleAccount", 0);
  histogram_tester.ExpectTotalCount(
      "Signin.SelectionHeuristicScore.IsPrimaryDifferentFromPreferred"
      ".MultipleAccounts",
      0);
  histogram_tester.ExpectTotalCount(
      "Signin.SelectionHeuristicScore.IsPrimaryDifferentFromPreferred"
      ".SingleAccount",
      0);
  histogram_tester.ExpectTotalCount(
      "Signin.SelectionHeuristicScore.OtherAccount", 0);
  EXPECT_EQ(pref_service()->GetTime(
                prefs::kAccountPreviewSelectionHeuristicScoresLastRecordedPref),
            base::Time::Now());
}

TEST_F(AccountPreviewMetricsRecorderTest,
       RecordSelectionHeuristicScores_DailyRateLimit) {
  base::HistogramTester histogram_tester;
  AccountPreviewMetricsRecorder recorder(*pref_service(), *identity_manager(),
                                         profile_metrics_service_);

  AccountPreviewData data;
  data.counts[syncer::PASSWORDS] = switches::kPasswordsQ3Threshold.Get();

  AccountPreviewHeuristicContext ctx{
      .gaia_id = GaiaId("user0"),
      .preview_data = raw_ref(data),
  };

  std::vector<AccountPreviewHeuristicContext> accounts = {ctx};

  // 1st recording succeeds.
  recorder.RecordSelectionHeuristicResult(
      accounts, ComputePreferredAccountForPromo(accounts));
  histogram_tester.ExpectTotalCount("Signin.SelectionHeuristic.Reason", 1);
  histogram_tester.ExpectTotalCount(
      "Signin.SelectionHeuristicScore.PreferredAccount.SingleAccount", 1);

  // 2nd call immediately after is rate-limited.
  recorder.RecordSelectionHeuristicResult(
      accounts, ComputePreferredAccountForPromo(accounts));
  histogram_tester.ExpectTotalCount("Signin.SelectionHeuristic.Reason", 1);
  histogram_tester.ExpectTotalCount(
      "Signin.SelectionHeuristicScore.PreferredAccount.SingleAccount", 1);

  // Fast forward 23 hours -> still rate-limited.
  task_environment_.FastForwardBy(base::Hours(23));
  recorder.RecordSelectionHeuristicResult(
      accounts, ComputePreferredAccountForPromo(accounts));
  histogram_tester.ExpectTotalCount("Signin.SelectionHeuristic.Reason", 1);
  histogram_tester.ExpectTotalCount(
      "Signin.SelectionHeuristicScore.PreferredAccount.SingleAccount", 1);

  // Fast forward 2 more hours (total 25 hours) -> records again.
  task_environment_.FastForwardBy(base::Hours(2));
  recorder.RecordSelectionHeuristicResult(
      accounts, ComputePreferredAccountForPromo(accounts));
  histogram_tester.ExpectTotalCount("Signin.SelectionHeuristic.Reason", 2);
  histogram_tester.ExpectTotalCount(
      "Signin.SelectionHeuristicScore.PreferredAccount.SingleAccount", 2);
}

}  // namespace signin
