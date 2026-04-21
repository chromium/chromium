// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/views/tabs/hovercard/tab_hover_card_bubble_view.h"

#include <algorithm>
#include <ios>
#include <limits>
#include <memory>
#include <optional>
#include <string>
#include <string_view>

#include "base/byte_size.h"
#include "base/containers/lru_cache.h"
#include "base/memory/raw_ptr.h"
#include "base/metrics/field_trial_params.h"
#include "base/metrics/histogram_macros.h"
#include "build/build_config.h"
#include "chrome/app/vector_icons/vector_icons.h"
#include "chrome/browser/browser_process.h"
#include "chrome/browser/ui/color/chrome_color_id.h"
#include "chrome/browser/ui/performance_controls/tab_resource_usage_tab_helper.h"
#include "chrome/browser/ui/tabs/public/tab_features.h"
#include "chrome/browser/ui/tabs/saved_tab_groups/collaboration_messaging_tab_data.h"
#include "chrome/browser/ui/tabs/tab_data.h"
#include "chrome/browser/ui/tabs/tab_group_data.h"
#include "chrome/browser/ui/tabs/tab_style.h"
#include "chrome/browser/ui/thumbnails/thumbnail_image.h"
#include "chrome/browser/ui/ui_features.h"
#include "chrome/browser/ui/views/chrome_typography.h"
#include "chrome/browser/ui/views/tabs/hovercard/fade_label_view.h"
#include "chrome/browser/ui/views/tabs/hovercard/hover_card_anchor_target.h"
#include "chrome/browser/ui/views/tabs/tab_style_views.h"
#include "components/collaboration/public/messaging/message.h"
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

// Border spacing for the tab group header hovercard.
constexpr auto kGroupHovercardBorderMargins = gfx::Insets::VH(6, 12);
// Margins space surrounding each text element for the tab group header
// hovercard.
constexpr auto kGroupTitleMargins = gfx::Insets::VH(6, 0);

