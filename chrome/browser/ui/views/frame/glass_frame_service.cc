// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/views/frame/glass_frame_service.h"

#include <memory>
#include <string>
#include <utility>

#include "base/check.h"
#include "base/feature_list.h"
#include "base/functional/bind.h"
#include "base/location.h"
#include "base/memory/raw_ptr.h"
#include "base/metrics/histogram_functions.h"
#include "base/time/time.h"
#include "base/timer/timer.h"
#include "base/values.h"
#include "build/build_config.h"
#include "chrome/browser/browser_process.h"
#include "chrome/browser/global_features.h"
#include "chrome/browser/media/webrtc/media_capture_devices_dispatcher.h"
#include "chrome/browser/performance_manager/public/user_tuning/battery_saver_mode_manager.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/themes/theme_service.h"
#include "chrome/browser/themes/theme_service_factory.h"
#include "chrome/browser/ui/browser_window/public/browser_window_features.h"
#include "chrome/browser/ui/browser_window/public/browser_window_interface.h"
#include "chrome/browser/ui/browser_window/public/global_browser_collection.h"
#include "chrome/browser/ui/exclusive_access/exclusive_access_context.h"
#include "chrome/browser/ui/exclusive_access/exclusive_access_manager.h"
#include "chrome/browser/ui/exclusive_access/fullscreen_controller.h"
#include "chrome/browser/ui/user_education/browser_user_education_interface.h"
#include "chrome/browser/ui/views/frame/safe_invoke/safe_invoke.h"
#include "chrome/common/pref_names.h"
#include "components/feature_engagement/public/feature_constants.h"
#include "components/metrics/daily_event.h"
#include "components/prefs/pref_registry_simple.h"
#include "components/prefs/pref_service.h"
#include "ui/base/ui_base_features.h"

#if BUILDFLAG(IS_MAC)
#include "base/mac/mac_util.h"
#include "media/base/media_switches.h"
#endif

namespace {

// The interval at which the DailyEvent::CheckInterval function should be
// called.
constexpr base::TimeDelta kDailyEventIntervalTimeDelta = base::Minutes(30);
}  // namespace

class GlassFrameMetricsReporter {
 public:
  explicit GlassFrameMetricsReporter(PrefService* pref_service)
      : pref_service_(pref_service),
        daily_event_(std::make_unique<metrics::DailyEvent>(
            pref_service,
            prefs::kGlassFrameDailySample,
            /*histogram_name=*/std::string())) {
    daily_event_->AddObserverClosure(base::BindRepeating(
        &GlassFrameMetricsReporter::OnDailyEvent, base::Unretained(this)));
    daily_event_->CheckInterval();
    daily_event_timer_.Start(FROM_HERE, kDailyEventIntervalTimeDelta,
                             daily_event_.get(),
                             &metrics::DailyEvent::CheckInterval);
  }

  ~GlassFrameMetricsReporter() = default;

 private:
  void OnDailyEvent() {
    base::UmaHistogramBoolean(
        "Browser.GlassFrame.Enabled.Daily",
        pref_service_->GetBoolean(prefs::kGlassFrameEnabled));
    const PrefService::Preference* const pref =
        pref_service_->FindPreference(prefs::kGlassFrameEnabled);
    CHECK(pref);
    base::UmaHistogramBoolean("Browser.GlassFrame.IsDefault.Daily",
                              pref->IsDefaultValue());
  }

  raw_ptr<PrefService> pref_service_ = nullptr;
  std::unique_ptr<metrics::DailyEvent> daily_event_;

  // The timer used to periodically check if the daily event should be
  // triggered.
  base::RepeatingTimer daily_event_timer_;
};

DEFINE_USER_DATA(GlassFrameService);

// static
GlassFrameService* GlassFrameService::GetInstance() {
  return Get(g_browser_process->GetUnownedUserDataHost());
}

