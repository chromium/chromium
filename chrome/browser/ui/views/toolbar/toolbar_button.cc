// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/views/toolbar/toolbar_button.h"

#include <utility>

#include "base/bind.h"
#include "base/feature_list.h"
#include "base/location.h"
#include "base/single_thread_task_runner.h"
#include "base/threading/thread_task_runner_handle.h"
#include "build/build_config.h"
#include "chrome/browser/themes/theme_properties.h"
#include "chrome/browser/themes/theme_service.h"
#include "chrome/browser/themes/theme_service_factory.h"
#include "chrome/browser/ui/layout_constants.h"
#include "chrome/browser/ui/tabs/tab_strip_model.h"
#include "chrome/browser/ui/ui_features.h"
#include "chrome/browser/ui/views/chrome_layout_provider.h"
#include "chrome/browser/ui/views/chrome_typography.h"
#include "chrome/browser/ui/views/toolbar/toolbar_ink_drop_util.h"
#include "ui/accessibility/ax_enums.mojom.h"
#include "ui/accessibility/ax_node_data.h"
#include "ui/base/models/menu_model.h"
#include "ui/display/display.h"
#include "ui/display/screen.h"
#include "ui/gfx/color_utils.h"
#include "ui/gfx/text_utils.h"
#include "ui/views/animation/ink_drop.h"
#include "ui/views/animation/ink_drop_highlight.h"
#include "ui/views/animation/installable_ink_drop.h"
#include "ui/views/background.h"
#include "ui/views/controls/button/label_button_border.h"
#include "ui/views/controls/menu/menu_item_view.h"
#include "ui/views/controls/menu/menu_model_adapter.h"
#include "ui/views/controls/menu/menu_runner.h"
#include "ui/views/view_class_properties.h"
#include "ui/views/widget/widget.h"

namespace {

constexpr int kBorderThicknessDpWithLabel = 1;
constexpr int kBorderThicknessDpWithoutLabel = 2;

SkColor GetDefaultTextColor(const ui::ThemeProvider* theme_provider) {
  DCHECK(theme_provider);
  // TODO(crbug.com/967317): Update to match mocks, i.e. return
  // gfx::kGoogleGrey900, if needed.
  return color_utils::GetColorWithMaxContrast(
      theme_provider->GetColor(ThemeProperties::COLOR_TOOLBAR));
}

SkColor GetDefaultBackgroundColor(const ui::ThemeProvider* theme_provider) {
  return color_utils::GetColorWithMaxContrast(
      GetDefaultTextColor(theme_provider));
}

}  // namespace

ToolbarButton::ToolbarButton(views::ButtonListener* listener)
    : ToolbarButton(listener, nullptr, nullptr) {}

ToolbarButton::ToolbarButton(views::ButtonListener* listener,
                             std::unique_ptr<ui::MenuModel> model,
                             TabStripModel* tab_strip_model,
                             bool trigger_menu_on_long_press)
    : views::LabelButton(listener, base::string16(), CONTEXT_TOOLBAR_BUTTON),
      model_(std::move(model)),
      tab_strip_model_(tab_strip_model),
      trigger_menu_on_long_press_(trigger_menu_on_long_press),
      highlight_color_animation_(this) {
  set_has_ink_drop_action_on_click(true);
  set_context_menu_controller(this);

  if (base::FeatureList::IsEnabled(views::kInstallableInkDropFeature)) {
    installable_ink_drop_ = std::make_unique<views::InstallableInkDrop>(this);
    installable_ink_drop_->SetConfig(GetToolbarInstallableInkDropConfig(this));
  }

  InstallToolbarButtonHighlightPathGenerator(this);

  SetInkDropMode(InkDropMode::ON);

  // Make sure icons are flipped by default so that back, forward, etc. follows
  // UI direction.
  EnableCanvasFlippingForRTLUI(true);

  set_ink_drop_visible_opacity(kToolbarInkDropVisibleOpacity);

  SetImageLabelSpacing(ChromeLayoutProvider::Get()->GetDistanceMetric(
      DISTANCE_RELATED_LABEL_HORIZONTAL_LIST));
  SetHorizontalAlignment(gfx::ALIGN_RIGHT);

  // Because we're using the internal padding to keep track of the changes we
  // make to the leading margin to handle Fitts' Law, it's easier to just
  // allocate the property once and modify the value.
  SetProperty(views::kInternalPaddingKey, gfx::Insets());

  UpdateColorsAndInsets();
}

