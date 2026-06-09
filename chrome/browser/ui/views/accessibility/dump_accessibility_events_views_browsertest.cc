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
// TODO(crbug.com/521439532): Re-enable the test on Win.
#if BUILDFLAG(IS_WIN)
#define MAYBE_MetaTest_FocusEventRecorded DISABLED_MetaTest_FocusEventRecorded
#else
#define MAYBE_MetaTest_FocusEventRecorded MetaTest_FocusEventRecorded
#endif
IN_PROC_BROWSER_TEST_P(DumpAccessibilityEventsViewsTest,
                       MAYBE_MetaTest_FocusEventRecorded) {
  SKIP_IF_VIEWS_AX_ENABLED();
  SetFilters(R"(
@WIN-ALLOW:EVENT_OBJECT_FOCUS*
@UIA-WIN-ALLOW:AutomationFocusChanged*
@MAC-ALLOW:AXFocusedUIElementChanged*
@AURALINUX-ALLOW:FOCUS-EVENT:TRUE*
@AURALINUX-ALLOW:STATE-CHANGE:FOCUSED:TRUE*
)");
  BEGIN_RECORDING_EVENTS_OR_SKIP("meta-test-focus-event-recorded");
  button_->RequestFocus();
}

// Tests that the allow filter correctly includes matching events.
IN_PROC_BROWSER_TEST_P(DumpAccessibilityEventsViewsTest,
                       MetaTest_FilterAllowWorks) {
  SKIP_IF_VIEWS_AX_ENABLED();
  // Views event tests deny events by default; this verifies ALLOW filters opt
  // matching events back in.
  SetFilters(R"(
@AURALINUX-ALLOW:FOCUS-EVENT*
@AURALINUX-DENY:FOCUS-EVENT:FALSE*
@AURALINUX-DENY:STATE-CHANGE:FOCUSED:FALSE*
@MAC-ALLOW:AXFocusedUIElementChanged*
@UIA-WIN-ALLOW:AutomationFocusChanged*
@WIN-ALLOW:EVENT_OBJECT_FOCUS*
)");
  BEGIN_RECORDING_EVENTS_OR_SKIP("meta-test-filter-allow-works");
  button_->RequestFocus();
}

// Tests that the deny filter correctly excludes matching events.
IN_PROC_BROWSER_TEST_P(DumpAccessibilityEventsViewsTest,
                       MetaTest_FilterDenyWorks) {
  SKIP_IF_VIEWS_AX_ENABLED();
  // Allow, then exclude focus-related events, resulting in no output.
  SetFilters(R"(
@AURALINUX-ALLOW:FOCUS-EVENT*
@AURALINUX-DENY:FOCUS-EVENT*
@AURALINUX-ALLOW:STATE-CHANGE*
@AURALINUX-DENY:STATE-CHANGE*
@MAC-ALLOW:AXFocusedUIElementChanged*
@MAC-DENY:AXFocusedUIElementChanged*
@UIA-WIN-ALLOW:AutomationFocusChanged*
@UIA-WIN-DENY:AutomationFocusChanged*
@WIN-ALLOW:EVENT_OBJECT_FOCUS*
@WIN-DENY:EVENT_OBJECT_FOCUS*
)");
  BEGIN_RECORDING_EVENTS_OR_SKIP("meta-test-filter-deny-works");
  button_->RequestFocus();
}

// Tests that a non-existent expectation file causes the test to be skipped
// on all platforms via BEGIN_RECORDING_EVENTS_OR_SKIP.
IN_PROC_BROWSER_TEST_P(DumpAccessibilityEventsViewsTest,
                       MetaTest_SkipsWhenNoExpectationFile) {
  auto session =
      BeginRecordingEvents("nonexistent-expectation-file-that-does-not-exist");
  EXPECT_FALSE(session)
      << "Session should be invalid when no expectation file exists";
}

INSTANTIATE_TEST_SUITE_P(
    All,
    DumpAccessibilityEventsViewsTest,
    ::testing::ValuesIn(
        DumpAccessibilityEventsViewsTestBase::EventTestPasses()),
    EventTestPassToString());

}  // namespace
}  // namespace views
