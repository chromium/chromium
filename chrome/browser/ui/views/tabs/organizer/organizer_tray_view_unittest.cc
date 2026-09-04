// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/views/tabs/organizer/organizer_tray_view.h"

#include "base/memory/raw_ptr.h"
#include "base/memory/scoped_refptr.h"
#include "base/test/bind.h"
#include "base/test/scoped_run_loop_timeout.h"
#include "chrome/browser/ui/actions/chrome_action_id.h"
#include "chrome/browser/ui/browser_actions.h"
#include "chrome/browser/ui/browser_element_identifiers.h"
#include "chrome/browser/ui/browser_window/public/browser_window_interface.h"
#include "chrome/browser/ui/browser_window/test/mock_browser_window_interface.h"
#include "chrome/browser/ui/tabs/organizer/organizer_panel_state_controller.h"
#include "chrome/browser/ui/views/interaction/browser_elements_views.h"
#include "chrome/browser/ui/views/interaction/browser_elements_views_impl.h"
#include "chrome/browser/ui/views/tabs/organizer/layout_constants.h"
#include "chrome/test/base/testing_profile.h"
#include "chrome/test/views/chrome_views_test_base.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "ui/actions/actions.h"
#include "ui/base/metadata/metadata_header_macros.h"
#include "ui/base/metadata/metadata_impl_macros.h"
#include "ui/compositor/layer.h"
#include "ui/views/interaction/interactive_views_test.h"
#include "ui/views/layout/delegating_layout_manager.h"
#include "ui/views/layout/layout_types.h"
#include "ui/views/layout/proposed_layout.h"
#include "ui/views/view_class_properties.h"
#include "ui/views/widget/widget.h"

namespace {

class FakeBrowserView : public views::View, public views::LayoutDelegate {
  METADATA_HEADER(FakeBrowserView, views::View)
 public:
  explicit FakeBrowserView(BrowserWindowInterface& browser) {
    SetProperty(views::kElementIdentifierKey, kBrowserViewElementId);
    SetLayoutManager(std::make_unique<views::DelegatingLayoutManager>(this));
    tray_view_ = AddChildView(std::make_unique<OrganizerTrayView>(browser));
  }

  ~FakeBrowserView() override = default;

  OrganizerTrayView* tray_view() const { return tray_view_; }

  views::ProposedLayout CalculateProposedLayout(
      const views::SizeBounds& size_bounds) const override {
    views::ProposedLayout layout;
    if (tray_view_ && tray_view_->GetVisible()) {
      const int width = base::ClampCeil(tray_view_->target_width() *
                                        tray_view_->GetAnimationValue());
      layout.child_layouts.push_back({
          .child_view = tray_view_.get(),
          .visible = true,
          .bounds = gfx::Rect(0, 0, width, size_bounds.height().value_or(0)),
      });
    }
    return layout;
  }

 private:
  raw_ptr<OrganizerTrayView> tray_view_ = nullptr;
};

BEGIN_METADATA(FakeBrowserView)
END_METADATA

}  // namespace

