// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_WEBUI_NTP_NTP_RESOURCE_CACHE_H_
#define CHROME_BROWSER_UI_WEBUI_NTP_NTP_RESOURCE_CACHE_H_

#include <memory>
#include <string>

#include "base/memory/raw_ptr.h"
#include "base/memory/ref_counted.h"
#include "base/memory/ref_counted_memory.h"
#include "base/scoped_observation.h"
#include "chrome/browser/themes/theme_service_observer.h"
#include "components/keyed_service/core/keyed_service.h"
#include "components/prefs/pref_change_registrar.h"
#include "content/public/browser/web_contents.h"
#include "ui/native_theme/native_theme.h"
#include "ui/native_theme/native_theme_observer.h"

class Profile;

namespace base {
class RefCountedMemory;
class Value;
}  // namespace base

namespace policy {
class PolicyChangeRegistrar;
}

namespace ui {
class ColorProvider;
class ThemeProvider;
}

SkColor GetThemeColor(const ui::NativeTheme* native_theme,
                      const ui::ColorProvider& cp,
                      int id);
std::string GetNewTabBackgroundPositionCSS(
    const ui::ThemeProvider& theme_provider);
std::string GetNewTabBackgroundTilingCSS(
    const ui::ThemeProvider& theme_provider);

// This class keeps a cache of NTP resources (HTML and CSS) so we don't have to
// regenerate them all the time.
// Note: This is only used for incognito and guest mode NTPs (NewTabUI), as well
// as for (non-incognito) app launcher pages (AppLauncherPageUI).
class NTPResourceCache : public ThemeServiceObserver,
                         public KeyedService,
                         public ui::NativeThemeObserver {
 public:
  enum WindowType {
    NORMAL,
    INCOGNITO,
    GUEST,
    // The OTR profile that is not used for Incognito or Guest windows.
    NON_PRIMARY_OTR,
  };

  explicit NTPResourceCache(Profile* profile);

  NTPResourceCache(const NTPResourceCache&) = delete;
  NTPResourceCache& operator=(const NTPResourceCache&) = delete;

  ~NTPResourceCache() override;

  base::RefCountedMemory* GetNewTabGuestHTML();
  base::RefCountedMemory* GetNewTabHTML(
      WindowType win_type,
      const content::WebContents::Getter& wc_getter);
  base::RefCountedMemory* GetNewTabCSS(
      WindowType win_type,
      const content::WebContents::Getter& wc_getter);

  // ThemeServiceObserver:
  void OnThemeChanged() override;

  static WindowType GetWindowType(Profile* profile);

 private:
  // KeyedService:
  void Shutdown() override;

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
  void CreateNewTabCSS(const content::WebContents::Getter& wc_getter);

  void CreateNewTabIncognitoHTML(const content::WebContents::Getter& wc_getter);
  void CreateNewTabIncognitoCSS(const content::WebContents::Getter& wc_getter);

  void CreateNewTabGuestHTML();

  void SetDarkKey(base::Value* dict);

  raw_ptr<Profile> profile_;

  scoped_refptr<base::RefCountedMemory> new_tab_css_;
  scoped_refptr<base::RefCountedMemory> new_tab_guest_html_;
  scoped_refptr<base::RefCountedMemory> new_tab_incognito_html_;
  scoped_refptr<base::RefCountedMemory> new_tab_incognito_css_;
  scoped_refptr<base::RefCountedMemory> new_tab_non_primary_otr_html_;
  PrefChangeRegistrar profile_pref_change_registrar_;
  PrefChangeRegistrar local_state_pref_change_registrar_;

  // Set based on platform_util::IsSwipeTrackingFromScrollEventsEnabled.
  bool is_swipe_tracking_from_scroll_events_enabled_;

  base::ScopedObservation<ui::NativeTheme, ui::NativeThemeObserver>
      theme_observation_{this};

  std::unique_ptr<policy::PolicyChangeRegistrar> policy_change_registrar_;
};

#endif  // CHROME_BROWSER_UI_WEBUI_NTP_NTP_RESOURCE_CACHE_H_
