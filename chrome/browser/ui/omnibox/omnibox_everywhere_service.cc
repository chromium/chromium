// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/omnibox/omnibox_everywhere_service.h"

#include <memory>
#include <vector>

#include "base/feature_list.h"
#include "build/build_config.h"
#include "chrome/browser/browser_process.h"
#include "chrome/browser/feature_engagement/tracker_factory.h"
#include "chrome/browser/global_features.h"
#include "chrome/browser/profiles/keep_alive/profile_keep_alive_types.h"
#include "chrome/browser/profiles/keep_alive/scoped_profile_keep_alive.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/ui/browser_commands.h"
#include "chrome/browser/ui/browser_window/public/browser_window_interface.h"
#include "chrome/browser/ui/browser_window/public/profile_browser_collection.h"
#include "chrome/browser/ui/navigator/browser_navigator.h"
#include "chrome/browser/ui/navigator/browser_navigator_params.h"
#include "chrome/browser/ui/omnibox/omnibox_everywhere/omnibox_everywhere_controller.h"
#include "chrome/browser/ui/omnibox/omnibox_everywhere/omnibox_everywhere_feature_promo_controller.h"
#include "chrome/browser/ui/omnibox/omnibox_everywhere/omnibox_everywhere_ui_manager.h"
#include "chrome/browser/ui/omnibox/omnibox_everywhere_service_factory.h"
#include "chrome/browser/ui/omnibox/omnibox_next_features.h"
#include "chrome/browser/user_education/user_education_service.h"
#include "chrome/browser/user_education/user_education_service_factory.h"
#include "components/feature_engagement/public/feature_constants.h"
#include "components/user_education/common/feature_promo/feature_promo_controller.h"
#include "content/public/browser/navigation_handle.h"
#include "third_party/skia/include/core/SkBitmap.h"

OmniboxEverywhereService::OmniboxEverywhereService(Profile* profile)
    : profile_(profile) {
  feature_engagement::Tracker* const tracker_service =
      feature_engagement::TrackerFactory::GetForBrowserContext(profile_);
  UserEducationService* const user_education_service =
      UserEducationServiceFactory::GetForBrowserContext(profile_);
  if (tracker_service && user_education_service) {
    feature_promo_controller_ = std::make_unique<
        omnibox_everywhere::OmniboxEverywhereFeaturePromoController>(
        tracker_service, user_education_service, weak_factory_.GetWeakPtr());
    feature_promo_controller_->Init();
  }
}

OmniboxEverywhereService::~OmniboxEverywhereService() {
  Shutdown();
}

user_education::FeaturePromoController*
OmniboxEverywhereService::feature_promo_controller() {
  return feature_promo_controller_.get();
}

const user_education::FeaturePromoController*
OmniboxEverywhereService::feature_promo_controller() const {
  return feature_promo_controller_.get();
}

omnibox_everywhere::OmniboxEverywhereController*
OmniboxEverywhereService::controller() const {
  return g_browser_process && g_browser_process->GetFeatures()
             ? g_browser_process->GetFeatures()->omnibox_everywhere_controller()
             : nullptr;
}

omnibox_everywhere::OmniboxEverywhereUIManager*
OmniboxEverywhereService::ui_manager() const {
  return controller() ? controller()->ui_manager() : nullptr;
}

bool OmniboxEverywhereService::AcquireProfileKeepAlive() {
  CHECK(profile_ && !profile_->IsOffTheRecord());

  if (profile_keep_alive_) {
    return true;
  }
  profile_keep_alive_ = ScopedProfileKeepAlive::TryAcquire(
      profile_, ProfileKeepAliveOrigin::kOmniboxEverywhere);
  return profile_keep_alive_ != nullptr;
}

void OmniboxEverywhereService::ReleaseProfileKeepAlive() {
  profile_keep_alive_.reset();
  if (feature_promo_controller_) {
    feature_promo_controller_->EndPromo(
        feature_engagement::kIPHOmniboxEverywhereLensPromoFeature,
        user_education::EndFeaturePromoReason::kAbortPromo);
  }
}

void OmniboxEverywhereService::Shutdown() {
  ReleaseProfileKeepAlive();
  feature_promo_controller_.reset();
  if (controller()) {
    controller()->ShutdownForProfile(profile_);
  }
}

