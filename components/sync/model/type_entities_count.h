// Copyright 2014 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_SYNC_MODEL_TYPE_ENTITIES_COUNT_H_
#define COMPONENTS_SYNC_MODEL_TYPE_ENTITIES_COUNT_H_

#include "components/sync/base/data_type.h"

namespace syncer {

// Used to track per data-type entity counts for debugging purposes.
struct TypeEntitiesCount {
  explicit TypeEntitiesCount(DataType type) : type(type) {}

  DataType type;

  int entities = 0;

  int non_tombstone_entities = 0;
};

}  // namespace syncer

#endif  // COMPONENTS_SYNC_MODEL_TYPE_ENTITIES_COUNT_H_
