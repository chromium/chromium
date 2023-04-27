// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/views/qrcode_generator/qrcode_generator_bubble.h"

#include "base/logging.h"
#include "base/memory/raw_ptr.h"
#include "base/scoped_observation.h"
#include "chrome/services/qrcode_generator/public/cpp/qrcode_generator_service.h"
#include "chrome/test/views/chrome_views_test_base.h"
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

TEST_F(QRCodeGeneratorBubbleTest, GeneratedCodeHasQuietZone) {
  const int kBaseSizeDip = 16;
  const int kQuietZoneTiles = 4;
  const int kTileToDip = 2;
  const int kQuietZoneDip = kQuietZoneTiles * kTileToDip;

  SkBitmap base_bitmap;
  base_bitmap.allocN32Pixels(kBaseSizeDip, kBaseSizeDip);
  base_bitmap.eraseColor(SK_ColorRED);
  auto base_image = gfx::ImageSkia::CreateFrom1xBitmap(base_bitmap);

  auto image = QRCodeGeneratorBubble::AddQRCodeQuietZone(
      base_image,
      gfx::Size(kBaseSizeDip / kTileToDip, kBaseSizeDip / kTileToDip),
      SK_ColorTRANSPARENT);

  EXPECT_EQ(base_image.width(), kBaseSizeDip);
  EXPECT_EQ(base_image.height(), kBaseSizeDip);
  EXPECT_EQ(image.width(), kBaseSizeDip + kQuietZoneDip * 2);
  EXPECT_EQ(image.height(), kBaseSizeDip + kQuietZoneDip * 2);

  EXPECT_EQ(SK_ColorRED, base_image.bitmap()->getColor(0, 0));

  EXPECT_EQ(SK_ColorTRANSPARENT, image.bitmap()->getColor(0, 0));
  EXPECT_EQ(SK_ColorTRANSPARENT,
            image.bitmap()->getColor(kQuietZoneDip, kQuietZoneDip - 1));
  EXPECT_EQ(SK_ColorTRANSPARENT,
            image.bitmap()->getColor(kQuietZoneDip - 1, kQuietZoneDip));
  EXPECT_EQ(SK_ColorRED,
            image.bitmap()->getColor(kQuietZoneDip, kQuietZoneDip));
}

// Test-fake implementation of QRImageGenerator; the real implementation
// can't be used in these tests because it requires spawning a service process.
class FakeQRCodeGeneratorService {
 public:
  FakeQRCodeGeneratorService() = default;

  using GenerateQRCodeCallback =
      base::OnceCallback<void(mojom::GenerateQRCodeResponsePtr)>;
  void GenerateQRCode(mojom::GenerateQRCodeRequestPtr request,
                      GenerateQRCodeCallback callback) {
    pending_callback_ = std::move(callback);
    if (run_loop_)
      run_loop_->Quit();
  }

  void WaitForRequest() {
    if (HasPendingRequest())
      return;
    run_loop_ = std::make_unique<base::RunLoop>();
    run_loop_->Run();
  }

  bool HasPendingRequest() const { return bool(pending_callback_); }

  void DeliverResponse(mojom::GenerateQRCodeResponsePtr response) {
    CHECK(pending_callback_);
    std::move(pending_callback_).Run(std::move(response));
  }

 private:
  GenerateQRCodeCallback pending_callback_;
  std::unique_ptr<base::RunLoop> run_loop_;
};

class ViewVisibilityWaiter : public views::ViewObserver {
 public:
  explicit ViewVisibilityWaiter(views::View* view) {
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

    anchor_widget_.reset(CreateTestWidget().release());
    anchor_view_ =
        anchor_widget_->SetContentsView(std::make_unique<views::View>());
    CHECK(anchor_view_);
    auto bubble = std::make_unique<QRCodeGeneratorBubble>(
        anchor_view_, nullptr, base::DoNothing(), base::DoNothing(),
        GURL("https://www.chromium.org/a"));

    // `base::Unretained` is okay, because `TearDown` will run before
    // destruction of `fake_service_` and will `reset` the `bubble_widget_`
    // which will destroy the `bubble` which will destroy/drop the callback we
    // are setting for testing below.
    bubble->SetQRCodeServiceForTesting(
        base::BindRepeating(&FakeQRCodeGeneratorService::GenerateQRCode,
                            base::Unretained(&fake_service_)));

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
           error_label()->GetPreferredSize().height() > 0 &&
           error_label()->GetPreferredSize().width() > 0;
  }

  bool ErrorLabelHiddenAndA11yIgnored() {
    return !error_label()->GetVisible() &&
           error_label()->GetViewAccessibility().IsIgnored();
  }

  FakeQRCodeGeneratorService* service() { return &fake_service_; }

 private:
  WidgetAutoclosePtr anchor_widget_;
  raw_ptr<views::View> anchor_view_;
  raw_ptr<QRCodeGeneratorBubble> bubble_;
  WidgetAutoclosePtr bubble_widget_;

