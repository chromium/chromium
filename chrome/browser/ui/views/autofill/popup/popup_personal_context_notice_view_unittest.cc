// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/views/autofill/popup/popup_personal_context_notice_view.h"

#include <memory>

#include "base/memory/raw_ptr.h"
#include "base/memory/weak_ptr.h"
#include "chrome/browser/ui/autofill/mock_autofill_popup_controller.h"
#include "chrome/test/views/chrome_views_test_base.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "ui/events/test/event_generator.h"
#include "ui/views/controls/button/md_text_button.h"
#include "ui/views/widget/widget.h"
#include "ui/views/widget/widget_utils.h"

namespace autofill {

namespace {
constexpr int kNoticePosition = 0;

class PopupPersonalContextNoticeViewTest : public ChromeViewsTestBase {
 public:
  void SetUp() override {
    ChromeViewsTestBase::SetUp();
    widget_ = CreateTestWidget(views::Widget::InitParams::CLIENT_OWNS_WIDGET);
    generator_ = std::make_unique<ui::test::EventGenerator>(
        GetRootWindow(widget_.get()));
  }

  void ShowView() {
    view_ = widget_->SetContentsView(
        std::make_unique<PopupPersonalContextNoticeView>(
            controller().GetWeakPtr(), kNoticePosition));

    // Assign manual bounds so the widget has a physical size.
    // In test env, this is required to position child views
    // so the EventGenerator can accurately click them.
    widget_->SetBounds(gfx::Rect(0, 0, 500, 500));
    widget_->Show();
  }

  void TearDown() override {
    view_ = nullptr;
    generator_.reset();
    widget_.reset();
    ChromeViewsTestBase::TearDown();
  }

 protected:
  PopupPersonalContextNoticeView& view() { return *view_; }
  MockAutofillPopupController& controller() { return controller_; }
  views::Widget& widget() { return *widget_; }
  ui::test::EventGenerator& generator() { return *generator_; }

 private:
  std::unique_ptr<views::Widget> widget_;
  std::unique_ptr<ui::test::EventGenerator> generator_;
  testing::NiceMock<MockAutofillPopupController> controller_;
  raw_ptr<PopupPersonalContextNoticeView> view_ = nullptr;
};

// Tests the notice view is correctly created and displays its initial elements.
TEST_F(PopupPersonalContextNoticeViewTest, InitialState) {
  ShowView();

  // Check that the "Got it" button is visible and has the correct text.
  views::MdTextButton* got_it_button = view().got_it_button_for_testing();
  ASSERT_NE(got_it_button, nullptr);
  EXPECT_TRUE(got_it_button->GetVisible());
  // TODO(crbug.com/517520354): Update to localized string.
  EXPECT_EQ(got_it_button->GetText(), u"OK");
}

// Tests that clicking on GotIt button triggers the removal of the notice.
TEST_F(PopupPersonalContextNoticeViewTest,
       GotItButtonTriggersRemoveSuggestion) {
  ShowView();

  // Ensure the child views (e.g. got_it_button) are laid out in the widget.
  // This calculates their screen coordinates so they can be accurately
  // located and clicked by the EventGenerator.
  widget().LayoutRootViewIfNecessary();

  views::MdTextButton* got_it_button = view().got_it_button_for_testing();

  EXPECT_CALL(
      controller(),
      RemoveSuggestion(
          kNoticePosition,
          AutofillMetrics::SingleEntryRemovalMethod::kDeleteButtonClicked))
      .WillOnce(testing::Return(true));

  generator().MoveMouseTo(got_it_button->GetBoundsInScreen().CenterPoint());
  generator().ClickLeftButton();
}

}  // namespace
}  // namespace autofill
