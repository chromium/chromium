// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/test/run_until.h"
#include "chrome/browser/ui/views/accessibility/dump_accessibility_events_views_browsertest_base.h"
#include "content/public/test/browser_test.h"
#include "ui/gfx/range/range.h"
#include "ui/views/accessibility/view_accessibility.h"
#include "ui/views/controls/textfield/textfield.h"
#include "ui/views/layout/fill_layout.h"
#include "ui/views/view.h"
#include "ui/views/widget/widget.h"

namespace views {
namespace {

class TextfieldDumpAccessibilityEventsTest
    : public DumpAccessibilityEventsViewsTestBase {
 public:
  void SetUpTestViews() override {
    auto container = std::make_unique<View>();
    container->SetLayoutManager(std::make_unique<FillLayout>());

    auto textfield = std::make_unique<Textfield>();
    textfield->GetViewAccessibility().SetName(u"Test Textfield");
    textfield_ = container->AddChildView(std::move(textfield));

    widget()->SetContentsView(std::move(container));
    widget()->Show();
  }

  void TearDownOnMainThread() override {
    textfield_ = nullptr;
    DumpAccessibilityEventsViewsTestBase::TearDownOnMainThread();
  }

 protected:
  raw_ptr<Textfield> textfield_ = nullptr;
};

IN_PROC_BROWSER_TEST_P(TextfieldDumpAccessibilityEventsTest, ValueChanged) {
  // TODO(crbug.com/40672441): The legacy path (ViewsAXDisabled) incorrectly
  // fires EVENT_OBJECT_NAMECHANGE for text field value changes via a wrong
  // kTextChanged -> EVENT_OBJECT_NAMECHANGE MSAA mapping. The accessible name
  // doesn't change — only the value does. Fixed with ViewsAX enabled.
  if (!IsViewsAXEnabled() &&
      GetApiType() == ui::AXApiType::kWinIA2) {
    GTEST_SKIP() << "Legacy path fires incorrect EVENT_OBJECT_NAMECHANGE";
  }
  // The ViewsAX path fires AXValueChanged on the parent AXGroup in addition
  // to the textfield itself. Filter it for consistent cross-variant output.
  SetFilters(R"(
@WIN-ALLOW:EVENT_OBJECT_VALUECHANGE*
@WIN-ALLOW:IA2_EVENT_TEXT_INSERTED*
@UIA-WIN-ALLOW:Text_TextChanged*
@UIA-WIN-ALLOW:ValueValue*
@MAC-ALLOW:AXValueChanged*
@AURALINUX-ALLOW:TEXT-INSERT*
)");
  AddDenyFilter("AXValueChanged on AXGroup*");
  BEGIN_RECORDING_EVENTS_OR_SKIP("textfield-value-changed");
  textfield_->SetText(u"Hello World");
}

IN_PROC_BROWSER_TEST_P(TextfieldDumpAccessibilityEventsTest,
                       TextSelectionChanged) {
  textfield_->SetText(u"Hello World");
  textfield_->SetSelectedRange(gfx::Range(0));
  WaitForPendingSerialization();

  textfield_->RequestFocus();
  ASSERT_TRUE(base::test::RunUntil([&]() { return textfield_->HasFocus(); }));
  WaitForPendingSerialization();

  SetFilters(R"(
@WIN-ALLOW:IA2_EVENT_TEXT_CARET_MOVED*
@UIA-WIN-ALLOW:Text_TextSelectionChanged*
@MAC-ALLOW:AXSelectedTextChanged*
@AURALINUX-ALLOW:TEXT-CARET-MOVED*
@AURALINUX-ALLOW:TEXT-SELECTION-CHANGED*
)");
  BEGIN_RECORDING_EVENTS_OR_SKIP("textfield-text-selection-changed");
  textfield_->SetSelectedRange(gfx::Range(1, 5));
  if (GetApiType() == ui::AXApiType::kWinUIA) {
    EXPECT_TRUE(WaitForCapturedEvent("Text_TextSelectionChanged"));
  }
}

INSTANTIATE_TEST_SUITE_P(
    All,
    TextfieldDumpAccessibilityEventsTest,
    ::testing::ValuesIn(
        DumpAccessibilityEventsViewsTestBase::EventTestPasses()),
    EventTestPassToString());

}  // namespace
}  // namespace views
