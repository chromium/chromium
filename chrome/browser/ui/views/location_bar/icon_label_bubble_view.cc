// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/views/location_bar/icon_label_bubble_view.h"

#include <algorithm>
#include <memory>
#include <utility>

#include "base/functional/bind.h"
#include "chrome/browser/ui/color/chrome_color_id.h"
#include "chrome/browser/ui/layout_constants.h"
#include "chrome/browser/ui/omnibox/omnibox_theme.h"
#include "chrome/browser/ui/views/location_bar/location_bar_util.h"
#include "ui/base/l10n/l10n_util.h"
#include "ui/base/metadata/metadata_impl_macros.h"
#include "ui/compositor/layer_animator.h"
#include "ui/compositor/scoped_layer_animation_settings.h"
#include "ui/gfx/canvas.h"
#include "ui/gfx/geometry/insets.h"
#include "ui/gfx/geometry/rect.h"
#include "ui/gfx/geometry/skia_conversions.h"
#include "ui/gfx/scoped_canvas.h"
#include "ui/views/accessibility/ax_virtual_view.h"
#include "ui/views/accessibility/view_accessibility.h"
#include "ui/views/animation/flood_fill_ink_drop_ripple.h"
#include "ui/views/animation/ink_drop.h"
#include "ui/views/animation/ink_drop_highlight.h"
#include "ui/views/animation/ink_drop_impl.h"
#include "ui/views/animation/ink_drop_mask.h"
#include "ui/views/animation/ink_drop_ripple.h"
#include "ui/views/background.h"
#include "ui/views/border.h"
#include "ui/views/controls/focus_ring.h"
#include "ui/views/controls/highlight_path_generator.h"
#include "ui/views/controls/image_view.h"
#include "ui/views/style/platform_style.h"
#include "ui/views/widget/widget.h"

namespace {

// Amount of space reserved for the separator that appears after the icon or
// label.
constexpr int kIconLabelBubbleSeparatorWidth = 1;

// Amount of space on either side of the separator that appears after the icon
// or label.
constexpr int kIconLabelBubbleSpaceBesideSeparator = 8;

// The length of the separator's fade animation. These values are empirical.
constexpr int kIconLabelBubbleFadeInDurationMs = 250;
constexpr int kIconLabelBubbleFadeOutDurationMs = 175;

// The length of the label fade in and out animations.
constexpr int kIconLabelAnimationDurationMs = 600;

}  // namespace

SkAlpha IconLabelBubbleView::Delegate::GetIconLabelBubbleSeparatorAlpha()
    const {
  return 0x69;
}

SkColor IconLabelBubbleView::Delegate::GetIconLabelBubbleInkDropColor() const {
  return GetIconLabelBubbleSurroundingForegroundColor();
}

IconLabelBubbleView::SeparatorView::SeparatorView(IconLabelBubbleView* owner) {
  DCHECK(owner);
  owner_ = owner;

  SetPaintToLayer();
  layer()->SetFillsBoundsOpaquely(false);
}

void IconLabelBubbleView::SeparatorView::OnPaint(gfx::Canvas* canvas) {
  // This uses the surrounding context as the base color instead of
  // IconLabelBubbleView::GetForegroundColor() so that if the
  // IconLabelBubbleView has been emphasized (e.g. red text for a security
  // error) the separator will still blend into the background.
  const SkColor separator_color = SkColorSetA(
      owner_->delegate_->GetIconLabelBubbleSurroundingForegroundColor(),
      owner_->delegate_->GetIconLabelBubbleSeparatorAlpha());
  const float x = GetLocalBounds().right() -
                  owner_->GetEndPaddingWithSeparator() -
                  1.0f / canvas->image_scale();
  canvas->DrawLine(gfx::PointF(x, GetLocalBounds().y()),
                   gfx::PointF(x, GetLocalBounds().bottom()), separator_color);
}

void IconLabelBubbleView::SeparatorView::OnThemeChanged() {
  views::View::OnThemeChanged();
  SchedulePaint();
}

