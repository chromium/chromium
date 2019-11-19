// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/games/core/games_prefs.h"

namespace games {
namespace prefs {

namespace {

const char kGamesInstallDirPref[] = "games.data_files_paths";

}  // namespace

void RegisterProfilePrefs(PrefRegistrySimple* registry) {
  registry->RegisterFilePathPref(kGamesInstallDirPref, base::FilePath());
}

void SetInstallDirPath(PrefService* prefs, const base::FilePath& file_path) {
  prefs->SetFilePath(kGamesInstallDirPref, file_path);
}

bool TryGetInstallDirPath(PrefService* prefs, base::FilePath* out_file_path) {
  *out_file_path = prefs->GetFilePath(kGamesInstallDirPref);
  return !out_file_path->empty();
}

}  // namespace prefs
}  // namespace games
