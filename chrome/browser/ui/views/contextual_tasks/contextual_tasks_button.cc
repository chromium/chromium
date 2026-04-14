// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/views/contextual_tasks/contextual_tasks_button.h"

#include <string>

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
#include "chrome/browser/ui/views/contextual_tasks/contextual_tasks_ephemeral_button_controller.h"
#include "chrome/browser/ui/views/frame/browser_view.h"
#include "chrome/browser/ui/views/frame/immersive_mode_controller.h"
#include "chrome/browser/ui/views/toolbar/toolbar_button.h"
#include "chrome/browser/ui/views/toolbar/toolbar_ink_drop_util.h"
#include "chrome/common/pref_names.h"
#include "components/contextual_tasks/public/features.h"
#include "components/prefs/pref_member.h"
#include "components/prefs/pref_service.h"
#include "components/strings/grit/components_strings.h"
#include "components/vector_icons/vector_icons.h"
#include "third_party/omnibox_proto/chrome_aim_entry_point.pb.h"
#include "ui/actions/actions.h"
#include "ui/base/interaction/element_identifier.h"
#include "ui/base/l10n/l10n_util.h"
#include "ui/base/metadata/metadata_impl_macros.h"
#include "ui/gfx/canvas.h"
#include "ui/gfx/scoped_canvas.h"
#include "ui/gfx/skia_paint_util.h"
#include "ui/gfx/vector_icon_types.h"
#include "ui/views/accessibility/view_accessibility.h"
#include "ui/views/painter.h"
#include "ui/views/view.h"
#include "ui/views/view_class_properties.h"

DEFINE_CLASS_ELEMENT_IDENTIFIER_VALUE(ContextualTasksButton,
                                      kContextualTasksToolbarButton);

namespace {

const int kGLogoIconSize = 16;

// Alpha value for the button shadow (approx. 24% opacity).
constexpr int kShadowAlpha = 0x3d;

// Will correspond to a 28x28 shadow circle when the location bar height is 34.
const int kCircleShadowInset = 3;

class ContextualTasksButtonShadowPainter : public views::Painter {
 public:
  ContextualTasksButtonShadowPainter(SkColor bg_color, SkColor shadow_color)
      : bg_color_(bg_color), shadow_color_(shadow_color) {}
  ~ContextualTasksButtonShadowPainter() override = default;

  gfx::Size GetMinimumSize() const override { return gfx::Size(); }

  void Paint(gfx::Canvas* canvas, const gfx::Size& size) override {
    gfx::ScopedCanvas scoped_canvas(canvas);
    const float scale = canvas->UndoDeviceScaleFactor();

    gfx::ShadowValues shadow;
    constexpr int kOffset = 1;
    constexpr int kBlur = 3;
    shadow.emplace_back(gfx::Vector2d(0, kOffset), kBlur,
                        SkColorSetA(shadow_color_, kShadowAlpha));

    cc::PaintFlags flags;
    flags.setAntiAlias(true);
    flags.setLooper(gfx::CreateShadowDrawLooper(shadow));
    flags.setStyle(cc::PaintFlags::kFill_Style);
    flags.setColor(bg_color_);

    gfx::Rect inset_rect(size);
    inset_rect.Inset(gfx::Insets(kCircleShadowInset));
    gfx::RectF fill_rect(gfx::ScaleToEnclosingRect(inset_rect, scale));
    gfx::PointF center = fill_rect.CenterPoint();
    float scaled_radius =
        std::min(fill_rect.width(), fill_rect.height()) / 2.0f;
    canvas->DrawCircle(center, scaled_radius, flags);
  }

 private:
  const SkColor bg_color_;
  const SkColor shadow_color_;
};

}  // namespace