ToolbarButton::~ToolbarButton() {}

void ToolbarButton::Init() {
  SetFocusBehavior(FocusBehavior::ACCESSIBLE_ONLY);
}

void ToolbarButton::SetHighlight(const base::string16& highlight_text,
                                 base::Optional<SkColor> highlight_color) {
  if (highlight_text.empty() && !highlight_color.has_value()) {
    ClearHighlight();
    return;
  }

  highlight_color_animation_.Show(highlight_color);
  LabelButton::SetText(highlight_text);
}

void ToolbarButton::SetText(const base::string16& text) {
  NOTREACHED() << "Use SetHighlight(text, _) instead of SetText(text)";
}

void ToolbarButton::ClearHighlight() {
  highlight_color_animation_.Hide();
  ShrinkDownThenClearText();
}

void ToolbarButton::UpdateColorsAndInsets() {
  const int highlight_radius =
      ChromeLayoutProvider::Get()->GetCornerRadiusMetric(
          views::EMPHASIS_MAXIMUM, size());

  base::Optional<SkColor> text_color =
      highlight_color_animation_.GetTextColor();
  if (text_color) {
    SetEnabledTextColors(*text_color);
  } else {
    ResetColorsFromNativeTheme();
  }

  base::Optional<SkColor> background_color =
      highlight_color_animation_.GetBackgroundColor();
  if (background_color) {
    // ToolbarButtons are always the height the location bar.
    const gfx::Insets bg_insets =
        gfx::Insets((height() - GetLayoutConstant(LOCATION_BAR_HEIGHT)) / 2) +
        *GetProperty(views::kInternalPaddingKey);
    SetBackground(views::CreateBackgroundFromPainter(
        views::Painter::CreateSolidRoundRectPainter(
            *background_color, highlight_radius, bg_insets)));
  } else {
    SetBackground(nullptr);
  }

  gfx::Insets new_insets =
      layout_insets_.value_or(GetLayoutInsets(TOOLBAR_BUTTON)) +
      layout_inset_delta_ + *GetProperty(views::kInternalPaddingKey);
  base::Optional<SkColor> border_color =
      highlight_color_animation_.GetBorderColor();
  if (!border() || new_insets != border()->GetInsets() ||
      last_border_color_ != border_color) {
    if (border_color) {
      int border_thickness_dp = GetText().empty()
                                    ? kBorderThicknessDpWithoutLabel
                                    : kBorderThicknessDpWithLabel;
      // Create a border with insets equal to |new_insets|, just split into a
      // solid border and padding.
      SetBorder(views::CreatePaddedBorder(
          views::CreateRoundedRectBorder(border_thickness_dp, highlight_radius,
                                         *border_color),
          new_insets - gfx::Insets(border_thickness_dp)));
    } else {
      SetBorder(views::CreateEmptyBorder(new_insets));
    }
    last_border_color_ = border_color;
  }

  // Update spacing on the outer-side of the label to match the current
  // highlight radius.
  SetLabelSideSpacing(highlight_radius / 2);
}

void ToolbarButton::SetLabelSideSpacing(int spacing) {
  gfx::Insets label_insets;
  // Add the spacing only if text is non-empty.
  if (!GetText().empty()) {
    // Add spacing to the opposing side.
    label_insets =
        gfx::MaybeFlipForRTL(GetHorizontalAlignment()) == gfx::ALIGN_RIGHT
            ? gfx::Insets(0, spacing, 0, 0)
            : gfx::Insets(0, 0, 0, spacing);
  }
  if (!label()->border() || label_insets != label()->border()->GetInsets()) {
    label()->SetBorder(views::CreateEmptyBorder(label_insets));
    // Forces LabelButton to dump the cached preferred size and recompute it.
    PreferredSizeChanged();
  }
}

void ToolbarButton::SetLayoutInsetDelta(const gfx::Insets& inset_delta) {
  if (layout_inset_delta_ == inset_delta)
    return;
  layout_inset_delta_ = inset_delta;
  UpdateColorsAndInsets();
}

