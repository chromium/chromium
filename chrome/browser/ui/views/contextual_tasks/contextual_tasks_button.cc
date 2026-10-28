// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/views/contextual_tasks/contextual_tasks_button.h"

#include <string>
#include <string_view>

#include "base/check_op.h"
#include "base/functional/bind.h"
#include "base/metrics/histogram_functions.h"
#include "base/strings/strcat.h"
#include "chrome/app/vector_icons/vector_icons.h"
#include "chrome/browser/contextual_tasks/contextual_tasks_panel_controller.h"
#include "chrome/browser/contextual_tasks/entry_point_eligibility_manager.h"
#include "chrome/browser/ui/browser_actions.h"
#include "chrome/browser/ui/browser_element_identifiers.h"
#include "chrome/browser/ui/browser_window/public/browser_window_features.h"
#include "chrome/browser/ui/browser_window/public/browser_window_interface.h"
#include "chrome/browser/ui/color/chrome_color_id.h"
#include "chrome/browser/ui/immersive/immersive_mode_controller.h"
#include "chrome/browser/ui/layout_constants.h"
#include "chrome/browser/ui/tabs/vertical_tab_strip_state_controller.h"
#include "chrome/browser/ui/ui_features.h"
#include "chrome/browser/ui/user_education/browser_user_education_interface.h"
#include "chrome/browser/ui/views/contextual_tasks/contextual_tasks_ephemeral_button_controller.h"
#include "chrome/browser/ui/views/contextual_tasks/contextual_tasks_ephemeral_button_menu_model.h"
#include "chrome/browser/ui/views/contextual_tasks/contextual_tasks_ephemeral_button_utils.h"
#include "chrome/browser/ui/views/frame/browser_view.h"
#include "chrome/browser/ui/views/toolbar/pinned_toolbar_button_status_indicator.h"
#include "chrome/browser/ui/views/toolbar/toolbar_button.h"
#include "chrome/browser/ui/views/toolbar/toolbar_ink_drop_util.h"
#include "chrome/grit/branded_strings.h"
#include "chrome/grit/generated_resources.h"
#include "chrome/grit/theme_resources.h"
#include "components/contextual_tasks/public/features.h"
#include "components/feature_engagement/public/feature_constants.h"
#include "components/omnibox/browser/vector_icons.h"
#include "components/strings/grit/components_strings.h"
#include "components/vector_icons/vector_icons.h"
#include "third_party/skia/include/core/SkRRect.h"
#include "ui/actions/actions.h"
#include "ui/base/interaction/element_identifier.h"
#include "ui/base/l10n/l10n_util.h"
#include "ui/base/metadata/metadata_impl_macros.h"
#include "ui/base/models/image_model.h"
#include "ui/base/ui_base_features.h"
#include "ui/compositor/layer.h"
#include "ui/compositor/layer_animator.h"
#include "ui/compositor/layer_owner.h"
#include "ui/gfx/canvas.h"
#include "ui/gfx/geometry/skia_conversions.h"
#include "ui/gfx/image/image_skia.h"
#include "ui/gfx/scoped_canvas.h"
#include "ui/gfx/skia_paint_util.h"
#include "ui/gfx/vector_icon_types.h"
#include "ui/views/accessibility/view_accessibility.h"
#include "ui/views/animation/animation_builder.h"
#include "ui/views/painter.h"
#include "ui/views/view.h"
#include "ui/views/view_class_properties.h"

using contextual_tasks::EphemeralButtonShape;

