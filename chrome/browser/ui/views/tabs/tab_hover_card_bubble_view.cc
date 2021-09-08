// Copyright (c) 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/views/tabs/tab_hover_card_bubble_view.h"

#include <algorithm>
#include <memory>

#include "base/containers/mru_cache.h"
#include "base/metrics/field_trial_params.h"
#include "base/metrics/histogram_macros.h"
#include "base/numerics/ranges.h"
#include "build/build_config.h"
#include "chrome/app/vector_icons/vector_icons.h"
#include "chrome/browser/themes/theme_properties.h"
#include "chrome/browser/ui/tabs/tab_renderer_data.h"
#include "chrome/browser/ui/tabs/tab_style.h"
#include "chrome/browser/ui/thumbnails/thumbnail_image.h"
#include "chrome/browser/ui/ui_features.h"
#include "chrome/browser/ui/views/chrome_layout_provider.h"
#include "chrome/browser/ui/views/chrome_typography.h"
#include "chrome/browser/ui/views/tabs/tab.h"
#include "chrome/browser/ui/views/tabs/tab_hover_card_controller.h"
#include "chrome/grit/generated_resources.h"
#include "components/url_formatter/url_formatter.h"
#include "ui/accessibility/ax_enums.mojom.h"
#include "ui/base/l10n/l10n_util.h"
#include "ui/base/metadata/metadata_header_macros.h"
#include "ui/base/metadata/metadata_impl_macros.h"
#include "ui/base/theme_provider.h"
#include "ui/compositor/layer.h"
#include "ui/gfx/animation/linear_animation.h"
#include "ui/gfx/animation/slide_animation.h"
#include "ui/gfx/animation/tween.h"
#include "ui/gfx/canvas.h"
#include "ui/gfx/geometry/rounded_corners_f.h"
#include "ui/gfx/image/image_skia.h"
#include "ui/gfx/paint_vector_icon.h"
#include "ui/gfx/text_constants.h"
#include "ui/native_theme/native_theme.h"
#include "ui/resources/grit/ui_resources.h"
#include "ui/views/accessibility/view_accessibility.h"
#include "ui/views/background.h"
#include "ui/views/bubble/bubble_frame_view.h"
#include "ui/views/controls/image_view.h"
#include "ui/views/controls/label.h"
#include "ui/views/layout/fill_layout.h"
#include "ui/views/layout/flex_layout.h"
#include "ui/views/layout/layout_provider.h"
#include "ui/views/view_class_properties.h"
#include "ui/views/widget/widget.h"

#if defined(OS_WIN)
#include "ui/base/win/shell.h"
#endif

namespace {
// Maximum number of lines that a title label occupies.
constexpr int kHoverCardTitleMaxLines = 2;

bool CustomShadowsSupported() {
#if defined(OS_WIN)
  return ui::win::IsAeroGlassEnabled();
#else
  return true;
#endif
}

std::unique_ptr<views::View> CreateAlertView(const TabAlertState& state) {
  auto alert_state_label = std::make_unique<views::Label>(
      std::u16string(), views::style::CONTEXT_DIALOG_BODY_TEXT,
      views::style::STYLE_PRIMARY);
  alert_state_label->SetHorizontalAlignment(gfx::ALIGN_LEFT);
  alert_state_label->SetMultiLine(true);
  alert_state_label->SetVisible(true);
  alert_state_label->SetText(chrome::GetTabAlertStateText(state));
  return alert_state_label;
}

// Calculates an appropriate size to display a preview image in the hover card.
// For the vast majority of images, the |preferred_size| is used, but extremely
// tall or wide images use the image size instead, centering in the available
// space.
gfx::Size GetPreviewImageSize(gfx::Size preview_size,
                              gfx::Size preferred_size) {
  DCHECK(!preferred_size.IsEmpty());
  if (preview_size.IsEmpty())
    return preview_size;
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
  if (ratio >= kMinStretchRatio && ratio <= kMaxStretchRatio)
    return preferred_size;
  return preview_size;
}

}  // namespace

// This is a label with two tweaks:
// - a solid background color, which can have alpha
// - a function to make the foreground and background color fade away (via
//   alpha) to zero as an animation progresses
//
// It is used to overlay the old title and domain values as a hover card slide
// animation happens.
class TabHoverCardBubbleView::FadeLabel : public views::Label {
 public:
  using Label::Label;

