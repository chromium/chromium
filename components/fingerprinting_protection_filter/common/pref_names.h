// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_FINGERPRINTING_PROTECTION_FILTER_COMMON_PREF_NAMES_H_
#define COMPONENTS_FINGERPRINTING_PROTECTION_FILTER_COMMON_PREF_NAMES_H_

namespace fingerprinting_protection_filter::prefs {

// Dict, acting as a set (values unused but by convention should be all `true`).
// Stores eTLD+1 of sites that have been excepted from Fingerprinting Protection
// by the refresh count heuristic. This is expected to be small and updated
// infrequently, as we don't expect the heuristic to be triggered often
// for a single user.
extern const char kRefreshHeuristicBreakageException[];

}  // namespace fingerprinting_protection_filter::prefs

#endif  // COMPONENTS_FINGERPRINTING_PROTECTION_FILTER_COMMON_PREF_NAMES_H_
