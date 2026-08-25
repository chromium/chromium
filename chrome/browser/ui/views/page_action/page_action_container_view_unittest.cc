// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/views/page_action/page_action_container_view.h"

#include "base/test/scoped_feature_list.h"
#include "chrome/browser/ui/actions/chrome_action_id.h"
#include "chrome/browser/ui/page_action/page_action_model.h"
#include "chrome/browser/ui/page_action/page_action_pass_key.h"
#include "chrome/browser/ui/page_action/test_support/test_page_action_properties_provider.h"
#include "chrome/browser/ui/ui_features.h"
#include "chrome/browser/ui/views/page_action/page_action_view.h"
#include "chrome/browser/ui/views/page_action/page_action_view_params.h"
#include "components/vector_icons/vector_icons.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "ui/base/ui_base_features.h"
#include "ui/views/test/views_test_base.h"

namespace page_actions {
namespace {

constexpr int kDefaultBetweenIconSpacing = 8;
constexpr int kDefaultIconSize = 16;

static constexpr actions::ActionId kTestPageActionId = kActionShowZoomBubble;
static const PageActionPropertiesMap kTestProperties = PageActionPropertiesMap{
    {
        kTestPageActionId,
        PageActionProperties{
            .histogram_name = "TestZoom",
            .type = PageActionIconType::kZoom,
        },
    },
};

class MockIconLabelViewDelegate : public IconLabelBubbleView::Delegate {
 public:
  MOCK_METHOD(SkColor,
              GetIconLabelBubbleSurroundingForegroundColor,
              (),
              (const, override));
  MOCK_METHOD(SkColor,
              GetIconLabelBubbleBackgroundColor,
              (),
              (const, override));
};

class PageActionContainerViewTest : public views::ViewsTestBase {
 public:
  PageActionContainerViewTest() = default;

  ~PageActionContainerViewTest() override = default;

  void TearDown() override {
    views::ViewsTestBase::TearDown();
    actions::ActionManager::Get().ResetActions();
  }

  PageActionViewParams DefaultViewParams() {
    return PageActionViewParams{
        .icon_size = kDefaultIconSize,
        .between_icon_spacing = kDefaultBetweenIconSpacing,
        .icon_label_bubble_delegate = &icon_label_view_delegate_};
  }

