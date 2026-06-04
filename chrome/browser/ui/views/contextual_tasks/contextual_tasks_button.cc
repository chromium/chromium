// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/views/contextual_tasks/contextual_tasks_button.h"

#include <string>

#include "base/check_op.h"
#include "base/feature_list.h"
#include "base/functional/bind.h"
#include "base/metrics/histogram_functions.h"
#include "base/metrics/user_metrics.h"
#include "base/metrics/user_metrics_action.h"
#include "chrome/app/vector_icons/vector_icons.h"
#include "chrome/browser/contextual_tasks/contextual_tasks_panel_controller.h"
#include "chrome/browser/contextual_tasks/entry_point_eligibility_manager.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/ui/actions/chrome_action_id.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/browser/ui/browser_actions.h"
#include "chrome/browser/ui/browser_window/public/browser_window_features.h"
#include "chrome/browser/ui/browser_window/public/browser_window_interface.h"
#include "chrome/browser/ui/layout_constants.h"
#include "chrome/browser/ui/tabs/vertical_tab_strip_state_controller.h"
#include "chrome/browser/ui/ui_features.h"
#include "chrome/browser/ui/views/contextual_tasks/contextual_tasks_ephemeral_button_controller.h"
#include "chrome/browser/ui/views/frame/browser_view.h"
#include "chrome/browser/ui/views/frame/immersive_mode_controller.h"
#include "chrome/browser/ui/views/toolbar/toolbar_button.h"
#include "chrome/browser/ui/views/toolbar/toolbar_ink_drop_util.h"
#include "chrome/common/pref_names.h"
#include "chrome/grit/branded_strings.h"
#include "chrome/grit/theme_resources.h"
#include "components/contextual_tasks/public/features.h"
#include "components/prefs/pref_member.h"
#include "components/prefs/pref_service.h"
#include "components/strings/grit/components_strings.h"
#include "components/vector_icons/vector_icons.h"
#include "third_party/omnibox_proto/chrome_aim_entry_point.pb.h"
#include "third_party/skia/include/core/SkRRect.h"
#include "ui/actions/actions.h"
#include "ui/base/interaction/element_identifier.h"
#include "ui/base/l10n/l10n_util.h"
#include "ui/base/metadata/metadata_impl_macros.h"
#include "ui/base/models/image_model.h"
#include "ui/base/resource/resource_bundle.h"
#include "ui/base/ui_base_features.h"
#include "ui/compositor/layer.h"
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

DEFINE_CLASS_ELEMENT_IDENTIFIER_VALUE(ContextualTasksButton,
                                      kContextualTasksToolbarButton);

namespace {

// Will correspond to a 28x28 shadow circle when the location bar height is 34.
const int kCircleShadowInset = 3;

// Corresponds to a 32x32 pill background shape when the location bar height
// is 34.
const int kPillShapeShadowInset = 1;

const int kGLogoCircularShapeIconSize = 16;

const int kGLogoPillShapeIconSize = 15;

// The top-left radius is needed to avoid the toolbar upper left rounded corner.
// This will translate to the top-right if the button is on that side.
const float kTopLeftRadius = 5.0f;

// Like the above, this radius corresponds to the button's bottom left corner.
const float kBottomLeftRadius = 0.0f;

// Margin to outset the background painted layer so the shadow is not clipped.
const int kShadowOutset = 12;

// Helper class to paint the contextual tasks button shadow. The general
// ViewShadow class doesn't work for the contextual tasks button because the
// button doesn't have a standard shape, and thus custom shadow logic.
class ContextualTasksButtonBackgroundPainter : public views::Painter {
 public:
  ContextualTasksButtonBackgroundPainter(SkColor bg_color,
                                         SkColor shadow_color,
                                         bool circle_shape)
      : bg_color_(bg_color),
        shadow_color_(shadow_color),
        is_circle_shape_(circle_shape) {}
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

