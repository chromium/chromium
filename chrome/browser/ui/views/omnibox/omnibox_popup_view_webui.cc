// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/views/omnibox/omnibox_popup_view_webui.h"

#include <memory>
#include <numeric>

#include "base/auto_reset.h"
#include "base/feature_list.h"
#include "base/functional/bind.h"
#include "build/build_config.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/ui/color/chrome_color_id.h"
#include "chrome/browser/ui/views/location_bar/location_bar_view.h"
#include "chrome/browser/ui/views/omnibox/omnibox_result_view.h"
#include "chrome/browser/ui/views/omnibox/omnibox_row_view.h"
#include "chrome/browser/ui/views/omnibox/omnibox_view_views.h"
#include "chrome/browser/ui/views/omnibox/rounded_omnibox_results_frame.h"
#include "chrome/browser/ui/views/omnibox/webui_omnibox_popup_view.h"
#include "chrome/browser/ui/views/theme_copying_widget.h"
#include "chrome/browser/ui/views/user_education/browser_feature_promo_controller.h"
#include "components/omnibox/browser/omnibox_edit_model.h"
#include "components/omnibox/browser/omnibox_prefs.h"
#include "components/omnibox/common/omnibox_features.h"
#include "third_party/abseil-cpp/absl/types/optional.h"
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
                                             OmniboxEditModel* edit_model,
                                             LocationBarView* location_bar_view)
    : OmniboxPopupViewViews(omnibox_view, edit_model, location_bar_view),
      webui_view_(nullptr) {}

void OmniboxPopupViewWebUI::OnSelectionChanged(
    OmniboxPopupSelection old_selection,
    OmniboxPopupSelection new_selection) {
  if (webui_view_) {
    webui_view_->OnSelectedLineChanged(old_selection.line, new_selection.line);
  }
}

void OmniboxPopupViewWebUI::ProvideButtonFocusHint(size_t line) {
  // TODO(crbug.com/1396174): Not implemented for WebUI omnibox popup yet.
}

void OmniboxPopupViewWebUI::OnMatchIconUpdated(size_t match_index) {
  // TODO(crbug.com/1396174): Not implemented for WebUI omnibox popup yet.
}

void OmniboxPopupViewWebUI::AddPopupAccessibleNodeData(
    ui::AXNodeData* node_data) {
  // TODO(crbug.com/1396174): Not implemented for WebUI omnibox popup yet.
}

bool OmniboxPopupViewWebUI::OnMouseDragged(const ui::MouseEvent& event) {
  // TODO(crbug.com/1396174): Not implemented for WebUI omnibox popup yet.
  return true;
}

void OmniboxPopupViewWebUI::UpdateChildViews() {
  if (!webui_view_) {
    webui_view_ = AddChildView(std::make_unique<WebUIOmniboxPopupView>(
        location_bar_view()->profile()));
  }
}

void OmniboxPopupViewWebUI::OnPopupCreated() {}

gfx::Rect OmniboxPopupViewWebUI::GetTargetBounds() const {
  int popup_height = 0;

  if (webui_view_) {
    popup_height = webui_view_->GetPreferredSize().height();
  }

  // Add enough space on the top and bottom so it looks like there is the same
  // amount of space between the text and the popup border as there is in the
  // interior between each row of text.
  popup_height += RoundedOmniboxResultsFrame::GetNonResultSectionHeight();

  // Add 8dp at the bottom for aesthetic reasons. https://crbug.com/1076646
  // It's expected that this space is dead unclickable/unhighlightable space.
  constexpr int kExtraBottomPadding = 8;
  popup_height += kExtraBottomPadding;

  // The rounded popup is always offset the same amount from the omnibox.
  gfx::Rect content_rect = location_bar_view()->GetBoundsInScreen();
  content_rect.Inset(
      -RoundedOmniboxResultsFrame::GetLocationBarAlignmentInsets());
  content_rect.set_height(popup_height);

  // Finally, expand the widget to accommodate the custom-drawn shadows.
  content_rect.Inset(-RoundedOmniboxResultsFrame::GetShadowInsets());
  return content_rect;
}
