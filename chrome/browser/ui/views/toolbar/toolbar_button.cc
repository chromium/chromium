// Copyright 2013 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/views/toolbar/toolbar_button.h"

#include <algorithm>
#include <memory>
#include <utility>

#include "base/feature_list.h"
#include "base/functional/bind.h"
#include "base/location.h"
#include "base/memory/raw_ptr.h"
#include "base/task/single_thread_task_runner.h"
#include "build/build_config.h"
#include "build/chromeos_buildflags.h"
#include "chrome/browser/themes/theme_properties.h"
#include "chrome/browser/themes/theme_service.h"
#include "chrome/browser/themes/theme_service_factory.h"
#include "chrome/browser/ui/color/chrome_color_id.h"
#include "chrome/browser/ui/color/chrome_color_provider_utils.h"
#include "chrome/browser/ui/layout_constants.h"
#include "chrome/browser/ui/tabs/tab_strip_model.h"
#include "chrome/browser/ui/ui_features.h"
#include "chrome/browser/ui/views/chrome_layout_provider.h"
#include "chrome/browser/ui/views/chrome_typography.h"
#include "chrome/browser/ui/views/toolbar/toolbar_ink_drop_util.h"
#include "components/user_education/common/user_education_class_properties.h"
#include "third_party/abseil-cpp/absl/types/optional.h"
#include "ui/accessibility/ax_enums.mojom.h"
#include "ui/accessibility/ax_node_data.h"
#include "ui/base/menu_source_utils.h"
#include "ui/base/metadata/metadata_impl_macros.h"
#include "ui/base/models/image_model.h"
#include "ui/base/models/menu_model.h"
#include "ui/base/ui_base_features.h"
#include "ui/color/color_provider.h"
#include "ui/compositor/paint_recorder.h"
#include "ui/display/display.h"
#include "ui/display/screen.h"
#include "ui/gfx/color_utils.h"
#include "ui/gfx/geometry/insets.h"
#include "ui/gfx/paint_vector_icon.h"
#include "ui/gfx/text_utils.h"
#include "ui/views/animation/ink_drop.h"
#include "ui/views/animation/ink_drop_highlight.h"
#include "ui/views/animation/ink_drop_mask.h"
#include "ui/views/background.h"
#include "ui/views/border.h"
#include "ui/views/controls/button/label_button_border.h"
#include "ui/views/controls/menu/menu_item_view.h"
#include "ui/views/controls/menu/menu_model_adapter.h"
#include "ui/views/controls/menu/menu_runner.h"
#include "ui/views/view_class_properties.h"
#include "ui/views/widget/widget.h"

namespace {

constexpr int kBorderThicknessDpWithLabel = 1;
constexpr int kBorderThicknessDpWithoutLabel = 2;

// Cycle duration of ink drop pulsing animation used for in-product help.
constexpr base::TimeDelta kFeaturePromoPulseDuration = base::Milliseconds(800);

// Max inset for pulsing animation.
constexpr float kFeaturePromoPulseInsetDip = 3.0f;

// An InkDropMask used to animate the size of the BrowserAppMenuButton's ink
// drop. This is used when showing in-product help.
class PulsingInkDropMask : public views::AnimationDelegateViews,
                           public views::InkDropMask {
 public:
  PulsingInkDropMask(views::View* layer_container,
                     const gfx::Size& layer_size,
                     const gfx::Insets& margins,
                     float normal_corner_radius,
                     float max_inset)
      : AnimationDelegateViews(layer_container),
        views::InkDropMask(layer_size),
        layer_container_(layer_container),
        margins_(margins),
        normal_corner_radius_(normal_corner_radius),
        max_inset_(max_inset),
        throb_animation_(this) {
    throb_animation_.SetThrobDuration(kFeaturePromoPulseDuration);
    throb_animation_.StartThrobbing(-1);
  }

 private:
  // views::InkDropMask:
  void OnPaintLayer(const ui::PaintContext& context) override {
    cc::PaintFlags flags;
    flags.setStyle(cc::PaintFlags::kFill_Style);
    flags.setAntiAlias(true);

    ui::PaintRecorder recorder(context, layer()->size());

    gfx::RectF bounds(layer()->bounds());
    bounds.Inset(gfx::InsetsF(margins_));

    const float current_inset =
        throb_animation_.CurrentValueBetween(0.0f, max_inset_);
    bounds.Inset(gfx::InsetsF(current_inset));
    const float corner_radius = normal_corner_radius_ - current_inset;

    recorder.canvas()->DrawRoundRect(bounds, corner_radius, flags);
  }

  // views::AnimationDelegateViews:
  void AnimationProgressed(const gfx::Animation* animation) override {
    DCHECK_EQ(animation, &throb_animation_);
    layer()->SchedulePaint(gfx::Rect(layer()->size()));

    // This is a workaround for crbug.com/935808: for scale factors >1,
    // invalidating the mask layer doesn't cause the whole layer to be repainted
    // on screen. TODO(crbug.com/935808): remove this workaround once the bug is
    // fixed.
    layer_container_->SchedulePaint();
  }

  // The View that contains the InkDrop layer we're masking. This must outlive
  // our instance.
  const raw_ptr<views::View> layer_container_;

  // Margins between the layer bounds and the visible ink drop. We use this
  // because sometimes the View we're masking is larger than the ink drop we
  // want to show.
  const gfx::Insets margins_;

  // Normal corner radius of the ink drop without animation. This is also the
  // corner radius at the largest instant of the animation.
  const float normal_corner_radius_;

  // Max inset, used at the smallest instant of the animation.
  const float max_inset_;

  gfx::ThrobAnimation throb_animation_;
};

}  // namespace

