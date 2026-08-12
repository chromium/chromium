// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/signin/core/browser/account_preview_heuristic.h"

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

AccountPreviewData CreatePreviewData(size_t passwords = 0,
                                     size_t bookmarks = 0,
                                     size_t autofill = 0,
                                     size_t wallet = 0,
                                     std::vector<DevicePreview> devices = {}) {
  AccountPreviewData data;
  if (passwords > 0) {
    data.counts[syncer::PASSWORDS] = passwords;
  }
  if (bookmarks > 0) {
    data.counts[syncer::BOOKMARKS] = bookmarks;
  }
  if (autofill > 0) {
    data.counts[syncer::AUTOFILL] = autofill;
  }
  if (wallet > 0) {
    data.counts[syncer::AUTOFILL_WALLET_METADATA] = wallet;
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

  AccountPreviewData data = CreatePreviewData(/*passwords=*/60);
  EXPECT_EQ(ComputeAccountPreviewPreference(GaiaId("user1"), data),
            std::nullopt);
}

TEST_F(AccountPreviewHeuristicTest,
       ComputeAccountPreviewPreferencePreferredDataTypesRankingAndQuartile) {
  // Passwords: 40 (median=20, ratio=2.0, quartile=kMedianToQ3)
  // Bookmarks: 50 (median=50, ratio=1.0, quartile=kMedianToQ3)
  // Autofill: 2 (median=15, ratio=0.13, quartile=kBelowQ1)
  // Wallet: 0 -> Excluded
  AccountPreviewData data =
      CreatePreviewData(/*passwords=*/40, /*bookmarks=*/50,
                        /*autofill=*/2, /*wallet=*/0);

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
  // Passwords: 2 (ratio=0.1), Autofill: 4 (ratio=0.267)
  AccountPreviewData data = CreatePreviewData(/*passwords=*/2, /*bookmarks=*/0,
                                              /*autofill=*/4, /*wallet=*/0);

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
       ComputeAccountPreviewPreference_FormFactorExtraction) {
  AccountPreviewData no_devices;
  auto pref_no_devices =
      ComputeAccountPreviewPreference(GaiaId("user1"), no_devices);
  ASSERT_TRUE(pref_no_devices.has_value());
  EXPECT_EQ(pref_no_devices->other_device_form_factor,
            sync_pb::SyncEnums_DeviceFormFactor_DEVICE_FORM_FACTOR_UNSPECIFIED);

  base::Time now = base::Time::Now();
  AccountPreviewData data_with_devices = CreatePreviewData(
      0, 0, 0, 0,
      {CreateDevicePreview(
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
  // kMedianToQ3): Passwords: 20 (median=20, ratio=1.0) Bookmarks: 50
  // (median=50, ratio=1.0) Autofill: 15 (median=15, ratio=1.0) Wallet: 3
  // (median=3, ratio=1.0) Tie-breaking should preserve the priority declaration
  // order (PASSWORDS -> BOOKMARKS -> AUTOFILL -> AUTOFILL_WALLET_METADATA).
  AccountPreviewData data = CreatePreviewData(
      /*passwords=*/20, /*bookmarks=*/50, /*autofill=*/15, /*wallet=*/3);

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

}  // namespace signin
