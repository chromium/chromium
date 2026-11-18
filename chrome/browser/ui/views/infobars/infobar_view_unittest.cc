// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/views/infobars/infobar_view.h"

#include <memory>
#include <optional>
#include <utility>
#include <vector>

#include "base/memory/raw_ptr.h"
#include "base/test/scoped_feature_list.h"
#include "chrome/browser/infobars/confirm_infobar_creation_allowlist.h"
#include "chrome/browser/ui/ui_features.h"
#include "chrome/browser/ui/views/chrome_layout_provider.h"
#include "chrome/browser/ui/views/infobars/confirm_infobar.h"
#include "chrome/browser/ui/views/infobars/infobar_container_view.h"
#include "components/infobars/core/confirm_infobar_delegate.h"
#include "components/infobars/core/infobar_delegate.h"
#include "components/infobars/core/infobar_manager.h"
#include "components/vector_icons/vector_icons.h"
#include "ui/base/mojom/window_open_disposition.mojom-shared.h"
#include "ui/base/ui_base_features.h"
#include "ui/gfx/animation/animation_test_api.h"
#include "ui/gfx/animation/tween.h"
#include "ui/gfx/scoped_animation_duration_scale_mode.h"
#include "ui/views/controls/button/md_text_button.h"
#include "ui/views/controls/image_view.h"
#include "ui/views/controls/label.h"
#include "ui/views/controls/link.h"
#include "ui/views/layout/flex_layout.h"
#include "ui/views/test/views_test_base.h"
#include "ui/views/view_class_properties.h"
#include "ui/views/widget/widget.h"
#include "url/gurl.h"

namespace {

class TestInfoBarDelegateWithIcon : public infobars::InfoBarDelegate {
 public:
  TestInfoBarDelegateWithIcon() = default;
  ~TestInfoBarDelegateWithIcon() override = default;

  infobars::InfoBarDelegate::InfoBarIdentifier GetIdentifier() const override {
    return TEST_INFOBAR;
  }

  const gfx::VectorIcon& GetVectorIcon() const override {
    return features::IsRoundedIconsEnabled() ? vector_icons::kWarningFilledIcon
                                             : vector_icons::kWarningOldIcon;
  }
};

class TestInfoBarViewWithLabelAndIcon : public InfoBarView {
 public:
  explicit TestInfoBarViewWithLabelAndIcon(
      std::unique_ptr<infobars::InfoBarDelegate> delegate)
      : InfoBarView(std::move(delegate)) {
    test_label_ = AddContentChildView(CreateLabel(u"Test Label"));
    test_label_->SetProperty(
        views::kFlexBehaviorKey,
        views::FlexSpecification(views::MinimumFlexSizeRule::kScaleToZero,
                                 views::MaximumFlexSizeRule::kPreferred));
  }

  views::Label* test_label() { return test_label_; }
  gfx::SlideAnimation* mutable_animation() { return animation(); }
  int GetPublicEndX() const { return GetEndX(); }
  int GetTotalHeight() const {
    return ChromeLayoutProvider::Get()->GetDistanceMetric(
        DISTANCE_INFOBAR_HEIGHT);
  }

 protected:
  int GetContentMinimumWidth() const override { return 0; }

  int GetContentPreferredWidth() const override {
    return test_label_->GetPreferredSize().width();
  }

 private:
  raw_ptr<views::Label> test_label_ = nullptr;
};

class TestConfirmInfoBarDelegate : public ConfirmInfoBarDelegate {
 public:
  explicit TestConfirmInfoBarDelegate(
      int buttons = BUTTON_OK,
      std::optional<ui::ButtonStyle> ok_style = std::nullopt,
      std::optional<ui::ButtonStyle> cancel_style = std::nullopt)
      : buttons_(buttons), ok_style_(ok_style), cancel_style_(cancel_style) {}
  ~TestConfirmInfoBarDelegate() override = default;

