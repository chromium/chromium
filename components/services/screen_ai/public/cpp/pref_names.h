// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_SERVICES_SCREEN_AI_PUBLIC_CPP_PREF_NAMES_H_
#define COMPONENTS_SERVICES_SCREEN_AI_PUBLIC_CPP_PREF_NAMES_H_

class PrefRegistrySimple;

namespace prefs {

// The scheduled time to clean up the ScreenAI library from the device.
extern const char kScreenAIScheduledDeletionTimePrefName[];

}  // namespace prefs

namespace screen_ai {

// Call once by the browser process to register Screen AI preferences.
void RegisterLocalStatePrefs(PrefRegistrySimple* registry);

}  // namespace screen_ai

#endif  // COMPONENTS_SERVICES_SCREEN_AI_PUBLIC_CPP_PREF_NAMES_H_
