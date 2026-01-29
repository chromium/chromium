// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_SERVICES_STORAGE_DOM_STORAGE_SQLITE_SQLITE_DATABASE_MACROS_H_
#define COMPONENTS_SERVICES_STORAGE_DOM_STORAGE_SQLITE_SQLITE_DATABASE_MACROS_H_

#include "base/types/expected.h"
#include "components/services/storage/dom_storage/sqlite/sqlite_database_utils.h"

// Returns a `DbStatus` if the passed expression evaluates to false.
// Requires `database_` to be in scope and point to an `sql::Database`.
#define RETURN_STATUS_ON_ERROR(expr)            \
  if (!(expr)) {                                \
    return storage::FromSqliteCode(*database_); \
  }

// Returns a `base::unexpected<DbStatus>` if the passed expression evaluates to
// false. Requires `database_` to be in scope and point to an `sql::Database`.
#define RETURN_UNEXPECTED_ON_ERROR(expr)                          \
  if (!(expr)) {                                                  \
    return base::unexpected(storage::FromSqliteCode(*database_)); \
  }

#endif  // COMPONENTS_SERVICES_STORAGE_DOM_STORAGE_SQLITE_SQLITE_DATABASE_MACROS_H_