// static
void GlassFrameService::RegisterLocalStatePrefs(PrefRegistrySimple* registry) {
  bool default_enabled = true;
#if defined(ARCH_CPU_X86_FAMILY)
  default_enabled = false;
#endif  // defined (ARCH_CPU_X86_FAMILY)
  registry->RegisterBooleanPref(prefs::kGlassFrameEnabled, default_enabled);
  metrics::DailyEvent::RegisterPref(registry, prefs::kGlassFrameDailySample);
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
#if !defined(ARCH_CPU_X86_FAMILY)
  pref_service->SetDefaultPrefValue(
      prefs::kGlassFrameEnabled,
      base::Value(features::kGlassFrameEnabledByDefault.Get()));
#endif  // !defined(ARCH_CPU_X86_FAMILY)
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

#if BUILDFLAG(IS_MAC)
  // When `kUseSCContentSharingPicker` is disabled, Chrome window capture on
  // macOS uses `ViewsWidgetVideoCaptureDeviceMac` (Viz `FrameSink` capture)
  // instead of `ScreenCaptureKitDeviceMac`. Because the glass frame is rendered
  // by a native AppKit `NSGlassEffectView` behind the transparent
  // `ui::Compositor` frame area, Viz `FrameSink` capture misses the glass
  // background and renders the frame area as black. Observe window capture so
  // we can fall back to the opaque frame while a window is being captured.
  if (!base::FeatureList::IsEnabled(media::kUseSCContentSharingPicker)) {
    media_stream_capture_observation_.Observe(
        MediaCaptureDevicesDispatcher::GetInstance()
            ->GetMediaStreamCaptureIndicator()
            .get());
  }
#endif  // BUILDFLAG(IS_MAC)

  // Pre-populate the deque with the most recently activated browsers.
  browser_collection->ForEach(
      [this](BrowserWindowInterface* browser) {
        MaybeTrackBrowser(browser);
        if (IsGlassFrameAllowed() &&
            eligible_browsers_.size() < kMaxGlassWindows &&
            IsBrowserEligibleForGlass(browser)) {
          eligible_browsers_.insert(browser);
        }
        return true;
      },
      BrowserCollection::Order::kActivation);
  UpdateHasMultipleOpenProfiles();

  metrics_reporter_ = std::make_unique<GlassFrameMetricsReporter>(
      g_browser_process->local_state());

  LogGlassFramePreferredLook();
}

GlassFrameService::~GlassFrameService() = default;

base::CallbackListSubscription
GlassFrameService::RegisterGlassFrameEligibilityChangedCallback(
    BrowserWindowInterface* browser_window_interface,
    GlassFrameEligibilityChangedCallback callback) {
  return window_callbacks_[browser_window_interface].Add(std::move(callback));
}

base::CallbackListSubscription
GlassFrameService::RegisterMultipleOpenProfilesChangedCallback(
    base::RepeatingClosure callback) {
  return multiple_open_profiles_callbacks_.Add(std::move(callback));
}

bool GlassFrameService::IsBrowserWindowEligible(
    BrowserWindowInterface* browser) {
  return eligible_browsers_.contains(browser);
}

bool GlassFrameService::HasMultipleOpenProfiles() const {
  return has_multiple_open_profiles_;
}

bool GlassFrameService::IsGlassFrameAllowed() {
  return is_glass_frame_enabled_ && !is_battery_saver_mode_active_ &&
         window_capturing_web_contents_.empty();
}

void GlassFrameService::OnBrowserCreated(BrowserWindowInterface* browser) {
  if (browser->GetType() != BrowserWindowInterface::TYPE_NORMAL) {
    return;
  }
  UpdateHasMultipleOpenProfiles();
}

void GlassFrameService::OnBrowserActivated(BrowserWindowInterface* browser) {
  if (browser->GetType() != BrowserWindowInterface::TYPE_NORMAL) {
    return;
  }

  MaybeTrackBrowser(browser);
  OnEligibleStateChanged();
  MaybeShowPromo(browser);
}