namespace {

// Will correspond to a 34x34 shadow circle when the location bar height is 34.
const int kCircleShadowInset = 0;

// Corresponds to a 32x32 pill background shape when the location bar height
// is 34.
const int kPillShapeShadowInset = 1;

const int kGLogoCircularShapeIconSize = 16;
const int kGLogoPillShapeIconSize = 15;

// The flat edge's top corner radius is needed to avoid the toolbar upper
// rounded corner. This applies to the top-left corner when the flat edge is on
// the left, or the top-right corner when the flat edge is on the right.
const float kFlatEdgeTopRadius = 5.0f;

// The flat edge's bottom corner radius. This applies to the bottom-left corner
// when the flat edge is on the left, or the bottom-right corner when the flat
// edge is on the right.
const float kFlatEdgeBottomRadius = 0.0f;

// Margin to outset the background painted layer so the shadow is not clipped.
const int kShadowOutset = 12;

// Width of the status indicator shown across the button.
constexpr int kStatusIndicatorWidth = 14;
// Height of the status indicator shown across the button.
constexpr int kStatusIndicatorHeight = 2;
// Spacing between the button's icon and the status indicator.
constexpr int kStatusIndicatorSpacing = 1;

gfx::Insets GetPillShadowInsets(EphemeralButtonShape shape) {
  gfx::Insets shadow_insets =
      gfx::Insets::VH(kPillShapeShadowInset, kPillShapeShadowInset);
  if (shape == EphemeralButtonShape::kFlatEdgeRight) {
    shadow_insets.set_right(0);
  } else {
    shadow_insets.set_left(0);
  }

  return shadow_insets;
}

SkRRect GetPillRRect(const gfx::Rect& rect, EphemeralButtonShape shape) {
  const float radius = rect.height() / 2.0f;
  SkVector radii[4];
  if (shape == EphemeralButtonShape::kFlatEdgeRight) {
    radii[0] = {radius, radius};                                // top-left
    radii[1] = {kFlatEdgeTopRadius, kFlatEdgeTopRadius};        // top-right
    radii[2] = {kFlatEdgeBottomRadius, kFlatEdgeBottomRadius};  // bottom-right
    radii[3] = {radius, radius};                                // bottom-left
  } else {
    radii[0] = {kFlatEdgeTopRadius, kFlatEdgeTopRadius};        // top-left
    radii[1] = {radius, radius};                                // top-right
    radii[2] = {radius, radius};                                // bottom-right
    radii[3] = {kFlatEdgeBottomRadius, kFlatEdgeBottomRadius};  // bottom-left
  }
  SkRRect rrect;
  rrect.setRectRadii(gfx::RectToSkRect(rect), radii);
  return rrect;
}

// Helper class to paint the contextual tasks button shadow. The general
// ViewShadow class doesn't work for the contextual tasks button because the
// button doesn't have a standard shape, and thus custom shadow logic.
class ContextualTasksButtonBackgroundPainter : public views::Painter {
 public:
  ContextualTasksButtonBackgroundPainter(SkColor bg_color,
                                         SkColor shadow_color,
                                         EphemeralButtonShape shape)
      : bg_color_(bg_color), shadow_color_(shadow_color), shape_(shape) {}
  ~ContextualTasksButtonBackgroundPainter() override = default;

  gfx::Size GetMinimumSize() const override { return gfx::Size(); }

  void Paint(gfx::Canvas* canvas, const gfx::Size& size) override {
    gfx::ScopedCanvas scoped_canvas(canvas);
    const float scale = canvas->UndoDeviceScaleFactor();

    gfx::ShadowValues shadow;
    constexpr int kOffset = 4;
    constexpr int kBlur = 12;
    shadow.emplace_back(gfx::Vector2d(0, kOffset), kBlur, shadow_color_);

    cc::PaintFlags flags;
    flags.setAntiAlias(true);
    flags.setLooper(gfx::CreateShadowDrawLooper(shadow));
    flags.setStyle(cc::PaintFlags::kFill_Style);
    flags.setColor(bg_color_);

    gfx::Rect button_rect(size);
    button_rect.Inset(gfx::Insets(kShadowOutset));

    switch (shape_) {
      case EphemeralButtonShape::kCircle: {
        gfx::Rect inset_rect = button_rect;
        inset_rect.Inset(gfx::Insets(kCircleShadowInset));
        gfx::RectF fill_rect(gfx::ScaleToEnclosedRect(inset_rect, scale));
        gfx::PointF center = fill_rect.CenterPoint();
        float scaled_radius =
            std::min(fill_rect.width(), fill_rect.height()) / 2.0f;
        canvas->DrawCircle(center, scaled_radius, flags);
        break;
      }
      case EphemeralButtonShape::kFlatEdgeRight:
      case EphemeralButtonShape::kFlatEdgeLeft: {
        gfx::Rect inset_rect = button_rect;
        inset_rect.Inset(GetPillShadowInsets(shape_));
        gfx::Rect fill_rect = gfx::ScaleToEnclosingRect(inset_rect, scale);
        canvas->sk_canvas()->drawRRect(GetPillRRect(fill_rect, shape_), flags);
        break;
      }
    }
  }

 private:
  const SkColor bg_color_;
  const SkColor shadow_color_;
  const EphemeralButtonShape shape_;
};

class ContextualTasksButtonHighlightPathGenerator
    : public views::HighlightPathGenerator {
 public:
  explicit ContextualTasksButtonHighlightPathGenerator(
      ContextualTasksButton* button)
      : button_(button) {}
  ~ContextualTasksButtonHighlightPathGenerator() override = default;

  SkPath GetHighlightPath(const views::View* view) override {
    gfx::Rect rect(view->size());
    const EphemeralButtonShape shape = button_->GetShape();
    switch (shape) {
      case EphemeralButtonShape::kCircle:
        rect.Inset(gfx::Insets(kCircleShadowInset));
        return SkPath::Oval(gfx::RectToSkRect(rect));
      case EphemeralButtonShape::kFlatEdgeRight:
      case EphemeralButtonShape::kFlatEdgeLeft:
        rect.Inset(GetPillShadowInsets(shape));
        return SkPath::RRect(GetPillRRect(rect, shape));
    }
  }

 private:
  const raw_ptr<ContextualTasksButton> button_;
};

}  // namespace

