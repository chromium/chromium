// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_ARC_FILE_SYSTEM_FILE_SYSTEM_MOJOM_TRAITS_H_
#define COMPONENTS_ARC_FILE_SYSTEM_FILE_SYSTEM_MOJOM_TRAITS_H_

#include "components/arc/mojom/file_system.mojom.h"
#include "storage/browser/file_system/watcher_manager.h"

namespace mojo {

template <>
struct EnumTraits<arc::mojom::ChangeType, storage::WatcherManager::ChangeType> {
  static arc::mojom::ChangeType ToMojom(
      storage::WatcherManager::ChangeType type);
  static bool FromMojom(arc::mojom::ChangeType mojom_type,
                        storage::WatcherManager::ChangeType* type);
};

}  // namespace mojo

#endif  // COMPONENTS_ARC_FILE_SYSTEM_FILE_SYSTEM_MOJOM_TRAITS_H_
