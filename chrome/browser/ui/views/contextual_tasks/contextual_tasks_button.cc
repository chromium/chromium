// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/views/contextual_tasks/contextual_tasks_button.h"

#include <string>
#include <string_view>

#include "base/check_op.h"
#include "base/functional/bind.h"
#include "base/metrics/histogram_functions.h"
#include "base/metrics/user_metrics.h"
#include "base/metrics/user_metrics_action.h"
#include "base/notreached.h"
#include "base/strings/strcat.h"
#include "chrome/app/vector_icons/vector_icons.h"
#include "chrome/browser/contextual_tasks/contextual_tasks_panel_controller.h"
#include "chrome/browser/contextual_tasks/contextual_tasks_utils.h"
#include "chrome/browser/contextual_tasks/entry_point_eligibility_manager.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/ui/actions/chrome_action_id.h"
#include "chrome/browser/ui/browser_actions.h"
#include "chrome/browser/ui/browser_element_identifiers.h"
#include "chrome/browser/ui/browser_window/public/browser_window_features.h"
#include "chrome/browser/ui/browser_window/public/browser_window_interface.h"
#include "chrome/browser/ui/immersive/immersive_mode_controller.h"
#include "chrome/browser/ui/layout_constants.h"
#include "chrome/browser/ui/side_panel/side_panel_entry_id.h"
#include "chrome/browser/ui/tabs/vertical_tab_strip_state_controller.h"
#include "chrome/browser/ui/ui_features.h"
#include "chrome/browser/ui/user_education/browser_user_education_interface.h"
#include "chrome/browser/ui/views/contextual_tasks/contextual_tasks_ephemeral_button_controller.h"
#include "chrome/browser/ui/views/frame/browser_view.h"
#include "chrome/browser/ui/views/toolbar/pinned_action_toolbar_button_menu_model.h"
#include "chrome/browser/ui/views/toolbar/toolbar_button.h"
#include "chrome/browser/ui/views/toolbar/toolbar_ink_drop_util.h"
#include "chrome/common/pref_names.h"
#include "chrome/grit/branded_strings.h"
#include "chrome/grit/generated_resources.h"
#include "chrome/grit/theme_resources.h"
#include "components/contextual_tasks/public/features.h"
#include "components/feature_engagement/public/feature_constants.h"
#include "components/omnibox/browser/vector_icons.h"
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

namespace {

// Will correspond to a 34x34 shadow circle when the location bar height is 34.
const int kCircleShadowInset = 0;

// Corresponds to a 32x32 pill background shape when the location bar height
// is 34.
const int kPillShapeShadowInset = 1;

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

gfx::Insets GetPillShadowInsets(ContextualTasksButton::Shape shape) {
  gfx::Insets shadow_insets =
      gfx::Insets::VH(kPillShapeShadowInset, kPillShapeShadowInset);
  if (shape == ContextualTasksButton::Shape::kFlatEdgeRight) {
    shadow_insets.set_right(0);
  } else {
    shadow_insets.set_left(0);
  }

  return shadow_insets;
}

SkRRect GetPillRRect(const gfx::Rect& rect,
                     ContextualTasksButton::Shape shape) {
  const float radius = rect.height() / 2.0f;
  SkVector radii[4];
  if (shape == ContextualTasksButton::Shape::kFlatEdgeRight) {
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
                                         ContextualTasksButton::Shape shape)
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
      case ContextualTasksButton::Shape::kCircle: {
        gfx::Rect inset_rect = button_rect;
        inset_rect.Inset(gfx::Insets(kCircleShadowInset));
        gfx::RectF fill_rect(gfx::ScaleToEnclosedRect(inset_rect, scale));
        gfx::PointF center = fill_rect.CenterPoint();
        float scaled_radius =
            std::min(fill_rect.width(), fill_rect.height()) / 2.0f;
        canvas->DrawCircle(center, scaled_radius, flags);
        break;
      }
      case ContextualTasksButton::Shape::kFlatEdgeRight:
      case ContextualTasksButton::Shape::kFlatEdgeLeft: {
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
  const ContextualTasksButton::Shape shape_;
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
    const ContextualTasksButton::Shape shape = button_->GetShape();
    switch (shape) {
      case ContextualTasksButton::Shape::kCircle:
        rect.Inset(gfx::Insets(kCircleShadowInset));
        return SkPath::Oval(gfx::RectToSkRect(rect));
      case ContextualTasksButton::Shape::kFlatEdgeRight:
      case ContextualTasksButton::Shape::kFlatEdgeLeft:
        rect.Inset(GetPillShadowInsets(shape));
        return SkPath::RRect(GetPillRRect(rect, shape));
    }
  }

 private:
  const raw_ptr<ContextualTasksButton> button_;
};

enum ContextualTasksButtonContextMenuCommand {
  kCommandRemoveForCurrentTask = 1,
  kCommandPinToToolbar = 2,
  kCommandUnpinFromToolbar = 3,
  kCommandCustomizeToolbar = 4,
};

constexpr size_t kPinnedActionMenuPinIndex = 0;
constexpr size_t kPinnedActionMenuUnpinIndex = 1;
constexpr size_t kPinnedActionMenuCustomizeIndex = 2;

std::optional<size_t> GetPinnedActionMenuIndexForCommand(int command_id) {
  switch (command_id) {
    case kCommandPinToToolbar:
      return kPinnedActionMenuPinIndex;
    case kCommandUnpinFromToolbar:
      return kPinnedActionMenuUnpinIndex;
    case kCommandCustomizeToolbar:
      return kPinnedActionMenuCustomizeIndex;
    default:
      return std::nullopt;
  }
}

}  // namespace