ContextualTasksButton::ContextualTasksButton(
    BrowserWindowInterface* browser_window_interface)
    : ToolbarButton(base::BindRepeating(&ContextualTasksButton::OnButtonPress,
                                        base::Unretained(this)),
                    nullptr,
                    nullptr),
      browser_window_interface_(browser_window_interface) {
  SetProperty(views::kElementIdentifierKey, kContextualTasksToolbarButton);
  const std::u16string button_tooltip =
      l10n_util::GetStringUTF16(IDS_CONTEXTUAL_TASKS_ENTRY_POINT_TOOLTIP);
  GetViewAccessibility().SetName(button_tooltip);
  SetTooltipText(button_tooltip);

  side_panel_alignment_.Init(
      prefs::kSidePanelHorizontalAlignment,
      browser_window_interface->GetProfile()->GetPrefs(),
      base::BindRepeating(&ContextualTasksButton::OnSidePanelAlignmentChanged,
                          base::Unretained(this)));

  if (contextual_tasks::kShowEntryPoint.Get() ==
      contextual_tasks::EntryPointOption::kToolbarPermanent) {
    pin_state_.Init(
        prefs::kPinContextualTaskButton,
        browser_window_interface->GetProfile()->GetPrefs(),
        base::BindRepeating(&ContextualTasksButton::OnPinStateChanged,
                            base::Unretained(this)));
  } else {
    CHECK(contextual_tasks::kShowEntryPoint.Get() ==
              contextual_tasks::EntryPointOption::kToolbarRevisit ||
          contextual_tasks::kShowEntryPoint.Get() ==
              contextual_tasks::EntryPointOption::kToolbarEphemeralBranded);
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

  OnSidePanelAlignmentChanged();
  MaybeUpdateVisibility();
}

ContextualTasksButton::~ContextualTasksButton() = default;

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

void ContextualTasksButton::OnPinStateChanged() {
  MaybeUpdateVisibility();
}

void ContextualTasksButton::OnSidePanelAlignmentChanged() {
  if (contextual_tasks::kShowEntryPoint.Get() ==
      contextual_tasks::EntryPointOption::kToolbarEphemeralBranded) {
    const int button_size =
        GetLayoutConstant(LayoutConstant::kLocationBarHeight);
    SetPreferredSize(gfx::Size(button_size, button_size));
    const gfx::Insets insets = gfx::Insets((button_size - kGLogoIconSize) / 2) +
                               *GetProperty(views::kInternalPaddingKey);
    SetLayoutInsets(insets);
    SetHorizontalAlignment(gfx::ALIGN_CENTER);

    const gfx::VectorIcon& contextual_tasks_icon =
#if BUILDFLAG(GOOGLE_CHROME_BRANDING)
        vector_icons::kGoogleGLogoIcon;
#else
        kBrowserLogoIcon;
#endif

    SetImageModel(views::Button::STATE_NORMAL,
                  ui::ImageModel::FromVectorIcon(
                      contextual_tasks_icon, ui::kColorIcon, kGLogoIconSize));
  } else {
    PrefService* const pref_service =
        browser_window_interface_->GetProfile()->GetPrefs();

    const gfx::VectorIcon& contextual_tasks_icon =
        pref_service->GetBoolean(prefs::kSidePanelHorizontalAlignment)
            ? kDockToRightSparkIcon
            : kDockToLeftSparkIcon;
    SetVectorIcon(contextual_tasks_icon);
  }
}

void ContextualTasksButton::UpdateColorsAndInsets() {
  ToolbarButton::UpdateColorsAndInsets();

  if (ShouldApplyCircularBackgroundShadow()) {
    const auto* color_provider = GetColorProvider();
    if (color_provider) {
      SetBackground(views::CreateBackgroundFromPainter(
          std::make_unique<ContextualTasksButtonShadowPainter>(
              color_provider->GetColor(ui::kColorButtonBackground),
              color_provider->GetColor(ui::kColorShadowBase))));
    }
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
  if (contextual_tasks::kShowEntryPoint.Get() !=
      contextual_tasks::EntryPointOption::kToolbarEphemeralBranded) {
    return false;
  }

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
      contextual_tasks::EntryPointOption::kToolbarPermanent) {
    SetVisible(is_button_eligible && pin_state_.GetValue());
  } else if (contextual_tasks::kShowEntryPoint.Get() ==
             contextual_tasks::EntryPointOption::kToolbarRevisit) {
    ContextualTasksEphemeralButtonController* const controller =
        ContextualTasksEphemeralButtonController::From(
            browser_window_interface_);
    SetVisible(is_button_eligible && controller->ShouldShowEphemeralButton());
  } else if (contextual_tasks::kShowEntryPoint.Get() ==
             contextual_tasks::EntryPointOption::kToolbarEphemeralBranded) {
    auto* panel_controller =
        contextual_tasks::ContextualTasksPanelController::From(
            browser_window_interface_);
    CHECK(panel_controller);
    ContextualTasksEphemeralButtonController* const controller =
        ContextualTasksEphemeralButtonController::From(
            browser_window_interface_);
    SetVisible(!panel_controller->IsPanelOpenForContextualTask() &&
               is_button_eligible && controller->ShouldShowEphemeralButton());
  }
}

BEGIN_METADATA(ContextualTasksButton)
END_METADATA
