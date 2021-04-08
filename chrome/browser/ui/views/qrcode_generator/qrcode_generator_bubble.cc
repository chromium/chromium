// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/views/qrcode_generator/qrcode_generator_bubble.h"

#include "base/base64.h"
#include "base/metrics/user_metrics.h"
#include "base/strings/strcat.h"
#include "base/strings/utf_string_conversions.h"
#include "chrome/browser/sharing/features.h"
#include "chrome/browser/themes/theme_properties.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/browser/ui/browser_dialogs.h"
#include "chrome/browser/ui/browser_finder.h"
#include "chrome/browser/ui/qrcode_generator/qrcode_generator_bubble_controller.h"
#include "chrome/browser/ui/ui_features.h"
#include "chrome/browser/ui/views/chrome_layout_provider.h"
#include "chrome/browser/ui/views/chrome_typography.h"
#include "chrome/browser/ui/views/frame/browser_view.h"
#include "chrome/browser/ui/views/frame/top_container_view.h"
#include "chrome/grit/generated_resources.h"
#include "components/url_formatter/url_formatter.h"
#include "content/public/browser/download_manager.h"
#include "content/public/browser/download_request_utils.h"
#include "content/public/browser/web_contents.h"
#include "net/traffic_annotation/network_traffic_annotation.h"
#include "ui/base/l10n/l10n_util.h"
#include "ui/base/theme_provider.h"
#include "ui/base/webui/web_ui_util.h"
#include "ui/events/event.h"
#include "ui/gfx/codec/png_codec.h"
#include "ui/gfx/image/image_skia_source.h"
#include "ui/native_theme/native_theme.h"
#include "ui/resources/grit/ui_resources.h"
#include "ui/views/border.h"
#include "ui/views/bubble/bubble_frame_view.h"
#include "ui/views/bubble/tooltip_icon.h"
#include "ui/views/controls/button/label_button.h"
#include "ui/views/controls/button/md_text_button.h"
#include "ui/views/controls/image_view.h"
#include "ui/views/controls/label.h"
#include "ui/views/controls/textfield/textfield.h"
#include "ui/views/layout/grid_layout.h"
#include "ui/views/layout/layout_provider.h"
#include "ui/views/metadata/metadata_impl_macros.h"
#include "ui/views/view.h"
#include "ui/views/widget/widget.h"

