// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/views/page_action/page_action_container_view.h"

#include <memory>
#include <optional>
#include <utility>
#include <vector>

#include "base/functional/bind.h"
#include "base/memory/raw_ptr.h"
#include "base/scoped_observation.h"
#include "base/test/scoped_feature_list.h"
#include "chrome/browser/ui/actions/chrome_action_id.h"
#include "chrome/browser/ui/page_action/page_action_model.h"
#include "chrome/browser/ui/page_action/page_action_model_observer.h"
#include "chrome/browser/ui/page_action/page_action_pass_key.h"
#include "chrome/browser/ui/page_action/test_support/mock_page_action_controller.h"
#include "chrome/browser/ui/page_action/test_support/test_page_action_properties_provider.h"
#include "chrome/browser/ui/ui_features.h"
#include "chrome/browser/ui/views/page_action/page_action_view.h"
#include "chrome/browser/ui/views/page_action/page_action_view_params.h"
#include "chrome/common/chrome_features.h"
#include "components/vector_icons/vector_icons.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "ui/actions/actions.h"
#include "ui/base/ui_base_features.h"
#include "ui/gfx/font_list.h"
#include "ui/views/controls/label.h"
#include "ui/views/test/views_test_base.h"
#include "ui/views/view_utils.h"
#include "ui/views/widget/widget.h"

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
      page_action_container->EnsurePageActionView(kTestPageActionId);
  ASSERT_TRUE(!!page_action_view);
  EXPECT_EQ(page_action_view,
            page_action_container->GetPageActionView(kTestPageActionId));
  EXPECT_EQ(kTestPageActionId, page_action_view->GetActionId());

  // Returns null if the action ID is not found.
  static constexpr actions::ActionId kNonExistantPageActionId = 1;
  EXPECT_EQ(nullptr,
            page_action_container->GetPageActionView(kNonExistantPageActionId));
  EXPECT_EQ(nullptr, page_action_container->EnsurePageActionView(
                         kNonExistantPageActionId));
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

  PageActionView* view1 =
      container_ptr->EnsurePageActionView(kTestPageActionId1);
  PageActionView* view2 =
      container_ptr->EnsurePageActionView(kTestPageActionId2);

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

  PageActionView* view1 =
      container_ptr->EnsurePageActionView(kTestPageActionId1);
  PageActionView* view2 =
      container_ptr->EnsurePageActionView(kTestPageActionId2);

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

  PageActionView* view1 =
      container_ptr->EnsurePageActionView(kTestPageActionId1);

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

TEST_F(PageActionContainerViewTest, IsPageActionVisible) {
  actions::ActionItem* item = actions::ActionManager::Get().AddAction(
      actions::ActionItem::Builder().SetActionId(kTestPageActionId).Build());
  PageActionContainerView container(
      {item}, TestPageActionPropertiesProvider(kTestProperties),
      DefaultViewParams());

  EXPECT_FALSE(container.IsPageActionVisible(kTestPageActionId));
  static constexpr actions::ActionId kNonExistentActionId = 1;
  EXPECT_FALSE(container.IsPageActionVisible(kNonExistentActionId));

  PageActionModel model(kTestPageActionId);
  auto pass_key = PageActionPassKey::PassKeyForTesting();
  model.SetActionItemProperties(pass_key, item);
  model.SetTabActive(pass_key, true);
  model.SetShowRequested(pass_key, true);
  container.EnsurePageActionView(kTestPageActionId)
      ->OnPageActionModelChanged(model);
  EXPECT_TRUE(container.IsPageActionVisible(kTestPageActionId));
}

class PageActionContainerViewLazyTest : public PageActionContainerViewTest {
 public:
  static constexpr actions::ActionId kActionId1 = kActionShowZoomBubble;
  static constexpr actions::ActionId kActionId2 = kActionSidePanelShowBookmarks;

  PageActionContainerViewLazyTest() {
    lazy_feature_list_.InitAndEnableFeature(
        features::kInitialWebUIChromeOptimizations);
  }

  void SetUp() override {
    PageActionContainerViewTest::SetUp();
    item1_ = actions::ActionManager::Get().AddAction(
        actions::ActionItem::Builder().SetActionId(kActionId1).Build());
    item2_ = actions::ActionManager::Get().AddAction(
        actions::ActionItem::Builder().SetActionId(kActionId2).Build());
    ResetModel(model1_, item1_);
    ResetModel(model2_, item2_);
    ON_CALL(controller_, AddObserver(testing::_, testing::_))
        .WillByDefault([this](actions::ActionId id, auto& observation) {
          observation.Observe(ModelFor(id));
        });
    ON_CALL(controller_, CreateActionItemSubscription(testing::_))
        .WillByDefault([this](actions::ActionItem* item) {
          auto cb = base::BindRepeating(
              &PageActionModel::SetActionItemProperties,
              base::Unretained(ModelFor(item->GetActionId().value())),
              PageActionPassKey::PassKeyForTesting(), base::Unretained(item));
          cb.Run();
          return item->AddActionChangedCallback(std::move(cb));
        });
  }

