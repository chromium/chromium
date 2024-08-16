// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/views/permissions/permission_prompt_bubble_two_origins_view.h"

#include "base/metrics/histogram_functions.h"
#include "chrome/app/vector_icons/vector_icons.h"
#include "chrome/browser/favicon/favicon_service_factory.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/browser/ui/views/permissions/permission_prompt_bubble_base_view.h"
#include "components/content_settings/core/browser/host_content_settings_map.h"
#include "components/content_settings/core/common/content_settings_types.h"
#include "components/favicon/core/favicon_service.h"
#include "components/favicon_base/favicon_callback.h"
#include "components/permissions/permission_util.h"
#include "components/strings/grit/components_strings.h"
#include "components/url_formatter/elide_url.h"
#include "ui/base/l10n/l10n_util.h"
#include "ui/views/controls/image_view.h"
#include "ui/views/layout/layout_provider.h"

namespace {

constexpr int kDesiredFaviconSizeInPixel = 28;
// TODO(b/278181254): Add metrics for how long the favicons take to be fetched,
// so we can adjust this delay accordingly.
constexpr int kMaxShowDelayMs = 200;

std::optional<std::u16string> GetExtraTextTwoOrigin(
    permissions::PermissionPrompt::Delegate& delegate) {
  CHECK_GT(delegate.Requests().size(), 0u);
  switch (delegate.Requests()[0]->request_type()) {
    case permissions::RequestType::kStorageAccess: {
      auto patterns = HostContentSettingsMap::GetPatternsForContentSettingsType(
          delegate.GetRequestingOrigin(), delegate.GetEmbeddingOrigin(),
          ContentSettingsType::STORAGE_ACCESS);

      return l10n_util::GetStringFUTF16(
          IDS_STORAGE_ACCESS_PERMISSION_TWO_ORIGIN_EXPLANATION,
          url_formatter::FormatUrlForSecurityDisplay(
              patterns.first.ToRepresentativeUrl(),
              url_formatter::SchemeDisplay::OMIT_CRYPTOGRAPHIC),
          url_formatter::FormatUrlForSecurityDisplay(
              patterns.second.ToRepresentativeUrl(),
              url_formatter::SchemeDisplay::OMIT_CRYPTOGRAPHIC));
    }
    default:
      return std::nullopt;
  }
}

bool HasExtraText(permissions::PermissionPrompt::Delegate& delegate) {
  CHECK_GT(delegate.Requests().size(), 0u);
  return delegate.Requests()[0]->request_type() ==
         permissions::RequestType::kStorageAccess;
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
                                     prompt_style) {
  SetTitle(CreateWindowTitle());

  auto extra_text = GetExtraTextTwoOrigin(*delegate);
  if (extra_text.has_value()) {
    CreateExtraTextLabel(extra_text.value());
  }

  CreatePermissionButtons(GetAllowAlwaysText(delegate->Requests()));

  // Only requests for Storage Access should use this prompt.
  CHECK(delegate);
  CHECK_GT(delegate->Requests().size(), 0u);
  CHECK_EQ(delegate->Requests()[0]->request_type(),
           permissions::RequestType::kStorageAccess);

  set_fixed_width(views::LayoutProvider::Get()->GetDistanceMetric(
      views::DISTANCE_MODAL_DIALOG_PREFERRED_WIDTH));

  CreateFaviconRow();
  MaybeAddLink();

  CHECK(browser);

  // Initializing favicon service.
  favicon::FaviconService* const favicon_service =
      FaviconServiceFactory::GetForProfile(browser->profile(),
                                           ServiceAccessType::EXPLICIT_ACCESS);
  favicon_tracker_ = std::make_unique<base::CancelableTaskTracker>();

  // Fetching requesting origin favicon.
  // Fetch raw favicon to set |fallback_to_host|=true since we otherwise might
  // not get a result if the user never visited the root URL of |site|.
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

PermissionPromptBubbleTwoOriginsView::~PermissionPromptBubbleTwoOriginsView() {
  if (favicon_left_) {
    favicon_left_->RemoveObserver(this);
  }
  if (favicon_right_) {
    favicon_right_->RemoveObserver(this);
  }
}

void PermissionPromptBubbleTwoOriginsView::AddedToWidget() {
  StartTrackingPictureInPictureOcclusion();

  if (GetUrlIdentityObject().type != UrlIdentity::Type::kDefault) {
    return;
  }

  auto title_container = std::make_unique<views::FlexLayoutView>();
  title_container->SetOrientation(views::LayoutOrientation::kVertical);

  title_container->AddChildView(std::move(favicon_container_));

  // TODO(crbug.com/40064079): There might be a risk of URL spoofing from
  // origins that are too large to fit in the bubble.
  auto label = std::make_unique<views::Label>(
      GetWindowTitle(), views::style::CONTEXT_DIALOG_TITLE);
  label->SetHorizontalAlignment(gfx::ALIGN_LEFT);
  label->SetCollapseWhenHidden(true);
  label->SetMultiLine(true);
  label->SetMaxLines(4);
  label->SetProperty(
      views::kFlexBehaviorKey,
      views::FlexSpecification(views::MinimumFlexSizeRule::kScaleToZero,
                               views::MaximumFlexSizeRule::kScaleToMaximum,
                               /*adjust_height_for_width=*/true));
  title_container->AddChildView(std::move(label));

  GetBubbleFrameView()->SetTitleView(std::move(title_container));
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

std::u16string PermissionPromptBubbleTwoOriginsView::CreateWindowTitle() {
  CHECK_GT(delegate()->Requests().size(), 0u);

  switch (delegate()->Requests()[0]->request_type()) {
    case permissions::RequestType::kStorageAccess: {
      content_settings::PatternPair patterns =
          HostContentSettingsMap::GetPatternsForContentSettingsType(
              delegate()->GetRequestingOrigin(),
              delegate()->GetEmbeddingOrigin(),
              ContentSettingsType::STORAGE_ACCESS);

      size_t title_offset;
      std::u16string title_string = l10n_util::GetStringFUTF16(
          IDS_STORAGE_ACCESS_PERMISSION_TWO_ORIGIN_PROMPT_TITLE,
          url_formatter::FormatUrlForSecurityDisplay(
              patterns.first.ToRepresentativeUrl(),
              url_formatter::SchemeDisplay::OMIT_CRYPTOGRAPHIC),
          &title_offset);
      SetTitleBoldedRanges(
          {{title_offset,
            title_offset + GetUrlIdentityObject().name.length()}});
      return title_string;
    }
    default:
      NOTREACHED();
  }
}

void PermissionPromptBubbleTwoOriginsView::CreateFaviconRow() {
  // Getting default favicon.
  ui::ImageModel default_favicon_ = ui::ImageModel::FromVectorIcon(
      kGlobeIcon, ui::kColorIcon, kDesiredFaviconSizeInPixel);

  const int favicon_margin = views::LayoutProvider::Get()->GetDistanceMetric(
      views::DISTANCE_VECTOR_ICON_PADDING);

  // Container for the favicon icons.
  favicon_container_ = std::make_unique<views::FlexLayoutView>();
  favicon_container_->SetOrientation(views::LayoutOrientation::kHorizontal);
  favicon_container_->SetProperty(views::kMarginsKey,
                                  gfx::Insets().set_bottom(favicon_margin));

  // Left favicon for requesting origin.
  favicon_left_ = favicon_container_->AddChildView(
      std::make_unique<views::ImageView>(default_favicon_));
  favicon_left_->SetVerticalAlignment(views::ImageView::Alignment::kLeading);
  favicon_left_->SetProperty(views::kMarginsKey,
                             gfx::Insets().set_right(favicon_margin));
  favicon_left_->AddObserver(this);

  // Right favicon for embedding origin.
  favicon_right_ = favicon_container_->AddChildView(
      std::make_unique<views::ImageView>(default_favicon_));
  favicon_right_->SetVerticalAlignment(views::ImageView::Alignment::kLeading);
  favicon_right_->SetProperty(views::kMarginsKey,
                              gfx::Insets().set_left(favicon_margin));
  favicon_right_->AddObserver(this);
}

void PermissionPromptBubbleTwoOriginsView::OnEmbeddingOriginFaviconLoaded(
    const favicon_base::FaviconRawBitmapResult& favicon_result) {
  favicon_right_received_ = true;
  base::UmaHistogramBoolean("Permissions.Prompt.HasEmbeddingFavicon",
                            favicon_result.is_valid());

  if (favicon_result.is_valid()) {
    favicon_right_->SetImage(ui::ImageModel::FromImage(
        gfx::Image::CreateFrom1xPNGBytes(favicon_result.bitmap_data)));
  }
  MaybeShow();
}

void PermissionPromptBubbleTwoOriginsView::OnRequestingOriginFaviconLoaded(
    const favicon_base::FaviconRawBitmapResult& favicon_result) {
  favicon_left_received_ = true;
  base::UmaHistogramBoolean("Permissions.Prompt.HasRequestingFavicon",
                            favicon_result.is_valid());

  if (favicon_result.is_valid()) {
    favicon_left_->SetImage(ui::ImageModel::FromImage(
        gfx::Image::CreateFrom1xPNGBytes(favicon_result.bitmap_data)));
  }
  MaybeShow();
}

void PermissionPromptBubbleTwoOriginsView::MaybeAddLink() {
  gfx::Range link_range;
  views::StyledLabel::RangeStyleInfo link_style;
  std::optional<std::u16string> link = GetLink(link_range, link_style);
  if (link.has_value()) {
    size_t index = HasExtraText(*delegate()) ? 1 : 0;
    auto* link_label =
        AddChildViewAt(std::make_unique<views::StyledLabel>(), index);
    link_label->SetText(link.value());
    link_label->SetID(
        permissions::PermissionPromptViewID::VIEW_ID_PERMISSION_PROMPT_LINK);
    if (!link_range.is_empty()) {
      link_label->AddStyleRange(link_range, link_style);
    }
  }
}

std::optional<std::u16string> PermissionPromptBubbleTwoOriginsView::GetLink(
    gfx::Range& link_range,
    views::StyledLabel::RangeStyleInfo& link_style) {
  CHECK_GT(delegate()->Requests().size(), 0u);
  switch (delegate()->Requests()[0]->request_type()) {
    case permissions::RequestType::kStorageAccess:
      return GetLinkStorageAccess(link_range, link_style);
    default:
      return std::nullopt;
  }
}

std::u16string PermissionPromptBubbleTwoOriginsView::GetLinkStorageAccess(
    gfx::Range& link_range,
    views::StyledLabel::RangeStyleInfo& link_style) {
  auto settings_text_for_link =
      l10n_util::GetStringUTF16(IDS_STORAGE_ACCESS_PERMISSION_TWO_ORIGIN_LINK);

  link_range = gfx::Range(0, settings_text_for_link.length());
  link_style =
      views::StyledLabel::RangeStyleInfo::CreateForLink(base::BindRepeating(
          &PermissionPromptBubbleTwoOriginsView::HelpCenterLinkClicked,
          base::Unretained(this)));

  return settings_text_for_link;
}

void PermissionPromptBubbleTwoOriginsView::HelpCenterLinkClicked(
    const ui::Event& event) {
  if (delegate()) {
    delegate()->OpenHelpCenterLink(event);
  }
}

void PermissionPromptBubbleTwoOriginsView::MaybeShow() {
  if (favicon_left_received_ && favicon_right_received_ &&
      show_timer_.IsRunning()) {
    show_timer_.FireNow();
  }
}

void PermissionPromptBubbleTwoOriginsView::OnViewIsDeleting(views::View* view) {
  // This is necessary to avoid dangling pointers since the favicon views are
  // owned by the custom title which is destroyed before this view.
  view->RemoveObserver(this);
  if (view == favicon_left_.get()) {
    favicon_left_ = nullptr;
  }
  if (view == favicon_right_.get()) {
    favicon_right_ = nullptr;
  }
}