namespace {

// Rendered QR Code size, pixels.
constexpr int kQRImageSizePx = 200;
constexpr int kPaddingTooltipDownloadButtonPx = 10;
// Padding around the QR code. Ensures we can scan when using dark themes.
constexpr int kQRPaddingPx = 40;

// Calculates preview image dimensions.
constexpr gfx::Size GetQRImageSize() {
  return gfx::Size(kQRImageSizePx, kQRImageSizePx);
}

// Calculates the height of the QR Code with padding.
constexpr gfx::Size GetPreferredQRCodeImageSize() {
  return gfx::Size(kQRImageSizePx + kQRPaddingPx,
                   kQRImageSizePx + kQRPaddingPx);
}

// Renders a solid square of color {r, g, b} at 100% alpha.
gfx::ImageSkia GetPlaceholderImageSkia(const SkColor color) {
  SkBitmap bitmap;
  bitmap.allocN32Pixels(kQRImageSizePx, kQRImageSizePx);
  bitmap.eraseARGB(0xFF, 0xFF, 0xFF, 0xFF);
  bitmap.eraseColor(color);
  return gfx::ImageSkia::CreateFromBitmap(bitmap, 1.0f);
}

// Adds a new small vertical padding row to the current bottom of |layout|.
void AddSmallPaddingRow(views::GridLayout* layout) {
  layout->AddPaddingRow(views::GridLayout::kFixedSize,
                        ChromeLayoutProvider::Get()->GetDistanceMetric(
                            DISTANCE_UNRELATED_CONTROL_VERTICAL_LARGE));
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
      controller_(controller),
      web_contents_(web_contents) {
  DCHECK(controller);

  SetButtons(ui::DIALOG_BUTTON_NONE);
  SetTitle(IDS_BROWSER_SHARING_QR_CODE_DIALOG_TITLE);

  base::RecordAction(base::UserMetricsAction("SharingQRCode.DialogLaunched"));
}

QRCodeGeneratorBubble::~QRCodeGeneratorBubble() = default;

void QRCodeGeneratorBubble::Show() {
  chrome::RecordDialogCreation(chrome::DialogIdentifier::QR_CODE_GENERATOR);
  textfield_url_->SetText(base::ASCIIToUTF16(url_.possibly_invalid_spec()));
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
  if (textfield_url_->GetText().empty()) {
    DisplayPlaceholderImage();
    return;
  }

  mojom::GenerateQRCodeRequestPtr request = mojom::GenerateQRCodeRequest::New();
  request->data = base::UTF16ToASCII(textfield_url_->GetText());
  request->should_render = true;
  request->render_dino = true;
  request->render_module_style = mojom::ModuleStyle::CIRCLES;
  request->render_locator_style = mojom::LocatorStyle::ROUNDED;

  mojom::QRCodeGeneratorService* generator = qr_code_service_remote_.get();
  // Rationale for Unretained(): Closing dialog closes the communication
  // channel; callback will not run.
  auto callback = base::BindOnce(
      &QRCodeGeneratorBubble::OnCodeGeneratorResponse, base::Unretained(this));
  generator->GenerateQRCode(std::move(request), std::move(callback));
}

void QRCodeGeneratorBubble::OnCodeGeneratorResponse(
    const mojom::GenerateQRCodeResponsePtr response) {
  if (response->error_code != mojom::QRCodeGeneratorError::NONE) {
    DisplayError(response->error_code);
    return;
  }

  ShrinkAndHideDisplay(center_error_label_);
  bottom_error_label_->SetVisible(false);
  download_button_->SetEnabled(true);
  gfx::ImageSkia image = gfx::ImageSkia::CreateFrom1xBitmap(response->bitmap);
  UpdateQRImage(image);
}

void QRCodeGeneratorBubble::UpdateQRImage(gfx::ImageSkia qr_image) {
  qr_code_image_->SetImage(qr_image);
  qr_code_image_->SetImageSize(GetQRImageSize());
  qr_code_image_->SetPreferredSize(GetPreferredQRCodeImageSize());
  qr_code_image_->SetVisible(true);
}

void QRCodeGeneratorBubble::DisplayPlaceholderImage() {
  UpdateQRImage(GetPlaceholderImageSkia(gfx::kGoogleGrey100));
}

void QRCodeGeneratorBubble::DisplayError(mojom::QRCodeGeneratorError error) {
  download_button_->SetEnabled(false);
  if (error == mojom::QRCodeGeneratorError::INPUT_TOO_LONG) {
    ShrinkAndHideDisplay(center_error_label_);
    DisplayPlaceholderImage();
    bottom_error_label_->SetVisible(true);
    return;
  }
  ShrinkAndHideDisplay(qr_code_image_);
  bottom_error_label_->SetVisible(false);
  center_error_label_->SetPreferredSize(GetPreferredQRCodeImageSize());
  center_error_label_->SetVisible(true);
}

void QRCodeGeneratorBubble::ShrinkAndHideDisplay(views::View* view) {
  view->SetPreferredSize(gfx::Size(0, 0));
  view->SetVisible(false);
}

views::View* QRCodeGeneratorBubble::GetInitiallyFocusedView() {
  return textfield_url_;
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

void QRCodeGeneratorBubble::Init() {
  // Requesting TEXT for trailing prevents extra padding at bottom of dialog.
  gfx::Insets insets =
      ChromeLayoutProvider::Get()->GetDialogInsetsForContentType(views::CONTROL,
                                                                 views::TEXT);
  set_margins(insets);

  // Internal IDs for column layout; no effect on UI.
  constexpr int kQRImageColumnSetId = 0;
  constexpr int kCenterErrorLabelColumnSetId = 1;
  constexpr int kTextFieldColumnSetId = 2;
  constexpr int kBottomErrorLabelColumnSetId = 3;
  constexpr int kDownloadRowColumnSetId = 4;

  // Add top-level Grid Layout manager for this dialog.
  views::GridLayout* const layout =
      SetLayoutManager(std::make_unique<views::GridLayout>());

  // QR Code image, with padding and border.
  views::ColumnSet* column_set_qr_image =
      layout->AddColumnSet(kQRImageColumnSetId);
  column_set_qr_image->AddColumn(
      views::GridLayout::CENTER,  // Center horizontally, do not resize.
      views::GridLayout::CENTER,  // Align center vertically, do not resize.
      1.0, views::GridLayout::ColumnSize::kUsePreferred, 0, 0);
  using Alignment = views::ImageView::Alignment;
  auto qr_code_image = std::make_unique<views::ImageView>();
  const int border_radius = views::LayoutProvider::Get()->GetCornerRadiusMetric(
      views::Emphasis::kHigh);
  qr_code_image->SetBorder(views::CreateRoundedRectBorder(
      /*thickness=*/2, border_radius, gfx::kGoogleGrey200));
  qr_code_image->SetHorizontalAlignment(Alignment::kCenter);
  qr_code_image->SetVerticalAlignment(Alignment::kCenter);
  qr_code_image->SetImageSize(GetQRImageSize());
  qr_code_image->SetPreferredSize(GetPreferredQRCodeImageSize());
  qr_code_image->SetBackground(
      views::CreateRoundedRectBackground(SK_ColorWHITE, border_radius));

  layout->StartRow(views::GridLayout::kFixedSize, kQRImageColumnSetId);
  qr_code_image_ = layout->AddView(std::move(qr_code_image));

  // Center error message.
  views::ColumnSet* column_set_center_error_label =
      layout->AddColumnSet(kCenterErrorLabelColumnSetId);
  column_set_center_error_label->AddColumn(
      views::GridLayout::CENTER, views::GridLayout::CENTER, 1.0,
      views::GridLayout::ColumnSize::kUsePreferred, 0, 0);
  auto center_error_label = std::make_unique<views::Label>();
  center_error_label->SetHorizontalAlignment(gfx::ALIGN_CENTER);
  center_error_label->SetVerticalAlignment(gfx::ALIGN_MIDDLE);
  center_error_label->SetEnabledColor(
      center_error_label->GetNativeTheme()->GetSystemColor(
          ui::NativeTheme::kColorId_LabelSecondaryColor));
  center_error_label->SetText(l10n_util::GetStringUTF16(
      IDS_BROWSER_SHARING_QR_CODE_DIALOG_ERROR_UNKNOWN));
  layout->StartRow(views::GridLayout::kFixedSize, kCenterErrorLabelColumnSetId);
  center_error_label_ = layout->AddView(std::move(center_error_label));
  ShrinkAndHideDisplay(center_error_label_);

  // Padding
  AddSmallPaddingRow(layout);

  // Text box to edit URL
  views::ColumnSet* column_set_textfield =
      layout->AddColumnSet(kTextFieldColumnSetId);
  int textfield_min_width = ChromeLayoutProvider::Get()->GetDistanceMetric(
                                views::DISTANCE_BUBBLE_PREFERRED_WIDTH) -
                            insets.left() - insets.right();
  column_set_textfield->AddColumn(
      views::GridLayout::FILL,    // Fill text field horizontally.
      views::GridLayout::CENTER,  // Align center vertically, do not resize.
      1.0, views::GridLayout::ColumnSize::kUsePreferred, 0,
      textfield_min_width);
  auto textfield_url = std::make_unique<views::Textfield>();
  textfield_url->SetAccessibleName(l10n_util::GetStringUTF16(
      IDS_BROWSER_SHARING_QR_CODE_DIALOG_URL_TEXTFIELD_ACCESSIBLE_NAME));
  textfield_url->SetText(
      base::ASCIIToUTF16(url_.spec()));  // TODO(skare): check
  textfield_url->set_controller(this);
  layout->StartRow(views::GridLayout::kFixedSize, kTextFieldColumnSetId);
  textfield_url_ = layout->AddView(std::move(textfield_url));

  // Lower error message.
  views::ColumnSet* column_set_bottom_error_label =
      layout->AddColumnSet(kBottomErrorLabelColumnSetId);
  column_set_bottom_error_label->AddColumn(
      views::GridLayout::FILL, views::GridLayout::CENTER, 1.0,
      views::GridLayout::ColumnSize::kUsePreferred, 0, 0);
  auto bottom_error_label = std::make_unique<views::Label>();
  bottom_error_label->SetHorizontalAlignment(gfx::ALIGN_LEFT);
  bottom_error_label->SetEnabledColor(
      bottom_error_label->GetNativeTheme()->GetSystemColor(
          ui::NativeTheme::kColorId_LabelSecondaryColor));
  // User-facing limit rounded down for readability.
  int maxUrlLength = base::GetFieldTrialParamByFeatureAsInt(
      kSharingQRCodeGenerator, "max_url_length", 250);
  bottom_error_label->SetText(l10n_util::GetStringFUTF16Int(
      IDS_BROWSER_SHARING_QR_CODE_DIALOG_ERROR_TOO_LONG, maxUrlLength));
  bottom_error_label->SetVisible(false);
  layout->StartRow(views::GridLayout::kFixedSize, kBottomErrorLabelColumnSetId);
  bottom_error_label_ = layout->AddView(std::move(bottom_error_label));
  // Updating the image requires both error labels to be initialized.
  DisplayPlaceholderImage();

  // Padding - larger between controls and action buttons.
  layout->AddPaddingRow(
      views::GridLayout::kFixedSize,
      ChromeLayoutProvider::Get()->GetDistanceMetric(
          views::DISTANCE_DIALOG_CONTENT_MARGIN_BOTTOM_CONTROL) -
          bottom_error_label_->GetPreferredSize().height());

  // Controls row: tooltip and download button.
  views::ColumnSet* control_columns =
      layout->AddColumnSet(kDownloadRowColumnSetId);
  // Column for tooltip.
  control_columns->AddColumn(
      views::GridLayout::LEADING,  // View is aligned to leading edge, not
                                   // resized.
      views::GridLayout::CENTER,   // View moves to center of vertical space.
      1.0,                         // This column has a resize weight of 1.
      views::GridLayout::ColumnSize::kUsePreferred,  // Use the preferred size
                                                     // of the view.
      0,                                             // Ignored for USE_PREF.
      0);                                            // Minimum width of 0.
  // Spacing between tooltip and download button.
  control_columns->AddPaddingColumn(views::GridLayout::kFixedSize,
                                    kPaddingTooltipDownloadButtonPx);
  // Column for download button.
  control_columns->AddColumn(
      views::GridLayout::TRAILING, views::GridLayout::CENTER, 1.0,
      views::GridLayout::ColumnSize::kUsePreferred, 0, 0);
  layout->StartRow(views::GridLayout::kFixedSize, kDownloadRowColumnSetId);

  // "More info" tooltip; looks like (i).
  auto tooltip_icon = std::make_unique<views::TooltipIcon>(
      l10n_util::GetStringUTF16(IDS_BROWSER_SHARING_QR_CODE_DIALOG_TOOLTIP));
  tooltip_icon->set_bubble_width(ChromeLayoutProvider::Get()->GetDistanceMetric(
      views::DISTANCE_BUBBLE_PREFERRED_WIDTH));
  tooltip_icon->set_anchor_point_arrow(
      views::BubbleBorder::Arrow::BOTTOM_RIGHT);
  tooltip_icon_ = layout->AddView(std::move(tooltip_icon));

  // Download button.
  download_button_ = layout->AddView(std::make_unique<views::MdTextButton>(
      base::BindRepeating(&QRCodeGeneratorBubble::DownloadButtonPressed,
                          base::Unretained(this)),
      l10n_util::GetStringUTF16(
          IDS_BROWSER_SHARING_QR_CODE_DIALOG_DOWNLOAD_BUTTON_LABEL)));
  download_button_->SetHorizontalAlignment(gfx::ALIGN_RIGHT);
  // End controls row

  // Initialize Service
  qr_code_service_remote_ = qrcode_generator::LaunchQRCodeGeneratorService();
}

void QRCodeGeneratorBubble::ContentsChanged(
    views::Textfield* sender,
    const std::u16string& new_contents) {
  DCHECK_EQ(sender, textfield_url_);
  if (sender == textfield_url_) {
    url_ = GURL(base::UTF16ToUTF8(new_contents));
    UpdateQRContent();

    static bool first_edit = true;
    if (first_edit) {
      base::RecordAction(
          base::UserMetricsAction("SharingQRCode.EditTextField"));
      first_edit = false;
    }
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

/*static*/
const std::u16string QRCodeGeneratorBubble::GetQRCodeFilenameForURL(
    const GURL& url) {
  if (!url.has_host() || url.HostIsIPAddress())
    return u"qrcode_chrome.png";

  return base::ASCIIToUTF16(base::StrCat({"qrcode_", url.host(), ".png"}));
}

void QRCodeGeneratorBubble::DownloadButtonPressed() {
  const gfx::ImageSkia& image_ref = qr_code_image_->GetImage();
  // Returns closest scaling to parameter (1.0).
  // Should be exact since we generated the bitmap.
  const gfx::ImageSkiaRep& image_rep = image_ref.GetRepresentation(1.0f);
  const SkBitmap& bitmap = image_rep.GetBitmap();
  const GURL data_url = GURL(webui::GetBitmapDataUrl(bitmap));

  Browser* browser = chrome::FindBrowserWithWebContents(web_contents_);
  content::DownloadManager* download_manager =
      content::BrowserContext::GetDownloadManager(browser->profile());
  net::NetworkTrafficAnnotationTag traffic_annotation =
      net::DefineNetworkTrafficAnnotation("qr_code_save", R"(
      semantics {
        sender: "QR Code Generator"
        description:
          "The user may generate a QR code linking to the current page or "
          "image. This bubble view has a download button to save the generated "
          "image to disk. "
          "The image is generated via a Mojo service, but locally, so this "
          "request never contacts the network. "
        trigger: "User clicks 'download' in a bubble view launched from the "
          "omnibox, right-click menu, or share dialog."
        data: "QR Code image based on the current page's URL."
        destination: LOCAL
      }
      policy {
        cookies_allowed: NO
        setting:
          "No user-visible setting for this feature. Experiment and rollout to "
          "be coordinated via Finch. Access point to be combined with other "
          "sharing features later in 2020."
        policy_exception_justification:
          "Not implemented, considered not required."
      })");
  std::unique_ptr<download::DownloadUrlParameters> params =
      content::DownloadRequestUtils::CreateDownloadForWebContentsMainFrame(
          web_contents_, data_url, traffic_annotation);
  // Suggest a name incorporating the hostname. Protocol, TLD, etc are
  // not taken into consideration. Duplicate names get automatic suffixes.
  params->set_suggested_name(GetQRCodeFilenameForURL(url_));
  download_manager->DownloadUrl(std::move(params));
  base::RecordAction(base::UserMetricsAction("SharingQRCode.DownloadQRCode"));
}

BEGIN_METADATA(QRCodeGeneratorBubble, LocationBarBubbleDelegateView)
END_METADATA

}  // namespace qrcode_generator