class OrganizerTrayViewTest
    : public views::test::InteractiveViewsTestMixin<ChromeViewsTestBase> {
 public:
  OrganizerTrayViewTest() = default;
  ~OrganizerTrayViewTest() override = default;

  void SetUp() override {
    InteractiveViewsTestMixin::SetUp();

    // To avoid cases where no base timeout is set, create an arbitrary
    // 10-second timeout.
    run_loop_timeout_ = std::make_unique<base::test::ScopedRunLoopTimeout>(
        FROM_HERE, base::Seconds(10));

    profile_ = std::make_unique<TestingProfile>();
    EXPECT_CALL(browser_, GetProfile())
        .WillRepeatedly(testing::Return(profile_.get()));

    // Create a root action item for the panel.
    root_action_ = actions::ActionManager::GetForTesting().AddAction(
        actions::ActionItem::Builder()
            .AddChildren(
                actions::ActionItem::Builder()
                    .SetActionId(kActionToggleOrganizerPanel)
                    .SetInvokeActionCallback(base::BindLambdaForTesting(
                        [this](actions::ActionItem*,
                               actions::ActionInvocationContext) {
                          state_controller_->SetOrganizerVisible(
                              !state_controller_->IsOrganizerPanelVisible());
                        })))
            .Build());

    browser_elements_ = std::make_unique<BrowserElementsViewsImpl>(browser_);

    browser_actions_ = std::make_unique<BrowserActions>(&browser_);
    browser_actions_->set_root_action_item_for_testing(root_action_);

    state_controller_ = std::make_unique<OrganizerPanelStateController>(
        &browser_, root_action_);

    widget_ = CreateTestWidget(views::Widget::InitParams::CLIENT_OWNS_WIDGET);
    browser_view_ =
        widget_->SetContentsView(std::make_unique<FakeBrowserView>(browser_));
    browser_elements_->Init(browser_view_);
    auto panel = std::make_unique<views::View>();
    panel_ = panel.get();
    panel_->SetProperty(views::kElementIdentifierKey,
                        kOrganizerPanelViewElementId);
    browser_view_->tray_view()->SetPanelView(std::move(panel));
    widget_->SetBounds(gfx::Rect(0, 0, 800, 600));
    widget_->Show();

    SetContextWidget(widget_.get());
  }

  void TearDown() override {
    SetContextWidget(nullptr);
    static_cast<BrowserElementsViews*>(browser_elements_.get())->TearDown();

    // Release views and widget.
    root_action_ = nullptr;
    browser_view_ = nullptr;
    panel_ = nullptr;
    widget_.reset();

    // Release services.
    state_controller_.reset();
    browser_actions_.reset();
    browser_elements_.reset();

    // Release other resources.
    profile_.reset();
    run_loop_timeout_.reset();

    InteractiveViewsTestMixin::TearDown();
  }

  auto TogglePanel() {
    return Do([this]() {
             actions::ActionManager::Get()
                 .FindAction(kActionToggleOrganizerPanel, root_action_)
                 ->InvokeAction();
           })
        .SetDescription("TogglePanel()");
  }

  auto FastForward(base::TimeDelta amount) {
    return Do([this, amount]() {
             task_environment()->FastForwardBy(amount);
             widget_->LayoutRootViewIfNecessary();
           })
        .SetDescription("FastForward()");
  }

  auto ShowPanel() {
    auto steps = Steps(
        EnsureNotPresent(OrganizerTrayView::kTrayElementId), TogglePanel(),
        InParallel(RunSubsequence(FastForward(
                       OrganizerTrayView::kPanelShowAnimationDuration +
                       base::Seconds(1))),
                   RunSubsequence(
                       WaitForShow(OrganizerTrayView::kTrayElementId),
                       WaitForEvent(OrganizerTrayView::kTrayElementId,
                                    OrganizerTrayView::kOpenAnimationComplete),
                       WaitForShow(kOrganizerPanelViewElementId))));
    AddDescriptionPrefix(steps, "ShowPanel()");
    return steps;
  }

  auto HidePanel() {
    auto steps = Steps(
        TogglePanel(),
        InParallel(RunSubsequence(FastForward(
                       OrganizerTrayView::kPanelHideAnimationDuration +
                       base::Seconds(1))),
                   RunSubsequence(
                       WaitForEvent(OrganizerTrayView::kTrayElementId,
                                    OrganizerTrayView::kCloseAnimationComplete),
                       WaitForHide(OrganizerTrayView::kTrayElementId),
                       WaitForHide(kOrganizerPanelViewElementId))));
    AddDescriptionPrefix(steps, "HidePanel()");
    return steps;
  }

  auto SetAnimationValue(double value) {
    return WithView(OrganizerTrayView::kTrayElementId,
                    [value](OrganizerTrayView* tray) {
                      tray->SetAnimationValueForTesting(value);
                      tray->GetWidget()->LayoutRootViewIfNecessary();
                    })
        .SetDescription("SetAnimationValue()");
  }

  auto SetExclusion(int width, int height) {
    return WithView(OrganizerTrayView::kTrayElementId,
                    [width, height](OrganizerTrayView* tray) {
                      tray->SetTopLeadingExclusion(gfx::Size(width, height));
                      tray->GetWidget()->LayoutRootViewIfNecessary();
                    })
        .SetDescription("SetExclusion()");
  }

  OrganizerTrayView* tray_view() { return browser_view_->tray_view(); }

 private:
  std::unique_ptr<TestingProfile> profile_;
  testing::NiceMock<MockBrowserWindowInterface> browser_;
  std::unique_ptr<BrowserElementsViewsImpl> browser_elements_;
  std::unique_ptr<BrowserActions> browser_actions_;
  std::unique_ptr<OrganizerPanelStateController> state_controller_;
  std::unique_ptr<views::Widget> widget_;
  raw_ptr<FakeBrowserView> browser_view_;
  raw_ptr<views::View> panel_;
  raw_ptr<actions::ActionItem> root_action_;
  std::unique_ptr<base::test::ScopedRunLoopTimeout> run_loop_timeout_;
};