void IconLabelBubbleView::SeparatorView::UpdateOpacity() {
  if (!GetVisible()) {
    return;
  }

  // When using focus rings are visible we should hide the separator instantly
  // when the IconLabelBubbleView is focused. Otherwise we should follow the
  // inkdrop.
  if (views::FocusRing::Get(owner_) && owner_->HasFocus()) {
    layer()->SetOpacity(0.0f);
    return;
  }

  views::InkDrop* const ink_drop = views::InkDrop::Get(owner_)->GetInkDrop();
  DCHECK(ink_drop);

  // If an inkdrop highlight or ripple is animating in or visible, the
  // separator should fade out.
  views::InkDropState state = ink_drop->GetTargetInkDropState();
  float opacity = 0.0f;
  float duration = kIconLabelBubbleFadeOutDurationMs;
  if (!ink_drop->IsHighlightFadingInOrVisible() &&
      (state == views::InkDropState::HIDDEN ||
       state == views::InkDropState::ACTION_TRIGGERED ||
       state == views::InkDropState::DEACTIVATED)) {
    opacity = 1.0f;
    duration = kIconLabelBubbleFadeInDurationMs;
  }

  ui::ScopedLayerAnimationSettings animation(layer()->GetAnimator());
  animation.SetTransitionDuration(base::Milliseconds(duration));
  animation.SetTweenType(gfx::Tween::Type::EASE_IN);
  layer()->SetOpacity(opacity);
}

BEGIN_METADATA(IconLabelBubbleView, SeparatorView)
END_METADATA

class IconLabelBubbleView::HighlightPathGenerator
    : public views::HighlightPathGenerator {
 public:
  HighlightPathGenerator() = default;

  HighlightPathGenerator(const HighlightPathGenerator&) = delete;
  HighlightPathGenerator& operator=(const HighlightPathGenerator&) = delete;

  // views::HighlightPathGenerator:
  SkPath GetHighlightPath(const views::View* view) override {
    return static_cast<const IconLabelBubbleView*>(view)->GetHighlightPath();
  }
};

IconLabelBubbleView::IconLabelBubbleView(const gfx::FontList& font_list,
                                         Delegate* delegate)
    : delegate_(delegate),
      separator_view_(AddChildView(std::make_unique<SeparatorView>(this))) {
  DCHECK(delegate_);

  SetFontList(font_list);
  SetHorizontalAlignment(gfx::ALIGN_LEFT);

  separator_view_->SetVisible(ShouldShowSeparator());

  views::InkDrop::Get(this)->SetVisibleOpacity(kOmniboxOpacitySelected);
  views::InkDrop::Get(this)->SetHighlightOpacity(kOmniboxOpacityHovered);

  views::InkDrop::Get(this)->SetCreateInkDropCallback(base::BindRepeating(
      [](IconLabelBubbleView* host) {
        std::unique_ptr<views::InkDrop> ink_drop =
            views::InkDrop::CreateInkDropForFloodFillRipple(
                views::InkDrop::Get(host), /*highlight_on_hover=*/true,
                /*highlight_on_focus=*/!views::FocusRing::Get(host));
        ink_drop->AddObserver(host);
        return ink_drop;
      },
      this));
  views::InkDrop::Get(this)->SetBaseColorCallback(base::BindRepeating(
      [](IconLabelBubbleView* host) {
        return host->delegate_->GetIconLabelBubbleInkDropColor();
      },
      this));

  views::HighlightPathGenerator::Install(
      this, std::make_unique<HighlightPathGenerator>());
  SetFocusBehavior(views::PlatformStyle::kDefaultFocusBehavior);

  UpdateBorder();

  SetNotifyEnterExitOnChild(true);

  // Flip the canvas in RTL so the separator is drawn on the correct side.
  separator_view_->SetFlipCanvasOnPaintForRTLUI(true);

  auto alert_view = std::make_unique<views::AXVirtualView>();
  alert_view->GetCustomData().role = ax::mojom::Role::kAlert;
  alert_view->GetCustomData().AddState(ax::mojom::State::kInvisible);
  alert_virtual_view_ = alert_view.get();
  GetViewAccessibility().AddVirtualChildView(std::move(alert_view));
}

IconLabelBubbleView::~IconLabelBubbleView() {}

void IconLabelBubbleView::InkDropAnimationStarted() {
  separator_view_->UpdateOpacity();
}

void IconLabelBubbleView::InkDropRippleAnimationEnded(
    views::InkDropState state) {}