  infobars::InfoBarDelegate::InfoBarIdentifier GetIdentifier() const override {
    return TEST_INFOBAR;
  }

  std::u16string GetMessageText() const override { return u"Test message"; }

  int GetButtons() const override { return buttons_; }

  std::optional<ui::ButtonStyle> GetButtonStyle(
      InfoBarButton button) const override {
    return (button == BUTTON_OK) ? ok_style_ : cancel_style_;
  }

 private:
  int buttons_;
  std::optional<ui::ButtonStyle> ok_style_;
  std::optional<ui::ButtonStyle> cancel_style_;
};

class TestConfirmInfoBarDelegateWithLink : public TestConfirmInfoBarDelegate {
 public:
  std::u16string GetLinkText() const override { return u"Test link"; }
};

class UniqueConfirmInfoBarDelegate : public TestConfirmInfoBarDelegate {
 public:
  bool EqualsDelegate(infobars::InfoBarDelegate* delegate) const override {
    return false;
  }
};

class FakeInfoBarManager : public infobars::InfoBarManager {
 public:
  FakeInfoBarManager() = default;
  ~FakeInfoBarManager() override = default;

  using InfoBarManager::set_animations_enabled;

  int GetActiveEntryID() override { return 0; }
  void OpenURL(const GURL& url,
               ui::mojom::WindowOpenDisposition disposition,
               const std::string& text_fragment) override {}
};

class DummyContainerDelegate : public infobars::InfoBarContainer::Delegate {
 public:
  void InfoBarContainerStateChanged(bool is_animating) override {}
};

}  // namespace

class InfoBarViewUnitTest : public views::ViewsTestBase {
 public:
  InfoBarViewUnitTest() = default;

  void SetUp() override {
    views::ViewsTestBase::SetUp();
    layout_provider_ = std::make_unique<ChromeLayoutProvider>();
  }

 private:
  std::unique_ptr<ChromeLayoutProvider> layout_provider_;
};

TEST_F(InfoBarViewUnitTest, CenteredLayout) {
  auto delegate = std::make_unique<TestInfoBarDelegateWithIcon>();
  auto infobar_view =
      std::make_unique<TestInfoBarViewWithLabelAndIcon>(std::move(delegate));

  auto widget = std::make_unique<views::Widget>();
  views::Widget::InitParams params =
      CreateParams(views::Widget::InitParams::CLIENT_OWNS_WIDGET,
                   views::Widget::InitParams::TYPE_WINDOW_FRAMELESS);
  widget->Init(std::move(params));

  widget->SetContentsView(infobar_view.get());
  widget->SetBounds(gfx::Rect(0, 0, 500, 50));
  widget->Show();
  widget->LayoutRootViewIfNecessary();

  // Verify the FlexLayout properties for centering.
  views::FlexLayout* layout =
      static_cast<views::FlexLayout*>(infobar_view->GetLayoutManager());
  ASSERT_NE(nullptr, layout);
  EXPECT_EQ(views::LayoutAlignment::kCenter, layout->cross_axis_alignment());

  // Find the balancer and spacer views used for centering.
  views::View* left_balancer = nullptr;
  views::View* right_spacer = nullptr;
  std::vector<views::View*> spacers;
  for (views::View* child : infobar_view->children()) {
    if (child->GetProperty(views::kElementIdentifierKey) ==
        InfoBarView::kLeftBalancerElementId) {
      left_balancer = child;
    } else if (child->GetProperty(views::kElementIdentifierKey) ==
               InfoBarView::kRightSpacerElementId) {
      right_spacer = child;
    }
    const views::FlexSpecification* flex_spec =
        child->GetProperty(views::kFlexBehaviorKey);
    if (flex_spec && flex_spec->weight() == 1) {
      spacers.push_back(child);
    }
  }

  // Verify the balancer and spacer are present.
  ASSERT_NE(nullptr, left_balancer);
  ASSERT_NE(nullptr, right_spacer);

  // Verify there are at least two spacers with the correct flex properties.
  ASSERT_GE(spacers.size(), 2u);
  for (views::View* spacer : spacers) {
    const views::FlexSpecification* spec =
        spacer->GetProperty(views::kFlexBehaviorKey);
    ASSERT_NE(nullptr, spec);
    EXPECT_EQ(1, spec->weight());
  }

  widget->CloseNow();
}

