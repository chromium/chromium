// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/views/accessibility/dump_accessibility_events_views_browsertest_base.h"
#include "content/public/test/browser_test.h"
#include "ui/views/accessibility/view_accessibility.h"
#include "ui/views/controls/slider.h"
#include "ui/views/layout/fill_layout.h"
#include "ui/views/view.h"
#include "ui/views/widget/widget.h"

namespace views {
namespace {

class SliderDumpAccessibilityEventsTest
    : public DumpAccessibilityEventsViewsTestBase {
 public:
  void SetUpTestViews() override {
    auto container = std::make_unique<View>();
    container->SetLayoutManager(std::make_unique<FillLayout>());

    auto slider = std::make_unique<Slider>();
    slider->GetViewAccessibility().SetName(u"Test Slider");
    slider_ = container->AddChildView(std::move(slider));

    widget()->SetContentsView(std::move(container));
    widget()->Show();
  }

  void TearDownOnMainThread() override {
    slider_ = nullptr;
    DumpAccessibilityEventsViewsTestBase::TearDownOnMainThread();
  }

 protected:
  raw_ptr<Slider> slider_ = nullptr;
};

IN_PROC_BROWSER_TEST_P(SliderDumpAccessibilityEventsTest, ValueChanged) {
  SetFilters(R"(
@WIN-ALLOW:EVENT_OBJECT_VALUECHANGE*
@UIA-WIN-ALLOW:RangeValueValue*
@MAC-ALLOW:AXValueChanged*
@AURALINUX-ALLOW:VALUE-CHANGED*
)");
  BEGIN_RECORDING_EVENTS_OR_SKIP("slider-value-changed");
  slider_->SetValue(0.5f);
}

INSTANTIATE_TEST_SUITE_P(
    All,
    SliderDumpAccessibilityEventsTest,
    ::testing::ValuesIn(
        DumpAccessibilityEventsViewsTestBase::EventTestPasses()),
    EventTestPassToString());

}  // namespace
}  // namespace views
