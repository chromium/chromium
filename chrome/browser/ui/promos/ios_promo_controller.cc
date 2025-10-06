// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/promos/ios_promo_controller.h"

#include "base/functional/bind.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/browser/ui/browser_window.h"
#include "chrome/browser/ui/browser_window/public/browser_window_interface.h"
#include "chrome/browser/ui/promos/ios_promo_trigger_service.h"
#include "chrome/browser/ui/promos/ios_promo_trigger_service_factory.h"
#include "chrome/browser/ui/promos/ios_promos_utils.h"

DEFINE_USER_DATA(IOSPromoController);

IOSPromoController::IOSPromoController(Browser* browser)
    : browser_(browser),
      scoped_unowned_user_data_(browser->GetUnownedUserDataHost(), *this) {
  IOSPromoTriggerService* service =
      IOSPromoTriggerServiceFactory::GetForProfile(browser_->profile());
  if (service) {
    promo_trigger_subscription_ =
        service->RegisterPromoCallback(base::BindRepeating(
            &IOSPromoController::OnPromoTriggered, base::Unretained(this)));
  }
}

IOSPromoController::~IOSPromoController() = default;

// static
IOSPromoController* IOSPromoController::From(
    BrowserWindowInterface* browser_window_interface) {
  return Get(browser_window_interface->GetUnownedUserDataHost());
}

void IOSPromoController::OnPromoTriggered(IOSPromoType promo_type) {
  BrowserWindow* window = browser_->window();
  // Don't show the promo if the window is not active or the toolbar is not
  // visible.
  if (!window || !window->IsActive() || !window->IsToolbarVisible()) {
    return;
  }

  ios_promos_utils::VerifyIOSPromoEligibility(promo_type, browser_);
}
