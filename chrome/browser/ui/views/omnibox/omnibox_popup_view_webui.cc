// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/views/omnibox/omnibox_popup_view_webui.h"

#include <memory>
#include <numeric>
#include <optional>
#include <string_view>

#include "base/feature_list.h"
#include "base/functional/bind.h"
#include "base/metrics/histogram_functions.h"
#include "base/strings/strcat.h"
#include "build/build_config.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/ui/color/chrome_color_id.h"
#include "chrome/browser/ui/omnibox/omnibox_controller.h"
#include "chrome/browser/ui/omnibox/omnibox_edit_model.h"
#include "chrome/browser/ui/omnibox/omnibox_next_features.h"
#include "chrome/browser/ui/omnibox/omnibox_popup_state_manager.h"
#include "chrome/browser/ui/views/location_bar/location_bar_view.h"
#include "chrome/browser/ui/views/omnibox/omnibox_popup_full_presenter.h"
#include "chrome/browser/ui/views/omnibox/omnibox_popup_presenter.h"
#include "chrome/browser/ui/views/omnibox/omnibox_popup_presenter_base.h"
#include "chrome/browser/ui/views/omnibox/omnibox_popup_webui_content.h"
#include "chrome/browser/ui/views/omnibox/omnibox_result_view.h"
#include "chrome/browser/ui/views/omnibox/omnibox_row_view.h"
#include "chrome/browser/ui/views/omnibox/omnibox_view_views.h"
#include "chrome/browser/ui/views/omnibox/rounded_omnibox_results_frame.h"
#include "chrome/browser/ui/views/theme_copying_widget.h"
#include "chrome/browser/ui/webui/searchbox/webui_omnibox_handler.h"
#include "chrome/browser/ui/webui/top_chrome/webui_contents_preload_manager.h"
#include "components/omnibox/browser/omnibox_prefs.h"
#include "components/omnibox/common/omnibox_features.h"
#include "ui/accessibility/ax_node_data.h"
#include "ui/base/metadata/metadata_impl_macros.h"
#include "ui/compositor/closure_animation_observer.h"
#include "ui/compositor/layer.h"
#include "ui/compositor/scoped_layer_animation_settings.h"
#include "ui/gfx/image/image.h"
#include "ui/views/accessibility/view_accessibility.h"
#include "ui/views/cascading_property.h"
#include "ui/views/layout/box_layout.h"
#include "ui/views/views_features.h"
#include "ui/views/widget/widget.h"

OmniboxPopupViewWebUI::OmniboxPopupViewWebUI(
    OmniboxView* omnibox_view,
    OmniboxController* controller,
    LocationBar* location_bar,
    OmniboxPopupPresenterDelegate& presenter_delegate)
    : OmniboxPopupView(controller),
      construction_time_(base::TimeTicks::Now()),
      omnibox_view_(omnibox_view),
      location_bar_(location_bar) {
  if (base::FeatureList::IsEnabled(omnibox::kWebUIOmniboxFullPopupV2)) {
    presenter_ = std::make_unique<OmniboxPopupFullPresenter>(
        location_bar, presenter_delegate, controller);
  } else {
    presenter_ = std::make_unique<OmniboxPopupPresenter>(
        location_bar, presenter_delegate, controller);
  }
  controller->edit_model()->set_popup_view(this);
  edit_model_observation_.Observe(controller->edit_model());
}

OmniboxPopupViewWebUI::~OmniboxPopupViewWebUI() {
  controller()->edit_model()->set_popup_view(nullptr);
}

bool OmniboxPopupViewWebUI::IsOpen() const {
  return presenter_->IsShown();
}

void OmniboxPopupViewWebUI::InvalidateLine(size_t line) {}

