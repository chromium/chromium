// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/views/tabs/tab_hover_card_bubble_view.h"

#include <algorithm>
#include <ios>
#include <limits>
#include <memory>
#include <optional>
#include <string>

#include "base/containers/lru_cache.h"
#include "base/memory/raw_ptr.h"
#include "base/metrics/field_trial_params.h"
#include "base/metrics/histogram_macros.h"
#include "build/build_config.h"
#include "chrome/app/vector_icons/vector_icons.h"
#include "chrome/browser/browser_process.h"
#include "chrome/browser/ui/color/chrome_color_id.h"
#include "chrome/browser/ui/performance_controls/tab_resource_usage_tab_helper.h"
#include "chrome/browser/ui/tabs/tab_renderer_data.h"
#include "chrome/browser/ui/tabs/tab_style.h"
#include "chrome/browser/ui/thumbnails/thumbnail_image.h"
#include "chrome/browser/ui/ui_features.h"
#include "chrome/browser/ui/views/chrome_typography.h"
#include "chrome/browser/ui/views/tabs/fade_label_view.h"
#include "chrome/browser/ui/views/tabs/filename_elider.h"
#include "chrome/browser/ui/views/tabs/tab.h"
#include "chrome/browser/ui/views/tabs/tab_style_views.h"
#include "chrome/grit/generated_resources.h"
#include "components/url_formatter/url_formatter.h"
#include "ui/accessibility/ax_enums.mojom.h"
#include "ui/base/l10n/l10n_util.h"
#include "ui/base/metadata/metadata_header_macros.h"
#include "ui/base/metadata/metadata_impl_macros.h"
#include "ui/base/mojom/dialog_button.mojom.h"
#include "ui/base/ui_base_features.h"
#include "ui/color/color_id.h"
#include "ui/color/color_provider.h"
#include "ui/compositor/layer.h"
#include "ui/gfx/animation/linear_animation.h"
#include "ui/gfx/animation/slide_animation.h"
#include "ui/gfx/animation/tween.h"
#include "ui/gfx/canvas.h"
#include "ui/gfx/geometry/rounded_corners_f.h"
#include "ui/gfx/image/image_skia.h"
#include "ui/gfx/paint_vector_icon.h"
#include "ui/gfx/text_constants.h"
#include "ui/resources/grit/ui_resources.h"
#include "ui/views/accessibility/view_accessibility.h"
#include "ui/views/background.h"
#include "ui/views/border.h"
#include "ui/views/bubble/bubble_frame_view.h"
#include "ui/views/controls/image_view.h"
#include "ui/views/controls/label.h"
#include "ui/views/layout/fill_layout.h"
#include "ui/views/layout/flex_layout.h"
#include "ui/views/layout/layout_provider.h"
#include "ui/views/style/typography.h"
#include "ui/views/view_class_properties.h"
#include "ui/views/widget/widget.h"

namespace {

// Maximum number of lines that a title label occupies.
constexpr int kHoverCardTitleMaxLines = 2;
// Spacing used to separate the title and domain labels.
constexpr int kTitleDomainSpacing = 4;
// Margins space surrounding the text (title and domain) in the hover card.
constexpr auto kTextMargins = gfx::Insets::VH(12, 12);

// Calculates an appropriate size to display a preview image in the hover card.
// For the vast majority of images, the |preferred_size| is used, but extremely
// tall or wide images use the image size instead, centering in the available
// space.
gfx::Size GetPreviewImageSize(gfx::Size preview_size,
                              gfx::Size preferred_size) {
  DCHECK(!preferred_size.IsEmpty());
  if (preview_size.IsEmpty()) {
    return preview_size;
  }
  const float preview_aspect_ratio =
      static_cast<float>(preview_size.width()) / preview_size.height();
  const float preferred_aspect_ratio =
      static_cast<float>(preferred_size.width()) / preferred_size.height();
  const float ratio = preview_aspect_ratio / preferred_aspect_ratio;
  // Images between 2/3 and 3/2 of the target aspect ratio use the preferred
  // size, stretching the image. Only images outside this range get centered.
  // Since this is a corner case most users will never see, the specific cutoffs
  // just need to be reasonable and don't need to be precise values (that is,
  // there is no "correct" value; if the results are not aesthetic they can be
  // tuned).
  constexpr float kMinStretchRatio = 0.667f;
  constexpr float kMaxStretchRatio = 1.5f;
  if (ratio >= kMinStretchRatio && ratio <= kMaxStretchRatio) {
    return preferred_size;
  }
  return preview_size;
}
}  // namespace

