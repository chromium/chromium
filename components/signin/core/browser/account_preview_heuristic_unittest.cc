// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/signin/core/browser/account_preview_heuristic.h"

#include <optional>
#include <vector>

#include "base/test/scoped_feature_list.h"
#include "base/time/time.h"
#include "components/signin/core/browser/account_preview_data.h"
#include "components/signin/core/browser/account_preview_data_service.h"
#include "components/signin/public/base/signin_buildflags.h"
#include "components/signin/public/base/signin_switches.h"
#include "components/sync/base/data_type.h"
#include "components/sync/protocol/sync_enums.pb.h"
#include "google_apis/gaia/gaia_id.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace signin {

namespace {

using ::testing::ElementsAre;
using ::testing::IsEmpty;

DevicePreview CreateDevicePreview(
    const std::string& guid,
    base::Time last_updated,
    sync_pb::SyncEnums_DeviceFormFactor form_factor) {
  DevicePreview device;
  device.cache_guid = guid;
  device.last_updated = last_updated;
  device.form_factor = form_factor;
  return device;
}

struct DataTypeCountsForTesting {
  size_t passwords = 0;
  size_t bookmarks = 0;
  size_t autofill = 0;
  size_t wallet = 0;
  size_t reading_list = 0;
#if BUILDFLAG(ENABLE_DICE_SUPPORT)
  size_t extensions = 0;
#endif
};

AccountPreviewData CreatePreviewData(DataTypeCountsForTesting counts = {},
                                     std::vector<DevicePreview> devices = {}) {
  AccountPreviewData data;
  if (counts.passwords > 0) {
    data.counts[syncer::PASSWORDS] = counts.passwords;
  }
  if (counts.bookmarks > 0) {
    data.counts[syncer::BOOKMARKS] = counts.bookmarks;
  }
  if (counts.autofill > 0) {
    data.counts[syncer::AUTOFILL] = counts.autofill;
  }
  if (counts.wallet > 0) {
    data.counts[syncer::AUTOFILL_WALLET_METADATA] = counts.wallet;
  }
  if (counts.reading_list > 0) {
    data.counts[syncer::READING_LIST] = counts.reading_list;
  }
#if BUILDFLAG(ENABLE_DICE_SUPPORT)
  if (counts.extensions > 0) {
    data.counts[syncer::EXTENSIONS] = counts.extensions;
  }
#endif
  data.devices = std::move(devices);
  return data;
}

class AccountPreviewHeuristicTest : public testing::Test {
  base::test::ScopedFeatureList scoped_feature_list_{
      switches::kEnableAccountPreviewPreferredAccount};
};

}  // namespace

// =============================================================================
// Account Data Types Criteria Tests (ComputeAccountPreviewPreference)
// =============================================================================

TEST(AccountPreviewHeuristicDisabledFeatureTest, ReturnsNullopt) {
  base::test::ScopedFeatureList feature_list;
  feature_list.InitAndDisableFeature(
      switches::kEnableAccountPreviewPreferredAccount);

  AccountPreviewData data = CreatePreviewData(
      {.passwords = 2 * switches::kPasswordsMedianThreshold.Get()});
  EXPECT_EQ(ComputeAccountPreviewPreference(GaiaId("user1"), data),
            std::nullopt);
  EXPECT_EQ(ComputePreferredAccountForPromo(
                {AccountPreviewHeuristicContext{.gaia_id = GaiaId("user1"),
                                                .preview_data = &data}})
                .preference,
            std::nullopt);
}

TEST_F(AccountPreviewHeuristicTest,
       ComputeAccountPreviewPreferencePreferredDataTypesRankingAndQuartile) {
  // Passwords: 2 * Median (ratio=2.0, quartile=kMedianToQ3)
  // Bookmarks: Median (ratio=1.0, quartile=kMedianToQ3)
  // Autofill: Q1 / 2 (ratio < 1.0, quartile=kBelowQ1)
  // Wallet: 0 -> Excluded
  AccountPreviewData data = CreatePreviewData({
      .passwords = 2 * switches::kPasswordsMedianThreshold.Get(),
      .bookmarks = switches::kBookmarksMedianThreshold.Get(),
      .autofill = switches::kAutofillQ1Threshold.Get() / 2,
  });

  auto pref = ComputeAccountPreviewPreference(GaiaId("user1"), data);
  ASSERT_TRUE(pref.has_value());
  EXPECT_EQ(pref->gaia_id, GaiaId("user1"));
  ASSERT_THAT(
      pref->preferred_data_types,
      ElementsAre(
          PreferredDataTypeInfo{.data_type = syncer::PASSWORDS,
                                .quartile = SyncDataQuartile::kMedianToQ3},
          PreferredDataTypeInfo{.data_type = syncer::BOOKMARKS,
                                .quartile = SyncDataQuartile::kMedianToQ3},
          PreferredDataTypeInfo{.data_type = syncer::AUTOFILL,
                                .quartile = SyncDataQuartile::kBelowQ1}));

  EXPECT_TRUE(pref->preferred_data_types[0].is_above_or_at_median());
  EXPECT_TRUE(pref->preferred_data_types[1].is_above_or_at_median());
  EXPECT_FALSE(pref->preferred_data_types[2].is_above_or_at_median());
}

TEST_F(AccountPreviewHeuristicTest,
       ComputeAccountPreviewPreferencePreferredDataTypesNoneAboveMedian) {
  // Passwords: Q1 / 4, Autofill: Q1 - 1
  // Autofill has a higher ratio to its median than Passwords, but both are
  // below their respective Q1 thresholds.
  AccountPreviewData data = CreatePreviewData({
      .passwords = switches::kPasswordsQ1Threshold.Get() / 4,
      .autofill = switches::kAutofillQ1Threshold.Get() - 1,
  });

  auto pref = ComputeAccountPreviewPreference(GaiaId("user1"), data);
  ASSERT_TRUE(pref.has_value());
  ASSERT_THAT(
      pref->preferred_data_types,
      ElementsAre(
          PreferredDataTypeInfo{.data_type = syncer::AUTOFILL,
                                .quartile = SyncDataQuartile::kBelowQ1},
          PreferredDataTypeInfo{.data_type = syncer::PASSWORDS,
                                .quartile = SyncDataQuartile::kBelowQ1}));

  EXPECT_FALSE(pref->preferred_data_types[0].is_above_or_at_median());
  EXPECT_FALSE(pref->preferred_data_types[1].is_above_or_at_median());
}

TEST_F(AccountPreviewHeuristicTest,
       ComputeAccountPreviewPreferenceEmptyDataTypes) {
  AccountPreviewData data = CreatePreviewData();
  auto pref = ComputeAccountPreviewPreference(GaiaId("user1"), data);
  ASSERT_TRUE(pref.has_value());
  EXPECT_THAT(pref->preferred_data_types, IsEmpty());
}

TEST_F(AccountPreviewHeuristicTest,
       ComputeAccountPreviewPreferenceFormFactorExtraction) {
  AccountPreviewData no_devices;
  auto pref_no_devices =
      ComputeAccountPreviewPreference(GaiaId("user1"), no_devices);
  ASSERT_TRUE(pref_no_devices.has_value());
  EXPECT_EQ(pref_no_devices->other_device_form_factor,
            sync_pb::SyncEnums_DeviceFormFactor_DEVICE_FORM_FACTOR_UNSPECIFIED);

  base::Time now = base::Time::Now();
  AccountPreviewData data_with_devices = CreatePreviewData(
      {}, {CreateDevicePreview(
               "guid1", now - base::Days(2),
               sync_pb::SyncEnums_DeviceFormFactor_DEVICE_FORM_FACTOR_DESKTOP),
           CreateDevicePreview(
               "guid2", now - base::Days(1),
               sync_pb::SyncEnums_DeviceFormFactor_DEVICE_FORM_FACTOR_PHONE)});

  auto pref_with_devices =
      ComputeAccountPreviewPreference(GaiaId("user2"), data_with_devices);
  ASSERT_TRUE(pref_with_devices.has_value());
  EXPECT_EQ(pref_with_devices->other_device_form_factor,
            sync_pb::SyncEnums_DeviceFormFactor_DEVICE_FORM_FACTOR_PHONE);
}

