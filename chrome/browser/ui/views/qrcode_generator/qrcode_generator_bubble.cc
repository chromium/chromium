// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/views/qrcode_generator/qrcode_generator_bubble.h"

#include "base/strings/utf_string_conversions.h"
#include "chrome/browser/themes/theme_properties.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/browser/ui/browser_dialogs.h"
#include "chrome/browser/ui/qrcode_generator/qrcode_generator_bubble_controller.h"
#include "chrome/browser/ui/ui_features.h"
#include "chrome/browser/ui/views/chrome_layout_provider.h"
#include "chrome/browser/ui/views/chrome_typography.h"
#include "chrome/browser/ui/views/frame/browser_view.h"
#include "chrome/browser/ui/views/frame/top_container_view.h"
#include "chrome/grit/generated_resources.h"
#include "components/url_formatter/url_formatter.h"
#include "content/public/browser/web_contents.h"
#include "ui/base/l10n/l10n_util.h"
#include "ui/base/theme_provider.h"
#include "ui/events/event.h"
#include "ui/gfx/image/image_skia_source.h"
#include "ui/native_theme/native_theme.h"
#include "ui/resources/grit/ui_resources.h"
#include "ui/views/bubble/bubble_frame_view.h"
#include "ui/views/bubble/tooltip_icon.h"
#include "ui/views/controls/button/label_button.h"
#include "ui/views/controls/button/md_text_button.h"
#include "ui/views/controls/image_view.h"
#include "ui/views/controls/label.h"
#include "ui/views/controls/textfield/textfield.h"
#include "ui/views/layout/grid_layout.h"
#include "ui/views/layout/layout_provider.h"
#include "ui/views/view.h"
#include "ui/views/widget/widget.h"

namespace {

// Rendered QR Code size, pixels.
constexpr int kQRImageSizePx = 200;
constexpr int kPaddingTooltipDownloadButtonPx = 10;

// Calculates preview image dimensions.
constexpr gfx::Size GetQRImageSize() {
  return gfx::Size(kQRImageSizePx, kQRImageSizePx);
}

// Renders a solid square of color {r, g, b} at 100% alpha.
gfx::ImageSkia GetPlaceholderImageSkia(unsigned r, unsigned g, unsigned b) {
  SkBitmap bitmap;
  bitmap.allocN32Pixels(kQRImageSizePx, kQRImageSizePx);
  bitmap.eraseARGB(r, g, b, 0);
  return gfx::ImageSkia(gfx::ImageSkiaRep(bitmap, 1.0f));
}

// Adds a new small vertical padding row to the current bottom of |layout|.
void AddSmallPaddingRow(views::GridLayout* layout) {
  layout->AddPaddingRow(views::GridLayout::kFixedSize,
                        ChromeLayoutProvider::Get()->GetDistanceMetric(
                            DISTANCE_RELATED_CONTROL_VERTICAL_SMALL));
}

}  // namespace

