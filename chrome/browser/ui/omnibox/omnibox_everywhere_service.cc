// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/omnibox/omnibox_everywhere_service.h"

#include <memory>
#include <utility>

#include "base/functional/callback_helpers.h"
#include "build/build_config.h"
#include "chrome/browser/browser_process.h"
#include "chrome/browser/feature_engagement/tracker_factory.h"
#include "chrome/browser/global_features.h"
#include "chrome/browser/profiles/keep_alive/profile_keep_alive_types.h"
#include "chrome/browser/profiles/keep_alive/scoped_profile_keep_alive.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/ui/browser_window/public/browser_window_interface.h"
#include "chrome/browser/ui/browser_window/public/create_browser_window.h"
#include "chrome/browser/ui/browser_window/public/profile_browser_collection.h"
#include "chrome/browser/ui/navigator/browser_navigator.h"
#include "chrome/browser/ui/navigator/browser_navigator_params.h"
#include "chrome/browser/ui/omnibox/omnibox_everywhere/omnibox_everywhere_controller.h"
#include "chrome/browser/ui/omnibox/omnibox_everywhere/omnibox_everywhere_feature_promo_controller.h"
#include "chrome/browser/ui/omnibox/omnibox_everywhere/omnibox_everywhere_prefs.h"
#include "chrome/browser/ui/omnibox/omnibox_everywhere/omnibox_everywhere_ui_manager.h"
#include "chrome/browser/ui/tabs/tab_strip_model.h"
#include "chrome/browser/user_education/user_education_service.h"
#include "chrome/browser/user_education/user_education_service_factory.h"
#include "components/feature_engagement/public/feature_constants.h"
#include "components/user_education/common/feature_promo/feature_promo_controller.h"
#include "content/public/browser/navigation_handle.h"
#include "content/public/browser/web_contents.h"
#include "third_party/skia/include/core/SkBitmap.h"
#include "ui/base/base_window.h"
#include "ui/base/page_transition_types.h"
#include "ui/base/window_open_disposition.h"
#include "ui/display/display.h"
#include "ui/display/screen.h"
#include "ui/gfx/geometry/rect.h"
#include "ui/views/widget/widget.h"

#if BUILDFLAG(IS_MAC)
#include "chrome/browser/ui/omnibox/omnibox_everywhere/mac_window_util.h"
#endif

namespace {

void ActivateAndFocusBrowserWindow(BrowserWindowInterface* bwi) {
  if (!bwi) {
    return;
  }
#if BUILDFLAG(IS_MAC)
  // On macOS, when navigating from Loomnibox (an auxiliary overlay window on
  // a fullscreen Space) to a browser window that may reside on another Space,
  // explicit application activation and window ordering is required to switch
  // Mission Control Spaces to Chrome.
  omnibox_everywhere::ActivateBrowserWindowOnMac(bwi);
#else
  if (bwi->GetWindow()) {
    if (bwi->GetWindow()->IsMinimized()) {
      bwi->GetWindow()->Restore();
    }
    bwi->GetWindow()->Show();
    bwi->GetWindow()->Activate();
  }
#endif
  if (auto* tab_strip = bwi->GetTabStripModel()) {
    if (auto* web_contents = tab_strip->GetActiveWebContents()) {
      web_contents->Focus();
    }
  }
}

}  // namespace

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
  EndLensPromo();
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
         !controller()->ui_manager()->is_demoted() &&
         controller()->ui_manager()->profile() == profile_;
}

void OmniboxEverywhereService::MaybeShowLensPromo() {
  if (feature_promo_controller_) {
    feature_promo_controller_->MaybeShowPromo(
        user_education::FeaturePromoParams(
            feature_engagement::kIPHOmniboxEverywhereLensPromoFeature));
  }
}

void OmniboxEverywhereService::EndLensPromo() {
  if (feature_promo_controller_) {
    feature_promo_controller_->EndPromo(
        feature_engagement::kIPHOmniboxEverywhereLensPromoFeature,
        user_education::EndFeaturePromoReason::kAbortPromo);
  }
}