TEST_F(AccountPreviewHeuristicTest,
       ComputeAccountPreviewPreferenceTieBreakingPreservesDataTypeOrder) {
  // All counts are set to their exact median (ratio = 1.0, quartile =
  // kMedianToQ3):
  // Tie-breaking should preserve the priority declaration order (PASSWORDS ->
  // BOOKMARKS -> AUTOFILL -> AUTOFILL_WALLET_METADATA).
  AccountPreviewData data = CreatePreviewData({
      .passwords = switches::kPasswordsMedianThreshold.Get(),
      .bookmarks = switches::kBookmarksMedianThreshold.Get(),
      .autofill = switches::kAutofillMedianThreshold.Get(),
      .wallet = switches::kAutofillWalletMetadataMedianThreshold.Get(),
  });

  auto pref = ComputeAccountPreviewPreference(GaiaId("user1"), data);
  ASSERT_TRUE(pref.has_value());
  EXPECT_THAT(
      pref->preferred_data_types,
      ElementsAre(
          PreferredDataTypeInfo{.data_type = syncer::PASSWORDS,
                                .quartile = SyncDataQuartile::kMedianToQ3},
          PreferredDataTypeInfo{.data_type = syncer::BOOKMARKS,
                                .quartile = SyncDataQuartile::kMedianToQ3},
          PreferredDataTypeInfo{.data_type = syncer::AUTOFILL,
                                .quartile = SyncDataQuartile::kMedianToQ3},
          PreferredDataTypeInfo{.data_type = syncer::AUTOFILL_WALLET_METADATA,
                                .quartile = SyncDataQuartile::kMedianToQ3}));
}

// =============================================================================
// Multi-Account Heuristic Selection Tests (ComputePreferredAccountForPromo)
// =============================================================================

TEST_F(AccountPreviewHeuristicTest, EmptyListReturnsNullopt) {
  EXPECT_EQ(ComputePreferredAccountForPromo({}).preference, std::nullopt);
}

TEST_F(AccountPreviewHeuristicTest, SingleValidAccountReturnsPreference) {
  AccountPreviewData data = CreatePreviewData(
      {.passwords = switches::kPasswordsMedianThreshold.Get()},
      {CreateDevicePreview(
          "guid", base::Time::Now(),
          sync_pb::SyncEnums_DeviceFormFactor_DEVICE_FORM_FACTOR_PHONE)});
  AccountPreviewHeuristicContext account{
      .gaia_id = GaiaId("user1"),
      .preview_data = &data,
  };

  auto pref = ComputePreferredAccountForPromo({account}).preference;
  ASSERT_TRUE(pref.has_value());
  EXPECT_EQ(pref->gaia_id, GaiaId("user1"));
  EXPECT_THAT(pref->preferred_data_types,
              ElementsAre(PreferredDataTypeInfo{
                  .data_type = syncer::PASSWORDS,
                  .quartile = SyncDataQuartile::kMedianToQ3}));
  EXPECT_EQ(pref->other_device_form_factor,
            sync_pb::SyncEnums_DeviceFormFactor_DEVICE_FORM_FACTOR_PHONE);
}

TEST_F(AccountPreviewHeuristicTest,
       ComputeAccountPreviewPreferenceIncludesNewDataTypes) {
  AccountPreviewData data = CreatePreviewData({
      .reading_list = switches::kReadingListMedianThreshold.Get(),
#if BUILDFLAG(ENABLE_DICE_SUPPORT)
      .extensions = switches::kExtensionsMedianThreshold.Get(),
#endif
  });

  auto pref = ComputeAccountPreviewPreference(GaiaId("user1"), data);
  ASSERT_TRUE(pref.has_value());
#if BUILDFLAG(ENABLE_DICE_SUPPORT)
  EXPECT_THAT(
      pref->preferred_data_types,
      ElementsAre(
          PreferredDataTypeInfo{.data_type = syncer::READING_LIST,
                                .quartile = SyncDataQuartile::kMedianToQ3},
          PreferredDataTypeInfo{.data_type = syncer::EXTENSIONS,
                                .quartile = SyncDataQuartile::kMedianToQ3}));
#else
  EXPECT_THAT(pref->preferred_data_types,
              ElementsAre(PreferredDataTypeInfo{
                  .data_type = syncer::READING_LIST,
                  .quartile = SyncDataQuartile::kMedianToQ3}));
#endif
}

TEST_F(AccountPreviewHeuristicTest,
       ComputePreferredAccountForPromoScoreIgnoredDataTypesNotUsedForScoring) {
  // Account A only has reading list and extensions data.
  AccountPreviewData data_a = CreatePreviewData({
      .reading_list = 100,
#if BUILDFLAG(ENABLE_DICE_SUPPORT)
      .extensions = 100,
#endif
  });
  // Account B has a password (Q1 threshold).
  AccountPreviewData data_b = CreatePreviewData({
      .passwords = switches::kPasswordsQ1Threshold.Get(),
  });

  AccountPreviewHeuristicContext account_a{
      .gaia_id = GaiaId("user_a"),
      .preview_data = &data_a,
  };
  AccountPreviewHeuristicContext account_b{
      .gaia_id = GaiaId("user_b"),
      .preview_data = &data_b,
  };

  // Account B should win because new data types do not contribute to sync data
  // score. Account A score is 0.
  auto result = ComputePreferredAccountForPromo({account_a, account_b});
  EXPECT_EQ(result.selected_account, GaiaId("user_b"));
  EXPECT_EQ(result.selection_reason,
            AccountPreviewSelectionReason::kSyncDataScore);
  EXPECT_EQ(result.account_scores[GaiaId("user_a")], 0);
  EXPECT_GT(result.account_scores[GaiaId("user_b")], 0);
}

TEST_F(AccountPreviewHeuristicTest,
       AccountPreviewHeuristicContextIsRegularAccount) {
  AccountPreviewData data = CreatePreviewData(
      {.passwords = switches::kPasswordsMedianThreshold.Get()});
  AccountPreviewHeuristicContext context{
      .gaia_id = GaiaId("user1"),
      .preview_data = &data,
  };
  EXPECT_TRUE(context.is_regular_account());

  // Ineligible if managed.
  context.is_managed = true;
  EXPECT_FALSE(context.is_regular_account());
  context.is_managed = false;

  // Ineligible if child account.
  context.is_child = true;
  EXPECT_FALSE(context.is_regular_account());
  context.is_child = false;
}

