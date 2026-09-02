// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/views/frame/glass_frame_service.h"

#include <algorithm>
#include <utility>

#include "base/check.h"
#include "base/functional/bind.h"
#include "base/metrics/histogram_functions.h"
#include "chrome/browser/browser_process.h"
#include "chrome/browser/global_features.h"
#include "chrome/browser/performance_manager/public/user_tuning/battery_saver_mode_manager.h"
#include "chrome/browser/themes/theme_service.h"
#include "chrome/browser/themes/theme_service_factory.h"
#include "chrome/browser/ui/browser_window/public/browser_window_features.h"
#include "chrome/browser/ui/browser_window/public/browser_window_interface.h"
#include "chrome/browser/ui/browser_window/public/global_browser_collection.h"
#include "chrome/browser/ui/exclusive_access/exclusive_access_manager.h"
#include "chrome/browser/ui/exclusive_access/fullscreen_controller.h"
#include "chrome/common/pref_names.h"
#include "components/prefs/pref_registry_simple.h"
#include "components/prefs/pref_service.h"

#if BUILDFLAG(IS_MAC)
#include "base/mac/mac_util.h"
#endif

DEFINE_USER_DATA(GlassFrameService);

// static
GlassFrameService* GlassFrameService::GetInstance() {
  return Get(g_browser_process->GetUnownedUserDataHost());
}

// static
void GlassFrameService::RegisterLocalStatePrefs(PrefRegistrySimple* registry) {
  registry->RegisterBooleanPref(prefs::kGlassFrameEnabled, true);
}

GlassFrameService::GlassFrameService(BrowserProcess& process)
    : scoped_unowned_user_data_(process.GetUnownedUserDataHost(), *this) {
  GlobalBrowserCollection* const browser_collection =
      GlobalBrowserCollection::GetInstance();
  CHECK(browser_collection);
  browser_collection_observation_.Observe(browser_collection);

  CHECK(g_browser_process);
  PrefService* const pref_service = g_browser_process->local_state();
  CHECK(pref_service);
  is_glass_frame_enabled_ = pref_service->GetBoolean(prefs::kGlassFrameEnabled);
  pref_change_registrar_.Init(pref_service);
  pref_change_registrar_.Add(
      prefs::kGlassFrameEnabled,
      base::BindRepeating(&GlassFrameService::OnGlassFrameEnabledPrefChanged,
                          base::Unretained(this)));
  CHECK(
      performance_manager::user_tuning::BatterySaverModeManager::HasInstance());
  auto* const bsm_manager =
      performance_manager::user_tuning::BatterySaverModeManager::GetInstance();
  is_battery_saver_mode_active_ = bsm_manager->IsBatterySaverActive();
  battery_saver_observation_.Observe(bsm_manager);

  // Pre-populate the deque with the most recently activated browsers.
  browser_collection->ForEach(
      [this](BrowserWindowInterface* browser) {
        MaybeTrackBrowser(browser);
        return true;
      },
      BrowserCollection::Order::kActivation);

  LogGlassFramePreferredLook();
}

GlassFrameService::~GlassFrameService() = default;

base::CallbackListSubscription
GlassFrameService::RegisterGlassFrameEligibilityChangedCallback(
    BrowserWindowInterface* browser_window_interface,
    GlassFrameEligibilityChangedCallback callback) {
  return window_callbacks_[browser_window_interface].Add(std::move(callback));
}

bool GlassFrameService::IsBrowserWindowEligible(
    BrowserWindowInterface* browser) {
  return GetEligibleBrowserWindowInterfaces().contains(browser);
}

void GlassFrameService::OnBrowserActivated(BrowserWindowInterface* browser) {
  MaybeTrackBrowser(browser);
  OnEligibleStateChanged();
}

void GlassFrameService::OnBrowserClosed(BrowserWindowInterface* browser) {
  StopTrackingBrowser(browser);
  OnEligibleStateChanged();
}

void GlassFrameService::OnBatterySaverActiveChanged(bool is_active) {
  if (is_battery_saver_mode_active_ == is_active) {
    return;
  }
  is_battery_saver_mode_active_ = is_active;
  OnEligibleStateChanged();
}

void GlassFrameService::OnBatterySaverModeManagerDestroyed() {
  // Reset the BatterySaverModeManager observation to prevent having
  // a dangling pointer to the BatterySaverModeManager on destruction.
  battery_saver_observation_.Reset();
  is_battery_saver_mode_active_ = false;
  OnEligibleStateChanged();
}

void GlassFrameService::OnThemeChanged() {
  OnEligibleStateChanged();
}