// TabHoverCardBubbleView::ThumbnailView:
// ----------------------------------------------------------

// Represents the preview image on the hover card. Allows for a new image to be
// faded in over the old image.
class TabHoverCardBubbleView::ThumbnailView
    : public views::View,
      public views::AnimationDelegateViews {
  METADATA_HEADER(ThumbnailView, views::View)

 public:
  explicit ThumbnailView(TabHoverCardBubbleView* bubble_view)
      : AnimationDelegateViews(this),
        bubble_view_(bubble_view),
        image_transition_animation_(this) {
    constexpr base::TimeDelta kImageTransitionDuration =
        kHoverCardSlideDuration;
    image_transition_animation_.SetDuration(kImageTransitionDuration);

    // Set a reasonable preview size so that ThumbnailView() is given an
    // appropriate amount of space in the layout.
    target_tab_image_ = AddChildView(CreateImageView());
    image_fading_out_ = AddChildView(CreateImageView());
    image_fading_out_->SetPaintToLayer();
    image_fading_out_->layer()->SetOpacity(0.0f);

    SetLayoutManager(std::make_unique<views::FillLayout>());
  }

  void SetAnimationEnabled(bool animation_enabled) {
    animation_enabled_ = animation_enabled;
  }

  // Sets the appropriate rounded corners for the preview image, for platforms
  // where layers must be explicitly clipped (because they are not clipped by
  // the widget).
  void SetRoundedCorners(bool round_corners, float radius) {
    const gfx::RoundedCornersF corners =
        round_corners ? gfx::RoundedCornersF(0, 0, radius, radius)
                      : gfx::RoundedCornersF(0, 0, 0, 0);
    image_fading_out_->layer()->SetRoundedCornerRadius(corners);
  }

  // Sets the new preview image. The old image will be faded out.
  void SetTargetTabImage(gfx::ImageSkia preview_image) {
    StartFadeOut();
    SetImage(target_tab_image_, preview_image, ImageType::kThumbnail);
    image_type_ = ImageType::kThumbnail;
  }

  // Clears the preview image and replaces it with a placeholder image. The old
  // image will be faded out.
  void SetPlaceholderImage() {
    if (image_type_ == ImageType::kPlaceholder) {
      return;
    }

    // Color provider may be null if there is no associated widget. In that case
    // there is nothing to render, and we can't get default colors to render
    // with anyway, so bail out.
    const auto* const color_provider = GetColorProvider();
    if (!color_provider) {
      return;
    }

    StartFadeOut();

    // Check the no-preview color and size to see if it needs to be
    // regenerated. DPI or theme change can cause a regeneration.
    const SkColor foreground_color =
        color_provider->GetColor(kColorTabHoverCardForeground);

    // Set the no-preview placeholder image. All sizes are in DIPs.
    // gfx::CreateVectorIcon() caches its result so there's no need to store
    // images here; if a particular size/color combination has already been
    // requested it will be low-cost to request it again.
    constexpr gfx::Size kNoPreviewImageSize{64, 64};
    const gfx::ImageSkia no_preview_image = gfx::CreateVectorIcon(
        kGlobeIcon, kNoPreviewImageSize.width(), foreground_color);
    SetImage(target_tab_image_, no_preview_image, ImageType::kPlaceholder);
    image_type_ = ImageType::kPlaceholder;
  }

  void ClearImage() {
    if (image_type_ == ImageType::kNone) {
      return;
    }

    StartFadeOut();
    SetImage(target_tab_image_, gfx::ImageSkia(), ImageType::kNone);
    image_type_ = ImageType::kNone;
  }

  void SetWaitingForImage() {
    if (image_type_ == ImageType::kNone) {
      image_type_ = ImageType::kNoneButWaiting;
      InvalidateLayout();
    }
  }

 private:
  enum class ImageType { kNone, kNoneButWaiting, kPlaceholder, kThumbnail };

  // Creates an image view with the appropriate default properties.
  static std::unique_ptr<views::ImageView> CreateImageView() {
    auto image_view = std::make_unique<views::ImageView>();
    image_view->SetHorizontalAlignment(views::ImageView::Alignment::kCenter);
    return image_view;
  }

  // Sets `image` on `image_view_`, configuring the image appropriately based
  // on whether it's a placeholder or not.
  void SetImage(views::ImageView* image_view,
                gfx::ImageSkia image,
                ImageType image_type) {
    image_view->SetImage(ui::ImageModel::FromImageSkia(image));
    switch (image_type) {
      case ImageType::kNone:
      case ImageType::kNoneButWaiting:
        image_view->SetBackground(
            views::CreateSolidBackground(bubble_view_->color()));
        break;
      case ImageType::kPlaceholder:
        image_view->SetVerticalAlignment(views::ImageView::Alignment::kCenter);
        image_view->SetImageSize(image.size());
        image_view->SetBackground(views::CreateSolidBackground(
            image_view->GetColorProvider()->GetColor(
                kColorTabHoverCardBackground)));
        break;
      case ImageType::kThumbnail:
        image_view->SetVerticalAlignment(views::ImageView::Alignment::kLeading);
        image_view->SetImageSize(GetPreviewImageSize(
            image.size(), bubble_view_->tab_style_->GetPreviewImageSize()));
        image_view->SetBackground(nullptr);
        break;
    }
  }

  // views::View:
  gfx::Size GetMinimumSize() const override { return gfx::Size(); }

  gfx::Size CalculatePreferredSize(
      const views::SizeBounds& available_size) const override {
    return image_type_ == ImageType::kNone
               ? gfx::Size()
               : bubble_view_->tab_style_->GetPreviewImageSize();
  }

  gfx::Size GetMaximumSize() const override {
    return bubble_view_->tab_style_->GetPreviewImageSize();
  }

  // views::AnimationDelegateViews:
  void AnimationProgressed(const gfx::Animation* animation) override {
    image_fading_out_->layer()->SetOpacity(1.0 - animation->GetCurrentValue());
  }

  void AnimationEnded(const gfx::Animation* animation) override {
    image_fading_out_->layer()->SetOpacity(0.0f);
    SetImage(image_fading_out_, gfx::ImageSkia(), ImageType::kNone);
  }

  void AnimationCanceled(const gfx::Animation* animation) override {
    AnimationEnded(animation);
  }

  // Begins fading out the existing image, which it reads from
  // `target_tab_image_`. Does a smart three-way crossfade if an image is
  // already fading out.
  void StartFadeOut() {
    // If we aren't visible, don't have a widget, or our widget is being
    // destructed and has no theme provider, skip trying to fade out since a
    // ColorProvider is needed for fading out placeholder images. (Note that
    // GetColorProvider() returns nullptr if there is no widget.)
    // See: crbug.com/1246914
    if (!GetVisible() || !GetColorProvider()) {
      return;
    }

    // For consistency, always bail out with a "don't crossfade" response if
    // animations are disabled.
    if (!animation_enabled_ || !GetPreviewImageCrossfadeStart().has_value()) {
      return;
    }

    gfx::ImageSkia old_image = target_tab_image_->GetImage();

    if (image_transition_animation_.is_animating()) {
      // If we're already animating and we've barely faded out the previous old
      // image, keep fading out the old one and just swap the new one
      // underneath.
      const double current_value =
          image_transition_animation_.GetCurrentValue();
      if (current_value <= 0.5) {
        return;
      }

      // Currently we have:
      //  - old preview at `current_value` opacity
      //  - previous old preview at 1.0 - `current_value` opacity
      // We will discard the previous old preview and move the old preview into
      // the occluding view. However, since the opacity of this view is
      // inversely proportional to animation progress, to keep the same opacity
      // (while we swap the new image in behind) we have to rewind the
      // animation.
      image_transition_animation_.SetCurrentValue(1.0 - current_value);
      SetImage(image_fading_out_, old_image, image_type_);
      AnimationProgressed(&image_transition_animation_);

    } else {
      SetImage(image_fading_out_, old_image, image_type_);
      image_fading_out_->layer()->SetOpacity(1.0f);
      image_transition_animation_.Start();
    }
  }

  const raw_ptr<TabHoverCardBubbleView> bubble_view_;

  bool animation_enabled_ = true;

  // Displays the image that we are trying to display for the target/current
  // tab. Placed under `image_fading_out_` so that it is revealed as the
  // previous image fades out.
  raw_ptr<views::ImageView> target_tab_image_ = nullptr;

  // Displays the previous image as it's fading out. Rendered over
  // `target_tab_image_` and has its alpha animated from 1 to 0.
  raw_ptr<views::ImageView> image_fading_out_ = nullptr;

  // Provides a smooth fade out for `image_fading_out_`. We do not use a
  // LayerAnimation because we need to rewind the transparency at various
  // times (it's not necessarily a single smooth animation).
  gfx::LinearAnimation image_transition_animation_;

  // Records what type of image `target_tab_image_` is showing. Used to
  // configure `image_fading_out_` when the target image becomes the previous
  // image and fades out.
  ImageType image_type_ = ImageType::kNone;
};

