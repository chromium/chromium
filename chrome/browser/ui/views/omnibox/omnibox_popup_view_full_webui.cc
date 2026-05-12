// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/views/omnibox/omnibox_popup_view_full_webui.h"

#include "base/metrics/histogram_functions.h"
#include "base/time/time.h"
#include "chrome/browser/ui/location_bar/location_bar.h"
#include "chrome/browser/ui/omnibox/omnibox_controller.h"
#include "chrome/browser/ui/omnibox/omnibox_edit_model.h"
#include "chrome/browser/ui/omnibox/omnibox_popup_state_manager.h"
#include "chrome/browser/ui/omnibox/omnibox_view.h"
#include "chrome/browser/ui/views/omnibox/omnibox_full_popup_webui_content.h"
#include "chrome/browser/ui/views/omnibox/omnibox_popup_full_presenter.h"
#include "chrome/browser/ui/views/omnibox/omnibox_popup_presenter_base.h"
#include "chrome/browser/ui/views/omnibox/omnibox_popup_presenter_delegate.h"
#include "chrome/browser/ui/views/omnibox/omnibox_popup_view_webui.h"
#include "chrome/browser/ui/webui/searchbox/webui_omnibox_handler.h"
#include "chrome/browser/ui/webui/top_chrome/webui_contents_preload_manager.h"
#include "chrome/browser/ui/webui/top_chrome/webui_contents_wrapper.h"

OmniboxPopupViewFullWebUI::OmniboxPopupViewFullWebUI(
    OmniboxView* omnibox_view,
    OmniboxController* controller,
    LocationBar* location_bar,
    OmniboxPopupPresenterDelegate& presenter_delegate)
    : OmniboxPopupViewWebUI(
          omnibox_view,
          controller,
          location_bar,
          presenter_delegate,
          std::make_unique<OmniboxPopupFullPresenter>(location_bar,
                                                      presenter_delegate,
                                                      controller)) {}

OmniboxPopupViewFullWebUI::~OmniboxPopupViewFullWebUI() = default;

void OmniboxPopupViewFullWebUI::UpdatePopupAppearance() {
  const bool should_be_visible =
      controller()->popup_state_manager()->popup_state() !=
          OmniboxPopupState::kAim &&
      controller()->edit_model()->has_focus() &&
      !omnibox_view_->IsImeShowingPopup();

  if (!should_be_visible) {
    presenter()->Hide();
    // Update the popup state manager that the full popup is closing.
    // Do this AFTER native widget operations. LocationBarView is subscribed
    // to popup state changes and attempts to call `UpdatePopupAppearance()`
    // again if the widget is open.
    // Only update the popup state if it's currently kFull. If it's already
    // transitioning to another state (e.g., kAim), don't override it.
    if (controller()->popup_state_manager()->popup_state() ==
        OmniboxPopupState::kFull) {
      controller()->popup_state_manager()->SetPopupState(
          OmniboxPopupState::kNone);
    }
  } else {
    const bool was_visible = IsOpen();

    presenter()->Show();
    if (!was_visible) {
      // Set the request time to now when the popup is first shown. This ensures
      // that latency is measured from the user interaction to show, even if the
      // WebUI was preloaded at startup.
      WebUIContentsPreloadManager::GetInstance()->SetRequestTime(
          presenter()->GetWebUIContent()->GetWebContents(),
          base::TimeTicks::Now());
    }
    // Update the popup state manager to reflect the appropriate "opened" state.
    controller()->popup_state_manager()->SetPopupState(
        OmniboxPopupState::kFull);

    if (!was_visible) {
      if (!construction_time_.is_null()) {
        const base::TimeDelta delta =
            base::TimeTicks::Now() - construction_time_;
        construction_time_ = base::TimeTicks();
        base::UmaHistogramTimes(
            base::StrCat({presenter()->GetPopupMetricPrefix(),
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
