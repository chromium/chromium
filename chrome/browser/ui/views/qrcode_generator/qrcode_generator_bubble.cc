// Copyright 2019 The Chromium Authors
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
#include "chrome/browser/ui/color/chrome_color_id.h"
#include "chrome/browser/ui/qrcode_generator/qrcode_generator_bubble_controller.h"
#include "chrome/browser/ui/ui_features.h"
#include "chrome/browser/ui/views/chrome_layout_provider.h"
#include "chrome/browser/ui/views/chrome_typography.h"
#include "chrome/browser/ui/views/frame/browser_view.h"
#include "chrome/browser/ui/views/frame/top_container_view.h"
#include "chrome/browser/ui/views/sharing_hub/sharing_hub_bubble_util.h"
#include "chrome/grit/generated_resources.h"
#include "chrome/services/qrcode_generator/public/cpp/qrcode_generator_service.h"
#include "chrome/services/qrcode_generator/public/mojom/qrcode_generator.mojom.h"
#include "components/url_formatter/url_formatter.h"
#include "content/public/browser/download_manager.h"
#include "content/public/browser/download_request_utils.h"
#include "content/public/browser/web_contents.h"
#include "net/traffic_annotation/network_traffic_annotation.h"
#include "ui/base/l10n/l10n_util.h"
#include "ui/base/metadata/metadata_impl_macros.h"
#include "ui/base/theme_provider.h"
#include "ui/base/webui/web_ui_util.h"
#include "ui/events/event.h"
#include "ui/gfx/codec/png_codec.h"
#include "ui/gfx/image/image_skia_operations.h"
#include "ui/gfx/image/image_skia_rep.h"
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
#include "ui/views/layout/box_layout_view.h"
#include "ui/views/layout/flex_layout.h"
#include "ui/views/layout/layout_provider.h"
#include "ui/views/view.h"
#include "ui/views/widget/widget.h"

namespace {

constexpr int kPaddingTooltipDownloadButtonPx = 10;

// Calculates the height of the QR Code with padding.
constexpr gfx::Size GetQRCodeImageSize() {
  constexpr int kQRImageSizePx = 240;
  return gfx::Size(kQRImageSizePx, kQRImageSizePx);
}

constexpr bool IsSquare(gfx::Size size) {
  return size.width() == size.height();
}

gfx::ImageSkia CreateBackgroundImageSkia(const gfx::Size& size, SkColor color) {
  SkBitmap bitmap;
  bitmap.allocN32Pixels(size.width(), size.height());
  bitmap.eraseColor(color);
  return gfx::ImageSkia::CreateFromBitmap(bitmap, 1.0f);
}

}  // namespace

