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
  EXPECT_EQ(ComputePreferredAccountForPromo({AccountPreviewHeuristicContext{
                .gaia_id = GaiaId("user1"), .preview_data = raw_ref(data)}}),
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
  EXPECT_EQ(ComputePreferredAccountForPromo({}), std::nullopt);
}

TEST_F(AccountPreviewHeuristicTest, SingleValidAccountReturnsPreference) {
  AccountPreviewData data = CreatePreviewData(
      {.passwords = switches::kPasswordsMedianThreshold.Get()},
      {CreateDevicePreview(
          "guid", base::Time::Now(),
          sync_pb::SyncEnums_DeviceFormFactor_DEVICE_FORM_FACTOR_PHONE)});
  AccountPreviewHeuristicContext account{
      .gaia_id = GaiaId("user1"),
      .preview_data = raw_ref(data),
  };

  auto pref = ComputePreferredAccountForPromo({account});
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
       AccountPreviewHeuristicContextIsRegularAccount) {
  AccountPreviewData data = CreatePreviewData(
      {.passwords = switches::kPasswordsMedianThreshold.Get()});
  AccountPreviewHeuristicContext context{
      .gaia_id = GaiaId("user1"),
      .preview_data = raw_ref(data),
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
      .preview_data = raw_ref(default_data),
  };

  // Managed candidate is not preferred.
  AccountPreviewData managed_data = CreatePreviewData(
      {.passwords = switches::kPasswordsQ3Threshold.Get() + 1});
  AccountPreviewHeuristicContext managed_candidate{
      .gaia_id = GaiaId("managed"),
      .preview_data = raw_ref(managed_data),
      .is_managed = true,
  };
  auto pref = ComputePreferredAccountForPromo({default_acc, managed_candidate});
  ASSERT_TRUE(pref.has_value());
  EXPECT_EQ(pref->gaia_id, GaiaId("default"));

  // Child candidate is not preferred.
  AccountPreviewData child_data = CreatePreviewData(
      {.passwords = switches::kPasswordsQ3Threshold.Get() + 1});
  AccountPreviewHeuristicContext child_candidate{
      .gaia_id = GaiaId("child"),
      .preview_data = raw_ref(child_data),
      .is_child = true,
  };
  pref = ComputePreferredAccountForPromo({default_acc, child_candidate});
  ASSERT_TRUE(pref.has_value());
  EXPECT_EQ(pref->gaia_id, GaiaId("default"));

  // If default account is managed (Priority 1), it is selected.
  AccountPreviewData managed_default_data = CreatePreviewData(
      {.passwords = switches::kPasswordsMedianThreshold.Get()});
  AccountPreviewHeuristicContext managed_default{
      .gaia_id = GaiaId("managed_default"),
      .preview_data = raw_ref(managed_default_data),
      .is_managed = true,
  };
  AccountPreviewData consumer_data = CreatePreviewData(
      {.passwords = switches::kPasswordsQ1Threshold.Get() / 2});
  AccountPreviewHeuristicContext consumer_candidate{
      .gaia_id = GaiaId("consumer"),
      .preview_data = raw_ref(consumer_data),
  };
  pref = ComputePreferredAccountForPromo({managed_default, consumer_candidate});
  ASSERT_TRUE(pref.has_value());
  EXPECT_EQ(pref->gaia_id, GaiaId("managed_default"));

  // If default account is child (Priority 1), it is selected.
  AccountPreviewHeuristicContext child_default{
      .gaia_id = GaiaId("child_default"),
      .preview_data = raw_ref(child_data),
      .is_child = true,
  };
  pref = ComputePreferredAccountForPromo({child_default, consumer_candidate});
  ASSERT_TRUE(pref.has_value());
  EXPECT_EQ(pref->gaia_id, GaiaId("child_default"));
}

