// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/views/qrcode_generator/qrcode_generator_bubble.h"

#include "base/logging.h"
#include "base/memory/raw_ptr.h"
#include "base/scoped_observation.h"
#include "chrome/test/base/testing_profile.h"
#include "chrome/test/views/chrome_views_test_base.h"
#include "components/qr_code_generator/bitmap_generator.h"
#include "content/public/test/test_renderer_host.h"
#include "content/public/test/web_contents_tester.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "ui/views/accessibility/view_accessibility.h"
#include "ui/views/controls/button/label_button.h"
#include "ui/views/controls/image_view.h"
#include "ui/views/controls/textfield/textfield.h"
#include "ui/views/view_observer.h"
#include "url/gurl.h"

namespace qrcode_generator {

namespace {

using QRCodeGeneratorBubbleTest = testing::Test;

TEST_F(QRCodeGeneratorBubbleTest, SuggestedDownloadURLNoIP) {
  EXPECT_EQ(QRCodeGeneratorBubble::GetQRCodeFilenameForURL(GURL("10.1.2.3")),
            u"qrcode_chrome.png");

  EXPECT_EQ(QRCodeGeneratorBubble::GetQRCodeFilenameForURL(
                GURL("https://chromium.org")),
            u"qrcode_chromium.org.png");

  EXPECT_EQ(
      QRCodeGeneratorBubble::GetQRCodeFilenameForURL(GURL("text, not url")),
      u"qrcode_chrome.png");
}

class VisibilityChangedWaiter : public views::ViewObserver {
 public:
  explicit VisibilityChangedWaiter(views::View* view) {
    observation_.Observe(view);
  }

  void Wait() { run_loop_.Run(); }

  void OnViewVisibilityChanged(views::View* view,
                               views::View* starting_view) override {
    run_loop_.Quit();
  }

 private:
  base::ScopedObservation<views::View, views::ViewObserver> observation_{this};
  base::RunLoop run_loop_;
};

class QRCodeGeneratorBubbleUITest : public ChromeViewsTestBase {
 public:
  void SetUp() override {
    ChromeViewsTestBase::SetUp();
    web_contents_ =
        content::WebContentsTester::CreateTestWebContents(&profile_, nullptr);

    // TODO(crbug.com/40232479) - We can probably clean this up and
    // get rid of the need for a WidgetAutoClosePtr when we switch to
    // CLIENT_OWNS_WIDGET.
    anchor_widget_.reset(
        CreateTestWidget(views::Widget::InitParams::WIDGET_OWNS_NATIVE_WIDGET)
            .release());
    anchor_view_ =
        anchor_widget_->SetContentsView(std::make_unique<views::View>());
    CHECK(anchor_view_);
    auto bubble = std::make_unique<QRCodeGeneratorBubble>(
        anchor_view_, web_contents_->GetWeakPtr(), base::DoNothing(),
        base::DoNothing(), GURL("https://www.chromium.org/a"));

    bubble_ = bubble.get();
    bubble_widget_.reset(
        views::BubbleDialogDelegateView::CreateBubble(std::move(bubble)));
  }

  void TearDown() override {
    bubble_widget_.reset();
    anchor_widget_.reset();
    ChromeViewsTestBase::TearDown();
  }

  QRCodeGeneratorBubble* bubble() { return bubble_; }
  views::ImageView* image() { return bubble_->image_for_testing(); }
  views::Textfield* textfield() { return bubble_->textfield_for_testing(); }
  views::Label* error_label() { return bubble_->error_label_for_testing(); }
  views::LabelButton* download_button() {
    return bubble_->download_button_for_testing();
  }

  bool ImageShowing() {
    return image()->GetVisible() && image()->GetPreferredSize().height() > 0 &&
           image()->GetPreferredSize().width() > 0;
  }

  bool ImagePlaceholderShowing() {
    return ImageShowing() && image()->GetImage().height() > 128 &&
           image()->GetImage().width() > 128 &&
           image()->GetImage().bitmap()->getColor(128, 128) ==
               SK_ColorTRANSPARENT;
  }

  // Note that this function and the one below it are not opposites!
  // ErrorLabelShowing() checks whether *all* the conditions for the label to
  // appear are true, and ErrorLabelHiddenAndA11yIgnored() checks whether *none*
  // of the conditions for the label to appear are true.
  bool ErrorLabelShowing() {
    return error_label()->GetVisible() &&
           !error_label()
                ->GetPreferredSize(
                    views::SizeBounds(error_label()->width(), {}))
                .IsEmpty();
  }

  bool ErrorLabelHiddenAndA11yIgnored() {
    return !error_label()->GetVisible() &&
           error_label()->GetViewAccessibility().GetIsIgnored();
  }

 private:
  content::RenderViewHostTestEnabler rvh_test_enabler_;
  TestingProfile profile_;
  std::unique_ptr<content::WebContents> web_contents_;

