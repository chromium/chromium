// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/browser_window_theme_observer/browser_window_theme_observer.h"

#include <utility>

#include "base/check.h"
#include "chrome/browser/themes/theme_service.h"
#include "chrome/browser/themes/theme_service_factory.h"
#include "chrome/browser/ui/browser_window.h"
#include "chrome/browser/ui/browser_window/public/browser_window_interface.h"
#include "chrome/browser/ui/ui_features.h"
#include "ui/native_theme/native_theme.h"

DEFINE_USER_DATA(BrowserWindowThemeObserver);

BrowserWindowThemeObserver::BrowserWindowThemeObserver(
    BrowserWindowInterface* browser)
    : scoped_unowned_user_data_(browser->GetUnownedUserDataHost(), *this) {
  ThemeService* theme_service =
      ThemeServiceFactory::GetForProfile(browser->GetProfile());
  CHECK(theme_service);
  theme_service_observation_.Observe(theme_service);

  // Native theme bridging is gated so that BrowserView is not notified twice
  // (once via BrowserWidget's own ui::NativeThemeObserver and once here) until
  // BrowserWidget has been migrated onto this observer.
  if (features::IsBrowserWindowListenerObservationEnabled()) {
    native_theme_observation_.Observe(
        ui::NativeTheme::GetInstanceForNativeUi());
  }
}

BrowserWindowThemeObserver::~BrowserWindowThemeObserver() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
}

// static
BrowserWindowThemeObserver* BrowserWindowThemeObserver::From(
    BrowserWindowInterface* browser) {
  return Get(browser->GetUnownedUserDataHost());
}

// static
const BrowserWindowThemeObserver* BrowserWindowThemeObserver::From(
    const BrowserWindowInterface* browser) {
  return Get(browser->GetUnownedUserDataHost());
}

base::CallbackListSubscription
BrowserWindowThemeObserver::RegisterThemeChangedCallback(
    ThemeChangedCallback callback) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  return theme_changed_callbacks_.Add(std::move(callback));
}

void BrowserWindowThemeObserver::NotifyThemeChanged(
    BrowserThemeChangeType type) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  theme_changed_callbacks_.Notify(type);
}

void BrowserWindowThemeObserver::OnThemeChanged() {
  NotifyThemeChanged(BrowserThemeChangeType::kBrowserTheme);
}

void BrowserWindowThemeObserver::OnNativeThemeUpdated(
    ui::NativeTheme* observed_theme) {
  NotifyThemeChanged(BrowserThemeChangeType::kNativeTheme);
}

bool BrowserWindowThemeObserver::IsObservingNativeThemeForTesting() const {
  return native_theme_observation_.IsObserving();
}