TEST_F(AccountPreviewHeuristicTest, DefaultAgaPrimary) {
  AccountPreviewData default_aga_data = CreatePreviewData(
      {.passwords = switches::kPasswordsQ1Threshold.Get() / 2});
  AccountPreviewHeuristicContext default_aga{
      .gaia_id = GaiaId("default_aga"),
      .preview_data = raw_ref(default_aga_data),
      .is_external_app_primary = true,
  };

  AccountPreviewData candidate_cross_more_data = CreatePreviewData(
      {.passwords = switches::kPasswordsMedianThreshold.Get()},
      {CreateDevicePreview(
          "guid", base::Time::Now(),
          sync_pb::SyncEnums_DeviceFormFactor_DEVICE_FORM_FACTOR_DESKTOP)});
  AccountPreviewHeuristicContext candidate_cross_more{
      .gaia_id = GaiaId("candidate_cross_more"),
      .preview_data = raw_ref(candidate_cross_more_data),
  };
  // AGA default account is Priority 2, so it is selected over secondary
  // candidates.
  auto pref =
      ComputePreferredAccountForPromo({default_aga, candidate_cross_more});
  ASSERT_TRUE(pref.has_value());
  EXPECT_EQ(pref->gaia_id, GaiaId("default_aga"));

  AccountPreviewData candidate_single_more_data = CreatePreviewData(
      {.passwords = switches::kPasswordsMedianThreshold.Get()});
  AccountPreviewHeuristicContext candidate_single_more{
      .gaia_id = GaiaId("candidate_single_more"),
      .preview_data = raw_ref(candidate_single_more_data),
  };
  pref = ComputePreferredAccountForPromo({default_aga, candidate_single_more});
  ASSERT_TRUE(pref.has_value());
  EXPECT_EQ(pref->gaia_id, GaiaId("default_aga"));

  AccountPreviewData candidate_cross_equal_data = CreatePreviewData(
      {.passwords = switches::kPasswordsQ1Threshold.Get() / 2},
      {CreateDevicePreview(
          "guid", base::Time::Now(),
          sync_pb::SyncEnums_DeviceFormFactor_DEVICE_FORM_FACTOR_DESKTOP)});
  AccountPreviewHeuristicContext candidate_cross_equal{
      .gaia_id = GaiaId("candidate_cross_equal"),
      .preview_data = raw_ref(candidate_cross_equal_data),
  };
  pref = ComputePreferredAccountForPromo({default_aga, candidate_cross_equal});
  ASSERT_TRUE(pref.has_value());
  EXPECT_EQ(pref->gaia_id, GaiaId("default_aga"));
}

TEST_F(AccountPreviewHeuristicTest, CandidateCrossDeviceDefaultSingleDevice) {
  AccountPreviewData default_data = CreatePreviewData(
      {.passwords = switches::kPasswordsMedianThreshold.Get()});
  AccountPreviewHeuristicContext default_single{
      .gaia_id = GaiaId("default"),
      .preview_data = raw_ref(default_data),
  };

  AccountPreviewData candidate_cross_equal_data = CreatePreviewData(
      {.passwords = switches::kPasswordsMedianThreshold.Get()},
      {CreateDevicePreview(
          "guid", base::Time::Now(),
          sync_pb::SyncEnums_DeviceFormFactor_DEVICE_FORM_FACTOR_PHONE)});
  AccountPreviewHeuristicContext candidate_cross_equal{
      .gaia_id = GaiaId("candidate_equal"),
      .preview_data = raw_ref(candidate_cross_equal_data),
  };
  // Equal sync data -> Candidate wins.
  auto pref =
      ComputePreferredAccountForPromo({default_single, candidate_cross_equal});
  ASSERT_TRUE(pref.has_value());
  EXPECT_EQ(pref->gaia_id, GaiaId("candidate_equal"));

  AccountPreviewData candidate_cross_less_data = CreatePreviewData(
      {.passwords = switches::kPasswordsQ1Threshold.Get() / 2},
      {CreateDevicePreview(
          "guid", base::Time::Now(),
          sync_pb::SyncEnums_DeviceFormFactor_DEVICE_FORM_FACTOR_PHONE)});
  AccountPreviewHeuristicContext candidate_cross_less{
      .gaia_id = GaiaId("candidate_less"),
      .preview_data = raw_ref(candidate_cross_less_data),
  };
  // Less sync data -> Default remains preferred.
  pref =
      ComputePreferredAccountForPromo({default_single, candidate_cross_less});
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
      .preview_data = raw_ref(default_data),
  };

  AccountPreviewData candidate_cross_equal_data = CreatePreviewData(
      {.passwords = switches::kPasswordsQ1Threshold.Get() / 2},
      {CreateDevicePreview(
          "guid2", base::Time::Now(),
          sync_pb::SyncEnums_DeviceFormFactor_DEVICE_FORM_FACTOR_PHONE)});
  AccountPreviewHeuristicContext candidate_cross_equal{
      .gaia_id = GaiaId("candidate_equal"),
      .preview_data = raw_ref(candidate_cross_equal_data),
  };
  // Equal sync data -> Default remains preferred (requires strictly more).
  auto pref =
      ComputePreferredAccountForPromo({default_cross, candidate_cross_equal});
  ASSERT_TRUE(pref.has_value());
  EXPECT_EQ(pref->gaia_id, GaiaId("default"));

  AccountPreviewData candidate_cross_more_data = CreatePreviewData(
      {.passwords = switches::kPasswordsMedianThreshold.Get()},
      {CreateDevicePreview(
          "guid3", base::Time::Now(),
          sync_pb::SyncEnums_DeviceFormFactor_DEVICE_FORM_FACTOR_PHONE)});
  AccountPreviewHeuristicContext candidate_cross_more{
      .gaia_id = GaiaId("candidate_more"),
      .preview_data = raw_ref(candidate_cross_more_data),
  };
  // Strictly more sync data -> Candidate wins.
  pref = ComputePreferredAccountForPromo({default_cross, candidate_cross_more});
  ASSERT_TRUE(pref.has_value());
  EXPECT_EQ(pref->gaia_id, GaiaId("candidate_more"));
}