void OmniboxEverywhereService::OnLensSearchClicked() {
  if (feature_promo_controller_) {
    feature_promo_controller_->NotifyFeatureUsedIfValid(
        feature_engagement::kIPHOmniboxEverywhereLensPromoFeature);
    feature_promo_controller_->EndPromo(
        feature_engagement::kIPHOmniboxEverywhereLensPromoFeature,
        user_education::EndFeaturePromoReason::kFeatureEngaged);
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
  if (ui_manager()) {
    ui_manager()->OnScreensharePickerOpened();
  }
}

void OmniboxEverywhereService::OnScreensharePickerClosed() {
  if (ui_manager()) {
    ui_manager()->OnScreensharePickerClosed();
  }
}

void OmniboxEverywhereService::ShowScreenshotDisclosureDialog(
    base::OnceClosure on_accepted,
    base::OnceClosure on_cancelled) {
  if (ui_manager()) {
    ui_manager()->ShowScreenshotDisclosureDialog(
        base::BindOnce(
            &OmniboxEverywhereService::OnScreenshotDisclosureAccepted,
            weak_factory_.GetWeakPtr(), std::move(on_accepted)),
        std::move(on_cancelled));
    return;
  }
  if (on_cancelled) {
    std::move(on_cancelled).Run();
  }
}

void OmniboxEverywhereService::OnScreenshotDisclosureAccepted(
    base::OnceClosure on_accepted) {
  omnibox_everywhere::prefs::SetScreenshotDisclosureAccepted(profile_, true);
  if (on_accepted) {
    std::move(on_accepted).Run();
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

void OmniboxEverywhereService::OnHotkeyDropdownOpened() {
  if (ui_manager()) {
    ui_manager()->OnHotkeyDropdownOpened();
  }
}

void OmniboxEverywhereService::OnHotkeyDropdownClosed() {
  if (ui_manager()) {
    ui_manager()->OnHotkeyDropdownClosed();
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
  NavigateInBrowserWindow(bwi, url, disposition, transition,
                          std::move(navigation_handle_callback));
}

void OmniboxEverywhereService::NavigateInBrowserWindow(
    BrowserWindowInterface* bwi,
    const GURL& url,
    WindowOpenDisposition disposition,
    ui::PageTransition transition,
    base::OnceCallback<void(content::NavigationHandle&)>
        navigation_handle_callback) {
  NavigateParams params = bwi ? NavigateParams(bwi, url, transition)
                              : NavigateParams(profile_, url, transition);
  params.should_trigger_session_restore = false;

  if (!bwi) {
    params.disposition = WindowOpenDisposition::NEW_WINDOW;
  } else {
    params.disposition = (disposition == WindowOpenDisposition::CURRENT_TAB)
                             ? WindowOpenDisposition::NEW_FOREGROUND_TAB
                             : disposition;
  }
  params.window_action = NavigateParams::WindowAction::kShowWindow;

  base::WeakPtr<content::NavigationHandle> handle = Navigate(&params);
  if (handle && navigation_handle_callback) {
    std::move(navigation_handle_callback).Run(*handle);
  }

  // Dismiss the Omnibox Everywhere popup before activating the target browser
  // window. Otherwise, dismissing the popup after activation causes AppKit on
  // macOS (and other window managers) to return focus to the previously active
  // application (e.g. a fullscreen app over which Loomnibox was displayed).
  HidePopup();

  ActivateAndFocusBrowserWindow(params.browser);
}

// static
gfx::Rect OmniboxEverywhereService::CalculateInPlaceWindowBounds(
    const gfx::Rect& popup_bounds,
    const gfx::Rect& work_area) {
  int x = popup_bounds.x() + (popup_bounds.width() - kInPlaceWindowWidth) / 2;
  int y = popup_bounds.y();
  gfx::Rect bounds(x, y, kInPlaceWindowWidth, kInPlaceWindowHeight);
  bounds.AdjustToFit(work_area);
  return bounds;
}

gfx::Rect OmniboxEverywhereService::GetInPlaceWindowBounds() const {
  const views::Widget* widget = ui_manager() ? ui_manager()->widget() : nullptr;
  auto* screen = display::Screen::Get();
  if (!widget || !screen) {
    return gfx::Rect(0, 0, kInPlaceWindowWidth, kInPlaceWindowHeight);
  }
  const gfx::Rect popup_bounds = widget->GetWindowBoundsInScreen();
  const gfx::Rect work_area =
      screen->GetDisplayMatching(popup_bounds).work_area();
  if (work_area.IsEmpty()) {
    return gfx::Rect(popup_bounds.x(), popup_bounds.y(), kInPlaceWindowWidth,
                     kInPlaceWindowHeight);
  }
  return CalculateInPlaceWindowBounds(popup_bounds, work_area);
}

void OmniboxEverywhereService::OpenUrlWithInPlaceWindow(
    const GURL& url,
    ui::PageTransition transition,
    base::OnceCallback<void(content::NavigationHandle&)>
        navigation_handle_callback) {
  BrowserWindowInterface* bwi = nullptr;
  if (GetBrowserWindowCreationStatusForProfile(*profile_) ==
      BrowserWindowInterface::CreationStatus::kOk) {
    BrowserWindowCreateParams browser_params(profile_,
                                             /*from_user_gesture=*/true);
    browser_params.should_trigger_session_restore = false;
    browser_params.initial_bounds = GetInPlaceWindowBounds();
    browser_params.initial_origin_specified =
        BrowserWindowCreateParams::ValueSpecified::kSpecified;
    bwi = CreateBrowserWindow(std::move(browser_params));
  }

  NavigateInBrowserWindow(bwi, url, WindowOpenDisposition::NEW_FOREGROUND_TAB,
                          transition, std::move(navigation_handle_callback));
}
