// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/user_education/views/help_bubble_view.h"

#include <memory>
#include <vector>

#include "base/bind.h"
#include "base/callback_forward.h"
#include "base/callback_helpers.h"
#include "base/memory/raw_ptr.h"
#include "base/test/mock_callback.h"
#include "components/user_education/common/feature_promo_specification.h"
#include "components/user_education/common/help_bubble_params.h"
#include "components/user_education/views/help_bubble_delegate.h"
#include "components/user_education/views/help_bubble_factory_views.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "ui/base/accelerators/accelerator.h"
#include "ui/base/interaction/element_tracker.h"
#include "ui/base/interaction/expect_call_in_scope.h"
#include "ui/base/interaction/interaction_test_util.h"
#include "ui/base/theme_provider.h"
#include "ui/views/interaction/interaction_test_util_views.h"
#include "ui/views/test/views_test_base.h"
#include "ui/views/widget/widget.h"
#include "ui/views/widget/widget_observer.h"

namespace user_education {

namespace {

// Fake delegate implementation that does not depend on the browser.
class TestDelegate : public HelpBubbleDelegate {
 public:
  TestDelegate() = default;
  ~TestDelegate() override = default;

  std::vector<ui::Accelerator> GetPaneNavigationAccelerators(
      ui::TrackedElement* anchor_element) const override {
    return std::vector<ui::Accelerator>();
  }

  // These methods return text contexts that will be handled by the app's
  // typography system.
  int GetTitleTextContext() const override { return 0; }
  int GetBodyTextContext() const override { return 0; }
  int GetButtonTextContext() const override { return 0; }

  // These methods return color codes that will be handled by the app's theming
  // system.
  ui::ColorId GetHelpBubbleBackgroundColorId() const override { return 0; }
  ui::ColorId GetHelpBubbleForegroundColorId() const override { return 0; }
  ui::ColorId GetHelpBubbleDefaultButtonBackgroundColorId() const override {
    return 0;
  }
  ui::ColorId GetHelpBubbleDefaultButtonForegroundColorId() const override {
    return 0;
  }
  ui::ColorId GetHelpBubbleButtonBorderColorId() const override { return 0; }
  ui::ColorId GetHelpBubbleCloseButtonInkDropColorId() const override {
    return 0;
  }
};

// Fake theme provider. There's a similar TestThemeProvider in chrome/test but
// we're avoiding using chrome-specific code here.
class TestThemeProvider : public ui::ThemeProvider {
 public:
  TestThemeProvider() = default;
  ~TestThemeProvider() override = default;

  gfx::ImageSkia* GetImageSkiaNamed(int id) const override { return nullptr; }
  color_utils::HSL GetTint(int id) const override { return color_utils::HSL(); }
  int GetDisplayProperty(int id) const override { return 0; }
  bool ShouldUseNativeFrame() const override { return false; }
  bool HasCustomImage(int id) const override { return false; }
  base::RefCountedMemory* GetRawData(
      int id,
      ui::ResourceScaleFactor scale_factor) const override {
    return nullptr;
  }
};

// A top-level widget that reports a dummy theme provider.
class TestThemedWidget : public views::Widget {
 public:
  const ui::ThemeProvider* GetThemeProvider() const override {
    return &test_theme_provider_;
  }

 private:
  TestThemeProvider test_theme_provider_;
};

}  // namespace

// Unit tests for HelpBubbleView. Timeout functionality isn't tested here due to
// the vagaries of trying to get simulated timed events to run without a full
// execution environment (specifically, Mac tests were extremely flaky without
// the browser).
//
// Timeouts are tested in:
// chrome/browser/ui/views/user_education/help_bubble_view_timeout_unittest.cc
class HelpBubbleViewTest : public views::ViewsTestBase {
 public:
  HelpBubbleViewTest() = default;
  ~HelpBubbleViewTest() override = default;

  void SetUp() override {
    ViewsTestBase::SetUp();
    widget_ = std::make_unique<TestThemedWidget>();
    widget_->Init(CreateParamsForTestWidget());
    view_ = widget_->SetContentsView(std::make_unique<views::View>());
    widget_->Show();
  }

  void TearDown() override {
    widget_.reset();
    ViewsTestBase::TearDown();
  }

 protected:
  HelpBubbleView* CreateHelpBubbleView(HelpBubbleParams params) {
    return new HelpBubbleView(&test_delegate_, view_, std::move(params));
  }

  HelpBubbleView* CreateHelpBubbleView(base::RepeatingClosure button_callback) {
    HelpBubbleParams params;
    params.body_text = u"To X, do Y";
    params.arrow = HelpBubbleArrow::kTopRight;

    if (button_callback) {
      HelpBubbleButtonParams button_params;
      button_params.text = u"Go away";
      button_params.is_default = true;
      button_params.callback = std::move(button_callback);
      params.buttons.push_back(std::move(button_params));
    }

    return CreateHelpBubbleView(std::move(params));
  }

  TestDelegate test_delegate_;
  base::raw_ptr<views::View> view_;
  std::unique_ptr<views::Widget> widget_;
};

TEST_F(HelpBubbleViewTest, CallButtonCallback_Mouse) {
  UNCALLED_MOCK_CALLBACK(base::RepeatingClosure, mock_callback);

  HelpBubbleView* const bubble = CreateHelpBubbleView(mock_callback.Get());

  // Simulate clicks on dismiss button.
  EXPECT_CALL_IN_SCOPE(
      mock_callback, Run,
      views::test::InteractionTestUtilSimulatorViews::PressButton(
          bubble->GetDefaultButtonForTesting(),
          ui::test::InteractionTestUtil::InputType::kMouse));

  bubble->GetWidget()->Close();
}

TEST_F(HelpBubbleViewTest, CallButtonCallback_Keyboard) {
  UNCALLED_MOCK_CALLBACK(base::RepeatingClosure, mock_callback);

  HelpBubbleView* const bubble = CreateHelpBubbleView(mock_callback.Get());

  // Simulate clicks on dismiss button.
  EXPECT_CALL_IN_SCOPE(
      mock_callback, Run,
      views::test::InteractionTestUtilSimulatorViews::PressButton(
          bubble->GetDefaultButtonForTesting(),
          ui::test::InteractionTestUtil::InputType::kKeyboard));

  bubble->GetWidget()->Close();
}

TEST_F(HelpBubbleViewTest, StableButtonOrder) {
  HelpBubbleParams params;
  params.body_text = u"To X, do Y";
  params.arrow = HelpBubbleArrow::kTopRight;

  constexpr char16_t kButton1Text[] = u"button 1";
  constexpr char16_t kButton2Text[] = u"button 2";
  constexpr char16_t kButton3Text[] = u"button 3";

  HelpBubbleButtonParams button1;
  button1.text = kButton1Text;
  button1.is_default = false;
  params.buttons.push_back(std::move(button1));

  HelpBubbleButtonParams button2;
  button2.text = kButton2Text;
  button2.is_default = true;
  params.buttons.push_back(std::move(button2));

  HelpBubbleButtonParams button3;
  button3.text = kButton3Text;
  button3.is_default = false;
  params.buttons.push_back(std::move(button3));

  auto* bubble = new HelpBubbleView(&test_delegate_, view_, std::move(params));
  EXPECT_EQ(kButton1Text, bubble->GetNonDefaultButtonForTesting(0)->GetText());
  EXPECT_EQ(kButton2Text, bubble->GetDefaultButtonForTesting()->GetText());
  EXPECT_EQ(kButton3Text, bubble->GetNonDefaultButtonForTesting(1)->GetText());
}

}  // namespace user_education