TEST_F(AccountPreviewHeuristicTest, CandidateSingleDeviceDefaultSingleDevice) {
  AccountPreviewData default_data = CreatePreviewData(
      {.passwords = switches::kPasswordsQ1Threshold.Get() / 2});
  AccountPreviewHeuristicContext default_single{
      .gaia_id = GaiaId("default"),
      .preview_data = raw_ref(default_data),
  };

  AccountPreviewData candidate_single_equal_data = CreatePreviewData(
      {.passwords = switches::kPasswordsQ1Threshold.Get() / 2});
  AccountPreviewHeuristicContext candidate_single_equal{
      .gaia_id = GaiaId("candidate_equal"),
      .preview_data = raw_ref(candidate_single_equal_data),
  };
  // Equal sync data -> Default remains preferred.
  auto pref =
      ComputePreferredAccountForPromo({default_single, candidate_single_equal});
  ASSERT_TRUE(pref.has_value());
  EXPECT_EQ(pref->gaia_id, GaiaId("default"));

  AccountPreviewData candidate_single_more_data = CreatePreviewData(
      {.passwords = switches::kPasswordsMedianThreshold.Get()});
  AccountPreviewHeuristicContext candidate_single_more{
      .gaia_id = GaiaId("candidate_more"),
      .preview_data = raw_ref(candidate_single_more_data),
  };
  // Strictly more sync data -> Candidate wins.
  pref =
      ComputePreferredAccountForPromo({default_single, candidate_single_more});
  ASSERT_TRUE(pref.has_value());
  EXPECT_EQ(pref->gaia_id, GaiaId("candidate_more"));
}

