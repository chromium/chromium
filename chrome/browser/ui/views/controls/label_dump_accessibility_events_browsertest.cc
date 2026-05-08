// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/views/accessibility/dump_accessibility_events_views_browsertest_base.h"
#include "content/public/test/browser_test.h"
#include "ui/views/accessibility/view_accessibility.h"
#include "ui/views/controls/label.h"
#include "ui/views/layout/fill_layout.h"
#include "ui/views/view.h"
#include "ui/views/widget/widget.h"

namespace views {
namespace {

class LabelDumpAccessibilityEventsTest
    : public DumpAccessibilityEventsViewsTestBase {
 public:
  void SetUpTestViews() override {
    auto container = std::make_unique<View>();
    container->SetLayoutManager(std::make_unique<FillLayout>());

    auto label = std::make_unique<Label>(u"Initial Text");
    label_ = container->AddChildView(std::move(label));

    widget()->SetContentsView(std::move(container));
    widget()->Show();
  }

  void TearDownOnMainThread() override {
    label_ = nullptr;
    DumpAccessibilityEventsViewsTestBase::TearDownOnMainThread();
  }

 protected:
  raw_ptr<Label> label_ = nullptr;
};

IN_PROC_BROWSER_TEST_P(LabelDumpAccessibilityEventsTest, SetText) {
  // The IA2 expected output only matches ViewsAX-enabled behavior.
  if (GetApiType() == ui::AXApiType::kWinIA2 && !IsViewsAXEnabled()) {
    GTEST_SKIP() << "Test skipped: IA2 with ViewsAX disabled";
  }
  SetFilters(R"(
@MAC-ALLOW:AXTitleChanged*
@WIN-ALLOW:IA2_EVENT_TEXT_*
@UIA-WIN-ALLOW:Text_TextChanged*
@AURALINUX-ALLOW:NAME-CHANGED*
)");
  BEGIN_RECORDING_EVENTS_OR_SKIP("label-set-text");
  label_->SetText(u"Updated Text");
}

INSTANTIATE_TEST_SUITE_P(
    All,
    LabelDumpAccessibilityEventsTest,
    ::testing::ValuesIn(
        DumpAccessibilityEventsViewsTestBase::EventTestPasses()),
    EventTestPassToString());

}  // namespace
}  // namespace views