 private:
  base::test::ScopedFeatureList scoped_feature_list_;
  MockIconLabelViewDelegate icon_label_view_delegate_;
};

TEST_F(PageActionContainerViewTest, GetPageActionView) {
  actions::ActionItem* action_item = actions::ActionManager::Get().AddAction(
      actions::ActionItem::Builder()
          .SetImage(ui::ImageModel::FromVectorIcon(
              features::IsRoundedIconsEnabled()
                  ? vector_icons::kArrowBackIcon
                  : vector_icons::kBackArrowOldIcon,
              ui::kColorSysPrimary,
              /*icon_size=*/16))
          .SetActionId(kTestPageActionId)
          .Build());

  auto page_action_container = std::make_unique<PageActionContainerView>(
      std::vector<actions::ActionItem*>{action_item},
      TestPageActionPropertiesProvider(kTestProperties), DefaultViewParams());

  PageActionView* page_action_view =
      page_action_container->GetPageActionView(kTestPageActionId);
  ASSERT_TRUE(!!page_action_view);
  EXPECT_EQ(kTestPageActionId, page_action_view->GetActionId());

  // Returns null if the action ID is not found.
  static constexpr actions::ActionId kNonExistantPageActionId = 1;
  EXPECT_EQ(nullptr,
            page_action_container->GetPageActionView(kNonExistantPageActionId));
}

// Verifies the container's elevated "capsule" background (elevated
// toolbar) is updated dynamically based on the number of visible page action
// icons.
TEST_F(PageActionContainerViewTest,
       CapsuleBackgroundUpdatedOnVisibilityChange) {
  base::test::ScopedFeatureList feature_list;
  feature_list.InitAndEnableFeature(features::kPageActionsElevatedToolbar);

  static constexpr actions::ActionId kTestPageActionId1 = kActionShowZoomBubble;
  static constexpr actions::ActionId kTestPageActionId2 =
      kActionSidePanelShowBookmarks;

  static const PageActionPropertiesMap kProperties = PageActionPropertiesMap{
      {kTestPageActionId1,
       PageActionProperties{.histogram_name = "TestZoom",
                            .type = PageActionIconType::kZoom}},
      {kTestPageActionId2,
       PageActionProperties{.histogram_name = "TestBookmark",
                            .type = PageActionIconType::kBookmarkStar}},
  };

  actions::ActionItem* action_item1 = actions::ActionManager::Get().AddAction(
      actions::ActionItem::Builder().SetActionId(kTestPageActionId1).Build());
  actions::ActionItem* action_item2 = actions::ActionManager::Get().AddAction(
      actions::ActionItem::Builder().SetActionId(kTestPageActionId2).Build());

  auto container = std::make_unique<PageActionContainerView>(
      std::vector<actions::ActionItem*>{action_item1, action_item2},
      TestPageActionPropertiesProvider(kProperties), DefaultViewParams());

  std::unique_ptr<views::Widget> widget =
      CreateTestWidget(views::Widget::InitParams::CLIENT_OWNS_WIDGET);
  PageActionContainerView* container_ptr =
      widget->SetContentsView(std::move(container));
  widget->Show();

  PageActionView* view1 = container_ptr->GetPageActionView(kTestPageActionId1);
  PageActionView* view2 = container_ptr->GetPageActionView(kTestPageActionId2);

  // Both actions are hidden initially: No background.
  EXPECT_FALSE(container_ptr->IsCapsuleActive());
  EXPECT_EQ(nullptr, container_ptr->GetBackground());

  auto pass_key = PageActionPassKey::PassKeyForTesting();

  // Make only one action visible: Background should remain hidden.
  PageActionModel model1(kTestPageActionId1);
  model1.SetActionItemProperties(pass_key, action_item1);
  model1.SetTabActive(pass_key, true);
  model1.SetShowRequested(pass_key, true);
  view1->OnPageActionModelChanged(model1);
  EXPECT_FALSE(container_ptr->IsCapsuleActive());
  EXPECT_EQ(nullptr, container_ptr->GetBackground());

  // Make a second action visible: Background should appear.
  PageActionModel model2(kTestPageActionId2);
  model2.SetActionItemProperties(pass_key, action_item2);
  model2.SetTabActive(pass_key, true);
  model2.SetShowRequested(pass_key, true);
  view2->OnPageActionModelChanged(model2);
  EXPECT_TRUE(container_ptr->IsCapsuleActive());
  EXPECT_NE(nullptr, container_ptr->GetBackground());

  // Hide the first action: Background should be removed.
  model1.SetShowRequested(pass_key, false);
  view1->OnPageActionModelChanged(model1);
  EXPECT_FALSE(container_ptr->IsCapsuleActive());
  EXPECT_EQ(nullptr, container_ptr->GetBackground());

  widget->Close();
}

// Verifies that when the elevated toolbar feature is disabled, the capsule
// background is not applied even when multiple page action icons are visible.
TEST_F(PageActionContainerViewTest,
       CapsuleBackgroundDisabledWhenFeatureDisabled) {
  base::test::ScopedFeatureList feature_list;
  feature_list.InitAndDisableFeature(features::kPageActionsElevatedToolbar);

  static constexpr actions::ActionId kTestPageActionId1 = kActionShowZoomBubble;
  static constexpr actions::ActionId kTestPageActionId2 =
      kActionSidePanelShowBookmarks;

  static const PageActionPropertiesMap kProperties = PageActionPropertiesMap{
      {kTestPageActionId1,
       PageActionProperties{.histogram_name = "TestZoom",
                            .type = PageActionIconType::kZoom}},
      {kTestPageActionId2,
       PageActionProperties{.histogram_name = "TestBookmark",
                            .type = PageActionIconType::kBookmarkStar}},
  };

  actions::ActionItem* action_item1 = actions::ActionManager::Get().AddAction(
      actions::ActionItem::Builder().SetActionId(kTestPageActionId1).Build());
  actions::ActionItem* action_item2 = actions::ActionManager::Get().AddAction(
      actions::ActionItem::Builder().SetActionId(kTestPageActionId2).Build());

  auto container = std::make_unique<PageActionContainerView>(
      std::vector<actions::ActionItem*>{action_item1, action_item2},
      TestPageActionPropertiesProvider(kProperties), DefaultViewParams());

  std::unique_ptr<views::Widget> widget =
      CreateTestWidget(views::Widget::InitParams::CLIENT_OWNS_WIDGET);
  PageActionContainerView* container_ptr =
      widget->SetContentsView(std::move(container));
  widget->Show();

  PageActionView* view1 = container_ptr->GetPageActionView(kTestPageActionId1);
  PageActionView* view2 = container_ptr->GetPageActionView(kTestPageActionId2);

  auto pass_key = PageActionPassKey::PassKeyForTesting();

  PageActionModel model1(kTestPageActionId1);
  model1.SetActionItemProperties(pass_key, action_item1);
  model1.SetTabActive(pass_key, true);
  model1.SetShowRequested(pass_key, true);
  view1->OnPageActionModelChanged(model1);

  PageActionModel model2(kTestPageActionId2);
  model2.SetActionItemProperties(pass_key, action_item2);
  model2.SetTabActive(pass_key, true);
  model2.SetShowRequested(pass_key, true);
  view2->OnPageActionModelChanged(model2);

  // Background should not be set when feature is disabled.
  EXPECT_FALSE(container_ptr->IsCapsuleActive());
  EXPECT_EQ(nullptr, container_ptr->GetBackground());

  widget->Close();
}

// Verifies that IsFirstVisibleViewChip correctly reports whether the first
// visible page action is rendered as a chip.
TEST_F(PageActionContainerViewTest, IsFirstVisibleViewChip) {
  static constexpr actions::ActionId kTestPageActionId1 = kActionShowZoomBubble;
  static constexpr actions::ActionId kTestPageActionId2 =
      kActionSidePanelShowBookmarks;

  static const PageActionPropertiesMap kProperties = PageActionPropertiesMap{
      {kTestPageActionId1,
       PageActionProperties{.histogram_name = "TestZoom",
                            .type = PageActionIconType::kZoom}},
      {kTestPageActionId2,
       PageActionProperties{.histogram_name = "TestBookmark",
                            .type = PageActionIconType::kBookmarkStar}},
  };

  actions::ActionItem* action_item1 = actions::ActionManager::Get().AddAction(
      actions::ActionItem::Builder()
          .SetActionId(kTestPageActionId1)
          .SetText(u"Test")
          .Build());
  actions::ActionItem* action_item2 = actions::ActionManager::Get().AddAction(
      actions::ActionItem::Builder().SetActionId(kTestPageActionId2).Build());

  auto container = std::make_unique<PageActionContainerView>(
      std::vector<actions::ActionItem*>{action_item1, action_item2},
      TestPageActionPropertiesProvider(kProperties), DefaultViewParams());

  std::unique_ptr<views::Widget> widget =
      CreateTestWidget(views::Widget::InitParams::CLIENT_OWNS_WIDGET);
  PageActionContainerView* container_ptr =
      widget->SetContentsView(std::move(container));
  widget->Show();

  PageActionView* view1 = container_ptr->GetPageActionView(kTestPageActionId1);

  // Initially, no views are visible -> false.
  EXPECT_FALSE(container_ptr->IsFirstVisibleViewChip());

  auto pass_key = PageActionPassKey::PassKeyForTesting();

  // Make view1 visible as an icon (not chip) -> false.
  PageActionModel model1(kTestPageActionId1);
  model1.SetActionItemProperties(pass_key, action_item1);
  model1.SetTabActive(pass_key, true);
  model1.SetShowRequested(pass_key, true);
  view1->OnPageActionModelChanged(model1);
  EXPECT_FALSE(container_ptr->IsFirstVisibleViewChip());

  // Make view1 visible as a suggestion chip -> true.
  model1.SetShouldShowSuggestionChip(pass_key, true);
  model1.SetSuggestionChipConfig(pass_key, {.should_animate = false});
  view1->OnPageActionModelChanged(model1);
  EXPECT_TRUE(container_ptr->IsFirstVisibleViewChip());

  widget->Close();
}

}  // namespace
}  // namespace page_actions
