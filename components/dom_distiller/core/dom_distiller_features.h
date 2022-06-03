// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_DOM_DISTILLER_CORE_DOM_DISTILLER_FEATURES_H_
#define COMPONENTS_DOM_DISTILLER_CORE_DOM_DISTILLER_FEATURES_H_

#include "base/feature_list.h"

class PrefService;

namespace user_prefs {
class PrefRegistrySyncable;
}

namespace dom_distiller {

extern const base::Feature kReaderMode;

void RegisterProfilePrefs(user_prefs::PrefRegistrySyncable* registry);

// Returns true when flag enable-dom-distiller is set or reader mode is enabled
// from flags or Finch.
bool IsDomDistillerEnabled();

// Returns true when reader mode flag is enabled and the flag parameter to add
// "offer reader mode" in chrome://settings is set.
bool OfferReaderModeInSettings();

// Returns true if a user should be shown the option to view pages in reader
// mode, when available. This happens when either:
// A. OfferReaderModeInSettings is true and kOfferReaderMode pref is enabled,
// B. or OfferReaderModeInSettings is false, but IsDomDistillerEnabled is true.
bool ShowReaderModeOption(PrefService* pref_service);

bool ShouldStartDistillabilityService();

}  // namespace dom_distiller

#endif  // COMPONENTS_DOM_DISTILLER_CORE_DOM_DISTILLER_FEATURES_H_
