// Copyright (c) 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/views/tabs/tab_hover_card_bubble_view.h"

#include <algorithm>
#include <memory>

#include "base/containers/mru_cache.h"
#include "base/metrics/field_trial_params.h"
#include "base/metrics/histogram_macros.h"
#include "build/build_config.h"
#include "chrome/app/vector_icons/vector_icons.h"
#include "chrome/browser/themes/theme_properties.h"
#include "chrome/browser/ui/tabs/tab_renderer_data.h"
#include "chrome/browser/ui/tabs/tab_style.h"
#include "chrome/browser/ui/thumbnails/thumbnail_image.h"
#include "chrome/browser/ui/views/chrome_layout_provider.h"
#include "chrome/browser/ui/views/chrome_typography.h"
#include "chrome/browser/ui/views/tabs/tab.h"
#include "chrome/browser/ui/views/tabs/tab_hover_card_controller.h"
#include "chrome/grit/generated_resources.h"
#include "components/url_formatter/url_formatter.h"
#include "ui/accessibility/ax_enums.mojom.h"
#include "ui/base/l10n/l10n_util.h"
#include "ui/base/theme_provider.h"
#include "ui/gfx/animation/linear_animation.h"
#include "ui/gfx/animation/slide_animation.h"
#include "ui/gfx/animation/tween.h"
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
#include "ui/views/layout/flex_layout.h"
#include "ui/views/layout/layout_provider.h"
#include "ui/views/metadata/metadata_header_macros.h"
#include "ui/views/metadata/metadata_impl_macros.h"
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
      float{preview_size.width()} / preview_size.height();
  const float preferred_aspect_ratio =
      float{preferred_size.width()} / preferred_size.height();
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

TabHoverCardBubbleView::TabHoverCardBubbleView(Tab* tab)
    : BubbleDialogDelegateView(tab,
                               views::BubbleBorder::TOP_LEFT,
                               views::BubbleBorder::STANDARD_SHADOW),
      using_rounded_corners_(CustomShadowsSupported()) {
  SetButtons(ui::DIALOG_BUTTON_NONE);

  // We'll do all of our own layout inside the bubble, so no need to inset this
  // view inside the client view.
  set_margins(gfx::Insets());

  // Inset the tab hover cards anchor rect to bring the card closer to the tab.
  constexpr gfx::Insets kTabHoverCardAnchorInsets(2, 0);
  set_anchor_view_insets(kTabHoverCardAnchorInsets);

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
    using Alignment = views::ImageView::Alignment;
    const gfx::Size preview_size = TabStyle::GetPreviewImageSize();
    preview_image_ = AddChildView(std::make_unique<views::ImageView>());
    preview_image_->SetVisible(true);
    preview_image_->SetHorizontalAlignment(Alignment::kCenter);
    preview_image_->SetVerticalAlignment(Alignment::kCenter);
    preview_image_->SetImageSize(preview_size);
    preview_image_->SetPreferredSize(preview_size);
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
                               views::MaximumFlexSizeRule::kPreferred));

  // Set up widget.

  views::BubbleDialogDelegateView::CreateBubble(this);
  set_adjust_if_offscreen(true);

  constexpr int kFootnoteVerticalMargin = 8;
  GetBubbleFrameView()->SetFootnoteMargins(
      gfx::Insets(kFootnoteVerticalMargin, kHorizontalMargin,
                  kFootnoteVerticalMargin, kHorizontalMargin));
  GetBubbleFrameView()->SetPreferredArrowAdjustment(
      views::BubbleFrameView::PreferredArrowAdjustment::kOffset);
  GetBubbleFrameView()->set_hit_test_transparent(true);

  if (using_rounded_corners_) {
    GetBubbleFrameView()->SetCornerRadius(
        ChromeLayoutProvider::Get()->GetCornerRadiusMetric(
            views::Emphasis::kHigh));
  }

  // Start in the fully "faded-in" position so that whatever text we initially
  // display is visible.
  SetTextFade(1.0);
}

TabHoverCardBubbleView::~TabHoverCardBubbleView() = default;

ax::mojom::Role TabHoverCardBubbleView::GetAccessibleWindowRole() {
  // Override the role so that hover cards are not read when they appear because
  // tabs handle accessibility text.
  return ax::mojom::Role::kIgnored;
}

void TabHoverCardBubbleView::Layout() {
  View::Layout();
  title_fade_label_->SetBoundsRect(title_label_->bounds());
  domain_fade_label_->SetBoundsRect(domain_label_->bounds());
}

void TabHoverCardBubbleView::UpdateCardContent(const Tab* tab) {
  // Preview image is never visible for the active tab.
  if (preview_image_)
    preview_image_->SetVisible(!tab->IsActive());

  std::u16string title;
  base::Optional<TabAlertState> old_alert_state = alert_state_;
  GURL domain_url;
  // Use committed URL to determine if no page has yet loaded, since the title
  // can be blank for some web pages.
  if (tab->data().last_committed_url.is_empty()) {
    domain_url = tab->data().visible_url;
    title = tab->data().IsCrashed()
                ? l10n_util::GetStringUTF16(IDS_HOVER_CARD_CRASHED_TITLE)
                : l10n_util::GetStringUTF16(IDS_TAB_LOADING_TITLE);
    alert_state_ = base::nullopt;
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

  domain_fade_label_->SetText(domain_label_->GetText());
  domain_label_->SetText(domain);

  // Because we may have changed the card's contents, if the card has yet to be
  // shown, ensure that it starts at the correct size.
  if (!GetWidget()->IsVisible())
    SizeToContents();
}

void TabHoverCardBubbleView::SetTextFade(double percent) {
  title_fade_label_->SetFade(percent);
  domain_fade_label_->SetFade(percent);
}

void TabHoverCardBubbleView::ClearPreviewImage() {
  DCHECK(preview_image_)
      << "This method should only be called when preview images are enabled.";

  // This can return null if there is no associated widget, etc. In that case
  // there is nothing to render, and we can't get theme default colors to render
  // with anyway, so bail out. This should hopefully address crbug.com/1070980
  // (Null dereference
  const ui::ThemeProvider* const theme_provider = GetThemeProvider();
  if (!theme_provider)
    return;

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
  preview_image_->SetImage(no_preview_image);
  preview_image_->SetImageSize(kNoPreviewImageSize);
  preview_image_->SetPreferredSize(TabStyle::GetPreviewImageSize());

  // Also possibly regenerate the background if it has changed.
  const SkColor background_color = theme_provider->GetColor(
      ThemeProperties::COLOR_HOVER_CARD_NO_PREVIEW_BACKGROUND);
  if (!preview_image_->background() ||
      preview_image_->background()->get_color() != background_color) {
    preview_image_->SetBackground(
        views::CreateSolidBackground(background_color));
  }
}

void TabHoverCardBubbleView::SetPreviewImage(gfx::ImageSkia preview_image) {
  DCHECK(preview_image_)
      << "This method should only be called when preview images are enabled.";

  const gfx::Size preview_size = TabStyle::GetPreviewImageSize();
  preview_image_->SetImage(preview_image);
  preview_image_->SetImageSize(
      GetPreviewImageSize(preview_image.size(), preview_size));
  preview_image_->SetPreferredSize(preview_size);
  preview_image_->SetBackground(nullptr);
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
  if (using_rounded_corners_ != CustomShadowsSupported()) {
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
