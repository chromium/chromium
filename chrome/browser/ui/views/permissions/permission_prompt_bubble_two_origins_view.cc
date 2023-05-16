// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/views/permissions/permission_prompt_bubble_two_origins_view.h"

#include "chrome/browser/favicon/favicon_service_factory.h"
#include "chrome/browser/ui/browser.h"
#include "components/favicon/core/favicon_service.h"
#include "components/favicon_base/favicon_callback.h"
#include "components/strings/grit/components_strings.h"
#include "components/url_formatter/elide_url.h"
#include "ui/base/l10n/l10n_util.h"
#include "ui/resources/grit/ui_resources.h"
#include "ui/views/controls/image_view.h"
#include "ui/views/vector_icons.h"

namespace {

// TODO(b/278181254): We might need to fetch larger icons on higher dpi
// screens.
constexpr int kDesiredFaviconSizeInPixel = 32;
// TODO(b/278181254): Add metrics for how long the favicons take to be fetched,
// so we can adjust this delay accordingly.
constexpr int kMaxShowDelayMs = 200;

absl::optional<std::u16string> GetExtraTextTwoOrigin(
    permissions::PermissionPrompt::Delegate& delegate) {
  CHECK_GT(delegate.Requests().size(), 0u);
  switch (delegate.Requests()[0]->request_type()) {
    case permissions::RequestType::kStorageAccess:
      return l10n_util::GetStringFUTF16(
          IDS_STORAGE_ACCESS_PERMISSION_TWO_ORIGIN_EXPLANATION,
          url_formatter::FormatUrlForSecurityDisplay(
              delegate.GetRequestingOrigin(),
              url_formatter::SchemeDisplay::OMIT_CRYPTOGRAPHIC));
    default:
      NOTREACHED_NORETURN();
  }
}

std::u16string GetWindowTitleTwoOrigin(
    permissions::PermissionPrompt::Delegate& delegate) {
  CHECK_GT(delegate.Requests().size(), 0u);
  switch (delegate.Requests()[0]->request_type()) {
    case permissions::RequestType::kStorageAccess:
      return l10n_util::GetStringFUTF16(
          IDS_STORAGE_ACCESS_PERMISSION_TWO_ORIGIN_PROMPT_TITLE,
          url_formatter::FormatUrlForSecurityDisplay(
              delegate.GetRequestingOrigin(),
              url_formatter::SchemeDisplay::OMIT_CRYPTOGRAPHIC),
          url_formatter::FormatUrlForSecurityDisplay(
              delegate.GetEmbeddingOrigin(),
              url_formatter::SchemeDisplay::OMIT_CRYPTOGRAPHIC));
    default:
      NOTREACHED_NORETURN();
  }
}

}  // namespace

PermissionPromptBubbleTwoOriginsView::PermissionPromptBubbleTwoOriginsView(
    Browser* browser,
    base::WeakPtr<permissions::PermissionPrompt::Delegate> delegate,
    base::TimeTicks permission_requested_time,
    PermissionPromptStyle prompt_style)
    : PermissionPromptBubbleBaseView(browser,
                                     delegate,
                                     permission_requested_time,
                                     prompt_style,
                                     GetWindowTitleTwoOrigin(*delegate),
                                     GetWindowTitleTwoOrigin(*delegate),
                                     GetExtraTextTwoOrigin(*delegate)) {
  // Only requests for SAA should use this prompt.
  CHECK(delegate);
  CHECK_GT(delegate->Requests().size(), 0u);
  CHECK_EQ(delegate->Requests()[0]->request_type(),
           permissions::RequestType::kStorageAccess);

  AddFaviconRow();

  CHECK(browser);

  // Initializing favicon service.
  favicon::FaviconService* const favicon_service =
      FaviconServiceFactory::GetForProfile(browser->profile(),
                                           ServiceAccessType::EXPLICIT_ACCESS);
  favicon_tracker_ = std::make_unique<base::CancelableTaskTracker>();

  // Fetching requesting origin favicon.
  favicon_service->GetRawFaviconForPageURL(
      delegate->GetRequestingOrigin(), {favicon_base::IconType::kFavicon},
      kDesiredFaviconSizeInPixel, /*fallback_to_host=*/true,
      base::BindOnce(&PermissionPromptBubbleTwoOriginsView::
                         OnRequestingOriginFaviconLoaded,
                     base::Unretained(this)),
      favicon_tracker_.get());

  // Fetching embedding origin favicon.
  favicon_service->GetRawFaviconForPageURL(
      delegate->GetEmbeddingOrigin(), {favicon_base::IconType::kFavicon},
      kDesiredFaviconSizeInPixel, /*fallback_to_host=*/true,
      base::BindOnce(
          &PermissionPromptBubbleTwoOriginsView::OnEmbeddingOriginFaviconLoaded,
          base::Unretained(this)),
      favicon_tracker_.get());
}

