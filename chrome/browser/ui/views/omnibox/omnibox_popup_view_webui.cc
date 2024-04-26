// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/views/omnibox/omnibox_popup_view_webui.h"

#include <memory>
#include <numeric>
#include <optional>

#include "base/auto_reset.h"
#include "base/feature_list.h"
#include "base/functional/bind.h"
#include "base/metrics/histogram_functions.h"
#include "build/build_config.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/ui/color/chrome_color_id.h"
#include "chrome/browser/ui/views/location_bar/location_bar_view.h"
#include "chrome/browser/ui/views/omnibox/omnibox_popup_presenter.h"
#include "chrome/browser/ui/views/omnibox/omnibox_result_view.h"
#include "chrome/browser/ui/views/omnibox/omnibox_row_view.h"
#include "chrome/browser/ui/views/omnibox/omnibox_view_views.h"
#include "chrome/browser/ui/views/omnibox/rounded_omnibox_results_frame.h"
#include "chrome/browser/ui/views/theme_copying_widget.h"
#include "chrome/browser/ui/views/user_education/browser_feature_promo_controller.h"
#include "chrome/browser/ui/webui/searchbox/realbox_handler.h"
#include "components/omnibox/browser/omnibox_controller.h"
#include "components/omnibox/browser/omnibox_edit_model.h"
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
      location_bar_view_(location_bar_view),
      presenter_(std::make_unique<OmniboxPopupPresenter>(location_bar_view,
                                                         controller)) {
  model()->set_popup_view(this);
}

OmniboxPopupViewWebUI::~OmniboxPopupViewWebUI() {
  model()->set_popup_view(nullptr);
}

bool OmniboxPopupViewWebUI::IsOpen() const {
  return presenter_->IsShown();
}

void OmniboxPopupViewWebUI::InvalidateLine(size_t line) {}

void OmniboxPopupViewWebUI::OnSelectionChanged(
    OmniboxPopupSelection old_selection,
    OmniboxPopupSelection new_selection) {
  if (RealboxHandler* handler = presenter_->GetHandler()) {
    handler->UpdateSelection(old_selection, new_selection);
  }
}

void OmniboxPopupViewWebUI::UpdatePopupAppearance() {
  // Measure time since construction just once.
  if (!construction_time_.is_null()) {
    base::TimeDelta delta = base::TimeTicks::Now() - construction_time_;
    construction_time_ = base::TimeTicks();
    base::UmaHistogramTimes("Omnibox.WebUI.FirstUpdate", delta);
  }

  if (controller()->autocomplete_controller()->result().empty() ||
      omnibox_view_->IsImeShowingPopup()) {
    presenter_->Hide();
  } else {
    const bool was_visible = presenter_->IsShown();
    presenter_->Show();
    if (!was_visible) {
      NotifyOpenListeners();
    }
  }
}

void OmniboxPopupViewWebUI::ProvideButtonFocusHint(size_t line) {
  // TODO(crbug.com/40062053): Not implemented for WebUI omnibox popup yet.
}

void OmniboxPopupViewWebUI::OnMatchIconUpdated(size_t match_index) {
  // TODO(crbug.com/40062053): Not implemented for WebUI omnibox popup yet.
}

void OmniboxPopupViewWebUI::OnDragCanceled() {}

void OmniboxPopupViewWebUI::GetPopupAccessibleNodeData(
    ui::AXNodeData* node_data) {}

void OmniboxPopupViewWebUI::AddPopupAccessibleNodeData(
    ui::AXNodeData* node_data) {
  // TODO(crbug.com/40062053): Not implemented for WebUI omnibox popup yet.
}

std::u16string OmniboxPopupViewWebUI::GetAccessibleButtonTextForResult(
    size_t line) {
  return u"";
}
