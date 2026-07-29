// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_VIEWS_FRAME_GLASS_FRAME_SERVICE_H_
#define CHROME_BROWSER_UI_VIEWS_FRAME_GLASS_FRAME_SERVICE_H_

#include <deque>
#include <map>

#include "base/callback_list.h"
#include "base/containers/flat_set.h"
#include "base/functional/callback.h"
#include "base/scoped_observation.h"
#include "chrome/browser/performance_manager/public/user_tuning/battery_saver_mode_manager.h"
#include "chrome/browser/ui/browser_window/public/browser_collection_observer.h"
#include "components/prefs/pref_change_registrar.h"
#include "ui/base/unowned_user_data/scoped_unowned_user_data.h"

class BrowserProcess;
class BrowserWindowInterface;
class GlobalBrowserCollection;
class PrefRegistrySimple;

// A singleton service that is the single source of truth for whether
// a browser window should display the glass frame or not.
class GlassFrameService : public BrowserCollectionObserver,
                          public performance_manager::user_tuning::
                              BatterySaverModeManager::Observer {
 public:
  DECLARE_USER_DATA(GlassFrameService);

  // Returns non-null if glass frame is enabled for this process (though it may
  // still be disabled in prefs, or for a particular background window). Call
  // IsBrowserWindowEligible() to determine if a particular window should get
  // glass treatment.
  static GlassFrameService* GetInstance();

  static void RegisterLocalStatePrefs(PrefRegistrySimple* registry);

  // Maximum number of windows that will display the glass frame at any given
  // time.
  static constexpr size_t kMaxGlassWindows = 1;

  GlassFrameService(const GlassFrameService&) = delete;
  GlassFrameService& operator=(const GlassFrameService&) = delete;

  using GlassFrameEligibilityChangedCallback =
      base::RepeatingCallback<void(bool is_eligible)>;
  base::CallbackListSubscription RegisterGlassFrameEligibilityChangedCallback(
      BrowserWindowInterface* browser_window_interface,
      GlassFrameEligibilityChangedCallback callback);

  bool IsBrowserWindowEligible(BrowserWindowInterface* browser);

  explicit GlassFrameService(BrowserProcess& process);
  ~GlassFrameService() override;

  // BrowserCollectionObserver:
  void OnBrowserActivated(BrowserWindowInterface* browser) override;
  void OnBrowserClosed(BrowserWindowInterface* browser) override;

  // BatterySaverModeManager::Observer:
  void OnBatterySaverActiveChanged(bool is_active) override;
  void OnBatterySaverModeManagerDestroyed() override;

 private:
  // Returns the set of BrowserWindowInterfaces for the most recently activated
  // browser window interfaces. The returned set has at most `kMaxGlassWindows`
  // elements.
  base::flat_set<BrowserWindowInterface*> MostRecentActivatedBrowsers();

  // Returns the set of BrowserWindowInterfaces that are eligible to display
  // the glass frame.
  base::flat_set<BrowserWindowInterface*> GetEligibleBrowserWindowInterfaces();

  void OnGlassFrameEnabledPrefChanged();

  void LogGlassFramePreferredLook();

  void NotifyEligibilityChanged();

  std::map<BrowserWindowInterface*, base::RepeatingCallbackList<void(bool)>>
      window_callbacks_;
  // Deque of tracked browsers, ordered from most recently activated to
  // least recently activated.
  std::deque<BrowserWindowInterface*> activated_browsers_;

  base::ScopedObservation<GlobalBrowserCollection, BrowserCollectionObserver>
      browser_collection_observation_{this};
  base::ScopedObservation<
      performance_manager::user_tuning::BatterySaverModeManager,
      performance_manager::user_tuning::BatterySaverModeManager::Observer>
      battery_saver_observation_{this};

  PrefChangeRegistrar pref_change_registrar_;
  bool is_battery_saver_mode_active_ = false;
  ::ui::ScopedUnownedUserData<GlassFrameService> scoped_unowned_user_data_;
};

#endif  // CHROME_BROWSER_UI_VIEWS_FRAME_GLASS_FRAME_SERVICE_H_
