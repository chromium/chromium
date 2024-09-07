// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/views/sharing_hub/screenshot/screenshot_captured_bubble.h"

#include <memory>
#include <utility>
#include <vector>

#include "base/containers/span.h"
#include "base/feature_list.h"
#include "base/files/file_path.h"
#include "base/files/file_util.h"
#include "base/functional/bind.h"
#include "base/metrics/user_metrics.h"
#include "base/strings/strcat.h"
#include "base/task/thread_pool.h"
#include "base/threading/thread_restrictions.h"
#include "chrome/browser/image_editor/image_editor_component_info.h"
#include "chrome/browser/image_editor/screenshot_flow.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/browser/ui/browser_finder.h"
#include "chrome/browser/ui/browser_navigator.h"
#include "chrome/browser/ui/browser_navigator_params.h"
#include "chrome/browser/ui/color/chrome_color_id.h"
#include "chrome/browser/ui/views/chrome_layout_provider.h"
#include "chrome/common/webui_url_constants.h"
#include "chrome/grit/generated_resources.h"
#include "content/public/browser/download_manager.h"
#include "content/public/browser/download_request_utils.h"
#include "content/public/browser/web_contents.h"
#include "ui/base/l10n/l10n_util.h"
#include "ui/base/metadata/metadata_impl_macros.h"
#include "ui/base/mojom/dialog_button.mojom.h"
#include "ui/base/webui/web_ui_util.h"
#include "ui/gfx/codec/png_codec.h"
#include "ui/gfx/image/image_skia_rep.h"
#include "ui/resources/grit/ui_resources.h"
#include "ui/views/border.h"
#include "ui/views/controls/button/label_button.h"
#include "ui/views/controls/button/md_text_button.h"
#include "ui/views/controls/image_view.h"
#include "ui/views/layout/box_layout.h"
#include "ui/views/layout/table_layout_view.h"
#include "ui/views/view.h"

namespace {

// Rendered image size, pixels.
constexpr int kImageWidthPx = 336;
constexpr int kImageHeightPx = 252;

}  // namespace

