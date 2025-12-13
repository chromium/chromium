// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_SAFETY_CHECK_SAFETY_CHECK_PREFS_H_
#define COMPONENTS_SAFETY_CHECK_SAFETY_CHECK_PREFS_H_

class PrefRegistrySimple;

namespace safety_check::prefs {

// Registers the Profile prefs needed by Safety Check.
void RegisterProfilePrefs(PrefRegistrySimple* registry);

}  // namespace safety_check::prefs

#endif  // COMPONENTS_SAFETY_CHECK_SAFETY_CHECK_PREFS_H_
