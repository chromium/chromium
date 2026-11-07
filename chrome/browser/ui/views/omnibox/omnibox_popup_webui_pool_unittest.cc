// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/views/omnibox/omnibox_popup_webui_pool.h"

#include "base/memory_coordinator/memory_limit.h"
#include "base/memory_coordinator/test_memory_consumer_registry.h"
#include "base/test/scoped_feature_list.h"
#include "components/omnibox/common/omnibox_features.h"
#include "testing/gtest/include/gtest/gtest.h"

class OmniboxPopupWebUIPoolTest : public testing::Test {
 protected:
  base::TestMemoryConsumerRegistry memory_consumer_registry_;
  base::test::ScopedFeatureList feature_list_;
};

// The pool listens for memory pressure only with the param.
TEST_F(OmniboxPopupWebUIPoolTest, RegistersForMemoryPressureWithParam) {
  feature_list_.InitAndEnableFeatureWithParameters(
      omnibox::kOmniboxWebUIPopupShareAcrossWindows,
      {{omnibox::kOmniboxWebUIPopupShareDropIdleOnMemoryPressure.name,
        "true"}});
  OmniboxPopupWebUIPool pool;
  EXPECT_EQ(memory_consumer_registry_.size(), 1u);
}

TEST_F(OmniboxPopupWebUIPoolTest, DoesNotRegisterWithoutParam) {
  feature_list_.InitAndEnableFeatureWithParameters(
      omnibox::kOmniboxWebUIPopupShareAcrossWindows,
      {{omnibox::kOmniboxWebUIPopupShareDropIdleOnMemoryPressure.name,
        "false"}});
  OmniboxPopupWebUIPool pool;
  EXPECT_EQ(memory_consumer_registry_.size(), 0u);
}

// By default only critical memory pressure drops the idle popups.
TEST_F(OmniboxPopupWebUIPoolTest, DropsIdlePopupsUnderCriticalPressure) {
  EXPECT_FALSE(OmniboxPopupWebUIPool::ShouldDropIdleWebUIContents(
      base::MemoryLimit::NoPressureThreshold()));
  EXPECT_FALSE(OmniboxPopupWebUIPool::ShouldDropIdleWebUIContents(
      base::MemoryLimit::ModeratePressureThreshold()));
  EXPECT_TRUE(OmniboxPopupWebUIPool::ShouldDropIdleWebUIContents(
      base::MemoryLimit::CriticalPressureThreshold()));
}

TEST_F(OmniboxPopupWebUIPoolTest,
       DropsIdlePopupsUnderModeratePressureWithParam) {
  feature_list_.InitAndEnableFeatureWithParameters(
      omnibox::kOmniboxWebUIPopupShareAcrossWindows,
      {{omnibox::kOmniboxWebUIPopupShareDropIdleOnModeratePressure.name,
        "true"}});
  EXPECT_FALSE(OmniboxPopupWebUIPool::ShouldDropIdleWebUIContents(
      base::MemoryLimit::NoPressureThreshold()));
  EXPECT_TRUE(OmniboxPopupWebUIPool::ShouldDropIdleWebUIContents(
      base::MemoryLimit::ModeratePressureThreshold()));
}
