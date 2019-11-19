// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/arc/file_system/file_system_mojom_traits.h"

#include "base/logging.h"
#include "components/arc/mojom/file_system.mojom.h"

namespace mojo {

// static
arc::mojom::ChangeType
EnumTraits<arc::mojom::ChangeType, storage::WatcherManager::ChangeType>::
    ToMojom(storage::WatcherManager::ChangeType type) {
  switch (type) {
    case storage::WatcherManager::CHANGED:
      return arc::mojom::ChangeType::CHANGED;
    case storage::WatcherManager::DELETED:
      return arc::mojom::ChangeType::DELETED;
  }
  NOTREACHED();
  return arc::mojom::ChangeType::CHANGED;
}

// static
bool EnumTraits<arc::mojom::ChangeType, storage::WatcherManager::ChangeType>::
    FromMojom(arc::mojom::ChangeType mojom_type,
              storage::WatcherManager::ChangeType* type) {
  switch (mojom_type) {
    case arc::mojom::ChangeType::CHANGED:
      *type = storage::WatcherManager::CHANGED;
      return true;
    case arc::mojom::ChangeType::DELETED:
      *type = storage::WatcherManager::DELETED;
      return true;
  }
  NOTREACHED();
  return false;
}

}  // namespace mojo
