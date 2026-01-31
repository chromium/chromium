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
#include "base/memory/raw_ptr.h"
#include "base/metrics/histogram_functions.h"
#include "build/build_config.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/ui/color/chrome_color_id.h"
#include "chrome/browser/ui/omnibox/omnibox_controller.h"
#include "chrome/browser/ui/omnibox/omnibox_edit_model.h"
#include "chrome/browser/ui/omnibox/omnibox_next_features.h"
#include "chrome/browser/ui/omnibox/omnibox_popup_state_manager.h"
#include "chrome/browser/ui/views/location_bar/location_bar_view.h"
#include "chrome/browser/ui/views/omnibox/omnibox_popup_presenter.h"
#include "chrome/browser/ui/views/omnibox/omnibox_popup_presenter_base.h"
#include "chrome/browser/ui/views/omnibox/omnibox_popup_webui_content.h"
#include "chrome/browser/ui/views/omnibox/omnibox_result_view.h"
#include "chrome/browser/ui/views/omnibox/omnibox_row_view.h"
#include "chrome/browser/ui/views/omnibox/omnibox_view_views.h"
#include "chrome/browser/ui/views/omnibox/rounded_omnibox_results_frame.h"
#include "chrome/browser/ui/views/theme_copying_widget.h"
#include "chrome/browser/ui/webui/searchbox/webui_omnibox_handler.h"
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

OmniboxPopupViewWebUI::OmniboxPopupViewWebUI(OmniboxViewViews* omnibox_view,
                                             OmniboxController* controller,
                                             LocationBarView* location_bar_view)
    : OmniboxPopupView(controller),
      construction_time_(base::TimeTicks::Now()),
      omnibox_view_(omnibox_view),
      location_bar_view_(location_bar_view) {
  presenter_ =
      std::make_unique<OmniboxPopupPresenter>(location_bar_view, controller);
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
  const bool should_be_visible =
      controller()->popup_state_manager()->popup_state() !=
          OmniboxPopupState::kAim &&
      (!controller()->autocomplete_controller()->result().empty() ||
       controller()
           ->autocomplete_controller()
           ->result()
           .has_contextual_chips()) &&
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
    // Update the popup state manager that the classic popup is opening.
    controller()->popup_state_manager()->SetPopupState(
        OmniboxPopupState::kClassic);

    if (!was_visible) {
      if (!construction_time_.is_null()) {
        const base::TimeDelta delta =
            base::TimeTicks::Now() - construction_time_;
        construction_time_ = base::TimeTicks();
        base::UmaHistogramTimes(
            "Omnibox.Popup.WebUI.ConstructionToFirstShownDuration", delta);
      }
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

raw_ptr<OmniboxPopupViewWebUI>
OmniboxPopupViewWebUI::GetOmniboxPopupViewWebUI() {
  return this;
}