namespace sharing_hub {

ScreenshotCapturedBubble::ScreenshotCapturedBubble(
    views::View* anchor_view,
    content::WebContents* web_contents,
    const gfx::Image& image,
    Profile* profile)
    : LocationBarBubbleDelegateView(anchor_view, nullptr),
      image_(image),
      web_contents_(web_contents->GetWeakPtr()),
      profile_(profile) {
  SetButtons(static_cast<int>(ui::mojom::DialogButton::kNone));
  SetTitle(IDS_BROWSER_SHARING_SCREENSHOT_POST_CAPTURE_TITLE);
}

ScreenshotCapturedBubble::~ScreenshotCapturedBubble() = default;

void ScreenshotCapturedBubble::OnThemeChanged() {
  LocationBarBubbleDelegateView::OnThemeChanged();

  const int border_radius = ChromeLayoutProvider::Get()->GetCornerRadiusMetric(
      views::Emphasis::kHigh);
  const auto* const color_provider = GetColorProvider();
  image_view_->SetBorder(views::CreateRoundedRectBorder(
      /*thickness=*/2, border_radius,
      color_provider->GetColor(kColorScreenshotCapturedImageBorder)));
  image_view_->SetBackground(views::CreateRoundedRectBackground(
      color_provider->GetColor(kColorScreenshotCapturedImageBackground),
      border_radius, 2));
}

void ScreenshotCapturedBubble::Show() {
  ShowForReason(USER_GESTURE);
}

views::View* ScreenshotCapturedBubble::GetInitiallyFocusedView() {
  return download_button_;
}

bool ScreenshotCapturedBubble::ShouldShowCloseButton() const {
  return true;
}

void ScreenshotCapturedBubble::WindowClosing() {}

void ScreenshotCapturedBubble::Init() {
  auto* layout_provider = ChromeLayoutProvider::Get();

  // Requesting TEXT for trailing prevents extra padding at bottom of dialog.
  gfx::Insets insets = layout_provider->GetDialogInsetsForContentType(
      views::DialogContentType::kControl, views::DialogContentType::kText);
  const int border_radius =
      layout_provider->GetCornerRadiusMetric(views::Emphasis::kHigh);

  int width_padding =
      (kImageWidthPx + border_radius - GetImageSize().width()) / 2;

  using Alignment = views::ImageView::Alignment;
  auto builder =
      views::Builder<ScreenshotCapturedBubble>(this)
          .set_margins(insets)
          .SetLayoutManager(std::make_unique<views::BoxLayout>(
              views::BoxLayout::Orientation::kVertical, gfx::Insets(),
              layout_provider->GetDistanceMetric(
                  DISTANCE_UNRELATED_CONTROL_VERTICAL_LARGE)))
          .AddChild(
              views::Builder<views::TableLayoutView>()
                  .AddPaddingColumn(views::TableLayout::kFixedSize,
                                    width_padding)
                  .AddColumn(views::LayoutAlignment::kCenter,
                             views::LayoutAlignment::kCenter, 1.0,
                             views::TableLayout::ColumnSize::kUsePreferred, 0,
                             0)
                  .AddPaddingColumn(views::TableLayout::kFixedSize,
                                    width_padding)
                  .AddRows(1, views::TableLayout::kFixedSize, 0)
                  .AddChild(views::Builder<views::ImageView>()
                                .SetHorizontalAlignment(Alignment::kCenter)
                                .SetVerticalAlignment(Alignment::kCenter)
                                .SetImageSize(GetImageSize())
                                .SetPreferredSize(
                                    GetImageSize() +
                                    gfx::Size(border_radius, border_radius))
                                .SetImage(ui::ImageModel::FromImage(image_))
                                .SetVisible(true)
                                .CopyAddressTo(&image_view_)));

  auto download_button =
      views::Builder<views::MdTextButton>()
          .SetCallback(base::BindRepeating(
              &ScreenshotCapturedBubble::DownloadButtonPressed,
              weak_factory_.GetWeakPtr()))
          .SetText(l10n_util::GetStringUTF16(
              IDS_BROWSER_SHARING_SCREENSHOT_DIALOG_DOWNLOAD_BUTTON_LABEL))
          .SetStyle(ui::ButtonStyle::kProminent)
          .Build();

  auto download_row = views::Builder<views::TableLayoutView>();

  // Column for download button
  download_row
      .AddColumn(views::LayoutAlignment::kEnd, views::LayoutAlignment::kCenter,
                 1.0, views::TableLayout::ColumnSize::kUsePreferred, 0, 0)
      .AddRows(1, views::TableLayout::kFixedSize, 0);
  download_row.AddChild(
      views::Builder<views::MdTextButton>(std::move(download_button))
          .CopyAddressTo(&download_button_));

  std::move(builder).AddChild(std::move(download_row)).BuildChildren();
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
  const SkBitmap& bitmap =
      image_view_->GetImage().GetRepresentation(1.0f).GetBitmap();
  const GURL data_url = GURL(webui::GetBitmapDataUrl(bitmap));

  if (!web_contents_)
    return;

  Browser* browser = chrome::FindBrowserWithTab(web_contents_.get());
  content::DownloadManager* download_manager =
      browser->profile()->GetDownloadManager();
  // TODO(crbug.com/40753957): Update the annotation's |setting| and
  // |chrome_policy| fields once the Sharing Hub is landed.
  net::NetworkTrafficAnnotationTag traffic_annotation =
      net::DefineNetworkTrafficAnnotation("desktop_screenshot_save", R"(
      semantics {
        sender: "Desktop Screenshots"
        description:
          "The user may capture a selection of the current page. This bubble "
          "view has a download button to save the generated image via a data "
          "URL to the disk on the local client. The feature is only for Mac, "
          "Windows and Linux OS."
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
          "be coordinated via Chrome Variations. This feature reads settings "
          "from prefs::kDisableScreenshots which is controlled by this policy."
        policy_exception_justification:
          "Not implemented, considered not required."
      })");
  std::unique_ptr<download::DownloadUrlParameters> params =
      content::DownloadRequestUtils::CreateDownloadForWebContentsMainFrame(
          web_contents_.get(), data_url, traffic_annotation);
  // Suggest a name incorporating the hostname. Protocol, TLD, etc are
  // not taken into consideration. Duplicate names get automatic suffixes.
  params->set_suggested_name(
      GetFilenameForURL(web_contents_->GetLastCommittedURL()));
  download_manager->DownloadUrl(std::move(params));
  base::RecordAction(base::UserMetricsAction(
      "SharingDesktopScreenshot.ScreenshotSavedViaBubble"));
}

// Calculates the size of the image with padding.
gfx::Size ScreenshotCapturedBubble::GetImageSize() {
  float scale_factor_x =
      static_cast<float>(kImageWidthPx) / static_cast<float>(image_.Width());
  float scale_factor_y =
      static_cast<float>(kImageHeightPx) / static_cast<float>(image_.Height());
  float scale_factor =
      scale_factor_x < scale_factor_y ? scale_factor_x : scale_factor_y;
  return gfx::Size(scale_factor * image_.Width(),
                   scale_factor * image_.Height());
}

BEGIN_METADATA(ScreenshotCapturedBubble)
END_METADATA

}  // namespace sharing_hub