  WidgetAutoclosePtr anchor_widget_;
  raw_ptr<views::View, DanglingUntriaged> anchor_view_;
  WidgetAutoclosePtr bubble_widget_;
  raw_ptr<QRCodeGeneratorBubble, DanglingUntriaged> bubble_;
};

// This test is a bit fiddly because mojo imposes asynchronicity on both sender
// and receiver sides of a service. That means that:
// 1. We need to wait for QR code generation requests to arrive at the service
// 2. We need to wait for observable changes to the ImageView to know when
//    responses have been delivered
TEST_F(QRCodeGeneratorBubbleUITest, ImageShowsAfterErrorState) {
  bubble()->Show();
  EXPECT_TRUE(ImageShowing());

  // The UI regenerates the QR code when the user types new text, so synthesize
  // that, but inject an error.
  {
    VisibilityChangedWaiter waiter(image());
    bubble()->SetQRCodeErrorForTesting(qr_code_generator::Error::kUnknownError);
    textfield()->InsertOrReplaceText(u"https://www.chromium.org/b");
    waiter.Wait();
    EXPECT_FALSE(ImageShowing());
  }

  // The UI regenerates the QR code when the user types new text, so synthesize
  // that, but this time clear the injected error.
  {
    VisibilityChangedWaiter waiter(image());
    bubble()->SetQRCodeErrorForTesting(std::nullopt);
    textfield()->InsertOrReplaceText(u"https://www.chromium.org/b");
    waiter.Wait();
    EXPECT_TRUE(ImageShowing());
  }
}

TEST_F(QRCodeGeneratorBubbleUITest,
       PlaceholderImageShowsAfterTextFieldEmptied) {
  // Expecting placeholder image when the text input is empty.
  textfield()->SelectAll(false);
  textfield()->DeleteRange(textfield()->GetSelectedRange());
  bubble()->Show();
  EXPECT_TRUE(ImagePlaceholderShowing());

  // Expecting image to be hidden after QR generation error.
  {
    VisibilityChangedWaiter waiter(image());
    bubble()->SetQRCodeErrorForTesting(qr_code_generator::Error::kUnknownError);
    textfield()->InsertOrReplaceText(u"https://www.chromium.org/b");
    waiter.Wait();

    EXPECT_FALSE(ImageShowing());
  }

  // Expecting image to be shown after QR generation success.
  {
    VisibilityChangedWaiter waiter(image());
    bubble()->SetQRCodeErrorForTesting(std::nullopt);
    // The UI regenerates the QR code when the user types new text, so
    // synthesize that.
    textfield()->InsertOrReplaceText(u"https://www.chromium.org/b");
    waiter.Wait();

    EXPECT_TRUE(ImageShowing());
    EXPECT_FALSE(ImagePlaceholderShowing());
    EXPECT_TRUE(download_button()->GetEnabled());
  }

  // Expecting placeholder image after deleting the text input again.
  textfield()->SelectAll(false);
  textfield()->DeleteRange(textfield()->GetSelectedRange());
  EXPECT_TRUE(ImageShowing());
  EXPECT_TRUE(ImagePlaceholderShowing());
  EXPECT_FALSE(download_button()->GetEnabled());
}

TEST_F(QRCodeGeneratorBubbleUITest, LabelHidesAfterErrorState) {
  // Expecting placeholder image when the text input is empty.
  textfield()->SelectAll(false);
  textfield()->DeleteRange(textfield()->GetSelectedRange());
  bubble()->Show();
  EXPECT_TRUE(ImagePlaceholderShowing());
  EXPECT_TRUE(ErrorLabelHiddenAndA11yIgnored());

  // Expecting image to be hidden after QR generation error.
  {
    VisibilityChangedWaiter waiter(image());
    bubble()->SetQRCodeErrorForTesting(qr_code_generator::Error::kUnknownError);
    textfield()->InsertOrReplaceText(u"https://www.chromium.org/b");
    waiter.Wait();

    EXPECT_FALSE(ImageShowing());
  }

  // Input-too-long should present a different UI from unknown errors.
  {
    VisibilityChangedWaiter waiter(image());
    bubble()->SetQRCodeErrorForTesting(qr_code_generator::Error::kInputTooLong);
    textfield()->InsertOrReplaceText(u"https://www.chromium.org/b");
    waiter.Wait();
    EXPECT_TRUE(ImageShowing());
    EXPECT_TRUE(ImagePlaceholderShowing());
    EXPECT_TRUE(ErrorLabelShowing());
    EXPECT_FALSE(download_button()->GetEnabled());
  }

  textfield()->SelectAll(false);
  textfield()->DeleteRange(textfield()->GetSelectedRange());

  EXPECT_TRUE(ImageShowing());
  EXPECT_TRUE(ImagePlaceholderShowing());
  EXPECT_TRUE(ErrorLabelHiddenAndA11yIgnored());
  EXPECT_FALSE(download_button()->GetEnabled());
}

}  // namespace

}  // namespace qrcode_generator
