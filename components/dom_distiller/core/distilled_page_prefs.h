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

// Interface for preferences used for distilled page.
class DistilledPagePrefs {
 public:
  class Observer : public base::CheckedObserver {
   public:
    virtual void OnChangeFontFamily(mojom::FontFamily font) = 0;
    virtual void OnChangeTheme(mojom::Theme theme) = 0;
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
  void SetFontScaling(float scaling);
  // Returns the user's preference for the font size scaling of distilled pages.
  float GetFontScaling();

  void AddObserver(Observer* obs);
  void RemoveObserver(Observer* obs);

 private:
  // Notifies all Observers of new font family.
  void NotifyOnChangeFontFamily();
  // Notifies all Observers of new theme.
  void NotifyOnChangeTheme();
  // Notifies all Observers of new font scaling.
  void NotifyOnChangeFontScaling();

  raw_ptr<PrefService> pref_service_;
  PrefChangeRegistrar pref_change_registrar_;
  base::ObserverList<Observer> observers_;

  std::optional<mojom::Theme> default_theme_;

  base::WeakPtrFactory<DistilledPagePrefs> weak_ptr_factory_{this};
};

}  // namespace dom_distiller

#endif  // COMPONENTS_DOM_DISTILLER_CORE_DISTILLED_PAGE_PREFS_H_
