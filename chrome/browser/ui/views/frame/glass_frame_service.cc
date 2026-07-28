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
#include "chrome/browser/ui/browser_window/public/browser_window_interface.h"
#include "chrome/browser/ui/browser_window/public/global_browser_collection.h"
#include "chrome/common/pref_names.h"
#include "components/prefs/pref_registry_simple.h"
#include "components/prefs/pref_service.h"

#if BUILDFLAG(IS_MAC)
#include "base/mac/mac_util.h"
#endif

namespace {
// The maximum number of windows tracked by this service that can be eligible
// for the glass frame.
constexpr size_t kMaxWindowsTrackedForGlassFrame = 50;
}  // namespace

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
        // Break out of ForEach early when we reached the maximum number of
        // tracked windows.
        if (activated_browsers_.size() >= kMaxWindowsTrackedForGlassFrame) {
          return false;
        }

        if (browser->GetType() == BrowserWindowInterface::TYPE_NORMAL) {
          activated_browsers_.push_back(browser);
        }

        // Keep iterating if we haven't hit the maximum number of tracked
        // windows.
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
  return callbacks_.Add(base::BindRepeating(
      [](BrowserWindowInterface* target_browser,
         GlassFrameEligibilityChangedCallback target_callback,
         const base::flat_set<BrowserWindowInterface*>& eligible) {
        target_callback.Run(eligible.contains(target_browser));
      },
      browser_window_interface, std::move(callback)));
}

bool GlassFrameService::IsBrowserWindowEligible(
    BrowserWindowInterface* browser) {
  return GetEligibleBrowserWindowInterfaces().contains(browser);
}

void GlassFrameService::OnBrowserActivated(BrowserWindowInterface* browser) {
  if (browser->GetType() != BrowserWindowInterface::TYPE_NORMAL) {
    return;
  }

  // If the browser is already in the list, remove it since it
  // needs to be re-added to the front of the list.
  auto it = std::find(activated_browsers_.begin(), activated_browsers_.end(),
                      browser);
  if (it != activated_browsers_.end()) {
    activated_browsers_.erase(it);
  }

  // If the list size has exceeded the maximum, remove the oldest browser.
  if (activated_browsers_.size() >= kMaxWindowsTrackedForGlassFrame) {
    activated_browsers_.pop_back();
  }

  activated_browsers_.push_front(browser);

  const base::flat_set<BrowserWindowInterface*> new_eligible =
      GetEligibleBrowserWindowInterfaces();
  callbacks_.Notify(new_eligible);
}

void GlassFrameService::OnBrowserClosed(BrowserWindowInterface* browser) {
  if (browser->GetType() != BrowserWindowInterface::TYPE_NORMAL) {
    return;
  }

  const base::flat_set<BrowserWindowInterface*> old_eligible =
      GetEligibleBrowserWindowInterfaces();

  auto it = std::find(activated_browsers_.begin(), activated_browsers_.end(),
                      browser);
  if (it != activated_browsers_.end()) {
    activated_browsers_.erase(it);
  }

  const base::flat_set<BrowserWindowInterface*> new_eligible =
      GetEligibleBrowserWindowInterfaces();
  if (old_eligible != new_eligible) {
    callbacks_.Notify(new_eligible);
  }
}

void GlassFrameService::OnBatterySaverActiveChanged(bool is_active) {
  if (is_battery_saver_mode_active_ == is_active) {
    return;
  }
  is_battery_saver_mode_active_ = is_active;
  callbacks_.Notify(GetEligibleBrowserWindowInterfaces());
}

void GlassFrameService::OnBatterySaverModeManagerDestroyed() {
  // Reset the BatterySaverModeManager observation to prevent having
  // a dangling pointer to the BatterySaverModeManager on destruction.
  battery_saver_observation_.Reset();
  is_battery_saver_mode_active_ = false;
}

base::flat_set<BrowserWindowInterface*>
GlassFrameService::MostRecentActivatedBrowsers() {
  base::flat_set<BrowserWindowInterface*> eligible;
  const size_t max_eligible_count =
      std::min(activated_browsers_.size(), kMaxGlassWindows);
  for (size_t i = 0; i < max_eligible_count; i++) {
    eligible.insert(activated_browsers_[i]);
  }
  return eligible;
}

base::flat_set<BrowserWindowInterface*>
GlassFrameService::GetEligibleBrowserWindowInterfaces() {
  if (!g_browser_process->local_state()->GetBoolean(
          prefs::kGlassFrameEnabled)) {
    return {};
  }

  if (is_battery_saver_mode_active_) {
    return {};
  }

  return MostRecentActivatedBrowsers();
}

void GlassFrameService::OnGlassFrameEnabledPrefChanged() {
  callbacks_.Notify(GetEligibleBrowserWindowInterfaces());
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