namespace qrcode_generator {

QRCodeGeneratorBubble::QRCodeGeneratorBubble(
    views::View* anchor_view,
    content::WebContents* web_contents,
    base::OnceClosure on_closing,
    base::OnceClosure on_back_button_pressed,
    const GURL& url)
    : LocationBarBubbleDelegateView(anchor_view, nullptr),
      url_(url),
      on_closing_(std::move(on_closing)),
      on_back_button_pressed_(std::move(on_back_button_pressed)),
      web_contents_(web_contents) {
  DCHECK(on_closing_);

  SetButtons(ui::DIALOG_BUTTON_NONE);
  SetTitle(IDS_BROWSER_SHARING_QR_CODE_DIALOG_TITLE);

  base::RecordAction(base::UserMetricsAction("SharingQRCode.DialogLaunched"));
}

QRCodeGeneratorBubble::~QRCodeGeneratorBubble() = default;

void QRCodeGeneratorBubble::Show() {
  textfield_url_->SetText(base::ASCIIToUTF16(url_.possibly_invalid_spec()));
  textfield_url_->SelectAll(false);
  UpdateQRContent();
  ShowForReason(USER_GESTURE);
}

void QRCodeGeneratorBubble::Hide() {
  if (on_closing_)
    std::move(on_closing_).Run();
  CloseBubble();
}

void QRCodeGeneratorBubble::OnThemeChanged() {
  LocationBarBubbleDelegateView::OnThemeChanged();

  const int border_radius = views::LayoutProvider::Get()->GetCornerRadiusMetric(
      views::Emphasis::kHigh);
  const auto* color_provider = GetColorProvider();
  qr_code_image_->SetBorder(views::CreateRoundedRectBorder(
      /*thickness=*/2, border_radius,
      color_provider->GetColor(kColorQrCodeBorder)));
  qr_code_image_->SetBackground(views::CreateRoundedRectBackground(
      color_provider->GetColor(kColorQrCodeBackground), border_radius, 2));
}

void QRCodeGeneratorBubble::UpdateQRContent() {
  mojom::GenerateQRCodeRequestPtr request = mojom::GenerateQRCodeRequest::New();
  request->data = url_.spec();
  request->should_render = true;
  request->center_image = mojom::CenterImage::CHROME_DINO;
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

  HideErrors(true);
  UpdateQRImage(AddQRCodeQuietZone(
      gfx::ImageSkia::CreateFrom1xBitmap(response->bitmap), response->data_size,
      GetColorProvider()->GetColor(kColorQrCodeBackground)));
}

void QRCodeGeneratorBubble::UpdateQRImage(gfx::ImageSkia qr_image) {
  qr_code_image_->SetImage(qr_image);
  const int border_radius = views::LayoutProvider::Get()->GetCornerRadiusMetric(
      views::Emphasis::kHigh);
  qr_code_image_->SetPreferredSize(GetQRCodeImageSize() +
                                   gfx::Size(border_radius, border_radius));
  qr_code_image_->SetVisible(true);
}

void QRCodeGeneratorBubble::DisplayPlaceholderImage() {
  UpdateQRImage(
      CreateBackgroundImageSkia(GetQRCodeImageSize(), SK_ColorTRANSPARENT));
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
  center_error_label_->SetPreferredSize(GetQRCodeImageSize());
  center_error_label_->SetVisible(true);
}

void QRCodeGeneratorBubble::HideErrors(bool enable_download_button) {
  ShrinkAndHideDisplay(center_error_label_);
  bottom_error_label_->SetVisible(false);
  download_button_->SetEnabled(enable_download_button);
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
  if (on_closing_)
    std::move(on_closing_).Run();
}

void QRCodeGeneratorBubble::Init() {
  // Requesting TEXT for trailing prevents extra padding at bottom of dialog.
  gfx::Insets insets =
      ChromeLayoutProvider::Get()->GetDialogInsetsForContentType(
          views::DialogContentType::kControl, views::DialogContentType::kText);
  set_margins(insets);

  // Add top-level Grid Layout manager for this dialog.
  auto* const layout = SetLayoutManager(std::make_unique<views::FlexLayout>());
  layout->SetOrientation(views::LayoutOrientation::kVertical);

  // QR Code image, with padding and border.
  using Alignment = views::ImageView::Alignment;
  auto qr_code_image = std::make_unique<views::ImageView>();
  qr_code_image->SetHorizontalAlignment(Alignment::kCenter);
  qr_code_image->SetVerticalAlignment(Alignment::kCenter);
  qr_code_image->SetImageSize(GetQRCodeImageSize());
  const int border_radius = views::LayoutProvider::Get()->GetCornerRadiusMetric(
      views::Emphasis::kHigh);
  qr_code_image->SetPreferredSize(GetQRCodeImageSize() +
                                  gfx::Size(border_radius, border_radius));
  qr_code_image->SetProperty(views::kCrossAxisAlignmentKey,
                             views::LayoutAlignment::kCenter);

  qr_code_image_ = AddChildView(std::move(qr_code_image));

  // Center error message.
  auto center_error_label = std::make_unique<views::Label>(
      l10n_util::GetStringUTF16(
          IDS_BROWSER_SHARING_QR_CODE_DIALOG_ERROR_UNKNOWN),
      views::style::CONTEXT_LABEL, views::style::STYLE_SECONDARY);
  center_error_label->SetHorizontalAlignment(gfx::ALIGN_CENTER);
  center_error_label->SetVerticalAlignment(gfx::ALIGN_MIDDLE);
  center_error_label_ = AddChildView(std::move(center_error_label));
  ShrinkAndHideDisplay(center_error_label_);

  // Text box to edit URL
  auto textfield_url = std::make_unique<views::Textfield>();
  textfield_url->SetAccessibleName(l10n_util::GetStringUTF16(
      IDS_BROWSER_SHARING_QR_CODE_DIALOG_URL_TEXTFIELD_ACCESSIBLE_NAME));
  textfield_url->SetText(
      base::ASCIIToUTF16(url_.spec()));  // TODO(skare): check
  textfield_url->set_controller(this);
  textfield_url->SetProperty(
      views::kMarginsKey,
      gfx::Insets::TLBR(ChromeLayoutProvider::Get()->GetDistanceMetric(
                            DISTANCE_UNRELATED_CONTROL_VERTICAL_LARGE),
                        0, 0, 0));
  int textfield_min_width = ChromeLayoutProvider::Get()->GetDistanceMetric(
                                views::DISTANCE_BUBBLE_PREFERRED_WIDTH) -
                            insets.left() - insets.right();
  textfield_url->SetPreferredSize(gfx::Size(
      textfield_min_width, textfield_url->GetPreferredSize().height()));
  textfield_url_ = AddChildView(std::move(textfield_url));

  // Lower error message.
  // User-facing limit rounded down to 250 characters for readability.
  auto bottom_error_label = std::make_unique<views::Label>(
      l10n_util::GetStringFUTF16Int(
          IDS_BROWSER_SHARING_QR_CODE_DIALOG_ERROR_TOO_LONG, 250),
      views::style::CONTEXT_LABEL, views::style::STYLE_SECONDARY);
  bottom_error_label->SetHorizontalAlignment(gfx::ALIGN_LEFT);
  bottom_error_label->SetVisible(false);
  auto* bottom_error_container = AddChildView(std::make_unique<views::View>());
  bottom_error_container->SetUseDefaultFillLayout(true);
  bottom_error_label_ =
      bottom_error_container->AddChildView(std::move(bottom_error_label));
  // Updating the image requires both error labels to be initialized.
  DisplayPlaceholderImage();

  // Padding - larger between controls and action buttons.
  bottom_error_container->SetProperty(
      views::kMarginsKey,
      gfx::Insets::TLBR(
          0, 0,
          ChromeLayoutProvider::Get()->GetDistanceMetric(
              views::DISTANCE_DIALOG_CONTENT_MARGIN_BOTTOM_CONTROL) -
              bottom_error_label_->GetPreferredSize().height(),
          0));

  auto* button_container =
      AddChildView(std::make_unique<views::BoxLayoutView>());
  button_container->SetCrossAxisAlignment(
      views::BoxLayout::CrossAxisAlignment::kCenter);

  // "More info" tooltip; looks like (i).
  auto tooltip_icon = std::make_unique<views::TooltipIcon>(
      l10n_util::GetStringUTF16(IDS_BROWSER_SHARING_QR_CODE_DIALOG_TOOLTIP));
  tooltip_icon->set_bubble_width(ChromeLayoutProvider::Get()->GetDistanceMetric(
      views::DISTANCE_BUBBLE_PREFERRED_WIDTH));
  tooltip_icon->set_anchor_point_arrow(views::BubbleBorder::Arrow::TOP_LEFT);
  tooltip_icon->SetProperty(
      views::kMarginsKey,
      gfx::Insets::TLBR(0, 0, 0, kPaddingTooltipDownloadButtonPx));
  tooltip_icon_ = button_container->AddChildView(std::move(tooltip_icon));

  auto* flex = button_container->AddChildView(std::make_unique<views::View>());
  button_container->SetFlexForView(flex, 1);

  // Download button.
  download_button_ =
      button_container->AddChildView(std::make_unique<views::MdTextButton>(
          base::BindRepeating(&QRCodeGeneratorBubble::DownloadButtonPressed,
                              base::Unretained(this)),
          l10n_util::GetStringUTF16(
              IDS_BROWSER_SHARING_QR_CODE_DIALOG_DOWNLOAD_BUTTON_LABEL)));
  download_button_->SetHorizontalAlignment(gfx::ALIGN_CENTER);
  // End controls row

  // Initialize Service
  if (!qr_code_service_remote_)
    qr_code_service_remote_ = qrcode_generator::LaunchQRCodeGeneratorService();
}

void QRCodeGeneratorBubble::AddedToWidget() {
  if (!on_back_button_pressed_)
    return;

  // Adding a title view will replace the default title.
  GetBubbleFrameView()->SetTitleView(
      std::make_unique<sharing_hub::TitleWithBackButtonView>(
          base::BindRepeating(&QRCodeGeneratorBubble::BackButtonPressed,
                              base::Unretained(this)),
          GetWindowTitle()));
}

void QRCodeGeneratorBubble::ContentsChanged(
    views::Textfield* sender,
    const std::u16string& new_contents) {
  DCHECK_EQ(sender, textfield_url_);
  if (sender == textfield_url_) {
    if (bottom_error_label_->GetVisible())
      HideErrors(false);
    GURL new_url(new_contents);
    if (!new_url.is_valid()) {
      textfield_url_->SetText(base::UTF8ToUTF16(url_.spec()));
      return;
    }
    url_ = new_url;
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

// Given a square |image| and a size in QR code tiles (*not* in pixels or
// dips) |qr_size|, produce a new image that contains |image| with the
// mandatory 4 tiles worth of white padding around the original image.
// static
gfx::ImageSkia QRCodeGeneratorBubble::AddQRCodeQuietZone(
    const gfx::ImageSkia& image,
    const gfx::Size& qr_size,
    SkColor background_color) {
  const gfx::Size image_size(image.width(), image.height());

  DCHECK(IsSquare(image_size));
  DCHECK(IsSquare(qr_size));

  // Set by the QR code specification. We need to leave this many tiles blank on
  // *each side* of the image.
  const int kQuietZoneSizeTiles = 4;
  const int tile_size = image.width() / qr_size.width();
  const gfx::Size background_size =
      image_size + gfx::Size(kQuietZoneSizeTiles * tile_size * 2,
                             kQuietZoneSizeTiles * tile_size * 2);

  auto final_image = gfx::ImageSkiaOperations::CreateSuperimposedImage(
      CreateBackgroundImageSkia(background_size, background_color), image);
  DCHECK(IsSquare(gfx::Size(final_image.width(), final_image.height())));
  return final_image;
}

void QRCodeGeneratorBubble::SetQRCodeServiceForTesting(
    mojo::Remote<mojom::QRCodeGeneratorService>&& remote) {
  qr_code_service_remote_ = std::move(remote);
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
      browser->profile()->GetDownloadManager();
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

void QRCodeGeneratorBubble::BackButtonPressed() {
  Hide();

  DCHECK(on_back_button_pressed_);
  std::move(on_back_button_pressed_).Run();
}

BEGIN_METADATA(QRCodeGeneratorBubble, LocationBarBubbleDelegateView)
END_METADATA

}  // namespace qrcode_generator
