// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_SERVICES_STORAGE_DOM_STORAGE_FEATURES_H_
#define COMPONENTS_SERVICES_STORAGE_DOM_STORAGE_FEATURES_H_

#include "base/component_export.h"
#include "base/features.h"

namespace storage {

// Enable to use a SQLite database with local storage and session storage
// instead of LevelDB. See "crbug.com/377242771: Migrate DOMStorage to use
// SQLite" for more details. When enabled, SQLite is used for both in-memory
// and on-disk storage.
COMPONENT_EXPORT(STORAGE_FEATURES) BASE_DECLARE_FEATURE(kDomStorageSqlite);

// Enable to use a SQLite database for in-memory local storage and session
// storage only. When enabled, the SQLite backend will be used for in-memory
// scenarios (e.g., Incognito mode) while on-disk storage continues to use
// LevelDB. If `kDomStorageSqlite` is enabled, SQLite is used for all scenarios
// regardless of this feature's state.
COMPONENT_EXPORT(STORAGE_FEATURES)
BASE_DECLARE_FEATURE(kDomStorageSqliteInMemory);

}  // namespace storage

#endif  // COMPONENTS_SERVICES_STORAGE_DOM_STORAGE_FEATURES_H_
