// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_SERVICES_APP_SERVICE_PUBLIC_CPP_PREFERRED_APPS_CONVERTER_H_
#define COMPONENTS_SERVICES_APP_SERVICE_PUBLIC_CPP_PREFERRED_APPS_CONVERTER_H_

#include "base/values.h"
#include "components/services/app_service/public/cpp/preferred_app.h"

namespace apps {

extern const char kAppIdKey[];
extern const char kIntentFilterKey[];
extern const char kPreferredAppsKey[];
extern const char kVersionKey[];

// Convert the PreferredAppsList struct to base::Value to write to JSON file.
// e.g. for preferred app with |app_id| "abcdefg", and |intent_filter| for url
// https://www.google.com/abc.
// The converted base::Value format will be:
//{"preferred_apps": [ {"app_id": "abcdefg",
//    "intent_filter": [ {
//       "condition_type": 0,
//       "condition_values": [ {
//          "match_type": 0,
//          "value": "https"
//       } ]
//    }, {
//       "condition_type": 1,
//       "condition_values": [ {
//          "match_type": 0,
//          "value": "www.google.com"
//       } ]
//    }, {
//       "condition_type": 2,
//       "condition_values": [ {
//          "match_type": 2,
//          "value": "/abc"
//       } ]
//    } ]
// } ],
// "version": 0}
base::Value ConvertPreferredAppsToValue(const PreferredApps& preferred_apps);

// Parse the base::Value read from JSON file back to preferred apps struct.
PreferredApps ParseValueToPreferredApps(
    const base::Value& preferred_apps_value);

// Upgrade the preferred apps struct to contain action in the filters.
void UpgradePreferredApps(PreferredApps& preferred_apps);

// Check if the preferred apps file already upgraded for supporting sharing.
bool IsUpgradedForSharing(const base::Value& preferred_apps_value);

}  // namespace apps

#endif  // COMPONENTS_SERVICES_APP_SERVICE_PUBLIC_CPP_PREFERRED_APPS_CONVERTER_H_