// Calculates an appropriate size to display a preview image in the hover card.
// For the vast majority of images, the `preferred_size` is used, but extremely
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
    SetImageFromIcon(ImageType::kPlaceholder, kGlobeIcon);
  }

  // Clears the preview image and replaces it with a crashed image. The old
  // image will be faded out.
  void SetCrashedImage() {
    SetImageFromIcon(ImageType::kCrashed, kCrashedTabIcon);
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
  enum class ImageType {
    kNone,
    kNoneButWaiting,
    kPlaceholder,
    kCrashed,
    kThumbnail
  };

  void SetImageFromIcon(ImageType type, const gfx::VectorIcon& icon) {
    if (image_type_ == type) {
      return;
    }

    // Color provider may be null if there is no associated widget. In that case
    // there is nothing to render, and we can't get default colors to render
    // with anyway, so bail out.
    const auto* const color_provider = GetColorProvider();
    if (!color_provider) {
      init_placeholder_image_ = type == ImageType::kPlaceholder;
      return;
    }

    StartFadeOut();

    // Check the no-preview color and size to see if it needs to be
    // regenerated. DPI or theme change can cause a regeneration.
    const SkColor foreground_color =
        color_provider->GetColor(kColorTabHoverCardForeground);

    // Set the image. All sizes are in DIPs.
    // gfx::CreateVectorIcon() caches its result so there's no need to store
    // images here; if a particular size/color combination has already been
    // requested it will be low-cost to request it again.
    constexpr int kIconSize = 64;
    const gfx::ImageSkia image =
        gfx::CreateVectorIcon(icon, kIconSize, foreground_color);
    SetImage(target_tab_image_, image, type);
    image_type_ = type;
  }

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
            views::CreateSolidBackground(bubble_view_->background_color()));
        break;
      case ImageType::kPlaceholder:
      case ImageType::kCrashed:
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

  void AddedToWidget() override {
    if (init_placeholder_image_) {
      SetPlaceholderImage();
      init_placeholder_image_ = false;
    }
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
    // See: crbug.com/40789563
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

  bool init_placeholder_image_ = false;

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

// TabHoverCardBubbleView::TabCardView
// ----------------------------------------------------------
class TabHoverCardBubbleView::TabCardView : public views::View {
  METADATA_HEADER(TabCardView, views::View)

 public:
  explicit TabCardView(TabHoverCardBubbleView* bubble_view)
      : bubble_view_(bubble_view) {
    CHECK(bubble_view_);
    SetProperty(views::kElementIdentifierKey, kTabCardElementId);

    title_label_ = AddChildView(std::make_unique<FadeLabelView>(
        kHoverCardTitleMaxLines, CONTEXT_TAB_HOVER_CARD_TITLE,
        views::style::STYLE_BODY_3_EMPHASIS));

    domain_label_ = AddChildView(std::make_unique<FadeLabelView>(
        1, views::style::CONTEXT_DIALOG_BODY_TEXT, views::style::STYLE_BODY_4));
    domain_label_->SetEnabledColor(kColorTabHoverCardSecondaryText);

    if (bubble_view_->bubble_params().show_image_preview) {
      thumbnail_view_ =
          AddChildView(std::make_unique<ThumbnailView>(bubble_view_));
      thumbnail_view_->SetAnimationEnabled(
          bubble_view_->bubble_params().use_animation);
      thumbnail_view_->SetRoundedCorners(true, bubble_view_->corner_radius());
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
    layout->SetOrientation(views::LayoutOrientation::kVertical)
        .SetMainAxisAlignment(views::LayoutAlignment::kStart)
        .SetCrossAxisAlignment(views::LayoutAlignment::kStretch)
        .SetCollapseMargins(true);

    // In some browser types (e.g. ChromeOS terminal app) we hide the domain
    // label. In those cases, we need to adjust the bottom margin of the title
    // element because it is no longer above another text element and needs a
    // bottom margin.
    gfx::Insets title_margins = kTextMargins;
    domain_label_->SetVisible(bubble_view_->bubble_params().show_domain);
    domain_label_->SetProperty(views::kElementIdentifierKey,
                               kHoverCardDomainLabelElementId);
    if (bubble_view_->bubble_params().show_domain) {
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
  }

  void UpdateContent(const HoverCardAnchorTarget* anchor_target) {
    CHECK(std::holds_alternative<TabCardData>(anchor_target->data()));
    const TabCardData& card_data = std::get<TabCardData>(anchor_target->data());

    // Preview image is never visible for the active tab.
    if (thumbnail_view_) {
      const bool has_thumbnail =
          card_data.thumbnail && card_data.thumbnail->has_data();
      if (!anchor_target->NeedsToShowThumbnail() ||
          (card_data.is_tab_discarded && !has_thumbnail)) {
        thumbnail_view_->ClearImage();
      } else {
        thumbnail_view_->SetWaitingForImage();
      }
    }

    title_label_->SetData(card_data.title_data);
    domain_label_->SetData(card_data.domain_data);

    if (bubble_view_->bubble_params().show_domain) {
      const bool domain_visible = !card_data.domain_data.text.empty();
      domain_label_->SetVisible(domain_visible);
      gfx::Insets title_margins = kTextMargins;
      if (domain_visible) {
        title_margins.set_bottom(0);
      }
      title_label_->SetProperty(views::kMarginsKey, title_margins);
    }

    footer_view_->SetAlertData(card_data.alert_data);

    const bool show_collaboration_messaging =
        card_data.show_collaboration_messaging;

    const base::ByteSize tab_memory_usage =
        card_data.tab_resource_usage
            ? card_data.tab_resource_usage->memory_usage()
            : base::ByteSize(0);
    const bool is_high_memory_usage =
        card_data.tab_resource_usage
            ? card_data.tab_resource_usage->is_high_memory_usage()
            : false;

    // High memory usage notification is considered a tab alert. Show it even
    // if the memory usage in hover cards pref is disabled.
    // However, collaboration messaging takes precedence over memory usage for
    // shared tabs.
    const bool show_memory_usage =
        ((tab_memory_usage.is_positive() &&
          bubble_view_->bubble_params().show_memory_usage) ||
         is_high_memory_usage) &&
        !show_collaboration_messaging && !card_data.show_discard_status;

    footer_view_->SetPerformanceData(
        {show_memory_usage, is_high_memory_usage, tab_memory_usage});

    CollaborationMessagingRowData collaboration_messaging_row_data;
    collaboration_messaging_row_data.should_show_collaboration_messaging =
        show_collaboration_messaging;
    collaboration_messaging_row_data.text = card_data.collaboration_message;
    collaboration_messaging_row_data.avatar =
        tab_groups::CollaborationMessagingTabData::GetHoverCardImage(
            GetWidget(), card_data.collaboration_avatar,
            /*has_message*/ card_data.collaboration_message.size() > 0u);

    footer_view_->SetCollaborationMessagingData(
        {collaboration_messaging_row_data});

    const bool show_footer = card_data.alert_data.alert_state.has_value() ||
                             card_data.show_discard_status ||
                             show_memory_usage || show_collaboration_messaging;

    if (thumbnail_view_) {
      // We only clip the corners of the fade image when there isn't a footer.
      thumbnail_view_->SetRoundedCorners(!show_footer,
                                         bubble_view_->corner_radius());
    }
  }

  ThumbnailView* thumbnail_view() { return thumbnail_view_; }
  FadeLabelView* title_label() { return title_label_; }
  FadeLabelView* domain_label() { return domain_label_; }
  FooterView* footer_view() { return footer_view_; }

  void SetTextFade(double percent) {
    title_label_->SetFade(percent);
    domain_label_->SetFade(percent);
    footer_view_->SetFade(percent);
  }

 private:
  raw_ptr<FadeLabelView> title_label_ = nullptr;
  raw_ptr<FadeLabelView> domain_label_ = nullptr;
  raw_ptr<FooterView> footer_view_ = nullptr;
  raw_ptr<ThumbnailView> thumbnail_view_ = nullptr;
  const raw_ptr<TabHoverCardBubbleView> bubble_view_;
};

BEGIN_METADATA(TabHoverCardBubbleView, TabCardView)
END_METADATA

// TabHoverCardBubbleView::GroupCardView
// ----------------------------------------------------------
class TabHoverCardBubbleView::GroupCardView : public views::View {
  METADATA_HEADER(GroupCardView, views::View)

 public:
  explicit GroupCardView(TabHoverCardBubbleView* bubble_view)
      : tab_titles_(tabs::TabGroupData::kMaxTabs, nullptr) {
    SetProperty(views::kElementIdentifierKey, kGroupCardElementId);

    title_ = AddChildView(std::make_unique<FadeLabelView>(
        kHoverCardTitleMaxLines, CONTEXT_TAB_HOVER_CARD_TITLE,
        views::style::STYLE_BODY_3_EMPHASIS));

    SetBorder(views::CreateEmptyBorder(kGroupHovercardBorderMargins));
    title_->SetProperty(views::kMarginsKey, kGroupTitleMargins);

    for (raw_ptr<FadeLabelView>& label : tab_titles_) {
      label = AddChildView(std::make_unique<FadeLabelView>(
          1, views::style::CONTEXT_DIALOG_BODY_TEXT,
          views::style::STYLE_BODY_4));
      label->SetProperty(views::kMarginsKey, kGroupTitleMargins);
      label->SetEnabledColor(kColorTabHoverCardSecondaryText);
    }

    footer_ = AddChildView(std::make_unique<FadeLabelView>(
        1, views::style::CONTEXT_DIALOG_BODY_TEXT, views::style::STYLE_BODY_4));
    footer_->SetProperty(views::kMarginsKey, kGroupTitleMargins);
    footer_->SetEnabledColor(kColorTabHoverCardSecondaryText);

    views::FlexLayout* const layout =
        SetLayoutManager(std::make_unique<views::FlexLayout>());
    layout->SetOrientation(views::LayoutOrientation::kVertical)
        .SetMainAxisAlignment(views::LayoutAlignment::kStart)
        .SetCrossAxisAlignment(views::LayoutAlignment::kStretch)
        .SetCollapseMargins(true);
  }

  void UpdateContent(const HoverCardAnchorTarget* anchor_target) {
    CHECK(std::holds_alternative<GroupCardData>(anchor_target->data()));
    const GroupCardData& card_data =
        std::get<GroupCardData>(anchor_target->data());

    title_->SetData(card_data.group_title_data);

    for (size_t i = 0; i < tab_titles_.size(); ++i) {
      if (i >= card_data.tab_title_data.size()) {
        tab_titles_[i]->SetData({u"", false});
        tab_titles_[i]->SetVisible(false);
      } else {
        tab_titles_[i]->SetData(card_data.tab_title_data[i]);
        tab_titles_[i]->SetVisible(true);
      }
    }

    footer_->SetData(card_data.excess_tab_data);
    if (card_data.excess_tab_data.text.empty()) {
      footer_->SetVisible(false);
    } else {
      footer_->SetVisible(true);
    }
  }

  void SetTextFade(double percent) {
    title_->SetFade(percent);
    for (raw_ptr<FadeLabelView> label : tab_titles_) {
      label->SetFade(percent);
    }
    footer_->SetFade(percent);
  }

  FadeLabelView* title() const { return title_; }
  const std::vector<raw_ptr<FadeLabelView>>& tab_titles() const {
    return tab_titles_;
  }
  FadeLabelView* footer() const { return footer_; }

 private:
  raw_ptr<FadeLabelView> title_ = nullptr;
  std::vector<raw_ptr<FadeLabelView>> tab_titles_;
  raw_ptr<FadeLabelView> footer_ = nullptr;
};
BEGIN_METADATA(TabHoverCardBubbleView, GroupCardView)
END_METADATA

// static
constexpr base::TimeDelta TabHoverCardBubbleView::kHoverCardSlideDuration;

DEFINE_CLASS_ELEMENT_IDENTIFIER_VALUE(TabHoverCardBubbleView,
                                      kHoverCardBubbleElementId);
DEFINE_CLASS_ELEMENT_IDENTIFIER_VALUE(TabHoverCardBubbleView,
                                      kHoverCardDomainLabelElementId);
DEFINE_CLASS_ELEMENT_IDENTIFIER_VALUE(TabHoverCardBubbleView,
                                      kTabCardElementId);
DEFINE_CLASS_ELEMENT_IDENTIFIER_VALUE(TabHoverCardBubbleView,
                                      kGroupCardElementId);

TabHoverCardBubbleView::TabHoverCardBubbleView(
    HoverCardAnchorTarget* anchor_target,
    const InitParams& params)
    : BubbleDialogDelegateView(anchor_target->GetAnchor(),
                               anchor_target->GetAnchorPosition(),
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

  views::FlexLayout* const layout =
      SetLayoutManager(std::make_unique<views::FlexLayout>());
  layout->SetOrientation(views::LayoutOrientation::kVertical)
      .SetMainAxisAlignment(views::LayoutAlignment::kStart)
      .SetCrossAxisAlignment(views::LayoutAlignment::kStretch)
      .SetCollapseMargins(true);

  SetProperty(views::kElementIdentifierKey, kHoverCardBubbleElementId);

  tab_card_view_ = AddChildView(std::make_unique<TabCardView>(this));

  if (std::holds_alternative<TabCardData>(anchor_target->data())) {
    const TabCardData& card_data = std::get<TabCardData>(anchor_target->data());
    bool valid_thumbnail = card_data.thumbnail &&
                           card_data.thumbnail->has_data() &&
                           anchor_target->NeedsToShowThumbnail();
    if (tab_card_view_->thumbnail_view() && !valid_thumbnail) {
      // Placeholder image should be used when there is no image data for the
      // given tab. Otherwise don't flash the placeholder while we wait for the
      // existing thumbnail to be decompressed.
      //
      // We tell the ThumbnailView to initialize with a place holder image when
      // it is added to the widget. Note that the ThumbnailView has to set the
      // placeholder image after CreateBubble() below, since setting up the
      // placeholder image and background color require a ColorProvider,
      // which is only available once this View has been added to its widget.
      tab_card_view_->thumbnail_view()->SetPlaceholderImage();
    }
  }

  group_card_view_ = AddChildView(std::make_unique<GroupCardView>(this));

  // Create the widget from the view. Additional setup happens in
  // `AddedToWidget()`.
  views::BubbleDialogDelegateView::CreateBubble(this);
}

TabHoverCardBubbleView::~TabHoverCardBubbleView() = default;

void TabHoverCardBubbleView::UpdateCardContent(
    const HoverCardAnchorTarget* anchor_target) {
  tab_card_view_->SetVisible(
      std::holds_alternative<TabCardData>(anchor_target->data()));
  group_card_view_->SetVisible(
      std::holds_alternative<GroupCardData>(anchor_target->data()));

  if (std::holds_alternative<TabCardData>(anchor_target->data())) {
    tab_card_view_->UpdateContent(anchor_target);
  } else {
    CHECK(std::holds_alternative<GroupCardData>(anchor_target->data()));
    group_card_view_->UpdateContent(anchor_target);
  }
}

void TabHoverCardBubbleView::SetTextFade(double percent) {
  tab_card_view_->SetTextFade(percent);
  group_card_view_->SetTextFade(percent);
}

void TabHoverCardBubbleView::SetTargetTabImage(gfx::ImageSkia preview_image) {
  DCHECK(tab_card_view_);
  DCHECK(tab_card_view_->thumbnail_view())
      << "This method should only be called when preview images are enabled.";
  tab_card_view_->thumbnail_view()->SetTargetTabImage(preview_image);
}

void TabHoverCardBubbleView::SetPlaceholderImage() {
  DCHECK(tab_card_view_);
  DCHECK(tab_card_view_->thumbnail_view())
      << "This method should only be called when preview images are enabled.";
  tab_card_view_->thumbnail_view()->SetPlaceholderImage();
}

void TabHoverCardBubbleView::SetCrashedImage() {
  DCHECK(tab_card_view_);
  DCHECK(tab_card_view_->thumbnail_view())
      << "This method should only be called when preview images are enabled.";
  tab_card_view_->thumbnail_view()->SetCrashedImage();
}

views::View* TabHoverCardBubbleView::GetTabCardViewForTesting() {
  return tab_card_view_.get();
}

views::View* TabHoverCardBubbleView::GetGroupCardViewForTesting() {
  return group_card_view_.get();
}

FadeLabelView* TabHoverCardBubbleView::GetTitleViewForTesting() const {
  return tab_card_view_->title_label();
}
FadeLabelView* TabHoverCardBubbleView::GetDomainViewForTesting() const {
  return tab_card_view_->domain_label();
}

views::View* TabHoverCardBubbleView::GetThumbnailViewForTesting() {
  return tab_card_view_->thumbnail_view();
}

FooterView* TabHoverCardBubbleView::GetFooterViewForTesting() {
  return tab_card_view_->footer_view();
}

FadeLabelView* TabHoverCardBubbleView::GetGroupTitleViewForTesting() const {
  return group_card_view_->title();
}

const std::vector<raw_ptr<FadeLabelView>>&
TabHoverCardBubbleView::GetGroupTabTitleViewsForTesting() const {
  return group_card_view_->tab_titles();
}

FadeLabelView* TabHoverCardBubbleView::GetGroupFooterViewForTesting() const {
  return group_card_view_->footer();
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

void TabHoverCardBubbleView::AddedToWidget() {
  set_adjust_if_offscreen(true);
  GetBubbleFrameView()->SetPreferredArrowAdjustment(
      views::BubbleFrameView::PreferredArrowAdjustment::kOffset);
  GetBubbleFrameView()->set_hit_test_transparent(true);

  GetBubbleFrameView()->SetRoundedCorners(gfx::RoundedCornersF(corner_radius_));

  // Start in the fully "faded-in" position so that whatever text we initially
  // display is visible. For TBD reasons, this needs to be done after the
  // CreateBubble() call, or the crossfades have an incorrect background color.
  SetTextFade(1.0);
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

// While the hover card is sliding, we do not want it to reposition itself
// according to the anchor view that it is observing. It is usually
// undergoing slide or fade animations. When in the middle of those
// animations, reacting to changes to the anchor view can cause visual
// flickering with the hover card bounds.
// See crbug.com/486948335 for an example.
void TabHoverCardBubbleView::OnAnchorBoundsChanged() {
  if (!sliding_) {
    BubbleDialogDelegateView::OnAnchorBoundsChanged();
  }
}

BEGIN_METADATA(TabHoverCardBubbleView)
END_METADATA