  METADATA_HEADER(FadeLabel);

  FadeLabel() = default;
  ~FadeLabel() override = default;

  // Sets the fade-out of the label as |percent| in the range [0, 1]. Since
  // FadeLabel is designed to mask new text with the old and then fade away, the
  // higher the percentage the less opaque the label.
  void SetFade(double percent) {
    if (percent >= 1.0)
      SetText(std::u16string());
    const SkAlpha alpha = base::saturated_cast<SkAlpha>(
        std::numeric_limits<SkAlpha>::max() * (1.0 - percent));
    SetBackgroundColor(SkColorSetA(GetBackgroundColor(), alpha));
    SetEnabledColor(SkColorSetA(GetEnabledColor(), alpha));
  }

 protected:
  // views::Label:
  void OnPaintBackground(gfx::Canvas* canvas) override {
    canvas->DrawColor(GetBackgroundColor());
  }
};

BEGIN_METADATA(TabHoverCardBubbleView, FadeLabel, views::Label)
END_METADATA

// Represents the preview image on the hover card. Allows for a new image to be
// faded in over the old image.
class TabHoverCardBubbleView::ThumbnailView
    : public views::View,
      public views::AnimationDelegateViews {
 public:
  // Specifies which (if any) of the corners of the preview image will be
  // rounded. See SetRoundedCorners() below for more information.
  enum RoundedCorners { kNone, kTopCorners, kBottomCorners };

  ThumbnailView()
      : AnimationDelegateViews(this), image_transition_animation_(this) {
    constexpr base::TimeDelta kImageTransitionDuration =
        kHoverCardSlideDuration;
    image_transition_animation_.SetDuration(kImageTransitionDuration);

    // Set a reasonable preview size so that ThumbnailView() is given an
    // appropriate amount of space in the layout.
    target_tab_image_ = AddChildView(CreateImageView());
    image_fading_out_ = AddChildView(CreateImageView());
    image_fading_out_->SetPaintToLayer();
    image_fading_out_->layer()->SetOpacity(0.0f);

    // Because all preview images should be the same size, we can just set the
    // preferred size of this view and then use a FillLayout to force the to
    // ImageViews to the correct size.
    SetPreferredSize(TabStyle::GetPreviewImageSize());
    SetLayoutManager(std::make_unique<views::FillLayout>());
  }

  // Set the preview image to be visible. Included to match with Hide().
  void Show() { SetVisible(true); }

  // Set the preview image to not be visible. Stops any current fade animation
  // and clears out all of the images to prevent flicker when the preview image
  // transitions from visible to invisible and vice-versa.
  void Hide() {
    SetVisible(false);
    // This will result in the fading image being discarded via the canceled
    // event.
    image_transition_animation_.End();
    target_tab_image_->SetImage(gfx::ImageSkia());
    target_tab_image_->SetBackground(nullptr);
  }

  // Sets the appropriate rounded corners for the preview image, for platforms
  // where layers must be explicitly clipped (because they are not clipped by
  // the widget). Set `rounded_corners` to kTopCorners if the preview image is
  // the topmost view in the widget (including header); set kBottomCorners if
  // the preview is the bottom-most view (including footer). If neither, use
  // kNone.
  void SetRoundedCorners(RoundedCorners rounded_corners, float radius) {
    gfx::RoundedCornersF corners;
    switch (rounded_corners) {
      case RoundedCorners::kNone:
        corners = {0, 0, 0, 0};
        break;
      case RoundedCorners::kTopCorners:
        corners = {radius, radius, 0, 0};
        break;
      case RoundedCorners::kBottomCorners:
        corners = {0, 0, radius, radius};
        break;
    }
    image_fading_out_->layer()->SetRoundedCornerRadius(corners);
  }

  // Sets the new preview image. The old image will be faded out.
  void SetTargetTabImage(gfx::ImageSkia preview_image) {
    StartFadeOut();
    SetImage(target_tab_image_, preview_image, /* is_placeholder = */ false);
    showing_placeholder_image_ = false;
  }

  // Clears the preview image and replaces it with a placeholder image. The old
  // image will be faded out.
  void SetPlaceholderImage() {
    if (showing_placeholder_image_)
      return;

    // Theme provider may be null if there is no associated widget. In that case
    // there is nothing to render, and we can't get theme default colors to
    // render with anyway, so bail out.
    const ui::ThemeProvider* const theme_provider = GetThemeProvider();
    if (!theme_provider)
      return;

    StartFadeOut();

    // Check the no-preview color and size to see if it needs to be
    // regenerated. DPI or theme change can cause a regeneration.
    const SkColor foreground_color = theme_provider->GetColor(
        ThemeProperties::COLOR_HOVER_CARD_NO_PREVIEW_FOREGROUND);

    // Set the no-preview placeholder image. All sizes are in DIPs.
    // gfx::CreateVectorIcon() caches its result so there's no need to store
    // images here; if a particular size/color combination has already been
    // requested it will be low-cost to request it again.
    constexpr gfx::Size kNoPreviewImageSize{64, 64};
    const gfx::ImageSkia no_preview_image = gfx::CreateVectorIcon(
        kGlobeIcon, kNoPreviewImageSize.width(), foreground_color);
    SetImage(target_tab_image_, no_preview_image, /* is_placeholder = */ true);
    showing_placeholder_image_ = true;
  }

 private:
  // Creates an image view with the appropriate default properties.
  static std::unique_ptr<views::ImageView> CreateImageView() {
    auto image_view = std::make_unique<views::ImageView>();
    using Alignment = views::ImageView::Alignment;
    image_view->SetHorizontalAlignment(Alignment::kCenter);
    image_view->SetVerticalAlignment(Alignment::kCenter);
    return image_view;
  }

  // Sets `image` on `image_view_`, configuring the image appropriately based
  // on whether it's a placeholder or not.
  static void SetImage(views::ImageView* image_view,
                       gfx::ImageSkia image,
                       bool is_placeholder) {
    image_view->SetImage(image);
    if (is_placeholder) {
      image_view->SetImage(image);
      image_view->SetImageSize(image.size());

      // Also possibly regenerate the background if it has changed.
      const SkColor background_color = image_view->GetThemeProvider()->GetColor(
          ThemeProperties::COLOR_HOVER_CARD_NO_PREVIEW_BACKGROUND);
      if (!image_view->background() ||
          image_view->background()->get_color() != background_color) {
        image_view->SetBackground(
            views::CreateSolidBackground(background_color));
      }
    } else {
      const gfx::Size preview_size = TabStyle::GetPreviewImageSize();
      image_view->SetImage(image);
      image_view->SetImageSize(GetPreviewImageSize(image.size(), preview_size));
      image_view->SetBackground(nullptr);
    }
  }

  // views::AnimationDelegateViews:
  void AnimationProgressed(const gfx::Animation* animation) override {
    image_fading_out_->layer()->SetOpacity(1.0 - animation->GetCurrentValue());
  }

  void AnimationEnded(const gfx::Animation* animation) override {
    image_fading_out_->layer()->SetOpacity(0.0f);
    image_fading_out_->SetImage(gfx::ImageSkia());
  }

  void AnimationCanceled(const gfx::Animation* animation) override {
    AnimationEnded(animation);
  }

  // Begins fading out the existing image, which it reads from
  // `target_tab_image_`. Does a smart three-way crossfade if an image is
  // already fading out.
  void StartFadeOut() {
    if (!GetVisible())
      return;

    if (!GetPreviewImageCrossfadeStart().has_value())
      return;

    gfx::ImageSkia old_image = target_tab_image_->GetImage();
    if (old_image.isNull())
      return;

    if (image_transition_animation_.is_animating()) {
      // If we're already animating and we've barely faded out the previous old
      // image, keep fading out the old one and just swap the new one
      // underneath.
      const double current_value =
          image_transition_animation_.GetCurrentValue();
      if (current_value <= 0.5)
        return;

      // Currently we have:
      //  - old preview at `current_value` opacity
      //  - previous old preview at 1.0 - `current_value` opacity
      // We will discard the previous old preview and move the old preview into
      // the occluding view. However, since the opacity of this view is
      // inversely proportional to animation progress, to keep the same opacity
      // (while we swap the new image in behind) we have to rewind the
      // animation.
      image_transition_animation_.SetCurrentValue(1.0 - current_value);
      SetImage(image_fading_out_, old_image, showing_placeholder_image_);
      AnimationProgressed(&image_transition_animation_);

    } else {
      SetImage(image_fading_out_, old_image, showing_placeholder_image_);
      image_fading_out_->layer()->SetOpacity(1.0f);
      image_transition_animation_.Start();
    }
  }

  // Displays the image that we are trying to display for the target/current
  // tab. Placed under `image_fading_out_` so that it is revealed as the
  // previous image fades out.
  views::ImageView* target_tab_image_ = nullptr;

  // Displays the previous image as it's fading out. Rendered over
  // `target_tab_image_` and has its alpha animated from 1 to 0.
  views::ImageView* image_fading_out_ = nullptr;

  // Provides a smooth fade out for `image_fading_out_`. We do not use a
  // LayerAnimation because we need to rewind the transparency at various
  // times (it's not necessarily a single smooth animation).
  gfx::LinearAnimation image_transition_animation_;

  // Records whether `target_tab_image_` is showing a placeholder image. Used
  // to configure `image_fading_out_` when the target image becomes the
  // previous image and fades out.
  bool showing_placeholder_image_ = false;
};

