// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROMEOS_ASH_COMPONENTS_EARLY_PREFS_EARLY_PREFS_CONSTANTS_H_
#define CHROMEOS_ASH_COMPONENTS_EARLY_PREFS_EARLY_PREFS_CONSTANTS_H_

namespace ash::early_prefs {

constexpr char kEarlyPrefsFileName[] = ".early_prefs";
constexpr char kEarlyPrefsHistogramName[] = "AshEarlyPrefs";

constexpr char kSchemaKey[] = "schema_version";
constexpr char kDataKey[] = "data";

constexpr char kPrefIsManagedKey[] = "managed";
constexpr char kPrefIsRecommendedKey[] = "recommended";
constexpr char kPrefValueKey[] = "value";

}  // namespace ash::early_prefs

#endif  // CHROMEOS_ASH_COMPONENTS_EARLY_PREFS_EARLY_PREFS_CONSTANTS_H_
