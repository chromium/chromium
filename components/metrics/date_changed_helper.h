// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_METRICS_DATE_CHANGED_HELPER_H_
#define COMPONENTS_METRICS_DATE_CHANGED_HELPER_H_

class PrefRegistrySimple;
class PrefService;

namespace metrics {

namespace date_changed_helper {

// Returns whether the local date has changed since last time this was called
// for the given |pref_name|. Simple alternative to |DailyEvent|.
// TODO: Consider adding an enum param to distinguish has-date-changed from
// has-day-elapsed if needed by consumers of this API.
bool HasDateChangedSinceLastCall(PrefService* pref_service,
                                 const char* pref_name);

// Registers the preference used by this helper.
void RegisterPref(PrefRegistrySimple* registry, const char* pref_name);

}  // namespace date_changed_helper

}  // namespace metrics

#endif  // COMPONENTS_METRICS_DATE_CHANGED_HELPER_H_
