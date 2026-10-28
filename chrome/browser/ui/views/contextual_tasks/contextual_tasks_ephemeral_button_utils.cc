// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/views/contextual_tasks/contextual_tasks_ephemeral_button_utils.h"

#include <optional>
#include <utility>

#include "base/check.h"
#include "base/feature_list.h"
#include "base/functional/callback.h"
#include "base/i18n/rtl.h"
#include "base/metrics/histogram_functions.h"
#include "base/metrics/user_metrics.h"
#include "base/metrics/user_metrics_action.h"
#include "base/values.h"
#include "chrome/browser/contextual_tasks/contextual_tasks_panel_controller.h"
#include "chrome/browser/contextual_tasks/contextual_tasks_utils.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/ui/browser_window/public/browser_window_interface.h"
#include "chrome/browser/ui/immersive/immersive_mode_controller.h"
#include "chrome/browser/ui/side_panel/side_panel_entry_id.h"
#include "chrome/browser/ui/tabs/vertical_tab_strip_state_controller.h"
#include "chrome/browser/ui/user_education/browser_user_education_interface.h"
#include "chrome/common/pref_names.h"
#include "components/contextual_tasks/public/features.h"
#include "components/feature_engagement/public/feature_constants.h"
#include "components/prefs/pref_service.h"
#include "third_party/omnibox_proto/chrome_aim_entry_point.pb.h"

namespace contextual_tasks {

bool IsContextualTasksSidePanelRightAligned(BrowserWindowInterface* browser) {
  if (!browser || !browser->GetProfile()) {
    return false;
  }
  PrefService* const pref_service = browser->GetProfile()->GetPrefs();
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

bool IsEphemeralButtonTrailing(BrowserWindowInterface* browser) {
  return IsContextualTasksSidePanelRightAligned(browser) != base::i18n::IsRTL();
}

bool ShouldPlaceCircularButtonBeforeProfile(BrowserWindowInterface* browser) {
  return IsContextualTasksPinningV2Enabled() ||
         (kEnableCircularEphemeralButtonNextToBatterySaver.Get() &&
          IsContextualTasksSidePanelRightAligned(browser));
}

EphemeralButtonShape GetEphemeralButtonShape(BrowserWindowInterface* browser) {
  ImmersiveModeController* const immersive_mode_controller =
      ImmersiveModeController::From(browser);
  if (immersive_mode_controller && immersive_mode_controller->IsEnabled()) {
    return EphemeralButtonShape::kCircle;
  }

  auto* const vertical_tabs_controller =
      tabs::VerticalTabStripStateController::From(browser);
  if (vertical_tabs_controller &&
      vertical_tabs_controller->ShouldDisplayVerticalTabs()) {
    return EphemeralButtonShape::kCircle;
  }

  if (ShouldPlaceCircularButtonBeforeProfile(browser)) {
    return EphemeralButtonShape::kCircle;
  }

  return IsContextualTasksSidePanelRightAligned(browser)
             ? EphemeralButtonShape::kFlatEdgeRight
             : EphemeralButtonShape::kFlatEdgeLeft;
}

void RecordEphemeralButtonShown(bool is_panel_open) {
  base::RecordAction(
      base::UserMetricsAction("ContextualTasks.EphemeralToolbarButton.Shown"));
  base::UmaHistogramBoolean("ContextualTasks.EphemeralToolbarButton.Shown",
                            true);
  if (is_panel_open) {
    base::RecordAction(base::UserMetricsAction(
        "ContextualTasks.EphemeralToolbarButton.Shown.ActivePanelOpen"));
    base::UmaHistogramBoolean(
        "ContextualTasks.EphemeralToolbarButton.Shown.ActivePanelOpen", true);
  } else {
    base::RecordAction(base::UserMetricsAction(
        "ContextualTasks.EphemeralToolbarButton.Shown.ActivePanelCollapsed"));
    base::UmaHistogramBoolean(
        "ContextualTasks.EphemeralToolbarButton.Shown.ActivePanelCollapsed",
        true);
  }
}

void HandleEphemeralButtonPressed(BrowserWindowInterface* browser,
                                  base::OnceClosure on_ephemeral_open) {
  if (auto* const user_ed = BrowserUserEducationInterface::From(browser);
      user_ed && user_ed->IsFeaturePromoActive(
                     feature_engagement::
                         kIPHContextualTasksEphemeralToolbarButtonFeature)) {
    user_ed->NotifyFeaturePromoFeatureUsed(
        feature_engagement::kIPHContextualTasksEphemeralToolbarButtonFeature,
        FeaturePromoFeatureUsedAction::kClosePromoIfPresent);
  }

  auto* const controller = ContextualTasksPanelController::From(browser);
  CHECK(controller);

  // When `kEphemeralPinningVisibleWhenPermanentlyPinned` is enabled, the
  // ephemeral button remains visible alongside the pinned button, and presses
  // on this button should always be logged as EphemeralToolbarButton actions.
  const bool is_pinned = !base::FeatureList::IsEnabled(
                             kEphemeralPinningVisibleWhenPermanentlyPinned) &&
                         GetEffectivePinState(browser->GetProfile());

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
    return;
  }

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

  if (!is_pinned && on_ephemeral_open) {
    std::move(on_ephemeral_open).Run();
  }

  if (kShowEntryPoint.Get() == EntryPointOption::kToolbarEphemeralBranded) {
    controller->Show(
        /*transition_from_tab=*/false,
        is_pinned ? omnibox::ChromeAimEntryPoint::
                        DESKTOP_CHROME_COBROWSE_PINNED_TOOLBAR_BUTTON
                  : omnibox::ChromeAimEntryPoint::UNKNOWN_AIM_ENTRY_POINT,
        /*use_no_animation=*/false, /*open_time_ticks=*/std::nullopt,
        is_pinned
            ? std::nullopt
            : std::make_optional(ContextualTasksPanelController::EntrySource::
                                     kEphemeralToolbarButton));
  } else {
    controller->OpenInZeroState();
  }
}

}  // namespace contextual_tasks