namespace qrcode_generator {

QRCodeGeneratorBubble::QRCodeGeneratorBubble(
    views::View* anchor_view,
    content::WebContents* web_contents,
    QRCodeGeneratorBubbleController* controller,
    const GURL& url)
    : LocationBarBubbleDelegateView(anchor_view, nullptr),
      url_(url),
      controller_(controller) {
  DCHECK(controller);

  DialogDelegate::set_buttons(ui::DIALOG_BUTTON_NONE);
}

QRCodeGeneratorBubble::~QRCodeGeneratorBubble() = default;

void QRCodeGeneratorBubble::Show() {
  chrome::RecordDialogCreation(chrome::DialogIdentifier::QR_CODE_GENERATOR);
  UpdateQRContent();
  ShowForReason(USER_GESTURE);
}

void QRCodeGeneratorBubble::Hide() {
  if (controller_) {
    controller_->OnBubbleClosed();
    controller_ = nullptr;
  }
  CloseBubble();
}

void QRCodeGeneratorBubble::UpdateQRContent() {
  // TODO(skare): Generate QR code OOP.
  // This is being done in a mojo service in a follow-on change.
  // At that time, handle error code for url-too-long, and !url_.valid().
  // As a placeholder, we cycle colors randomly as input changes.
  UpdateQRImage(
      GetPlaceholderImageSkia(rand() % 0x100, rand() % 0x100, rand() % 0x100));
}

void QRCodeGeneratorBubble::UpdateQRImage(gfx::ImageSkia qr_image) {
  qr_code_image_->SetImage(qr_image);
  qr_code_image_->SetImageSize(GetQRImageSize());
  qr_code_image_->SetBackground(nullptr);
}

views::View* QRCodeGeneratorBubble::GetInitiallyFocusedView() {
  return textfield_url_;
}

base::string16 QRCodeGeneratorBubble::GetWindowTitle() const {
  return l10n_util::GetStringUTF16(IDS_BROWSER_SHARING_QR_CODE_DIALOG_TITLE);
}

bool QRCodeGeneratorBubble::ShouldShowCloseButton() const {
  return true;
}

void QRCodeGeneratorBubble::WindowClosing() {
  if (controller_) {
    controller_->OnBubbleClosed();
    controller_ = nullptr;
  }
}

bool QRCodeGeneratorBubble::Close() {
  return Cancel();
}

const char* QRCodeGeneratorBubble::GetClassName() const {
  return "QRCodeGeneratorBubble";
}

void QRCodeGeneratorBubble::Init() {
  set_margins(ChromeLayoutProvider::Get()->GetDialogInsetsForContentType(
      views::CONTROL, views::CONTROL));

  // Internal IDs for column layout; no effect on UI.
  constexpr int kSingleColumnSetId = 0;
  constexpr int kDownloadRowColumnSetId = 1;

  // Add top-level Grid Layout manager for this dialog.
  views::GridLayout* const layout =
      SetLayoutManager(std::make_unique<views::GridLayout>());
  views::ColumnSet* column_set = layout->AddColumnSet(kSingleColumnSetId);
  column_set->AddColumn(
      views::GridLayout::FILL,    // Fill horizontally.
      views::GridLayout::CENTER,  // Align center vertically, do not resize.
      1.0, views::GridLayout::USE_PREF, 0,
      ChromeLayoutProvider::Get()->GetDistanceMetric(
          DISTANCE_BUBBLE_PREFERRED_WIDTH));

  // Main QR Code Image
  using Alignment = views::ImageView::Alignment;
  auto qr_code_image = std::make_unique<views::ImageView>();
  qr_code_image->SetVisible(true);
  qr_code_image->SetHorizontalAlignment(Alignment::kCenter);
  qr_code_image->SetVerticalAlignment(Alignment::kCenter);
  qr_code_image->SetImageSize(GetQRImageSize());
  qr_code_image->SetPreferredSize(GetQRImageSize());
  qr_code_image->SetImage(GetPlaceholderImageSkia(0x33, 0x33, 0xCC));
  layout->StartRow(views::GridLayout::kFixedSize, kSingleColumnSetId);
  qr_code_image_ = layout->AddView(std::move(qr_code_image));

  // Padding
  AddSmallPaddingRow(layout);

  // Text box to edit URL
  auto textfield_url = std::make_unique<views::Textfield>();
  textfield_url->SetAccessibleName(l10n_util::GetStringUTF16(
      IDS_BROWSER_SHARING_QR_CODE_DIALOG_URL_TEXTFIELD_ACCESSIBLE_NAME));
  textfield_url->SetText(base::ASCIIToUTF16(url_.spec()));
  textfield_url->set_controller(this);
  layout->StartRow(views::GridLayout::kFixedSize, kSingleColumnSetId);
  textfield_url_ = layout->AddView(std::move(textfield_url));

  // Padding
  AddSmallPaddingRow(layout);

  // Controls row: tooltip and download button.
  views::ColumnSet* control_columns =
      layout->AddColumnSet(kDownloadRowColumnSetId);
  // Column for tooltip.
  control_columns->AddColumn(
      views::GridLayout::FILL,      // View fills the horizontal space.
      views::GridLayout::CENTER,    // View moves to center of vertical space.
      1.0,                          // This column has a resize weight of 1.
      views::GridLayout::USE_PREF,  // Use the preferred size of the view.
      0,                            // Ignored for USE_PREF.
      0);                           // Minimum width of 0.
  // Spacing between tooltip and download button.
  control_columns->AddPaddingColumn(views::GridLayout::kFixedSize,
                                    kPaddingTooltipDownloadButtonPx);
  // Column for download button.
  control_columns->AddColumn(views::GridLayout::TRAILING,
                             views::GridLayout::CENTER, 1.0,
                             views::GridLayout::USE_PREF, 0, 0);
  layout->StartRow(views::GridLayout::kFixedSize, kDownloadRowColumnSetId);
  // "More info" tooltip; looks like (i).
  auto tooltip_icon = std::make_unique<views::TooltipIcon>(
      l10n_util::GetStringUTF16(IDS_BROWSER_SHARING_QR_CODE_DIALOG_TOOLTIP));
  tooltip_icon_ = layout->AddView(std::move(tooltip_icon));
  // Download button.
  std::unique_ptr<views::LabelButton> download_button =
      views::MdTextButton::CreateSecondaryUiButton(
          this, l10n_util::GetStringUTF16(
                    IDS_BROWSER_SHARING_QR_CODE_DIALOG_DOWNLOAD_BUTTON_LABEL));
  download_button->SetHorizontalAlignment(gfx::ALIGN_RIGHT);
  download_button_ = layout->AddView(std::move(download_button));
  // End controls row
}

void QRCodeGeneratorBubble::ContentsChanged(
    views::Textfield* sender,
    const base::string16& new_contents) {
  DCHECK_EQ(sender, textfield_url_);
  if (sender == textfield_url_) {
    url_ = GURL(base::UTF16ToUTF8(new_contents));
    UpdateQRContent();
  }
}

bool QRCodeGeneratorBubble::HandleKeyEvent(views::Textfield* sender,
                                           const ui::KeyEvent& key_event) {
  return false;
}

bool QRCodeGeneratorBubble::HandleMouseEvent(
    views::Textfield* sender,
    const ui::MouseEvent& mouse_event) {
  return false;
}

void QRCodeGeneratorBubble::ButtonPressed(views::Button* sender,
                                          const ui::Event& event) {
  DCHECK_EQ(sender, download_button_);
  if (sender == download_button_) {
    // TODO(skare): Send data: URL for QR code to download manager.
  }
}

}  // namespace qrcode_generator
