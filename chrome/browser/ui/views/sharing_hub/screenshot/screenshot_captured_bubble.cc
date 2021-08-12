// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/views/sharing_hub/screenshot/screenshot_captured_bubble.h"

#include "base/strings/strcat.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/browser/ui/browser_finder.h"
#include "chrome/browser/ui/browser_navigator.h"
#include "chrome/browser/ui/browser_navigator_params.h"
#include "chrome/browser/ui/views/chrome_layout_provider.h"
#include "chrome/common/webui_url_constants.h"
#include "chrome/grit/generated_resources.h"
#include "content/public/browser/download_manager.h"
#include "content/public/browser/download_request_utils.h"
#include "content/public/browser/web_contents.h"
#include "ui/base/l10n/l10n_util.h"
#include "ui/base/metadata/metadata_impl_macros.h"
#include "ui/base/webui/web_ui_util.h"
#include "ui/resources/grit/ui_resources.h"
#include "ui/views/border.h"
#include "ui/views/controls/button/label_button.h"
#include "ui/views/controls/button/md_text_button.h"
#include "ui/views/controls/image_view.h"
#include "ui/views/layout/grid_layout.h"
#include "ui/views/layout/layout_provider.h"
#include "ui/views/view.h"

namespace {

// Rendered image size, pixels.
constexpr int kImageWidthPx = 336;
constexpr int kImageHeightPx = 252;

// Calculates the size of the image with padding.
constexpr gfx::Size GetImageSize() {
  // TODO(kmilka): Freeform capture will lead to variable aspect ratio, we
  // should handle this gracefully.
  return gfx::Size(kImageWidthPx, kImageHeightPx);
}

// Adds a new small vertical padding row to the current bottom of |layout|.
void AddSmallPaddingRow(views::GridLayout* layout) {
  layout->AddPaddingRow(views::GridLayout::kFixedSize,
                        ChromeLayoutProvider::Get()->GetDistanceMetric(
                            DISTANCE_UNRELATED_CONTROL_VERTICAL_LARGE));
}

}  // namespace

