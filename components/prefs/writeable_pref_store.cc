// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/prefs/writeable_pref_store.h"

void WriteablePrefStore::ReportSubValuesChanged(
    std::string_view key,
    std::set<std::vector<std::string>> path_components,
    uint32_t flags) {
  // Default implementation. Subclasses may use |path_components| to improve
  // performance.
  ReportValueChanged(key, flags);
}
