// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROMEOS_ASH_COMPONENTS_SCALABLE_IPH_SCALABLE_IPH_CONSTANTS_H_
#define CHROMEOS_ASH_COMPONENTS_SCALABLE_IPH_SCALABLE_IPH_CONSTANTS_H_

namespace scalable_iph {

enum class ActionType {
  // `kInvalid` is reserved to be used as an initial value. This must not be
  // used in prod.
  kInvalid = 0,
  kOpenChrome,
  kOpenPersonalizationApp,
  kOpenPlayStore,
  kOpenGoogleDocs,
  kOpenGooglePhotos,
  kOpenSettingsPrinter,
  kOpenPhoneHub,
  kOpenYouTube,
  kOpenFileManager,
};

// Constants for events.
// Naming convention: Camel case starting with a capital letter. Note that
// Scalable Iph event names must start with `ScalableIph` as Iph event names
// live in a global namespace.

// `FiveMinTick` event is recorded every five minutes after OOBE completion.
constexpr char kEventNameFiveMinTick[] = "ScalableIphFiveMinTick";

// Constants for custom conditions.
// Naming convention:
// Camel case starting with a capital letter. Note that param names must start
// with `x_CustomCondition` prefix:
// - `x_` is from the feature engagement framework. The framework ignores any
//   params start with it.
// - `CustomCondition` indicates this param is for custom condition. We use
//   params for other things as well, e.g. UIs.
//
// Usage:
// Custom conditions is an extension implemented in `ScalableIph` framework.
// Those conditions are checked in addition to other event conditions of the
// feature engagement framework.
//
// Example:
// "x_CustomConditionsNetworkConnection": "Online"

// `NetworkConnection` condition is satisfied if a device is online. For now, we
// only support `Online` as the expected condition.
constexpr char kCustomConditionNetworkConnectionParamName[] =
    "x_CustomConditionNetworkConnection";
constexpr char kCustomConditionNetworkConnectionOnline[] = "Online";

// `ClientAgeInDays` condition is satisfied if a device's client age is on or
// below the specified number of days. The number must be a positive integer
// including 0.
// - The day count starts from 0. For example, if you specify 0 as a value, it
//   means that a profile is created in the last 24 hours.
// - The day in this condition does not match with the calendar day. If a
//   profile is created at 3 pm on May 1st, the day 0 ends at 3 pm on May 2nd.
constexpr char kCustomConditionClientAgeInDaysParamName[] =
    "x_CustomConditionClientAgeInDays";

}  // namespace scalable_iph

#endif  // CHROMEOS_ASH_COMPONENTS_SCALABLE_IPH_SCALABLE_IPH_CONSTANTS_H_