  void TearDown() override {
    container_ = nullptr;
    if (widget_) {
      widget_->CloseNow();
      widget_.reset();
    }
    model1_.reset();
    model2_.reset();
    item1_ = nullptr;
    item2_ = nullptr;
    PageActionContainerViewTest::TearDown();
  }

 protected:
  void ResetModel(std::unique_ptr<PageActionModel>& model,
                  actions::ActionItem* item,
                  bool show = false) {
    model = std::make_unique<PageActionModel>(item->GetActionId().value());
    auto key = PageActionPassKey::PassKeyForTesting();
    model->SetActionItemProperties(key, item);
    model->SetTabActive(key, true);
    model->SetShowRequested(key, show);
  }

  PageActionModel* ModelFor(actions::ActionId id) {
    return id == kActionId1 ? model1_.get() : model2_.get();
  }

  void CreateContainer(std::optional<PageActionViewParams> params = {}) {
    widget_ = CreateTestWidget(views::Widget::InitParams::CLIENT_OWNS_WIDGET);
    container_ =
        widget_->SetContentsView(std::make_unique<PageActionContainerView>(
            std::vector<actions::ActionItem*>{item1_, item2_},
            TestPageActionPropertiesProvider(properties_),
            params.value_or(DefaultViewParams())));
    widget_->Show();
  }

  base::test::ScopedFeatureList lazy_feature_list_;
  const PageActionPropertiesMap properties_{
      {kActionId1,
       {.histogram_name = "TestZoom", .type = PageActionIconType::kZoom}},
      {kActionId2,
       {.histogram_name = "TestBookmark",
        .type = PageActionIconType::kBookmarkStar}},
  };
  testing::NiceMock<MockPageActionController> controller_;
  std::unique_ptr<PageActionModel> model1_;
  std::unique_ptr<PageActionModel> model2_;
  raw_ptr<actions::ActionItem> item1_ = nullptr;
  raw_ptr<actions::ActionItem> item2_ = nullptr;
  std::unique_ptr<views::Widget> widget_;
  raw_ptr<PageActionContainerView> container_ = nullptr;
};

TEST_F(PageActionContainerViewLazyTest, LazyCreationOrderAndFontListLifetime) {
  const int expected_font_size = gfx::FontList().GetFontSize() + 4;
  {
    gfx::FontList stack_font_list = gfx::FontList().DeriveWithSizeDelta(4);
    PageActionViewParams params = DefaultViewParams();
    params.font_list = stack_font_list;
    CreateContainer(params);
  }

  EXPECT_THAT(container_->children(), testing::IsEmpty());
  EXPECT_FALSE(container_->IsPageActionVisible(kActionId1));
  EXPECT_FALSE(container_->IsPageActionVisible(kActionId2));
  EXPECT_THAT(container_->children(), testing::IsEmpty());

  // Attaching the controller materializes only already-visible action 2.
  model2_->SetShowRequested(PageActionPassKey::PassKeyForTesting(), true);
  container_->SetController(&controller_);
  PageActionView* view2 = container_->GetPageActionView(kActionId2);
  EXPECT_EQ(nullptr, container_->GetPageActionView(kActionId1));
  EXPECT_THAT(container_->children(), testing::ElementsAre(view2));
  EXPECT_FALSE(container_->IsPageActionVisible(kActionId1));
  EXPECT_TRUE(container_->IsPageActionVisible(kActionId2));

  // Detaching stops observing pending action 1.
  container_->SetController(nullptr);
  model1_->SetShowRequested(PageActionPassKey::PassKeyForTesting(), true);
  EXPECT_THAT(container_->children(), testing::ElementsAre(view2));

  // Re-attaching materializes action 1 in initial order with copied font list.
  container_->SetController(&controller_);
  PageActionView* view1 = container_->GetPageActionView(kActionId1);
  EXPECT_THAT(container_->children(), testing::ElementsAre(view1, view2));
  EXPECT_TRUE(container_->IsPageActionVisible(kActionId1));
  EXPECT_EQ(expected_font_size,
            views::AsViewClass<views::Label>(view1->GetLabelForTesting())
                ->font_list()
                .GetFontSize());
}

