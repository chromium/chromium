// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifdef UNSAFE_BUFFERS_BUILD
// TODO(crbug.com/40285824): Remove this and convert code to safer constructs.
#pragma allow_unsafe_buffers
#endif

#include "chrome/browser/ui/prefs/pref_watcher.h"

#include "base/functional/bind.h"
#include "build/build_config.h"
#include "build/chromeos_buildflags.h"
#include "chrome/browser/browser_process.h"
#include "chrome/browser/privacy_sandbox/tracking_protection_settings_factory.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/renderer_preferences_util.h"
#include "chrome/browser/ui/prefs/prefs_tab_helper.h"
#include "chrome/common/pref_names.h"
#include "components/language/core/browser/pref_names.h"
#include "components/live_caption/pref_names.h"
#include "components/privacy_sandbox/tracking_protection_settings.h"
#include "third_party/blink/public/common/renderer_preferences/renderer_preferences.h"
#include "ui/native_theme/native_theme.h"

#if BUILDFLAG(IS_CHROMEOS_ASH)
#include "ash/constants/ash_pref_names.h"
#endif

#if BUILDFLAG(IS_ANDROID)
#include "components/browser_ui/accessibility/android/font_size_prefs_android.h"
#endif

namespace {

// The list of prefs we want to observe.
const char* const kWebPrefsToObserve[] = {
#if BUILDFLAG(ENABLE_EXTENSIONS)
    prefs::kAnimationPolicy,
#endif
    prefs::kDefaultCharset,
    prefs::kDisable3DAPIs,
    prefs::kEnableHyperlinkAuditing,
    prefs::kWebKitAllowRunningInsecureContent,
    prefs::kWebKitDefaultFixedFontSize,
    prefs::kWebKitDefaultFontSize,
    prefs::kWebKitDomPasteEnabled,
    prefs::kAccessibilityCaptionsTextSize,
    prefs::kAccessibilityCaptionsTextFont,
    prefs::kAccessibilityCaptionsTextColor,
    prefs::kAccessibilityCaptionsTextOpacity,
    prefs::kAccessibilityCaptionsBackgroundColor,
    prefs::kAccessibilityCaptionsTextShadow,
    prefs::kAccessibilityCaptionsBackgroundOpacity,
#if BUILDFLAG(IS_ANDROID)
    browser_ui::prefs::kWebKitFontScaleFactor,
    prefs::kAccessibilityTextSizeContrastFactor,
    browser_ui::prefs::kWebKitForceEnableZoom,
    prefs::kAccessibilityFontWeightAdjustment,
    prefs::kWebKitPasswordEchoEnabled,
#endif
    prefs::kWebKitForceDarkModeEnabled,
    prefs::kWebKitJavascriptEnabled,
    prefs::kWebKitLoadsImagesAutomatically,
    prefs::kWebKitMinimumFontSize,
    prefs::kWebKitMinimumLogicalFontSize,
    prefs::kWebKitPluginsEnabled,
    prefs::kWebkitTabsToLinks,
    prefs::kWebKitTextAreasAreResizable,
    prefs::kWebKitWebSecurityEnabled,
#if BUILDFLAG(IS_CHROMEOS_ASH)
    ash::prefs::kAccessibilityFocusHighlightEnabled,
#else
    prefs::kAccessibilityFocusHighlightEnabled,
#endif
    prefs::kPageColorsBlockList,
};

const int kWebPrefsToObserveLength = std::size(kWebPrefsToObserve);

}  // namespace