TEST_F(InfoBarViewUnitTest, CloseButtonIsVisibleAndCorrectlyPositioned) {
  auto delegate = std::make_unique<TestInfoBarDelegateWithIcon>();
  auto infobar_view =
      std::make_unique<TestInfoBarViewWithLabelAndIcon>(std::move(delegate));

  auto widget = std::make_unique<views::Widget>();
  views::Widget::InitParams params =
      CreateParams(views::Widget::InitParams::CLIENT_OWNS_WIDGET,
                   views::Widget::InitParams::TYPE_WINDOW_FRAMELESS);
  widget->Init(std::move(params));

  widget->SetContentsView(infobar_view.get());
  widget->SetBounds(gfx::Rect(0, 0, 500, 50));
  widget->Show();
  widget->LayoutRootViewIfNecessary();

  // Verify that the close button is visible.
  views::View* close_button = infobar_view->close_button();
  ASSERT_NE(nullptr, close_button);
  EXPECT_TRUE(close_button->GetVisible());

  // Verify that the close button is positioned at the end of the infobar.
  EXPECT_GT(close_button->x(), infobar_view->test_label()->x());

  widget->CloseNow();
}

TEST_F(InfoBarViewUnitTest, ConfirmInfoBarButtonPadding) {
  auto delegate = std::make_unique<TestConfirmInfoBarDelegate>();
  auto infobar_view = ConfirmInfoBar::Create(
      ConfirmInfoBarCreationAllowlist::GetPassKeyForTesting(),
      std::move(delegate));

  auto widget = std::make_unique<views::Widget>();
  views::Widget::InitParams params =
      CreateParams(views::Widget::InitParams::CLIENT_OWNS_WIDGET,
                   views::Widget::InitParams::TYPE_WINDOW_FRAMELESS);
  widget->Init(std::move(params));

  widget->SetContentsView(infobar_view.get());
  widget->SetBounds(gfx::Rect(0, 0, 500, 50));
  widget->Show();
  widget->LayoutRootViewIfNecessary();

  views::MdTextButton* ok_button = infobar_view->ok_button_for_testing();
  ASSERT_NE(nullptr, ok_button);

  const gfx::Insets expected_padding =
      gfx::Insets::VH(ChromeLayoutProvider::Get()->GetDistanceMetric(
                          DISTANCE_INFOBAR_BUTTON_VERTICAL_PADDING),
                      ChromeLayoutProvider::Get()->GetDistanceMetric(
                          DISTANCE_INFOBAR_BUTTON_HORIZONTAL_PADDING));

  EXPECT_EQ(expected_padding, ok_button->GetInsets());
  widget->CloseNow();
}

TEST_F(InfoBarViewUnitTest, TrailingLinkIsVerticallyCentered) {
  auto delegate = std::make_unique<TestConfirmInfoBarDelegateWithLink>();
  auto infobar_view = ConfirmInfoBar::Create(
      ConfirmInfoBarCreationAllowlist::GetPassKeyForTesting(),
      std::move(delegate));

  auto widget = std::make_unique<views::Widget>();
  views::Widget::InitParams params =
      CreateParams(views::Widget::InitParams::CLIENT_OWNS_WIDGET,
                   views::Widget::InitParams::TYPE_WINDOW_FRAMELESS);
  widget->Init(std::move(params));

  widget->SetContentsView(infobar_view.get());
  widget->SetBounds(gfx::Rect(0, 0, 500, 50));
  widget->Show();
  widget->LayoutRootViewIfNecessary();

  views::Label* message_label = infobar_view->label_for_testing();
  views::Link* link = infobar_view->link_for_testing();
  views::View* close_button = infobar_view->close_button();
  ASSERT_NE(nullptr, message_label);
  ASSERT_NE(nullptr, link);
  ASSERT_NE(nullptr, close_button);
  ASSERT_LT(link->height(), close_button->height());

  const int link_center_y = link->GetBoundsInScreen().CenterPoint().y();
  EXPECT_NEAR(message_label->GetBoundsInScreen().CenterPoint().y(),
              link_center_y, 1);
  EXPECT_NEAR(close_button->GetBoundsInScreen().CenterPoint().y(),
              link_center_y, 1);

  widget->CloseNow();
}