ToolbarButton::ToolbarButton(PressedCallback callback)
    : ToolbarButton(std::move(callback), nullptr, nullptr) {}

ToolbarButton::ToolbarButton(PressedCallback callback,
                             std::unique_ptr<ui::MenuModel> model,
                             TabStripModel* tab_strip_model,
                             bool trigger_menu_on_long_press)
    : views::LabelButton(std::move(callback),
                         std::u16string(),
                         CONTEXT_TOOLBAR_BUTTON),
      model_(std::move(model)),
      tab_strip_model_(tab_strip_model),
      trigger_menu_on_long_press_(trigger_menu_on_long_press),
      highlight_color_animation_(this) {
  ConfigureInkDropForToolbar(this);

  set_context_menu_controller(this);

  views::InkDrop::Get(this)->SetCreateMaskCallback(base::BindRepeating(
      [](ToolbarButton* host) -> std::unique_ptr<views::InkDropMask> {
        if (host->has_in_product_help_promo_) {
          // This gets the latest ink drop insets. |SetTrailingMargin()| is
          // called whenever our margins change (i.e. due to the window
          // maximizing or minimizing) and updates our internal padding property
          // accordingly.
          const gfx::Insets ink_drop_insets = GetToolbarInkDropInsets(host);
          const float corner_radius = (host->height() - ink_drop_insets.top() -
                                       ink_drop_insets.bottom()) /
                                      2.0f;
          return std::make_unique<PulsingInkDropMask>(
              host->ink_drop_container(), host->size(), ink_drop_insets,
              corner_radius, kFeaturePromoPulseInsetDip);
        }
        return std::make_unique<views::PathInkDropMask>(host->size(),
                                                        GetHighlightPath(host));
      },
      this));
  views::InkDrop::Get(this)->SetBaseColorCallback(base::BindRepeating(
      [](ToolbarButton* host) {
        if (host->has_in_product_help_promo_) {
          return host->GetColorProvider()->GetColor(
              kColorToolbarFeaturePromoHighlight);
        }
        absl::optional<SkColor> drop_base_color =
            host->highlight_color_animation_.GetInkDropBaseColor();
        if (drop_base_color)
          return *drop_base_color;
        return GetToolbarInkDropBaseColor(host);
      },
      this));

  // Make sure icons are flipped by default so that back, forward, etc. follows
  // UI direction.
  SetFlipCanvasOnPaintForRTLUI(true);

  SetImageLabelSpacing(ChromeLayoutProvider::Get()->GetDistanceMetric(
      DISTANCE_RELATED_LABEL_HORIZONTAL_LIST));
  SetHorizontalAlignment(gfx::ALIGN_RIGHT);

  // Because we're using the internal padding to keep track of the changes we
  // make to the leading margin to handle Fitts' Law, it's easier to just
  // allocate the property once and modify the value.
  SetProperty(views::kInternalPaddingKey, gfx::Insets());

  SetFocusBehavior(FocusBehavior::ACCESSIBLE_ONLY);
  views::FocusRing::Get(this)->SetOutsetFocusRingDisabled(true);
}

