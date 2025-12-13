// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/views/omnibox/omnibox_popup_closer.h"

#include "base/feature_list.h"
#include "base/logging.h"
#include "chrome/browser/ui/omnibox/omnibox_controller.h"
#include "chrome/browser/ui/view_ids.h"
#include "chrome/browser/ui/views/frame/browser_view.h"
#include "chrome/browser/ui/views/location_bar/location_bar_view.h"
#include "chrome/browser/ui/views/omnibox/omnibox_view_views.h"
#include "ui/views/view.h"

namespace omnibox {

namespace {

std::string CloseReasonToString(omnibox::PopupCloseReason reason) {
  switch (reason) {
    case omnibox::PopupCloseReason::kBlur:
      return "kBlur";
    case omnibox::PopupCloseReason::kBrowserWidgetMoved:
      return "kBrowserWidgetMoved";
    case omnibox::PopupCloseReason::kEscapeKeyPressed:
      return "kEscapeKeyPressed";
    case omnibox::PopupCloseReason::kLocationIconDragged:
      return "kLocationIconDragged";
    case omnibox::PopupCloseReason::kMouseClickOutside:
      return "kMouseClickOutside";
    case omnibox::PopupCloseReason::kRevertAll:
      return "kRevertAll";
    case omnibox::PopupCloseReason::kTextDrag:
      return "kTextDrag";
    case omnibox::PopupCloseReason::kOther:
      return "kOther";
  }
}

}  // namespace

OmniboxPopupCloser::OmniboxPopupCloser(BrowserView* browser_view)
    : browser_view_(browser_view) {
  // Observe UI events on `BrowserView`.
  browser_view_observation_.Observe(browser_view_);
}

OmniboxPopupCloser::~OmniboxPopupCloser() = default;

void OmniboxPopupCloser::OnMouseEvent(ui::MouseEvent* event) {
  // Close the omnibox popup if the click is outside the omnibox view.
  if (!browser_view_->browser()->is_delete_scheduled() &&
      event->type() == ui::EventType::kMousePressed) {
    LocationBarView* location_bar_view = browser_view_->GetLocationBarView();
    CHECK(location_bar_view);
    const auto* const view = static_cast<views::View*>(event->target());
    CHECK(view);
    if (!location_bar_view->Contains(view)) {
      CloseWithReason(PopupCloseReason::kMouseClickOutside);
    }
  }
}

void OmniboxPopupCloser::CloseWithReason(PopupCloseReason reason) {
  VLOG(1) << "Closing omnibox popup with reason: "
          << CloseReasonToString(reason);
  auto* location_bar_view = browser_view_->GetLocationBarView();
  // Clearing the autocomplete results closes the popup.
  location_bar_view->GetOmniboxController()->StopAutocomplete(
      /*clear_result=*/true);
  // Reset focus ring for the AIM button if it was set.
  location_bar_view->omnibox_view()->ApplyFocusRingToAimButton(false);
}

}  // namespace omnibox