bool IconLabelBubbleView::ShouldShowLabel() const {
  if (slide_animation_.is_animating() || is_animation_paused_) {
    return !IsShrinking() ||
           (width() > image_container_view()->GetPreferredSize().width());
  }
  return label()->GetVisible() && !label()->GetText().empty();
}

void IconLabelBubbleView::Layout(PassKey) {
  ink_drop_container()->SetBoundsRect(GetLocalBounds());

  // We may not have horizontal room for both the image and the trailing
  // padding. When the view is expanding (or showing-label steady state), the
  // image. When the view is contracting (or hidden-label steady state), whittle
  // away at the trailing padding instead.
  int bubble_trailing_padding = GetEndPaddingWithSeparator();
  int image_width = image_container_view()->GetPreferredSize().width();
  const int space_shortage = image_width + bubble_trailing_padding - width();
  if (space_shortage > 0) {
    if (ShouldShowLabel()) {
      image_width -= space_shortage;
    } else {
      bubble_trailing_padding -= space_shortage;
    }
  }
  image_container_view()->SetBounds(GetInsets().left(), 0, image_width,
                                    height());

  // Compute the label bounds. The label gets whatever size is left over after
  // accounting for the preferred image width and padding amounts. Note that if
  // the label has zero size it doesn't actually matter what we compute its X
  // value to be, since it won't be visible.
  const int label_x =
      image_container_view()->bounds().right() + GetInternalSpacing();
  int label_width = std::max(0, width() - label_x - bubble_trailing_padding -
                                    GetWidthBetweenIconAndSeparator());
  label()->SetBounds(label_x, 0, label_width, height());

  // The separator should be the same height as the icons.
  const int separator_height = GetLayoutConstant(LOCATION_BAR_ICON_SIZE);
  gfx::Rect separator_bounds(label()->bounds());
  separator_bounds.Inset(
      gfx::Insets::VH((separator_bounds.height() - separator_height) / 2, 0));

  float separator_width =
      GetWidthBetweenIconAndSeparator() + GetEndPaddingWithSeparator();
  int separator_x = label()->GetText().empty()
                        ? image_container_view()->bounds().right()
                        : label()->bounds().right();
  separator_view_->SetBounds(separator_x, separator_bounds.y(), separator_width,
                             separator_height);

  if (views::FocusRing::Get(this)) {
    views::FocusRing::Get(this)->DeprecatedLayoutImmediately();
    views::FocusRing::Get(this)->SchedulePaint();
  }
}

void IconLabelBubbleView::SetBackgroundVisibility(
    BackgroundVisibility background_visibility) {
  background_visibility_ = background_visibility;
  UpdateBackground();
}

void IconLabelBubbleView::SetLabel(const std::u16string& label_text) {
  SetLabel(label_text, label_text);
}

void IconLabelBubbleView::SetLabel(const std::u16string& label_text,
                                   const std::u16string& accessible_name) {
  // TODO(crbug.com/40890218): Under what conditions, if any, will the text be
  // empty? Read the description of the bug and update accordingly.
  GetViewAccessibility().SetName(
      accessible_name, accessible_name.empty()
                           ? ax::mojom::NameFrom::kAttributeExplicitlyEmpty
                           : ax::mojom::NameFrom::kAttribute);
  label()->SetText(label_text);
  separator_view_->SetVisible(ShouldShowSeparator());
  separator_view_->UpdateOpacity();
}

void IconLabelBubbleView::SetFontList(const gfx::FontList& font_list) {
  label()->SetFontList(font_list);
}

SkColor IconLabelBubbleView::GetBackgroundColor() const {
  ui::ColorId background_color_id = ui::kUiColorsStart;
  if (background_color_id_.has_value()) {
    background_color_id = background_color_id_.value();
  } else if (PaintedOnSolidBackground()) {
    background_color_id = use_tonal_color_when_expanded_
                              ? kColorPageInfoBackgroundTonal
                              : kColorPageInfoBackground;
  } else {
    // If background is not explicitly specified or we are not painting over a
    // solid background, seek the background color from the icon view's context.
    return delegate_->GetIconLabelBubbleBackgroundColor();
  }
  return GetColorProvider()->GetColor(background_color_id);
}

