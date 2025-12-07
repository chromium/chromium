// Copyright 2014 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/dom_distiller/core/distilled_page_prefs.h"

#include "base/functional/bind.h"
#include "base/location.h"
#include "base/memory/weak_ptr.h"
#include "base/observer_list.h"
#include "base/task/single_thread_task_runner.h"
#include "components/dom_distiller/core/dom_distiller_features.h"
#include "components/dom_distiller/core/pref_names.h"
#include "components/prefs/pref_change_registrar.h"
#include "components/prefs/pref_registry_simple.h"
#include "components/prefs/pref_service.h"

namespace {

const float kDefaultFontScale = 1.0f;

}  // namespace

namespace dom_distiller {

DistilledPagePrefs::DistilledPagePrefs(PrefService* pref_service)
    : pref_service_(pref_service) {
  pref_change_registrar_.Init(pref_service);
  pref_change_registrar_.Add(
      prefs::kFont,
      base::BindRepeating(&DistilledPagePrefs::NotifyOnChangeFontFamily,
                          weak_ptr_factory_.GetWeakPtr()));
  pref_change_registrar_.Add(
      prefs::kTheme,
      base::BindRepeating(&DistilledPagePrefs::NotifyOnChangeTheme,
                          weak_ptr_factory_.GetWeakPtr(),
                          ThemeSettingsUpdateSource::kUserPreference));
  pref_change_registrar_.Add(
      prefs::kFontScale,
      base::BindRepeating(&DistilledPagePrefs::NotifyOnChangeFontScaling,
                          weak_ptr_factory_.GetWeakPtr()));
}

DistilledPagePrefs::~DistilledPagePrefs() = default;

// static
void DistilledPagePrefs::RegisterProfilePrefs(PrefRegistrySimple* registry) {
  registry->RegisterIntegerPref(prefs::kTheme,
                                static_cast<int32_t>(mojom::Theme::kLight));
  registry->RegisterIntegerPref(
      prefs::kFont, static_cast<int32_t>(mojom::FontFamily::kSansSerif));
  registry->RegisterDoublePref(prefs::kFontScale, kDefaultFontScale);
  registry->RegisterBooleanPref(prefs::kReaderForAccessibility, false);
}

void DistilledPagePrefs::SetFontFamily(mojom::FontFamily new_font_family) {
  pref_service_->SetInteger(prefs::kFont,
                            static_cast<int32_t>(new_font_family));
  base::SingleThreadTaskRunner::GetCurrentDefault()->PostTask(
      FROM_HERE, base::BindOnce(&DistilledPagePrefs::NotifyOnChangeFontFamily,
                                weak_ptr_factory_.GetWeakPtr()));
}

mojom::FontFamily DistilledPagePrefs::GetFontFamily() {
  auto font_family =
      static_cast<mojom::FontFamily>(pref_service_->GetInteger(prefs::kFont));
  if (mojom::IsKnownEnumValue(font_family))
    return font_family;

  // Persisted data was incorrect, trying to clean it up by storing the
  // default.
  SetFontFamily(mojom::FontFamily::kSansSerif);
  return mojom::FontFamily::kSansSerif;
}

void DistilledPagePrefs::SetUserPrefTheme(mojom::Theme new_theme) {
  if (static_cast<mojom::Theme>(pref_service_->GetInteger(prefs::kTheme)) ==
      new_theme) {
    return;
  }
  pref_service_->SetInteger(prefs::kTheme, static_cast<int32_t>(new_theme));
}

void DistilledPagePrefs::SetDefaultTheme(mojom::Theme default_theme) {
  if (default_theme_ == default_theme) {
    return;
  }
  default_theme_ = default_theme;
  base::SingleThreadTaskRunner::GetCurrentDefault()->PostTask(
      FROM_HERE, base::BindOnce(&DistilledPagePrefs::NotifyOnChangeTheme,
                                weak_ptr_factory_.GetWeakPtr(),
                                ThemeSettingsUpdateSource::kSystem));
}

mojom::Theme DistilledPagePrefs::GetTheme() {
  mojom::Theme theme;
  if (pref_service_->FindPreference(prefs::kTheme)->HasUserSetting()) {
    theme = static_cast<mojom::Theme>(pref_service_->GetInteger(prefs::kTheme));
  } else {
    theme = default_theme_;
  }
  if (mojom::IsKnownEnumValue(theme))
    return theme;

  // Persisted data was incorrect, trying to clean it up by storing the
  // default.
  SetUserPrefTheme(mojom::Theme::kLight);
  return mojom::Theme::kLight;
}

void DistilledPagePrefs::SetUserPrefFontScaling(float scaling) {
  pref_service_->SetDouble(prefs::kFontScale, scaling);
  base::SingleThreadTaskRunner::GetCurrentDefault()->PostTask(
      FROM_HERE, base::BindOnce(&DistilledPagePrefs::NotifyOnChangeFontScaling,
                                weak_ptr_factory_.GetWeakPtr()));
}

void DistilledPagePrefs::SetDefaultFontScaling(float scaling) {
  // Default zoom level pref is outside of the distilled page prefs font
  // scaling range, so set it to the closest boundary.
  default_font_scaling_ = scaling;
#if BUILDFLAG(IS_ANDROID)
  ClampDefaultFontScaling();
#else
  default_font_scaling_ = std::clamp(scaling, kMinFontScale, kMaxFontScale);
#endif
  base::SingleThreadTaskRunner::GetCurrentDefault()->PostTask(
      FROM_HERE, base::BindOnce(&DistilledPagePrefs::NotifyOnChangeFontScaling,
                                weak_ptr_factory_.GetWeakPtr()));
}

float DistilledPagePrefs::GetFontScaling() {
  float scaling;
  if (pref_service_->FindPreference(prefs::kFontScale)->HasUserSetting()) {
    scaling = pref_service_->GetDouble(prefs::kFontScale);
  } else {
#if BUILDFLAG(IS_ANDROID)
    ClampDefaultFontScaling();
    scaling = default_font_scaling_;
#else
    scaling = kDefaultFontScale;
#endif
  }
  if (scaling < kMinFontScale || scaling > kMaxFontScale) {
    // Persisted data was incorrect, trying to clean it up by storing the
    // default.
    SetUserPrefFontScaling(kDefaultFontScale);
    return kDefaultFontScale;
  }
  return scaling;
}

void DistilledPagePrefs::AddObserver(Observer* obs) {
  observers_.AddObserver(obs);
}

void DistilledPagePrefs::RemoveObserver(Observer* obs) {
  observers_.RemoveObserver(obs);
}

#if BUILDFLAG(IS_ANDROID)
void DistilledPagePrefs::ClampDefaultFontScaling() {
  float min_font_scale;
  float max_font_scale;
  if (base::FeatureList::IsEnabled(dom_distiller::kReaderModeDistillInApp)) {
    min_font_scale = kMinFontScaleAndroidInApp;
    max_font_scale = kMaxFontScaleAndroidInApp;
  } else {
    min_font_scale = kMinFontScaleAndroidCCT;
    max_font_scale = kMaxFontScaleAndroidCCT;
  }
  default_font_scaling_ =
      std::clamp(default_font_scaling_, min_font_scale, max_font_scale);
}
#endif

void DistilledPagePrefs::NotifyOnChangeFontFamily() {
  mojom::FontFamily new_font_family = GetFontFamily();
  for (Observer& observer : observers_)
    observer.OnChangeFontFamily(new_font_family);
}

void DistilledPagePrefs::NotifyOnChangeTheme(
    ThemeSettingsUpdateSource source) {
  mojom::Theme new_theme = GetTheme();
  for (Observer& observer : observers_)
    observer.OnChangeTheme(new_theme, source);
}

void DistilledPagePrefs::NotifyOnChangeFontScaling() {
  float scaling = GetFontScaling();
  for (Observer& observer : observers_)
    observer.OnChangeFontScaling(scaling);
}

}  // namespace dom_distiller
