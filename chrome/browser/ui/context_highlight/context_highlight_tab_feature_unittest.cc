// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/context_highlight/context_highlight_tab_feature.h"

#include "base/token.h"
#include "cc/trees/tracked_element_rects.h"
#include "chrome/test/base/chrome_render_view_host_test_harness.h"
#include "components/page_content_annotations/core/tracked_element_feature.h"
#include "components/tabs/public/mock_tab_interface.h"
#include "content/public/browser/render_widget_host.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "ui/gfx/geometry/rect.h"

using ::page_content_annotations::TrackedElementFeature;

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

TEST_F(ContextHighlightTabFeatureTest, CachedRectsUpdated) {
  TestTabInterface tab;
  EXPECT_CALL(tab, GetContents())
      .WillRepeatedly(testing::Return(web_contents()));

  auto feature = std::make_unique<ContextHighlightTabFeature>(tab);

  // Initially no cached rects.
  EXPECT_EQ(feature->latest_rects(), cc::TrackedElementRects());

  // Test update with empty rects.
  feature->OnTrackedElementRectsChanged(cc::TrackedElementRects(), 1.0f);
  EXPECT_EQ(feature->latest_rects(), cc::TrackedElementRects());

  // Simulate rects change.
  cc::TrackedElementRects rects;
  base::Token id(1, 2);
  cc::TrackedElementRect data(id, gfx::Rect(10, 20, 100, 200));
  cc::TrackedElementFeature tracked_element_feature =
      static_cast<cc::TrackedElementFeature>(
          TrackedElementFeature::kAIHighlight);
  rects.insert({tracked_element_feature, {data}});
  float scale = 1.5f;

  feature->OnTrackedElementRectsChanged(rects, scale);

  // Verify cached values.
  EXPECT_EQ(feature->latest_rects(), rects);
  EXPECT_EQ(feature->latest_scale_factor(), scale);
}

TEST_F(ContextHighlightTabFeatureTest, RectsResetOnDiscard) {
  TestTabInterface tab;
  EXPECT_CALL(tab, GetContents())
      .WillRepeatedly(testing::Return(web_contents()));

  TabInterface::WillDiscardContentsCallback discard_callback;
  EXPECT_CALL(tab, RegisterWillDiscardContents(testing::_))
      .WillOnce(
          testing::DoAll(testing::SaveArg<0>(&discard_callback),
                         testing::Return(base::CallbackListSubscription())));

  auto feature = std::make_unique<ContextHighlightTabFeature>(tab);

  // Set some rects.
  cc::TrackedElementRects rects;
  base::Token id(1, 2);
  cc::TrackedElementRect data(id, gfx::Rect(10, 20, 100, 200));
  cc::TrackedElementFeature tracked_element_feature =
      static_cast<cc::TrackedElementFeature>(
          TrackedElementFeature::kAIHighlight);
  rects.insert({tracked_element_feature, {data}});
  feature->OnTrackedElementRectsChanged(rects, 1.0f);
  EXPECT_EQ(feature->latest_rects(), rects);

  // Simulate discard.
  discard_callback.Run(&tab, web_contents(), nullptr);

  // Rects should be reset.
  EXPECT_EQ(feature->latest_rects(), cc::TrackedElementRects());
}

}  // namespace tabs