PermissionPromptBubbleTwoOriginsView::~PermissionPromptBubbleTwoOriginsView() =
    default;

void PermissionPromptBubbleTwoOriginsView::AddedToWidget() {
  if (GetUrlIdentityObject().type == UrlIdentity::Type::kDefault) {
    // TODO(crbug/1433644): There might be a risk of URL spoofing from origins
    // that are too wide to fit in the bubble.
    std::unique_ptr<views::Label> label = std::make_unique<views::Label>(
        GetWindowTitle(), views::style::CONTEXT_DIALOG_TITLE);
    label->SetHorizontalAlignment(gfx::ALIGN_LEFT);
    label->SetCollapseWhenHidden(true);
    label->SetMultiLine(true);
    label->SetMaxLines(4);
    GetBubbleFrameView()->SetTitleView(std::move(label));
  }
}

void PermissionPromptBubbleTwoOriginsView::Show() {
  CreateWidget();

  if (favicon_left_received_ && favicon_right_received_) {
    ShowWidget();
    return;
  }

  show_timer_.Start(FROM_HERE, base::Milliseconds(kMaxShowDelayMs),
                    base::BindOnce(&PermissionPromptBubbleBaseView::ShowWidget,
                                   base::Unretained(this)));
}

void PermissionPromptBubbleTwoOriginsView::AddFaviconRow() {
  // Line container for the favicon icons.
  auto* line_container =
      AddChildViewAt(std::make_unique<views::View>(), /*index=*/0);

  views::BoxLayout* box_layout =
      line_container->SetLayoutManager(std::make_unique<views::BoxLayout>(
          views::BoxLayout::Orientation::kHorizontal, gfx::Insets(),
          /*between_child_spacing=*/4));

  // Center box_layout children horizontally and vertically.
  box_layout->set_main_axis_alignment(
      views::BoxLayout::MainAxisAlignment::kCenter);
  box_layout->set_cross_axis_alignment(
      views::BoxLayout::CrossAxisAlignment::kCenter);

  // Getting default favicon.
  const ui::NativeTheme* native_theme =
      ui::NativeTheme::GetInstanceForNativeUi();
  bool is_dark = native_theme && native_theme->ShouldUseDarkColors();
  int resource_id =
      is_dark ? IDR_DEFAULT_FAVICON_DARK_32 : IDR_DEFAULT_FAVICON_32;
  ui::ImageModel default_favicon_ = ui::ImageModel::FromResourceId(resource_id);

  // Left favicon for embedding origin.
  favicon_left_ = line_container->AddChildView(
      std::make_unique<views::ImageView>(default_favicon_));
  favicon_left_->SetVerticalAlignment(views::ImageView::Alignment::kLeading);

  // Three dots.
  line_container->AddChildView(
      std::make_unique<views::ImageView>(ui::ImageModel::FromVectorIcon(
          views::kOptionsIcon, ui::kColorIcon, /*icon_size=*/40)));

  // Right favicon for requesting origin.
  favicon_right_ = line_container->AddChildView(
      std::make_unique<views::ImageView>(default_favicon_));
  favicon_right_->SetVerticalAlignment(views::ImageView::Alignment::kLeading);
}

void PermissionPromptBubbleTwoOriginsView::OnEmbeddingOriginFaviconLoaded(
    const favicon_base::FaviconRawBitmapResult& favicon_result) {
  favicon_left_received_ = true;

  if (favicon_result.is_valid()) {
    favicon_left_->SetImage(ui::ImageModel::FromImage(
        gfx::Image::CreateFrom1xPNGBytes(favicon_result.bitmap_data->front(),
                                         favicon_result.bitmap_data->size())));
  }
  MaybeShow();
}

void PermissionPromptBubbleTwoOriginsView::OnRequestingOriginFaviconLoaded(
    const favicon_base::FaviconRawBitmapResult& favicon_result) {
  favicon_right_received_ = true;

  if (favicon_result.is_valid()) {
    favicon_right_->SetImage(ui::ImageModel::FromImage(
        gfx::Image::CreateFrom1xPNGBytes(favicon_result.bitmap_data->front(),
                                         favicon_result.bitmap_data->size())));
  }
  MaybeShow();
}

void PermissionPromptBubbleTwoOriginsView::MaybeShow() {
  if (favicon_left_received_ && favicon_right_received_ &&
      show_timer_.IsRunning()) {
    show_timer_.FireNow();
  }
}