void ToolbarButton::SetLeadingMargin(int margin) {
  gfx::Insets* const internal_padding = GetProperty(views::kInternalPaddingKey);
  if (internal_padding->left() == margin)
    return;
  internal_padding->set_left(margin);
  UpdateColorsAndInsets();
}

void ToolbarButton::SetTrailingMargin(int margin) {
  gfx::Insets* const internal_padding = GetProperty(views::kInternalPaddingKey);
  if (internal_padding->right() == margin)
    return;
  internal_padding->set_right(margin);
  UpdateColorsAndInsets();
}

void ToolbarButton::ClearPendingMenu() {
  show_menu_factory_.InvalidateWeakPtrs();
}

bool ToolbarButton::IsMenuShowing() const {
  return menu_showing_;
}

void ToolbarButton::SetLayoutInsets(const gfx::Insets& insets) {
  layout_insets_ = insets;
  UpdateColorsAndInsets();
}

void ToolbarButton::OnBoundsChanged(const gfx::Rect& previous_bounds) {
  UpdateColorsAndInsets();
  LabelButton::OnBoundsChanged(previous_bounds);
}

void ToolbarButton::OnThemeChanged() {
  LabelButton::OnThemeChanged();
  if (installable_ink_drop_)
    installable_ink_drop_->SetConfig(GetToolbarInstallableInkDropConfig(this));
}

gfx::Rect ToolbarButton::GetAnchorBoundsInScreen() const {
  gfx::Rect bounds = GetBoundsInScreen();
  gfx::Insets insets = GetToolbarInkDropInsets(this);
  // If the button is extended, don't inset the leading edge. The anchored menu
  // should extend to the screen edge as well so the menu is easier to hit
  // (Fitts's law).
  // TODO(pbos): Make sure the button is aware of that it is being extended or
  // not (leading_margin_ cannot be used as it can be 0 in fullscreen on Touch).
  // When this is implemented, use 0 as a replacement for leading_margin_ in
  // fullscreen only. Always keep the rest.
  insets.Set(insets.top(), 0, insets.bottom(), 0);
  bounds.Inset(insets);
  return bounds;
}

bool ToolbarButton::OnMousePressed(const ui::MouseEvent& event) {
  if (trigger_menu_on_long_press_ && IsTriggerableEvent(event) &&
      GetEnabled() && ShouldShowMenu() && HitTestPoint(event.location())) {
    // Store the y pos of the mouse coordinates so we can use them later to
    // determine if the user dragged the mouse down (which should pop up the
    // drag down menu immediately, instead of waiting for the timer)
    y_position_on_lbuttondown_ = event.y();

    // Schedule a task that will show the menu.
    base::ThreadTaskRunnerHandle::Get()->PostDelayedTask(
        FROM_HERE,
        base::BindOnce(&ToolbarButton::ShowDropDownMenu,
                       show_menu_factory_.GetWeakPtr(),
                       ui::GetMenuSourceTypeForEvent(event)),
        base::TimeDelta::FromMilliseconds(500));
  }

  return LabelButton::OnMousePressed(event);
}

bool ToolbarButton::OnMouseDragged(const ui::MouseEvent& event) {
  bool result = LabelButton::OnMouseDragged(event);

  if (trigger_menu_on_long_press_ && show_menu_factory_.HasWeakPtrs()) {
    // If the mouse is dragged to a y position lower than where it was when
    // clicked then we should not wait for the menu to appear but show
    // it immediately.
    if (event.y() > y_position_on_lbuttondown_ + GetHorizontalDragThreshold()) {
      show_menu_factory_.InvalidateWeakPtrs();
      ShowDropDownMenu(ui::GetMenuSourceTypeForEvent(event));
    }
  }

  return result;
}

void ToolbarButton::OnMouseReleased(const ui::MouseEvent& event) {
  if (IsTriggerableEvent(event) ||
      (event.IsRightMouseButton() && !HitTestPoint(event.location()))) {
    LabelButton::OnMouseReleased(event);
  }

  if (IsTriggerableEvent(event))
    show_menu_factory_.InvalidateWeakPtrs();
}

void ToolbarButton::OnMouseCaptureLost() {}

