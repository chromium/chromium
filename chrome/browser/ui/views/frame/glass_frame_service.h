// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_VIEWS_FRAME_GLASS_FRAME_SERVICE_H_
#define CHROME_BROWSER_UI_VIEWS_FRAME_GLASS_FRAME_SERVICE_H_

#include <map>
#include <memory>

#include "base/callback_list.h"
#include "base/containers/flat_set.h"
#include "base/functional/callback.h"
#include "base/memory/raw_ptr.h"
#include "base/scoped_multi_source_observation.h"
#include "base/scoped_observation.h"
#include "chrome/browser/media/webrtc/media_stream_capture_indicator.h"
#include "chrome/browser/performance_manager/public/user_tuning/battery_saver_mode_manager.h"
#include "chrome/browser/themes/theme_service.h"
#include "chrome/browser/themes/theme_service_observer.h"
#include "chrome/browser/ui/browser_window/public/browser_collection_observer.h"
#include "components/prefs/pref_change_registrar.h"
#include "ui/base/unowned_user_data/scoped_unowned_user_data.h"

namespace content {
class WebContents;
}  // namespace content

class BrowserProcess;
class BrowserWindowInterface;
class GlassFrameMetricsReporter;
class GlobalBrowserCollection;
class PrefRegistrySimple;

// A singleton service that is the single source of truth for whether
// a browser window should display the glass frame or not.
class GlassFrameService : public BrowserCollectionObserver,
                          public performance_manager::user_tuning::
                              BatterySaverModeManager::Observer,
                          public MediaStreamCaptureIndicator::Observer,
                          public ThemeServiceObserver {
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

  base::CallbackListSubscription RegisterMultipleOpenProfilesChangedCallback(
      base::RepeatingClosure callback);

  bool IsBrowserWindowEligible(BrowserWindowInterface* browser);

  // Returns true if more than one profile is currently loaded.
  bool HasMultipleOpenProfiles() const;

  explicit GlassFrameService(BrowserProcess& process);
  ~GlassFrameService() override;

  // BrowserCollectionObserver:
  void OnBrowserCreated(BrowserWindowInterface* browser) override;
  void OnBrowserActivated(BrowserWindowInterface* browser) override;
  void OnBrowserClosed(BrowserWindowInterface* browser) override;

  // BatterySaverModeManager::Observer:
  void OnBatterySaverActiveChanged(bool is_active) override;
  void OnBatterySaverModeManagerDestroyed() override;

  // MediaStreamCaptureIndicator::Observer:
  void OnIsCapturingWindowChanged(content::WebContents* web_contents,
                                  bool is_capturing_window) override;

  // ThemeServiceObserver:
  void OnThemeChanged() override;

  // Recreates the metrics reporter so that a fresh `metrics::DailyEvent` reads
  // the latest `prefs::kGlassFrameDailySample` value from prefs and immediately
  // runs `CheckInterval()`.
  void ResetMetricsReporterForTesting();

 private:
  // Returns true if `browser` meets the conditions to display the glass frame
  // (ignoring the `kMaxGlassWindows` limit).
  bool IsBrowserEligibleForGlass(BrowserWindowInterface* browser);

  // Returns true if the entire browser process is allowed to show the glass
  // frame. Returns false otherwise.
  bool IsGlassFrameAllowed();

  // Returns the set of BrowserWindowInterfaces that are eligible to display
  // the glass frame. The returned set has at most `kMaxGlassWindows` elements.
  base::flat_set<raw_ptr<BrowserWindowInterface>>
  GetEligibleBrowserWindowInterfaces();

  void OnGlassFrameEnabledPrefChanged();

  void LogGlassFramePreferredLook();

  void MaybeTrackBrowser(BrowserWindowInterface* browser);

  void StopTrackingBrowser(BrowserWindowInterface* browser);

  void OnEligibleStateChanged();

  void MaybeShowPromo(BrowserWindowInterface* browser);

  void UpdateHasMultipleOpenProfiles();

  std::map<BrowserWindowInterface*, base::RepeatingCallbackList<void(bool)>>
      window_callbacks_;
  base::RepeatingClosureList multiple_open_profiles_callbacks_;
  std::map<BrowserWindowInterface*, base::CallbackListSubscription>
      fullscreen_subscriptions_;
  // Set of tracked normal browsers.
  base::flat_set<raw_ptr<BrowserWindowInterface>> tracked_browsers_;
  // Set of browsers currently eligible to display the glass frame.
  base::flat_set<raw_ptr<BrowserWindowInterface>> eligible_browsers_;
  // Set of WebContents currently capturing a window.
  base::flat_set<raw_ptr<content::WebContents>> window_capturing_web_contents_;

  base::ScopedObservation<GlobalBrowserCollection, BrowserCollectionObserver>
      browser_collection_observation_{this};
  base::ScopedObservation<
      performance_manager::user_tuning::BatterySaverModeManager,
      performance_manager::user_tuning::BatterySaverModeManager::Observer>
      battery_saver_observation_{this};
  base::ScopedObservation<MediaStreamCaptureIndicator,
                          MediaStreamCaptureIndicator::Observer>
      media_stream_capture_observation_{this};
  base::ScopedMultiSourceObservation<ThemeService, ThemeServiceObserver>
      theme_observations_{this};

  PrefChangeRegistrar pref_change_registrar_;
  std::unique_ptr<GlassFrameMetricsReporter> metrics_reporter_;
  bool is_glass_frame_enabled_ = true;
  bool is_battery_saver_mode_active_ = false;
  bool has_attempted_startup_promo_ = false;
  bool has_multiple_open_profiles_ = false;
  ::ui::ScopedUnownedUserData<GlassFrameService> scoped_unowned_user_data_;
};

#endif  // CHROME_BROWSER_UI_VIEWS_FRAME_GLASS_FRAME_SERVICE_H_
