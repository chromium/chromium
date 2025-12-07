// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/views/page_action/page_action_model.h"

#include "base/types/pass_key.h"
#include "chrome/browser/ui/views/page_action/page_action_controller.h"
#include "chrome/browser/ui/views/page_action/page_action_model_observer.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "ui/actions/actions.h"
#include "ui/gfx/image/image_skia.h"
#include "ui/gfx/image/image_unittest_util.h"

namespace page_actions {
namespace {

using ::actions::ActionItem;

const std::u16string kOverrideText = u"Override";
const std::u16string kTestText = u"Test";
const std::u16string kTooltipText = u"Tooltip";
const std::u16string kDefaultText = u"DefaultName";
const std::u16string kOverrideName = u"OverrideName";
const ui::ImageModel kTestImage =
    ui::ImageModel::FromImageSkia(gfx::test::CreateImageSkia(/*size=*/16));

base::PassKey<PageActionController> PassKey() {
  return PageActionController::PassKeyForTesting();
}

class MockPageActionModelObserver : public PageActionModelObserver {
 public:
  MOCK_METHOD(void,
              OnPageActionModelChanged,
              (const PageActionModelInterface& model),
              (override));
  MOCK_METHOD(void,
              OnPageActionModelWillBeDeleted,
              (const PageActionModelInterface& model),
              (override));
};

class PageActionModelTest : public ::testing::Test {
 protected:
  void SetUp() override { model_.AddObserver(&observer_); }

  void TearDown() override { model_.RemoveObserver(&observer_); }

