// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/views/accessibility/dump_accessibility_events_views_browsertest_base.h"
#include "content/public/test/browser_test.h"
#include "ui/views/accessibility/view_accessibility.h"
#include "ui/views/controls/button/label_button.h"
#include "ui/views/layout/fill_layout.h"
#include "ui/views/view.h"
#include "ui/views/widget/widget.h"

namespace views {
namespace {

class DumpAccessibilityEventsViewsTest
    : public DumpAccessibilityEventsViewsTestBase {
 public:
  void SetUpTestViews() override {
    // Create a simple container view for framework tests.
    auto container = std::make_unique<View>();
    container->SetLayoutManager(std::make_unique<FillLayout>());

    auto button = std::make_unique<LabelButton>(Button::PressedCallback(),
                                                u"Test Button");
    button->GetViewAccessibility().SetName(u"Test Button");
    button_ = container->AddChildView(std::move(button));

    widget()->SetContentsView(std::move(container));
    widget()->Show();
  }

  void TearDownOnMainThread() override {
    button_ = nullptr;
    DumpAccessibilityEventsViewsTestBase::TearDownOnMainThread();
  }

 protected:
  raw_ptr<LabelButton> button_ = nullptr;
};

// Meta-tests to validate the testing framework itself.

// Tests that focus events are recorded and match the expectation file.
IN_PROC_BROWSER_TEST_P(DumpAccessibilityEventsViewsTest,
                       MetaTest_FocusEventRecorded) {
  SKIP_IF_VIEWS_AX_ENABLED();
  button_->RequestFocus();
  EndTestAndCompareEvents("meta-test-focus-event-recorded");
}

// Tests that the allow filter correctly includes matching events.
IN_PROC_BROWSER_TEST_P(DumpAccessibilityEventsViewsTest,
                       MetaTest_FilterAllowWorks) {
  SKIP_IF_VIEWS_AX_ENABLED();
  // First deny everything, then re-allow only focus-related events.
  // This verifies that ALLOW filters can override a preceding DENY.
  AddDenyFilter("*");
  SetFilters(R"(
@AURALINUX-ALLOW:FOCUS-EVENT*
@MAC-ALLOW:AXFocusedUIElementChanged*
@UIA-WIN-ALLOW:AutomationFocusChanged*
@WIN-ALLOW:EVENT_OBJECT_FOCUS*
)");
  button_->RequestFocus();
  EndTestAndCompareEvents("meta-test-filter-allow-works");
}

// Tests that the deny filter correctly excludes matching events.
IN_PROC_BROWSER_TEST_P(DumpAccessibilityEventsViewsTest,
                       MetaTest_FilterDenyWorks) {
  SKIP_IF_VIEWS_AX_ENABLED();
  // Exclude focus-related events, resulting in no output.
  SetFilters(R"(
@AURALINUX-DENY:FOCUS-EVENT*
@AURALINUX-DENY:STATE-CHANGE*
@MAC-DENY:AXFocusedUIElementChanged*
@UIA-WIN-DENY:AutomationFocusChanged*
@WIN-DENY:EVENT_OBJECT_FOCUS*
)");
  button_->RequestFocus();
  EndTestAndCompareEvents("meta-test-filter-deny-works");
}

INSTANTIATE_TEST_SUITE_P(
    All,
    DumpAccessibilityEventsViewsTest,
    ::testing::ValuesIn(
        DumpAccessibilityEventsViewsTestBase::EventTestPasses()),
    EventTestPassToString());

}  // namespace
}  // namespace views
