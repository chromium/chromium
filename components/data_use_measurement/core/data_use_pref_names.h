// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_DATA_USE_MEASUREMENT_CORE_DATA_USE_PREF_NAMES_H_
#define COMPONENTS_DATA_USE_MEASUREMENT_CORE_DATA_USE_PREF_NAMES_H_

namespace data_use_measurement {

namespace prefs {
// Dictionary prefs for measuring cellular data used. |key| is
// the date of data usage (stored as string using exploded format). |value|
// stores the data used for that date as a double in kilobytes.
const char kDataUsedUserForeground[] =
    "data_use_measurement.data_used.user.foreground";
const char kDataUsedUserBackground[] =
    "data_use_measurement.data_used.user.background";
const char kDataUsedServicesForeground[] =
    "data_use_measurement.data_used.services.foreground";
const char kDataUsedServicesBackground[] =
    "data_use_measurement.data_used.services.background";
}  // namespace prefs

}  // namespace data_use_measurement

#endif  // COMPONENTS_DATA_USE_MEASUREMENT_CORE_DATA_USE_PREF_NAMES_H_
