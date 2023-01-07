// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_CONTENT_CREATION_NOTES_CORE_NOTE_PREFS_H_
#define COMPONENTS_CONTENT_CREATION_NOTES_CORE_NOTE_PREFS_H_

#include <vector>

#include "components/content_creation/notes/core/templates/template_types.h"
#include "components/prefs/pref_registry_simple.h"
#include "components/prefs/pref_service.h"
#include "third_party/abseil-cpp/absl/types/optional.h"

namespace content_creation {
namespace prefs {

// Registers Note prefs.
void RegisterProfilePrefs(PrefRegistrySimple* registry);

// Sets |order| into the random order pref view |prefs|.
void SetRandomOrder(PrefService* prefs,
                    const std::vector<NoteTemplateIds>& order);

// Tries to get the random order pref via |prefs|.
// Returns an optional value which will contain the vector of note template IDs
// if the pref value was set. Returns an empty optional value if not.
absl::optional<std::vector<NoteTemplateIds>> TryGetRandomOrder(
    PrefService* prefs);

}  // namespace prefs
}  // namespace content_creation

#endif  // COMPONENTS_CONTENT_CREATION_NOTES_CORE_NOTE_PREFS_H_
