// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_GAMES_CORE_GAMES_PREFS_H_
#define COMPONENTS_GAMES_CORE_GAMES_PREFS_H_

#include "base/files/file_path.h"
#include "components/prefs/pref_registry_simple.h"
#include "components/prefs/pref_service.h"

namespace games {
namespace prefs {

// Registers Games pref.
void RegisterProfilePrefs(PrefRegistrySimple* registry);

void SetInstallDirPath(PrefService* prefs, const base::FilePath& file_path);

bool TryGetInstallDirPath(PrefService* prefs, base::FilePath* out_file_path);

}  // namespace prefs
}  // namespace games

#endif  // COMPONENTS_GAMES_CORE_GAMES_PREFS_H_
