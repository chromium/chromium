// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/dom_distiller/core/distilled_page_prefs.h"

#include "base/bind.h"
#include "base/location.h"
#include "base/memory/weak_ptr.h"
#include "base/observer_list.h"
#include "base/single_thread_task_runner.h"
#include "base/threading/thread_task_runner_handle.h"
#include "components/dom_distiller/core/pref_names.h"
#include "components/pref_registry/pref_registry_syncable.h"
#include "components/prefs/pref_service.h"

namespace dom_distiller {

DistilledPagePrefs::DistilledPagePrefs(PrefService* pref_service)
    : pref_service_(pref_service) {}

DistilledPagePrefs::~DistilledPagePrefs() {}

// static
void DistilledPagePrefs::RegisterProfilePrefs(
    user_prefs::PrefRegistrySyncable* registry) {
  registry->RegisterIntegerPref(
      prefs::kTheme, DistilledPagePrefs::THEME_LIGHT,
      user_prefs::PrefRegistrySyncable::SYNCABLE_PREF);
  registry->RegisterIntegerPref(
      prefs::kFont, DistilledPagePrefs::FONT_FAMILY_SANS_SERIF,
      user_prefs::PrefRegistrySyncable::SYNCABLE_PREF);
  registry->RegisterDoublePref(prefs::kFontScale, 1.0);
  registry->RegisterBooleanPref(
      prefs::kReaderForAccessibility, false,
      user_prefs::PrefRegistrySyncable::SYNCABLE_PREF);
}

void DistilledPagePrefs::SetFontFamily(
    DistilledPagePrefs::FontFamily new_font_family) {
  pref_service_->SetInteger(prefs::kFont, new_font_family);
  base::ThreadTaskRunnerHandle::Get()->PostTask(
      FROM_HERE,
      base::BindOnce(&DistilledPagePrefs::NotifyOnChangeFontFamily,
                     weak_ptr_factory_.GetWeakPtr(), new_font_family));
}

DistilledPagePrefs::FontFamily DistilledPagePrefs::GetFontFamily() {
  int font_family = pref_service_->GetInteger(prefs::kFont);
  if (font_family < 0 ||
      font_family >= DistilledPagePrefs::FONT_FAMILY_NUM_ENTRIES) {
    // Persisted data was incorrect, trying to clean it up by storing the
    // default.
    SetFontFamily(DistilledPagePrefs::FONT_FAMILY_SANS_SERIF);
    return DistilledPagePrefs::FONT_FAMILY_SANS_SERIF;
  }
  return static_cast<FontFamily>(font_family);
}

void DistilledPagePrefs::SetTheme(DistilledPagePrefs::Theme new_theme) {
  pref_service_->SetInteger(prefs::kTheme, new_theme);
  base::ThreadTaskRunnerHandle::Get()->PostTask(
      FROM_HERE, base::BindOnce(&DistilledPagePrefs::NotifyOnChangeTheme,
                                weak_ptr_factory_.GetWeakPtr(), new_theme));
}

DistilledPagePrefs::Theme DistilledPagePrefs::GetTheme() {
  int theme = pref_service_->GetInteger(prefs::kTheme);
  if (theme < 0 || theme >= DistilledPagePrefs::THEME_NUM_ENTRIES) {
    // Persisted data was incorrect, trying to clean it up by storing the
    // default.
    SetTheme(DistilledPagePrefs::THEME_LIGHT);
    return DistilledPagePrefs::THEME_LIGHT;
  }
  return static_cast<Theme>(theme);
}

void DistilledPagePrefs::SetFontScaling(float scaling) {
  pref_service_->SetDouble(prefs::kFontScale, scaling);
  base::ThreadTaskRunnerHandle::Get()->PostTask(
      FROM_HERE, base::BindOnce(&DistilledPagePrefs::NotifyOnChangeFontScaling,
                                weak_ptr_factory_.GetWeakPtr(), scaling));
}

float DistilledPagePrefs::GetFontScaling() {
  float scaling = pref_service_->GetDouble(prefs::kFontScale);
  if (scaling < 0.4 || scaling > 2.5) {
    // Persisted data was incorrect, trying to clean it up by storing the
    // default.
    SetFontScaling(1.0);
    return 1.0;
  }
  return scaling;
}

void DistilledPagePrefs::AddObserver(Observer* obs) {
  observers_.AddObserver(obs);
}

void DistilledPagePrefs::RemoveObserver(Observer* obs) {
  observers_.RemoveObserver(obs);
}

void DistilledPagePrefs::NotifyOnChangeFontFamily(
    DistilledPagePrefs::FontFamily new_font_family) {
  for (Observer& observer : observers_)
    observer.OnChangeFontFamily(new_font_family);
}

void DistilledPagePrefs::NotifyOnChangeTheme(
    DistilledPagePrefs::Theme new_theme) {
  for (Observer& observer : observers_)
    observer.OnChangeTheme(new_theme);
}

void DistilledPagePrefs::NotifyOnChangeFontScaling(float scaling) {
  for (Observer& observer : observers_)
    observer.OnChangeFontScaling(scaling);
}

}  // namespace dom_distiller