TEST_F(AccountPreviewHeuristicTest, CandidateAgaPrimary) {
  AccountPreviewData candidate_aga_data = CreatePreviewData(
      {.passwords = switches::kPasswordsQ1Threshold.Get() / 2});
  AccountPreviewHeuristicContext candidate_aga{
      .gaia_id = GaiaId("candidate_aga"),
      .preview_data = raw_ref(candidate_aga_data),
      .is_external_app_primary = true,
  };

  AccountPreviewData default_cross_more_data = CreatePreviewData(
      {.passwords = switches::kPasswordsMedianThreshold.Get()},
      {CreateDevicePreview(
          "guid", base::Time::Now(),
          sync_pb::SyncEnums_DeviceFormFactor_DEVICE_FORM_FACTOR_DESKTOP)});
  AccountPreviewHeuristicContext default_cross_more{
      .gaia_id = GaiaId("default_cross"),
      .preview_data = raw_ref(default_cross_more_data),
  };
  // AGA candidate (Priority 2) wins over non-managed default account.
  auto pref =
      ComputePreferredAccountForPromo({default_cross_more, candidate_aga});
  ASSERT_TRUE(pref.has_value());
  EXPECT_EQ(pref->gaia_id, GaiaId("candidate_aga"));

  AccountPreviewData default_single_more_data = CreatePreviewData(
      {.passwords = switches::kPasswordsMedianThreshold.Get()});
  AccountPreviewHeuristicContext default_single_more{
      .gaia_id = GaiaId("default_single"),
      .preview_data = raw_ref(default_single_more_data),
  };
  pref = ComputePreferredAccountForPromo({default_single_more, candidate_aga});
  ASSERT_TRUE(pref.has_value());
  EXPECT_EQ(pref->gaia_id, GaiaId("candidate_aga"));

  AccountPreviewData default_cross_equal_data = CreatePreviewData(
      {.passwords = switches::kPasswordsQ1Threshold.Get() / 2},
      {CreateDevicePreview(
          "guid", base::Time::Now(),
          sync_pb::SyncEnums_DeviceFormFactor_DEVICE_FORM_FACTOR_DESKTOP)});
  AccountPreviewHeuristicContext default_cross_equal{
      .gaia_id = GaiaId("default_cross_equal"),
      .preview_data = raw_ref(default_cross_equal_data),
  };
  pref = ComputePreferredAccountForPromo({default_cross_equal, candidate_aga});
  ASSERT_TRUE(pref.has_value());
  EXPECT_EQ(pref->gaia_id, GaiaId("candidate_aga"));

  // Managed default (Priority 1) beats AGA candidate (Priority 2).
  AccountPreviewData managed_default_data = CreatePreviewData(
      {.passwords = switches::kPasswordsQ1Threshold.Get() / 2});
  AccountPreviewHeuristicContext managed_default{
      .gaia_id = GaiaId("managed_default"),
      .preview_data = raw_ref(managed_default_data),
      .is_managed = true,
  };
  pref = ComputePreferredAccountForPromo({managed_default, candidate_aga});
  ASSERT_TRUE(pref.has_value());
  EXPECT_EQ(pref->gaia_id, GaiaId("managed_default"));

  // AGA candidate that is managed is ignored, so consumer default remains
  // selected.
  AccountPreviewHeuristicContext managed_aga{
      .gaia_id = GaiaId("managed_aga"),
      .preview_data = raw_ref(candidate_aga_data),
      .is_managed = true,
      .is_external_app_primary = true,
  };
  pref = ComputePreferredAccountForPromo({default_cross_more, managed_aga});
  ASSERT_TRUE(pref.has_value());
  EXPECT_EQ(pref->gaia_id, GaiaId("default_cross"));

  // AGA candidate that is child is ignored, so consumer default remains
  // selected.
  AccountPreviewHeuristicContext child_aga{
      .gaia_id = GaiaId("child_aga"),
      .preview_data = raw_ref(candidate_aga_data),
      .is_child = true,
      .is_external_app_primary = true,
  };
  pref = ComputePreferredAccountForPromo({default_cross_more, child_aga});
  ASSERT_TRUE(pref.has_value());
  EXPECT_EQ(pref->gaia_id, GaiaId("default_cross"));
}