TEST_F(AccountPreviewHeuristicTest, Disqualifications) {
  AccountPreviewData default_data = CreatePreviewData(
      {.passwords = switches::kPasswordsMedianThreshold.Get()});
  AccountPreviewHeuristicContext default_acc{
      .gaia_id = GaiaId("default"),
      .preview_data = &default_data,
  };

  // Managed candidate is not preferred.
  AccountPreviewData managed_data = CreatePreviewData(
      {.passwords = switches::kPasswordsQ3Threshold.Get() + 1});
  AccountPreviewHeuristicContext managed_candidate{
      .gaia_id = GaiaId("managed"),
      .preview_data = &managed_data,
      .is_managed = true,
  };
  auto pref = ComputePreferredAccountForPromo({default_acc, managed_candidate})
                  .preference;
  ASSERT_TRUE(pref.has_value());
  EXPECT_EQ(pref->gaia_id, GaiaId("default"));

  // Child candidate is not preferred.
  AccountPreviewData child_data = CreatePreviewData(
      {.passwords = switches::kPasswordsQ3Threshold.Get() + 1});
  AccountPreviewHeuristicContext child_candidate{
      .gaia_id = GaiaId("child"),
      .preview_data = &child_data,
      .is_child = true,
  };
  pref = ComputePreferredAccountForPromo({default_acc, child_candidate})
             .preference;
  ASSERT_TRUE(pref.has_value());
  EXPECT_EQ(pref->gaia_id, GaiaId("default"));

  // If default account is managed (Priority 1), it is selected.
  AccountPreviewData managed_default_data = CreatePreviewData(
      {.passwords = switches::kPasswordsMedianThreshold.Get()});
  AccountPreviewHeuristicContext managed_default{
      .gaia_id = GaiaId("managed_default"),
      .preview_data = &managed_default_data,
      .is_managed = true,
  };
  AccountPreviewData consumer_data = CreatePreviewData(
      {.passwords = switches::kPasswordsQ1Threshold.Get() / 2});
  AccountPreviewHeuristicContext consumer_candidate{
      .gaia_id = GaiaId("consumer"),
      .preview_data = &consumer_data,
  };
  pref = ComputePreferredAccountForPromo({managed_default, consumer_candidate})
             .preference;
  ASSERT_TRUE(pref.has_value());
  EXPECT_EQ(pref->gaia_id, GaiaId("managed_default"));

  // If default account is child (Priority 1), it is selected.
  AccountPreviewHeuristicContext child_default{
      .gaia_id = GaiaId("child_default"),
      .preview_data = &child_data,
      .is_child = true,
  };
  pref = ComputePreferredAccountForPromo({child_default, consumer_candidate})
             .preference;
  ASSERT_TRUE(pref.has_value());
  EXPECT_EQ(pref->gaia_id, GaiaId("child_default"));
}

TEST_F(AccountPreviewHeuristicTest, DefaultAgaPrimary) {
  AccountPreviewData default_aga_data = CreatePreviewData(
      {.passwords = switches::kPasswordsQ1Threshold.Get() / 2});
  AccountPreviewHeuristicContext default_aga{
      .gaia_id = GaiaId("default_aga"),
      .preview_data = &default_aga_data,
      .is_external_app_primary = true,
  };

  AccountPreviewData candidate_cross_more_data = CreatePreviewData(
      {.passwords = switches::kPasswordsMedianThreshold.Get()},
      {CreateDevicePreview(
          "guid", base::Time::Now(),
          sync_pb::SyncEnums_DeviceFormFactor_DEVICE_FORM_FACTOR_DESKTOP)});
  AccountPreviewHeuristicContext candidate_cross_more{
      .gaia_id = GaiaId("candidate_cross_more"),
      .preview_data = &candidate_cross_more_data,
  };
  // AGA default account is Priority 2, so it is selected over secondary
  // candidates.
  auto pref =
      ComputePreferredAccountForPromo({default_aga, candidate_cross_more})
          .preference;
  ASSERT_TRUE(pref.has_value());
  EXPECT_EQ(pref->gaia_id, GaiaId("default_aga"));

  AccountPreviewData candidate_single_more_data = CreatePreviewData(
      {.passwords = switches::kPasswordsMedianThreshold.Get()});
  AccountPreviewHeuristicContext candidate_single_more{
      .gaia_id = GaiaId("candidate_single_more"),
      .preview_data = &candidate_single_more_data,
  };
  pref = ComputePreferredAccountForPromo({default_aga, candidate_single_more})
             .preference;
  ASSERT_TRUE(pref.has_value());
  EXPECT_EQ(pref->gaia_id, GaiaId("default_aga"));

  AccountPreviewData candidate_cross_equal_data = CreatePreviewData(
      {.passwords = switches::kPasswordsQ1Threshold.Get() / 2},
      {CreateDevicePreview(
          "guid", base::Time::Now(),
          sync_pb::SyncEnums_DeviceFormFactor_DEVICE_FORM_FACTOR_DESKTOP)});
  AccountPreviewHeuristicContext candidate_cross_equal{
      .gaia_id = GaiaId("candidate_cross_equal"),
      .preview_data = &candidate_cross_equal_data,
  };
  pref = ComputePreferredAccountForPromo({default_aga, candidate_cross_equal})
             .preference;
  ASSERT_TRUE(pref.has_value());
  EXPECT_EQ(pref->gaia_id, GaiaId("default_aga"));
}

TEST_F(AccountPreviewHeuristicTest, CandidateCrossDeviceDefaultSingleDevice) {
  AccountPreviewData default_data = CreatePreviewData(
      {.passwords = switches::kPasswordsMedianThreshold.Get()});
  AccountPreviewHeuristicContext default_single{
      .gaia_id = GaiaId("default"),
      .preview_data = &default_data,
  };

  AccountPreviewData candidate_cross_equal_data = CreatePreviewData(
      {.passwords = switches::kPasswordsMedianThreshold.Get()},
      {CreateDevicePreview(
          "guid", base::Time::Now(),
          sync_pb::SyncEnums_DeviceFormFactor_DEVICE_FORM_FACTOR_PHONE)});
  AccountPreviewHeuristicContext candidate_cross_equal{
      .gaia_id = GaiaId("candidate_equal"),
      .preview_data = &candidate_cross_equal_data,
  };
  // Equal sync data -> Candidate wins.
  auto pref =
      ComputePreferredAccountForPromo({default_single, candidate_cross_equal})
          .preference;
  ASSERT_TRUE(pref.has_value());
  EXPECT_EQ(pref->gaia_id, GaiaId("candidate_equal"));

  AccountPreviewData candidate_cross_less_data = CreatePreviewData(
      {.passwords = switches::kPasswordsQ1Threshold.Get() / 2},
      {CreateDevicePreview(
          "guid", base::Time::Now(),
          sync_pb::SyncEnums_DeviceFormFactor_DEVICE_FORM_FACTOR_PHONE)});
  AccountPreviewHeuristicContext candidate_cross_less{
      .gaia_id = GaiaId("candidate_less"),
      .preview_data = &candidate_cross_less_data,
  };
  // Less sync data -> Default remains preferred.
  pref = ComputePreferredAccountForPromo({default_single, candidate_cross_less})
             .preference;
  ASSERT_TRUE(pref.has_value());
  EXPECT_EQ(pref->gaia_id, GaiaId("default"));
}

