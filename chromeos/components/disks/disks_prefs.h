// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROMEOS_COMPONENTS_DISKS_DISKS_PREFS_H_
#define CHROMEOS_COMPONENTS_DISKS_DISKS_PREFS_H_

class PrefRegistrySimple;

namespace disks::prefs {

extern const char kExternalStorageDisabled[];
extern const char kExternalStorageReadOnly[];

// Registers external storage specific profile preferences for browser prefs.
void RegisterProfilePrefs(PrefRegistrySimple* registry);

}  // namespace disks::prefs

#endif  // CHROMEOS_COMPONENTS_DISKS_DISKS_PREFS_H_