void ToolbarButton::OnMouseExited(const ui::MouseEvent& event) {
  // Starting a drag results in a MouseExited, we need to ignore it.
  // A right click release triggers an exit event. We want to
  // remain in a PUSHED state until the drop down menu closes.
  if (state() != STATE_DISABLED && !InDrag() && state() != STATE_PRESSED)
    SetState(STATE_NORMAL);
}

void ToolbarButton::OnGestureEvent(ui::GestureEvent* event) {
  if (menu_showing_) {
    // While dropdown menu is showing the button should not handle gestures.
    event->StopPropagation();
    return;
  }

  LabelButton::OnGestureEvent(event);
}

void ToolbarButton::GetAccessibleNodeData(ui::AXNodeData* node_data) {
  Button::GetAccessibleNodeData(node_data);
  node_data->role = ax::mojom::Role::kButton;
  if (model_)
    node_data->SetHasPopup(ax::mojom::HasPopup::kMenu);
  if (GetEnabled())
    node_data->SetDefaultActionVerb(ax::mojom::DefaultActionVerb::kPress);
}

std::unique_ptr<views::InkDrop> ToolbarButton::CreateInkDrop() {
  // Ensure this doesn't get called when InstallableInkDrops are enabled.
  DCHECK(!base::FeatureList::IsEnabled(views::kInstallableInkDropFeature));
  return views::LabelButton::CreateInkDrop();
}

std::unique_ptr<views::InkDropHighlight> ToolbarButton::CreateInkDropHighlight()
    const {
  // Ensure this doesn't get called when InstallableInkDrops are enabled.
  DCHECK(!base::FeatureList::IsEnabled(views::kInstallableInkDropFeature));
  return CreateToolbarInkDropHighlight(this);
}

SkColor ToolbarButton::GetInkDropBaseColor() const {
  // Ensure this doesn't get called when InstallableInkDrops are enabled.
  DCHECK(!base::FeatureList::IsEnabled(views::kInstallableInkDropFeature));
  base::Optional<SkColor> drop_base_color =
      highlight_color_animation_.GetInkDropBaseColor();
  if (drop_base_color)
    return *drop_base_color;
  return GetToolbarInkDropBaseColor(this);
}

views::InkDrop* ToolbarButton::GetInkDrop() {
  if (installable_ink_drop_)
    return installable_ink_drop_.get();
  return views::LabelButton::GetInkDrop();
}

void ToolbarButton::ShowContextMenuForViewImpl(View* source,
                                               const gfx::Point& point,
                                               ui::MenuSourceType source_type) {
  if (!GetEnabled())
    return;

  show_menu_factory_.InvalidateWeakPtrs();
  ShowDropDownMenu(source_type);
}

// static
SkColor ToolbarButton::AdjustHighlightColorForContrast(
    const ui::ThemeProvider* theme_provider,
    SkColor desired_dark_color,
    SkColor desired_light_color,
    SkColor dark_extreme,
    SkColor light_extreme) {
  if (!theme_provider)
    return desired_light_color;
  const SkColor background_color =
      base::FeatureList::IsEnabled(features::kAnimatedAvatarButton)
          ? GetDefaultBackgroundColor(theme_provider)
          : theme_provider->GetColor(ThemeProperties::COLOR_TOOLBAR);
  const SkColor contrasting_color = color_utils::PickContrastingColor(
      desired_dark_color, desired_light_color, background_color);
  const SkColor limit =
      contrasting_color == desired_dark_color ? dark_extreme : light_extreme;
  // Setting highlight color will set the text to the highlight color, and the
  // background to the same color with a low alpha. This means that our target
  // contrast is between the text (the highlight color) and a blend of the
  // highlight color and the toolbar color.
  const SkColor base_color = color_utils::AlphaBlend(
      contrasting_color, background_color, kToolbarButtonBackgroundAlpha);

  // Add a fudge factor to the minimum contrast ratio since we'll actually be
  // blending with the adjusted color.
  return color_utils::BlendForMinContrast(
             contrasting_color, base_color, limit,
             color_utils::kMinimumReadableContrastRatio * 1.05)
      .color;
}

// static
SkColor ToolbarButton::GetDefaultBorderColor(views::View* host_view) {
  return SkColorSetA(GetToolbarInkDropBaseColor(host_view),
                     kToolbarButtonBackgroundAlpha);
}