TEST_F(AccountPreviewHeuristicTest, CandidateCrossDeviceDefaultCrossDevice) {
  AccountPreviewData default_data = CreatePreviewData(
      {.passwords = switches::kPasswordsQ1Threshold.Get() / 2},
      {CreateDevicePreview(
          "guid1", base::Time::Now(),
          sync_pb::SyncEnums_DeviceFormFactor_DEVICE_FORM_FACTOR_DESKTOP)});
  AccountPreviewHeuristicContext default_cross{
      .gaia_id = GaiaId("default"),
      .preview_data = &default_data,
  };

  AccountPreviewData candidate_cross_equal_data = CreatePreviewData(
      {.passwords = switches::kPasswordsQ1Threshold.Get() / 2},
      {CreateDevicePreview(
          "guid2", base::Time::Now(),
          sync_pb::SyncEnums_DeviceFormFactor_DEVICE_FORM_FACTOR_PHONE)});
  AccountPreviewHeuristicContext candidate_cross_equal{
      .gaia_id = GaiaId("candidate_equal"),
      .preview_data = &candidate_cross_equal_data,
  };
  // Equal sync data -> Default remains preferred (requires strictly more).
  auto pref =
      ComputePreferredAccountForPromo({default_cross, candidate_cross_equal})
          .preference;
  ASSERT_TRUE(pref.has_value());
  EXPECT_EQ(pref->gaia_id, GaiaId("default"));

  AccountPreviewData candidate_cross_more_data = CreatePreviewData(
      {.passwords = switches::kPasswordsMedianThreshold.Get()},
      {CreateDevicePreview(
          "guid3", base::Time::Now(),
          sync_pb::SyncEnums_DeviceFormFactor_DEVICE_FORM_FACTOR_PHONE)});
  AccountPreviewHeuristicContext candidate_cross_more{
      .gaia_id = GaiaId("candidate_more"),
      .preview_data = &candidate_cross_more_data,
  };
  // Strictly more sync data -> Candidate wins.
  pref = ComputePreferredAccountForPromo({default_cross, candidate_cross_more})
             .preference;
  ASSERT_TRUE(pref.has_value());
  EXPECT_EQ(pref->gaia_id, GaiaId("candidate_more"));
}

TEST_F(AccountPreviewHeuristicTest, CandidateSingleDeviceDefaultSingleDevice) {
  AccountPreviewData default_data = CreatePreviewData(
      {.passwords = switches::kPasswordsQ1Threshold.Get() / 2});
  AccountPreviewHeuristicContext default_single{
      .gaia_id = GaiaId("default"),
      .preview_data = &default_data,
  };

  AccountPreviewData candidate_single_equal_data = CreatePreviewData(
      {.passwords = switches::kPasswordsQ1Threshold.Get() / 2});
  AccountPreviewHeuristicContext candidate_single_equal{
      .gaia_id = GaiaId("candidate_equal"),
      .preview_data = &candidate_single_equal_data,
  };
  // Equal sync data -> Default remains preferred.
  auto pref =
      ComputePreferredAccountForPromo({default_single, candidate_single_equal})
          .preference;
  ASSERT_TRUE(pref.has_value());
  EXPECT_EQ(pref->gaia_id, GaiaId("default"));

  AccountPreviewData candidate_single_more_data = CreatePreviewData(
      {.passwords = switches::kPasswordsMedianThreshold.Get()});
  AccountPreviewHeuristicContext candidate_single_more{
      .gaia_id = GaiaId("candidate_more"),
      .preview_data = &candidate_single_more_data,
  };
  // Strictly more sync data -> Candidate wins.
  pref =
      ComputePreferredAccountForPromo({default_single, candidate_single_more})
          .preference;
  ASSERT_TRUE(pref.has_value());
  EXPECT_EQ(pref->gaia_id, GaiaId("candidate_more"));
}

TEST_F(AccountPreviewHeuristicTest, CandidateAgaPrimary) {
  AccountPreviewData candidate_aga_data = CreatePreviewData(
      {.passwords = switches::kPasswordsQ1Threshold.Get() / 2});
  AccountPreviewHeuristicContext candidate_aga{
      .gaia_id = GaiaId("candidate_aga"),
      .preview_data = &candidate_aga_data,
      .is_external_app_primary = true,
  };

  AccountPreviewData default_cross_more_data = CreatePreviewData(
      {.passwords = switches::kPasswordsMedianThreshold.Get()},
      {CreateDevicePreview(
          "guid", base::Time::Now(),
          sync_pb::SyncEnums_DeviceFormFactor_DEVICE_FORM_FACTOR_DESKTOP)});
  AccountPreviewHeuristicContext default_cross_more{
      .gaia_id = GaiaId("default_cross"),
      .preview_data = &default_cross_more_data,
  };
  // AGA candidate (Priority 2) wins over non-managed default account.
  auto pref =
      ComputePreferredAccountForPromo({default_cross_more, candidate_aga})
          .preference;
  ASSERT_TRUE(pref.has_value());
  EXPECT_EQ(pref->gaia_id, GaiaId("candidate_aga"));

  AccountPreviewData default_single_more_data = CreatePreviewData(
      {.passwords = switches::kPasswordsMedianThreshold.Get()});
  AccountPreviewHeuristicContext default_single_more{
      .gaia_id = GaiaId("default_single"),
      .preview_data = &default_single_more_data,
  };
  pref = ComputePreferredAccountForPromo({default_single_more, candidate_aga})
             .preference;
  ASSERT_TRUE(pref.has_value());
  EXPECT_EQ(pref->gaia_id, GaiaId("candidate_aga"));

  AccountPreviewData default_cross_equal_data = CreatePreviewData(
      {.passwords = switches::kPasswordsQ1Threshold.Get() / 2},
      {CreateDevicePreview(
          "guid", base::Time::Now(),
          sync_pb::SyncEnums_DeviceFormFactor_DEVICE_FORM_FACTOR_DESKTOP)});
  AccountPreviewHeuristicContext default_cross_equal{
      .gaia_id = GaiaId("default_cross_equal"),
      .preview_data = &default_cross_equal_data,
  };
  pref = ComputePreferredAccountForPromo({default_cross_equal, candidate_aga})
             .preference;
  ASSERT_TRUE(pref.has_value());
  EXPECT_EQ(pref->gaia_id, GaiaId("candidate_aga"));

  // Managed default (Priority 1) beats AGA candidate (Priority 2).
  AccountPreviewData managed_default_data = CreatePreviewData(
      {.passwords = switches::kPasswordsQ1Threshold.Get() / 2});
  AccountPreviewHeuristicContext managed_default{
      .gaia_id = GaiaId("managed_default"),
      .preview_data = &managed_default_data,
      .is_managed = true,
  };
  pref = ComputePreferredAccountForPromo({managed_default, candidate_aga})
             .preference;
  ASSERT_TRUE(pref.has_value());
  EXPECT_EQ(pref->gaia_id, GaiaId("managed_default"));

  // AGA candidate that is managed is ignored, so consumer default remains
  // selected.
  AccountPreviewHeuristicContext managed_aga{
      .gaia_id = GaiaId("managed_aga"),
      .preview_data = &candidate_aga_data,
      .is_managed = true,
      .is_external_app_primary = true,
  };
  pref = ComputePreferredAccountForPromo({default_cross_more, managed_aga})
             .preference;
  ASSERT_TRUE(pref.has_value());
  EXPECT_EQ(pref->gaia_id, GaiaId("default_cross"));

  // AGA candidate that is child is ignored, so consumer default remains
  // selected.
  AccountPreviewHeuristicContext child_aga{
      .gaia_id = GaiaId("child_aga"),
      .preview_data = &candidate_aga_data,
      .is_child = true,
      .is_external_app_primary = true,
  };
  pref = ComputePreferredAccountForPromo({default_cross_more, child_aga})
             .preference;
  ASSERT_TRUE(pref.has_value());
  EXPECT_EQ(pref->gaia_id, GaiaId("default_cross"));
}