SkColor IconLabelBubbleView::GetForegroundColor() const {
  ui::ColorId foreground_color_id = ui::kUiColorsStart;
  if (foreground_color_id_.has_value()) {
    foreground_color_id = foreground_color_id_.value();
  } else if (PaintedOnSolidBackground()) {
    foreground_color_id = use_tonal_color_when_expanded_
                              ? kColorPageInfoForegroundTonal
                              : kColorPageInfoForeground;
  } else {
    // If foreground is not explicitly specified or we are not painting over a
    // solid background, seek the foreground color from the icon view's context.
    return delegate_->GetIconLabelBubbleSurroundingForegroundColor();
  }
  return GetColorProvider()->GetColor(foreground_color_id);
}

bool IconLabelBubbleView::IconColorShouldMatchForeground() const {
  // Icons should match the label foreground color if the foreground color is
  // explicitly overridden or solid backgrounds are used.
  return foreground_color_id_.has_value() || PaintedOnSolidBackground();
}

void IconLabelBubbleView::SetCustomForegroundColorId(
    const ui::ColorId color_id) {
  if (foreground_color_id_ == color_id) {
    return;
  }
  foreground_color_id_ = color_id;
}

void IconLabelBubbleView::SetCustomBackgroundColorId(
    const ui::ColorId color_id) {
  if (background_color_id_ == color_id) {
    return;
  }
  background_color_id_ = color_id;
}

void IconLabelBubbleView::UpdateLabelColors() {
  SetEnabledTextColors(GetForegroundColor());
  label()->SetBackgroundColor(GetBackgroundColor());
}

void IconLabelBubbleView::UpdateBackground() {
  if (!GetWidget()) {
    return;
  }

  // If the label is showing we must ensure the icon label is painted over a
  // solid background.
  const bool painted_on_solid_background = PaintedOnSolidBackground();
  SetBackground(painted_on_solid_background
                    ? views::CreateRoundedRectBackground(
                          GetBackgroundColor(), GetPreferredSize().height())
                    : nullptr);
  // TODO(pbos): Consider renaming kPageInfo/kPageAction color IDs to share the
  // same prefix. Here PageInfo assumes to have a background and PageAction
  // assumes to not have one.
  ConfigureInkDropForRefresh2023(this,
                                 painted_on_solid_background
                                     ? kColorPageInfoIconHover
                                     : kColorPageActionIconHover,
                                 kColorPageInfoIconPressed);
}

void IconLabelBubbleView::SetUseTonalColorsWhenExpanded(bool use_tonal_colors) {
  use_tonal_color_when_expanded_ = use_tonal_colors;
}

bool IconLabelBubbleView::ShouldShowSeparator() const {
  return ShouldShowLabel();
}

bool IconLabelBubbleView::ShouldShowLabelAfterAnimation() const {
  return ShouldShowSeparator();
}

int IconLabelBubbleView::GetWidthBetween(int min, int max) const {
  // TODO(crbug.com/41420184): Disable animations globally instead of having
  // piecemeal opt ins for respecting prefers reduced motion.
  if (gfx::Animation::PrefersReducedMotion()) {
    return max;
  }

  if (!slide_animation_.is_animating() && !is_animation_paused_) {
    return max;
  }

  double progress = is_animation_paused_ ? pause_animation_state_
                                         : slide_animation_.GetCurrentValue();
  // This tween matches the default for SlideAnimation.
  const gfx::Tween::Type kTween = gfx::Tween::EASE_OUT;
  if (progress < open_state_fraction_) {
    double state =
        gfx::Tween::CalculateValue(kTween, progress / open_state_fraction_);
    return gfx::Tween::IntValueBetween(state, min, max);
  }

  if (progress <= (1 - open_state_fraction_)) {
    return max;
  }

  // Clamp value to 1.0 to handle floating arithmetic rounding errors.
  double state = gfx::Tween::CalculateValue(
      kTween, std::min(1.0, (progress - (1 - open_state_fraction_)) /
                                open_state_fraction_));
  // Note |min| and |max| are reversed.
  return gfx::Tween::IntValueBetween(state, max, min);
}

bool IconLabelBubbleView::IsShrinking() const {
  if (!slide_animation_.is_animating() || is_animation_paused_) {
    return false;
  }
  return slide_animation_.IsClosing() ||
         (open_state_fraction_ < 1.0 &&
          slide_animation_.GetCurrentValue() > (1.0 - open_state_fraction_));
}

bool IconLabelBubbleView::ShowBubble(const ui::Event& event) {
  return false;
}