TEST_F(AccountPreviewHeuristicTest,
       MultiAccountCandidateSelectionAndTieBreaking) {
  AccountPreviewData data1 = CreatePreviewData(
      {.passwords = switches::kPasswordsQ1Threshold.Get() / 2});
  AccountPreviewHeuristicContext acc1{
      .gaia_id = GaiaId("acc1"),
      .preview_data = raw_ref(data1),
  };
  AccountPreviewData data2 = CreatePreviewData(
      {.passwords = switches::kPasswordsQ1Threshold.Get() / 2});
  AccountPreviewHeuristicContext acc2{
      .gaia_id = GaiaId("acc2"),
      .preview_data = raw_ref(data2),
  };
  AccountPreviewData data3 = CreatePreviewData(
      {.passwords = switches::kPasswordsMedianThreshold.Get()},
      {CreateDevicePreview(
          "guid", base::Time::Now(),
          sync_pb::SyncEnums_DeviceFormFactor_DEVICE_FORM_FACTOR_TABLET)});
  AccountPreviewHeuristicContext acc3{
      .gaia_id = GaiaId("acc3"),
      .preview_data = raw_ref(data3),
  };

  // acc3 beats acc1 and acc2.
  auto pref = ComputePreferredAccountForPromo({acc1, acc2, acc3});
  ASSERT_TRUE(pref.has_value());
  EXPECT_EQ(pref->gaia_id, GaiaId("acc3"));
  EXPECT_EQ(pref->other_device_form_factor,
            sync_pb::SyncEnums_DeviceFormFactor_DEVICE_FORM_FACTOR_TABLET);

  // Tie between acc1 and acc2 preserves the earlier account (acc1).
  pref = ComputePreferredAccountForPromo({acc1, acc2});
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
      .preview_data = raw_ref(data_1q4),
  };

  AccountPreviewData data_2q3 = CreatePreviewData({
      .bookmarks = switches::kBookmarksMedianThreshold.Get(),
      .autofill = switches::kAutofillMedianThreshold.Get(),
  });
  AccountPreviewHeuristicContext acc_2q3{
      .gaia_id = GaiaId("acc_2q3"),
      .preview_data = raw_ref(data_2q3),
  };

  // acc_1q4 is preferred as it has higher data type score.
  auto pref = ComputePreferredAccountForPromo({acc_1q4, acc_2q3});
  ASSERT_TRUE(pref.has_value());
  EXPECT_EQ(pref->gaia_id, GaiaId("acc_1q4"));

  pref = ComputePreferredAccountForPromo({acc_2q3, acc_1q4});
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
      .preview_data = raw_ref(data_1q4),
  };

  AccountPreviewData data_2q3_1q1 = CreatePreviewData({
      .passwords = switches::kPasswordsQ1Threshold.Get() / 2,
      .bookmarks = switches::kBookmarksMedianThreshold.Get(),
      .autofill = switches::kAutofillMedianThreshold.Get(),
  });
  AccountPreviewHeuristicContext acc_2q3_1q1{
      .gaia_id = GaiaId("acc_2q3_1q1"),
      .preview_data = raw_ref(data_2q3_1q1),
  };

  // acc_2q3_1q1 is preferred as it has higher data type score.
  auto pref = ComputePreferredAccountForPromo({acc_1q4, acc_2q3_1q1});
  ASSERT_TRUE(pref.has_value());
  EXPECT_EQ(pref->gaia_id, GaiaId("acc_2q3_1q1"));

  pref = ComputePreferredAccountForPromo({acc_2q3_1q1, acc_1q4});
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
      .preview_data = raw_ref(data_1q4),
  };

  AccountPreviewData data_low_quartiles = CreatePreviewData({
      .passwords = switches::kPasswordsQ1Threshold.Get() / 2,
      .bookmarks = switches::kBookmarksQ1Threshold.Get() / 2,
      .autofill = switches::kAutofillQ1Threshold.Get() / 2,
      .wallet = switches::kAutofillWalletMetadataQ1Threshold.Get(),
  });
  AccountPreviewHeuristicContext acc_low_quartiles{
      .gaia_id = GaiaId("acc_low_quartiles"),
      .preview_data = raw_ref(data_low_quartiles),
  };

  // acc_1q4 is preferred as it has higher sync data score.
  auto pref = ComputePreferredAccountForPromo({acc_1q4, acc_low_quartiles});
  ASSERT_TRUE(pref.has_value());
  EXPECT_EQ(pref->gaia_id, GaiaId("acc_1q4"));

  pref = ComputePreferredAccountForPromo({acc_low_quartiles, acc_1q4});
  ASSERT_TRUE(pref.has_value());
  EXPECT_EQ(pref->gaia_id, GaiaId("acc_1q4"));
}

}  // namespace signin
