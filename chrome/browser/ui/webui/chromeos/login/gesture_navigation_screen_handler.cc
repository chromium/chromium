// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/webui/chromeos/login/gesture_navigation_screen_handler.h"

#include "chrome/browser/ash/login/screens/gesture_navigation_screen.h"
#include "chrome/grit/generated_resources.h"
#include "components/login/localized_values_builder.h"

namespace chromeos {

constexpr StaticOobeScreenId GestureNavigationScreenView::kScreenId;

GestureNavigationScreenHandler::GestureNavigationScreenHandler(
    JSCallsContainer* js_calls_container)
    : BaseScreenHandler(kScreenId, js_calls_container) {
  set_user_acted_method_path("login.GestureNavigationScreen.userActed");
}

GestureNavigationScreenHandler::~GestureNavigationScreenHandler() = default;

void GestureNavigationScreenHandler::Show() {
  if (!page_is_ready()) {
    show_on_init_ = true;
    return;
  }

  ShowScreen(kScreenId);
}

void GestureNavigationScreenHandler::Bind(GestureNavigationScreen* screen) {
  screen_ = screen;
  BaseScreenHandler::SetBaseScreen(screen);
}

void GestureNavigationScreenHandler::Hide() {}

void GestureNavigationScreenHandler::DeclareLocalizedValues(
    ::login::LocalizedValuesBuilder* builder) {
  builder->Add("gestureNavigationIntroTitle",
               IDS_OOBE_GESTURE_NAVIGATION_INTRO_TITLE);
  builder->Add("gestureNavigationIntroNextButton",
               IDS_OOBE_GESTURE_NAVIGATION_INTRO_NEXT_BUTTON);
  builder->Add("gestureNavigationIntroGoHomeItem",
               IDS_OOBE_GESTURE_NAVIGATION_INTRO_GO_HOME);
  builder->Add("gestureNavigationIntroSwitchAppItem",
               IDS_OOBE_GESTURE_NAVIGATION_INTRO_SWITCH_APP);
  builder->Add("gestureNavigationIntroGoBackItem",
               IDS_OOBE_GESTURE_NAVIGATION_INTRO_GO_BACK);
  builder->Add("gestureNavigationHomeTitle",
               IDS_OOBE_GESTURE_NAVIGATION_HOME_TITLE);
  builder->Add("gestureNavigationHomeDescription",
               IDS_OOBE_GESTURE_NAVIGATION_HOME_DESCRIPTION);
  builder->Add("gestureNavigationBackTitle",
               IDS_OOBE_GESTURE_NAVIGATION_BACK_TITLE);
  builder->Add("gestureNavigationBackDescription",
               IDS_OOBE_GESTURE_NAVIGATION_BACK_DESCRIPTION);
  builder->Add("gestureNavigationOverviewTitle",
               IDS_OOBE_GESTURE_NAVIGATION_OVERVIEW_TITLE);
  builder->Add("gestureNavigationOverviewDescription",
               IDS_OOBE_GESTURE_NAVIGATION_OVERVIEW_DESCRIPTION);
}

void GestureNavigationScreenHandler::Initialize() {
  if (show_on_init_) {
    Show();
    show_on_init_ = false;
  }
}

void GestureNavigationScreenHandler::RegisterMessages() {
  AddCallback("handleGesturePageChange",
              &GestureNavigationScreenHandler::HandleGesturePageChange);
  BaseScreenHandler::RegisterMessages();
}

void GestureNavigationScreenHandler::HandleGesturePageChange(
    const std::string& new_page) {
  screen_->GesturePageChange(new_page);
}

}  // namespace chromeos