namespace sharing_hub {

ScreenshotCapturedBubble::ScreenshotCapturedBubble(
    views::View* anchor_view,
    content::WebContents* web_contents,
    const gfx::Image& image,
    Profile* profile,
    base::OnceCallback<void(NavigateParams*)> edit_callback)
    : LocationBarBubbleDelegateView(anchor_view, nullptr),
      image_(image),
      web_contents_(web_contents),
      profile_(profile),
      edit_callback_(std::move(edit_callback)) {
  SetButtons(ui::DIALOG_BUTTON_NONE);
  SetTitle(IDS_BROWSER_SHARING_SCREENSHOT_POST_CAPTURE_TITLE);
}

ScreenshotCapturedBubble::~ScreenshotCapturedBubble() = default;

void ScreenshotCapturedBubble::Show() {
  ShowForReason(USER_GESTURE);
}

views::View* ScreenshotCapturedBubble::GetInitiallyFocusedView() {
  return download_button_;
}

bool ScreenshotCapturedBubble::ShouldShowCloseButton() const {
  return true;
}

void ScreenshotCapturedBubble::WindowClosing() {
  NOTIMPLEMENTED();
}

void ScreenshotCapturedBubble::Init() {
  // Requesting TEXT for trailing prevents extra padding at bottom of dialog.
  gfx::Insets insets =
      ChromeLayoutProvider::Get()->GetDialogInsetsForContentType(
          views::DialogContentType::kControl, views::DialogContentType::kText);
  set_margins(insets);

  // Internal IDs for column layout; no effect on UI.
  constexpr int kImageColumnSetId = 0;
  constexpr int kDownloadRowColumnSetId = 1;

  // Add top-level Grid Layout manager for this dialog.
  views::GridLayout* const layout =
      SetLayoutManager(std::make_unique<views::GridLayout>());

  // Captured image, with padding and border.
  views::ColumnSet* column_set_image = layout->AddColumnSet(kImageColumnSetId);
  column_set_image->AddColumn(
      views::GridLayout::CENTER,  // Center horizontally, do not resize.
      views::GridLayout::CENTER,  // Align center vertically, do not resize.
      1.0, views::GridLayout::ColumnSize::kUsePreferred, 0, 0);
  using Alignment = views::ImageView::Alignment;
  auto image_view = std::make_unique<views::ImageView>();
  const int border_radius = views::LayoutProvider::Get()->GetCornerRadiusMetric(
      views::Emphasis::kHigh);
  image_view->SetBorder(views::CreateRoundedRectBorder(
      /*thickness=*/2, border_radius, gfx::kGoogleGrey200));
  image_view->SetHorizontalAlignment(Alignment::kCenter);
  image_view->SetVerticalAlignment(Alignment::kCenter);
  image_view->SetImageSize(GetImageSize());
  image_view->SetPreferredSize(GetImageSize() +
                               gfx::Size(border_radius, border_radius));
  image_view->SetBackground(
      views::CreateRoundedRectBackground(SK_ColorWHITE, border_radius));

  layout->StartRow(views::GridLayout::kFixedSize, kImageColumnSetId);

  // Image. TODO(kmilka): Use a scaled thumbnail here?
  image_view->SetImage(image_.ToImageSkia());
  image_view->SetVisible(true);
  image_view_ = layout->AddView(std::move(image_view));

  // Edit button.
  auto edit_button = std::make_unique<views::MdTextButton>(
      base::BindRepeating(&ScreenshotCapturedBubble::EditButtonPressed,
                          base::Unretained(this)),
      l10n_util::GetStringUTF16(
          IDS_BROWSER_SHARING_SCREENSHOT_DIALOG_EDIT_BUTTON_LABEL));
  edit_button->SetHorizontalAlignment(gfx::ALIGN_LEFT);

  // Download button.
  auto download_button = std::make_unique<views::MdTextButton>(
      base::BindRepeating(&ScreenshotCapturedBubble::DownloadButtonPressed,
                          base::Unretained(this)),
      l10n_util::GetStringUTF16(
          IDS_BROWSER_SHARING_SCREENSHOT_DIALOG_DOWNLOAD_BUTTON_LABEL));
  download_button->SetHorizontalAlignment(gfx::ALIGN_RIGHT);
  download_button->SetProminent(true);

  // Padding
  AddSmallPaddingRow(layout);

  // Controls row: edit, share, and download button.
  views::ColumnSet* control_columns =
      layout->AddColumnSet(kDownloadRowColumnSetId);
  // Column for edit button.
  control_columns->AddColumn(
      views::GridLayout::LEADING, views::GridLayout::CENTER, 1.0,
      views::GridLayout::ColumnSize::kUsePreferred, 0, 0);
  layout->StartRow(views::GridLayout::kFixedSize, kDownloadRowColumnSetId);

  int kPaddingEditShareButtonPx =
      kImageWidthPx - edit_button->CalculatePreferredSize().width() -
      download_button->CalculatePreferredSize().width();
  // Spacing between the edit and share buttons.
  control_columns->AddPaddingColumn(views::GridLayout::kFixedSize,
                                    kPaddingEditShareButtonPx);

  // Column for download button
  control_columns->AddColumn(
      views::GridLayout::TRAILING, views::GridLayout::CENTER, 1.0,
      views::GridLayout::ColumnSize::kUsePreferred, 0, 0);
  layout->StartRow(views::GridLayout::kFixedSize, kDownloadRowColumnSetId);

  edit_button_ = layout->AddView(std::move(edit_button));
  download_button_ = layout->AddView(std::move(download_button));
  // End controls row
}

/*static*/
const std::u16string ScreenshotCapturedBubble::GetFilenameForURL(
    const GURL& url) {
  if (!url.has_host() || url.HostIsIPAddress())
    return u"chrome_screenshot.png";

  return base::ASCIIToUTF16(
      base::StrCat({"chrome_screenshot_", url.host(), ".png"}));
}

void ScreenshotCapturedBubble::DownloadButtonPressed() {
  // Returns closest scaling to parameter (1.0).
  const gfx::ImageSkia& image_ref = image_view_->GetImage();
  const gfx::ImageSkiaRep& image_rep = image_ref.GetRepresentation(1.0f);
  const SkBitmap& bitmap = image_rep.GetBitmap();
  const GURL data_url = GURL(webui::GetBitmapDataUrl(bitmap));

  Browser* browser = chrome::FindBrowserWithWebContents(web_contents_);
  content::DownloadManager* download_manager =
      browser->profile()->GetDownloadManager();
  // TODO(crbug.com/1186839): Update the annotation's |setting| and
  // |chrome_policy| fields once the Sharing Hub is landed.
  net::NetworkTrafficAnnotationTag traffic_annotation =
      net::DefineNetworkTrafficAnnotation("desktop_screenshot_save", R"(
      semantics {
        sender: "Desktop Screenshots"
        description:
          "The user may capture a selection of the current page. This bubble "
          "view has a download button to save the generated image to disk. "
        trigger: "User clicks 'download' in a bubble view launched from the "
          "omnibox after the 'Screenshot' option is selected in the sharing "
          "hub and a selection is made on the page. "
        data: "A capture of a portion of the current webpage."
        destination: LOCAL
      }
      policy {
        cookies_allowed: NO
        setting:
          "No user-visible setting for this feature. Experiment and rollout to "
          "be coordinated via Chrome Variations."
        policy_exception_justification:
          "Not implemented, considered not required."
      })");
  std::unique_ptr<download::DownloadUrlParameters> params =
      content::DownloadRequestUtils::CreateDownloadForWebContentsMainFrame(
          web_contents_, data_url, traffic_annotation);
  // Suggest a name incorporating the hostname. Protocol, TLD, etc are
  // not taken into consideration. Duplicate names get automatic suffixes.
  params->set_suggested_name(
      GetFilenameForURL(web_contents_->GetLastCommittedURL()));
  download_manager->DownloadUrl(std::move(params));
}

void ScreenshotCapturedBubble::EditButtonPressed() {
  GURL url(chrome::kChromeUIImageEditorURL);
  NavigateParams params(profile_, url, ui::PAGE_TRANSITION_LINK);
  params.disposition = WindowOpenDisposition::NEW_FOREGROUND_TAB;
  params.window_action = NavigateParams::SHOW_WINDOW;
  std::move(edit_callback_).Run(&params);
}

BEGIN_METADATA(ScreenshotCapturedBubble, LocationBarBubbleDelegateView)
END_METADATA

}  // namespace sharing_hub
