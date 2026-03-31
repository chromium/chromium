// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_BROWSER_WINDOW_THEME_OBSERVER_H_
#define CHROME_BROWSER_UI_BROWSER_WINDOW_THEME_OBSERVER_H_

#include "base/callback_list.h"
#include "base/scoped_observation.h"
#include "chrome/browser/themes/theme_service.h"
#include "chrome/browser/themes/theme_service_observer.h"
#include "chrome/browser/ui/browser_window.h"
#include "ui/base/unowned_user_data/scoped_unowned_user_data.h"

class BrowserWindowInterface;

// Observes ThemeService for theme changes and notifies registered listeners.
// This was previously handled directly by Browser.
class BrowserWindowThemeObserver : public ThemeServiceObserver {
 public:
  DECLARE_USER_DATA(BrowserWindowThemeObserver);

  using ThemeChangedCallback =
      base::RepeatingCallback<void(BrowserThemeChangeType)>;

  explicit BrowserWindowThemeObserver(BrowserWindowInterface* browser);
  BrowserWindowThemeObserver(const BrowserWindowThemeObserver&) = delete;
  BrowserWindowThemeObserver& operator=(const BrowserWindowThemeObserver&) =
      delete;
  ~BrowserWindowThemeObserver() override;

  static BrowserWindowThemeObserver* From(BrowserWindowInterface* browser);

  // Register a callback to be invoked when the theme changes.
  base::CallbackListSubscription RegisterThemeChangedCallback(
      ThemeChangedCallback callback);

  // Notify all listeners of a theme change with the given type.
  // Used by AppBrowserController to notify web app theme changes.
  void NotifyThemeChanged(BrowserThemeChangeType type);

  // ThemeServiceObserver:
  void OnThemeChanged() override;

 private:
  base::RepeatingCallbackList<void(BrowserThemeChangeType)>
      theme_changed_callbacks_;
  base::ScopedObservation<ThemeService, ThemeServiceObserver>
      theme_service_observation_{this};
  ui::ScopedUnownedUserData<BrowserWindowThemeObserver>
      scoped_unowned_user_data_;
};

#endif  // CHROME_BROWSER_UI_BROWSER_WINDOW_THEME_OBSERVER_H_