ContextualTasksButton::ContextualTasksButton(
    BrowserWindowInterface* browser_window_interface)
    : ToolbarButton(
          base::BindRepeating(&ContextualTasksButton::OnButtonPress,
                              base::Unretained(this)),
          base::FeatureList::IsEnabled(
              contextual_tasks::kContextualTasksEphemeralButtonContextMenu)
              ? std::make_unique<ContextualTasksEphemeralButtonMenuModel>(
                    browser_window_interface)
              : nullptr,
          nullptr,
          /*trigger_menu_on_long_press=*/false),
      browser_window_interface_(browser_window_interface) {
  SetPaintToLayer();
  layer()->SetFillsBoundsOpaquely(false);
  // The contextual tasks button is ephemeral and starts hidden until an active
  // task requires it.
  SetVisible(false);
  SetProperty(views::kElementIdentifierKey,
              kContextualTasksEphemeralToolbarButtonElementId);
  status_indicator_ =
      PinnedToolbarButtonStatusIndicator::Install(image_container_view());
  status_indicator_->SetColorId(kColorToolbarActionItemEngaged,
                                kColorToolbarButtonIconInactive);
  UpdateTooltipText();

  if (contextual_tasks::kShowEntryPoint.Get() ==
      contextual_tasks::EntryPointOption::kToolbarEphemeralBranded) {
    ContextualTasksEphemeralButtonController* const controller =
        ContextualTasksEphemeralButtonController::From(
            browser_window_interface_);
    if (controller) {
      should_update_visibility_subscription_ =
          controller->RegisterShouldUpdateButtonVisibility(base::BindRepeating(
              &ContextualTasksButton::OnShouldUpdateVisibility,
              base::Unretained(this)));
      should_update_position_subscription_ =
          controller->RegisterShouldUpdateButtonPosition(base::BindRepeating(
              &ContextualTasksButton::OnShouldUpdatePosition,
              base::Unretained(this)));
    }
  }

  eligibility_change_subscription_ =
      contextual_tasks::EntryPointEligibilityManager::From(
          browser_window_interface_)
          ->RegisterOnEntryPointEligibilityChanged(
              base::BindRepeating(&ContextualTasksButton::OnEligibilityChange,
                                  base::Unretained(this)));

  auto* controller = contextual_tasks::ContextualTasksPanelController::From(
      browser_window_interface_);
  CHECK(controller);
  panel_controller_observation_.Observe(controller);

  ImmersiveModeController* immersive_mode_controller =
      ImmersiveModeController::From(browser_window_interface_);
  if (immersive_mode_controller) {
    immersive_mode_observation_.Observe(immersive_mode_controller);
  }

  auto* vertical_tab_strip_controller =
      tabs::VerticalTabStripStateController::From(browser_window_interface_);
  if (vertical_tab_strip_controller) {
    vertical_tabs_subscription_ =
        vertical_tab_strip_controller->RegisterOnModeChanged(
            base::BindRepeating(
                [](ContextualTasksButton* button,
                   tabs::VerticalTabStripStateController*) {
                  button->UpdateColorsAndInsets();
                },
                base::Unretained(this)));
  }

  OnShouldUpdatePosition();
  MaybeUpdateVisibility();

  if (contextual_tasks::kShowEntryPoint.Get() ==
      contextual_tasks::EntryPointOption::kToolbarEphemeralBranded) {
    views::HighlightPathGenerator::Install(
        this,
        std::make_unique<ContextualTasksButtonHighlightPathGenerator>(this));
  }
}

ContextualTasksButton::~ContextualTasksButton() {
  anchor_highlight_.reset();
  ClearDropShadow();
}

float ContextualTasksButton::GetCornerRadiusFor(
    ToolbarButton::Edge edge) const {
  if (contextual_tasks::kShowEntryPoint.Get() ==
      contextual_tasks::EntryPointOption::kToolbarEphemeralBranded) {
    const EphemeralButtonShape button_shape = GetShape();
    switch (edge) {
      case ToolbarButton::Edge::kTopLeft:
        if (button_shape == EphemeralButtonShape::kFlatEdgeLeft) {
          return kFlatEdgeTopRadius;
        }
        break;
      case ToolbarButton::Edge::kTopRight:
        if (button_shape == EphemeralButtonShape::kFlatEdgeRight) {
          return kFlatEdgeTopRadius;
        }
        break;
      case ToolbarButton::Edge::kBottomLeft:
        if (button_shape == EphemeralButtonShape::kFlatEdgeLeft) {
          return kFlatEdgeBottomRadius;
        }
        break;
      case ToolbarButton::Edge::kBottomRight:
        if (button_shape == EphemeralButtonShape::kFlatEdgeRight) {
          return kFlatEdgeBottomRadius;
        }
        break;
      default:
        break;
    }
  }
  // Toolbar buttons always have rounded corners for all its edges.
  return GetRoundedCornerRadius();
}