TEST_F(AccountPreviewHeuristicTest,
       MultiAccountCandidateSelectionAndTieBreaking) {
  AccountPreviewData data1 = CreatePreviewData(
      {.passwords = switches::kPasswordsQ1Threshold.Get() / 2});
  AccountPreviewHeuristicContext acc1{
      .gaia_id = GaiaId("acc1"),
      .preview_data = &data1,
  };
  AccountPreviewData data2 = CreatePreviewData(
      {.passwords = switches::kPasswordsQ1Threshold.Get() / 2});
  AccountPreviewHeuristicContext acc2{
      .gaia_id = GaiaId("acc2"),
      .preview_data = &data2,
  };
  AccountPreviewData data3 = CreatePreviewData(
      {.passwords = switches::kPasswordsMedianThreshold.Get()},
      {CreateDevicePreview(
          "guid", base::Time::Now(),
          sync_pb::SyncEnums_DeviceFormFactor_DEVICE_FORM_FACTOR_TABLET)});
  AccountPreviewHeuristicContext acc3{
      .gaia_id = GaiaId("acc3"),
      .preview_data = &data3,
  };

  // acc3 beats acc1 and acc2.
  auto pref = ComputePreferredAccountForPromo({acc1, acc2, acc3}).preference;
  ASSERT_TRUE(pref.has_value());
  EXPECT_EQ(pref->gaia_id, GaiaId("acc3"));
  EXPECT_EQ(pref->other_device_form_factor,
            sync_pb::SyncEnums_DeviceFormFactor_DEVICE_FORM_FACTOR_TABLET);

  // Tie between acc1 and acc2 preserves the earlier account (acc1).
  pref = ComputePreferredAccountForPromo({acc1, acc2}).preference;
  ASSERT_TRUE(pref.has_value());
  EXPECT_EQ(pref->gaia_id, GaiaId("acc1"));
}

TEST_F(AccountPreviewHeuristicTest,
       ExponentialQuartileScores1Q4Vs2Q3ScoreTieHigherQuartileWins) {
  // Account 1: 1 Q4 (Passwords >= Q3 -> kAboveQ3 = Q4, score = 8)
  // Account 2: 2 Q3 (Bookmarks >= Median -> kMedianToQ3 = Q3, score = 4;
  //                  Autofill >= Median -> kMedianToQ3 = Q3, score = 4)
  // Both accounts have total sync data score = 8.
  // Account 1 wins the tie-breaker because it has a higher Q4 count (1 vs 0).
  AccountPreviewData data_1q4 = CreatePreviewData(
      {.passwords = switches::kPasswordsQ3Threshold.Get() + 1});
  AccountPreviewHeuristicContext acc_1q4{
      .gaia_id = GaiaId("acc_1q4"),
      .preview_data = &data_1q4,
  };

  AccountPreviewData data_2q3 = CreatePreviewData({
      .bookmarks = switches::kBookmarksMedianThreshold.Get(),
      .autofill = switches::kAutofillMedianThreshold.Get(),
  });
  AccountPreviewHeuristicContext acc_2q3{
      .gaia_id = GaiaId("acc_2q3"),
      .preview_data = &data_2q3,
  };

  // acc_1q4 is preferred as it has higher data type score.
  auto pref = ComputePreferredAccountForPromo({acc_1q4, acc_2q3}).preference;
  ASSERT_TRUE(pref.has_value());
  EXPECT_EQ(pref->gaia_id, GaiaId("acc_1q4"));

  pref = ComputePreferredAccountForPromo({acc_2q3, acc_1q4}).preference;
  ASSERT_TRUE(pref.has_value());
  EXPECT_EQ(pref->gaia_id, GaiaId("acc_1q4"));
}

TEST_F(AccountPreviewHeuristicTest,
       ExponentialQuartileScores1Q4Vs2Q3Plus1Q1HigherScoreWins) {
  // Account 1: 1 Q4 (Passwords >= Q3 -> kAboveQ3 = Q4, score = 8)
  // Account 2: 2 Q3 + 1 Q1 (Bookmarks >= Median -> kMedianToQ3 = Q3, score = 4;
  //                         Autofill >= Median -> kMedianToQ3 = Q3, score = 4;
  //                         Passwords < Q1 -> kBelowQ1 = Q1, score = 1)
  // Account 2 has total sync data score = 4 + 4 + 1 = 9, which is strictly
  // greater than Account 1's score of 8. Therefore, Account 2 wins.
  AccountPreviewData data_1q4 = CreatePreviewData(
      {.passwords = switches::kPasswordsQ3Threshold.Get() + 1});
  AccountPreviewHeuristicContext acc_1q4{
      .gaia_id = GaiaId("acc_1q4"),
      .preview_data = &data_1q4,
  };

  AccountPreviewData data_2q3_1q1 = CreatePreviewData({
      .passwords = switches::kPasswordsQ1Threshold.Get() / 2,
      .bookmarks = switches::kBookmarksMedianThreshold.Get(),
      .autofill = switches::kAutofillMedianThreshold.Get(),
  });
  AccountPreviewHeuristicContext acc_2q3_1q1{
      .gaia_id = GaiaId("acc_2q3_1q1"),
      .preview_data = &data_2q3_1q1,
  };

  // acc_2q3_1q1 is preferred as it has higher data type score.
  auto pref =
      ComputePreferredAccountForPromo({acc_1q4, acc_2q3_1q1}).preference;
  ASSERT_TRUE(pref.has_value());
  EXPECT_EQ(pref->gaia_id, GaiaId("acc_2q3_1q1"));

  pref = ComputePreferredAccountForPromo({acc_2q3_1q1, acc_1q4}).preference;
  ASSERT_TRUE(pref.has_value());
  EXPECT_EQ(pref->gaia_id, GaiaId("acc_2q3_1q1"));
}

TEST_F(AccountPreviewHeuristicTest,
       ExponentialQuartileScores1Q4VsLowQuartilesHigherScoreWins) {
  // Account 1: 1 Q4 (Passwords >= Q3 -> kAboveQ3 = Q4, score = 8)
  // Account 2: 3 Q1s + 1 Q2 (Passwords < Q1 -> kBelowQ1 = Q1, score = 1;
  //                          Bookmarks < Q1 -> kBelowQ1 = Q1, score = 1;
  //                          Autofill < Q1 -> kBelowQ1 = Q1, score = 1;
  //                          Wallet in [Q1, Median) -> kQ1ToMedian = Q2, score
  //                          = 2)
  // Account 1 has score 8, while Account 2 has score 1 + 1 + 1 + 2 = 5.
  // Under the exponential scoring, Account 1 wins decisively with score 8 > 5.
  AccountPreviewData data_1q4 = CreatePreviewData(
      {.passwords = switches::kPasswordsQ3Threshold.Get() + 1});
  AccountPreviewHeuristicContext acc_1q4{
      .gaia_id = GaiaId("acc_1q4"),
      .preview_data = &data_1q4,
  };

  AccountPreviewData data_low_quartiles = CreatePreviewData({
      .passwords = switches::kPasswordsQ1Threshold.Get() / 2,
      .bookmarks = switches::kBookmarksQ1Threshold.Get() / 2,
      .autofill = switches::kAutofillQ1Threshold.Get() / 2,
      .wallet = switches::kAutofillWalletMetadataQ1Threshold.Get(),
  });
  AccountPreviewHeuristicContext acc_low_quartiles{
      .gaia_id = GaiaId("acc_low_quartiles"),
      .preview_data = &data_low_quartiles,
  };

  // acc_1q4 is preferred as it has higher sync data score.
  auto pref =
      ComputePreferredAccountForPromo({acc_1q4, acc_low_quartiles}).preference;
  ASSERT_TRUE(pref.has_value());
  EXPECT_EQ(pref->gaia_id, GaiaId("acc_1q4"));

  pref =
      ComputePreferredAccountForPromo({acc_low_quartiles, acc_1q4}).preference;
  ASSERT_TRUE(pref.has_value());
}

