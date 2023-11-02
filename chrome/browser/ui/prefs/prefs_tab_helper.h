// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_PREFS_PREFS_TAB_HELPER_H_
#define CHROME_BROWSER_UI_PREFS_PREFS_TAB_HELPER_H_

#include "base/memory/raw_ptr.h"
#include "base/memory/weak_ptr.h"
#include "build/build_config.h"
#include "chrome/browser/font_pref_change_notifier.h"
#include "chrome/browser/themes/theme_service_observer.h"
#include "content/public/browser/web_contents_user_data.h"

#if !BUILDFLAG(IS_ANDROID)
#include "chrome/browser/ui/zoom/chrome_zoom_level_prefs.h"
#endif

class Profile;

namespace content {
class WebContents;
}

namespace user_prefs {
class PrefRegistrySyncable;
}

// Per-tab class to handle user preferences.
class PrefsTabHelper : public ThemeServiceObserver,
                       public content::WebContentsUserData<PrefsTabHelper> {
 public:
  PrefsTabHelper(const PrefsTabHelper&) = delete;
  PrefsTabHelper& operator=(const PrefsTabHelper&) = delete;

  ~PrefsTabHelper() override;

  static void RegisterProfilePrefs(user_prefs::PrefRegistrySyncable* registry,
                                   const std::string& locale);
  static void GetServiceInstance();

 protected:
  // Update the RenderView's WebPreferences. Exposed as protected for testing.
  virtual void UpdateWebPreferences();

 private:
  explicit PrefsTabHelper(content::WebContents* contents);
  friend class content::WebContentsUserData<PrefsTabHelper>;
  friend class PrefWatcher;

  // ThemeServiceObserver overrides:
  void OnThemeChanged() override;

  // Update the WebContents's blink::RendererPreferences.
  void UpdateRendererPreferences();

  void OnFontFamilyPrefChanged(const std::string& pref_name);
  void OnWebPrefChanged(const std::string& pref_name);

  void NotifyWebkitPreferencesChanged(const std::string& pref_name);

  raw_ptr<Profile> profile_;
#if !BUILDFLAG(IS_ANDROID)
  base::CallbackListSubscription default_zoom_level_subscription_;
  FontPrefChangeNotifier::Registrar font_change_registrar_;
#endif  // !BUILDFLAG(IS_ANDROID)
  base::WeakPtrFactory<PrefsTabHelper> weak_ptr_factory_{this};

  WEB_CONTENTS_USER_DATA_KEY_DECL();
};

#endif  // CHROME_BROWSER_UI_PREFS_PREFS_TAB_HELPER_H_