bool IconLabelBubbleView::IsBubbleShowing() const {
  return false;
}

void IconLabelBubbleView::OnTouchUiChanged() {
  UpdateBorder();

  // PreferredSizeChanged() incurs an expensive layout of the location bar, so
  // only call it when this view is showing.
  if (GetVisible()) {
    PreferredSizeChanged();
  }
}

gfx::Size IconLabelBubbleView::CalculatePreferredSize(
    const views::SizeBounds& available_size) const {
  return GetSizeForLabelWidth(
      label()
          ->GetPreferredSize(views::SizeBounds(label()->width(), {}))
          .width());
}

bool IconLabelBubbleView::OnMousePressed(const ui::MouseEvent& event) {
  suppress_button_release_ = IsBubbleShowing();
  return LabelButton::OnMousePressed(event);
}

void IconLabelBubbleView::OnThemeChanged() {
  LabelButton::OnThemeChanged();

  // LabelButton::OnThemeChanged() sets a views::Background on the label
  // under certain conditions. We don't want that, so unset the background.
  label()->SetBackground(nullptr);

  UpdateLabelColors();
  UpdateBackground();
}

bool IconLabelBubbleView::IsTriggerableEvent(const ui::Event& event) {
  if (event.IsMouseEvent()) {
    return !IsBubbleShowing() && !suppress_button_release_;
  }

  return true;
}

bool IconLabelBubbleView::ShouldUpdateInkDropOnClickCanceled() const {
  // The click might be cancelled because the bubble is still opened. In this
  // case, the ink drop state should not be hidden by Button.
  return false;
}

void IconLabelBubbleView::NotifyClick(const ui::Event& event) {
  LabelButton::NotifyClick(event);
  ShowBubble(event);
}

void IconLabelBubbleView::OnFocus() {
  separator_view_->UpdateOpacity();
  LabelButton::OnFocus();
}

void IconLabelBubbleView::OnBlur() {
  separator_view_->UpdateOpacity();
  LabelButton::OnBlur();
}

void IconLabelBubbleView::AnimationEnded(const gfx::Animation* animation) {
  if (animation != &slide_animation_) {
    return views::LabelButton::AnimationEnded(animation);
  }

  if (!is_animation_paused_) {
    // In some cases we want the text to disappear even after animating.
    // Subclasses override `ShouldShowLabelAfterAnimation` for custom behavior.
    // Default behavior is when we do not show separator, the label should
    // collapse.
    ResetSlideAnimation(/*show_label=*/ShouldShowLabelAfterAnimation());
    PreferredSizeChanged();
  }

  views::InkDrop::Get(this)->GetInkDrop()->SetShowHighlightOnHover(true);
  views::InkDrop::Get(this)->GetInkDrop()->SetShowHighlightOnFocus(
      !views::FocusRing::Get(this));
  UpdateBackground();
  UpdateBorder();
}

void IconLabelBubbleView::AnimationProgressed(const gfx::Animation* animation) {
  if (animation != &slide_animation_) {
    return views::LabelButton::AnimationProgressed(animation);
  }

  if (!is_animation_paused_) {
    PreferredSizeChanged();
  }

  UpdateBorder();
}

void IconLabelBubbleView::AnimationCanceled(const gfx::Animation* animation) {
  if (animation != &slide_animation_) {
    return views::LabelButton::AnimationCanceled(animation);
  }

  AnimationEnded(animation);
}

void IconLabelBubbleView::SetImageModel(const ui::ImageModel& image_model) {
  DCHECK(!image_model.IsEmpty());
  LabelButton::SetImageModel(STATE_NORMAL, image_model);
}

gfx::Size IconLabelBubbleView::GetSizeForLabelWidth(int label_width) const {
  gfx::Size image_size = image_container_view()->GetPreferredSize();
  image_size.Enlarge(GetInsets().left() + GetWidthBetweenIconAndSeparator() +
                         GetEndPaddingWithSeparator(),
                     GetInsets().height());

  const bool shrinking = IsShrinking();
  // The out portion of the in-out animation continues for the last few pixels
  // even after the label is not visible in order to slide the icon into its
  // final position. Therefore it is necessary to calculate additional width
  // even when the label is hidden as long as the animation is still shrinking.
  if (!ShouldShowLabel() && !shrinking) {
    return image_size;
  }

  const int min_width =
      shrinking ? image_size.width() : grow_animation_starting_width_;
  const int max_width = image_size.width() + GetInternalSpacing() + label_width;

  return gfx::Size(GetWidthBetween(min_width, max_width), image_size.height());
}