ToolbarButton::~ToolbarButton() = default;

void ToolbarButton::SetHighlight(const std::u16string& highlight_text,
                                 absl::optional<SkColor> highlight_color) {
  if (highlight_text.empty() && !highlight_color.has_value()) {
    ClearHighlight();
    return;
  }

  highlight_color_animation_.Show(highlight_color);
  SetText(highlight_text);
}

void ToolbarButton::SetText(const std::u16string& text) {
  LabelButton::SetText(text);
  UpdateColorsAndInsets();
}

void ToolbarButton::TouchUiChanged() {
  UpdateIcon();
  UpdateColorsAndInsets();
  PreferredSizeChanged();
}

void ToolbarButton::ClearHighlight() {
  highlight_color_animation_.Hide();
  ShrinkDownThenClearText();
}

void ToolbarButton::UpdateColorsAndInsets() {
  // First, calculate new border insets assuming CalculatePreferredSize()
  // accurately reflects the desired content size.

  const gfx::Size current_preferred_size = CalculatePreferredSize();
  const gfx::Insets current_insets = GetInsets();
  const gfx::Size target_contents_size =
      current_preferred_size - current_insets.size();

  const gfx::Insets target_insets =
      layout_insets_.value_or(::GetLayoutInsets(TOOLBAR_BUTTON)) +
      layout_inset_delta_ + *GetProperty(views::kInternalPaddingKey);

  const gfx::Size target_size = target_contents_size + target_insets.size();

  const int highlight_radius =
      ChromeLayoutProvider::Get()->GetCornerRadiusMetric(
          views::Emphasis::kMaximum, target_size);

  SetEnabledTextColors(highlight_color_animation_.GetTextColor());

  // ToolbarButton height is constrained by the height of the location bar.
  const int extra_height = std::max(
      0, target_size.height() - GetLayoutConstant(LOCATION_BAR_HEIGHT));
  const gfx::Insets paint_insets =
      gfx::Insets(extra_height / 2) + *GetProperty(views::kInternalPaddingKey);

  absl::optional<SkColor> background_color =
      highlight_color_animation_.GetBackgroundColor();
  if (background_color) {
    SetBackground(views::CreateBackgroundFromPainter(
        views::Painter::CreateSolidRoundRectPainter(
            *background_color, highlight_radius, paint_insets)));
    label()->SetBackgroundColor(*background_color);
  } else {
    SetBackground(nullptr);
    const auto* cp = GetColorProvider();
    if (cp)
      label()->SetBackgroundColor(cp->GetColor(kColorToolbar));
  }

  // Apply new border with target insets.

  absl::optional<SkColor> border_color =
      highlight_color_animation_.GetBorderColor();
  if (!GetBorder() || target_insets != current_insets ||
      last_border_color_ != border_color ||
      last_paint_insets_ != paint_insets) {
    if (ShouldPaintBorder() && border_color) {
      int border_thickness_dp = GetText().empty()
                                    ? kBorderThicknessDpWithoutLabel
                                    : kBorderThicknessDpWithLabel;
      // Create a border with insets totalling |target_insets|, split into
      // painted insets (matching the background) and internal padding to
      // position child views correctly.
      std::unique_ptr<views::Border> border = views::CreateRoundedRectBorder(
          border_thickness_dp, highlight_radius, paint_insets, *border_color);
      const gfx::Insets extra_insets = target_insets - border->GetInsets();
      SetBorder(views::CreatePaddedBorder(std::move(border), extra_insets));
    } else {
      SetBorder(views::CreateEmptyBorder(target_insets));
    }
    last_border_color_ = border_color;
    last_paint_insets_ = paint_insets;
  }

  // Update spacing on the outer-side of the label to match the current
  // highlight radius.
  SetLabelSideSpacing(highlight_radius / 2);
}

SkColor ToolbarButton::GetForegroundColor(ButtonState state) const {
  const auto* color_provider = GetColorProvider();
  if (has_in_product_help_promo_)
    return color_provider->GetColor(kColorToolbarFeaturePromoHighlight);
  switch (state) {
    case ButtonState::STATE_HOVERED:
      return color_provider->GetColor(kColorToolbarButtonIconHovered);
    case ButtonState::STATE_PRESSED:
      return color_provider->GetColor(kColorToolbarButtonIconPressed);
    case ButtonState::STATE_DISABLED:
      return color_provider->GetColor(kColorToolbarButtonIconInactive);
    case ButtonState::STATE_NORMAL:
      return color_provider->GetColor(kColorToolbarButtonIcon);
    default:
      NOTREACHED_NORETURN();
  }
}

