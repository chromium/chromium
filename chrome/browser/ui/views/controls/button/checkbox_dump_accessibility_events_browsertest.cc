// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "build/build_config.h"
#include "chrome/browser/ui/views/accessibility/dump_accessibility_events_views_browsertest_base.h"
#include "content/public/test/browser_test.h"
#include "ui/views/accessibility/view_accessibility.h"
#include "ui/views/controls/button/checkbox.h"
#include "ui/views/layout/fill_layout.h"
#include "ui/views/view.h"
#include "ui/views/widget/widget.h"

namespace views {
namespace {

class CheckboxDumpAccessibilityEventsTest
    : public DumpAccessibilityEventsViewsTestBase {
 public:
  std::vector<ui::AXPropertyFilter> DefaultFilters() const override {
    std::vector<ui::AXPropertyFilter> filters;

#if BUILDFLAG(IS_WIN)
    filters.emplace_back("EVENT_OBJECT_STATECHANGE*",
                         ui::AXPropertyFilter::ALLOW);
    filters.emplace_back("ToggleToggleState*", ui::AXPropertyFilter::ALLOW);
    filters.emplace_back("AriaProperties*", ui::AXPropertyFilter::DENY);
#elif BUILDFLAG(IS_MAC)
    filters.emplace_back("AXValueChanged*", ui::AXPropertyFilter::ALLOW);
#elif BUILDFLAG(IS_LINUX)
    filters.emplace_back("STATE-CHANGE:CHECKED*", ui::AXPropertyFilter::ALLOW);
#endif

    return filters;
  }

  void SetUpTestViews() override {
    auto container = std::make_unique<View>();
    container->SetLayoutManager(std::make_unique<FillLayout>());

    auto checkbox = std::make_unique<Checkbox>(u"Test Checkbox");
    checkbox->GetViewAccessibility().SetName(u"Test Checkbox");
    checkbox_ = container->AddChildView(std::move(checkbox));

    widget()->SetContentsView(std::move(container));
    widget()->Show();
  }

  void TearDownOnMainThread() override {
    checkbox_ = nullptr;
    DumpAccessibilityEventsViewsTestBase::TearDownOnMainThread();
  }

 protected:
  raw_ptr<Checkbox> checkbox_ = nullptr;
};

IN_PROC_BROWSER_TEST_P(CheckboxDumpAccessibilityEventsTest,
                       CheckboxCheckedStateChanged) {
  BEGIN_RECORDING_EVENTS_OR_SKIP("checkbox-checked-state-changed");
  checkbox_->SetChecked(true);
}

INSTANTIATE_TEST_SUITE_P(
    All,
    CheckboxDumpAccessibilityEventsTest,
    ::testing::ValuesIn(
        DumpAccessibilityEventsViewsTestBase::EventTestPasses()),
    EventTestPassToString());

}  // namespace
}  // namespace views
