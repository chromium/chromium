// Copyright 2013 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/views/frame/immersive_mode_controller.h"

#include "base/observer_list.h"
#include "chrome/browser/ui/browser_window/public/browser_window_interface.h"
#include "ui/base/unowned_user_data/scoped_unowned_user_data.h"

DEFINE_USER_DATA(ImmersiveModeController);

// static
ImmersiveModeController* ImmersiveModeController::From(
    BrowserWindowInterface* browser) {
  return Get(browser->GetUnownedUserDataHost());
}
// static
const ImmersiveModeController* ImmersiveModeController::From(
    const BrowserWindowInterface* browser) {
  return Get(browser->GetUnownedUserDataHost());
}

ImmersiveModeController::ImmersiveModeController(
    BrowserWindowInterface* browser)
    : scoped_unowned_user_data_(browser->GetUnownedUserDataHost(), *this) {}

ImmersiveModeController::~ImmersiveModeController() {
  observers_.Notify(&Observer::OnImmersiveModeControllerDestroyed);
}

void ImmersiveModeController::AddObserver(Observer* observer) {
  observers_.AddObserver(observer);
}

void ImmersiveModeController::RemoveObserver(Observer* observer) {
  observers_.RemoveObserver(observer);
}