void ToolbarButton::UpdateIconsWithColors(const gfx::VectorIcon& icon,
                                          SkColor normal_color,
                                          SkColor hovered_color,
                                          SkColor pressed_color,
                                          SkColor disabled_color) {
  const int icon_size = GetIconSize();
  SetImageModel(ButtonState::STATE_NORMAL,
                ui::ImageModel::FromVectorIcon(icon, normal_color, icon_size));
  SetImageModel(ButtonState::STATE_HOVERED,
                ui::ImageModel::FromVectorIcon(icon, hovered_color, icon_size));
  SetImageModel(ButtonState::STATE_PRESSED,
                ui::ImageModel::FromVectorIcon(icon, pressed_color, icon_size));
  SetImageModel(Button::STATE_DISABLED, ui::ImageModel::FromVectorIcon(
                                            icon, disabled_color, icon_size));
}

int ToolbarButton::GetIconSize() const {
  if (ui::TouchUiController::Get()->touch_ui()) {
    return kDefaultTouchableIconSize;
  }

  return features::IsChromeRefresh2023() ? kDefaultIconSizeChromeRefresh
                                         : kDefaultIconSize;
}

bool ToolbarButton::ShouldPaintBorder() const {
  return true;
}

bool ToolbarButton::ShouldBlendHighlightColor() const {
  return !features::IsChromeRefresh2023();
}

bool ToolbarButton::ShouldDirectlyUseHighlightAsBackground() const {
  return true;
}

absl::optional<SkColor> ToolbarButton::GetHighlightTextColor() const {
  return absl::nullopt;
}

absl::optional<SkColor> ToolbarButton::GetHighlightBorderColor() const {
  return absl::nullopt;
}

void ToolbarButton::SetVectorIcon(const gfx::VectorIcon& icon) {
  SetVectorIcons(icon, icon);
}

void ToolbarButton::SetVectorIcons(const gfx::VectorIcon& icon,
                                   const gfx::VectorIcon& touch_icon) {
  vector_icons_.emplace(VectorIcons{icon, touch_icon});
  if (GetThemeProvider())
    UpdateIcon();
}

void ToolbarButton::UpdateIcon() {
  // TODO(pbos): See if the default can turn into a DCHECK, if we don't provide
  // vector icons we need to override this to properly update icons. This is a
  // foot shooter.
  if (vector_icons_) {
    UpdateIconsWithStandardColors(ui::TouchUiController::Get()->touch_ui()
                                      ? vector_icons_->touch_icon
                                      : vector_icons_->icon);
  }
}

void ToolbarButton::UpdateIconsWithStandardColors(const gfx::VectorIcon& icon) {
  UpdateIconsWithColors(icon, GetForegroundColor(ButtonState::STATE_NORMAL),
                        GetForegroundColor(ButtonState::STATE_HOVERED),
                        GetForegroundColor(ButtonState::STATE_PRESSED),
                        GetForegroundColor(ButtonState::STATE_DISABLED));
}