// static
constexpr base::TimeDelta TabHoverCardBubbleView::kHoverCardSlideDuration;

TabHoverCardBubbleView::TabHoverCardBubbleView(Tab* tab)
    : BubbleDialogDelegateView(tab,
                               views::BubbleBorder::TOP_LEFT,
                               views::BubbleBorder::STANDARD_SHADOW) {
  if (CustomShadowsSupported()) {
    corner_radius_ = ChromeLayoutProvider::Get()->GetCornerRadiusMetric(
        views::Emphasis::kHigh);
  }
  SetButtons(ui::DIALOG_BUTTON_NONE);

  // We'll do all of our own layout inside the bubble, so no need to inset this
  // view inside the client view.
  set_margins(gfx::Insets());

  // Set so that when hovering over a tab in a inactive window that window will
  // not become active. Setting this to false creates the need to explicitly
  // hide the hovercard on press, touch, and keyboard events.
  SetCanActivate(false);
#if defined(OS_MAC)
  set_accept_events(false);
#endif

  // Set so that the tab hover card is not focus traversable when keyboard
  // navigating through the tab strip.
  set_focus_traversable_from_anchor_view(false);

  title_label_ = AddChildView(std::make_unique<views::Label>(
      std::u16string(), CONTEXT_TAB_HOVER_CARD_TITLE,
      views::style::STYLE_PRIMARY));
  title_label_->SetHorizontalAlignment(gfx::ALIGN_LEFT);
  title_label_->SetVerticalAlignment(gfx::ALIGN_TOP);
  title_label_->SetMultiLine(true);
  title_label_->SetMaxLines(kHoverCardTitleMaxLines);

  title_fade_label_ = AddChildView(std::make_unique<FadeLabel>(
      std::u16string(), CONTEXT_TAB_HOVER_CARD_TITLE,
      views::style::STYLE_PRIMARY));
  title_fade_label_->SetHorizontalAlignment(gfx::ALIGN_LEFT);
  title_fade_label_->SetVerticalAlignment(gfx::ALIGN_TOP);
  title_fade_label_->SetMultiLine(true);
  title_fade_label_->SetMaxLines(kHoverCardTitleMaxLines);
  title_fade_label_->GetViewAccessibility().OverrideIsIgnored(true);

  domain_label_ = AddChildView(std::make_unique<views::Label>(
      std::u16string(), views::style::CONTEXT_DIALOG_BODY_TEXT,
      views::style::STYLE_SECONDARY,
      gfx::DirectionalityMode::DIRECTIONALITY_AS_URL));
  domain_label_->SetElideBehavior(gfx::ELIDE_MIDDLE);
  domain_label_->SetHorizontalAlignment(gfx::ALIGN_LEFT);
  domain_label_->SetMultiLine(false);

  domain_fade_label_ = AddChildView(std::make_unique<FadeLabel>(
      std::u16string(), views::style::CONTEXT_DIALOG_BODY_TEXT,
      views::style::STYLE_SECONDARY,
      gfx::DirectionalityMode::DIRECTIONALITY_AS_URL));
  domain_fade_label_->SetElideBehavior(gfx::ELIDE_MIDDLE);
  domain_fade_label_->SetHorizontalAlignment(gfx::ALIGN_LEFT);
  domain_fade_label_->SetMultiLine(false);
  domain_fade_label_->GetViewAccessibility().OverrideIsIgnored(true);

  if (TabHoverCardController::AreHoverCardImagesEnabled()) {
    thumbnail_view_ = AddChildView(std::make_unique<ThumbnailView>());
    thumbnail_view_->SetRoundedCorners(
        ThumbnailView::RoundedCorners::kBottomCorners,
        corner_radius_.value_or(0));
  }

  // Set up layout.

  views::FlexLayout* const layout =
      SetLayoutManager(std::make_unique<views::FlexLayout>());
  layout->SetOrientation(views::LayoutOrientation::kVertical);
  layout->SetMainAxisAlignment(views::LayoutAlignment::kStart);
  layout->SetCrossAxisAlignment(views::LayoutAlignment::kStretch);
  layout->SetCollapseMargins(true);
  layout->SetChildViewIgnoredByLayout(title_fade_label_, true);
  layout->SetChildViewIgnoredByLayout(domain_fade_label_, true);

  constexpr int kHorizontalMargin = 18;
  constexpr int kVerticalMargin = 10;

  gfx::Insets title_margins(kVerticalMargin, kHorizontalMargin);

  // In some browser types (e.g. ChromeOS terminal app) we hide the domain
  // label. In those cases, we need to adjust the bottom margin of the title
  // element because it is no longer above another text element and needs a
  // bottom margin.
  const bool show_domain = tab->controller()->ShowDomainInHoverCards();
  domain_label_->SetVisible(show_domain);
  if (show_domain) {
    title_margins.set_bottom(0);
    domain_label_->SetProperty(
        views::kMarginsKey,
        gfx::Insets(0, kHorizontalMargin, kVerticalMargin, kHorizontalMargin));
  }

  title_label_->SetProperty(views::kMarginsKey, title_margins);
  title_label_->SetProperty(
      views::kFlexBehaviorKey,
      views::FlexSpecification(views::MinimumFlexSizeRule::kScaleToMinimum,
                               views::MaximumFlexSizeRule::kUnbounded));

  // Set up widget.

  views::BubbleDialogDelegateView::CreateBubble(this);
  set_adjust_if_offscreen(true);

#if defined(OS_LINUX)
  // Ensure the hover card Widget assumes the highest z-order to avoid occlusion
  // by other secondary UI Widgets (such as the omnibox Widget, see
  // crbug.com/1226536).
  GetWidget()->StackAtTop();
#endif

  constexpr int kFootnoteVerticalMargin = 8;
  GetBubbleFrameView()->SetFootnoteMargins(
      gfx::Insets(kFootnoteVerticalMargin, kHorizontalMargin,
                  kFootnoteVerticalMargin, kHorizontalMargin));
  GetBubbleFrameView()->SetPreferredArrowAdjustment(
      views::BubbleFrameView::PreferredArrowAdjustment::kOffset);
  GetBubbleFrameView()->set_hit_test_transparent(true);

  if (using_rounded_corners())
    GetBubbleFrameView()->SetCornerRadius(corner_radius_.value());

  // Placeholder image should be used when there is no image data for the
  // given tab. Otherwise don't flash the placeholder while we wait for the
  // existing thumbnail to be decompressed.
  //
  // Note that this code has to go after CreateBubble() above, since setting up
  // the placeholder image and background color require a ThemeProvider, which
  // is only available once this View has been added to its widget.
  if (thumbnail_view_ && !tab->data().thumbnail->has_data() &&
      !tab->IsActive()) {
    thumbnail_view_->SetPlaceholderImage();
  }

  // Start in the fully "faded-in" position so that whatever text we initially
  // display is visible.
  SetTextFade(1.0);
}

