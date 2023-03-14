// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/user_notes/user_notes_prefs.h"

#include "components/pref_registry/pref_registry_syncable.h"
#include "components/prefs/pref_registry_simple.h"
#include "components/user_notes/user_notes_features.h"

namespace prefs {

const char kUserNotesSortByNewest[] = "user_notes.sort_by_newest";

}  // namespace prefs

namespace user_notes {

void RegisterProfilePrefs(PrefRegistrySimple* registry) {
  if (user_notes::IsUserNotesEnabled()) {
    registry->RegisterBooleanPref(prefs::kUserNotesSortByNewest, false);
  }
}

}  // namespace user_notes