void ContextualTasksButton::OnImmersiveFullscreenEntered() {
  UpdateColorsAndInsets();
}

void ContextualTasksButton::OnImmersiveFullscreenExited() {
  UpdateColorsAndInsets();
}

void ContextualTasksButton::OnImmersiveModeControllerDestroyed() {
  immersive_mode_observation_.Reset();
}

void ContextualTasksButton::OnButtonPress() {
  // `base::Unretained()` is safe because `HandleEphemeralButtonPressed()` runs
  // the callback synchronously, before it returns, while `this` is still alive.
  contextual_tasks::HandleEphemeralButtonPressed(
      browser_window_interface_,
      base::BindOnce(&ContextualTasksButton::RecordShownDuration,
                     base::Unretained(this), "Clicked"));
}

void ContextualTasksButton::OnShouldUpdatePosition() {
  if (contextual_tasks::kShowEntryPoint.Get() ==
      contextual_tasks::EntryPointOption::kToolbarEphemeralBranded) {
    SetHorizontalAlignment(gfx::ALIGN_CENTER);
    UpdateColorsAndInsets();
    UpdateDropShadow();
    MaybeUpdateVisibility();
  } else {
    if (contextual_tasks::ShouldUseSparkIconForPinButtons()) {
      SetVectorIcon(features::IsRoundedIconsEnabled()
                        ? omnibox::kSearchSparkIcon
                        : omnibox::kSearchSparkOldIcon);
    } else if (contextual_tasks::ShouldUseSuperGIconForPinButtons()) {
      SetImageModel(views::Button::STATE_NORMAL, GetButtonImage());
    } else if (contextual_tasks::ShouldUseGraySuperGIconForPinButtons()) {
#if BUILDFLAG(GOOGLE_CHROME_BRANDING)
      SetVectorIcon(vector_icons::kGoogleGLogoMonochromeIcon);
#else
      SetVectorIcon(features::IsRoundedIconsEnabled() ? kChromeProductIcon
                                                      : kBrowserLogoOldIcon);
#endif
    } else {
      const gfx::VectorIcon& contextual_tasks_icon =
          contextual_tasks::IsContextualTasksSidePanelRightAligned(
              browser_window_interface_)
              ? kDockToRightSparkCustomIcon
              : kDockToLeftSparkCustomIcon;
      SetVectorIcon(contextual_tasks_icon);
    }
  }
}

bool ContextualTasksButton::ShouldShowMenu() {
  UpdateContextMenu();
  return menu_model() && menu_model()->GetItemCount() > 0 &&
         ToolbarButton::ShouldShowMenu();
}

void ContextualTasksButton::UpdateColorsAndInsets() {
  if (contextual_tasks::IsContextualTasksPinningV2Enabled()) {
    SetDefaultBackgroundColorId(ShouldShowCircleHighlight()
                                    ? kColorAvatarButtonHighlightDefault
                                    : kChromeColorsStart);
  }
  ToolbarButton::UpdateColorsAndInsets();

  if (contextual_tasks::kShowEntryPoint.Get() !=
      contextual_tasks::EntryPointOption::kToolbarEphemeralBranded) {
    return;
  }

  const int button_size = GetLayoutConstant(LayoutConstant::kLocationBarHeight);
  SetPreferredSize(gfx::Size(button_size, button_size));
  SetImageModel(views::Button::STATE_NORMAL, GetButtonImage());

  const auto* color_provider = GetColorProvider();
  if (!color_provider) {
    return;
  }

  const EphemeralButtonShape shape = GetShape();
  if (shape == EphemeralButtonShape::kCircle) {
    ClearProperty(views::kMarginsKey);
  } else {
    SetProperty(views::kMarginsKey, gfx::Insets());
  }
  gfx::Insets insets;
  switch (shape) {
    case EphemeralButtonShape::kCircle: {
      const int icon_inset = (button_size - GetIconSize()) / 2;
      insets = gfx::Insets(icon_inset);
      break;
    }
    case EphemeralButtonShape::kFlatEdgeRight:
    case EphemeralButtonShape::kFlatEdgeLeft: {
      const int icon_inset = (button_size - kGLogoPillShapeIconSize) / 2;
      constexpr int kIconOffset = 2;
      const int rounded_edge_inset = icon_inset + kIconOffset;
      const int flat_edge_inset = icon_inset - kIconOffset;
      insets =
          (shape == EphemeralButtonShape::kFlatEdgeRight)
              ? gfx::Insets::TLBR(0, rounded_edge_inset, 0, flat_edge_inset)
              : gfx::Insets::TLBR(0, flat_edge_inset, 0, rounded_edge_inset);
      break;
    }
  }
  SetLayoutInsets(insets + *GetProperty(views::kInternalPaddingKey));

  UpdateDropShadow();
  UpdateStatusIndicator();
}

