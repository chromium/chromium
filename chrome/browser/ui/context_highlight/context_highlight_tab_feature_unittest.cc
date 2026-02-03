// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/context_highlight/context_highlight_tab_feature.h"

#include "base/token.h"
#include "cc/trees/tracked_element_bounds.h"
#include "chrome/test/base/chrome_render_view_host_test_harness.h"
#include "components/tabs/public/mock_tab_interface.h"
#include "content/public/browser/render_widget_host.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "ui/gfx/geometry/rect.h"

namespace tabs {

namespace {

class TestTabInterface : public MockTabInterface {
 public:
  TestTabInterface() = default;
  ~TestTabInterface() override = default;

  ui::UnownedUserDataHost& GetUnownedUserDataHost() override {
    return user_data_host_;
  }
  const ui::UnownedUserDataHost& GetUnownedUserDataHost() const override {
    return user_data_host_;
  }

 private:
  ui::UnownedUserDataHost user_data_host_;
};

}  // namespace

class ContextHighlightTabFeatureTest : public ChromeRenderViewHostTestHarness {
 public:
  ContextHighlightTabFeatureTest() = default;
};

TEST_F(ContextHighlightTabFeatureTest, CachedBoundsUpdated) {
  TestTabInterface tab;
  EXPECT_CALL(tab, GetContents())
      .WillRepeatedly(testing::Return(web_contents()));

  auto feature = std::make_unique<ContextHighlightTabFeature>(tab);

  // Initially no cached bounds.
  EXPECT_EQ(feature->latest_bounds(), cc::TrackedElementBounds());

  // Simulate bounds change.
  cc::TrackedElementBounds bounds;
  base::Token id(1, 2);
  bounds[id] = {gfx::Rect(10, 20, 100, 200)};
  float scale = 1.5f;

  feature->OnTrackedElementBoundsChanged(bounds, scale);

  // Verify cached values.
  EXPECT_EQ(feature->latest_bounds(), bounds);
  EXPECT_EQ(feature->latest_scale_factor(), scale);
}

TEST_F(ContextHighlightTabFeatureTest, BoundsResetOnDiscard) {
  TestTabInterface tab;
  EXPECT_CALL(tab, GetContents())
      .WillRepeatedly(testing::Return(web_contents()));

  TabInterface::WillDiscardContentsCallback discard_callback;
  EXPECT_CALL(tab, RegisterWillDiscardContents(testing::_))
      .WillOnce(
          testing::DoAll(testing::SaveArg<0>(&discard_callback),
                         testing::Return(base::CallbackListSubscription())));

  auto feature = std::make_unique<ContextHighlightTabFeature>(tab);

  // Set some bounds.
  cc::TrackedElementBounds bounds;
  base::Token id(1, 2);
  bounds[id] = {gfx::Rect(10, 20, 100, 200)};
  feature->OnTrackedElementBoundsChanged(bounds, 1.0f);
  EXPECT_EQ(feature->latest_bounds(), bounds);

  // Simulate discard.
  discard_callback.Run(&tab, web_contents(), nullptr);

  // Bounds should be reset.
  EXPECT_EQ(feature->latest_bounds(), cc::TrackedElementBounds());
}

}  // namespace tabs
