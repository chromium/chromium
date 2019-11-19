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
            data.EligibilityReasonForPreview(PreviewsType::OFFLINE));

  data.SetEligibilityReasonForPreview(
      PreviewsType::NOSCRIPT, PreviewsEligibilityReason::BLACKLIST_UNAVAILABLE);
  data.SetEligibilityReasonForPreview(
      PreviewsType::NOSCRIPT,
      PreviewsEligibilityReason::BLACKLIST_DATA_NOT_LOADED);

  EXPECT_EQ(base::nullopt,
            data.EligibilityReasonForPreview(PreviewsType::OFFLINE));
  EXPECT_EQ(PreviewsEligibilityReason::BLACKLIST_DATA_NOT_LOADED,
            data.EligibilityReasonForPreview(PreviewsType::NOSCRIPT));
}

TEST(PreviewsUserDataTest, DeepCopy) {
  uint64_t id = 4u;
  std::unique_ptr<PreviewsUserData> data =
      std::make_unique<PreviewsUserData>(id);
  EXPECT_EQ(id, data->page_id());

  EXPECT_EQ(0, data->data_savings_inflation_percent());
  EXPECT_FALSE(data->cache_control_no_transform_directive());
  EXPECT_EQ(previews::PreviewsType::NONE, data->CommittedPreviewsType());
  EXPECT_FALSE(data->black_listed_for_lite_page());
  EXPECT_FALSE(data->offline_preview_used());
  EXPECT_EQ(data->server_lite_page_info(), nullptr);
  EXPECT_EQ(base::nullopt, data->serialized_hint_version_string());

  base::TimeTicks now = base::TimeTicks::Now();

  data->set_data_savings_inflation_percent(123);
  data->set_cache_control_no_transform_directive();
  data->SetCommittedPreviewsType(previews::PreviewsType::NOSCRIPT);
  data->set_offline_preview_used(true);
  data->set_black_listed_for_lite_page(true);
  data->set_server_lite_page_info(
      std::make_unique<PreviewsUserData::ServerLitePageInfo>());
  data->server_lite_page_info()->original_navigation_start = now;
  data->set_serialized_hint_version_string("someversion");

  PreviewsUserData data_copy(*data);
  EXPECT_EQ(id, data_copy.page_id());
  EXPECT_EQ(data->CoinFlipForNavigation(), data_copy.CoinFlipForNavigation());
  EXPECT_EQ(123, data_copy.data_savings_inflation_percent());
  EXPECT_TRUE(data_copy.cache_control_no_transform_directive());
  EXPECT_EQ(previews::PreviewsType::NOSCRIPT,
            data_copy.CommittedPreviewsType());
  EXPECT_TRUE(data_copy.black_listed_for_lite_page());
  EXPECT_TRUE(data_copy.offline_preview_used());
  EXPECT_NE(data->server_lite_page_info(), nullptr);
  EXPECT_EQ(data->server_lite_page_info()->original_navigation_start, now);
  EXPECT_EQ("someversion", data->serialized_hint_version_string());
}

TEST(PreviewsUserDataTest, TestCoinFlip_HasCommittedPreviewsType) {
  uint64_t id = 1u;
  std::unique_ptr<PreviewsUserData> data =
      std::make_unique<PreviewsUserData>(id);

  data->SetCommittedPreviewsTypeForTesting(PreviewsType::NOSCRIPT);
  data->set_coin_flip_holdback_result(CoinFlipHoldbackResult::kHoldback);
  EXPECT_FALSE(data->HasCommittedPreviewsType());

  data->set_coin_flip_holdback_result(CoinFlipHoldbackResult::kAllowed);
  EXPECT_TRUE(data->HasCommittedPreviewsType());
}

TEST(PreviewsUserDataTest, TestCoinFlip_CommittedPreviewsType) {
  uint64_t id = 1u;
  std::unique_ptr<PreviewsUserData> data =
      std::make_unique<PreviewsUserData>(id);

  data->SetCommittedPreviewsTypeForTesting(PreviewsType::NOSCRIPT);
  data->set_coin_flip_holdback_result(CoinFlipHoldbackResult::kHoldback);
  EXPECT_EQ(data->CommittedPreviewsType(), PreviewsType::NONE);
  EXPECT_EQ(data->PreHoldbackCommittedPreviewsType(), PreviewsType::NOSCRIPT);

  data->set_coin_flip_holdback_result(CoinFlipHoldbackResult::kAllowed);
  EXPECT_EQ(data->CommittedPreviewsType(), PreviewsType::NOSCRIPT);
  EXPECT_EQ(data->PreHoldbackCommittedPreviewsType(), PreviewsType::NOSCRIPT);
}

TEST(PreviewsUserDataTest, TestCoinFlip_AllowedPreviewsState) {
  uint64_t id = 1u;
  std::unique_ptr<PreviewsUserData> data =
      std::make_unique<PreviewsUserData>(id);

  data->set_allowed_previews_state(content::NOSCRIPT_ON);
  data->set_coin_flip_holdback_result(CoinFlipHoldbackResult::kHoldback);
  EXPECT_EQ(data->AllowedPreviewsState(), content::PREVIEWS_OFF);
  EXPECT_EQ(data->PreHoldbackAllowedPreviewsState(), content::NOSCRIPT_ON);

  data->set_coin_flip_holdback_result(CoinFlipHoldbackResult::kAllowed);
  EXPECT_EQ(data->AllowedPreviewsState(), content::NOSCRIPT_ON);
  EXPECT_EQ(data->PreHoldbackAllowedPreviewsState(), content::NOSCRIPT_ON);
}

TEST(PreviewsUserDataTest, TestCoinFlip_CommittedPreviewsState) {
  uint64_t id = 1u;
  std::unique_ptr<PreviewsUserData> data =
      std::make_unique<PreviewsUserData>(id);

  data->set_committed_previews_state(content::NOSCRIPT_ON);
  data->set_coin_flip_holdback_result(CoinFlipHoldbackResult::kHoldback);
  EXPECT_EQ(data->CommittedPreviewsState(), content::PREVIEWS_OFF);
  EXPECT_EQ(data->PreHoldbackCommittedPreviewsState(), content::NOSCRIPT_ON);

  data->set_coin_flip_holdback_result(CoinFlipHoldbackResult::kAllowed);
  EXPECT_EQ(data->CommittedPreviewsState(), content::NOSCRIPT_ON);
  EXPECT_EQ(data->PreHoldbackCommittedPreviewsState(), content::NOSCRIPT_ON);
}

}  // namespace previews
