// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/views/omnibox/omnibox_popup_view_webui.h"

#include <memory>

#include "base/feature_list.h"
#include "base/metrics/histogram_functions.h"
#include "base/strings/strcat.h"
#include "build/build_config.h"
#include "chrome/browser/ui/omnibox/omnibox_controller.h"
#include "chrome/browser/ui/omnibox/omnibox_edit_model.h"
#include "chrome/browser/ui/omnibox/omnibox_next_features.h"
#include "chrome/browser/ui/omnibox/omnibox_popup_state_manager.h"
#include "chrome/browser/ui/views/location_bar/location_bar_view.h"
#include "chrome/browser/ui/views/omnibox/omnibox_popup_presenter.h"
#include "chrome/browser/ui/views/omnibox/omnibox_popup_presenter_base.h"
#include "chrome/browser/ui/views/omnibox/omnibox_popup_webui_content.h"
#include "chrome/browser/ui/views/omnibox/rounded_omnibox_results_frame.h"
#include "chrome/browser/ui/webui/omnibox_popup/omnibox_popup_ui.h"
#include "chrome/browser/ui/webui/searchbox/webui_omnibox_handler.h"
#include "chrome/browser/ui/webui/top_chrome/webui_contents_preload_manager.h"
#include "components/omnibox/common/omnibox_features.h"
#include "ui/accessibility/ax_node_data.h"
#include "ui/base/window_open_disposition.h"
#include "ui/views/widget/widget.h"

OmniboxPopupViewWebUI::OmniboxPopupViewWebUI(
    OmniboxView* omnibox_view,
    OmniboxController* controller,
    LocationBar* location_bar,
    OmniboxPopupPresenterDelegate& presenter_delegate)
    : OmniboxPopupViewWebUI(
          omnibox_view,
          controller,
          location_bar,
          presenter_delegate,
          std::make_unique<OmniboxPopupPresenter>(location_bar,
                                                  presenter_delegate,
                                                  controller)) {}

OmniboxPopupViewWebUI::OmniboxPopupViewWebUI(
    OmniboxView* omnibox_view,
    OmniboxController* controller,
    LocationBar* location_bar,
    OmniboxPopupPresenterDelegate& presenter_delegate,
    std::unique_ptr<OmniboxPopupPresenterBase> presenter)
    : OmniboxPopupView(controller),
      omnibox_view_(omnibox_view),
      location_bar_(location_bar),
      presenter_(std::move(presenter)) {
  controller->edit_model()->set_popup_view(this);
  edit_model_observation_.Observe(controller->edit_model());
}

OmniboxPopupViewWebUI::~OmniboxPopupViewWebUI() {
  controller()->edit_model()->set_popup_view(nullptr);
}

bool OmniboxPopupViewWebUI::IsOpen() const {
  return presenter_->IsShown();
}

OmniboxPopupPresenterBase* OmniboxPopupViewWebUI::presenter() {
  return presenter_.get();
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
      (omnibox::kShowLensSearchChip.Get() ||
       omnibox::kAskGShowChip.Get());
  const bool has_results_or_chips =
      has_results || (contextual_chips_feature_enabled && has_contextual_chips);
  const bool should_be_visible =
      controller()->popup_state_manager()->popup_state() !=
          OmniboxPopupState::kAim &&
      (has_results_or_chips ||
       (base::FeatureList::IsEnabled(omnibox::kWebUIOmniboxFullPopup) &&
        controller()->edit_model()->has_focus())) &&
      !omnibox_view_->IsImeShowingPopup();

  if (!should_be_visible) {
    presenter_->Hide();
    // Update the popup state manager that the classic popup is closing.
    // Do this AFTER widget operations. LocationBarView is subscribed to state
    // changes and attempts to call `UpdatePopupAppearance()` again if the
    // widget is open.
    // Only update the state if it's currently kClassic. If it's already
    // transitioning to another state (e.g., kAim), don't override it.
    if (controller()->popup_state_manager()->popup_state() ==
        OmniboxPopupState::kClassic) {
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
    controller()->popup_state_manager()->SetPopupState(
        OmniboxPopupState::kClassic);

    if (!was_visible) {
      if (!logged_first_shown_metric_) {
        const base::TimeDelta delta =
            base::TimeTicks::Now() - construction_time();
        logged_first_shown_metric_ = true;
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
    if (!omnibox::ShouldUseWebUIOmniboxFullHandler()) {
      // Full webui omnibox avoids this code path by intentionally excluding
      // the edit model. Use of downcast here seems better than complicating
      // base classes with methods only relevant to the classic omnibox.
      static_cast<WebuiOmniboxHandler*>(handler)->StepSelection(direction,
                                                                step);
    }
  }
}

void OmniboxPopupViewWebUI::OpenCurrentSelection(
    WindowOpenDisposition disposition) {
  auto* controller =
      presenter()->GetWebUIContent()->contents_wrapper()->GetWebUIController();
  if (auto* handler = controller ? controller->omnibox_handler() : nullptr) {
    if (!omnibox::ShouldUseWebUIOmniboxFullHandler()) {
      // Full webui omnibox avoids this code path by intentionally excluding
      // the edit model. Use of downcast here seems better than complicating
      // base classes with methods only relevant to the classic omnibox.
      static_cast<WebuiOmniboxHandler*>(handler)->OpenCurrentSelection(
          disposition);
    }
  }
}

bool OmniboxPopupViewWebUI::IsSelectionPopupControlled() const {
  return base::FeatureList::IsEnabled(
             omnibox::kWebUIOmniboxPopupSelectionControl) &&
         IsOpen();
}
