// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/views/context_hub/save_to_memory_bank_bubble_view.h"

#include <memory>

#include "chrome/grit/generated_resources.h"
#include "chrome/test/base/testing_profile.h"
#include "chrome/test/views/chrome_views_test_base.h"
#include "content/public/test/test_renderer_host.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "ui/base/l10n/l10n_util.h"
#include "ui/views/widget/widget.h"

class SaveToMemoryBankBubbleViewTest : public ChromeViewsTestBase {
 public:
  SaveToMemoryBankBubbleViewTest() = default;
  ~SaveToMemoryBankBubbleViewTest() override = default;

  void SetUp() override {
    ChromeViewsTestBase::SetUp();
    profile_ = std::make_unique<TestingProfile>();
    anchor_widget_ =
        CreateTestWidget(views::Widget::InitParams::CLIENT_OWNS_WIDGET,
                         views::Widget::InitParams::TYPE_WINDOW);
    anchor_widget_->Show();
  }

  void TearDown() override {
    anchor_widget_.reset();
    profile_.reset();
    ChromeViewsTestBase::TearDown();
  }

 protected:
  content::RenderViewHostTestEnabler rvh_test_enabler_;
  std::unique_ptr<TestingProfile> profile_;
  std::unique_ptr<views::Widget> anchor_widget_;
};

TEST_F(SaveToMemoryBankBubbleViewTest, CreateAndShowBubble) {
  auto delegate = std::make_unique<SaveToMemoryBankBubbleView>(
      anchor_widget_->GetContentsView(), profile_.get());
  SaveToMemoryBankBubbleView* delegate_ptr = delegate.get();

  std::unique_ptr<views::Widget> bubble_widget =
      views::BubbleDialogDelegate::CreateBubble(delegate_ptr);
  ASSERT_TRUE(bubble_widget);
  EXPECT_EQ(l10n_util::GetStringUTF16(IDS_CONTENT_CONTEXT_SAVE_TO_MEMORY_BANKS),
            bubble_widget->widget_delegate()->GetWindowTitle());
  EXPECT_NE(nullptr, delegate_ptr->web_view_for_testing());
}

TEST_F(SaveToMemoryBankBubbleViewTest, CloseContentsClosesWidget) {
  auto delegate = std::make_unique<SaveToMemoryBankBubbleView>(
      anchor_widget_->GetContentsView(), profile_.get());
  SaveToMemoryBankBubbleView* delegate_ptr = delegate.get();

  std::unique_ptr<views::Widget> bubble_widget =
      views::BubbleDialogDelegate::CreateBubble(delegate_ptr);
  ASSERT_TRUE(bubble_widget);
  EXPECT_FALSE(bubble_widget->IsClosed());

  delegate_ptr->CloseContents(nullptr);
  EXPECT_TRUE(bubble_widget->IsClosed());
}