void ToolbarButton::SetLabelSideSpacing(int spacing) {
  gfx::Insets label_insets;
  // Add the spacing only if text is non-empty.
  if (!GetText().empty()) {
    // Add spacing to the opposing side.
    label_insets =
        gfx::MaybeFlipForRTL(GetHorizontalAlignment()) == gfx::ALIGN_RIGHT
            ? gfx::Insets::TLBR(0, spacing, 0, 0)
            : gfx::Insets::TLBR(0, 0, 0, spacing);
  }
  if (!label()->GetBorder() ||
      label_insets != label()->GetBorder()->GetInsets()) {
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

absl::optional<gfx::Insets> ToolbarButton::GetLayoutInsets() const {
  return layout_insets_;
}

void ToolbarButton::SetLayoutInsets(const absl::optional<gfx::Insets>& insets) {
  if (layout_insets_ == insets)
    return;
  layout_insets_ = insets;
  UpdateColorsAndInsets();
}

void ToolbarButton::OnBoundsChanged(const gfx::Rect& previous_bounds) {
  if (size() != previous_bounds.size())
    UpdateColorsAndInsets();
  LabelButton::OnBoundsChanged(previous_bounds);
}

void ToolbarButton::OnThemeChanged() {
  UpdateColorsAndInsets();
  UpdateIcon();

  // Call this after UpdateIcon() to properly reset images.
  LabelButton::OnThemeChanged();
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
  insets.set_left_right(0, 0);
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
    base::SingleThreadTaskRunner::GetCurrentDefault()->PostDelayedTask(
        FROM_HERE,
        base::BindOnce(&ToolbarButton::ShowDropDownMenu,
                       show_menu_factory_.GetWeakPtr(),
                       ui::GetMenuSourceTypeForEvent(event)),
        base::Milliseconds(500));
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
  if (GetState() != STATE_DISABLED && !InDrag() && GetState() != STATE_PRESSED)
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
  if (model_)
    node_data->SetHasPopup(ax::mojom::HasPopup::kMenu);
}

std::u16string ToolbarButton::GetTooltipText(const gfx::Point& p) const {
  // Suppress tooltip when IPH is showing.
  return has_in_product_help_promo_ ? std::u16string()
                                    : views::LabelButton::GetTooltipText(p);
}

void ToolbarButton::ShowContextMenuForViewImpl(View* source,
                                               const gfx::Point& point,
                                               ui::MenuSourceType source_type) {
  if (!GetEnabled())
    return;

  show_menu_factory_.InvalidateWeakPtrs();
  ShowDropDownMenu(source_type);
}

void ToolbarButton::AfterPropertyChange(const void* key, int64_t old_value) {
  View::AfterPropertyChange(key, old_value);
  if (key == user_education::kHasInProductHelpPromoKey)
    SetHasInProductHelpPromo(
        GetProperty(user_education::kHasInProductHelpPromoKey));
}

void ToolbarButton::SetHasInProductHelpPromo(bool has_in_product_help_promo) {
  if (has_in_product_help_promo_ == has_in_product_help_promo)
    return;

  has_in_product_help_promo_ = has_in_product_help_promo;

  // We call SetBaseColorCallback() and SetCreateMaskCallback(),
  // returning the promo values if we are showing an in-product help promo.
  // Calling HostSizeChanged() will force the new mask and color to be fetched.
  //
  // TODO(collinbaker): Consider adding explicit way to recreate mask instead
  // of relying on HostSizeChanged() to do so.
  views::InkDrop::Get(this)->GetInkDrop()->HostSizeChanged(size());

  views::InkDropState next_state;
  if (has_in_product_help_promo_ ||
      (ShouldShowInkdropAfterIphInteraction() && GetVisible())) {
    // If we are showing a promo, we must use the ACTIVATED state to show the
    // highlight. Otherwise, if the menu is currently showing, we need to keep
    // the ink drop in the ACTIVATED state.
    next_state = views::InkDropState::ACTIVATED;
  } else {
    // If we are not showing a promo and the menu is hidden, we use the
    // DEACTIVATED state.
    next_state = views::InkDropState::DEACTIVATED;
    // TODO(collinbaker): this is brittle since we don't know if something
    // else should keep this ACTIVATED or in some other state. Consider adding
    // code to track the correct state and restore to that.
  }
  views::InkDrop::Get(this)->GetInkDrop()->AnimateToState(next_state);

  UpdateIcon();
  SchedulePaint();
}

bool ToolbarButton::ShouldShowMenu() {
  return model_ != nullptr;
}

bool ToolbarButton::ShouldShowInkdropAfterIphInteraction() {
  return true;
}

void ToolbarButton::ShowDropDownMenu(ui::MenuSourceType source_type) {
  if (!ShouldShowMenu())
    return;

  gfx::Rect menu_anchor_bounds = GetAnchorBoundsInScreen();

#if BUILDFLAG(IS_CHROMEOS_ASH)
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

  views::InkDrop::Get(this)->AnimateToState(views::InkDropState::ACTIVATED,
                                            nullptr /* event */);

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
  menu_model_adapter_->set_triggerable_event_flags(GetTriggerableEventFlags());
  menu_runner_ = std::make_unique<views::MenuRunner>(
      menu_model_adapter_->CreateMenu(), views::MenuRunner::HAS_MNEMONICS);
  menu_runner_->RunMenuAt(GetWidget(), nullptr, menu_anchor_bounds,
                          views::MenuAnchorPosition::kTopLeft, source_type);
}

void ToolbarButton::OnMenuClosed() {
  views::InkDrop::Get(this)->AnimateToState(views::InkDropState::DEACTIVATED,
                                            nullptr /* event */);

  menu_showing_ = false;

  // Set the state back to normal after the drop down menu is closed.
  if (GetState() != STATE_DISABLED) {
    views::InkDrop::Get(this)->GetInkDrop()->SetHovered(IsMouseHovered());
    SetState(STATE_NORMAL);
  }

  menu_runner_.reset();
  menu_model_adapter_.reset();
}

namespace {

// The default duration does not work well for dark mode where the animation has
// to make a big contrast difference.
// TODO(crbug.com/967317): This needs to be consistent with the duration of the
// border animation in ToolbarIconContainerView.
constexpr base::TimeDelta kHighlightAnimationDuration = base::Milliseconds(300);

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

ToolbarButton::HighlightColorAnimation::~HighlightColorAnimation() = default;

void ToolbarButton::HighlightColorAnimation::Show(
    absl::optional<SkColor> highlight_color) {
  // If the animation is showing, we will jump to a different color in the
  // middle of the animation and continue animating towards the new
  // |highlight_color_|. If the animation is fully shown, we will jump directly
  // to the new |highlight_color_|. This is not ideal but making it smoother is
  // not worth the extra complexity given this should be very rare.
  if (highlight_color_animation_.GetCurrentValue() == 0.0f ||
      highlight_color_animation_.IsClosing()) {
    highlight_color_animation_.Show();
  }
  highlight_color_ = highlight_color;
  parent_->UpdateColorsAndInsets();
}

void ToolbarButton::HighlightColorAnimation::Hide() {
  highlight_color_animation_.Hide();
}

absl::optional<SkColor> ToolbarButton::HighlightColorAnimation::GetTextColor()
    const {
  if (!IsShown() || !parent_->GetColorProvider())
    return absl::nullopt;

  // Use the overridden value supplied by the button.
  const absl::optional<SkColor> text_color_overridden =
      parent_->GetHighlightTextColor();
  SkColor text_color;

  if (text_color_overridden.has_value()) {
    text_color = *text_color_overridden;
  } else if (highlight_color_) {
    text_color = *highlight_color_;
  } else {
    text_color = parent_->GetColorProvider()->GetColor(kColorToolbarButtonText);
  }
  return FadeWithAnimation(text_color, highlight_color_animation_);
}

absl::optional<SkColor> ToolbarButton::HighlightColorAnimation::GetBorderColor()
    const {
  if (!IsShown() || !parent_->GetColorProvider()) {
    return absl::nullopt;
  }

  // Use the overridden value is supplied by the button
  const absl::optional<SkColor> border_color_overridden =
      parent_->GetHighlightBorderColor();
  SkColor border_color;

  if (border_color_overridden.has_value()) {
    border_color = border_color_overridden.value();
  } else if (highlight_color_.has_value()) {
    border_color = highlight_color_.value();
  } else {
    border_color =
        parent_->GetColorProvider()->GetColor(kColorToolbarButtonBorder);
  }
  return FadeWithAnimation(border_color, highlight_color_animation_);
}

absl::optional<SkColor>
ToolbarButton::HighlightColorAnimation::GetBackgroundColor() const {
  const auto* const color_provider = parent_->GetColorProvider();
  if (!IsShown() || !color_provider)
    return absl::nullopt;
  SkColor bg_color =
      color_provider->GetColor(kColorToolbarButtonBackgroundHighlightedDefault);
  if (highlight_color_.has_value()) {
    bg_color =
        parent_->ShouldBlendHighlightColor()
            ? color_utils::AlphaBlend(highlight_color_.value(),
                                      color_provider->GetColor(kColorToolbar),
                                      kToolbarInkDropHighlightVisibleAlpha)
            : highlight_color_.value();
  }
  return FadeWithAnimation(bg_color, highlight_color_animation_);
}

absl::optional<SkColor>
ToolbarButton::HighlightColorAnimation::GetInkDropBaseColor() const {
  if (!highlight_color_)
    return absl::nullopt;
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

BEGIN_METADATA(ToolbarButton, views::LabelButton)
ADD_PROPERTY_METADATA(absl::optional<gfx::Insets>, LayoutInsets)
END_METADATA