TEST_F(AccountPreviewHeuristicTest, ComputePreferredAccountForPromoResult) {
  AccountPreviewData data0 = CreatePreviewData({
      .passwords = switches::kPasswordsQ1Threshold.Get(),
  });
  AccountPreviewData data1 = CreatePreviewData({
      .passwords = switches::kPasswordsQ3Threshold.Get(),
  });

  AccountPreviewHeuristicContext acc0{
      .gaia_id = GaiaId("acc0"),
      .preview_data = &data0,
  };
  AccountPreviewHeuristicContext acc1{
      .gaia_id = GaiaId("acc1"),
      .preview_data = &data1,
  };

  // Empty accounts
  AccountPreviewSelectionResult empty_result =
      ComputePreferredAccountForPromo({});
  EXPECT_EQ(empty_result.selected_account, std::nullopt);
  EXPECT_EQ(empty_result.selection_reason,
            AccountPreviewSelectionReason::kNoSelection);
  EXPECT_EQ(empty_result.preference, std::nullopt);
  EXPECT_TRUE(empty_result.account_scores.empty());

  // Priority 1: default account not regular -> kNonRegularDefault
  acc0.is_managed = true;
  std::vector<AccountPreviewHeuristicContext> accounts1 = {acc0, acc1};
  AccountPreviewSelectionResult p1_result =
      ComputePreferredAccountForPromo(accounts1);
  EXPECT_EQ(p1_result.selected_account, GaiaId("acc0"));
  EXPECT_EQ(p1_result.selection_reason,
            AccountPreviewSelectionReason::kNonRegularDefault);
  ASSERT_TRUE(p1_result.preference.has_value());
  EXPECT_EQ(p1_result.preference->gaia_id, GaiaId("acc0"));
  EXPECT_TRUE(p1_result.account_scores.empty());
  acc0.is_managed = false;

  // Priority 2: AGA regular account exists -> kExternalAppPrimary
  acc1.is_external_app_primary = true;
  std::vector<AccountPreviewHeuristicContext> accounts2 = {acc0, acc1};
  AccountPreviewSelectionResult p2_result =
      ComputePreferredAccountForPromo(accounts2);
  EXPECT_EQ(p2_result.selected_account, GaiaId("acc1"));
  EXPECT_EQ(p2_result.selection_reason,
            AccountPreviewSelectionReason::kExternalAppPrimary);
  ASSERT_TRUE(p2_result.preference.has_value());
  EXPECT_EQ(p2_result.preference->gaia_id, GaiaId("acc1"));
  EXPECT_TRUE(p2_result.account_scores.empty());
  acc1.is_external_app_primary = false;

  // Priority 3: compares regular accounts using scores -> kSyncDataScore.
  // Non-regular accounts (acc2) are ignored and omitted from account_scores.
  AccountPreviewData data2 = CreatePreviewData({
      .passwords = switches::kPasswordsQ3Threshold.Get(),
  });
  AccountPreviewHeuristicContext acc2{
      .gaia_id = GaiaId("acc2"),
      .preview_data = &data2,
      .is_managed = true,
  };
  std::vector<AccountPreviewHeuristicContext> accounts3 = {acc0, acc1, acc2};
  AccountPreviewSelectionResult p3_result =
      ComputePreferredAccountForPromo(accounts3);
  EXPECT_EQ(p3_result.selected_account, GaiaId("acc1"));
  EXPECT_EQ(p3_result.selection_reason,
            AccountPreviewSelectionReason::kSyncDataScore);
  ASSERT_TRUE(p3_result.preference.has_value());
  EXPECT_EQ(p3_result.preference->gaia_id, GaiaId("acc1"));
  EXPECT_EQ(p3_result.account_scores.at(GaiaId("acc0")), 2);
  EXPECT_EQ(p3_result.account_scores.at(GaiaId("acc1")), 8);
  EXPECT_FALSE(p3_result.account_scores.contains(GaiaId("acc2")));
}

// =============================================================================
// Account Switching Selection Heuristic Tests
// (ComputeAccountSwitchingSelection)
// =============================================================================

TEST_F(AccountPreviewHeuristicTest,
       ComputeAccountSwitchingSelectionPreconditions) {
  AccountSwitchingSelectionResult empty_result =
      ComputeAccountSwitchingSelection({});
  EXPECT_EQ(empty_result.outcome,
            AccountSwitchingSelectionOutcome::kWouldNotShowNoPrimaryAccount);
  EXPECT_EQ(empty_result.selected_account, std::nullopt);
  EXPECT_EQ(empty_result.preference, std::nullopt);

  AccountPreviewData data = CreatePreviewData({
      .passwords = switches::kPasswordsQ3Threshold.Get(),
  });
  AccountPreviewHeuristicContext signed_out_acc0{
      .gaia_id = GaiaId("acc0"),
      .preview_data = &data,
      .is_primary = false,
  };
  AccountPreviewHeuristicContext signed_out_acc1{
      .gaia_id = GaiaId("acc1"),
      .preview_data = &data,
  };
  AccountSwitchingSelectionResult no_primary_result =
      ComputeAccountSwitchingSelection({signed_out_acc0, signed_out_acc1});
  EXPECT_EQ(no_primary_result.outcome,
            AccountSwitchingSelectionOutcome::kWouldNotShowNoPrimaryAccount);
  EXPECT_EQ(no_primary_result.selected_account, std::nullopt);

  AccountPreviewHeuristicContext primary_acc{
      .gaia_id = GaiaId("acc0"),
      .preview_data = &data,
      .is_primary = true,
  };
  AccountSwitchingSelectionResult single_result =
      ComputeAccountSwitchingSelection({primary_acc});
  EXPECT_EQ(single_result.outcome,
            AccountSwitchingSelectionOutcome::kWouldNotShowNotEnoughAccounts);
  EXPECT_EQ(single_result.selected_account, std::nullopt);
  EXPECT_EQ(single_result.preference, std::nullopt);
}