    if (is_circle_shape_) {
      gfx::Rect inset_rect = button_rect;
      inset_rect.Inset(gfx::Insets(kCircleShadowInset));
      gfx::RectF fill_rect(gfx::ScaleToEnclosedRect(inset_rect, scale));
      gfx::PointF center = fill_rect.CenterPoint();
      float scaled_radius =
          std::min(fill_rect.width(), fill_rect.height()) / 2.0f;
      canvas->DrawCircle(center, scaled_radius, flags);
    } else {
      gfx::Rect inset_rect = button_rect;
      inset_rect.Inset(gfx::Insets::TLBR(kPillShapeShadowInset, 0,
                                         kPillShapeShadowInset,
                                         kPillShapeShadowInset));
      gfx::Rect fill_rect = gfx::ScaleToEnclosingRect(inset_rect, scale);

      float radius = fill_rect.height() / 2.0f;

      const SkVector radii[4] = {
          {kTopLeftRadius, kTopLeftRadius},       // top-left
          {radius, radius},                       // top-right
          {radius, radius},                       // bottom-right
          {kBottomLeftRadius, kBottomLeftRadius}  // bottom-left
      };

      SkRRect rrect;
      rrect.setRectRadii(gfx::RectToSkRect(fill_rect), radii);

      canvas->sk_canvas()->drawRRect(rrect, flags);
    }
  }

 private:
  const SkColor bg_color_;
  const SkColor shadow_color_;
  const bool is_circle_shape_;
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
    if (button_->ShouldApplyCircularBackgroundShadow()) {
      rect.Inset(gfx::Insets(kCircleShadowInset));
      return SkPath::Oval(gfx::RectToSkRect(rect));
    } else {
      rect.Inset(gfx::Insets::TLBR(kPillShapeShadowInset, 0,
                                   kPillShapeShadowInset,
                                   kPillShapeShadowInset));
      const float radius = rect.height() / 2.0f;
      const SkVector radii[4] = {
          {kTopLeftRadius, kTopLeftRadius},       // top-left
          {radius, radius},                       // top-right
          {radius, radius},                       // bottom-right
          {kBottomLeftRadius, kBottomLeftRadius}  // bottom-left
      };
      SkRRect rrect;
      rrect.setRectRadii(gfx::RectToSkRect(rect), radii);
      return SkPath::RRect(rrect);
    }
  }

 private:
  const raw_ptr<ContextualTasksButton> button_;
};

}  // namespace

