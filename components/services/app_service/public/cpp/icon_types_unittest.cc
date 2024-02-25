// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/services/app_service/public/cpp/icon_types.h"

#include "components/services/app_service/public/cpp/icon_effects.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace apps {

using IconTypesTest = testing::Test;

TEST_F(IconTypesTest, VerifyMergeBothAreNull) {
  auto icon_key = MergeIconKey(nullptr, nullptr);
  EXPECT_FALSE(icon_key.has_value());
}

TEST_F(IconTypesTest, VerifyMergeDeltaIsNull) {
  {
    IconKey state(/*resource_id=*/65535, IconEffects::kCrOsStandardIcon);
    state.update_version = IconKey::kInvalidResourceId;

    auto icon_key = MergeIconKey(&state, nullptr);
    ASSERT_TRUE(icon_key.has_value());
    EXPECT_EQ(state, icon_key.value());
  }
  {
    IconKey state(IconKey::kInvalidResourceId, IconEffects::kBlocked);
    state.update_version = IconKey::kInitVersion;

    auto icon_key = MergeIconKey(&state, nullptr);
    ASSERT_TRUE(icon_key.has_value());
    EXPECT_EQ(state, icon_key.value());
  }
}

TEST_F(IconTypesTest, VerifyMergeStateIsNull) {
  {
    IconKey delta(/*resource_id=*/65535, IconEffects::kChromeBadge);
    delta.update_version = false;

    auto icon_key = MergeIconKey(nullptr, &delta);
    IconKey state(delta.resource_id, delta.icon_effects);
    state.update_version = IconKey::kInvalidVersion;
    ASSERT_TRUE(icon_key.has_value());
    EXPECT_EQ(state, icon_key.value());
  }
  {
    IconKey delta(IconKey::kInvalidResourceId,
                  IconEffects::kCrOsStandardIcon | IconEffects::kPaused);
    delta.update_version = false;

    auto icon_key = MergeIconKey(nullptr, &delta);
    IconKey state(delta.resource_id, delta.icon_effects);
    state.update_version = IconKey::kInitVersion;
    ASSERT_TRUE(icon_key.has_value());
    EXPECT_EQ(state, icon_key.value());
  }
  {
    IconKey delta(IconKey::kInvalidResourceId,
                  IconEffects::kCrOsStandardIcon | IconEffects::kBlocked);
    delta.update_version = true;

    auto icon_key = MergeIconKey(nullptr, &delta);
    IconKey state(delta.resource_id, delta.icon_effects);
    state.update_version = IconKey::kInitVersion;
    ASSERT_TRUE(icon_key.has_value());
    EXPECT_EQ(state, icon_key.value());
  }
}

TEST_F(IconTypesTest, VerifyMergeBothAreNotNull) {
  {
    IconKey state(/*resource_id=*/65535, IconEffects::kNone);
    state.update_version = IconKey::kInvalidVersion;
    IconKey delta(/*resource_id=*/65535, IconEffects::kChromeBadge);
    delta.update_version = false;

    auto icon_key = MergeIconKey(&state, &delta);
    state.icon_effects = delta.icon_effects;
    ASSERT_TRUE(icon_key.has_value());
    EXPECT_EQ(state, icon_key.value());
  }
  {
    IconKey state(IconKey::kInvalidResourceId, IconEffects::kNone);
    state.update_version = IconKey::kInitVersion;
    IconKey delta(IconKey::kInvalidResourceId,
                  IconEffects::kCrOsStandardIcon | IconEffects::kPaused);
    delta.update_version = false;

    auto icon_key = MergeIconKey(&state, &delta);
    state.icon_effects = delta.icon_effects;
    ASSERT_TRUE(icon_key.has_value());
    EXPECT_EQ(state, icon_key.value());
  }
  {
    IconKey state(IconKey::kInvalidResourceId, IconEffects::kChromeBadge);
    state.update_version = 100;
    IconKey delta(IconKey::kInvalidResourceId,
                  IconEffects::kChromeBadge | IconEffects::kPaused);
    delta.update_version = true;

    auto icon_key = MergeIconKey(&state, &delta);
    state.icon_effects = delta.icon_effects;
    ++absl::get<int32_t>(state.update_version);
    ASSERT_TRUE(icon_key.has_value());
    EXPECT_EQ(state, icon_key.value());
  }
}

}  // namespace apps
