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
  button_->RequestFocus();
  EndTestAndCompareEvents("meta-test-focus-event-recorded");
}

// Tests that the allow filter correctly includes matching events.
IN_PROC_BROWSER_TEST_P(DumpAccessibilityEventsViewsTest,
                       MetaTest_FilterAllowWorks) {
  button_->RequestFocus();
  EndTestAndCompareEvents("meta-test-filter-allow-works");
}

// Tests that the deny filter correctly excludes matching events.
IN_PROC_BROWSER_TEST_P(DumpAccessibilityEventsViewsTest,
                       MetaTest_FilterDenyWorks) {
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