BEGIN_METADATA(TabHoverCardBubbleView, ThumbnailView)
END_METADATA

// TabHoverCardBubbleView:
// ----------------------------------------------------------

// static
constexpr base::TimeDelta TabHoverCardBubbleView::kHoverCardSlideDuration;

DEFINE_CLASS_ELEMENT_IDENTIFIER_VALUE(TabHoverCardBubbleView,
                                      kHoverCardBubbleElementId);
DEFINE_CLASS_ELEMENT_IDENTIFIER_VALUE(TabHoverCardBubbleView,
                                      kHoverCardDomainLabelElementId);

TabHoverCardBubbleView::TabHoverCardBubbleView(Tab* tab,
                                               const InitParams& params)
    : BubbleDialogDelegateView(tab,
                               views::BubbleBorder::TOP_LEFT,
                               views::BubbleBorder::STANDARD_SHADOW),
      tab_style_(TabStyle::Get()),
      bubble_params_(params) {
  SetButtons(static_cast<int>(ui::mojom::DialogButton::kNone));

  // Remove the accessible role so that hover cards are not read when they
  // appear because tabs handle accessibility text.
  SetAccessibleWindowRole(ax::mojom::Role::kNone);

  // We'll do all of our own layout inside the bubble, so no need to inset this
  // view inside the client view.
  set_margins(gfx::Insets());

  // Set so that when hovering over a tab in a inactive window that window will
  // not become active. Setting this to false creates the need to explicitly
  // hide the hovercard on press, touch, and keyboard events.
  SetCanActivate(false);
#if BUILDFLAG(IS_MAC) || BUILDFLAG(IS_LINUX) || BUILDFLAG(IS_CHROMEOS)
  set_accept_events(false);
#endif

  // Set so that the tab hover card is not focus traversable when keyboard
  // navigating through the tab strip.
  set_focus_traversable_from_anchor_view(false);

    title_label_ = AddChildView(std::make_unique<FadeLabelView>(
        kHoverCardTitleMaxLines, CONTEXT_TAB_HOVER_CARD_TITLE,
        views::style::STYLE_BODY_3_EMPHASIS));
    domain_label_ = AddChildView(std::make_unique<FadeLabelView>(
        1, views::style::CONTEXT_DIALOG_BODY_TEXT, views::style::STYLE_BODY_4));
    domain_label_->SetEnabledColorId(kColorTabHoverCardSecondaryText);

  if (bubble_params_.show_image_preview) {
    thumbnail_view_ = AddChildView(std::make_unique<ThumbnailView>(this));
    thumbnail_view_->SetAnimationEnabled(bubble_params_.use_animation);
    thumbnail_view_->SetRoundedCorners(true, corner_radius_);
  }

  footer_view_ = AddChildView(std::make_unique<FooterView>());
  footer_view_->SetProperty(
      views::kFlexBehaviorKey,
      views::FlexSpecification(
          footer_view_->flex_layout()->GetDefaultFlexRule())
          .WithWeight(0));

  // Set up layout.

  views::FlexLayout* const layout =
      SetLayoutManager(std::make_unique<views::FlexLayout>());
  layout->SetOrientation(views::LayoutOrientation::kVertical);
  layout->SetMainAxisAlignment(views::LayoutAlignment::kStart);
  layout->SetCrossAxisAlignment(views::LayoutAlignment::kStretch);
  layout->SetCollapseMargins(true);

  // In some browser types (e.g. ChromeOS terminal app) we hide the domain
  // label. In those cases, we need to adjust the bottom margin of the title
  // element because it is no longer above another text element and needs a
  // bottom margin.

  gfx::Insets title_margins = kTextMargins;
  domain_label_->SetVisible(bubble_params_.show_domain);
  domain_label_->SetProperty(views::kElementIdentifierKey,
                             kHoverCardDomainLabelElementId);
  if (bubble_params_.show_domain) {
    title_margins.set_bottom(0);
    gfx::Insets domain_margins = kTextMargins;
    domain_margins.set_top(kTitleDomainSpacing);
    domain_label_->SetProperty(views::kMarginsKey, domain_margins);
  }

  title_label_->SetProperty(views::kMarginsKey, title_margins);
  title_label_->SetProperty(
      views::kFlexBehaviorKey,
      views::FlexSpecification(views::MinimumFlexSizeRule::kScaleToMinimum,
                               views::MaximumFlexSizeRule::kScaleToMaximum)
          .WithOrder(2));
  if (thumbnail_view_) {
    thumbnail_view_->SetProperty(
        views::kFlexBehaviorKey,
        views::FlexSpecification(views::MinimumFlexSizeRule::kScaleToMinimum,
                                 views::MaximumFlexSizeRule::kScaleToMaximum)
            .WithOrder(1));
  }

  // Set up widget.

  views::BubbleDialogDelegateView::CreateBubble(this);
  set_adjust_if_offscreen(true);
  GetBubbleFrameView()->SetPreferredArrowAdjustment(
      views::BubbleFrameView::PreferredArrowAdjustment::kOffset);
  GetBubbleFrameView()->set_hit_test_transparent(true);

  GetBubbleFrameView()->SetCornerRadius(corner_radius_);

  // Placeholder image should be used when there is no image data for the
  // given tab. Otherwise don't flash the placeholder while we wait for the
  // existing thumbnail to be decompressed.
  //
  // Note that this code has to go after CreateBubble() above, since setting up
  // the placeholder image and background color require a ColorProvider, which
  // is only available once this View has been added to its widget.
  if (thumbnail_view_ && !tab->HasThumbnail() && !tab->IsActive()) {
    thumbnail_view_->SetPlaceholderImage();
  }

  // Start in the fully "faded-in" position so that whatever text we initially
  // display is visible.
  SetTextFade(1.0);

  SetProperty(views::kElementIdentifierKey, kHoverCardBubbleElementId);
}

