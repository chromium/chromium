// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_ONC_ONC_PREF_NAMES_H_
#define COMPONENTS_ONC_ONC_PREF_NAMES_H_

#include "components/onc/onc_export.h"

class PrefRegistrySimple;

namespace user_prefs {
class PrefRegistrySyncable;
}

namespace onc {

namespace prefs {

ONC_EXPORT extern const char kDeviceOpenNetworkConfiguration[];
ONC_EXPORT extern const char kOpenNetworkConfiguration[];

}  // namespace prefs

ONC_EXPORT void RegisterPrefs(PrefRegistrySimple* registry);

ONC_EXPORT void RegisterProfilePrefs(
    user_prefs::PrefRegistrySyncable* registry);

}  // namespace onc

#endif  // COMPONENTS_ONC_ONC_PREF_NAMES_H_
