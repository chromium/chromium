// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_SERVICES_STORAGE_DOM_STORAGE_FEATURES_H_
#define COMPONENTS_SERVICES_STORAGE_DOM_STORAGE_FEATURES_H_

#include "base/features.h"

namespace storage {

// Enable to use a SQLite database with local storage and session storage
// instead of LevelDB. See "crbug.com/377242771: Migrate DOMStorage to use
// SQLite" for more details.
BASE_DECLARE_FEATURE(kDomStorageSqlite);

}  // namespace storage

#endif  // COMPONENTS_SERVICES_STORAGE_DOM_STORAGE_FEATURES_H_
