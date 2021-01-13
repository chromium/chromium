// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/previews/content/previews_user_data.h"

#include <stdint.h>

#include "base/time/time.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace previews {

TEST(PreviewsUserDataTest, TestConstructor) {
  uint64_t id = 5u;
  std::unique_ptr<PreviewsUserData> data(new PreviewsUserData(5u));
  EXPECT_EQ(id, data->page_id());
}

TEST(PreviewsUserDataTest, TestSetEligibilityReason) {
  PreviewsUserData data(1u);
  EXPECT_EQ(base::nullopt,
            data.EligibilityReasonForPreview(PreviewsType::DEFER_ALL_SCRIPT));

  data.SetEligibilityReasonForPreview(
      PreviewsType::DEFER_ALL_SCRIPT,
      PreviewsEligibilityReason::BLOCKLIST_UNAVAILABLE);
  data.SetEligibilityReasonForPreview(
      PreviewsType::DEFER_ALL_SCRIPT,
      PreviewsEligibilityReason::BLOCKLIST_DATA_NOT_LOADED);

  EXPECT_EQ(PreviewsEligibilityReason::BLOCKLIST_DATA_NOT_LOADED,
            data.EligibilityReasonForPreview(PreviewsType::DEFER_ALL_SCRIPT));
}

TEST(PreviewsUserDataTest, TestSetEligibilityReasonNull) {
  PreviewsUserData data(1u);
  EXPECT_EQ(base::nullopt,
            data.EligibilityReasonForPreview(PreviewsType::DEFER_ALL_SCRIPT));
}

TEST(PreviewsUserDataTest, DeepCopy) {
  uint64_t id = 4u;
  std::unique_ptr<PreviewsUserData> data =
      std::make_unique<PreviewsUserData>(id);
  EXPECT_EQ(id, data->page_id());

  EXPECT_EQ(0, data->data_savings_inflation_percent());
  EXPECT_FALSE(data->cache_control_no_transform_directive());
  EXPECT_EQ(previews::PreviewsType::NONE, data->CommittedPreviewsType());
  EXPECT_FALSE(data->block_listed_for_lite_page());

  data->set_data_savings_inflation_percent(123);
  data->set_cache_control_no_transform_directive();
  data->SetCommittedPreviewsType(previews::PreviewsType::DEFER_ALL_SCRIPT);
  data->set_block_listed_for_lite_page(true);

  PreviewsUserData data_copy(*data);
  EXPECT_EQ(id, data_copy.page_id());
  EXPECT_EQ(data->CoinFlipForNavigation(), data_copy.CoinFlipForNavigation());
  EXPECT_EQ(123, data_copy.data_savings_inflation_percent());
  EXPECT_TRUE(data_copy.cache_control_no_transform_directive());
  EXPECT_EQ(previews::PreviewsType::DEFER_ALL_SCRIPT,
            data_copy.CommittedPreviewsType());
  EXPECT_TRUE(data_copy.block_listed_for_lite_page());
}

TEST(PreviewsUserDataTest, TestCoinFlip_HasCommittedPreviewsType) {
  uint64_t id = 1u;
  std::unique_ptr<PreviewsUserData> data =
      std::make_unique<PreviewsUserData>(id);

  data->SetCommittedPreviewsTypeForTesting(PreviewsType::DEFER_ALL_SCRIPT);
  data->set_coin_flip_holdback_result(CoinFlipHoldbackResult::kHoldback);
  EXPECT_FALSE(data->HasCommittedPreviewsType());

  data->set_coin_flip_holdback_result(CoinFlipHoldbackResult::kAllowed);
  EXPECT_TRUE(data->HasCommittedPreviewsType());
}

TEST(PreviewsUserDataTest, TestCoinFlip_CommittedPreviewsType) {
  uint64_t id = 1u;
  std::unique_ptr<PreviewsUserData> data =
      std::make_unique<PreviewsUserData>(id);

  data->SetCommittedPreviewsTypeForTesting(PreviewsType::DEFER_ALL_SCRIPT);
  data->set_coin_flip_holdback_result(CoinFlipHoldbackResult::kHoldback);
  EXPECT_EQ(data->CommittedPreviewsType(), PreviewsType::NONE);
  EXPECT_EQ(data->PreHoldbackCommittedPreviewsType(),
            PreviewsType::DEFER_ALL_SCRIPT);

  data->set_coin_flip_holdback_result(CoinFlipHoldbackResult::kAllowed);
  EXPECT_EQ(data->CommittedPreviewsType(), PreviewsType::DEFER_ALL_SCRIPT);
  EXPECT_EQ(data->PreHoldbackCommittedPreviewsType(),
            PreviewsType::DEFER_ALL_SCRIPT);
}

TEST(PreviewsUserDataTest, TestCoinFlip_AllowedPreviewsState) {
  uint64_t id = 1u;
  std::unique_ptr<PreviewsUserData> data =
      std::make_unique<PreviewsUserData>(id);

  data->set_allowed_previews_state(blink::PreviewsTypes::DEFER_ALL_SCRIPT_ON);
  data->set_coin_flip_holdback_result(CoinFlipHoldbackResult::kHoldback);
  EXPECT_EQ(data->AllowedPreviewsState(), blink::PreviewsTypes::PREVIEWS_OFF);
  EXPECT_EQ(data->PreHoldbackAllowedPreviewsState(),
            blink::PreviewsTypes::DEFER_ALL_SCRIPT_ON);

  data->set_coin_flip_holdback_result(CoinFlipHoldbackResult::kAllowed);
  EXPECT_EQ(data->AllowedPreviewsState(),
            blink::PreviewsTypes::DEFER_ALL_SCRIPT_ON);
  EXPECT_EQ(data->PreHoldbackAllowedPreviewsState(),
            blink::PreviewsTypes::DEFER_ALL_SCRIPT_ON);
}

TEST(PreviewsUserDataTest, TestCoinFlip_CommittedPreviewsState) {
  uint64_t id = 1u;
  std::unique_ptr<PreviewsUserData> data =
      std::make_unique<PreviewsUserData>(id);

  data->set_committed_previews_state(blink::PreviewsTypes::DEFER_ALL_SCRIPT_ON);
  data->set_coin_flip_holdback_result(CoinFlipHoldbackResult::kHoldback);
  EXPECT_EQ(data->CommittedPreviewsState(), blink::PreviewsTypes::PREVIEWS_OFF);
  EXPECT_EQ(data->PreHoldbackCommittedPreviewsState(),
            blink::PreviewsTypes::DEFER_ALL_SCRIPT_ON);

  data->set_coin_flip_holdback_result(CoinFlipHoldbackResult::kAllowed);
  EXPECT_EQ(data->CommittedPreviewsState(),
            blink::PreviewsTypes::DEFER_ALL_SCRIPT_ON);
  EXPECT_EQ(data->PreHoldbackCommittedPreviewsState(),
            blink::PreviewsTypes::DEFER_ALL_SCRIPT_ON);
}

}  // namespace previews