  FakeQRCodeGeneratorService fake_service_;
};

// This test is a bit fiddly because mojo imposes asynchronicity on both sender
// and receiver sides of a service. That means that:
// 1. We need to wait for QR code generation requests to arrive at the service
// 2. We need to wait for observable changes to the ImageView to know when
//    responses have been delivered
TEST_F(QRCodeGeneratorBubbleUITest, ImageShowsAfterErrorState) {
  bubble()->Show();

  EXPECT_TRUE(ImageShowing());

  service()->WaitForRequest();
  ASSERT_TRUE(service()->HasPendingRequest());
  auto error_response = mojom::GenerateQRCodeResponse::New();
  error_response->error_code = mojom::QRCodeGeneratorError::UNKNOWN_ERROR;

  EXPECT_TRUE(ImageShowing());

  {
    ViewVisibilityWaiter waiter(image());
    service()->DeliverResponse(std::move(error_response));
    waiter.Wait();

    EXPECT_FALSE(ImageShowing());
  }

  // The UI regenerates the QR code when the user types new text, so synthesize
  // that.
  textfield()->InsertOrReplaceText(u"https://www.chromium.org/b");
  service()->WaitForRequest();

  auto ok_response = mojom::GenerateQRCodeResponse::New();
  ok_response->error_code = mojom::QRCodeGeneratorError::NONE;
  ok_response->bitmap.allocN32Pixels(16, 16);
  ok_response->data_size = gfx::Size(16, 16);

  {
    ViewVisibilityWaiter waiter(image());
    service()->DeliverResponse(std::move(ok_response));
    waiter.Wait();

    EXPECT_TRUE(ImageShowing());
  }
}

TEST_F(QRCodeGeneratorBubbleUITest,
       PlaceholderImageShowsAfterTextFieldEmptied) {
  bubble()->Show();

  EXPECT_TRUE(ImagePlaceholderShowing());

  service()->WaitForRequest();
  ASSERT_TRUE(service()->HasPendingRequest());
  auto error_response = mojom::GenerateQRCodeResponse::New();
  error_response->error_code = mojom::QRCodeGeneratorError::UNKNOWN_ERROR;

  EXPECT_TRUE(ImagePlaceholderShowing());

  {
    ViewVisibilityWaiter waiter(image());
    service()->DeliverResponse(std::move(error_response));
    waiter.Wait();

    EXPECT_FALSE(ImageShowing());
  }

  auto ok_response = mojom::GenerateQRCodeResponse::New();
  ok_response->error_code = mojom::QRCodeGeneratorError::NONE;
  ok_response->bitmap.allocN32Pixels(16, 16);
  ok_response->bitmap.eraseColor(SK_ColorRED);
  ok_response->data_size = gfx::Size(16, 16);

  // The UI regenerates the QR code when the user types new text, so synthesize
  // that.
  textfield()->InsertOrReplaceText(u"https://www.chromium.org/b");
  service()->WaitForRequest();

  {
    ViewVisibilityWaiter waiter(image());
    service()->DeliverResponse(std::move(ok_response));
    waiter.Wait();

    EXPECT_TRUE(ImageShowing());
    EXPECT_FALSE(ImagePlaceholderShowing());
    EXPECT_TRUE(download_button()->GetEnabled());
  }

  textfield()->SelectAll(false);
  textfield()->DeleteRange(textfield()->GetSelectedRange());

  EXPECT_TRUE(ImageShowing());
  EXPECT_TRUE(ImagePlaceholderShowing());
  EXPECT_FALSE(download_button()->GetEnabled());
}

TEST_F(QRCodeGeneratorBubbleUITest, LabelHidesAfterErrorState) {
  bubble()->Show();

  EXPECT_TRUE(ImagePlaceholderShowing());
  EXPECT_TRUE(ErrorLabelHiddenAndA11yIgnored());

  service()->WaitForRequest();
  ASSERT_TRUE(service()->HasPendingRequest());
  auto error_response = mojom::GenerateQRCodeResponse::New();
  error_response->error_code = mojom::QRCodeGeneratorError::UNKNOWN_ERROR;

  EXPECT_TRUE(ImagePlaceholderShowing());

  {
    ViewVisibilityWaiter waiter(image());
    service()->DeliverResponse(std::move(error_response));
    waiter.Wait();

    EXPECT_FALSE(ImageShowing());
  }

  auto too_long_response = mojom::GenerateQRCodeResponse::New();
  too_long_response->error_code = mojom::QRCodeGeneratorError::INPUT_TOO_LONG;

  // The UI regenerates the QR code when the user types new text, so synthesize
  // that.
  textfield()->InsertOrReplaceText(u"https://www.chromium.org/b");
  service()->WaitForRequest();

  {
    ViewVisibilityWaiter waiter(image());
    service()->DeliverResponse(std::move(too_long_response));
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