base::flat_set<BrowserWindowInterface*>
GlassFrameService::ActivationOrderedEligibleBrowsers() {
  base::flat_set<BrowserWindowInterface*> activation_ordered_eligible_browsers;
  GlobalBrowserCollection::GetInstance()->ForEach(
      [&activation_ordered_eligible_browsers,
       this](BrowserWindowInterface* browser) {
        // Stop iterating once the maximum number of glass windows is reached.
        if (activation_ordered_eligible_browsers.size() >= kMaxGlassWindows) {
          return false;
        }
        // Skip untracked windows (e.g. non-normal windows or background windows
        // that have not yet been activated).
        if (!tracked_browsers_.contains(browser)) {
          return true;
        }
        // Skip windows currently in fullscreen mode.
        auto* const exclusive_access_manager =
            ExclusiveAccessManager::From(browser);
        if (exclusive_access_manager &&
            exclusive_access_manager->fullscreen_controller() &&
            exclusive_access_manager->fullscreen_controller()
                ->IsFullscreenForBrowser()) {
          return true;
        }
        // Skip windows using an extension theme, which disables glass.
        if (auto* const theme_service =
                ThemeServiceFactory::GetForProfile(browser->GetProfile())) {
          if (theme_service->UsingExtensionTheme()) {
            return true;
          }
        }
        activation_ordered_eligible_browsers.insert(browser);
        return activation_ordered_eligible_browsers.size() < kMaxGlassWindows;
      },
      BrowserCollection::Order::kActivation);
  return activation_ordered_eligible_browsers;
}

base::flat_set<BrowserWindowInterface*>
GlassFrameService::GetEligibleBrowserWindowInterfaces() {
  if (!is_glass_frame_enabled_) {
    return {};
  }

  if (is_battery_saver_mode_active_) {
    return {};
  }

  return ActivationOrderedEligibleBrowsers();
}

void GlassFrameService::OnGlassFrameEnabledPrefChanged() {
  PrefService* const pref_service = g_browser_process->local_state();
  CHECK(pref_service);
  const bool is_enabled = pref_service->GetBoolean(prefs::kGlassFrameEnabled);
  if (is_glass_frame_enabled_ == is_enabled) {
    return;
  }
  is_glass_frame_enabled_ = is_enabled;
  OnEligibleStateChanged();
}

void GlassFrameService::OnEligibleStateChanged() {
  const base::flat_set<BrowserWindowInterface*> eligible =
      GetEligibleBrowserWindowInterfaces();
  for (auto& [browser, callback_list] : window_callbacks_) {
    callback_list.Notify(eligible.contains(browser));
  }
}

void GlassFrameService::MaybeTrackBrowser(BrowserWindowInterface* browser) {
  if (browser->GetType() != BrowserWindowInterface::TYPE_NORMAL) {
    return;
  }
  if (!fullscreen_subscriptions_.contains(browser)) {
    if (auto* const exclusive_access_manager =
            ExclusiveAccessManager::From(browser)) {
      if (auto* const fullscreen_controller =
              exclusive_access_manager->fullscreen_controller()) {
        fullscreen_subscriptions_[browser] =
            fullscreen_controller->RegisterOnFullscreenStateChanged(
                base::BindRepeating(&GlassFrameService::OnEligibleStateChanged,
                                    base::Unretained(this)));
      }
    }
  }
  if (auto* const theme_service =
          ThemeServiceFactory::GetForProfile(browser->GetProfile())) {
    if (!theme_observations_.IsObservingSource(theme_service)) {
      theme_observations_.AddObservation(theme_service);
    }
  }

  tracked_browsers_.insert(browser);
}

void GlassFrameService::StopTrackingBrowser(BrowserWindowInterface* browser) {
  fullscreen_subscriptions_.erase(browser);
  window_callbacks_.erase(browser);
  tracked_browsers_.erase(browser);

  if (auto* const theme_service =
          ThemeServiceFactory::GetForProfile(browser->GetProfile())) {
    bool is_still_used = false;
    for (BrowserWindowInterface* tracked : tracked_browsers_) {
      if (ThemeServiceFactory::GetForProfile(tracked->GetProfile()) ==
          theme_service) {
        is_still_used = true;
        break;
      }
    }
    if (!is_still_used &&
        theme_observations_.IsObservingSource(theme_service)) {
      theme_observations_.RemoveObservation(theme_service);
    }
  }
}

void GlassFrameService::LogGlassFramePreferredLook() {
#if BUILDFLAG(IS_MAC)
  if (base::mac::MacOSMajorVersion() == 26) {
    base::UmaHistogramEnumeration(
        "Mac.GlassFrame.MacOS26LiquidGlassPreferredLook",
        base::mac::GetMacOS26LiquidGlassPreferredLook());
  }
#endif  // BUILDFLAG(IS_MAC)
}