TabHoverCardBubbleView::~TabHoverCardBubbleView() = default;

ax::mojom::Role TabHoverCardBubbleView::GetAccessibleWindowRole() {
  // Override the role so that hover cards are not read when they appear because
  // tabs handle accessibility text.
  return ax::mojom::Role::kNone;
}

void TabHoverCardBubbleView::Layout() {
  View::Layout();
  title_fade_label_->SetBoundsRect(title_label_->bounds());
  domain_fade_label_->SetBoundsRect(domain_label_->bounds());
}

void TabHoverCardBubbleView::UpdateCardContent(const Tab* tab) {
  // Preview image is never visible for the active tab.
  if (thumbnail_view_) {
    if (tab->IsActive())
      thumbnail_view_->Hide();
    else
      thumbnail_view_->Show();
  }

  std::u16string title;
  absl::optional<TabAlertState> old_alert_state = alert_state_;
  GURL domain_url;
  // Use committed URL to determine if no page has yet loaded, since the title
  // can be blank for some web pages.
  if (tab->data().last_committed_url.is_empty()) {
    domain_url = tab->data().visible_url;
    title = tab->data().IsCrashed()
                ? l10n_util::GetStringUTF16(IDS_HOVER_CARD_CRASHED_TITLE)
                : l10n_util::GetStringUTF16(IDS_TAB_LOADING_TITLE);
    alert_state_ = absl::nullopt;
  } else {
    domain_url = tab->data().last_committed_url;
    title = tab->data().title;
    alert_state_ = Tab::GetAlertStateToShow(tab->data().alert_state);
  }
  std::u16string domain;
  if (domain_url.SchemeIsFile()) {
    title_label_->SetMultiLine(false);
    title_label_->SetElideBehavior(gfx::ELIDE_MIDDLE);
    domain = l10n_util::GetStringUTF16(IDS_HOVER_CARD_FILE_URL_SOURCE);
  } else {
    title_label_->SetElideBehavior(gfx::ELIDE_TAIL);
    title_label_->SetMultiLine(true);
    if (domain_url.SchemeIsBlob()) {
      domain = l10n_util::GetStringUTF16(IDS_HOVER_CARD_BLOB_URL_SOURCE);
    } else {
      domain = url_formatter::FormatUrl(
          domain_url,
          url_formatter::kFormatUrlOmitDefaults |
              url_formatter::kFormatUrlOmitHTTPS |
              url_formatter::kFormatUrlOmitTrivialSubdomains |
              url_formatter::kFormatUrlTrimAfterHost,
          net::UnescapeRule::NORMAL, nullptr, nullptr, nullptr);
    }
  }
  title_fade_label_->SetText(title_label_->GetText());
  title_label_->SetText(title);

  if (alert_state_ != old_alert_state) {
    GetBubbleFrameView()->SetFootnoteView(
        alert_state_.has_value() ? CreateAlertView(*alert_state_) : nullptr);
  }

  // We only clip the corners of the fade image when there isn't a footer.
  if (thumbnail_view_) {
    thumbnail_view_->SetRoundedCorners(
        GetBubbleFrameView()->GetFootnoteView()
            ? ThumbnailView::RoundedCorners::kNone
            : ThumbnailView::RoundedCorners::kBottomCorners,
        corner_radius_.value_or(0));
  }

  domain_fade_label_->SetText(domain_label_->GetText());
  domain_label_->SetText(domain);
}

