// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/webui/ash/login/gesture_navigation_screen_handler.h"

#include "chrome/browser/ash/login/screens/gesture_navigation_screen.h"
#include "chrome/grit/generated_resources.h"
#include "components/login/localized_values_builder.h"

namespace ash {

GestureNavigationScreenHandler::GestureNavigationScreenHandler()
    : BaseScreenHandler(kScreenId) {}

GestureNavigationScreenHandler::~GestureNavigationScreenHandler() = default;

void GestureNavigationScreenHandler::Show() {
  ShowInWebUI();
}

base::WeakPtr<GestureNavigationScreenView>
GestureNavigationScreenHandler::AsWeakPtr() {
  return weak_ptr_factory_.GetWeakPtr();
}

void GestureNavigationScreenHandler::DeclareLocalizedValues(
    ::login::LocalizedValuesBuilder* builder) {
  builder->Add("gestureNavigationIntroTitle",
               IDS_OOBE_GESTURE_NAVIGATION_INTRO_TITLE);
  builder->Add("gestureNavigationIntroNextButton",
               IDS_OOBE_GESTURE_NAVIGATION_INTRO_NEXT_BUTTON);
  builder->Add("gestureNavigationIntroSkipButton",
               IDS_OOBE_GESTURE_NAVIGATION_INTRO_SKIP);
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

}  // namespace ash
