// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/views/global_error_bubble_view.h"

#include <memory>
#include <vector>

#include "base/strings/utf_string_conversions.h"
#include "chrome/browser/ui/global_error/global_error.h"
#include "chrome/browser/ui/views/chrome_layout_provider.h"
#include "chrome/test/views/chrome_test_views_delegate.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "ui/gfx/codec/png_codec.h"
#include "ui/gfx/image/image.h"
#include "ui/gfx/image/image_png_rep.h"
#include "ui/gfx/image/image_unittest_util.h"
#include "ui/views/controls/button/button.h"
#include "ui/views/controls/button/label_button.h"

using ::testing::_;
using ::testing::Return;
using ::testing::StrictMock;

namespace views {

namespace {

class MockGlobalErrorWithStandardBubble : public GlobalErrorWithStandardBubble {
 public:
  MockGlobalErrorWithStandardBubble() {}
  ~MockGlobalErrorWithStandardBubble() override;

  MOCK_METHOD0(GetBubbleViewIcon, gfx::Image());
  MOCK_METHOD0(GetBubbleViewTitle, base::string16());
  MOCK_METHOD0(GetBubbleViewMessage, std::vector<base::string16>());
  MOCK_CONST_METHOD0(ShouldShowCloseButton, bool());
  MOCK_METHOD0(ShouldAddElevationIconToAcceptButton, bool());
  MOCK_METHOD1(BubbleViewDidClose, void(Browser* browser));
  MOCK_METHOD1(OnBubbleViewDidClose, void(Browser* browser));
  MOCK_METHOD1(BubbleViewAcceptButtonPressed, void(Browser* browser));
  MOCK_METHOD1(BubbleViewCancelButtonPressed, void(Browser* browser));
  MOCK_CONST_METHOD0(ShouldCloseOnDeactivate, bool());
  MOCK_METHOD0(HasBubbleView, bool());
  MOCK_METHOD0(HasShownBubbleView, bool());
  MOCK_METHOD1(ShowBubbleView, void(Browser* browser));
  MOCK_METHOD0(GetBubbleView, GlobalErrorBubbleViewBase*());
  MOCK_METHOD0(HasMenuItem, bool());
  MOCK_METHOD0(MenuItemCommandID, int());
  MOCK_METHOD0(MenuItemLabel, base::string16());
  MOCK_METHOD1(ExecuteMenuItem, void(Browser* browser));
  MOCK_METHOD0(GetBubbleViewMessages, std::vector<base::string16>());

  base::string16 GetBubbleViewAcceptButtonLabel() {
    return base::UTF8ToUTF16("Ok");
  }
  base::string16 GetBubbleViewCancelButtonLabel() {
    return base::UTF8ToUTF16("Cancel");
  }

 private:
  DISALLOW_COPY_AND_ASSIGN(MockGlobalErrorWithStandardBubble);
};

MockGlobalErrorWithStandardBubble::~MockGlobalErrorWithStandardBubble() {}

}  // namespace

class GlobalErrorBubbleViewTest : public testing::Test {
 public:
  GlobalErrorBubbleViewTest()
      : mock_global_error_with_standard_bubble_(
            std::make_unique<StrictMock<MockGlobalErrorWithStandardBubble>>()),
        button_(nullptr, base::string16()),
        view_(std::make_unique<GlobalErrorBubbleView>(
            &arg_view_,
            gfx::Rect(gfx::Point(), gfx::Size()),
            views::BubbleBorder::NONE,
            nullptr,
            mock_global_error_with_standard_bubble_->AsWeakPtr())) {}

 protected:
  ChromeTestViewsDelegate test_views_delegate_;
  std::unique_ptr<StrictMock<MockGlobalErrorWithStandardBubble>>
      mock_global_error_with_standard_bubble_;
  views::View arg_view_;
  views::LabelButton button_;
  std::unique_ptr<GlobalErrorBubbleView> view_;

 private:
  DISALLOW_COPY_AND_ASSIGN(GlobalErrorBubbleViewTest);
};

TEST_F(GlobalErrorBubbleViewTest, Basic) {
  EXPECT_CALL(*mock_global_error_with_standard_bubble_, GetBubbleViewTitle());
  view_->GetWindowTitle();

  std::vector<gfx::ImagePNGRep> image_png_reps;
  image_png_reps.push_back(
      gfx::ImagePNGRep(gfx::test::CreatePNGBytes(25), 1.0f));
  gfx::Image image = gfx::Image(image_png_reps);
  EXPECT_CALL(*mock_global_error_with_standard_bubble_, GetBubbleViewIcon())
      .WillOnce(Return(image));
  view_->GetWindowIcon();

  EXPECT_EQ(ChromeLayoutProvider::Get()->ShouldShowWindowIcon(),
            view_->ShouldShowWindowIcon());

  EXPECT_CALL(*mock_global_error_with_standard_bubble_,
              BubbleViewDidClose(nullptr));
  view_->WindowClosing();

  EXPECT_CALL(*mock_global_error_with_standard_bubble_,
              ShouldShowCloseButton());
  view_->ShouldShowCloseButton();
  view_->GetDialogButtons();

  EXPECT_CALL(*mock_global_error_with_standard_bubble_,
              BubbleViewCancelButtonPressed(nullptr));
  view_->Cancel();

  EXPECT_CALL(*mock_global_error_with_standard_bubble_,
              BubbleViewAcceptButtonPressed(nullptr));
  view_->Accept();
}

TEST_F(GlobalErrorBubbleViewTest, ErrorIsNull) {
  mock_global_error_with_standard_bubble_.reset();
  view_->GetWindowTitle();
  view_->WindowClosing();

  view_->ShouldShowCloseButton();

  view_->Cancel();
  view_->Accept();
}

}  // namespace views
