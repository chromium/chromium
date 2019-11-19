// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_PREFS_PREFS_TAB_HELPER_H_
#define CHROME_BROWSER_UI_PREFS_PREFS_TAB_HELPER_H_

#include "base/callback_list.h"
#include "base/compiler_specific.h"
#include "base/macros.h"
#include "base/memory/weak_ptr.h"
#include "chrome/browser/font_pref_change_notifier.h"
#include "content/public/browser/notification_observer.h"
#include "content/public/browser/notification_registrar.h"
#include "content/public/browser/web_contents_user_data.h"

#if !defined(OS_ANDROID)
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
class PrefsTabHelper : public content::NotificationObserver,
                       public content::WebContentsUserData<PrefsTabHelper> {
 public:
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

  // content::NotificationObserver overrides:
  void Observe(int type,
               const content::NotificationSource& source,
               const content::NotificationDetails& details) override;

  // Update the WebContents's blink::mojom::RendererPreferences.
  void UpdateRendererPreferences();

  void OnFontFamilyPrefChanged(const std::string& pref_name);
  void OnWebPrefChanged(const std::string& pref_name);

  void NotifyWebkitPreferencesChanged(const std::string& pref_name);

  content::WebContents* web_contents_;
  Profile* profile_;
  content::NotificationRegistrar registrar_;
  std::unique_ptr<base::CallbackList<void(void)>::Subscription>
      style_sheet_subscription_;
#if !defined(OS_ANDROID)
  std::unique_ptr<ChromeZoomLevelPrefs::DefaultZoomLevelSubscription>
      default_zoom_level_subscription_;
  FontPrefChangeNotifier::Registrar font_change_registrar_;
#endif  // !defined(OS_ANDROID)
  base::WeakPtrFactory<PrefsTabHelper> weak_ptr_factory_{this};

  WEB_CONTENTS_USER_DATA_KEY_DECL();

  DISALLOW_COPY_AND_ASSIGN(PrefsTabHelper);
};

#endif  // CHROME_BROWSER_UI_PREFS_PREFS_TAB_HELPER_H_
