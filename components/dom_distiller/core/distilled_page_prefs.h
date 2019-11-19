// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_DOM_DISTILLER_CORE_DISTILLED_PAGE_PREFS_H_
#define COMPONENTS_DOM_DISTILLER_CORE_DISTILLED_PAGE_PREFS_H_

#include "base/macros.h"
#include "base/memory/weak_ptr.h"
#include "base/observer_list.h"

class PrefService;

namespace user_prefs {
class PrefRegistrySyncable;
}

namespace dom_distiller {

// Interface for preferences used for distilled page.
class DistilledPagePrefs {
 public:
  // Possible font families for distilled page.
  // These must be kept in sync with the resource strings in
  // chrome/android/java/res/values/arrays.xml
  // Values should start from 0 and can't have gaps.
  // A Java counterpart will be generated for this enum.
  // GENERATED_JAVA_ENUM_PACKAGE: org.chromium.components.dom_distiller.core
  // GENERATED_JAVA_CLASS_NAME_OVERRIDE: FontFamily
  // GENERATED_JAVA_PREFIX_TO_STRIP: FONT_FAMILY_
  enum FontFamily {
    FONT_FAMILY_SANS_SERIF = 0,
    FONT_FAMILY_SERIF = 1,
    FONT_FAMILY_MONOSPACE = 2,
    FONT_FAMILY_NUM_ENTRIES = 3
  };

  // Possible themes for distilled page.
  // Values should start from 0 and can't have gaps.
  // A Java counterpart will be generated for this enum.
  // GENERATED_JAVA_ENUM_PACKAGE: org.chromium.components.dom_distiller.core
  // GENERATED_JAVA_CLASS_NAME_OVERRIDE: Theme
  // GENERATED_JAVA_PREFIX_TO_STRIP: THEME_
  enum Theme {
    THEME_LIGHT = 0,
    THEME_DARK = 1,
    THEME_SEPIA = 2,
    THEME_NUM_ENTRIES = 3
  };

  class Observer {
   public:
    virtual void OnChangeFontFamily(FontFamily font) = 0;
    virtual void OnChangeTheme(Theme theme) = 0;
    virtual void OnChangeFontScaling(float scaling) = 0;
  };

  explicit DistilledPagePrefs(PrefService* pref_service);
  ~DistilledPagePrefs();

  static void RegisterProfilePrefs(user_prefs::PrefRegistrySyncable* registry);

  // Sets the user's preference for the font family of distilled pages.
  void SetFontFamily(FontFamily new_font);
  // Returns the user's preference for the font family of distilled pages.
  FontFamily GetFontFamily();

  // Sets the user's preference for the theme of distilled pages.
  void SetTheme(Theme new_theme);
  // Returns the user's preference for the theme of distilled pages.
  Theme GetTheme();

  // Sets the user's preference for the font size scaling of distilled pages.
  void SetFontScaling(float scaling);
  // Returns the user's preference for the font size scaling of distilled pages.
  float GetFontScaling();

  void AddObserver(Observer* obs);
  void RemoveObserver(Observer* obs);

 private:
  // Notifies all Observers of new font family.
  void NotifyOnChangeFontFamily(FontFamily font_family);
  // Notifies all Observers of new theme.
  void NotifyOnChangeTheme(Theme theme);
  // Notifies all Observers of new font scaling.
  void NotifyOnChangeFontScaling(float scaling);

  PrefService* pref_service_;
  base::ObserverList<Observer>::Unchecked observers_;

  base::WeakPtrFactory<DistilledPagePrefs> weak_ptr_factory_{this};

  DISALLOW_COPY_AND_ASSIGN(DistilledPagePrefs);
};

}  // namespace dom_distiller

#endif  // COMPONENTS_DOM_DISTILLER_CORE_DISTILLED_PAGE_PREFS_H_
