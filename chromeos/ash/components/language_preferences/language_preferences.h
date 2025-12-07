// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROMEOS_ASH_COMPONENTS_LANGUAGE_PREFERENCES_LANGUAGE_PREFERENCES_H_
#define CHROMEOS_ASH_COMPONENTS_LANGUAGE_PREFERENCES_LANGUAGE_PREFERENCES_H_

class PrefRegistrySimple;

// TODO(yusukes): Rename this file to input_method_preference.cc. Since
// "language" usually means UI language, the current file name is confusing.
// The namespace should also be changed to "namespace input_method {".

// This file defines types and declare variables used in "Languages and
// Input" settings in Chromium OS.
namespace ash {
namespace language_prefs {

// ---------------------------------------------------------------------------
// For input method engine management
// ---------------------------------------------------------------------------
extern const char kGeneralSectionName[];
extern const char kPreloadEnginesConfigName[];

// ---------------------------------------------------------------------------
// For keyboard stuff
// ---------------------------------------------------------------------------
// A string Chrome preference (Local State) of the preferred keyboard layout in
// the login screen.
extern const char kPreferredKeyboardLayout[];

// Registers non-user prefs for the default keyboard layout on the login screen.
void RegisterPrefs(PrefRegistrySimple* registry);

}  // namespace language_prefs
}  // namespace ash

#endif  // CHROMEOS_ASH_COMPONENTS_LANGUAGE_PREFERENCES_LANGUAGE_PREFERENCES_H_
