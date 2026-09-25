// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/views/tabs/organizer/organizer_panel_test.h"

#include <memory>

#include "chrome/browser/ui/actions/chrome_action_id.h"
#include "chrome/browser/ui/browser_element_identifiers.h"
#include "chrome/browser/ui/browser_window/public/browser_window_interface.h"
#include "chrome/browser/ui/views/animations/organizer_panel_animations.h"
#include "chrome/browser/ui/views/tabs/organizer/layout_constants.h"
#include "chrome/browser/ui/views/tabs/organizer/organizer_panel_utils.h"
#include "ui/base/metadata/metadata_impl_macros.h"
#include "ui/views/interaction/interactive_views_test.h"
#include "ui/views/layout/fill_layout.h"
#include "ui/views/view_class_properties.h"

namespace organizer_panel::test {

BEGIN_METADATA(FakeVerticalTabStrip)
END_METADATA

FakeVerticalTabStrip::FakeVerticalTabStrip() {
  SetLayoutManager(std::make_unique<views::FillLayout>());
  SetProperty(views::kElementIdentifierKey, kTabStripRegionElementId);
}

FakeVerticalTabStrip::~FakeVerticalTabStrip() = default;

void FakeVerticalTabStrip::SetAnimationValue(double value) {
  if (panel_view_) {
    panel_view_->SetVisible(value > 0.0);
  }
}

void FakeVerticalTabStrip::SetOrganizerPanelView(
    std::unique_ptr<views::View> panel_view) {
  CHECK(!HasOrganizerPanelView());
  panel_view->SetVisible(false);
  panel_view_ = AddChildView(std::move(panel_view));
}

std::unique_ptr<views::View> FakeVerticalTabStrip::TakeOrganizerPanelView() {
  CHECK(HasOrganizerPanelView());
  return RemoveChildViewT(std::exchange(panel_view_, nullptr));
}

bool FakeVerticalTabStrip::HasOrganizerPanelView() const {
  return panel_view_ != nullptr;
}

BEGIN_METADATA(FakeBrowserView)
END_METADATA

FakeBrowserView::FakeBrowserView(BrowserWindowInterface& browser) {
  SetProperty(views::kElementIdentifierKey, kBrowserViewElementId);
  SetLayoutManager(std::make_unique<views::DelegatingLayoutManager>(this));
  fake_vertical_tab_strip_ =
      AddChildView(std::make_unique<FakeVerticalTabStrip>());
  tray_view_ =
      AddChildView(std::make_unique<OrganizerTrayView>(browser, nullptr));
}
FakeBrowserView::~FakeBrowserView() = default;

void FakeBrowserView::SetAnimationValue(double animation_value) {
  animation_value_ = animation_value;
  if (tray_view_) {
    tray_view_->InvalidateLayout();
  }
  if (fake_vertical_tab_strip_) {
    fake_vertical_tab_strip_->SetAnimationValue(animation_value);
  }
}

views::ProposedLayout FakeBrowserView::CalculateProposedLayout(
    const views::SizeBounds& size_bounds) const {
  views::ProposedLayout layout;
  if (tray_view_) {
    const int width =
        base::ClampCeil(tray_view_->target_width() * animation_value_);
    layout.child_layouts.push_back({
        .child_view = tray_view_.get(),
        .visible =
            animation_value_ > 0.0 &&
            OrganizerPanelHost::FromView(tray_view_)->HasOrganizerPanelView(),
        .bounds = gfx::Rect(0, 0, width, size_bounds.height().value_or(0)),
    });
  }
  if (fake_vertical_tab_strip_) {
    layout.child_layouts.push_back({
        .child_view = fake_vertical_tab_strip_.get(),
        .visible = true,
        .bounds = gfx::Rect(0, 0, 240, size_bounds.height().value_or(0)),
    });
  }
  return layout;
}

OrganizerPanelTestBase::OrganizerPanelTestBase() {
  feature_list_.InitAndEnableFeature(kOrganizerPanel);
}
OrganizerPanelTestBase::~OrganizerPanelTestBase() = default;

void OrganizerPanelTestBase::SetUp() {
  InteractiveViewsTestMixin::SetUp();

  // To avoid cases where no base timeout is set, create an arbitrary
  // 10-second timeout.
  run_loop_timeout_ = std::make_unique<base::test::ScopedRunLoopTimeout>(
      FROM_HERE, base::Seconds(10));

  profile_ = std::make_unique<TestingProfile>();
  EXPECT_CALL(browser_, GetType())
      .WillRepeatedly(testing::Return(BrowserWindowInterface::TYPE_NORMAL));
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

  animation_controller_ =
      std::make_unique<BrowserAnimationController>(browser_);
  animation_controller_->AddAnimationProvider(
      std::make_unique<OrganizerPanelAnimations>());

  vertical_tab_strip_controller_ =
      std::make_unique<tabs::test::MockVerticalTabStripStateController>(
          browser_);
  EXPECT_CALL(*vertical_tab_strip_controller_, ShouldDisplayVerticalTabs)
      .WillRepeatedly(testing::Return(false));
  EXPECT_CALL(*vertical_tab_strip_controller_, IsCollapsed)
      .WillRepeatedly(testing::Return(false));
  EXPECT_CALL(*vertical_tab_strip_controller_, IsExpandOnHoverEnabled)
      .WillRepeatedly(testing::Return(true));
  EXPECT_CALL(*vertical_tab_strip_controller_, GetUncollapsedWidth)
      .WillRepeatedly(
          testing::Return(organizer_panel::kOrganizerPanelMinWidth));

  state_controller_ =
      std::make_unique<OrganizerPanelController>(browser_, root_action_);

  widget_ = CreateTestWidget(views::Widget::InitParams::CLIENT_OWNS_WIDGET);
  browser_view_ =
      widget_->SetContentsView(std::make_unique<FakeBrowserView>(browser_));
  browser_elements_->Init(browser_view_);
  widget_->SetBounds(gfx::Rect(0, 0, 800, 600));
  widget_->Show();

  // Correct approach is to set this through the controller. It does not need
  // to be reset later as it will be torn down with its parent view.
  auto panel = std::make_unique<views::View>();
  panel_ = panel.get();
  panel_->SetProperty(views::kElementIdentifierKey, kOrganizerPanelElementId);
  SetContextWidget(widget_.get());

  BeforeSetPanel();
  state_controller_->SetPanelViewForTesting(std::move(panel));
}

void OrganizerPanelTestBase::TearDown() {
  SetContextWidget(nullptr);
  static_cast<BrowserElementsViews*>(browser_elements_.get())->TearDown();

  // Release views and widget.
  root_action_ = nullptr;
  browser_view_ = nullptr;
  panel_ = nullptr;
  widget_.reset();

  // Release services.
  state_controller_.reset();
  animation_controller_.reset();
  vertical_tab_strip_controller_.reset();
  browser_actions_.reset();
  browser_elements_.reset();

  // Release other resources.
  profile_.reset();
  run_loop_timeout_.reset();

  InteractiveViewsTestMixin::TearDown();
}

views::test::InteractiveViewsTestApi::StepBuilder
OrganizerPanelTestBase::TogglePanel() {
  return Do([this]() {
           actions::ActionManager::Get()
               .FindAction(kActionToggleOrganizerPanel, root_action_)
               ->InvokeAction();
         })
      .SetDescription("TogglePanel()");
}

views::test::InteractiveViewsTestApi::StepBuilder
OrganizerPanelTestBase::SetAnimationValue(double value) {
  return WithView(kBrowserViewElementId,
                  [value](FakeBrowserView* browser_view) {
                    browser_view->SetAnimationValue(value);
                    browser_view->GetWidget()->LayoutRootViewIfNecessary();
                  })
      .SetDescription("SetAnimationValue()");
}

views::test::InteractiveViewsTestApi::MultiStep
OrganizerPanelTestBase::ShowPanel() {
  auto steps = Steps(
      EnsureNotPresent(OrganizerTrayView::kTrayElementId), TogglePanel(),
      InParallel(RunSubsequence(SetAnimationValue(1.0)),
                 RunSubsequence(WaitForShow(OrganizerTrayView::kTrayElementId),
                                WaitForShow(kOrganizerPanelElementId))));
  AddDescriptionPrefix(steps, "ShowPanel()");
  return steps;
}

views::test::InteractiveViewsTestApi::MultiStep
OrganizerPanelTestBase::HidePanel() {
  auto steps = Steps(
      TogglePanel(),
      InParallel(RunSubsequence(SetAnimationValue(0.0)),
                 RunSubsequence(WaitForHide(OrganizerTrayView::kTrayElementId),
                                WaitForHide(kOrganizerPanelElementId))));
  AddDescriptionPrefix(steps, "HidePanel()");
  return steps;
}

views::test::InteractiveViewsTestApi::StepBuilder
OrganizerPanelTestBase::SetExclusion(int width, int height) {
  return WithView(OrganizerTrayView::kTrayElementId,
                  [width, height](OrganizerTrayView* tray) {
                    tray->SetTopLeadingExclusion(gfx::Size(width, height));
                    tray->GetWidget()->LayoutRootViewIfNecessary();
                  })
      .SetDescription("SetExclusion()");
}

}  // namespace organizer_panel::test
