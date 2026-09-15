// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/views/accessibility/dump_accessibility_events_views_browsertest_base.h"
#include "content/public/test/browser_test.h"
#include "ui/events/test/event_generator.h"
#include "ui/views/accessibility/view_accessibility.h"
#include "ui/views/controls/button/label_button.h"
#include "ui/views/layout/fill_layout.h"
#include "ui/views/view.h"
#include "ui/views/widget/widget.h"
#include "ui/views/widget/widget_utils.h"

namespace views {
namespace {

class ButtonDumpAccessibilityEventsTest
    : public DumpAccessibilityEventsViewsTestBase {
 public:
  void SetUpTestViews() override {
    // Create a container view with a button.
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
    // Clear the view pointer before the base class destroys the widget,
    // since button_ points to a view owned by the widget.
    button_ = nullptr;
    DumpAccessibilityEventsViewsTestBase::TearDownOnMainThread();
  }

 protected:
  raw_ptr<LabelButton> button_ = nullptr;
};

IN_PROC_BROWSER_TEST_P(ButtonDumpAccessibilityEventsTest, ButtonClick) {
  SKIP_IF_VIEWS_AX_ENABLED();
  SetFilters(R"(
@WIN-ALLOW:EVENT_OBJECT_STATECHANGE*
@UIA-WIN-ALLOW:ToggleToggleState*
@MAC-ALLOW:AXValueChanged*
@AURALINUX-ALLOW:STATE-CHANGE:CHECKED*
)");
  BEGIN_RECORDING_EVENTS_OR_SKIP("button-click");
  ui::test::EventGenerator generator(GetRootWindow(widget()));
  generator.MoveMouseTo(button_->GetBoundsInScreen().CenterPoint());
  generator.ClickLeftButton();
}

IN_PROC_BROWSER_TEST_P(ButtonDumpAccessibilityEventsTest, ButtonFocus) {
  SetFilters(R"(
@WIN-ALLOW:EVENT_OBJECT_FOCUS*
@UIA-WIN-ALLOW:AutomationFocusChanged*
@MAC-ALLOW:AXFocusedUIElementChanged*
@AURALINUX-ALLOW:FOCUS-EVENT:TRUE*
@AURALINUX-ALLOW:STATE-CHANGE:FOCUSED:TRUE*
)");

  BEGIN_RECORDING_EVENTS_OR_SKIP("button-focus");
  button_->RequestFocus();
}

IN_PROC_BROWSER_TEST_P(ButtonDumpAccessibilityEventsTest, EnabledStateChanged) {
  SetFilters(R"(
@WIN-ALLOW:EVENT_OBJECT_STATECHANGE*
@UIA-WIN-ALLOW:IsEnabled*
@AURALINUX-ALLOW:STATE-CHANGE:ENABLED*
@AURALINUX-ALLOW:STATE-CHANGE:READ-ONLY*
@AURALINUX-ALLOW:STATE-CHANGE:SENSITIVE*
)");
  BEGIN_RECORDING_EVENTS_OR_SKIP("button-enabled-state-changed");
  button_->SetEnabled(false);
  WaitForPendingSerialization();
  button_->SetEnabled(true);
}

INSTANTIATE_TEST_SUITE_P(
    All,
    ButtonDumpAccessibilityEventsTest,
    ::testing::ValuesIn(
        DumpAccessibilityEventsViewsTestBase::EventTestPasses()),
    EventTestPassToString());

}  // namespace
}  // namespace views