void OmniboxEverywhereService::HidePopup() {
  if (controller()) {
    controller()->Hide();
  }
}

bool OmniboxEverywhereService::IsPopupVisible() const {
  return controller() && controller()->IsVisible();
}

bool OmniboxEverywhereService::IsPopupVisibleForProfile() const {
  return controller() && controller()->ui_manager() &&
         controller()->ui_manager()->IsVisible() &&
         controller()->ui_manager()->profile() == profile_;
}

void OmniboxEverywhereService::MaybeShowLensPromo() {
  if (feature_promo_controller_) {
    feature_promo_controller_->MaybeShowPromo(
        user_education::FeaturePromoParams(
            feature_engagement::kIPHOmniboxEverywhereLensPromoFeature));
  }
}

void OmniboxEverywhereService::ShowProfilePicker() {
  if (controller()) {
    controller()->ShowProfilePicker();
  }
}

void OmniboxEverywhereService::OnDrivePickerOpened() {
  if (ui_manager()) {
    ui_manager()->OnDrivePickerOpened();
  }
}

void OmniboxEverywhereService::OnDrivePickerClosed() {
  if (ui_manager()) {
    ui_manager()->OnDrivePickerClosed();
  }
}

void OmniboxEverywhereService::OnScreensharePickerOpened() {
  if (feature_promo_controller_) {
    feature_promo_controller_->NotifyFeatureUsedIfValid(
        feature_engagement::kIPHOmniboxEverywhereLensPromoFeature);
    feature_promo_controller_->EndPromo(
        feature_engagement::kIPHOmniboxEverywhereLensPromoFeature,
        user_education::EndFeaturePromoReason::kFeatureEngaged);
  }
  if (ui_manager()) {
    ui_manager()->OnScreensharePickerOpened();
  }
}

void OmniboxEverywhereService::OnScreensharePickerClosed() {
  if (ui_manager()) {
    ui_manager()->OnScreensharePickerClosed();
  }
}

void OmniboxEverywhereService::ShowRegionSelectOverlay(
    const SkBitmap& screenshot,
    const RegionCaptureSource& source,
    RegionSelectedCallback callback) {
  if (ui_manager()) {
    ui_manager()->ShowRegionSelectOverlay(screenshot, source,
                                          std::move(callback));
    return;
  }
  std::move(callback).Run(SkBitmap());
}

void OmniboxEverywhereService::OnFileChooserOpened() {
  if (ui_manager()) {
    ui_manager()->OnFileChooserOpened();
  }
}

void OmniboxEverywhereService::OnFileChooserClosed() {
  if (ui_manager()) {
    ui_manager()->OnFileChooserClosed();
  }
}

void OmniboxEverywhereService::OpenUrl(const GURL& url,
                                       WindowOpenDisposition disposition,
                                       ui::PageTransition transition) {
  OpenUrl(url, disposition, transition, base::NullCallback());
}

void OmniboxEverywhereService::OpenUrl(
    const GURL& url,
    WindowOpenDisposition disposition,
    ui::PageTransition transition,
    base::OnceCallback<void(content::NavigationHandle&)>
        navigation_handle_callback) {
  auto* browser_collection = ProfileBrowserCollection::GetForProfile(profile_);
  CHECK(browser_collection);
  BrowserWindowInterface* bwi = browser_collection->GetLastActiveBrowser();
  bool is_new_window = false;
  if (!bwi) {
    bwi = chrome::OpenEmptyWindow(profile_);
    is_new_window = true;
  }

  if (bwi) {
    NavigateParams params(bwi, url, transition);
    params.disposition =
        is_new_window ? WindowOpenDisposition::CURRENT_TAB
                      : ((disposition == WindowOpenDisposition::CURRENT_TAB)
                             ? WindowOpenDisposition::NEW_FOREGROUND_TAB
                             : disposition);
    params.window_action = NavigateParams::WindowAction::kShowWindow;
    base::WeakPtr<content::NavigationHandle> handle = Navigate(&params);
    if (handle && navigation_handle_callback) {
      std::move(navigation_handle_callback).Run(*handle);
    }
  }

  HidePopup();
}
