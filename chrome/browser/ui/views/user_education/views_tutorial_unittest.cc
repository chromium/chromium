// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/functional/bind.h"
#include "base/test/bind.h"
#include "chrome/grit/generated_resources.h"
#include "components/user_education/common/help_bubble_factory_registry.h"
#include "components/user_education/common/help_bubble_params.h"
#include "components/user_education/common/tutorial_description.h"
#include "components/user_education/common/tutorial_identifier.h"
#include "components/user_education/common/tutorial_registry.h"
#include "components/user_education/common/tutorial_service.h"
#include "components/user_education/views/help_bubble_factory_views.h"
#include "components/user_education/views/help_bubble_view.h"
#include "components/user_education/views/help_bubble_views_test_util.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "ui/base/interaction/element_identifier.h"
#include "ui/base/interaction/element_tracker.h"
#include "ui/base/interaction/expect_call_in_scope.h"
#include "ui/base/interaction/interaction_sequence.h"
#include "ui/views/controls/button/label_button.h"
#include "ui/views/controls/label.h"
#include "ui/views/interaction/element_tracker_views.h"
#include "ui/views/interaction/interaction_test_util_views.h"
#include "ui/views/interaction/interactive_views_test.h"
#include "ui/views/layout/flex_layout_view.h"
#include "ui/views/metadata/view_factory.h"
#include "ui/views/test/views_test_base.h"
#include "ui/views/test/views_test_utils.h"
#include "ui/views/view_class_properties.h"

namespace {

DEFINE_LOCAL_ELEMENT_IDENTIFIER_VALUE(kButtonElementId);
DEFINE_LOCAL_ELEMENT_IDENTIFIER_VALUE(kIndicatorElementId);

class TestTutorialService : public user_education::TutorialService {
 public:
  using TutorialService::TutorialService;
  ~TestTutorialService() override = default;

 protected:
  std::u16string GetBodyIconAltText(bool is_last_step) const override {
    return u"Alt Text";
  }
};

}  // namespace

class ViewsTutorialTest : public views::test::InteractiveViewsTest {
 public:
  ViewsTutorialTest() {
    help_bubble_registry_.MaybeRegister<user_education::HelpBubbleFactoryViews>(
        &test_help_bubble_delegate_);
  }

  ~ViewsTutorialTest() override = default;

  void SetUp() override {
    InteractiveViewsTestT::SetUp();
    widget_ = std::make_unique<views::Widget>();

    views::Widget::InitParams params =
        CreateParams(views::Widget::InitParams::WIDGET_OWNS_NATIVE_WIDGET,
                     views::Widget::InitParams::TYPE_WINDOW_FRAMELESS);
    widget_->Init(std::move(params));
    widget_->SetContentsView(
        views::Builder<views::FlexLayoutView>()
            .SetOrientation(views::LayoutOrientation::kHorizontal)
            .AddChildren(
                views::Builder<views::LabelButton>(
                    std::make_unique<views::LabelButton>(
                        base::BindRepeating(&ViewsTutorialTest::OnButtonPressed,
                                            base::Unretained(this)),
                        u"Button"))
                    .SetProperty(views::kElementIdentifierKey, kButtonElementId)
                    .CopyAddressTo(&button_),
                views::Builder<views::Label>(
                    std::make_unique<views::Label>(u"Indicator Label"))
                    .SetVisible(false)
                    .SetProperty(views::kElementIdentifierKey,
                                 kIndicatorElementId)
                    .CopyAddressTo(&indicator_))
            .Build());

    widget_->Show();
    SetContextWidget(widget_.get());
  }

  void TearDown() override {
    widget_.reset();
    button_ = nullptr;
    indicator_ = nullptr;
    ViewsTestBase::TearDown();
  }

 protected:
  user_education::test::TestHelpBubbleDelegate test_help_bubble_delegate_;
  user_education::HelpBubbleFactoryRegistry help_bubble_registry_;
  user_education::TutorialRegistry tutorial_registry_;
  TestTutorialService tutorial_service_{&tutorial_registry_,
                                        &help_bubble_registry_};

  std::unique_ptr<views::Widget> widget_;
  raw_ptr<views::LabelButton, DanglingUntriaged> button_ = nullptr;
  raw_ptr<views::Label, DanglingUntriaged> indicator_ = nullptr;
  bool hide_button_on_press_ = false;

 private:
  void OnButtonPressed(const ui::Event& event) {
    CHECK(button_);
    CHECK(indicator_);
    if (hide_button_on_press_) {
      button_->SetVisible(false);
    }
    indicator_->SetVisible(true);
  }
};