TEST_F(PageActionContainerViewLazyTest,
       GetAndEnsurePageActionViewBeforeAndAfterController) {
  CreateContainer();
  EXPECT_EQ(nullptr, container_->GetPageActionView(kActionId1));
  EXPECT_THAT(container_->children(), testing::IsEmpty());

  PageActionView* view1 = container_->EnsurePageActionView(kActionId1);
  ASSERT_NE(nullptr, view1);
  EXPECT_EQ(view1, container_->GetPageActionView(kActionId1));
  EXPECT_THAT(container_->children(), testing::ElementsAre(view1));

  model1_->SetShowRequested(PageActionPassKey::PassKeyForTesting(), true);
  container_->SetController(&controller_);
  EXPECT_TRUE(container_->IsPageActionVisible(kActionId1));
  EXPECT_THAT(container_->children(), testing::ElementsAre(view1));

  EXPECT_EQ(nullptr, container_->GetPageActionView(kActionId2));
  PageActionView* view2 = container_->EnsurePageActionView(kActionId2);
  ASSERT_NE(nullptr, view2);
  EXPECT_EQ(view2, container_->GetPageActionView(kActionId2));
  EXPECT_THAT(container_->children(), testing::ElementsAre(view1, view2));
  model2_->SetShowRequested(PageActionPassKey::PassKeyForTesting(), true);
  EXPECT_TRUE(container_->IsPageActionVisible(kActionId2));

  static constexpr actions::ActionId kNonExistentActionId = 1;
  EXPECT_EQ(nullptr, container_->EnsurePageActionView(kNonExistentActionId));
}

TEST_F(PageActionContainerViewLazyTest,
       MaterializeDuringCreateActionItemSubscription) {
  CreateContainer();
  EXPECT_CALL(controller_, CreateActionItemSubscription(item1_.get()))
      .WillOnce([&](actions::ActionItem* item) {
        container_->EnsurePageActionView(kActionId1);
        return base::CallbackListSubscription();
      })
      .WillOnce(testing::DoDefault());
  EXPECT_CALL(controller_, CreateActionItemSubscription(item2_.get()))
      .WillOnce(testing::DoDefault());

  container_->SetController(&controller_);
  ASSERT_NE(nullptr, container_->GetPageActionView(kActionId1));
  EXPECT_EQ(nullptr, container_->GetPageActionView(kActionId2));
}

TEST_F(PageActionContainerViewLazyTest,
       ModelDeletionDetachesAndAllowsReattach) {
  CreateContainer();
  model1_->SetShowRequested(PageActionPassKey::PassKeyForTesting(), true);
  container_->SetController(&controller_);
  ASSERT_EQ(1u, container_->children().size());

  model1_.reset();
  model2_.reset();
  EXPECT_FALSE(container_->IsPageActionVisible(kActionId1));
  EXPECT_EQ(1u, container_->children().size());

  // Re-attaching the same controller address with new models rebinds both.
  ResetModel(model1_, item1_, /*show=*/true);
  ResetModel(model2_, item2_, /*show=*/true);
  container_->SetController(&controller_);
  EXPECT_TRUE(container_->IsPageActionVisible(kActionId1));
  EXPECT_TRUE(container_->IsPageActionVisible(kActionId2));
  EXPECT_EQ(2u, container_->children().size());
}

TEST_F(PageActionContainerViewLazyTest,
       LazyMaterializationDoesNotDoubleNotify) {
  struct CountingObserver : public PageActionModelObserver {
    void OnPageActionModelChanged(const PageActionModelInterface&) override {
      ++notify_count;
    }
    int notify_count = 0;
  };

  CreateContainer();
  CountingObserver sibling_observer;
  base::ScopedObservation<PageActionModelInterface, PageActionModelObserver>
      sibling_observation{&sibling_observer};
  int add_observer_calls = 0;
  ON_CALL(controller_, AddObserver(kActionId1, testing::_))
      .WillByDefault([&](actions::ActionId id, auto& observation) {
        observation.Observe(model1_.get());
        if (++add_observer_calls == 2) {
          sibling_observation.Observe(model1_.get());
        }
      });

  container_->SetController(&controller_);
  model1_->SetShowRequested(PageActionPassKey::PassKeyForTesting(), true);
  EXPECT_EQ(2, add_observer_calls);
  EXPECT_EQ(0, sibling_observer.notify_count);
  EXPECT_TRUE(container_->IsPageActionVisible(kActionId1));

  model1_->SetShowRequested(PageActionPassKey::PassKeyForTesting(), false);
  EXPECT_EQ(1, sibling_observer.notify_count);
}

TEST_F(PageActionContainerViewTest, EagerViewCreationWhenFeatureDisabled) {
  actions::ActionItem* item = actions::ActionManager::Get().AddAction(
      actions::ActionItem::Builder().SetActionId(kTestPageActionId).Build());
  for (bool enable_umbrella : {false, true}) {
    SCOPED_TRACE(enable_umbrella);
    base::test::ScopedFeatureList feature_list;
    if (enable_umbrella) {
      feature_list.InitAndEnableFeatureWithParameters(
          features::kInitialWebUIChromeOptimizations,
          {{features::kPageActionsLazyViewCreation.name, "false"}});
    } else {
      feature_list.InitAndDisableFeature(
          features::kInitialWebUIChromeOptimizations);
    }
    PageActionContainerView container(
        {item}, TestPageActionPropertiesProvider(kTestProperties),
        DefaultViewParams());
    EXPECT_EQ(1u, container.children().size());
    EXPECT_NE(nullptr, container.GetPageActionView(kTestPageActionId));
    EXPECT_FALSE(container.IsPageActionVisible(kTestPageActionId));
  }
}

}  // namespace
}  // namespace page_actions
