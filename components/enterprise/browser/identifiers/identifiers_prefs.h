// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_ENTERPRISE_BROWSER_IDENTIFIERS_IDENTIFIERS_PREFS_H_
#define COMPONENTS_ENTERPRISE_BROWSER_IDENTIFIERS_IDENTIFIERS_PREFS_H_

class PrefRegistrySimple;

namespace enterprise {

// The name of the preference that stores the generated profile GUID.
extern const char kProfileGUIDPref[];

void RegisterIdentifiersProfilePrefs(PrefRegistrySimple* registry);

}  // namespace enterprise

#endif  // COMPONENTS_ENTERPRISE_BROWSER_IDENTIFIERS_IDENTIFIERS_PREFS_H_
