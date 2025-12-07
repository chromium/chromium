// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_DATA_SHARING_MIGRATION_PUBLIC_SYNC_ENTITY_IDENTIFIER_H_
#define COMPONENTS_DATA_SHARING_MIGRATION_PUBLIC_SYNC_ENTITY_IDENTIFIER_H_

#include "base/uuid.h"
#include "components/sync/base/data_type.h"

namespace data_sharing {

// A unique, self-describing identifier for any entity managed by the sync
// system. It combines the entity's data type with its GUID.
struct SyncEntityIdentifier {
  syncer::DataType type;
  base::Uuid guid;
};

}  // namespace data_sharing

#endif  // COMPONENTS_DATA_SHARING_MIGRATION_PUBLIC_SYNC_ENTITY_IDENTIFIER_H_