TEST_F(InfoBarViewUnitTest, DefaultInfoBarButtonStyleSingleButton) {
  // Single buttons are Prominent by default.
  // Single OK button.
  {
    auto delegate = std::make_unique<TestConfirmInfoBarDelegate>(
        ConfirmInfoBarDelegate::BUTTON_OK);
    auto infobar_view = ConfirmInfoBar::Create(
        ConfirmInfoBarCreationAllowlist::GetPassKeyForTesting(),
        std::move(delegate));

    EXPECT_EQ(ui::ButtonStyle::kProminent,
              infobar_view->ok_button_for_testing()->GetStyle());
  }
  // Single CANCEL button.
  {
    auto delegate = std::make_unique<TestConfirmInfoBarDelegate>(
        ConfirmInfoBarDelegate::BUTTON_CANCEL);
    auto infobar_view = ConfirmInfoBar::Create(
        ConfirmInfoBarCreationAllowlist::GetPassKeyForTesting(),
        std::move(delegate));

    EXPECT_EQ(ui::ButtonStyle::kProminent,
              infobar_view->cancel_button_for_testing()->GetStyle());
  }
}

TEST_F(InfoBarViewUnitTest, DefaultInfoBarButtonStyleMultipleButton) {
  auto delegate = std::make_unique<TestConfirmInfoBarDelegate>(
      ConfirmInfoBarDelegate::BUTTON_OK |
      ConfirmInfoBarDelegate::BUTTON_CANCEL);
  auto infobar_view = ConfirmInfoBar::Create(
      ConfirmInfoBarCreationAllowlist::GetPassKeyForTesting(),
      std::move(delegate));

  EXPECT_EQ(ui::ButtonStyle::kProminent,
            infobar_view->ok_button_for_testing()->GetStyle());
  EXPECT_EQ(ui::ButtonStyle::kTonal,
            infobar_view->cancel_button_for_testing()->GetStyle());
}

TEST_F(InfoBarViewUnitTest, CustomInfoBarButtonStyle) {
  auto delegate = std::make_unique<TestConfirmInfoBarDelegate>(
      ConfirmInfoBarDelegate::BUTTON_OK | ConfirmInfoBarDelegate::BUTTON_CANCEL,
      ui::ButtonStyle::kTonal, ui::ButtonStyle::kProminent);
  auto infobar_view = ConfirmInfoBar::Create(
      ConfirmInfoBarCreationAllowlist::GetPassKeyForTesting(),
      std::move(delegate));

  EXPECT_EQ(ui::ButtonStyle::kTonal,
            infobar_view->ok_button_for_testing()->GetStyle());
  EXPECT_EQ(ui::ButtonStyle::kProminent,
            infobar_view->cancel_button_for_testing()->GetStyle());
}