DEFINE_CLASS_ELEMENT_IDENTIFIER_VALUE(
    ContextualTasksButton,
    kContextualTasksEphemeralButtonRemoveForTaskMenuItem);

ContextualTasksButton::ContextualTasksButton(
    BrowserWindowInterface* browser_window_interface)
    : ToolbarButton(
          base::BindRepeating(&ContextualTasksButton::OnButtonPress,
                              base::Unretained(this)),
          base::FeatureList::IsEnabled(
              contextual_tasks::kContextualTasksEphemeralButtonContextMenu)
              ? std::make_unique<ui::SimpleMenuModel>(this)
              : nullptr,
          nullptr,
          /*trigger_menu_on_long_press=*/false),
      browser_window_interface_(browser_window_interface) {
  if (auto* simple_menu_model =
          static_cast<ui::SimpleMenuModel*>(menu_model())) {
    const int close_label_id =
        contextual_tasks::
                ShouldUseCloseSidePanelForEphemeralButtonContextMenu()
            ? IDS_CONTEXTUAL_TASKS_EPHEMERAL_BUTTON_CONTEXT_MENU_CLOSE
            : IDS_CONTEXTUAL_TASKS_EPHEMERAL_BUTTON_CONTEXT_MENU_REMOVE;
    simple_menu_model->AddItemWithStringIdAndIcon(
        kCommandRemoveForCurrentTask, close_label_id,
        ui::ImageModel::FromVectorIcon(
            features::IsRoundedIconsEnabled() ? vector_icons::kCloseIcon
                                              : vector_icons::kCloseOldIcon,
            ui::kColorIcon, ui::SimpleMenuModel::kDefaultIconSize));
    simple_menu_model->SetElementIdentifierAt(
        0, kContextualTasksEphemeralButtonRemoveForTaskMenuItem);
    simple_menu_model->AddSeparator(ui::NORMAL_SEPARATOR);

    pinned_action_menu_model_ =
        std::make_unique<PinnedActionToolbarButtonMenuModel>(
            browser_window_interface_, kActionSidePanelShowContextualTasks);
    constexpr std::pair<int, size_t> kPinnedMenuCommands[] = {
        {kCommandPinToToolbar, kPinnedActionMenuPinIndex},
        {kCommandUnpinFromToolbar, kPinnedActionMenuUnpinIndex},
        {kCommandCustomizeToolbar, kPinnedActionMenuCustomizeIndex},
    };
    for (const auto& [command_id, index] : kPinnedMenuCommands) {
      simple_menu_model->AddItemWithIcon(
          command_id, pinned_action_menu_model_->GetLabelAt(index),
          pinned_action_menu_model_->GetIconAt(index));
      simple_menu_model->SetElementIdentifierAt(
          simple_menu_model->GetItemCount() - 1,
          pinned_action_menu_model_->GetElementIdentifierAt(index));
    }
  }
  SetPaintToLayer();
  layer()->SetFillsBoundsOpaquely(false);
  // The contextual tasks button is ephemeral and starts hidden until an active
  // task requires it.
  SetVisible(false);
  SetProperty(views::kElementIdentifierKey,
              kContextualTasksEphemeralToolbarButtonElementId);
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
    const Shape button_shape = GetShape();
    switch (edge) {
      case ToolbarButton::Edge::kTopLeft:
        if (button_shape == Shape::kFlatEdgeLeft) {
          return kFlatEdgeTopRadius;
        }
        break;
      case ToolbarButton::Edge::kTopRight:
        if (button_shape == Shape::kFlatEdgeRight) {
          return kFlatEdgeTopRadius;
        }
        break;
      case ToolbarButton::Edge::kBottomLeft:
        if (button_shape == Shape::kFlatEdgeLeft) {
          return kFlatEdgeBottomRadius;
        }
        break;
      case ToolbarButton::Edge::kBottomRight:
        if (button_shape == Shape::kFlatEdgeRight) {
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
  if (auto* const user_ed =
          BrowserUserEducationInterface::From(browser_window_interface_);
      user_ed && user_ed->IsFeaturePromoActive(
                     feature_engagement::
                         kIPHContextualTasksEphemeralToolbarButtonFeature)) {
    user_ed->NotifyFeaturePromoFeatureUsed(
        feature_engagement::kIPHContextualTasksEphemeralToolbarButtonFeature,
        FeaturePromoFeatureUsedAction::kClosePromoIfPresent);
  }

  auto* controller = contextual_tasks::ContextualTasksPanelController::From(
      browser_window_interface_);
  CHECK(controller);

  // When kEphemeralPinningVisibleWhenPermanentlyPinned is enabled, the
  // ephemeral button remains visible alongside the pinned button, and presses
  // on this button should always be logged as EphemeralToolbarButton actions.
  bool is_pinned =
      !base::FeatureList::IsEnabled(
          contextual_tasks::kEphemeralPinningVisibleWhenPermanentlyPinned) &&
      contextual_tasks::GetEffectivePinState(
          browser_window_interface_->GetProfile());

  if (controller->IsPanelOpenForContextualTask()) {
    base::RecordAction(base::UserMetricsAction(
        "ContextualTasks.ToolbarButton.UserAction.CloseSidePanel"));
    base::UmaHistogramBoolean(
        "ContextualTasks.ToolbarButton.UserAction.CloseSidePanel", true);

    const char* sub_action =
        is_pinned
            ? "ContextualTasks.PermanentToolbarButton.UserAction.CloseSidePanel"
            : "ContextualTasks.EphemeralToolbarButton.UserAction."
              "CloseSidePanel";
    base::RecordAction(base::UserMetricsAction(sub_action));
    base::UmaHistogramBoolean(sub_action, true);

    controller->Close();
  } else {
    base::RecordAction(base::UserMetricsAction(
        "ContextualTasks.ToolbarButton.UserAction.OpenSidePanel"));
    base::UmaHistogramBoolean(
        "ContextualTasks.ToolbarButton.UserAction.OpenSidePanel", true);

    const char* sub_action =
        is_pinned
            ? "ContextualTasks.PermanentToolbarButton.UserAction.OpenSidePanel"
            : "ContextualTasks.EphemeralToolbarButton.UserAction.OpenSidePanel";
    base::RecordAction(base::UserMetricsAction(sub_action));
    base::UmaHistogramBoolean(sub_action, true);

    if (!is_pinned) {
      RecordShownDuration("Clicked");
    }

    if (contextual_tasks::kShowEntryPoint.Get() ==
        contextual_tasks::EntryPointOption::kToolbarEphemeralBranded) {
      controller->Show(
          /*transition_from_tab=*/false,
          is_pinned ? omnibox::ChromeAimEntryPoint::
                          DESKTOP_CHROME_COBROWSE_PINNED_TOOLBAR_BUTTON
                    : omnibox::ChromeAimEntryPoint::UNKNOWN_AIM_ENTRY_POINT,
          /*use_no_animation=*/false, /*open_time_ticks=*/std::nullopt,
          is_pinned
              ? std::nullopt
              : std::make_optional(
                    contextual_tasks::ContextualTasksPanelController::
                        EntrySource::kEphemeralToolbarButton));
    } else {
      controller->OpenInZeroState();
    }
  }
}

bool ContextualTasksButton::IsTrailing() const {
  return IsSidePanelRightAligned() != base::i18n::IsRTL();
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
    } else {
      const gfx::VectorIcon& contextual_tasks_icon =
          IsSidePanelRightAligned() ? kDockToRightSparkCustomIcon
                                    : kDockToLeftSparkCustomIcon;
      SetVectorIcon(contextual_tasks_icon);
    }
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

  const Shape shape = GetShape();
  if (shape == Shape::kCircle) {
    ClearProperty(views::kMarginsKey);
  } else {
    SetProperty(views::kMarginsKey, gfx::Insets());
  }
  gfx::Insets insets;
  switch (shape) {
    case Shape::kCircle: {
      const int icon_inset = (button_size - GetIconSize()) / 2;
      insets = gfx::Insets(icon_inset);
      break;
    }
    case Shape::kFlatEdgeRight:
    case Shape::kFlatEdgeLeft: {
      const int icon_inset = (button_size - kGLogoPillShapeIconSize) / 2;
      constexpr int kIconOffset = 2;
      const int rounded_edge_inset = icon_inset + kIconOffset;
      const int flat_edge_inset = icon_inset - kIconOffset;
      insets =
          (shape == Shape::kFlatEdgeRight)
              ? gfx::Insets::TLBR(0, rounded_edge_inset, 0, flat_edge_inset)
              : gfx::Insets::TLBR(0, flat_edge_inset, 0, rounded_edge_inset);
      break;
    }
  }
  SetLayoutInsets(insets + *GetProperty(views::kInternalPaddingKey));

  UpdateDropShadow();
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

bool ContextualTasksButton::IsCommandIdEnabled(int command_id) const {
  if (std::optional<size_t> index =
          GetPinnedActionMenuIndexForCommand(command_id);
      index.has_value() && pinned_action_menu_model_) {
    return pinned_action_menu_model_->IsEnabledAt(*index);
  }
  return true;
}

bool ContextualTasksButton::IsCommandIdVisible(int command_id) const {
  if (std::optional<size_t> index =
          GetPinnedActionMenuIndexForCommand(command_id);
      index.has_value() && pinned_action_menu_model_) {
    return pinned_action_menu_model_->IsVisibleAt(*index);
  }
  return true;
}

void ContextualTasksButton::ExecuteCommand(int command_id, int event_flags) {
  if (command_id == kCommandRemoveForCurrentTask) {
    auto* const controller = ContextualTasksEphemeralButtonController::From(
        browser_window_interface_);
    if (!controller) {
      return;
    }
    base::RecordAction(base::UserMetricsAction(
        "ContextualTasks.EphemeralToolbarButton.UserAction."
        "RemoveByContextMenu"));
    base::UmaHistogramBoolean(
        "ContextualTasks.EphemeralToolbarButton.UserAction."
        "RemoveByContextMenu",
        true);
    controller->RemoveEphemeralButtonForCurrentTask();
    return;
  }
  if (std::optional<size_t> index =
          GetPinnedActionMenuIndexForCommand(command_id);
      index.has_value() && pinned_action_menu_model_) {
    if (command_id == kCommandPinToToolbar) {
      base::RecordAction(base::UserMetricsAction(
          "ContextualTasks.EphemeralToolbarButton.UserAction."
          "PinByContextMenu"));
      base::UmaHistogramBoolean(
          "ContextualTasks.EphemeralToolbarButton.UserAction."
          "PinByContextMenu",
          true);
    }
    pinned_action_menu_model_->ActivatedAt(*index, event_flags);
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

ContextualTasksButton::Shape ContextualTasksButton::GetShape() const {
  ImmersiveModeController* const immersive_mode_controller =
      ImmersiveModeController::From(browser_window_interface_);
  if (immersive_mode_controller && immersive_mode_controller->IsEnabled()) {
    return Shape::kCircle;
  }

  auto* const controller =
      tabs::VerticalTabStripStateController::From(browser_window_interface_);
  if (controller && controller->ShouldDisplayVerticalTabs()) {
    return Shape::kCircle;
  }

  if (contextual_tasks::IsContextualTasksPinningV2Enabled() ||
      (contextual_tasks::kEnableCircularEphemeralButtonNextToBatterySaver
           .Get() &&
       IsSidePanelRightAligned())) {
    return Shape::kCircle;
  }

  return IsSidePanelRightAligned() ? Shape::kFlatEdgeRight
                                   : Shape::kFlatEdgeLeft;
}

bool ContextualTasksButton::IsHighlighted() const {
  return anchor_highlight_.has_value();
}

ui::Layer* ContextualTasksButton::GetDropShadowLayerForTesting() const {
  return drop_shadow_painted_layer_ ? drop_shadow_painted_layer_->layer()
                                    : nullptr;
}

bool ContextualTasksButton::IsSidePanelRightAligned() const {
  if (!browser_window_interface_ || !browser_window_interface_->GetProfile()) {
    return false;
  }
  PrefService* const pref_service =
      browser_window_interface_->GetProfile()->GetPrefs();
  if (!pref_service) {
    return false;
  }
  const base::DictValue& overrides =
      pref_service->GetDict(prefs::kSidePanelAlignmentOverrides);
  std::optional<bool> override_value = overrides.FindBool(
      SidePanelEntryIdToString(SidePanelEntryId::kContextualTasks));
  if (override_value.has_value()) {
    return *override_value;
  }
  return pref_service->GetBoolean(prefs::kSidePanelHorizontalAlignment);
}

void ContextualTasksButton::MaybeUpdateVisibility() {
  if (contextual_tasks::kShowEntryPoint.Get() !=
      contextual_tasks::EntryPointOption::kToolbarEphemeralBranded) {
    return;
  }

  const bool is_button_eligible =
      contextual_tasks::IsContextualTasksUIEnabled();

  ContextualTasksEphemeralButtonController* const controller =
      ContextualTasksEphemeralButtonController::From(browser_window_interface_);
  CHECK(controller);

  const bool matches_dock =
      contextual_tasks::IsContextualTasksPinningV2Enabled() ||
      (contextual_tasks::kEnableCircularEphemeralButtonNextToBatterySaver
           .Get() &&
       IsSidePanelRightAligned()) ||
      !IsTrailing() ||
      contextual_tasks::GetEnableRightHandContextualTasksEphemeralButton();

  const bool was_visible = GetVisible();
  const bool will_be_visible = matches_dock && is_button_eligible &&
                               controller->ShouldShowEphemeralButton();

  if (!was_visible && will_be_visible) {
    if (layer()) {
      layer()->SetOpacity(0.0f);  // Silence the flash before it becomes visible
    }
    ClearDropShadow();
    SetVisible(true);
    AnimateShow();
    shown_time_ticks_ = base::TimeTicks::Now();
    base::RecordAction(base::UserMetricsAction(
        "ContextualTasks.EphemeralToolbarButton.Shown"));
    base::UmaHistogramBoolean("ContextualTasks.EphemeralToolbarButton.Shown",
                              true);
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
      if (!drop_shadow_painted_layer_) {
        UpdateDropShadow();
      }
      MaybeToggleFeaturePromo();
    }
    SetVisible(will_be_visible);
  }
  was_panel_open_ = controller->IsPanelOpen();
  UpdateHighlight();
  UpdateTooltipText();
}

void ContextualTasksButton::UpdateHighlight() {
  if (!contextual_tasks::IsContextualTasksPinningV2Enabled()) {
    anchor_highlight_.reset();
    return;
  }
  ContextualTasksEphemeralButtonController* const ephemeral_controller =
      ContextualTasksEphemeralButtonController::From(browser_window_interface_);
  auto* const panel_controller =
      contextual_tasks::ContextualTasksPanelController::From(
          browser_window_interface_);
  const bool should_highlight =
      GetVisible() && ephemeral_controller &&
      ephemeral_controller->IsPanelOpen() && panel_controller &&
      panel_controller->IsPanelOpenForContextualTask();
  if (should_highlight) {
    if (!anchor_highlight_.has_value()) {
      anchor_highlight_ = AddAnchorHighlight();
    }
  } else {
    anchor_highlight_.reset();
  }
}

void ContextualTasksButton::UpdateTooltipText() {
  ContextualTasksEphemeralButtonController* const ephemeral_controller =
      ContextualTasksEphemeralButtonController::From(browser_window_interface_);
  auto* const panel_controller =
      contextual_tasks::ContextualTasksPanelController::From(
          browser_window_interface_);
  const bool is_panel_open =
      ephemeral_controller ? (ephemeral_controller->IsPanelOpen() &&
                              panel_controller &&
                              panel_controller->IsPanelOpenForContextualTask())
                           : (panel_controller &&
                              panel_controller->IsPanelOpenForContextualTask());

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

  auto contextual_tasks_button_background_painter =
      std::make_unique<ContextualTasksButtonBackgroundPainter>(
          color_provider->GetColor(kColorToolbar),
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

ui::ImageModel ContextualTasksButton::GetButtonImage() {
  if (contextual_tasks::ShouldUseSparkIconForPinButtons()) {
    return ui::ImageModel::FromVectorIcon(
        features::IsRoundedIconsEnabled() ? omnibox::kSearchSparkIcon
                                          : omnibox::kSearchSparkOldIcon,
        ui::kColorIcon,
        GetShape() == Shape::kCircle ? GetIconSize()
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
  return ui::ImageModel::FromVectorIcon(contextual_tasks_icon, ui::kColorIcon,
                                        GetShape() == Shape::kCircle
                                            ? GetIconSize()
                                            : kGLogoPillShapeIconSize);
}

BEGIN_METADATA(ContextualTasksButton)
END_METADATA
