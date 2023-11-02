// Copyright 2016 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_ONC_ONC_PREF_NAMES_H_
#define COMPONENTS_ONC_ONC_PREF_NAMES_H_

#include "base/component_export.h"

class PrefRegistrySimple;

namespace onc {

namespace prefs {

COMPONENT_EXPORT(ONC) extern const char kDeviceOpenNetworkConfiguration[];
COMPONENT_EXPORT(ONC) extern const char kOpenNetworkConfiguration[];

}  // namespace prefs

COMPONENT_EXPORT(ONC) void RegisterPrefs(PrefRegistrySimple* registry);

COMPONENT_EXPORT(ONC)
void RegisterProfilePrefs(PrefRegistrySimple* registry);

}  // namespace onc

#endif  // COMPONENTS_ONC_ONC_PREF_NAMES_H_