TEST_F(InfoBarViewUnitTest, IconSizeForInfobarRefresh) {
  auto delegate = std::make_unique<TestInfoBarDelegateWithIcon>();
  auto infobar_view =
      std::make_unique<TestInfoBarViewWithLabelAndIcon>(std::move(delegate));

  auto widget = std::make_unique<views::Widget>();
  views::Widget::InitParams params =
      CreateParams(views::Widget::InitParams::CLIENT_OWNS_WIDGET,
                   views::Widget::InitParams::TYPE_WINDOW_FRAMELESS);
  widget->Init(std::move(params));

  widget->SetContentsView(infobar_view.get());
  widget->SetBounds(gfx::Rect(0, 0, 500, 50));
  widget->Show();
  widget->LayoutRootViewIfNecessary();

  views::ImageView* icon = infobar_view->icon();
  ASSERT_NE(nullptr, icon);

  EXPECT_EQ(gfx::Size(24, 24), icon->GetPreferredSize());
  widget->CloseNow();
}

TEST_F(InfoBarViewUnitTest, InfobarContainerPadding) {
  auto delegate = std::make_unique<TestInfoBarDelegateWithIcon>();
  auto infobar_view =
      std::make_unique<TestInfoBarViewWithLabelAndIcon>(std::move(delegate));

  auto widget = std::make_unique<views::Widget>();
  views::Widget::InitParams params =
      CreateParams(views::Widget::InitParams::CLIENT_OWNS_WIDGET,
                   views::Widget::InitParams::TYPE_WINDOW_FRAMELESS);
  widget->Init(std::move(params));

  widget->SetContentsView(infobar_view.get());
  widget->SetBounds(gfx::Rect(0, 0, 500, 50));
  widget->Show();
  widget->LayoutRootViewIfNecessary();

  EXPECT_EQ(infobar_view->GetTotalHeight(), infobar_view->target_height());

  widget->CloseNow();
}

TEST_F(InfoBarViewUnitTest, InfobarEliding) {
  auto delegate = std::make_unique<TestInfoBarDelegateWithIcon>();
  auto infobar_view =
      std::make_unique<TestInfoBarViewWithLabelAndIcon>(std::move(delegate));
  views::Label* label = infobar_view->test_label();
  label->SetText(
      u"This is a long label that should be elided when the window is narrow.");

  auto widget = std::make_unique<views::Widget>();
  views::Widget::InitParams params =
      CreateParams(views::Widget::InitParams::CLIENT_OWNS_WIDGET,
                   views::Widget::InitParams::TYPE_WINDOW_FRAMELESS);
  widget->Init(std::move(params));

  widget->SetContentsView(infobar_view.get());
  widget->SetBounds(gfx::Rect(0, 0, 200, 50));
  widget->Show();
  widget->LayoutRootViewIfNecessary();

  // Verify that the label is elided in a narrow window.
  EXPECT_TRUE(label->IsDisplayTextTruncated());

  // Resize the window to be wider and verify that the label is no longer
  // elided.
  widget->SetBounds(gfx::Rect(0, 0, 800, 50));
  widget->LayoutRootViewIfNecessary();
  EXPECT_FALSE(label->IsDisplayTextTruncated());

  widget->CloseNow();
}