ContextualTasksButton::ContextualTasksButton(
    BrowserWindowInterface* browser_window_interface)
    : ToolbarButton(base::BindRepeating(&ContextualTasksButton::OnButtonPress,
                                        base::Unretained(this)),
                    nullptr,
                    nullptr),
      browser_window_interface_(browser_window_interface) {
  SetPaintToLayer();
  layer()->SetFillsBoundsOpaquely(false);
  SetProperty(views::kElementIdentifierKey, kContextualTasksToolbarButton);
  const std::u16string button_tooltip =
#if BUILDFLAG(GOOGLE_CHROME_BRANDING)
      (contextual_tasks::kShowEntryPoint.Get() ==
       contextual_tasks::EntryPointOption::kToolbarEphemeralBranded)
          ? l10n_util::GetStringUTF16(
                IDS_CONTEXTUAL_TASKS_ENTRY_POINT_TOOLTIP_V2)
          : l10n_util::GetStringUTF16(IDS_CONTEXTUAL_TASKS_ENTRY_POINT_TOOLTIP);
#else
      l10n_util::GetStringUTF16(IDS_CONTEXTUAL_TASKS_ENTRY_POINT_TOOLTIP);
#endif
  GetViewAccessibility().SetName(button_tooltip);
  SetTooltipText(button_tooltip);

  side_panel_alignment_.Init(
      prefs::kSidePanelHorizontalAlignment,
      browser_window_interface->GetProfile()->GetPrefs(),
      base::BindRepeating(&ContextualTasksButton::OnSidePanelAlignmentChanged,
                          base::Unretained(this)));

  if (contextual_tasks::kShowEntryPoint.Get() ==
      contextual_tasks::EntryPointOption::kToolbarEphemeralBranded) {
    ContextualTasksEphemeralButtonController* const controller =
        ContextualTasksEphemeralButtonController::From(
            browser_window_interface_);
    should_update_visibility_subscription_ =
        controller->RegisterShouldUpdateButtonVisibility(base::BindRepeating(
            &ContextualTasksButton::OnShouldUpdateVisibility,
            base::Unretained(this)));
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

  OnSidePanelAlignmentChanged();
  MaybeUpdateVisibility();

  if (contextual_tasks::kShowEntryPoint.Get() ==
      contextual_tasks::EntryPointOption::kToolbarEphemeralBranded) {
    views::HighlightPathGenerator::Install(
        this,
        std::make_unique<ContextualTasksButtonHighlightPathGenerator>(this));
  }
}

ContextualTasksButton::~ContextualTasksButton() {
  if (drop_shadow_painted_layer_) {
    views::View::RemoveLayerFromRegions(drop_shadow_painted_layer_->layer());
  }
}

float ContextualTasksButton::GetCornerRadiusFor(
    ToolbarButton::Edge edge) const {
  if (contextual_tasks::kShowEntryPoint.Get() ==
          contextual_tasks::EntryPointOption::kToolbarEphemeralBranded &&
      !ShouldApplyCircularBackgroundShadow()) {
    switch (edge) {
      case ToolbarButton::Edge::kTopLeft:
        return kTopLeftRadius;
      case ToolbarButton::Edge::kBottomLeft:
        return kBottomLeftRadius;
      default:
        return GetRoundedCornerRadius();
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
  auto* controller = contextual_tasks::ContextualTasksPanelController::From(
      browser_window_interface_);
  CHECK(controller);
  // TODO(crbug.com/480218994): Clean up the ToggleContextualTasksSidePanel
  // browser action, since the logic is now handled in this method.
  if (controller->IsPanelOpenForContextualTask()) {
    base::RecordAction(base::UserMetricsAction(
        "ContextualTasks.ToolbarButton.UserAction.CloseSidePanel"));
    base::UmaHistogramBoolean(
        "ContextualTasks.ToolbarButton.UserAction.CloseSidePanel", true);
    controller->Close();
  } else {
    base::RecordAction(base::UserMetricsAction(
        "ContextualTasks.ToolbarButton.UserAction.OpenSidePanel"));
    base::UmaHistogramBoolean(
        "ContextualTasks.ToolbarButton.UserAction.OpenSidePanel", true);
    controller->Show(
        /*transition_from_tab=*/false,
        omnibox::ChromeAimEntryPoint::DESKTOP_CHROME_COBROWSE_TOOLBAR_BUTTON);
  }
}


void ContextualTasksButton::OnSidePanelAlignmentChanged() {
  if (contextual_tasks::kShowEntryPoint.Get() ==
      contextual_tasks::EntryPointOption::kToolbarEphemeralBranded) {
    SetHorizontalAlignment(gfx::ALIGN_CENTER);
    UpdateColorsAndInsets();
  } else {
    PrefService* const pref_service =
        browser_window_interface_->GetProfile()->GetPrefs();

    const gfx::VectorIcon& contextual_tasks_icon =
        pref_service->GetBoolean(prefs::kSidePanelHorizontalAlignment)
            ? kDockToRightSparkCustomIcon
            : kDockToLeftSparkCustomIcon;
    SetVectorIcon(contextual_tasks_icon);
  }
}

void ContextualTasksButton::UpdateColorsAndInsets() {
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

  const int icon_size = ShouldApplyCircularBackgroundShadow()
                            ? kGLogoCircularShapeIconSize
                            : kGLogoPillShapeIconSize;
  const int icon_inset = (button_size - icon_size) / 2;
  const int vertical_inset =
      ShouldApplyCircularBackgroundShadow() ? icon_inset : 0;
  const int icon_offset = ShouldApplyCircularBackgroundShadow() ? 0 : 2;
  const gfx::Insets insets =
      gfx::Insets::TLBR(vertical_inset, icon_inset - icon_offset,
                        vertical_inset, icon_inset + icon_offset) +
      *GetProperty(views::kInternalPaddingKey);
  SetLayoutInsets(insets);

  if (drop_shadow_painted_layer_) {
    views::View::RemoveLayerFromRegions(drop_shadow_painted_layer_->layer());
  }

  auto contextual_tasks_button_background_painter =
      std::make_unique<ContextualTasksButtonBackgroundPainter>(
          color_provider->GetColor(kColorToolbar),
          color_provider->GetColor(kColorToolbarContextualTasksButtonShadow),
          ShouldApplyCircularBackgroundShadow());

  drop_shadow_painted_layer_ = views::Painter::CreatePaintedLayer(
      std::move(contextual_tasks_button_background_painter));
  ui::Layer* const drop_shadow_layer = drop_shadow_painted_layer_->layer();
  drop_shadow_layer->SetFillsBoundsOpaquely(false);

  // Use the views version of AddLayerToRegion because the LabelButton already
  // overrides AddLayerToRegion() to support painting labels. As a result, the
  // unqualified version will result in the shadow being rendered incorrectly.
  views::View::AddLayerToRegion(drop_shadow_layer, views::LayerRegion::kBelow);
  UpdateDropShadowLayerBounds();
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

void ContextualTasksButton::OnShouldUpdateVisibility(bool should_show) {
  MaybeUpdateVisibility();
}

void ContextualTasksButton::OnEligibilityChange(bool is_eligible) {
  MaybeUpdateVisibility();
}

void ContextualTasksButton::OnSurfaceStateChanged(
    contextual_tasks::ContextualTasksPanelHost::SurfaceState state,
    contextual_tasks::ContextualTasksPanelHost::StateChangeReason reason) {
  MaybeUpdateVisibility();
}

void ContextualTasksButton::OnControllerDestroyed() {
  panel_controller_observation_.Reset();
}

bool ContextualTasksButton::ShouldApplyCircularBackgroundShadow() const {
  ImmersiveModeController* immersive_mode_controller =
      ImmersiveModeController::From(browser_window_interface_);
  if (immersive_mode_controller && immersive_mode_controller->IsEnabled()) {
    return true;
  }

  auto* controller =
      tabs::VerticalTabStripStateController::From(browser_window_interface_);
  return controller && controller->ShouldDisplayVerticalTabs();
}

void ContextualTasksButton::MaybeUpdateVisibility() {
  const bool is_button_eligible =
      contextual_tasks::EntryPointEligibilityManager::From(
          browser_window_interface_)
          ->AreEntryPointsEligible();

  if (contextual_tasks::kShowEntryPoint.Get() ==
      contextual_tasks::EntryPointOption::kToolbarEphemeralBranded) {
    ContextualTasksEphemeralButtonController* const controller =
        ContextualTasksEphemeralButtonController::From(
            browser_window_interface_);
    const bool was_visible = GetVisible();
    SetVisible(is_button_eligible && controller->ShouldShowEphemeralButton());
    if (!was_visible && GetVisible()) {
      layer()->SetOpacity(0.0f);
      drop_shadow_painted_layer_->layer()->SetOpacity(0.0f);
      views::AnimationBuilder()
          .Once()
          .SetDuration(
              base::Milliseconds(features::kSidePanelFlyoverDurationMs.Get()))
          .SetOpacity(layer(), 1.0f)
          .SetOpacity(drop_shadow_painted_layer_->layer(), 1.0f);
    }
  }
}

void ContextualTasksButton::UpdateDropShadowLayerBounds() {
  CHECK(drop_shadow_painted_layer_);
  gfx::Rect layer_bounds = GetLocalBounds();
  layer_bounds.Outset(kShadowOutset);
  layer_bounds.Offset(layer()->bounds().OffsetFromOrigin());
  drop_shadow_painted_layer_->layer()->SetBounds(layer_bounds);
}

ui::ImageModel ContextualTasksButton::GetButtonImage() {
#if BUILDFLAG(GOOGLE_CHROME_BRANDING)
  if (contextual_tasks::kShowEntryPoint.Get() ==
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
  return ui::ImageModel::FromVectorIcon(contextual_tasks_icon, ui::kColorIcon,
                                        ShouldApplyCircularBackgroundShadow()
                                            ? kGLogoCircularShapeIconSize
                                            : kGLogoPillShapeIconSize);
}

BEGIN_METADATA(ContextualTasksButton)
END_METADATA