  PageActionModel model_;
  testing::NiceMock<MockPageActionModelObserver> observer_;
};

TEST_F(PageActionModelTest, DefaultVisibility) {
  EXPECT_FALSE(model_.GetVisible());
}

TEST_F(PageActionModelTest, VisibilityConditions) {
  EXPECT_CALL(observer_, OnPageActionModelChanged).Times(4);

  model_.SetShowRequested(PassKey(), true);
  EXPECT_FALSE(model_.GetVisible());

  model_.SetActionItemProperties(
      PassKey(),
      ActionItem::Builder().SetEnabled(true).SetVisible(true).Build().get());
  EXPECT_FALSE(model_.GetVisible());

  model_.SetTabActive(PassKey(), true);
  EXPECT_TRUE(model_.GetVisible());

  model_.SetHasPinnedIcon(PassKey(), true);
  EXPECT_FALSE(model_.GetVisible());
}

TEST_F(PageActionModelTest, ShouldChipBeVisible) {
  EXPECT_CALL(observer_, OnPageActionModelChanged).Times(2);

  model_.SetShouldShowSuggestionChip(PassKey(), true);
  EXPECT_EQ(model_.ShouldShowSuggestionChip(), true);

  model_.SetShouldShowSuggestionChip(PassKey(), false);
  EXPECT_EQ(model_.ShouldShowSuggestionChip(), false);
}

TEST_F(PageActionModelTest, ChipVisibility) {
  EXPECT_CALL(observer_, OnPageActionModelChanged).Times(2);

  model_.SetIsChipShowing(PassKey(), true);
  EXPECT_EQ(model_.IsChipShowing(), true);

  model_.SetIsChipShowing(PassKey(), false);
  EXPECT_EQ(model_.IsChipShowing(), false);
}

TEST_F(PageActionModelTest, ShouldAnnounceChip) {
  EXPECT_CALL(observer_, OnPageActionModelChanged).Times(1);
  model_.SetSuggestionChipConfig(PassKey(), {.should_announce_chip = true});
  EXPECT_EQ(model_.GetShouldAnnounceChip(), true);

  EXPECT_CALL(observer_, OnPageActionModelChanged).Times(1);
  model_.SetSuggestionChipConfig(PassKey(), {.should_announce_chip = false});
  EXPECT_EQ(model_.GetShouldAnnounceChip(), false);
}

TEST_F(PageActionModelTest, ShouldAnimateChip) {
  model_.SetSuggestionChipConfig(PassKey(), {.should_animate = true});
  EXPECT_EQ(model_.GetShouldAnimateChipOut(), true);
  EXPECT_EQ(model_.GetShouldAnimateChipIn(), true);

  EXPECT_CALL(observer_, OnPageActionModelChanged).Times(1);
  model_.SetSuggestionChipConfig(PassKey(), {.should_animate = false});
  EXPECT_EQ(model_.GetShouldAnimateChipOut(), false);
  EXPECT_EQ(model_.GetShouldAnimateChipIn(), false);
}

TEST_F(PageActionModelTest, ShouldAnimateIn) {
  model_.SetSuggestionChipConfig(PassKey(), {.should_animate = true});
  model_.SetShouldShowSuggestionChip(PassKey(), true);
  EXPECT_EQ(model_.GetShouldAnimateChipIn(), true);

  // Mark the chip as shown.
  model_.SetIsChipShowing(PassKey(), true);
  EXPECT_EQ(model_.GetShouldAnimateChipIn(), false);

  // Hiding the chip should not reset the animation state.
  model_.SetIsChipShowing(PassKey(), false);
  EXPECT_EQ(model_.GetShouldAnimateChipIn(), false);

  // Requesting to show the chip again should the animation state.
  model_.SetShouldShowSuggestionChip(PassKey(), true);
  EXPECT_EQ(model_.GetShouldAnimateChipIn(), true);
}

TEST_F(PageActionModelTest, OverrideText) {
  EXPECT_CALL(observer_, OnPageActionModelChanged).Times(2);

  model_.SetOverrideText(PassKey(), kOverrideText);
  EXPECT_EQ(model_.GetText(), kOverrideText);

  model_.SetOverrideText(PassKey(), std::nullopt);
  EXPECT_EQ(model_.GetText(), std::u16string());
}

TEST_F(PageActionModelTest, OverrideImage) {
  model_.SetActionItemProperties(
      PassKey(), ActionItem::Builder().SetImage(kTestImage).Build().get());
  EXPECT_EQ(model_.GetImage(), kTestImage);

  ui::ImageModel kOverrideImage =
      ui::ImageModel::FromImageSkia(gfx::test::CreateImageSkia(/*size=*/32));

  EXPECT_CALL(observer_, OnPageActionModelChanged).Times(1);
  model_.SetOverrideImage(PassKey(), kOverrideImage,
                          PageActionColorSource::kForeground);
  EXPECT_EQ(model_.GetImage(), kOverrideImage);

  EXPECT_CALL(observer_, OnPageActionModelChanged).Times(1);
  model_.SetOverrideImage(PassKey(), std::nullopt,
                          PageActionColorSource::kForeground);
  EXPECT_EQ(model_.GetImage(), kTestImage);
}

TEST_F(PageActionModelTest, OverrideTooltip) {
  auto action_item = ActionItem::Builder().SetTooltipText(kTooltipText).Build();
  EXPECT_CALL(observer_, OnPageActionModelChanged).Times(1);
  model_.SetActionItemProperties(PassKey(), action_item.get());
  EXPECT_EQ(model_.GetTooltipText(), kTooltipText);

  EXPECT_CALL(observer_, OnPageActionModelChanged).Times(1);
  model_.SetOverrideTooltip(PassKey(), kOverrideText);
  EXPECT_EQ(model_.GetTooltipText(), kOverrideText);

  EXPECT_CALL(observer_, OnPageActionModelChanged).Times(1);
  model_.SetOverrideTooltip(PassKey(), std::nullopt);
  EXPECT_EQ(model_.GetTooltipText(), kTooltipText);
}

TEST_F(PageActionModelTest, SetActionItemProperties) {
  // NOTE: The visibility is exercised by the test VisibilityConditions.
  EXPECT_CALL(observer_, OnPageActionModelChanged).Times(1);

  model_.SetActionItemProperties(PassKey(), ActionItem::Builder()
                                                .SetText(kTestText)
                                                .SetTooltipText(kTooltipText)
                                                .SetImage(kTestImage)
                                                .SetIsShowingBubble(true)
                                                .Build()
                                                .get());

  EXPECT_EQ(model_.GetText(), kTestText);
  EXPECT_EQ(model_.GetImage(), kTestImage);
  EXPECT_EQ(model_.GetTooltipText(), kTooltipText);
  EXPECT_EQ(model_.GetActionItemIsShowingBubble(), true);
}

TEST_F(PageActionModelTest, ShouldGetSuppressedByOmnibox) {
  model_.SetShowRequested(PassKey(), true);
  model_.SetActionItemProperties(
      PassKey(),
      ActionItem::Builder().SetEnabled(true).SetVisible(true).Build().get());
  model_.SetTabActive(PassKey(), true);
  // Confirm it's now visible by default.
  EXPECT_TRUE(model_.GetVisible());

  // Because we know we are about to toggle “hide” on and off once,
  // we expect 2 calls to OnPageActionModelChanged():
  //   1) false->true  => triggers a notify
  //   2) true->false  => triggers a notify
  EXPECT_CALL(observer_, OnPageActionModelChanged).Times(2);

  model_.SetIsSuppressedByOmnibox(PassKey(), true);
  EXPECT_FALSE(model_.GetVisible());

  model_.SetIsSuppressedByOmnibox(PassKey(), false);
  EXPECT_TRUE(model_.GetVisible());
}

TEST_F(PageActionModelTest, ShouldIgnoreOmniboxSuppression) {
  model_.SetShowRequested(PassKey(), true);
  model_.SetActionItemProperties(
      PassKey(),
      ActionItem::Builder().SetEnabled(true).SetVisible(true).Build().get());
  model_.SetTabActive(PassKey(), true);
  model_.SetExemptFromOmniboxSuppression(PassKey(), true);

  // Confirm it's now visible by default.
  EXPECT_TRUE(model_.GetVisible());

  EXPECT_CALL(observer_, OnPageActionModelChanged).Times(1);

  model_.SetIsSuppressedByOmnibox(PassKey(), true);
  EXPECT_TRUE(model_.GetVisible());
}

TEST_F(PageActionModelTest, OverrideAccessibleName) {
  // Set the default accessible name through SetActionItemProperties.
  EXPECT_CALL(observer_, OnPageActionModelChanged).Times(1);
  model_.SetActionItemProperties(
      PassKey(), ActionItem::Builder().SetText(kDefaultText).Build().get());

  EXPECT_EQ(model_.GetAccessibleName(), kDefaultText);

  // Set the override name — should notify and change value.
  EXPECT_CALL(observer_, OnPageActionModelChanged).Times(1);
  model_.SetOverrideAccessibleName(PassKey(), kOverrideName);
  EXPECT_EQ(model_.GetAccessibleName(), kOverrideName);

  // Clear the override — should notify and fallback to default.
  EXPECT_CALL(observer_, OnPageActionModelChanged).Times(1);
  model_.SetOverrideAccessibleName(PassKey(), std::nullopt);
  EXPECT_EQ(model_.GetAccessibleName(), kDefaultText);
}

TEST_F(PageActionModelTest, ActionActive) {
  // Default state should be inactive.
  EXPECT_FALSE(model_.GetActionActive());

  // Setting active should notify and update the state.
  EXPECT_CALL(observer_, OnPageActionModelChanged).Times(1);
  model_.SetActionActive(PassKey(), true);
  EXPECT_TRUE(model_.GetActionActive());
  testing::Mock::VerifyAndClearExpectations(&observer_);

  // Setting active again should not notify or change the state.
  EXPECT_CALL(observer_, OnPageActionModelChanged).Times(0);
  model_.SetActionActive(PassKey(), true);
  EXPECT_TRUE(model_.GetActionActive());
  testing::Mock::VerifyAndClearExpectations(&observer_);

  // Setting inactive should notify and update the state.
  EXPECT_CALL(observer_, OnPageActionModelChanged).Times(1);
  model_.SetActionActive(PassKey(), false);
  EXPECT_FALSE(model_.GetActionActive());
  testing::Mock::VerifyAndClearExpectations(&observer_);

  // Setting inactive again should not notify or change the state.
  EXPECT_CALL(observer_, OnPageActionModelChanged).Times(0);
  model_.SetActionActive(PassKey(), false);
  EXPECT_FALSE(model_.GetActionActive());
  testing::Mock::VerifyAndClearExpectations(&observer_);
}

TEST_F(PageActionModelTest, OverrideImageWithColorSource) {
  model_.SetActionItemProperties(
      PassKey(), ActionItem::Builder().SetImage(kTestImage).Build().get());
  EXPECT_EQ(model_.GetImage(), kTestImage);
  EXPECT_EQ(model_.GetColorSource(), PageActionColorSource::kForeground);

  ui::ImageModel kOverrideImage =
      ui::ImageModel::FromImageSkia(gfx::test::CreateImageSkia(/*size=*/32));

  // Override with a new image and color source.
  EXPECT_CALL(observer_, OnPageActionModelChanged).Times(1);
  model_.SetOverrideImage(PassKey(), kOverrideImage,
                          PageActionColorSource::kCascadingAccent);
  EXPECT_EQ(model_.GetImage(), kOverrideImage);
  EXPECT_EQ(model_.GetColorSource(), PageActionColorSource::kCascadingAccent);

  // Override with the same image and color source should not notify.
  EXPECT_CALL(observer_, OnPageActionModelChanged).Times(0);
  model_.SetOverrideImage(PassKey(), kOverrideImage,
                          PageActionColorSource::kCascadingAccent);
  EXPECT_EQ(model_.GetImage(), kOverrideImage);
  EXPECT_EQ(model_.GetColorSource(), PageActionColorSource::kCascadingAccent);

  // Clear override image, should revert to default image and preserve color
  // source.
  EXPECT_CALL(observer_, OnPageActionModelChanged).Times(1);
  model_.SetOverrideImage(PassKey(), std::nullopt,
                          PageActionColorSource::kCascadingAccent);
  EXPECT_EQ(model_.GetImage(), kTestImage);
  EXPECT_EQ(model_.GetColorSource(), PageActionColorSource::kCascadingAccent);
}

}  // namespace
}  // namespace page_actions