TEST_F(InfoBarViewUnitTest, ShowAndHideApplyMotionCurves) {
  base::test::ScopedFeatureList feature_list(features::kInfoBarQueuedCountPill);
  gfx::ScopedAnimationDurationScaleMode normal_duration(
      gfx::ScopedAnimationDurationScaleMode::NORMAL_DURATION);
  auto render_mode = gfx::AnimationTestApi::SetRichAnimationRenderMode(
      gfx::Animation::RichAnimationRenderMode::FORCE_ENABLED);
  auto reduced_motion =
      gfx::AnimationTestApi::SetPrefersReducedMotionForTesting(false);

  auto delegate = std::make_unique<TestInfoBarDelegateWithIcon>();
  auto infobar_view =
      std::make_unique<TestInfoBarViewWithLabelAndIcon>(std::move(delegate));

  auto widget = std::make_unique<views::Widget>();
  views::Widget::InitParams params =
      CreateParams(views::Widget::InitParams::CLIENT_OWNS_WIDGET,
                   views::Widget::InitParams::TYPE_WINDOW_FRAMELESS);
  widget->Init(std::move(params));
  widget->SetContentsView(infobar_view.get());
  widget->SetBounds(gfx::Rect(0, 0, 500, 50));
  widget->Show();

  gfx::SlideAnimation* slide_animation = infobar_view->mutable_animation();
  gfx::AnimationTestApi anim_api(slide_animation);
  const base::TimeTicks start = base::TimeTicks::Now();
  const base::TimeDelta quarter = slide_animation->GetSlideDuration() / 4;
  ASSERT_GT(quarter, base::TimeDelta());

  // Enter animation uses LINEAR_OUT_SLOW_IN (fast entry, decelerating into
  // rest), so at 25% of duration it has covered more than 25% of distance.
  infobar_view->Show(/*animate=*/true);
  anim_api.SetStartTime(start);
  anim_api.Step(start + quarter);
  EXPECT_DOUBLE_EQ(
      gfx::Tween::CalculateValue(gfx::Tween::LINEAR_OUT_SLOW_IN, 0.25),
      slide_animation->GetCurrentValue());
  EXPECT_GT(slide_animation->GetCurrentValue(), 0.25);

  // Exit animation uses FAST_OUT_LINEAR_IN (accelerating from rest out of
  // scene), so at 25% of duration it has covered less than 25% of the exit.
  slide_animation->Reset(1.0);
  infobar_view->Hide(/*animate=*/true);
  anim_api.SetStartTime(start);
  anim_api.Step(start + quarter);
  EXPECT_DOUBLE_EQ(
      1.0 - gfx::Tween::CalculateValue(gfx::Tween::FAST_OUT_LINEAR_IN, 0.25),
      slide_animation->GetCurrentValue());
  EXPECT_GT(slide_animation->GetCurrentValue(), 0.75);

  // Stop before completion so the unowned test InfoBar does not self-delete.
  slide_animation->Stop();
  widget->CloseNow();
}

TEST_F(InfoBarViewUnitTest, ShowAndHideKeepLinearTweenWhenFeatureDisabled) {
  base::test::ScopedFeatureList feature_list;
  feature_list.InitAndDisableFeature(features::kInfoBarQueuedCountPill);
  gfx::ScopedAnimationDurationScaleMode normal_duration(
      gfx::ScopedAnimationDurationScaleMode::NORMAL_DURATION);
  auto render_mode = gfx::AnimationTestApi::SetRichAnimationRenderMode(
      gfx::Animation::RichAnimationRenderMode::FORCE_ENABLED);
  auto reduced_motion =
      gfx::AnimationTestApi::SetPrefersReducedMotionForTesting(false);

  auto delegate = std::make_unique<TestInfoBarDelegateWithIcon>();
  auto infobar_view =
      std::make_unique<TestInfoBarViewWithLabelAndIcon>(std::move(delegate));

  auto widget = std::make_unique<views::Widget>();
  views::Widget::InitParams params =
      CreateParams(views::Widget::InitParams::CLIENT_OWNS_WIDGET,
                   views::Widget::InitParams::TYPE_WINDOW_FRAMELESS);
  widget->Init(std::move(params));
  widget->SetContentsView(infobar_view.get());
  widget->SetBounds(gfx::Rect(0, 0, 500, 50));
  widget->Show();

  gfx::SlideAnimation* slide_animation = infobar_view->mutable_animation();
  gfx::AnimationTestApi anim_api(slide_animation);
  const base::TimeTicks start = base::TimeTicks::Now();
  const base::TimeDelta quarter = slide_animation->GetSlideDuration() / 4;
  ASSERT_GT(quarter, base::TimeDelta());

  infobar_view->Show(/*animate=*/true);
  anim_api.SetStartTime(start);
  anim_api.Step(start + quarter);
  EXPECT_DOUBLE_EQ(0.25, slide_animation->GetCurrentValue());

  slide_animation->Reset(1.0);
  infobar_view->Hide(/*animate=*/true);
  anim_api.SetStartTime(start);
  anim_api.Step(start + quarter);
  EXPECT_DOUBLE_EQ(0.75, slide_animation->GetCurrentValue());

  slide_animation->Stop();
  widget->CloseNow();
}