void TabHoverCardBubbleView::SetTextFade(double percent) {
  title_fade_label_->SetFade(percent);
  domain_fade_label_->SetFade(percent);
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

// static
absl::optional<double> TabHoverCardBubbleView::GetPreviewImageCrossfadeStart() {
  static const double start_percent = base::GetFieldTrialParamByFeatureAsDouble(
      features::kTabHoverCardImages,
      features::kTabHoverCardImagesCrossfadePreviewAtParameterName, -1.0);
  return start_percent >= 0.0
             ? absl::make_optional(base::ClampToRange(start_percent, 0.0, 1.0))
             : absl::nullopt;
}

gfx::Size TabHoverCardBubbleView::CalculatePreferredSize() const {
  gfx::Size preferred_size = GetLayoutManager()->GetPreferredSize(this);
  preferred_size.set_width(TabStyle::GetPreviewImageSize().width());
  DCHECK(!preferred_size.IsEmpty());
  return preferred_size;
}

void TabHoverCardBubbleView::OnThemeChanged() {
  BubbleDialogDelegateView::OnThemeChanged();

  // Bubble closes if the theme changes to the point where the border has to be
  // regenerated. See crbug.com/1140256
  if (using_rounded_corners() != CustomShadowsSupported()) {
    GetWidget()->Close();
    return;
  }

  // Update fade labels' background color to match that of the the original
  // label since these child views are ignored by layout.
  title_fade_label_->SetBackgroundColor(title_label_->GetBackgroundColor());
  domain_fade_label_->SetBackgroundColor(domain_label_->GetBackgroundColor());
}

BEGIN_METADATA(TabHoverCardBubbleView, views::BubbleDialogDelegateView)
END_METADATA