void OmniboxPopupViewWebUI::UpdatePopupAppearance() {
  const bool has_results =
      !controller()->autocomplete_controller()->result().empty();
  // TODO(crbug.com/498556249): Consolidate/correct chip visibility logic.
  // As written the code below is a bit misleading as the actual decision of
  // whether or not to show chips is made in WebUI Typescript. This manifests as
  // a bug where the popup will be visible if the user types something and
  // backspaces when chips are enabled but no chips are actually shown due to
  // the Typescript logic.
  const bool has_contextual_chips =
      controller()->autocomplete_controller()->result().has_contextual_chips();
  const bool contextual_chips_feature_enabled =
      omnibox::IsAimPopupEnabled(location_bar_->GetProfile()) &&
      (omnibox::kShowRecentTabChip.Get() || omnibox::kShowLensSearchChip.Get());
  const bool has_results_or_chips =
      has_results || (contextual_chips_feature_enabled && has_contextual_chips);
  const bool should_be_visible =
      controller()->popup_state_manager()->popup_state() !=
          OmniboxPopupState::kAim &&
      (has_results_or_chips || (omnibox::IsWebUIOmniboxFullPopupEnabled() &&
                                controller()->edit_model()->has_focus())) &&
      !omnibox_view_->IsImeShowingPopup();

  if (!should_be_visible) {
    presenter_->Hide();
    // Update the popup state manager that the classic popup is closing.
    // Do this AFTER widget operations. LocationBarView is subscribed to state
    // changes and attempts to call `UpdatePopupAppearance()` again if the
    // widget is open.
    // Only update the state if it's currently kClassic/kFull. If it's already
    // transitioning to another state (e.g., kAim), don't override it.
    if (controller()->popup_state_manager()->popup_state() ==
            OmniboxPopupState::kClassic ||
        controller()->popup_state_manager()->popup_state() ==
            OmniboxPopupState::kFull) {
      controller()->popup_state_manager()->SetPopupState(
          OmniboxPopupState::kNone);
    }
  } else {
    const bool was_visible = IsOpen();

    presenter_->Show();
    if (!was_visible) {
      // Set the request time to now when the popup is first shown. This ensures
      // that latency is measured from the user interaction to show, even if the
      // WebUI was preloaded at startup.
      WebUIContentsPreloadManager::GetInstance()->SetRequestTime(
          presenter_->GetWebUIContent()->GetWebContents(),
          base::TimeTicks::Now());
    }
    // Update the popup state manager to reflect the appropriate "opened" state.
    auto new_state =
        base::FeatureList::IsEnabled(omnibox::kWebUIOmniboxFullPopupV2)
            ? OmniboxPopupState::kFull
            : OmniboxPopupState::kClassic;
    controller()->popup_state_manager()->SetPopupState(new_state);

    if (!was_visible) {
      if (!construction_time_.is_null()) {
        const base::TimeDelta delta =
            base::TimeTicks::Now() - construction_time_;
        construction_time_ = base::TimeTicks();
        base::UmaHistogramTimes(
            base::StrCat({presenter_->GetPopupMetricPrefix(),
                          ".ConstructionToFirstShownDuration"}),
            delta);
      }
    }

    auto* controller = presenter()
                           ->GetWebUIContent()
                           ->contents_wrapper()
                           ->GetWebUIController();
    if (auto* handler = controller ? controller->omnibox_handler() : nullptr) {
      handler->SetAimButtonVisible(omnibox_view_->AimButtonVisible());
    }
  }
}

void OmniboxPopupViewWebUI::OnContentsChanged() {
  UpdatePopupAppearance();
}

void OmniboxPopupViewWebUI::ProvideButtonFocusHint(size_t line) {
  // TODO(crbug.com/40062053): Not implemented for WebUI omnibox popup yet.
}

void OmniboxPopupViewWebUI::OnDragCanceled() {}

void OmniboxPopupViewWebUI::GetPopupAccessibleNodeData(
    ui::AXNodeData* node_data) const {}

void OmniboxPopupViewWebUI::StepSelection(
    OmniboxPopupSelection::Direction direction,
    OmniboxPopupSelection::Step step) {
  auto* controller =
      presenter()->GetWebUIContent()->contents_wrapper()->GetWebUIController();
  if (auto* handler = controller ? controller->omnibox_handler() : nullptr) {
    handler->SetAimButtonVisible(omnibox_view_->AimButtonVisible());
    handler->StepSelection(direction, step);
  }
}

void OmniboxPopupViewWebUI::OpenCurrentSelection(
    WindowOpenDisposition disposition) {
  auto* controller =
      presenter()->GetWebUIContent()->contents_wrapper()->GetWebUIController();
  if (auto* handler = controller ? controller->omnibox_handler() : nullptr) {
    handler->OpenCurrentSelection(disposition);
  }
}

bool OmniboxPopupViewWebUI::IsSelectionPopupControlled() const {
  return base::FeatureList::IsEnabled(
             omnibox::kWebUIOmniboxPopupSelectionControl) &&
         IsOpen();
}