TabHoverCardBubbleView::~TabHoverCardBubbleView() = default;

void TabHoverCardBubbleView::UpdateCardContent(const Tab* tab) {
  // Preview image is never visible for the active tab.
  if (thumbnail_view_) {
    if (tab->IsActive() || (tab->IsDiscarded() && !tab->HasThumbnail())) {
      thumbnail_view_->ClearImage();
    } else {
      thumbnail_view_->SetWaitingForImage();
    }
  }

  std::u16string title;
  const TabRendererData& tab_data = tab->data();
  GURL domain_url;
  // Use committed URL to determine if no page has yet loaded, since the title
  // can be blank for some web pages.
  if (!tab_data.last_committed_url.is_valid()) {
    domain_url = tab_data.visible_url;
    title = tab_data.IsCrashed()
                ? l10n_util::GetStringUTF16(IDS_HOVER_CARD_CRASHED_TITLE)
                : l10n_util::GetStringUTF16(IDS_TAB_LOADING_TITLE);
    alert_state_ = std::nullopt;
  } else {
    domain_url = tab_data.last_committed_url;
    title = tab_data.title;
    alert_state_ = Tab::GetAlertStateToShow(tab_data.alert_state);
  }

  std::u16string domain;
  bool is_filename = false;
  if (domain_url.SchemeIsFile()) {
    is_filename = true;
    domain = l10n_util::GetStringUTF16(IDS_HOVER_CARD_FILE_URL_SOURCE);
  } else {
    if (domain_url.SchemeIsBlob()) {
      domain = l10n_util::GetStringUTF16(IDS_HOVER_CARD_BLOB_URL_SOURCE);
    } else {
      if (tab_data.should_display_url) {
        // Hide the domain when necessary. This leaves an empty space in the
        // card, but this scenario is very rare. Also, shrinking the card to
        // remove the space would result in visual noise, so we keep it simple.
        domain = url_formatter::FormatUrl(
            domain_url,
            url_formatter::kFormatUrlOmitDefaults |
                url_formatter::kFormatUrlOmitHTTPS |
                url_formatter::kFormatUrlOmitTrivialSubdomains |
                url_formatter::kFormatUrlTrimAfterHost,
            base::UnescapeRule::NORMAL, nullptr, nullptr, nullptr);
      }

      // Most of the time we want our standard (tail-elided) formatting for web
      // pages, but when viewing an image in the browser, many users want to
      // view the image dimensions (see crbug.com/1222984) so for titles that
      // "look" like images (i.e. that end with a dimension) we instead switch
      // to middle-elide.
      if (FilenameElider::FindImageDimensions(title) != std::u16string::npos) {
        is_filename = true;
      }
    }
  }

  title_label_->SetData({title, is_filename});
  domain_label_->SetData({domain, false});

  const bool show_discard_status = tab_data.should_show_discard_status;
  const int64_t tab_memory_usage_in_bytes =
      tab_data.tab_resource_usage
          ? tab_data.tab_resource_usage->memory_usage_in_bytes()
          : 0;
  const bool is_high_memory_usage =
      tab_data.tab_resource_usage
          ? tab_data.tab_resource_usage->is_high_memory_usage()
          : false;
  // High memory usage notification is considered a tab alert. Show it even
  // if the memory usage in hover cards pref is disabled.
  const bool show_memory_usage =
      !show_discard_status &&
      ((bubble_params_.show_memory_usage && tab_memory_usage_in_bytes > 0) ||
       is_high_memory_usage);
  const bool show_footer =
      alert_state_.has_value() || show_discard_status || show_memory_usage;

  footer_view_->SetAlertData({alert_state_, show_discard_status,
                              tab_data.discarded_memory_savings_in_bytes});

  footer_view_->SetPerformanceData(
      {show_memory_usage, is_high_memory_usage, tab_memory_usage_in_bytes});

  if (thumbnail_view_) {
    // We only clip the corners of the fade image when there isn't a footer.
    thumbnail_view_->SetRoundedCorners(!show_footer, corner_radius_);
  }
}

