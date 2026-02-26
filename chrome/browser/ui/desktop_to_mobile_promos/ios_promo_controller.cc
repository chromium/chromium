// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/desktop_to_mobile_promos/ios_promo_controller.h"

#include "base/functional/bind.h"
#include "chrome/browser/browser_process.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/browser/ui/browser_window.h"
#include "chrome/browser/ui/browser_window/public/browser_window_interface.h"
#include "chrome/browser/ui/desktop_to_mobile_promos/ios_promo_trigger_service.h"
#include "chrome/browser/ui/desktop_to_mobile_promos/ios_promo_trigger_service_factory.h"
#include "chrome/browser/ui/desktop_to_mobile_promos/ios_promos_utils.h"
#include "chrome/browser/ui/user_education/browser_user_education_interface.h"
#include "chrome/browser/ui/views/frame/browser_view.h"
#include "chrome/common/pref_names.h"
#include "components/desktop_to_mobile_promos/features.h"
#include "components/desktop_to_mobile_promos/promos_types.h"
#include "components/feature_engagement/public/feature_constants.h"
#include "components/prefs/pref_service.h"
#include "components/sync_device_info/device_info.h"
#include "ui/views/widget/widget.h"

using desktop_to_mobile_promos::PromoType;

namespace {

// Returns the feature associated with the given PromoType.
const base::Feature& FeatureForIOSPromoType(PromoType promo_type) {
  switch (promo_type) {
    case PromoType::kPassword:
      return feature_engagement::kIPHiOSPasswordPromoDesktopFeature;
    case PromoType::kAddress:
      return feature_engagement::kIPHiOSAddressPromoDesktopFeature;
    case PromoType::kPayment:
      return feature_engagement::kIPHiOSPaymentPromoDesktopFeature;
    case PromoType::kEnhancedBrowsing:
      return feature_engagement::kIPHiOSEnhancedBrowsingDesktopFeature;
    case PromoType::kLens:
      return feature_engagement::kIPHiOSLensPromoDesktopFeature;
    case PromoType::kTabGroups:
      return feature_engagement::kIPHiOSTabGroupsDesktopFeature;
    case PromoType::kPriceTracking:
      return feature_engagement::kIPHiOSPriceTrackingDesktopFeature;
  }
}

}  // namespace

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

bool IOSPromoController::CanShowIOSPromo(PromoType promo_type) {
  // TODO(crbug.com/485862381): Remove once check is integrated with user
  // education system.
  PrefService* local_state = g_browser_process->local_state();
  if (local_state && !local_state->GetBoolean(prefs::kPromotionsEnabled)) {
    return false;
  }

  BrowserWindow* window = browser_->window();
  // Don't show the promo if the toolbar is not visible.
  if (!window || !window->IsToolbarVisible()) {
    return false;
  }

  // Do not show the promo if the window is not active.
  BrowserView* browser_view = BrowserView::GetBrowserViewForBrowser(browser_);
  if (!browser_view || !browser_view->GetWidget()->ShouldPaintAsActive()) {
    return false;
  }

  if (GetMobilePromoOnDesktopForcePromoType() ==
          IOSPromoBubbleForceType::kNoOverride &&
      !IsUserEligibleForPromo(promo_type)) {
    return false;
  }

  return true;
}

void IOSPromoController::OnPromoTriggered(PromoType promo_type) {
  if (!CanShowIOSPromo(promo_type)) {
    return;
  }

  auto* user_education_interface =
      BrowserUserEducationInterface::From(browser_);
  if (user_education_interface) {
    user_education_interface->MaybeShowFeaturePromo(
        FeatureForIOSPromoType(promo_type));
  }
}

// Returns true if the promo should be shown based on user eligibility criteria.
// Only show the promo if the user meets one of the two conditions:
// 1. kMobilePromoOnDesktop is enabled AND has a "not very active" iOS device
// 2. kMobilePromoOnDesktopWithQRCode is enabled AND does not have Chrome
// installed on any iOS device
bool IOSPromoController::IsUserEligibleForPromo(PromoType promo_type) {
  // Don't show the promo if the user has a recent active Android device.
  if (ios_promos_utils::IsUserActiveOnAndroid(browser_->profile())) {
    return false;
  }

  MobilePromoOnDesktopPromoType feature_type;
  switch (promo_type) {
    case PromoType::kPassword:
      feature_type = MobilePromoOnDesktopPromoType::kAutofillPromo;
      break;
    case PromoType::kEnhancedBrowsing:
      feature_type = MobilePromoOnDesktopPromoType::kESBPromo;
      break;
    case PromoType::kLens:
      feature_type = MobilePromoOnDesktopPromoType::kLensPromo;
      break;
    case PromoType::kTabGroups:
      feature_type = MobilePromoOnDesktopPromoType::kTabGroups;
      break;
    case PromoType::kPriceTracking:
      feature_type = MobilePromoOnDesktopPromoType::kPriceTracking;
      break;
    case PromoType::kAddress:
    case PromoType::kPayment:
      // These promos are not yet supported in this flow.
      return false;
  }

  IOSPromoTriggerService* service =
      IOSPromoTriggerServiceFactory::GetForProfile(browser_->profile());
  if (!service) {
    return false;
  }
  const syncer::DeviceInfo* device = service->GetIOSDeviceToRemind();

  // Check if user is eligible for Reminder type promo.
  if (device && !ios_promos_utils::IsUserActiveOnIOS(browser_->profile()) &&
      device->desktop_to_ios_promo_receiving_enabled() &&
      MobilePromoOnDesktopTypeEnabled(
          feature_type, desktop_to_mobile_promos::BubbleType::kReminder)) {
    return true;
  }

  // Check if user is eligible for QRCode type promo.
  if (!device &&
      MobilePromoOnDesktopTypeEnabled(
          feature_type, desktop_to_mobile_promos::BubbleType::kQRCode)) {
    return true;
  }

  return false;
}
