// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/fullscreen/browser_window_fullscreen_controller.h"

#include "base/types/to_address.h"
#include "chrome/browser/ui/browser_element_identifiers.h"
#include "chrome/browser/ui/browser_window/public/browser_window_interface.h"
#include "chrome/browser/ui/immersive/immersive_mode_controller.h"
#include "chrome/browser/ui/interaction/browser_elements.h"
#include "chrome/browser/ui/views/frame/browser_frame_view.h"
#include "chrome/browser/ui/views/interaction/browser_elements_views.h"
#include "ui/views/interaction/element_tracker_views.h"
#include "ui/views/view_utils.h"
#include "ui/views/widget/widget.h"

DEFINE_USER_DATA(BrowserWindowFullscreenController);

namespace {

const BrowserFrameView* GetBrowserFrameView(BrowserWindowInterface* browser) {
  auto* browser_elements = BrowserElementsViews::From(browser);
  if (!browser_elements || !browser_elements->GetContext()) {
    return nullptr;
  }
  return browser_elements->GetViewAs<BrowserFrameView>(
      kBrowserFrameElementId, /*require_visible=*/false);
}

}  // namespace

BrowserWindowFullscreenController::BrowserWindowFullscreenController(
    BrowserWindowInterface& browser)
    : browser_(browser),
      scoped_data_holder_(browser.GetUnownedUserDataHost(), *this) {}

BrowserWindowFullscreenController::~BrowserWindowFullscreenController() =
    default;

// static
BrowserWindowFullscreenController* BrowserWindowFullscreenController::From(
    BrowserWindowInterface* browser) {
  return browser
             ? ui::ScopedUnownedUserData<BrowserWindowFullscreenController>::
                   Get(browser->GetUnownedUserDataHost())
             : nullptr;
}

// static
const BrowserWindowFullscreenController*
BrowserWindowFullscreenController::From(const BrowserWindowInterface* browser) {
  return browser
             ? ui::ScopedUnownedUserData<BrowserWindowFullscreenController>::
                   Get(browser->GetUnownedUserDataHost())
             : nullptr;
}

bool BrowserWindowFullscreenController::ShouldHideUIForFullscreen() const {
  if (should_hide_ui_for_fullscreen_for_testing_.has_value()) {
    return should_hide_ui_for_fullscreen_for_testing_.value();
  }

  // Immersive mode needs UI for the slide-down top panel.
  BrowserWindowInterface* browser = base::to_address(browser_);
  if (auto* immersive_mode_controller = ImmersiveModeController::From(browser);
      immersive_mode_controller && immersive_mode_controller->IsEnabled()) {
    // Do not hide UI for immersive mode.
    return false;
  }

  const BrowserFrameView* browser_frame_view = GetBrowserFrameView(browser);
  if (!browser_frame_view) {
    // Do not hide UI by default.
    return false;
  }

  CHECK(browser_frame_view->GetWidget());
  return browser_frame_view->GetWidget()->IsFullscreen() &&
         browser_frame_view->ShouldHideTopUIInFullscreen();
}

bool BrowserWindowFullscreenController::IsForceFullscreen() const {
  return force_fullscreen_;
}

void BrowserWindowFullscreenController::SetForceFullscreen(
    bool force_fullscreen) {
  force_fullscreen_ = force_fullscreen;
}