TEST_F(AccountPreviewHeuristicTest,
       ComputeAccountSwitchingSelectionDisqualifications) {
  std::vector<DevicePreview> devices = {CreateDevicePreview(
      "dev1", base::Time::Now(),
      sync_pb::SyncEnums_DeviceFormFactor_DEVICE_FORM_FACTOR_PHONE)};
  AccountPreviewData low_data = CreatePreviewData();
  AccountPreviewData high_data_no_devices = CreatePreviewData({
      .passwords = switches::kPasswordsQ3Threshold.Get(),
  });
  AccountPreviewData high_data = CreatePreviewData(
      {
          .passwords = switches::kPasswordsQ3Threshold.Get(),
      },
      devices);

  AccountPreviewHeuristicContext primary{
      .gaia_id = GaiaId("primary"),
      .preview_data = &low_data,
      .is_primary = true,
  };
  AccountPreviewHeuristicContext secondary{
      .gaia_id = GaiaId("secondary"),
      .preview_data = &high_data,
  };

  // Primary is managed -> kWouldNotShowNonRegularPrimaryAccount.
  primary.is_managed = true;
  auto res = ComputeAccountSwitchingSelection({primary, secondary});
  EXPECT_EQ(
      res.outcome,
      AccountSwitchingSelectionOutcome::kWouldNotShowNonRegularPrimaryAccount);
  EXPECT_EQ(res.selected_account, std::nullopt);
  primary.is_managed = false;

  // Primary is child -> kWouldNotShowNonRegularPrimaryAccount.
  primary.is_child = true;
  res = ComputeAccountSwitchingSelection({primary, secondary});
  EXPECT_EQ(
      res.outcome,
      AccountSwitchingSelectionOutcome::kWouldNotShowNonRegularPrimaryAccount);
  EXPECT_EQ(res.selected_account, std::nullopt);
  primary.is_child = false;

  // Primary is AGA -> kWouldNotShowExternalAppPrimaryAccount.
  primary.is_external_app_primary = true;
  res = ComputeAccountSwitchingSelection({primary, secondary});
  EXPECT_EQ(
      res.outcome,
      AccountSwitchingSelectionOutcome::kWouldNotShowExternalAppPrimaryAccount);
  EXPECT_EQ(res.selected_account, std::nullopt);
  primary.is_external_app_primary = false;

  // Primary has missing preview data -> kWouldNotShowPrimaryMissingPreviewData.
  primary.preview_data = nullptr;
  res = ComputeAccountSwitchingSelection({primary, secondary});
  EXPECT_EQ(
      res.outcome,
      AccountSwitchingSelectionOutcome::kWouldNotShowPrimaryMissingPreviewData);
  EXPECT_EQ(res.selected_account, std::nullopt);
  primary.preview_data = &low_data;

  // Regular secondary has missing preview data ->
  // kWouldNotShowSecondaryMissingPreviewData.
  secondary.preview_data = nullptr;
  res = ComputeAccountSwitchingSelection({primary, secondary});
  EXPECT_EQ(res.outcome, AccountSwitchingSelectionOutcome::
                             kWouldNotShowSecondaryMissingPreviewData);
  EXPECT_EQ(res.selected_account, std::nullopt);
  secondary.preview_data = &high_data;

  // Regular secondary has no other devices ->
  // kWouldNotShowNoSecondaryWithOtherDevices.
  AccountPreviewHeuristicContext secondary_no_devices{
      .gaia_id = GaiaId("secondary_no_devices"),
      .preview_data = &high_data_no_devices,
  };
  res = ComputeAccountSwitchingSelection({primary, secondary_no_devices});
  EXPECT_EQ(res.outcome, AccountSwitchingSelectionOutcome::
                             kWouldNotShowNoSecondaryWithOtherDevices);
  EXPECT_EQ(res.selected_account, std::nullopt);

  // Only secondary is managed -> kWouldNotShowNoRegularSecondaryAccount.
  secondary.is_managed = true;
  res = ComputeAccountSwitchingSelection({primary, secondary});
  EXPECT_EQ(
      res.outcome,
      AccountSwitchingSelectionOutcome::kWouldNotShowNoRegularSecondaryAccount);
  EXPECT_EQ(res.selected_account, std::nullopt);

  // Managed secondary, secondary with missing preview data, and single-device
  // secondary are ignored while regular secondary with other devices is
  // selected.
  AccountPreviewHeuristicContext secondary_missing_data{
      .gaia_id = GaiaId("secondary_missing_data"),
      .preview_data = nullptr,
  };
  AccountPreviewHeuristicContext regular_secondary{
      .gaia_id = GaiaId("regular_secondary"),
      .preview_data = &high_data,
  };
  res = ComputeAccountSwitchingSelection(
      {primary, secondary, secondary_missing_data, secondary_no_devices,
       regular_secondary});
  EXPECT_EQ(res.outcome,
            AccountSwitchingSelectionOutcome::kWouldShowLowPrimaryScore);
  EXPECT_EQ(res.selected_account, GaiaId("regular_secondary"));
}

TEST_F(AccountPreviewHeuristicTest,
       ComputeAccountSwitchingSelectionPrimaryLowScore0To2) {
  std::vector<DevicePreview> devices = {CreateDevicePreview(
      "dev1", base::Time::Now(),
      sync_pb::SyncEnums_DeviceFormFactor_DEVICE_FORM_FACTOR_PHONE)};
  // Primary Y in {0, 2}: needs secondary X >= Y + 1.
  AccountPreviewData score0 = CreatePreviewData({}, devices);
  AccountPreviewData score1 = CreatePreviewData(
      {
          .passwords = switches::kPasswordsQ1Threshold.Get() / 2,
      },
      devices);
  AccountPreviewData score2 = CreatePreviewData(
      {
          .passwords = switches::kPasswordsQ1Threshold.Get(),
      },
      devices);
  AccountPreviewData score3 = CreatePreviewData(
      {
          .passwords = switches::kPasswordsQ1Threshold.Get(),
          .bookmarks = switches::kBookmarksQ1Threshold.Get() / 2,
      },
      devices);

  AccountPreviewHeuristicContext primary0{
      .gaia_id = GaiaId("primary0"),
      .preview_data = &score0,
      .is_primary = true,
  };
  AccountPreviewHeuristicContext sec0{
      .gaia_id = GaiaId("sec0"),
      .preview_data = &score0,
  };
  AccountPreviewHeuristicContext sec1{
      .gaia_id = GaiaId("sec1"),
      .preview_data = &score1,
  };
  AccountPreviewHeuristicContext sec2{
      .gaia_id = GaiaId("sec2"),
      .preview_data = &score2,
  };
  AccountPreviewHeuristicContext sec3{
      .gaia_id = GaiaId("sec3"),
      .preview_data = &score3,
  };

  // Y = 0, X = 0 -> does not meet threshold (X >= 1 required).
  auto res = ComputeAccountSwitchingSelection({primary0, sec0});
  EXPECT_EQ(res.outcome, AccountSwitchingSelectionOutcome::
                             kWouldNotShowSecondaryDoesNotMeetThreshold);
  EXPECT_EQ(res.selected_account, std::nullopt);

  // Y = 0, X = 1 -> meets threshold (1 >= 0 + 1).
  res = ComputeAccountSwitchingSelection({primary0, sec1});
  EXPECT_EQ(res.outcome,
            AccountSwitchingSelectionOutcome::kWouldShowLowPrimaryScore);
  EXPECT_EQ(res.selected_account, GaiaId("sec1"));
  ASSERT_TRUE(res.preference.has_value());
  EXPECT_EQ(res.preference->gaia_id, GaiaId("sec1"));

  // Primary Y = 2: needs X >= 3.
  AccountPreviewHeuristicContext primary2{
      .gaia_id = GaiaId("primary2"),
      .preview_data = &score2,
      .is_primary = true,
  };

  // Y = 2, X = 2 -> does not meet threshold (X >= 3 required).
  res = ComputeAccountSwitchingSelection({primary2, sec2});
  EXPECT_EQ(res.outcome, AccountSwitchingSelectionOutcome::
                             kWouldNotShowSecondaryDoesNotMeetThreshold);
  EXPECT_EQ(res.selected_account, std::nullopt);

  // Y = 2, X = 3 -> meets threshold (3 >= 2 + 1).
  res = ComputeAccountSwitchingSelection({primary2, sec3});
  EXPECT_EQ(res.outcome,
            AccountSwitchingSelectionOutcome::kWouldShowLowPrimaryScore);
  EXPECT_EQ(res.selected_account, GaiaId("sec3"));
}