void ContextualTasksButton::Layout(PassKey) {
  LayoutSuperclass<ToolbarButton>(this);
  if (!status_indicator_) {
    return;
  }
  gfx::Rect status_rect(kStatusIndicatorWidth, kStatusIndicatorHeight);
  const gfx::Rect image_container_bounds =
      image_container_view()->GetLocalBounds();
  const int new_x = image_container_bounds.x() +
                    (image_container_bounds.width() - status_rect.width()) / 2;
  const int new_y =
      image_container_bounds.bottom() +
      (features::IsRoundedIconsEnabled() ? 0 : kStatusIndicatorSpacing);
  status_rect.set_origin(gfx::Point(new_x, new_y));
  status_indicator_->SetBoundsRect(status_rect);
}

void ContextualTasksButton::UpdateIcon() {
  const std::optional<VectorIcons>& icons = GetVectorIcons();
  if (!icons.has_value() || !GetColorProvider()) {
    return;
  }
  const gfx::VectorIcon& icon = ui::TouchUiController::Get()->touch_ui()
                                    ? icons->touch_icon
                                    : icons->icon;
  if (contextual_tasks::
          IsContextualTasksPinnedButtonActiveIndicatorEnabled() &&
      !contextual_tasks::
          IsContextualTasksPinnedButtonActiveIndicatorCircleHighlightEnabled()) {
    const SkColor engaged_color =
        GetColorProvider()->GetColor(kColorToolbarActionItemEngaged);
    UpdateIconsWithColors(icon, engaged_color, engaged_color, engaged_color,
                          GetForegroundColor(ButtonState::STATE_DISABLED));
  } else {
    ToolbarButton::UpdateIcon();
  }
}

void ContextualTasksButton::OnViewLayerBoundsSet(views::View* observed_view) {
  CHECK_EQ(observed_view, this);
  ToolbarButton::OnViewLayerBoundsSet(observed_view);

  // Update the position of the drop shadow layer to ensure that it is shown
  // behind the ContextualTasks button.
  if (drop_shadow_painted_layer_) {
    UpdateDropShadowLayerBounds();
  }
}

void ContextualTasksButton::RemoveLayerFromRegions(ui::Layer* old_layer) {
  const std::vector<ui::Layer*> region_layers =
      GetLayersInOrder(views::ViewLayer::kExclude);
  if (std::ranges::contains(region_layers, old_layer)) {
    views::View::RemoveLayerFromRegions(old_layer);
  } else {
    ToolbarButton::RemoveLayerFromRegions(old_layer);
  }
}

void ContextualTasksButton::OnShouldUpdateVisibility(bool should_show) {
  MaybeUpdateVisibility();
}

void ContextualTasksButton::OnEligibilityChange(bool is_eligible) {
  UpdateContextMenu();
  MaybeUpdateVisibility();
}

void ContextualTasksButton::UpdateContextMenu() {
  if (auto* const contextual_tasks_menu_model =
          static_cast<ContextualTasksEphemeralButtonMenuModel*>(menu_model())) {
    contextual_tasks_menu_model->UpdateItems();
  }
}

void ContextualTasksButton::OnSurfaceStateChanged(
    contextual_tasks::ContextualTasksPanelHost::SurfaceState state,
    contextual_tasks::ContextualTasksPanelHost::StateChangeReason reason) {
  MaybeUpdateVisibility();
}

void ContextualTasksButton::OnControllerDestroyed() {
  panel_controller_observation_.Reset();
}

EphemeralButtonShape ContextualTasksButton::GetShape() const {
  return contextual_tasks::GetEphemeralButtonShape(browser_window_interface_);
}

bool ContextualTasksButton::IsHighlighted() const {
  return anchor_highlight_.has_value();
}

ui::Layer* ContextualTasksButton::GetDropShadowLayerForTesting() const {
  return drop_shadow_painted_layer_ ? drop_shadow_painted_layer_->layer()
                                    : nullptr;
}