void IconLabelBubbleView::UpdateBorder() {
  // Bubbles are given the full internal height of the location bar so that all
  // child views in the location bar have the same height. The visible height of
  // the bubble should be smaller, so use an empty border to shrink down the
  // content bounds so the background gets painted correctly.
  SetBorder(views::CreateEmptyBorder(gfx::Insets::VH(
      GetLayoutConstant(LOCATION_BAR_CHILD_INTERIOR_PADDING),
      GetLayoutInsets(LOCATION_BAR_ICON_INTERIOR_PADDING).left())));
}

int IconLabelBubbleView::GetInternalSpacing() const {
  if (image_container_view()->GetPreferredSize().IsEmpty()) {
    return 0;
  }

  constexpr int kDefaultInternalSpacingTouchUI = 10;
  constexpr int kDefaultInternalSpacingChromeRefresh = 4;

  return (ui::TouchUiController::Get()->touch_ui()
              ? kDefaultInternalSpacingTouchUI
              : kDefaultInternalSpacingChromeRefresh) +
         GetExtraInternalSpacing();
}

int IconLabelBubbleView::GetExtraInternalSpacing() const {
  return 0;
}

int IconLabelBubbleView::GetWidthBetweenIconAndSeparator() const {
  return ShouldShowSeparator() ? kIconLabelBubbleSpaceBesideSeparator : 0;
}

int IconLabelBubbleView::GetEndPaddingWithSeparator() const {
  int end_padding = ShouldShowSeparator() ? kIconLabelBubbleSpaceBesideSeparator
                                          : GetInsets().right();
  if (ShouldShowSeparator()) {
    end_padding += kIconLabelBubbleSeparatorWidth;
  }
  return end_padding;
}

void IconLabelBubbleView::SetUpForAnimation() {
  views::InkDrop::Get(this)->SetMode(views::InkDropHost::InkDropMode::ON);
  SetFocusBehavior(views::PlatformStyle::kDefaultFocusBehavior);
  label()->SetElideBehavior(gfx::NO_ELIDE);
  label()->SetVisible(false);
  slide_animation_.SetSlideDuration(base::Milliseconds(150));
  open_state_fraction_ = 1.0;
}

void IconLabelBubbleView::SetUpForInOutAnimation(base::TimeDelta duration) {
  SetUpForAnimation();
  // The duration of the slide includes the appearance of the label (600ms),
  // statically showing the label (1800ms), and hiding the label (600ms). The
  // proportion of time spent in each portion of the animation is controlled by
  // open_state_fraction_.
  slide_animation_.SetSlideDuration(
      duration + 2 * base::Milliseconds(kIconLabelAnimationDurationMs));
  // The tween is calculated in GetWidthBetween().
  slide_animation_.SetTweenType(gfx::Tween::LINEAR);
  open_state_fraction_ = static_cast<float>(kIconLabelAnimationDurationMs) /
                         duration.InMilliseconds();
}

void IconLabelBubbleView::AnimateIn(std::optional<int> string_id) {
  if (!label()->GetVisible()) {
    // Start animation from the current width, otherwise the icon will also be
    // included if visible.
    grow_animation_starting_width_ = GetVisible() ? width() : 0;
    if (string_id) {
      std::u16string label = l10n_util::GetStringUTF16(string_id.value());
      SetLabel(label);

      // Send an accessibility alert whose text is the label's text. Doing this
      // causes a screenreader to immediately announce the text of the button,
      // which serves to announce it. This is done unconditionally here if there
      // is text because the animation is intended to draw attention to the
      // instance anyway.
      alert_virtual_view_->GetCustomData().RemoveState(
          ax::mojom::State::kInvisible);

      // A valid role must be set prior to setting the name.
      // TODO(crbug.com/40863593): Consider using AnnounceText instead of a
      // virtual view.
      alert_virtual_view_->GetCustomData().role = ax::mojom::Role::kAlert;
      alert_virtual_view_->GetCustomData().SetNameChecked(label);
      alert_virtual_view_->NotifyAccessibilityEvent(ax::mojom::Event::kAlert);
    }
    label()->SetVisible(true);
    ShowAnimation();
  }
}