bool ToolbarButton::ShouldShowMenu() {
  return model_ != nullptr;
}

void ToolbarButton::ShowDropDownMenu(ui::MenuSourceType source_type) {
  if (!ShouldShowMenu())
    return;

  gfx::Rect menu_anchor_bounds = GetAnchorBoundsInScreen();

#if defined(OS_CHROMEOS)
  // A window won't overlap between displays on ChromeOS.
  // Use the left bound of the display on which
  // the menu button exists.
  gfx::NativeView view = GetWidget()->GetNativeView();
  display::Display display =
      display::Screen::GetScreen()->GetDisplayNearestView(view);
  int left_bound = display.bounds().x();
#else
  // The window might be positioned over the edge between two screens. We'll
  // want to position the dropdown on the screen the mouse cursor is on.
  display::Screen* screen = display::Screen::GetScreen();
  display::Display display =
      screen->GetDisplayNearestPoint(screen->GetCursorScreenPoint());
  int left_bound = display.bounds().x();
#endif
  if (menu_anchor_bounds.x() < left_bound)
    menu_anchor_bounds.set_x(left_bound);

  // Make the button look depressed while the menu is open.
  SetState(STATE_PRESSED);

  menu_showing_ = true;

  AnimateInkDrop(views::InkDropState::ACTIVATED, nullptr /* event */);

  // Exit if the model is null. Although ToolbarButton::ShouldShowMenu()
  // performs the same check, its overrides may not.
  if (!model_)
    return;

  if (tab_strip_model_ && !tab_strip_model_->GetActiveWebContents())
    return;

  // Create and run menu.
  menu_model_adapter_ = std::make_unique<views::MenuModelAdapter>(
      model_.get(), base::BindRepeating(&ToolbarButton::OnMenuClosed,
                                        base::Unretained(this)));
  menu_model_adapter_->set_triggerable_event_flags(triggerable_event_flags());
  menu_runner_ = std::make_unique<views::MenuRunner>(
      menu_model_adapter_->CreateMenu(), views::MenuRunner::HAS_MNEMONICS);
  menu_runner_->RunMenuAt(GetWidget(), nullptr, menu_anchor_bounds,
                          views::MenuAnchorPosition::kTopLeft, source_type);
}

void ToolbarButton::OnMenuClosed() {
  AnimateInkDrop(views::InkDropState::DEACTIVATED, nullptr /* event */);

  menu_showing_ = false;

  // Set the state back to normal after the drop down menu is closed.
  if (state() != STATE_DISABLED) {
    GetInkDrop()->SetHovered(IsMouseHovered());
    SetState(STATE_NORMAL);
  }

  menu_runner_.reset();
  menu_model_adapter_.reset();
}

const char* ToolbarButton::GetClassName() const {
  return "ToolbarButton";
}

namespace {

// The default duration does not work well for dark mode where the animation has
// to make a big contrast difference.
// TODO(crbug.com/967317): This needs to be consistent with the duration of the
// border animation in ToolbarIconContainerView.
constexpr base::TimeDelta kHighlightAnimationDuration =
    base::TimeDelta::FromMilliseconds(300);
constexpr SkAlpha kBackgroundBaseLayerAlpha = 204;

SkColor FadeWithAnimation(SkColor color, const gfx::Animation& animation) {
  return SkColorSetA(color, SkColorGetA(color) * animation.GetCurrentValue());
}

}  // namespace

ToolbarButton::HighlightColorAnimation::HighlightColorAnimation(
    ToolbarButton* parent)
    : parent_(parent), highlight_color_animation_(this) {
  DCHECK(parent_);
  highlight_color_animation_.SetTweenType(gfx::Tween::EASE_IN_OUT);
  highlight_color_animation_.SetSlideDuration(kHighlightAnimationDuration);
}

ToolbarButton::HighlightColorAnimation::~HighlightColorAnimation() {}