TEST_F(OrganizerTrayViewTest, AnimatesOpen) {
  RunTestSequence(
      ShowPanel(),
      CheckView(
          kOrganizerPanelViewElementId,
          [](views::View* view) { return view->width(); },
          tray_view()->target_width()),
      CheckView(
          kOrganizerPanelViewElementId,
          [](views::View* view) { return view->x(); }, 0),
      CheckView(
          kOrganizerPanelViewElementId,
          [](views::View* view) { return view->y(); }, testing::Gt(0)));
}

TEST_F(OrganizerTrayViewTest, AnimatesClosed) {
  RunTestSequence(ShowPanel(), HidePanel());
}

TEST_F(OrganizerTrayViewTest, PositionsElementsDuringAnimation) {
  RunTestSequence(
      ShowPanel(), SetAnimationValue(0.5),
      CheckView(
          kOrganizerPanelViewElementId,
          [](views::View* view) { return view->width(); },
          tray_view()->target_width()),
      CheckView(
          kOrganizerPanelViewElementId,
          [](views::View* view) { return view->x(); }, testing::Lt(0)),
      CheckView(
          kOrganizerPanelViewElementId,
          [](views::View* view) {
            return view->bounds().right() - view->parent()->width();
          },
          0));
}

TEST_F(OrganizerTrayViewTest, CloseButtonFade) {
  RunTestSequence(
      ShowPanel(),
      SetExclusion(organizer_panel::kOrganizerPanelMinWidth / 2, 0),
      SetAnimationValue(0.25),
      CheckView(
          kOrganizerPanelButtonElementId,
          [](views::View* view) { return view->layer()->opacity(); }, 0.0),
      SetAnimationValue(0.5),
      CheckView(
          kOrganizerPanelButtonElementId,
          [](views::View* view) { return view->layer()->opacity(); }, 0.0),
      SetAnimationValue(0.75),
      CheckView(
          kOrganizerPanelButtonElementId,
          [](views::View* view) {
            // Note: this will be less than 50% because the close button has
            // nonzero size and must fade out before it touches the exclusion
            // area.
            return view->layer()->opacity();
          },
          testing::AllOf(testing::Gt(0.0), testing::Le(0.5))),
      SetAnimationValue(1.0),
      CheckView(
          kOrganizerPanelButtonElementId,
          [](views::View* view) { return view->layer()->opacity(); }, 1.0));
}

TEST_F(OrganizerTrayViewTest, SizeControlsToExclusionHeight) {
  int expected_top = 0;
  RunTestSequence(
      ShowPanel(),
      // Tall exclusion.
      SetExclusion(10, 100),
      CheckView(
          kOrganizerPanelControlsViewElementId,
          [](views::View* view) {
            return view->height() +
                   view->GetProperty(views::kMarginsKey)->height();
          },
          100)
          .SetDescription("Controls area should match exclusion height."),
      CheckView(
          kOrganizerPanelViewElementId,
          [](views::View* view) { return view->y(); }, 100)
          .SetDescription("Controls area should match exclusion height."),
      CheckView(
          kOrganizerPanelButtonElementId,
          [](views::View* view) { return view->y(); }, testing::Gt(0))
          .SetDescription(
              "Button should float down to center in larger header."),
      // Short exclusion.
      SetExclusion(10, 1),
      CheckView(
          kOrganizerPanelControlsViewElementId,
          [&expected_top](views::View* view) {
            expected_top = view->bounds().bottom() +
                           view->GetProperty(views::kMarginsKey)->bottom();
            return view->height() - view->GetPreferredSize().height();
          },
          0)
          .SetDescription("For small exclusion, controls height should match "
                          "preferred height."),
      CheckView(
          kOrganizerPanelButtonElementId,
          [](views::View* view) { return view->y(); }, 0)
          .SetDescription("For small exclusion, button should be top-aligned."),
      CheckView(
          kOrganizerPanelViewElementId,
          [&expected_top](views::View* view) {
            return view->y() - expected_top;
          },
          0)
          .SetDescription("Panel should start beneath controls."));
}