TEST_F(AccountPreviewHeuristicTest,
       ComputeAccountSwitchingSelectionPrimaryDoubledScore3To5) {
  std::vector<DevicePreview> devices = {CreateDevicePreview(
      "dev1", base::Time::Now(),
      sync_pb::SyncEnums_DeviceFormFactor_DEVICE_FORM_FACTOR_PHONE)};
  // Primary Y = 3 (1xQ2 + 1xQ1): needs X > 6 (i.e. X >= 7).
  AccountPreviewData score3 = CreatePreviewData(
      {
          .passwords = switches::kPasswordsQ1Threshold.Get(),
          .bookmarks = switches::kBookmarksQ1Threshold.Get() / 2,
      },
      devices);
  AccountPreviewData score6 = CreatePreviewData(
      {
          .passwords = switches::kPasswordsMedianThreshold.Get(),
          .bookmarks = switches::kBookmarksQ1Threshold.Get(),
      },
      devices);
  AccountPreviewData score7 = CreatePreviewData(
      {
          .passwords = switches::kPasswordsMedianThreshold.Get(),
          .bookmarks = switches::kBookmarksQ1Threshold.Get(),
          .autofill = switches::kAutofillQ1Threshold.Get() / 2,
      },
      devices);
  // Primary Y = 5 (1xQ3 + 1xQ1): needs X > 10 (i.e. X >= 11).
  AccountPreviewData score5 = CreatePreviewData(
      {
          .passwords = switches::kPasswordsMedianThreshold.Get(),
          .bookmarks = switches::kBookmarksQ1Threshold.Get() / 2,
      },
      devices);
  AccountPreviewData score10 = CreatePreviewData(
      {
          .passwords = switches::kPasswordsQ3Threshold.Get(),
          .bookmarks = switches::kBookmarksQ1Threshold.Get(),
      },
      devices);
  AccountPreviewData score11 = CreatePreviewData(
      {
          .passwords = switches::kPasswordsQ3Threshold.Get(),
          .bookmarks = switches::kBookmarksQ1Threshold.Get(),
          .autofill = switches::kAutofillQ1Threshold.Get() / 2,
      },
      devices);

  AccountPreviewHeuristicContext primary3{
      .gaia_id = GaiaId("primary3"),
      .preview_data = &score3,
      .is_primary = true,
  };
  AccountPreviewHeuristicContext sec6{
      .gaia_id = GaiaId("sec6"),
      .preview_data = &score6,
  };
  AccountPreviewHeuristicContext sec7{
      .gaia_id = GaiaId("sec7"),
      .preview_data = &score7,
  };

  // Y = 3, X = 6 -> does not meet threshold (6 <= 2 * 3).
  auto res = ComputeAccountSwitchingSelection({primary3, sec6});
  EXPECT_EQ(res.outcome, AccountSwitchingSelectionOutcome::
                             kWouldNotShowSecondaryDoesNotMeetThreshold);
  EXPECT_EQ(res.selected_account, std::nullopt);

  // Y = 3, X = 7 -> meets threshold (7 > 2 * 3).
  res = ComputeAccountSwitchingSelection({primary3, sec7});
  EXPECT_EQ(res.outcome,
            AccountSwitchingSelectionOutcome::kWouldShowDoubledPrimaryScore);
  EXPECT_EQ(res.selected_account, GaiaId("sec7"));

  // Y = 5, X = 10 -> does not meet threshold (10 <= 2 * 5).
  AccountPreviewHeuristicContext primary5{
      .gaia_id = GaiaId("primary5"),
      .preview_data = &score5,
      .is_primary = true,
  };
  AccountPreviewHeuristicContext sec10{
      .gaia_id = GaiaId("sec10"),
      .preview_data = &score10,
  };
  AccountPreviewHeuristicContext sec11{
      .gaia_id = GaiaId("sec11"),
      .preview_data = &score11,
  };
  res = ComputeAccountSwitchingSelection({primary5, sec10});
  EXPECT_EQ(res.outcome, AccountSwitchingSelectionOutcome::
                             kWouldNotShowSecondaryDoesNotMeetThreshold);
  EXPECT_EQ(res.selected_account, std::nullopt);

  // Y = 5, X = 11 -> meets threshold (11 > 2 * 5).
  res = ComputeAccountSwitchingSelection({primary5, sec11});
  EXPECT_EQ(res.outcome,
            AccountSwitchingSelectionOutcome::kWouldShowDoubledPrimaryScore);
  EXPECT_EQ(res.selected_account, GaiaId("sec11"));
}

TEST_F(AccountPreviewHeuristicTest,
       ComputeAccountSwitchingSelectionPrimaryExceedsUpperLimit6Plus) {
  std::vector<DevicePreview> devices = {CreateDevicePreview(
      "dev1", base::Time::Now(),
      sync_pb::SyncEnums_DeviceFormFactor_DEVICE_FORM_FACTOR_PHONE)};
  // Primary X = 6 (1xQ3 + 1xQ2): cutoff at 6+, never switches even if secondary
  // has maximum possible score (32).
  AccountPreviewData score6 = CreatePreviewData({
      .passwords = switches::kPasswordsMedianThreshold.Get(),
      .bookmarks = switches::kBookmarksQ1Threshold.Get(),
  });
  AccountPreviewData score32 = CreatePreviewData(
      {
          .passwords = switches::kPasswordsQ3Threshold.Get(),
          .bookmarks = switches::kBookmarksQ3Threshold.Get(),
          .autofill = switches::kAutofillQ3Threshold.Get(),
          .wallet = switches::kAutofillWalletMetadataQ3Threshold.Get(),
      },
      devices);

  AccountPreviewHeuristicContext primary6{
      .gaia_id = GaiaId("primary6"),
      .preview_data = &score6,
      .is_primary = true,
  };
  AccountPreviewHeuristicContext sec32{
      .gaia_id = GaiaId("sec32"),
      .preview_data = &score32,
  };

  auto res = ComputeAccountSwitchingSelection({primary6, sec32});
  EXPECT_EQ(
      res.outcome,
      AccountSwitchingSelectionOutcome::kWouldNotShowPrimaryExceedsUpperLimit);
  EXPECT_EQ(res.selected_account, std::nullopt);
  EXPECT_EQ(res.preference, std::nullopt);
}

TEST_F(AccountPreviewHeuristicTest,
       ComputeAccountSwitchingSelectionSecondaryTieBreaking) {
  std::vector<DevicePreview> devices = {CreateDevicePreview(
      "dev1", base::Time::Now(),
      sync_pb::SyncEnums_DeviceFormFactor_DEVICE_FORM_FACTOR_PHONE)};
  AccountPreviewData primary_data = CreatePreviewData();
  // Secondary A: 2xQ3 (score 8) with device
  AccountPreviewData sec_2q3_data = CreatePreviewData(
      {
          .passwords = switches::kPasswordsMedianThreshold.Get(),
          .bookmarks = switches::kBookmarksMedianThreshold.Get(),
      },
      devices);
  // Secondary B: 1xQ4 (score 8, higher Q4 count wins tie-breaker) with device
  AccountPreviewData sec_1q4_data = CreatePreviewData(
      {
          .passwords = switches::kPasswordsQ3Threshold.Get(),
      },
      devices);

  AccountPreviewHeuristicContext primary{
      .gaia_id = GaiaId("primary"),
      .preview_data = &primary_data,
      .is_primary = true,
  };
  AccountPreviewHeuristicContext sec_2q3{
      .gaia_id = GaiaId("sec_2q3"),
      .preview_data = &sec_2q3_data,
  };
  AccountPreviewHeuristicContext sec_1q4{
      .gaia_id = GaiaId("sec_1q4"),
      .preview_data = &sec_1q4_data,
  };

  auto res = ComputeAccountSwitchingSelection({primary, sec_2q3, sec_1q4});
  EXPECT_EQ(res.outcome,
            AccountSwitchingSelectionOutcome::kWouldShowLowPrimaryScore);
  EXPECT_EQ(res.selected_account, GaiaId("sec_1q4"));
}

}  // namespace signin