// Watching all these settings per tab is slow when a user has a lot of tabs and
// and they use session restore. So watch them once per profile.
// http://crbug.com/452693
PrefWatcher::PrefWatcher(Profile* profile)
    : profile_(profile),
      tracking_protection_settings_(
          TrackingProtectionSettingsFactory::GetForProfile(profile)) {
  CHECK(tracking_protection_settings_);
  tracking_protection_settings_observation_.Observe(
      tracking_protection_settings_);
  native_theme_observation_.Observe(ui::NativeTheme::GetInstanceForWeb());

  profile_pref_change_registrar_.Init(profile_->GetPrefs());

  base::RepeatingClosure renderer_callback = base::BindRepeating(
      &PrefWatcher::UpdateRendererPreferences, base::Unretained(this));
  profile_pref_change_registrar_.Add(language::prefs::kAcceptLanguages,
                                     renderer_callback);
  profile_pref_change_registrar_.Add(prefs::kEnableReferrers,
                                     renderer_callback);
  profile_pref_change_registrar_.Add(prefs::kEnableEncryptedMedia,
                                     renderer_callback);
  profile_pref_change_registrar_.Add(
      prefs::kPrefixedVideoFullscreenApiAvailability, renderer_callback);
  profile_pref_change_registrar_.Add(prefs::kWebRTCIPHandlingPolicy,
                                     renderer_callback);
  profile_pref_change_registrar_.Add(prefs::kWebRTCUDPPortRange,
                                     renderer_callback);

#if !BUILDFLAG(IS_ANDROID)
  profile_pref_change_registrar_.Add(prefs::kCaretBrowsingEnabled,
                                     renderer_callback);
#endif

#if !BUILDFLAG(IS_MAC)
  profile_pref_change_registrar_.Add(prefs::kFullscreenAllowed,
                                     renderer_callback);
#endif

  PrefChangeRegistrar::NamedChangeCallback webkit_callback =
      base::BindRepeating(&PrefWatcher::OnWebPrefChanged,
                          base::Unretained(this));
  for (int i = 0; i < kWebPrefsToObserveLength; ++i) {
    const char* pref_name = kWebPrefsToObserve[i];
    profile_pref_change_registrar_.Add(pref_name, webkit_callback);
  }
  // LocalState can be NULL in tests.
  if (g_browser_process->local_state()) {
    local_state_pref_change_registrar_.Init(g_browser_process->local_state());
    local_state_pref_change_registrar_.Add(prefs::kAllowCrossOriginAuthPrompt,
                                           renderer_callback);
    local_state_pref_change_registrar_.Add(
        prefs::kExplicitlyAllowedNetworkPorts, renderer_callback);
  }
}

PrefWatcher::~PrefWatcher() = default;

void PrefWatcher::RegisterHelper(PrefsTabHelper* helper) {
  tab_helpers_.insert(helper);
}

void PrefWatcher::UnregisterHelper(PrefsTabHelper* helper) {
  tab_helpers_.erase(helper);
}

void PrefWatcher::RegisterRendererPreferenceWatcher(
    mojo::PendingRemote<blink::mojom::RendererPreferenceWatcher> watcher) {
  renderer_preference_watchers_.Add(std::move(watcher));
}

void PrefWatcher::Shutdown() {
  tracking_protection_settings_ = nullptr;
  tracking_protection_settings_observation_.Reset();
  profile_pref_change_registrar_.RemoveAll();
  local_state_pref_change_registrar_.RemoveAll();
}

void PrefWatcher::OnNativeThemeUpdated(
    ui::NativeTheme* observed_theme) {
  UpdateRendererPreferences();
}

void PrefWatcher::OnDoNotTrackEnabledChanged() {
  UpdateRendererPreferences();
}

void PrefWatcher::UpdateRendererPreferences() {
  for (PrefsTabHelper* helper : tab_helpers_) {
    helper->UpdateRendererPreferences();
  }

  blink::RendererPreferences prefs;
  renderer_preferences_util::UpdateFromSystemSettings(&prefs, profile_);
  for (auto& watcher : renderer_preference_watchers_)
    watcher->NotifyUpdate(prefs);
}

void PrefWatcher::OnWebPrefChanged(const std::string& pref_name) {
  for (PrefsTabHelper* helper : tab_helpers_) {
    helper->OnWebPrefChanged(pref_name);
  }
}

// static
PrefWatcher* PrefWatcherFactory::GetForProfile(Profile* profile) {
  return static_cast<PrefWatcher*>(
      GetInstance()->GetServiceForBrowserContext(profile, true));
}

// static
PrefWatcherFactory* PrefWatcherFactory::GetInstance() {
  return base::Singleton<PrefWatcherFactory>::get();
}

PrefWatcherFactory::PrefWatcherFactory()
    : ProfileKeyedServiceFactory(
          "PrefWatcher",
          ProfileSelections::Builder()
              .WithRegular(ProfileSelection::kOwnInstance)
              // TODO(crbug.com/40257657): Check if this service is needed in
              // Guest mode.
              .WithGuest(ProfileSelection::kOwnInstance)
              // TODO(crbug.com/41488885): Check if this service is needed for
              // Ash Internals.
              .WithAshInternals(ProfileSelection::kOwnInstance)
              .Build()) {
  DependsOn(TrackingProtectionSettingsFactory::GetInstance());
}

PrefWatcherFactory::~PrefWatcherFactory() = default;

std::unique_ptr<KeyedService>
PrefWatcherFactory::BuildServiceInstanceForBrowserContext(
    content::BrowserContext* browser_context) const {
  return std::make_unique<PrefWatcher>(
      Profile::FromBrowserContext(browser_context));
}

// static
PrefWatcher* PrefWatcher::Get(Profile* profile) {
  return PrefWatcherFactory::GetForProfile(profile);
}
