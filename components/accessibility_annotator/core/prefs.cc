// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/accessibility_annotator/core/prefs.h"

#include "components/prefs/pref_registry_simple.h"

namespace accessibility_annotator::prefs {

void RegisterProfilePrefs(PrefRegistrySimple* registry) {
  registry->RegisterBooleanPref(kShouldShowRemoteAnnotatorFirstRunInfo, true);
}

}  // namespace accessibility_annotator::prefs