// Verifies that having a bubble disappear in the middle of a tutorial due to
// its anchor view going away does not break the tutorial; the tutorial should
// be constructed in such a way that either the *sequence* requires the anchor
// stay visible or that when it does disappear the tutorial has already moved on
// to the next step.
TEST_F(ViewsTutorialTest, BubbleDismissOnViewHiddenDoesNotEndTutorial) {
  UNCALLED_MOCK_CALLBACK(user_education::TutorialService::CompletedCallback,
                         completed);
  UNCALLED_MOCK_CALLBACK(user_education::TutorialService::AbortedCallback,
                         aborted);
  UNCALLED_MOCK_CALLBACK(user_education::TutorialService::RestartedCallback,
                         restarted);

  static const user_education::TutorialIdentifier kTutorialId = "Tutorial";
  constexpr user_education::HelpBubbleArrow kArrow =
      user_education::HelpBubbleArrow::kTopLeft;
  user_education::TutorialDescription desc;
  desc.steps.emplace_back(
      user_education::TutorialDescription::BubbleStep(kButtonElementId)
          .SetBubbleBodyText(IDS_TUTORIAL_TAB_GROUP_ADD_TAB_TO_GROUP)
          .SetBubbleArrow(kArrow));
  desc.steps.emplace_back(
      user_education::TutorialDescription::HiddenStep::WaitForActivated(
          kButtonElementId));
  desc.steps.emplace_back(
      user_education::TutorialDescription::BubbleStep(kIndicatorElementId)
          .SetBubbleBodyText(IDS_TUTORIAL_TAB_GROUP_ADD_TAB_TO_GROUP)
          .SetBubbleArrow(kArrow));
  tutorial_registry_.AddTutorial(kTutorialId, std::move(desc));

  hide_button_on_press_ = true;

  EXPECT_CALL(completed, Run);

  RunTestSequence(
      Do([&]() {
        tutorial_service_.StartTutorial(
            kTutorialId,
            views::ElementTrackerViews::GetContextForWidget(widget_.get()),
            completed.Get(), aborted.Get(), restarted.Get());
      }),
      Check([this]() { return tutorial_service_.IsRunningTutorial(); },
            "Ensure tutorial is running."),
      WaitForShow(
          user_education::HelpBubbleView::kHelpBubbleElementIdForTesting),
      PressButton(kButtonElementId),
      WaitForHide(
          user_education::HelpBubbleView::kHelpBubbleElementIdForTesting),
      WaitForShow(
          user_education::HelpBubbleView::kHelpBubbleElementIdForTesting),
      PressButton(user_education::HelpBubbleView::kDefaultButtonIdForTesting));
}

// Verifies that the final bubble of a tutorial disappearing due to its anchor
// going away *does* in fact complete the tutorial. This could be a common
// pattern in cases where the bubble is attached to a dialog (which could be
// closed) or in a WebUI (which could have its tab closed or its page
// navigated).
TEST_F(ViewsTutorialTest, FinalBubbleDismissOnViewHiddenDoesEndTutorial) {
  UNCALLED_MOCK_CALLBACK(user_education::TutorialService::CompletedCallback,
                         completed);
  UNCALLED_MOCK_CALLBACK(user_education::TutorialService::AbortedCallback,
                         aborted);
  UNCALLED_MOCK_CALLBACK(user_education::TutorialService::RestartedCallback,
                         restarted);

  static const user_education::TutorialIdentifier kTutorialId = "Tutorial";
  constexpr user_education::HelpBubbleArrow kArrow =
      user_education::HelpBubbleArrow::kTopLeft;
  user_education::TutorialDescription desc;
  desc.steps.emplace_back(
      user_education::TutorialDescription::BubbleStep(kButtonElementId)
          .SetBubbleBodyText(IDS_TUTORIAL_TAB_GROUP_ADD_TAB_TO_GROUP)
          .SetBubbleArrow(kArrow));
  desc.steps.emplace_back(
      user_education::TutorialDescription::HiddenStep::WaitForActivated(
          kButtonElementId));
  desc.steps.emplace_back(
      user_education::TutorialDescription::BubbleStep(kIndicatorElementId)
          .SetBubbleBodyText(IDS_TUTORIAL_TAB_GROUP_ADD_TAB_TO_GROUP)
          .SetBubbleArrow(kArrow));
  tutorial_registry_.AddTutorial(kTutorialId, std::move(desc));

  EXPECT_CALL(completed, Run);

  RunTestSequence(
      Do([&]() {
        tutorial_service_.StartTutorial(
            kTutorialId,
            views::ElementTrackerViews::GetContextForWidget(widget_.get()),
            completed.Get(), aborted.Get(), restarted.Get());
      }),
      Check([&]() { return tutorial_service_.IsRunningTutorial(); },
            "Ensure tutorial is running."),
      PressButton(kButtonElementId),
      WaitForHide(
          user_education::HelpBubbleView::kHelpBubbleElementIdForTesting),
      WaitForShow(
          user_education::HelpBubbleView::kHelpBubbleElementIdForTesting),
      WithView(kIndicatorElementId, [](views::Label* indicator) {
        indicator->SetVisible(false);
      }).SetMustRemainVisible(false));
}
