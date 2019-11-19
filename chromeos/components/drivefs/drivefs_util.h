// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROMEOS_COMPONENTS_DRIVEFS_DRIVEFS_UTIL_H_
#define CHROMEOS_COMPONENTS_DRIVEFS_DRIVEFS_UTIL_H_

#include "chromeos/components/drivefs/mojom/drivefs.mojom.h"

namespace drivefs {

// The type represents some sort of a file.
inline bool IsAFile(mojom::FileMetadata::Type type) {
  return type == mojom::FileMetadata::Type::kHosted ||
         type == mojom::FileMetadata::Type::kFile;
}

// The type represents some sort of a directory.
inline bool IsADirectory(mojom::FileMetadata::Type type) {
  return type == mojom::FileMetadata::Type::kDirectory;
}

// The type represents a virtual cloud-hosted object.
inline bool IsHosted(mojom::FileMetadata::Type type) {
  return type == mojom::FileMetadata::Type::kHosted;
}

// The type represents a real local object.
inline bool IsLocal(mojom::FileMetadata::Type type) {
  return type != mojom::FileMetadata::Type::kHosted;
}

}  // namespace drivefs

#endif  // CHROMEOS_COMPONENTS_DRIVEFS_DRIVEFS_UTIL_H_