void TabHoverCardBubbleView::SetTextFade(double percent) {
  title_label_->SetFade(percent);
  domain_label_->SetFade(percent);
  footer_view_->SetFade(percent);
}

void TabHoverCardBubbleView::SetTargetTabImage(gfx::ImageSkia preview_image) {
  DCHECK(thumbnail_view_)
      << "This method should only be called when preview images are enabled.";
  thumbnail_view_->SetTargetTabImage(preview_image);
}

void TabHoverCardBubbleView::SetPlaceholderImage() {
  DCHECK(thumbnail_view_)
      << "This method should only be called when preview images are enabled.";
  thumbnail_view_->SetPlaceholderImage();
}

std::u16string TabHoverCardBubbleView::GetTitleTextForTesting() const {
  return title_label_->GetText();
}

std::u16string TabHoverCardBubbleView::GetDomainTextForTesting() const {
  return domain_label_->GetText();
}

views::View* TabHoverCardBubbleView::GetThumbnailViewForTesting() {
  return thumbnail_view_;
}

FooterView* TabHoverCardBubbleView::GetFooterViewForTesting() {
  return footer_view_;
}

// static
std::optional<double> TabHoverCardBubbleView::GetPreviewImageCrossfadeStart() {
  static const double start_percent = base::GetFieldTrialParamByFeatureAsDouble(
      features::kTabHoverCardImages,
      features::kTabHoverCardImagesCrossfadePreviewAtParameterName, 0.25);
  return start_percent >= 0.0
             ? std::make_optional(std::clamp(start_percent, 0.0, 1.0))
             : std::nullopt;
}

gfx::Size TabHoverCardBubbleView::CalculatePreferredSize(
    const views::SizeBounds& available_size) const {
  const int width = tab_style_->GetPreviewImageSize().width();
  const int height =
      GetLayoutManager()->GetPreferredHeightForWidth(this, width);
  const gfx::Size preferred_size(width, height);
  DCHECK(!preferred_size.IsEmpty());
  return preferred_size;
}

BEGIN_METADATA(TabHoverCardBubbleView)
END_METADATA