void ToolbarButton::HighlightColorAnimation::Show(
    base::Optional<SkColor> highlight_color) {
  if (base::FeatureList::IsEnabled(features::kAnimatedAvatarButton)) {
    // If the animation is showing, we will jump to a different color in the
    // middle of the animation and continue animating towards the new
    // |highlight_color_|. If the animation is fully shown, we will jump
    // directly to the new |highlight_color_|. This is not ideal but making it
    // smoother is not worth the extra complexity given this should be very
    // rare.
    if (highlight_color_animation_.GetCurrentValue() == 0.0f ||
        highlight_color_animation_.IsClosing()) {
      highlight_color_animation_.Show();
    }
  } else {
    // If the animation is disabled, jump directly to the final state.
    highlight_color_animation_.Reset(1.0f);
  }

  highlight_color_ = highlight_color;
  parent_->UpdateColorsAndInsets();
}

void ToolbarButton::HighlightColorAnimation::Hide() {
  if (base::FeatureList::IsEnabled(features::kAnimatedAvatarButton)) {
    highlight_color_animation_.Hide();
  } else {
    ClearHighlightColor();
  }
}

base::Optional<SkColor> ToolbarButton::HighlightColorAnimation::GetTextColor()
    const {
  if (!base::FeatureList::IsEnabled(features::kAnimatedAvatarButton))
    return highlight_color_;

  if (!IsShown() || !parent_->GetThemeProvider())
    return base::nullopt;
  SkColor text_color;
  if (highlight_color_) {
    text_color = *highlight_color_;
  } else {
    text_color = GetDefaultTextColor(parent_->GetThemeProvider());
  }
  return FadeWithAnimation(text_color, highlight_color_animation_);
}

base::Optional<SkColor> ToolbarButton::HighlightColorAnimation::GetBorderColor()
    const {
  if (!IsShown() ||
      !base::FeatureList::IsEnabled(features::kAnimatedAvatarButton) ||
      !parent_->GetThemeProvider()) {
    return base::nullopt;
  }

  SkColor border_color;
  if (highlight_color_) {
    border_color = *highlight_color_;
  } else {
    border_color = ToolbarButton::GetDefaultBorderColor(parent_);
  }
  return FadeWithAnimation(border_color, highlight_color_animation_);
}

base::Optional<SkColor>
ToolbarButton::HighlightColorAnimation::GetBackgroundColor() const {
  if (!base::FeatureList::IsEnabled(features::kAnimatedAvatarButton)) {
    if (!highlight_color_)
      return base::nullopt;
    return SkColorSetA(*highlight_color_, kToolbarButtonBackgroundAlpha);
  }

  if (!IsShown() || !parent_->GetThemeProvider())
    return base::nullopt;
  SkColor bg_color =
      SkColorSetA(GetDefaultBackgroundColor(parent_->GetThemeProvider()),
                  kBackgroundBaseLayerAlpha);
  if (highlight_color_) {
    // TODO(crbug.com/967317): Change the highlight opacity to 4% to match the
    // mocks, if needed.
    bg_color = color_utils::GetResultingPaintColor(
        /*fg=*/SkColorSetA(*highlight_color_,
                           SkColorGetA(*highlight_color_) *
                               kToolbarInkDropHighlightVisibleOpacity),
        /*bg=*/bg_color);
  }
  return FadeWithAnimation(bg_color, highlight_color_animation_);
}

base::Optional<SkColor>
ToolbarButton::HighlightColorAnimation::GetInkDropBaseColor() const {
  if (!highlight_color_)
    return base::nullopt;
  return *highlight_color_;
}

void ToolbarButton::HighlightColorAnimation::AnimationEnded(
    const gfx::Animation* animation) {
  // Only reset the color after the animation slides _back_ and not when it
  // finishes sliding fully _open_.
  if (highlight_color_animation_.GetCurrentValue() == 0.0f)
    ClearHighlightColor();
}

void ToolbarButton::HighlightColorAnimation::AnimationProgressed(
    const gfx::Animation* animation) {
  parent_->UpdateColorsAndInsets();
}

bool ToolbarButton::HighlightColorAnimation::IsShown() const {
  return highlight_color_animation_.is_animating() ||
         highlight_color_animation_.GetCurrentValue() == 1.0f;
}

void ToolbarButton::HighlightColorAnimation::ClearHighlightColor() {
  highlight_color_animation_.Reset(0.0f);
  highlight_color_.reset();
  parent_->UpdateColorsAndInsets();
}
