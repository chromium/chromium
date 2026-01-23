// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/views/omnibox/omnibox_popup_aim_presenter.h"

#include <optional>

#include "chrome/browser/ui/omnibox/omnibox_controller.h"
#include "chrome/browser/ui/omnibox/omnibox_popup_state_manager.h"
#include "chrome/browser/ui/views/location_bar/location_bar_view.h"
#include "chrome/browser/ui/views/omnibox/omnibox_aim_popup_webui_content.h"
#include "content/public/browser/browser_accessibility_state.h"
#include "ui/accessibility/ax_mode.h"

OmniboxPopupAimPresenter::OmniboxPopupAimPresenter(
    LocationBarView* location_bar_view,
    OmniboxController* controller)
    : OmniboxPopupPresenterBase(location_bar_view), controller_(controller) {
  SetWebUIContent(std::make_unique<OmniboxAimPopupWebUIContent>(
      this, this->location_bar_view(), controller));
}

OmniboxPopupAimPresenter::~OmniboxPopupAimPresenter() = default;

void OmniboxPopupAimPresenter::Show() {
  OmniboxPopupPresenterBase::Show();
  if (GetWidget() && !widget_observation_.IsObserving()) {
    widget_observation_.Observe(GetWidget());
  }
}

void OmniboxPopupAimPresenter::Hide() {
  // If UI devtools settings allows closing the view
  // (`ShouldHandleNativeWidgetActivationChanged()`), refocus the location bar
  // for screen readers to prevent unreliable focus tracking.
  if (GetWidget() &&
      GetWidget()->ShouldHandleNativeWidgetActivationChanged(false) &&
      GetWidget()->IsActive()) {
    const bool is_screen_reader_enabled =
        content::BrowserAccessibilityState::GetInstance()
            ->GetAccessibilityMode()
            .has_mode(ui::AXMode::kScreenReader);
    if (is_screen_reader_enabled) {
      location_bar_view()->FocusLocation(true);
    }
  }
  widget_observation_.Reset();
  OmniboxPopupPresenterBase::Hide();
}

void OmniboxPopupAimPresenter::OnWidgetActivationChanged(views::Widget* widget,
                                                         bool active) {
  // This method is called when the focus is transferred to or from this widget.
  // If a user clicks outside the popup, we will hide the popup.
  //
  // Separately, if a user opens a context menu inside this popup. The context
  // menu is a child widget so this popup widget is still considered active. We
  // will not hide the popup.
  if (!active && controller_->popup_state_manager()->popup_state() ==
                     OmniboxPopupState::kAim &&
                    !location_bar_view()->in_popup_state_transition()) {
    controller_->popup_state_manager()->SetPopupState(OmniboxPopupState::kNone);
  }
}

void OmniboxPopupAimPresenter::WidgetDestroyed() {
  // Update the popup state manager if widget was destroyed externally, e.g., by
  // the OS. This ensures the popup state manager stays in sync.
  if (controller_->popup_state_manager()->popup_state() ==
      OmniboxPopupState::kAim) {
    controller_->popup_state_manager()->SetPopupState(OmniboxPopupState::kNone);
  }
}