TEST_F(InfoBarViewUnitTest, AnimatedCloseKeepsIndicatorVisibleDuringExit) {
  base::test::ScopedFeatureList feature_list(features::kInfoBarQueuedCountPill);
  gfx::ScopedAnimationDurationScaleMode normal_duration(
      gfx::ScopedAnimationDurationScaleMode::NORMAL_DURATION);
  auto render_mode = gfx::AnimationTestApi::SetRichAnimationRenderMode(
      gfx::Animation::RichAnimationRenderMode::FORCE_ENABLED);
  auto reduced_motion =
      gfx::AnimationTestApi::SetPrefersReducedMotionForTesting(false);

  FakeInfoBarManager manager;
  manager.set_animations_enabled(false);

  DummyContainerDelegate container_delegate;

  auto widget = std::make_unique<views::Widget>();
  views::Widget::InitParams params =
      CreateParams(views::Widget::InitParams::CLIENT_OWNS_WIDGET,
                   views::Widget::InitParams::TYPE_WINDOW_FRAMELESS);
  widget->Init(std::move(params));
  InfoBarContainerView* container = widget->SetContentsView(
      std::make_unique<InfoBarContainerView>(&container_delegate));
  container->SetVisible(true);
  widget->SetBounds(gfx::Rect(0, 0, 800, 100));
  widget->Show();

  container->ChangeInfoBarManager(&manager);

  // Add `infobar_1` and `infobar_2` without enter animation so `infobar_1`
  // starts settled at full open height (animation value == 1.0).
  ConfirmInfoBar* infobar_1 = manager.AddInfoBar(ConfirmInfoBar::Create(
      ConfirmInfoBarCreationAllowlist::GetPassKeyForTesting(),
      std::make_unique<UniqueConfirmInfoBarDelegate>()));
  ConfirmInfoBar* infobar_2 = manager.AddInfoBar(ConfirmInfoBar::Create(
      ConfirmInfoBarCreationAllowlist::GetPassKeyForTesting(),
      std::make_unique<UniqueConfirmInfoBarDelegate>()));
  ASSERT_DOUBLE_EQ(1.0,
                   std::as_const(*infobar_1).animation().GetCurrentValue());
  widget->LayoutRootViewIfNecessary();

  views::View* indicator_1 =
      infobar_1->GetViewByElementId(InfoBarView::kQueuedIndicatorElementId);
  ASSERT_NE(nullptr, indicator_1);
  ASSERT_TRUE(indicator_1->GetVisible());
  const int initial_button_x =
      infobar_1->ok_button_for_testing()->GetBoundsInScreen().x();

  // Enable animations for dismissal: `infobar_1` stays in the container while
  // sliding closed and keeps its "1 of 2" label so its centered content does
  // not jump horizontally mid-exit, while `infobar_2` simultaneously shows
  // "2 of 2".
  manager.set_animations_enabled(true);
  manager.RemoveInfoBar(infobar_1);
  EXPECT_TRUE(std::as_const(*infobar_1).animation().is_animating());
  widget->LayoutRootViewIfNecessary();

  EXPECT_TRUE(indicator_1->GetVisible());
  EXPECT_EQ(initial_button_x,
            infobar_1->ok_button_for_testing()->GetBoundsInScreen().x());

  views::View* indicator_2 =
      infobar_2->GetViewByElementId(InfoBarView::kQueuedIndicatorElementId);
  ASSERT_NE(nullptr, indicator_2);
  EXPECT_TRUE(indicator_2->GetVisible());

  container->ChangeInfoBarManager(nullptr);
  widget->CloseNow();
}
