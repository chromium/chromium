// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_BROWSER_WINDOW_THEME_OBSERVER_BROWSER_WINDOW_THEME_OBSERVER_H_
#define CHROME_BROWSER_UI_BROWSER_WINDOW_THEME_OBSERVER_BROWSER_WINDOW_THEME_OBSERVER_H_

#include "base/callback_list.h"
#include "base/scoped_observation.h"
#include "base/sequence_checker.h"
#include "chrome/browser/themes/theme_service.h"
#include "chrome/browser/themes/theme_service_observer.h"
#include "ui/base/unowned_user_data/scoped_unowned_user_data.h"
#include "ui/native_theme/native_theme.h"
#include "ui/native_theme/native_theme_observer.h"

class BrowserWindowInterface;
enum class BrowserThemeChangeType;

// Window-scoped feature, owned by BrowserWindowFeatures, that bridges theme
// change notifications into listener-style callbacks.
//
// Rather than having every window-level UI component inherit
// ThemeServiceObserver and ui::NativeThemeObserver directly, this class
// observes the underlying theme sources once per browser window and fans
// notifications out to subscribers registered via
// RegisterThemeChangedCallback(). Subscriptions are RAII-managed: destroying
// the returned base::CallbackListSubscription unregisters the callback, so
// subscribers need no manual teardown.
//
// Theme sources bridged:
//  - The profile's ThemeService, reported as
//    BrowserThemeChangeType::kBrowserTheme.
//  - ui::NativeTheme::GetInstanceForNativeUi(), reported as
//    BrowserThemeChangeType::kNativeTheme. This source is only observed when
//    features::kBrowserWindowListenerObservation is enabled; until then
//    BrowserWidget delivers native theme changes to BrowserView directly.
//
// All methods must be called on the UI thread.
class BrowserWindowThemeObserver : public ThemeServiceObserver,
                                   public ui::NativeThemeObserver {
 public:
  DECLARE_USER_DATA(BrowserWindowThemeObserver);

  using ThemeChangedCallback =
      base::RepeatingCallback<void(BrowserThemeChangeType)>;

  explicit BrowserWindowThemeObserver(BrowserWindowInterface* browser);
  BrowserWindowThemeObserver(const BrowserWindowThemeObserver&) = delete;
  BrowserWindowThemeObserver& operator=(const BrowserWindowThemeObserver&) =
      delete;
  ~BrowserWindowThemeObserver() override;

  // Returns the instance attached to `browser`, or nullptr if none.
  static BrowserWindowThemeObserver* From(BrowserWindowInterface* browser);
  static const BrowserWindowThemeObserver* From(
      const BrowserWindowInterface* browser);

  // Registers `callback` to be invoked whenever the theme of this window
  // changes. The callback is invoked until the returned subscription is
  // destroyed.
  base::CallbackListSubscription RegisterThemeChangedCallback(
      ThemeChangedCallback callback);

  // Notifies all registered callbacks of a theme change of the given `type`.
  // Called internally in response to ThemeService and ui::NativeTheme
  // notifications, and directly by embedders that own additional theme sources
  // (e.g. AppBrowserController for web app theme colors). Also useful in tests
  // to drive subscribers without a real ThemeService.
  void NotifyThemeChanged(BrowserThemeChangeType type);

  // Returns true if this observer is bridging ui::NativeTheme updates.
  bool IsObservingNativeThemeForTesting() const;

  // ThemeServiceObserver:
  void OnThemeChanged() override;

  // ui::NativeThemeObserver:
  void OnNativeThemeUpdated(ui::NativeTheme* observed_theme) override;

 private:
  base::RepeatingCallbackList<void(BrowserThemeChangeType)>
      theme_changed_callbacks_;
  base::ScopedObservation<ThemeService, ThemeServiceObserver>
      theme_service_observation_{this};
  base::ScopedObservation<ui::NativeTheme, ui::NativeThemeObserver>
      native_theme_observation_{this};

  SEQUENCE_CHECKER(sequence_checker_);

  ui::ScopedUnownedUserData<BrowserWindowThemeObserver>
      scoped_unowned_user_data_;
};

#endif  // CHROME_BROWSER_UI_BROWSER_WINDOW_THEME_OBSERVER_BROWSER_WINDOW_THEME_OBSERVER_H_