void ContextualTasksButton::MaybeUpdateVisibility() {
  if (contextual_tasks::kShowEntryPoint.Get() !=
      contextual_tasks::EntryPointOption::kToolbarEphemeralBranded) {
    return;
  }

  ContextualTasksEphemeralButtonController* const controller =
      ContextualTasksEphemeralButtonController::From(browser_window_interface_);
  CHECK(controller);

  const bool was_visible = GetVisible();
  const bool will_be_visible = controller->ShouldShowEphemeralButton();
  const bool is_panel_open = controller->IsPanelOpen();
  const bool was_active_panel_open = was_visible && was_panel_open_;
  const bool will_be_active_panel_open = will_be_visible && is_panel_open;
  const bool was_active_panel_collapsed = was_visible && !was_panel_open_;
  const bool will_be_active_panel_collapsed = will_be_visible && !is_panel_open;

  if (!was_active_panel_open && will_be_active_panel_open) {
    shown_time_ticks_ = base::TimeTicks();
    contextual_tasks::RecordEphemeralButtonShown(/*is_panel_open=*/true);
  } else if (!was_active_panel_collapsed && will_be_active_panel_collapsed) {
    shown_time_ticks_ = base::TimeTicks::Now();
    contextual_tasks::RecordEphemeralButtonShown(/*is_panel_open=*/false);
  }

  if (!was_visible && will_be_visible) {
    if (layer()) {
      layer()->SetOpacity(0.0f);  // Silence the flash before it becomes visible
    }
    ClearDropShadow();
    SetVisible(true);
    AnimateShow();
  } else {
    if (!will_be_visible) {
      anchor_highlight_.reset();
      if (const auto removal_reason = controller->removal_reason();
          removal_reason.has_value()) {
        switch (*removal_reason) {
          case ContextualTasksEphemeralButtonController::RemovalReason::
              kContextMenu:
            RecordShownDuration("RemovedByContextMenu");
            break;
          case ContextualTasksEphemeralButtonController::RemovalReason::
              kAutoDismissed:
            RecordShownDuration("AutoDismissed");
            break;
          case ContextualTasksEphemeralButtonController::RemovalReason::
              kTaskDisassociated:
            RecordShownDuration("TaskDisassociated");
            break;
        }
      }
      shown_time_ticks_ = base::TimeTicks();
      if (layer() && layer()->GetAnimator()) {
        layer()->GetAnimator()->AbortAllAnimations();
      }
      ClearDropShadow();
      if (was_visible) {
        if (auto* const user_ed = BrowserUserEducationInterface::From(
                browser_window_interface_)) {
          user_ed->AbortFeaturePromo(
              feature_engagement::
                  kIPHContextualTasksEphemeralToolbarButtonFeature);
        }
      }
    } else {
      UpdateDropShadow();
      MaybeToggleFeaturePromo();
    }
    SetVisible(will_be_visible);
  }
  was_panel_open_ = is_panel_open;
  UpdateStatusIndicator();
  UpdateHighlight();
  UpdateTooltipText();
  UpdateContextMenu();
}

void ContextualTasksButton::UpdateHighlight() {
  if (!contextual_tasks::IsContextualTasksPinningV2Enabled()) {
    anchor_highlight_.reset();
    return;
  }
  const bool should_highlight = GetVisible() && IsPanelOpen();
  if (should_highlight) {
    if (!anchor_highlight_.has_value()) {
      anchor_highlight_ = AddAnchorHighlight();
    }
  } else {
    anchor_highlight_.reset();
  }
  UpdateColorsAndInsets();
}

void ContextualTasksButton::UpdateTooltipText() {
  const bool is_panel_open = IsPanelOpen();

  int tooltip_id = IDS_CONTEXTUAL_TASKS_ENTRY_POINT_TOOLTIP;
  if (contextual_tasks::kShowEntryPoint.Get() ==
      contextual_tasks::EntryPointOption::kToolbarEphemeralBranded) {
    tooltip_id = is_panel_open
                     ? IDS_CONTEXTUAL_TASKS_EPHEMERAL_BUTTON_CLOSE_TOOL_TIP
                     : IDS_CONTEXTUAL_TASKS_SIDE_PANEL_REOPEN_TOOL_TIP;
  }
  const std::u16string button_tooltip = l10n_util::GetStringUTF16(tooltip_id);
  GetViewAccessibility().SetName(button_tooltip);
  SetTooltipText(button_tooltip);
}

void ContextualTasksButton::RecordShownDuration(std::string_view end_reason) {
  if (shown_time_ticks_.is_null()) {
    return;
  }
  const base::TimeDelta duration = base::TimeTicks::Now() - shown_time_ticks_;
  shown_time_ticks_ = base::TimeTicks();
  base::UmaHistogramLongTimes(
      "ContextualTasks.EphemeralToolbarButton.ShownDuration", duration);
  base::UmaHistogramLongTimes(
      base::StrCat({"ContextualTasks.EphemeralToolbarButton.ShownDuration.",
                    end_reason}),
      duration);
}

