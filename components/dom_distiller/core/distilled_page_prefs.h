// Copyright 2014 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_DOM_DISTILLER_CORE_DISTILLED_PAGE_PREFS_H_
#define COMPONENTS_DOM_DISTILLER_CORE_DISTILLED_PAGE_PREFS_H_

#include "base/memory/raw_ptr.h"
#include "base/memory/weak_ptr.h"
#include "base/observer_list.h"
#include "components/dom_distiller/core/mojom/distilled_page_prefs.mojom.h"
#include "components/prefs/pref_change_registrar.h"

class PrefRegistrySimple;
class PrefService;

namespace dom_distiller {

inline constexpr float kMinFontScale = 0.4f;
inline constexpr float kMaxFontScale = 3.0f;

// Custom values for Android reader mode font scaling boundaries.
inline constexpr float kMinFontScaleAndroidInApp = 1.0f;
inline constexpr float kMaxFontScaleAndroidInApp = 2.5f;
inline constexpr float kMinFontScaleAndroidCCT = 0.5f;
inline constexpr float kMaxFontScaleAndroidCCT = 2.0f;

// The source for updates to the distiller theme settings.
enum class ThemeSettingsUpdateSource {
  kSystem,
  kUserPreference,
};

// Interface for preferences used for distilled page.
class DistilledPagePrefs {
 public:
  class Observer : public base::CheckedObserver {
   public:
    virtual void OnChangeFontFamily(mojom::FontFamily font) = 0;
    virtual void OnChangeTheme(mojom::Theme theme,
                               ThemeSettingsUpdateSource source) = 0;
    virtual void OnChangeFontScaling(float scaling) = 0;
  };

  explicit DistilledPagePrefs(PrefService* pref_service);

  DistilledPagePrefs(const DistilledPagePrefs&) = delete;
  DistilledPagePrefs& operator=(const DistilledPagePrefs&) = delete;

  ~DistilledPagePrefs();

  static void RegisterProfilePrefs(PrefRegistrySimple* registry);

  // Sets the user's preference for the font family of distilled pages.
  void SetFontFamily(mojom::FontFamily new_font);
  // Returns the user's preference for the font family of distilled pages.
  mojom::FontFamily GetFontFamily();

  // Sets the user's preference for the theme of distilled pages.
  void SetUserPrefTheme(mojom::Theme new_theme);

  // Sets default theme, used when user's preference for theme is not set.
  void SetDefaultTheme(mojom::Theme default_theme);

  // Returns the theme for distilled pages. If user's preference for the theme
  // is set, it will return the user's preference for the theme. Otherwise, it
  // will return the value of default_theme_.
  mojom::Theme GetTheme();

  // Sets the user's preference for the font size scaling of distilled pages.
  void SetUserPrefFontScaling(float scaling);

  // Sets default font scaling, used when user's preference for font scaling is
  // not set. This will be aligned with the default zoom.
  void SetDefaultFontScaling(float scaling);

  // Returns the font size scaling of distilled pages. If user's preference for
  // font size scaling is set, it will return the user's preference. Otherwise,
  // it will return the value of default_font_scaling_.
  float GetFontScaling();

  void AddObserver(Observer* obs);
  void RemoveObserver(Observer* obs);

 private:
#if BUILDFLAG(IS_ANDROID)
  // Clamps the default font scaling to properly follow min and max font scaling
  // for whether the distillation is in-app or CCT.
  void ClampDefaultFontScaling();
#endif

  // Notifies all Observers of new font family.
  void NotifyOnChangeFontFamily();
  // Notifies all Observers of new theme.
  void NotifyOnChangeTheme(ThemeSettingsUpdateSource source);
  // Notifies all Observers of new font scaling.
  void NotifyOnChangeFontScaling();

  raw_ptr<PrefService> pref_service_;
  PrefChangeRegistrar pref_change_registrar_;
  base::ObserverList<Observer> observers_;

  mojom::Theme default_theme_ = mojom::Theme::kLight;
  float default_font_scaling_ = 1.0f;

  base::WeakPtrFactory<DistilledPagePrefs> weak_ptr_factory_{this};
};

}  // namespace dom_distiller

#endif  // COMPONENTS_DOM_DISTILLER_CORE_DISTILLED_PAGE_PREFS_H_