void GlassFrameService::OnBrowserClosed(BrowserWindowInterface* browser) {
  StopTrackingBrowser(browser);
  if (browser->GetType() != BrowserWindowInterface::TYPE_NORMAL) {
    return;
  }
  UpdateHasMultipleOpenProfiles();
  if (eligible_browsers_.erase(browser)) {
    OnEligibleStateChanged();
  }
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

void GlassFrameService::OnIsCapturingWindowChanged(
    content::WebContents* web_contents,
    bool is_capturing_window) {
  const bool was_capturing_any_window = !window_capturing_web_contents_.empty();
  if (is_capturing_window) {
    window_capturing_web_contents_.insert(web_contents);
  } else {
    window_capturing_web_contents_.erase(web_contents);
  }
  if (was_capturing_any_window != !window_capturing_web_contents_.empty()) {
    OnEligibleStateChanged();
  }
}

void GlassFrameService::OnThemeChanged() {
  OnEligibleStateChanged();
}

void GlassFrameService::ResetMetricsReporterForTesting() {
  PrefService* const pref_service = g_browser_process->local_state();
  CHECK(pref_service);
  metrics_reporter_ = std::make_unique<GlassFrameMetricsReporter>(pref_service);
}

bool GlassFrameService::IsBrowserEligibleForGlass(
    BrowserWindowInterface* browser) {
  // Skip untracked windows (e.g. non-normal windows or background windows
  // that have not yet been activated).
  if (!tracked_browsers_.contains(browser)) {
    return false;
  }
  // Skip windows currently in fullscreen mode (browser or tab/video
  // fullscreen).
  if (SafeInvoke(ExclusiveAccessManager::From(browser))
          .Then(&ExclusiveAccessManager::context)
          .Then(&ExclusiveAccessContext::IsFullscreen)
          .value_or(false)) {
    return false;
  }
  // Skip windows using an extension theme, which disables glass.
  if (SafeInvoke(ThemeServiceFactory::GetForProfile(browser->GetProfile()))
          .Then(&ThemeService::UsingExtensionTheme)
          .value_or(false)) {
    return false;
  }
  return true;
}

base::flat_set<raw_ptr<BrowserWindowInterface>>
GlassFrameService::GetEligibleBrowserWindowInterfaces() {
  if (!IsGlassFrameAllowed()) {
    return {};
  }

  base::flat_set<raw_ptr<BrowserWindowInterface>>
      activation_ordered_eligible_browsers;
  GlobalBrowserCollection::GetInstance()->ForEach(
      [&activation_ordered_eligible_browsers,
       this](BrowserWindowInterface* browser) {
        // Stop iterating once the maximum number of glass windows is reached.
        if (activation_ordered_eligible_browsers.size() >= kMaxGlassWindows) {
          return false;
        }
        if (IsBrowserEligibleForGlass(browser)) {
          activation_ordered_eligible_browsers.insert(browser);
        }
        return activation_ordered_eligible_browsers.size() < kMaxGlassWindows;
      },
      BrowserCollection::Order::kActivation);
  return activation_ordered_eligible_browsers;
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
  const base::flat_set<raw_ptr<BrowserWindowInterface>>
      previous_eligible_browsers = std::exchange(
          eligible_browsers_, GetEligibleBrowserWindowInterfaces());
  for (auto& [browser, callback_list] : window_callbacks_) {
    const bool is_eligible = eligible_browsers_.contains(browser);
    if (previous_eligible_browsers.contains(browser) != is_eligible) {
      callback_list.Notify(is_eligible);
    }
  }
}

void GlassFrameService::MaybeShowPromo(BrowserWindowInterface* browser) {
  if (has_attempted_startup_promo_ || is_battery_saver_mode_active_) {
    return;
  }

  PrefService* const pref_service = g_browser_process->local_state();
  CHECK(pref_service);
  const PrefService::Preference* const pref =
      pref_service->FindPreference(prefs::kGlassFrameEnabled);
  CHECK(pref);
  if (!pref->IsDefaultValue()) {
    return;
  }

  const base::Feature& promo_feature =
      is_glass_frame_enabled_ ? feature_engagement::kIPHGlassFrameOptOutFeature
                              : feature_engagement::kIPHGlassFrameOptInFeature;
  if (!base::FeatureList::IsEnabled(promo_feature)) {
    return;
  }

  if (auto* const user_education =
          BrowserUserEducationInterface::From(browser)) {
    has_attempted_startup_promo_ = true;
    user_education->MaybeShowStartupFeaturePromo(promo_feature);
  }
}

void GlassFrameService::UpdateHasMultipleOpenProfiles() {
  Profile* first_profile = nullptr;
  bool has_multiple_open_profiles = false;
  GlobalBrowserCollection::GetInstance()->ForEach(
      [&first_profile,
       &has_multiple_open_profiles](BrowserWindowInterface* browser) {
        if (browser->GetType() != BrowserWindowInterface::TYPE_NORMAL) {
          return true;
        }
        Profile* const profile = browser->GetProfile();
        if (!first_profile) {
          first_profile = profile;
        } else if (first_profile != profile) {
          has_multiple_open_profiles = true;
          return false;
        }
        return true;
      });

  if (has_multiple_open_profiles_ != has_multiple_open_profiles) {
    has_multiple_open_profiles_ = has_multiple_open_profiles;
    multiple_open_profiles_callbacks_.Notify();
  }
}

void GlassFrameService::MaybeTrackBrowser(BrowserWindowInterface* browser) {
  if (browser->GetType() != BrowserWindowInterface::TYPE_NORMAL ||
      !tracked_browsers_.insert(browser).second) {
    return;
  }

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
  if (auto* const theme_service =
          ThemeServiceFactory::GetForProfile(browser->GetProfile())) {
    if (!theme_observations_.IsObservingSource(theme_service)) {
      theme_observations_.AddObservation(theme_service);
    }
  }
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