void ContextualTasksButton::MaybeToggleFeaturePromo() {
  ContextualTasksEphemeralButtonController* const controller =
      ContextualTasksEphemeralButtonController::From(browser_window_interface_);
  if (!controller) {
    return;
  }
  if (controller->IsPanelOpen()) {
    if (auto* const user_ed =
            BrowserUserEducationInterface::From(browser_window_interface_)) {
      user_ed->AbortFeaturePromo(
          feature_engagement::kIPHContextualTasksEphemeralToolbarButtonFeature);
    }
  } else if (was_panel_open_ &&
             (!layer() || !layer()->GetAnimator() ||
              !layer()->GetAnimator()->is_animating())) {
    MaybeShowFeaturePromo();
  }
}

void ContextualTasksButton::MaybeShowFeaturePromo() {
  if (!GetVisible()) {
    return;
  }
  ContextualTasksEphemeralButtonController* const controller =
      ContextualTasksEphemeralButtonController::From(browser_window_interface_);
  if (!controller || controller->IsPanelOpen()) {
    return;
  }
  if (auto* const user_ed =
          BrowserUserEducationInterface::From(browser_window_interface_)) {
    user_ed->MaybeShowFeaturePromo(
        feature_engagement::kIPHContextualTasksEphemeralToolbarButtonFeature);
  }
}

void ContextualTasksButton::UpdateDropShadow(bool force_paint,
                                             float initial_opacity) {
  if (contextual_tasks::IsContextualTasksPinningV2Enabled()) {
    return;
  }

  if (!GetVisible() && !force_paint) {
    return;
  }

  const auto* color_provider = GetColorProvider();
  if (!color_provider) {
    return;
  }

  if (drop_shadow_painted_layer_ &&
      drop_shadow_painted_layer_->layer()->GetAnimator() &&
      drop_shadow_painted_layer_->layer()->GetAnimator()->is_animating()) {
    UpdateDropShadowLayerBounds();
    return;
  }

  float target_opacity =
      drop_shadow_painted_layer_
          ? drop_shadow_painted_layer_->layer()->GetTargetOpacity()
          : initial_opacity;

  ClearDropShadow();

  const ChromeColorIds bg_color_id =
      ShouldShowCircleHighlight() ? kColorAvatarButtonHighlightDefault
                                  : kColorToolbar;
  auto contextual_tasks_button_background_painter =
      std::make_unique<ContextualTasksButtonBackgroundPainter>(
          color_provider->GetColor(bg_color_id),
          color_provider->GetColor(kColorToolbarContextualTasksButtonShadow),
          GetShape());

  drop_shadow_painted_layer_ = views::Painter::CreatePaintedLayer(
      std::move(contextual_tasks_button_background_painter));
  ui::Layer* const drop_shadow_layer = drop_shadow_painted_layer_->layer();
  drop_shadow_layer->SetFillsBoundsOpaquely(false);
  drop_shadow_layer->SetOpacity(target_opacity);

  // Use the views version of AddLayerToRegion because the LabelButton already
  // overrides AddLayerToRegion() to support painting labels. As a result, the
  // unqualified version will result in the shadow being rendered incorrectly.
  views::View::AddLayerToRegion(drop_shadow_layer, views::LayerRegion::kBelow);
  UpdateDropShadowLayerBounds();
}

void ContextualTasksButton::UpdateDropShadowLayerBounds() {
  CHECK(drop_shadow_painted_layer_);
  gfx::Rect layer_bounds = GetLocalBounds();
  layer_bounds.Outset(kShadowOutset);
  layer_bounds.Offset(layer()->bounds().OffsetFromOrigin());
  drop_shadow_painted_layer_->layer()->SetBounds(layer_bounds);
}

void ContextualTasksButton::UpdateStatusIndicator() {
  if (!status_indicator_) {
    return;
  }
  if (contextual_tasks::
          IsContextualTasksPinnedButtonActiveIndicatorEnabled() &&
      !contextual_tasks::
          IsContextualTasksPinnedButtonActiveIndicatorCircleHighlightEnabled() &&
      GetVisible()) {
    status_indicator_->Show();
  } else {
    status_indicator_->Hide();
  }
}

void ContextualTasksButton::AnimateShow() {
  UpdateDropShadow(/*force_paint=*/true, /*initial_opacity=*/0.0f);
  if (!layer()) {
    MaybeShowFeaturePromo();
    return;
  }
  views::AnimationBuilder builder;
  auto& sequence =
      builder
          .OnEnded(base::BindOnce(&ContextualTasksButton::MaybeShowFeaturePromo,
                                  weak_ptr_factory_.GetWeakPtr()))
          .Once()
          .SetDuration(base::Milliseconds(350))
          .SetOpacity(layer(), 1.0f);

  if (drop_shadow_painted_layer_) {
    drop_shadow_painted_layer_->layer()->SetOpacity(0.0f);
    sequence.SetOpacity(drop_shadow_painted_layer_->layer(), 1.0f);
  }
}

