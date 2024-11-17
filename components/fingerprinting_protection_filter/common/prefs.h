// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_FINGERPRINTING_PROTECTION_FILTER_COMMON_PREFS_H_
#define COMPONENTS_FINGERPRINTING_PROTECTION_FILTER_COMMON_PREFS_H_

class PrefRegistrySimple;

namespace fingerprinting_protection_filter::prefs {
void RegisterProfilePrefs(PrefRegistrySimple* registry);
}  // namespace fingerprinting_protection_filter::prefs

#endif  // COMPONENTS_FINGERPRINTING_PROTECTION_FILTER_COMMON_PREFS_H_
