// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/test/run_until.h"
#include "chrome/browser/ui/views/accessibility/dump_accessibility_events_views_browsertest_base.h"
#include "content/public/test/browser_test.h"
#include "ui/accessibility/accessibility_features.h"
#include "ui/aura/client/cursor_client.h"
#include "ui/events/test/event_generator.h"
#include "ui/views/corewm/tooltip_controller.h"
#include "ui/views/corewm/tooltip_controller_test_helper.h"
#include "ui/views/view.h"
#include "ui/views/widget/widget.h"
#include "ui/wm/core/cursor_manager.h"

#if BUILDFLAG(IS_WIN)
#include "ui/accessibility/platform/inspect/ax_event_recorder_win.h"
#include "ui/accessibility/platform/inspect/ax_event_recorder_win_uia.h"
#include "ui/aura/window_tree_host.h"
#endif

namespace views {
namespace {

class TooltipDumpAccessibilityEventsTest
    : public DumpAccessibilityEventsViewsTestBase {
 public:
  // The tooltip's transient HWND has no registered UIA listeners (the recorder
  // subscribes on the desktop before it exists), so disable the optimization
  // that skips events when no listeners are present.
  void ChooseFeatures(
      std::vector<base::test::FeatureRef>* enabled_features,
      std::vector<base::test::FeatureRef>* disabled_features) override {
    disabled_features->emplace_back(::features::kUiaEventOptimization);
  }

  void SetUpTestViews() override {
    auto container = std::make_unique<View>();
    auto tooltip_view = std::make_unique<corewm::test::TooltipTestView>();
    tooltip_view->set_tooltip_text(u"Test tooltip text");
    tooltip_view->SetBoundsRect(gfx::Rect(0, 0, 200, 100));
    tooltip_target_ = container->AddChildView(std::move(tooltip_view));

    widget()->SetContentsView(std::move(container));
    widget()->Show();

    // Create the EventGenerator but NOT the TooltipControllerTestHelper yet.
    // The helper enables skip_show_delay which must not be active during the
    // UIA recorder's initialization RunLoop (it pumps the message loop).
    aura::Window* root_window = widget()->GetNativeWindow()->GetRootWindow();
    event_generator_ = std::make_unique<ui::test::EventGenerator>(root_window);

    EnsureCursorVisible();

    // Park the cursor away from the tooltip view so the UIA recorder's
    // initialization RunLoop doesn't accidentally trigger a tooltip.
    gfx::Point safe_point =
        widget()->GetWindowBoundsInScreen().bottom_right();
    safe_point.Offset(-10, -10);
    event_generator_->MoveMouseTo(safe_point);
  }

  void TearDownOnMainThread() override {
    tooltip_helper_.reset();
    event_generator_.reset();
    tooltip_target_ = nullptr;
    DumpAccessibilityEventsViewsTestBase::TearDownOnMainThread();
  }

  // Tooltip creates a separate HWND, so UIA TreeScope_Subtree on the test
  // widget won't see its events. Scope UIA to the desktop instead.
  std::unique_ptr<ui::AXEventRecorder> CreateEventRecorder() override {
#if BUILDFLAG(IS_WIN)
    ui::AXTreeSelector selector;
    switch (GetApiType()) {
      case ui::AXApiType::kWinIA2:
        selector.widget = reinterpret_cast<gfx::AcceleratedWidget>(
            widget()->GetNativeWindow()->GetHost()->GetAcceleratedWidget());
        return std::make_unique<ui::AXEventRecorderWin>(
            base::GetCurrentProcId(), selector, ui::AXEventRecorderWin::kSync);
      case ui::AXApiType::kWinUIA:
        selector.widget =
            reinterpret_cast<gfx::AcceleratedWidget>(GetDesktopWindow());
        return std::make_unique<ui::AXEventRecorderWinUia>(selector);
      default:
        return nullptr;
    }
#else
    return DumpAccessibilityEventsViewsTestBase::CreateEventRecorder();
#endif
  }

  // Desktop-scoped UIA receives events from all windows. Only allow
  // tooltip-specific events.
  std::vector<ui::AXPropertyFilter> DefaultFilters() const override {
    std::vector<ui::AXPropertyFilter> filters;
#if BUILDFLAG(IS_WIN)
    filters.emplace_back("*ROLE_SYSTEM_TOOLTIP*", ui::AXPropertyFilter::ALLOW);
    filters.emplace_back("ToolTipOpened*", ui::AXPropertyFilter::ALLOW);
    filters.emplace_back("ToolTipClosed*", ui::AXPropertyFilter::ALLOW);
#endif
    return filters;
  }

 protected:
  void ShowTooltip() {
    EnsureTooltipHelper();

    // Move outside first, then onto the target to generate a real
    // ET_MOUSE_MOVED. Avoid (0,0) — TooltipController's same-location
    // filter skips moves matching last_mouse_loc_ which defaults to (0,0).
    gfx::Point outside = widget()->GetWindowBoundsInScreen().bottom_right();
    outside.Offset(-10, -10);
    event_generator_->MoveMouseTo(outside);

    gfx::Point center = tooltip_target_->GetBoundsInScreen().CenterPoint();
    event_generator_->MoveMouseTo(center);
    tooltip_helper_->UpdateIfRequired(corewm::TooltipTrigger::kCursor);

    ASSERT_TRUE(base::test::RunUntil(
        [&]() { return tooltip_helper_->IsTooltipVisible(); }));
  }

  void HideTooltip() {
    gfx::Point outside = widget()->GetWindowBoundsInScreen().bottom_right();
    outside.Offset(-10, -10);
    event_generator_->MoveMouseTo(outside);
    ASSERT_TRUE(base::test::RunUntil(
        [&]() { return !tooltip_helper_->IsTooltipVisible(); }));
  }

 private:
  // Must be called after BEGIN_RECORDING_EVENTS_OR_SKIP so that
  // skip_show_delay is not active during the UIA recorder's init RunLoop.
  void EnsureTooltipHelper() {
    if (tooltip_helper_) {
      return;
    }
    aura::Window* root_window = widget()->GetNativeWindow()->GetRootWindow();
    tooltip_helper_ =
        std::make_unique<corewm::test::TooltipControllerTestHelper>(
            root_window);
    EnsureCursorVisible();
    tooltip_helper_->HideAndReset();
  }

  void EnsureCursorVisible() {
    wm::CursorManager::ResetCursorVisibilityStateForTest();
    aura::Window* root_window = widget()->GetNativeWindow()->GetRootWindow();
    aura::client::CursorClient* cursor_client =
        aura::client::GetCursorClient(root_window);
    if (cursor_client && !cursor_client->IsCursorVisible()) {
      cursor_client->ShowCursor();
    }
  }

  raw_ptr<View> tooltip_target_ = nullptr;
  std::unique_ptr<ui::test::EventGenerator> event_generator_;
  std::unique_ptr<corewm::test::TooltipControllerTestHelper> tooltip_helper_;
};

IN_PROC_BROWSER_TEST_P(TooltipDumpAccessibilityEventsTest,
                       DISABLED_TooltipOpened) {
  AddDenyFilter("EVENT_OBJECT_HIDE*");
  AddDenyFilter("ToolTipClosed*");

  BEGIN_RECORDING_EVENTS_OR_SKIP("tooltip-opened");
  ShowTooltip();

#if BUILDFLAG(IS_WIN)
  // UIA delivers tooltip events from a different HWND than TestComplete,
  // so wait for the event before stopping to avoid a race.
  if (GetApiType() == ui::AXApiType::kWinUIA) {
    ASSERT_TRUE(WaitForCapturedEvent("ToolTipOpened"));
  }
#endif
}

IN_PROC_BROWSER_TEST_P(TooltipDumpAccessibilityEventsTest,
                       DISABLED_TooltipClosed) {
  AddDenyFilter("EVENT_OBJECT_SHOW*");
  AddDenyFilter("ToolTipOpened*");

  BEGIN_RECORDING_EVENTS_OR_SKIP("tooltip-closed");

  ShowTooltip();
  HideTooltip();

#if BUILDFLAG(IS_WIN)
  // UIA delivers tooltip events from a different HWND than TestComplete,
  // so wait for the event before stopping to avoid a race.
  if (GetApiType() == ui::AXApiType::kWinUIA) {
    ASSERT_TRUE(WaitForCapturedEvent("ToolTipClosed"));
  }
#endif
}

INSTANTIATE_TEST_SUITE_P(
    All,
    TooltipDumpAccessibilityEventsTest,
    ::testing::ValuesIn(
        DumpAccessibilityEventsViewsTestBase::EventTestPasses()),
    EventTestPassToString());

}  // namespace
}  // namespace views
