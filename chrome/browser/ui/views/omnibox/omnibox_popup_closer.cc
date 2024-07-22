// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/views/omnibox/omnibox_popup_closer.h"

#include "base/feature_list.h"
#include "chrome/browser/ui/ui_features.h"
#include "chrome/browser/ui/view_ids.h"
#include "chrome/browser/ui/views/frame/browser_view.h"
#include "chrome/browser/ui/views/location_bar/location_bar_view.h"
#include "chrome/browser/ui/views/omnibox/omnibox_popup_view_views.h"
#include "chrome/browser/ui/views/omnibox/omnibox_view_views.h"
#include "components/omnibox/browser/omnibox_edit_model.h"
#include "ui/views/view.h"

namespace ui {

OmniboxPopupCloser::OmniboxPopupCloser(BrowserView* browser_view)
    : browser_view_(browser_view) {
  if (base::FeatureList::IsEnabled(
          features::kCloseOmniboxPopupOnInactiveAreaClick)) {
    observer_.Observe(browser_view_);
  }
}

OmniboxPopupCloser::~OmniboxPopupCloser() = default;

void OmniboxPopupCloser::OnMouseEvent(ui::MouseEvent* event) {
  if (!browser_view_->browser()->is_delete_scheduled() &&
      event->type() == ui::EventType::kMousePressed) {
    LocationBarView* location_bar_view = browser_view_->GetLocationBarView();
    CHECK(location_bar_view);
    const auto* const view = static_cast<views::View*>(event->target());
    CHECK(view);
    if (!location_bar_view->Contains(view)) {
      location_bar_view->omnibox_view()->CloseOmniboxPopup();
    }
  }
}

}  // namespace ui
