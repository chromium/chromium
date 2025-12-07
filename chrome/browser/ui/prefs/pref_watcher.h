// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_PREFS_PREF_WATCHER_H_
#define CHROME_BROWSER_UI_PREFS_PREF_WATCHER_H_

#include "base/memory/raw_ptr.h"
#include "base/memory/singleton.h"
#include "base/scoped_observation.h"
#include "chrome/browser/profiles/profile_keyed_service_factory.h"
#include "components/keyed_service/core/keyed_service.h"
#include "components/prefs/pref_change_registrar.h"
#include "mojo/public/cpp/bindings/remote_set.h"
#include "third_party/blink/public/mojom/renderer_preference_watcher.mojom.h"
#include "ui/native_theme/native_theme_observer.h"

class Profile;
class PrefsTabHelper;
class PrimaryPastePrefHelper;

// Watches updates in WebKitPreferences and blink::RendererPreferences, and
// notifies tab helpers and registered watchers of those updates.
class PrefWatcher : public KeyedService,
                    public ui::NativeThemeObserver {
 public:
  explicit PrefWatcher(Profile* profile);
  ~PrefWatcher() override;

  static PrefWatcher* Get(Profile* profile);

  void RegisterHelper(PrefsTabHelper* helper);
  void UnregisterHelper(PrefsTabHelper* helper);
  void RegisterRendererPreferenceWatcher(
      mojo::PendingRemote<blink::mojom::RendererPreferenceWatcher> watcher);

 private:
  // KeyedService overrides:
  void Shutdown() override;

  // ui::NativeThemeObserver:
  void OnNativeThemeUpdated(ui::NativeTheme* observed_theme) override;

  void UpdateRendererPreferences();
  void OnWebPrefChanged(const std::string& pref_name);
  void OnLiveCaptionEnabledPrefChanged(const std::string& pref_name);

  raw_ptr<Profile> profile_;
  PrefChangeRegistrar profile_pref_change_registrar_;
  PrefChangeRegistrar local_state_pref_change_registrar_;

  // |tab_helpers_| observe changes in WebKitPreferences and
  // blink::RendererPreferences.
  std::set<raw_ptr<PrefsTabHelper, SetExperimental>> tab_helpers_;

#if BUILDFLAG(IS_LINUX)
  friend class PrimaryPastePrefHelper;
  std::unique_ptr<PrimaryPastePrefHelper> primary_paste_pref_helper_;
#endif

  // |renderer_preference_watchers_| observe changes in
  // blink::RendererPreferences. If the consumer also wants to WebKit
  // preference changes, use |tab_helpers_|.
  mojo::RemoteSet<blink::mojom::RendererPreferenceWatcher>
      renderer_preference_watchers_;

  base::ScopedObservation<ui::NativeTheme, ui::NativeThemeObserver>
      native_theme_observation_{this};
};

class PrefWatcherFactory : public ProfileKeyedServiceFactory {
 public:
  static PrefWatcher* GetForProfile(Profile* profile);
  static PrefWatcherFactory* GetInstance();

 private:
  friend struct base::DefaultSingletonTraits<PrefWatcherFactory>;

  PrefWatcherFactory();
  ~PrefWatcherFactory() override;

  // BrowserContextKeyedServiceFactory:
  std::unique_ptr<KeyedService> BuildServiceInstanceForBrowserContext(
      content::BrowserContext* browser_context) const override;
};

#endif  // CHROME_BROWSER_UI_PREFS_PREF_WATCHER_H_