void ContextualTasksButton::ClearDropShadow() {
  if (drop_shadow_painted_layer_) {
    if (auto* drop_shadow_layer = drop_shadow_painted_layer_->layer()) {
      if (drop_shadow_layer->GetAnimator()) {
        drop_shadow_layer->GetAnimator()->AbortAllAnimations();
      }
      views::View::RemoveLayerFromRegions(drop_shadow_layer);
    }
    drop_shadow_painted_layer_.reset();
  }
}

bool ContextualTasksButton::IsPanelOpen() const {
  ContextualTasksEphemeralButtonController* const ephemeral_controller =
      ContextualTasksEphemeralButtonController::From(browser_window_interface_);
  auto* const panel_controller =
      contextual_tasks::ContextualTasksPanelController::From(
          browser_window_interface_);
  return ephemeral_controller
             ? (ephemeral_controller->IsPanelOpen() && panel_controller &&
                panel_controller->IsPanelOpenForContextualTask())
             : (panel_controller &&
                panel_controller->IsPanelOpenForContextualTask());
}

bool ContextualTasksButton::ShouldShowCircleHighlight() const {
  if (!contextual_tasks::
          IsContextualTasksPinnedButtonActiveIndicatorCircleHighlightEnabled() ||
      IsPanelOpen()) {
    return false;
  }
  ContextualTasksEphemeralButtonController* const ephemeral_controller =
      ContextualTasksEphemeralButtonController::From(browser_window_interface_);
  return ephemeral_controller &&
         ephemeral_controller->IsActiveTabAssociatedToTask();
}

ui::ImageModel ContextualTasksButton::GetButtonImage() {
  const bool use_active_indicator =
      contextual_tasks::IsContextualTasksPinnedButtonActiveIndicatorEnabled() &&
      !contextual_tasks::
          IsContextualTasksPinnedButtonActiveIndicatorCircleHighlightEnabled();
  const ui::ColorId icon_color_id =
      use_active_indicator ? ui::ColorId{kColorToolbarActionItemEngaged}
                           : ui::ColorId{ui::kColorIcon};
  if (contextual_tasks::ShouldUseSparkIconForPinButtons()) {
    return ui::ImageModel::FromVectorIcon(
        features::IsRoundedIconsEnabled() ? omnibox::kSearchSparkIcon
                                          : omnibox::kSearchSparkOldIcon,
        icon_color_id,
        GetShape() == EphemeralButtonShape::kCircle ? GetIconSize()
                                                    : kGLogoPillShapeIconSize);
  }
  if (contextual_tasks::ShouldUseGraySuperGIconForPinButtons()) {
    const gfx::VectorIcon& gray_super_g_icon =
#if BUILDFLAG(GOOGLE_CHROME_BRANDING)
        vector_icons::kGoogleGLogoMonochromeIcon;
#else
        features::IsRoundedIconsEnabled() ? kChromeProductIcon
                                          : kBrowserLogoOldIcon;
#endif
    return ui::ImageModel::FromVectorIcon(
        gray_super_g_icon, icon_color_id,
        GetShape() == EphemeralButtonShape::kCircle ? GetIconSize()
                                                    : kGLogoPillShapeIconSize);
  }
#if BUILDFLAG(GOOGLE_CHROME_BRANDING)
  if (contextual_tasks::ShouldUseSuperGIconForPinButtons() ||
      contextual_tasks::kShowEntryPoint.Get() ==
          contextual_tasks::EntryPointOption::kToolbarEphemeralBranded) {
    return ui::ImageModel::FromImageSkia(
        *ui::ResourceBundle::GetSharedInstance().GetImageSkiaNamed(
            IDR_GOOGLE_G_GRADIENT_16_ALT));
  }
#endif
  const gfx::VectorIcon& contextual_tasks_icon =
#if BUILDFLAG(GOOGLE_CHROME_BRANDING)
      vector_icons::kGoogleGLogoIcon;
#else
      features::IsRoundedIconsEnabled() ? kChromeProductIcon
                                        : kBrowserLogoOldIcon;
#endif
  return ui::ImageModel::FromVectorIcon(
      contextual_tasks_icon, icon_color_id,
      GetShape() == EphemeralButtonShape::kCircle ? kGLogoCircularShapeIconSize
                                                  : kGLogoPillShapeIconSize);
}

BEGIN_METADATA(ContextualTasksButton)
END_METADATA
