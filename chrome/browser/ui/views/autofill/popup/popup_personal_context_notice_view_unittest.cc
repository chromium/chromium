// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/views/autofill/popup/popup_personal_context_notice_view.h"

#include <memory>

#include "base/memory/raw_ptr.h"
#include "chrome/test/views/chrome_views_test_base.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "ui/views/controls/button/md_text_button.h"
#include "ui/views/widget/widget.h"

namespace autofill {
namespace {

class PopupPersonalContextNoticeViewTest : public ChromeViewsTestBase {
 public:
  void SetUp() override {
    ChromeViewsTestBase::SetUp();
    widget_ = CreateTestWidget(views::Widget::InitParams::CLIENT_OWNS_WIDGET);
  }

  void ShowView() {
    view_ = widget_->SetContentsView(
        std::make_unique<PopupPersonalContextNoticeView>());
    widget_->Show();
  }

  void TearDown() override {
    view_ = nullptr;
    widget_.reset();
    ChromeViewsTestBase::TearDown();
  }

 protected:
  PopupPersonalContextNoticeView& view() { return *view_; }
  views::Widget& widget() { return *widget_; }

 private:
  std::unique_ptr<views::Widget> widget_;
  raw_ptr<PopupPersonalContextNoticeView> view_ = nullptr;
};

// Tests that the `PopupPersonalContextNoticeView` is created and displayed
// correctly.
TEST_F(PopupPersonalContextNoticeViewTest, PopupPersonalContextNoticeView) {
  ShowView();

  // Check that the "Got it" button is visible and has the correct text.
  views::MdTextButton* got_it_button = view().got_it_button_for_testing();
  ASSERT_NE(got_it_button, nullptr);
  EXPECT_TRUE(got_it_button->GetVisible());
  // TODO(crbug.com/517520354): Update to localized string.
  EXPECT_EQ(got_it_button->GetText(), u"OK");
}

}  // namespace
}  // namespace autofill
