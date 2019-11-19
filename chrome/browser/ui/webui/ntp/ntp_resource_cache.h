// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_WEBUI_NTP_NTP_RESOURCE_CACHE_H_
#define CHROME_BROWSER_UI_WEBUI_NTP_NTP_RESOURCE_CACHE_H_

#include <memory>

#include "base/compiler_specific.h"
#include "base/macros.h"
#include "base/memory/ref_counted.h"
#include "base/scoped_observer.h"
#include "components/keyed_service/core/keyed_service.h"
#include "components/prefs/pref_change_registrar.h"
#include "content/public/browser/notification_observer.h"
#include "content/public/browser/notification_registrar.h"
#include "ui/native_theme/native_theme.h"
#include "ui/native_theme/native_theme_observer.h"

class Profile;

namespace base {
class RefCountedMemory;
class Value;
}

namespace content {
class RenderProcessHost;
}

namespace policy {
class PolicyChangeRegistrar;
}

// This class keeps a cache of NTP resources (HTML and CSS) so we don't have to
// regenerate them all the time.
// Note: This is only used for incognito and guest mode NTPs (NewTabUI), as well
// as for (non-incognito) app launcher pages (AppLauncherPageUI).
class NTPResourceCache : public content::NotificationObserver,
                         public KeyedService,
                         public ui::NativeThemeObserver {
 public:
  enum WindowType {
    NORMAL,
    INCOGNITO,
    GUEST,
  };

  explicit NTPResourceCache(Profile* profile);
  ~NTPResourceCache() override;

  base::RefCountedMemory* GetNewTabHTML(WindowType win_type);
  base::RefCountedMemory* GetNewTabCSS(WindowType win_type);

  // content::NotificationObserver:
  void Observe(int type,
               const content::NotificationSource& source,
               const content::NotificationDetails& details) override;

  static WindowType GetWindowType(
      Profile* profile, content::RenderProcessHost* render_host);

 private:
  // ui::NativeThemeObserver:
  void OnNativeThemeUpdated(ui::NativeTheme* updated_theme) override;

  void OnPreferenceChanged();

  void OnPolicyChanged(const base::Value* previous, const base::Value* current);

  // Invalidates the NTPResourceCache.
  void Invalidate();

  // Helper to determine if the resource cache for the main (not incognito or
  // guest) HTML should be invalidated.
  // This is called on every page load, and can be used to check values that
  // don't generate a notification when changed (e.g., system preferences).
  bool NewTabHTMLNeedsRefresh();

  void CreateNewTabHTML();
  void CreateNewTabCSS();

  void CreateNewTabIncognitoHTML();
  void CreateNewTabIncognitoCSS();

  void CreateNewTabGuestHTML();

  void SetDarkKey(base::Value* dict);

  Profile* profile_;

  scoped_refptr<base::RefCountedMemory> new_tab_html_;
  scoped_refptr<base::RefCountedMemory> new_tab_css_;
  scoped_refptr<base::RefCountedMemory> new_tab_guest_html_;
  scoped_refptr<base::RefCountedMemory> new_tab_incognito_html_;
  scoped_refptr<base::RefCountedMemory> new_tab_incognito_css_;
  content::NotificationRegistrar registrar_;
  PrefChangeRegistrar profile_pref_change_registrar_;
  PrefChangeRegistrar local_state_pref_change_registrar_;

  // Set based on platform_util::IsSwipeTrackingFromScrollEventsEnabled.
  bool is_swipe_tracking_from_scroll_events_enabled_;

  ScopedObserver<ui::NativeTheme, ui::NativeThemeObserver> theme_observer_{
      this};

  std::unique_ptr<policy::PolicyChangeRegistrar> policy_change_registrar_;

  DISALLOW_COPY_AND_ASSIGN(NTPResourceCache);
};

#endif  // CHROME_BROWSER_UI_WEBUI_NTP_NTP_RESOURCE_CACHE_H_