void IconLabelBubbleView::AnimateOut() {
  if (label()->GetVisible()) {
    label()->SetVisible(false);
    alert_virtual_view_->GetCustomData().AddState(ax::mojom::State::kInvisible);
    alert_virtual_view_->NotifyAccessibilityEvent(ax::mojom::Event::kHide);
    HideAnimation();
  }
}

void IconLabelBubbleView::ResetSlideAnimation(bool show_label) {
  label()->SetVisible(show_label);
  slide_animation_.Reset(show_label);
}

void IconLabelBubbleView::ReduceAnimationTimeForTesting() {
  slide_animation_.SetSlideDuration(base::Milliseconds(1));
}

void IconLabelBubbleView::PauseAnimation() {
  if (slide_animation_.is_animating()) {
    // If the user clicks while we're animating, the bubble arrow will be
    // pointing to the image, and if we allow the animation to keep running, the
    // image will move away from the arrow (or we'll have to move the bubble,
    // which is even worse). So we want to stop the animation.  We have two
    // choices: jump to the final post-animation state (no label visible), or
    // pause the animation where we are and continue running after the bubble
    // closes. The former looks more jerky, so we avoid it unless the animation
    // hasn't even fully exposed the image yet, in which case pausing with half
    // an image visible will look broken.
    if (!is_animation_paused_ && ShouldShowLabel()) {
      is_animation_paused_ = true;
      pause_animation_state_ = slide_animation_.GetCurrentValue();
    }
    slide_animation_.Reset();
  }
}

void IconLabelBubbleView::UnpauseAnimation() {
  if (is_animation_paused_) {
    slide_animation_.Reset(pause_animation_state_);
    is_animation_paused_ = false;
    ShowAnimation();
  }
}

double IconLabelBubbleView::GetAnimationValue() const {
  return slide_animation_.GetCurrentValue();
}

void IconLabelBubbleView::ShowAnimation() {
  slide_animation_.Show();
  views::InkDrop::Get(this)->GetInkDrop()->SetShowHighlightOnHover(false);
  views::InkDrop::Get(this)->GetInkDrop()->SetShowHighlightOnFocus(false);
  UpdateBackground();
}

void IconLabelBubbleView::HideAnimation() {
  slide_animation_.Hide();
  views::InkDrop::Get(this)->GetInkDrop()->SetShowHighlightOnHover(false);
  views::InkDrop::Get(this)->GetInkDrop()->SetShowHighlightOnFocus(false);
  UpdateBackground();
}

// TODO(josephjoopark): Refactor using addCircle().
SkPath IconLabelBubbleView::GetHighlightPath() const {
  gfx::Rect highlight_bounds = GetLocalBounds();
  if (ShouldShowSeparator()) {
    highlight_bounds.Inset(
        gfx::Insets::TLBR(0, 0, 0, GetEndPaddingWithSeparator()));
  }
  highlight_bounds = GetMirroredRect(highlight_bounds);

  const float corner_radius = highlight_bounds.height() / 2.f;
  const SkRect rect = RectToSkRect(highlight_bounds);

  return SkPath().addRoundRect(rect, corner_radius, corner_radius);
  // return SkPath().addCircle(12, radius, radius); // size / 2
}

bool IconLabelBubbleView::PaintedOnSolidBackground() const {
  // If the label is showing we must ensure the icon label is painted over a
  // solid background.
  return (background_visibility_ == BackgroundVisibility::kAlways) ||
         ((background_visibility_ == BackgroundVisibility::kWithLabel) &&
          ShouldShowLabel()) ||
         background_color_id_.has_value();
}

BEGIN_METADATA(IconLabelBubbleView)
ADD_READONLY_PROPERTY_METADATA(SkColor,
                               ForegroundColor,
                               ui::metadata::SkColorConverter)
ADD_READONLY_PROPERTY_METADATA(double, AnimationValue)
ADD_READONLY_PROPERTY_METADATA(int, InternalSpacing)
ADD_READONLY_PROPERTY_METADATA(int, ExtraInternalSpacing)
ADD_READONLY_PROPERTY_METADATA(int, WidthBetweenIconAndSeparator)
ADD_READONLY_PROPERTY_METADATA(int, EndPaddingWithSeparator)
END_METADATA
