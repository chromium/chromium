// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chromeos/ash/experiences/arc/file_system/file_system_mojom_traits.h"

#include "base/notreached.h"
#include "chromeos/ash/experiences/arc/mojom/file_system.mojom.h"

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
}

// static
std::optional<storage::WatcherManager::ChangeType>
EnumTraits<arc::mojom::ChangeType, storage::WatcherManager::ChangeType>::
    FromMojom(arc::mojom::ChangeType mojom_type) {
  switch (mojom_type) {
    case arc::mojom::ChangeType::CHANGED:
      return storage::WatcherManager::CHANGED;
    case arc::mojom::ChangeType::DELETED:
      return storage::WatcherManager::DELETED;
  }
  NOTREACHED();
}

}  // namespace mojo
